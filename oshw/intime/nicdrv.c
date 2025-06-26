/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * EtherCAT RAW socket driver.
 *
 * Low level interface functions to send and receive EtherCAT packets.
 * EtherCAT has the property that packets are only sent by the master,
 * and the sent packets always return in the receive buffer.
 * There can be multiple packets "on the wire" before they return.
 * To combine the received packets with the original sent packets a buffer
 * system is installed. The identifier is put in the index item of the
 * EtherCAT header. The index is stored and compared when a frame is received.
 * If there is a match the packet can be combined with the transmit packet
 * and returned to the higher level function.
 *
 * The socket layer can exhibit a reversal in the packet order (rare).
 * If the Tx order is A-B-C the return order could be A-C-B. The indexed buffer
 * will reorder the packets automatically.
 *
 * The "redundant" option will configure two sockets and two NIC interfaces.
 * Slaves are connected to both interfaces, one on the IN port and one on the
 * OUT port. Packets are send via both interfaces. Any one of the connections
 * (also an interconnect) can be removed and the slaves are still serviced with
 * packets. The software layer will detect the possible failure modes and
 * compensate. If needed the packets from interface A are resent through interface B.
 * This layer if fully transparent for the higher layers.
 */

#include <ioctl.h>
#include <stdio.h>
#include <string.h>
#include <rt.h>
#include <traceapi.h>
#include "hpeif2.h"

#include "soem/soem.h"
#include "nicdrv.h"

/** Redundancy modes */
enum
{
   /** No redundancy, single NIC mode */
   ECT_RED_NONE,
   /** Double redundant NIC connection */
   ECT_RED_DOUBLE
};

/** Primary source MAC address used for EtherCAT.
 * This address is not the MAC address used from the NIC.
 * EtherCAT does not care about MAC addressing, but it is used here to
 * differentiate the route the packet traverses through the EtherCAT
 * segment. This is needed to find out the packet flow in redundant
 * configurations. */
const uint16 priMAC[3] = {0x0101, 0x0101, 0x0101};
/** Secondary source MAC address used for EtherCAT. */
const uint16 secMAC[3] = {0x0404, 0x0404, 0x0404};

/** second MAC word is used for identification */
#define RX_PRIM          priMAC[1]
/** second MAC word is used for identification */
#define RX_SEC           secMAC[1]

#define ECAT_PRINT_INFO  printf
#define ECAT_PRINT_WARN  printf
#define ECAT_PRINT_ERROR printf

/* HPE port settings */
const unsigned long interrupt_mode = NO_INTERRUPT;
const unsigned long phy_settings = SPEED_100 | DUPLEX_FULL;

/** Basic setup to connect NIC to socket.
 * @param[in] ifname      = Name of NIC device, f.e. "eth0"
 * @param[in] secondary   = if >0 then use secondary stack instead of primary
 * @return >0 if succeeded
 */
int ecx_setupnic(ecx_portt *port, const char *ifname, int secondary)
{
   HPESTATUS status;
   HPEMEDIASTATUS mstat;
   time_t now, t;
   unsigned char mac[6];
   int i;
   int dontWaitForLink = 0;
   int result = 1;
   HPE_CONFIG_OPTIONS conf = {0, 0, NULL};

   status = hpeOpen(ifname, phy_settings, interrupt_mode, &(port->handle));
   if (status != E_OK)
   {
      ECAT_PRINT_ERROR("hpeOpen failed with status %04x ", status);
      if (status == E_EXIST)
         ECAT_PRINT_ERROR("E_EXIST\n");
      else if (status == E_STATE)
         ECAT_PRINT_ERROR("E_STATE\n");
      else if (status == E_PARAM)
         ECAT_PRINT_ERROR("E_PARAM\n");
      else if (status == E_INVALID_ADDR)
         ECAT_PRINT_ERROR("E_INVALID_ADDR\n");
      else if (status == E_IO)
         ECAT_PRINT_ERROR("E_IO\n");
      else if (status == E_TIME)
         ECAT_PRINT_ERROR("E_TIME\n");
      else
         ECAT_PRINT_ERROR("UNKNOWN\n");
      result = 0;
      goto end;
   }

   conf.option_flags |= OPT_PROMISC;

   status = hpeConfigOptions(port->handle, &conf, sizeof(conf));
   if (status != E_OK)
   {
      ECAT_PRINT_ERROR("hpeConfigOptions failed with status %04x\n", status);
      // NOTE: HPE driver for Intel 10/100 Mbps device currently doesn't support multicast.
      result = 0;
      goto end;
   }

   time(&now);
   do
   {
      status = hpeGetMediaStatus(port->handle, &mstat);
      if (status != E_OK)
      {
         ECAT_PRINT_ERROR("hpeGetMediaStatus failed with status %04x\n", status);
         result = 0;
         goto end;
      }
      if (mstat.media_speed == SPEED_NONE)
      {
         RtSleepEx(1000);
         time(&t);
      }
   } while (mstat.media_speed == SPEED_NONE && t < (now + 10));

   if (((mstat.media_speed & phy_settings) == 0) || ((mstat.media_duplex & phy_settings) == 0))
   {
      ECAT_PRINT_ERROR("Media not connected as requested: speed=%u, duplex=%u\n",
                       mstat.media_speed, mstat.media_duplex);
      result = 0;
      goto end;
   }

   status = hpeGetMacAddress(port->handle, mac);
   if (status != E_OK)
   {
      ECAT_PRINT_ERROR("hpeGetMacAddress failed with status %04x\n", status);
      result = 0;
      goto end;
   }

   /* allocate 2 receive buffers and attach them */
   status = hpeAllocateReceiveBufferSet(port->handle, &(port->rx_buffers), EC_MAXBUF, EC_BUFSIZE);
   if (status != E_OK)
   {
      ECAT_PRINT_ERROR("hpeAllocateReceiveBufferSet failed with status %04x\n", status);
      result = 0;
      goto end;
   }

   status = hpeAttachReceiveBufferSet(port->handle, port->rx_buffers);
   if (status != E_OK)
   {
      ECAT_PRINT_ERROR("hpeAttachReceiveBufferSet failed with status %04x\n", status);
      result = 0;
      goto end;
   }

   if (secondary)
   {
      /* secondary port struct available? */
      if (port->redport)
      {
         /* when using secondary socket it is automatically a redundant setup */
         port->redstate = ECT_RED_DOUBLE;
      }
   }
   else
   {
      port->redstate = ECT_RED_NONE;
      /* Init regions */
      port->getindex_region = CreateRtRegion(PRIORITY_QUEUING);
      port->rx_region = CreateRtRegion(PRIORITY_QUEUING);
      port->tx_region = CreateRtRegion(PRIORITY_QUEUING);
      port->lastidx = 0;
      port->stack.txbuf = &(port->txbuf);
      port->stack.txbuflength = &(port->txbuflength);
      port->stack.tempbuf = &(port->tempinbuf);
      port->stack.rxbuf = &(port->rxbuf);
      port->stack.rxbufstat = &(port->rxbufstat);
      port->stack.rxsa = &(port->rxsa);
   }

   /* setup ethernet headers in tx buffers so we don't have to repeat it */
   for (i = 0; i < EC_MAXBUF; i++)
   {
      ec_setupheader(&(port->txbuf[i]));
      port->rxbufstat[i] = EC_BUF_EMPTY;
      // allocate 1 transmit buffer of two fragments and 500 bytes and attach it
      port->tx_buffers[i] = (HPETXBUFFERSET *)AllocateRtMemory(sizeof(HPETXBUFFER) + sizeof(HPETXBUFFERSET));
      port->tx_buffers[i]->buffer_count = 1;
      port->tx_buffers[i]->buffers[0].fragment_count = 1;
      // first fragment is the fixed header, second is dynamic data
      port->tx_buffers[i]->buffers[0].fragments[0].ptr = port->txbuf[i];
      port->tx_buffers[i]->buffers[0].fragments[0].size = EC_BUFSIZE;
      port->tx_buffers[i]->buffers[0].fragments[0].paddr = 0;
   }
   ec_setupheader(&(port->txbuf2));

end:
   return result;
}

/** Close sockets used
 * @return 0
 */
int ecx_closenic(ecx_portt *port)
{
   HPESTATUS status;
   int i;

   if (port->tx_buffers)
   {
      for (i = 0; i < EC_MAXBUF; i++)
      {
         FreeRtMemory(port->tx_buffers[i]);
         port->tx_buffers[i] = NULL;
      }
   }
   if (port->rx_buffers)
   {
      hpeFreeReceiveBufferSet(port->handle, &(port->rx_buffers));
   }

   status = hpeClose(port->handle);
   if (status != E_OK)
   {
      ECAT_PRINT_ERROR("hpeClose failed with status %04x\n", status);
   }
   return 0;
}

/** Fill buffer with Ethernet header structure.
 * Destination MAC is always broadcast.
 * Ethertype is always ETH_P_ECAT.
 * @param[out] p = buffer
 */
void ec_setupheader(void *p)
{
   ec_etherheadert *bp;
   bp = (ec_etherheadert *)p;
   bp->da0 = oshw_htons(0xffff);
   bp->da1 = oshw_htons(0xffff);
   bp->da2 = oshw_htons(0xffff);
   bp->sa0 = oshw_htons(priMAC[0]);
   bp->sa1 = oshw_htons(priMAC[1]);
   bp->sa2 = oshw_htons(priMAC[2]);
   bp->etype = oshw_htons(ETH_P_ECAT);
}

/** Get new frame identifier index and allocate corresponding rx buffer.
 * @return new index.
 */
uint8 ecx_getindex(ecx_portt *port)
{
   uint8 idx;
   uint8 cnt;

   WaitForRtControl(port->getindex_region);

   idx = port->lastidx + 1;
   /* index can't be larger than buffer array */
   if (idx >= EC_MAXBUF)
   {
      idx = 0;
   }
   cnt = 0;
   /* try to find unused index */
   while ((port->rxbufstat[idx] != EC_BUF_EMPTY) && (cnt < EC_MAXBUF))
   {
      idx++;
      cnt++;
      if (idx >= EC_MAXBUF)
      {
         idx = 0;
      }
   }
   port->rxbufstat[idx] = EC_BUF_ALLOC;
   if (port->redstate != ECT_RED_NONE)
   {
      port->redport->rxbufstat[idx] = EC_BUF_ALLOC;
   }

   port->lastidx = idx;
   ReleaseRtControl();

   return idx;
}

/** Set rx buffer status.
 * @param[in] idx      = index in buffer array
 * @param[in] bufstat  = status to set
 */
void ecx_setbufstat(ecx_portt *port, uint8 idx, int bufstat)
{
   port->rxbufstat[idx] = bufstat;
   if (port->redstate != ECT_RED_NONE)
   {
      port->redport->rxbufstat[idx] = bufstat;
   }
}

/** Transmit buffer over socket (non blocking).
 * @param[in] idx         = index in tx buffer array
 * @param[in] stacknumber  = 0=Primary 1=Secondary stack
 * @return socket send result
 */
int ecx_outframe(ecx_portt *port, uint8 idx, int stacknumber)
{
   HPESTATUS status;
   DWORD txstate;
   int lp;
   ec_stackT *stack;
   int result = 0;
   int retries = 1024;

   if (!stacknumber)
   {
      stack = &(port->stack);
   }
   else
   {
      stack = &(port->redport->stack);
   }
   lp = (*stack->txbuflength)[idx];
   port->tx_buffers[idx]->buffer_count = 1;
   port->tx_buffers[idx]->buffers[0].fragments[0].size = lp;

   // wait for transmit to complete
   do
   {
      result = hpeGetTransmitterState(port->handle, &txstate) == E_OK && txstate == HPE_TXBUSY;
   } while (result && retries-- > 0);

   if (result)
   {
      result = -1;
      goto end;
   }

   log_RT_event('S', (WORD)2);
   status = hpeAttachTransmitBufferSet(port->handle, port->tx_buffers[idx]);
   if (status != E_OK)
   {
      result = -2;
      goto end;
   }
   log_RT_event('S', (WORD)3);

   (*stack->rxbufstat)[idx] = EC_BUF_TX;
   status = hpeStartTransmitter(port->handle);
   if (status != E_OK)
   {
      (*stack->rxbufstat)[idx] = EC_BUF_EMPTY;
      result = -3;
      goto end;
   }

   log_RT_event('S', (WORD)4);
   result = lp;

end:

   return result;
}

/** Transmit buffer over socket (non blocking).
 * @param[in] idx         = index in tx buffer array
 * @return socket send result
 */
int ecx_outframe_red(ecx_portt *port, uint8 idx)
{
   HPESTATUS status;
   ec_comt *datagramP;
   ec_etherheadert *ehp;
   int rval;

   ehp = (ec_etherheadert *)&(port->txbuf[idx]);
   /* rewrite MAC source address 1 to primary */
   ehp->sa1 = oshw_htons(priMAC[1]);
   /* transmit over primary socket*/
   rval = ecx_outframe(port, idx, 0);
   if (port->redstate != ECT_RED_NONE)
   {
      ehp = (ec_etherheadert *)&(port->txbuf2);
      /* use dummy frame for secondary socket transmit (BRD) */
      datagramP = (ec_comt *)&(port->txbuf2)[ETH_HEADERSIZE];
      /* write index to frame */
      datagramP->index = idx;
      /* rewrite MAC source address 1 to secondary */
      ehp->sa1 = oshw_htons(secMAC[1]);
      /* transmit over secondary socket */
      // send(sockhandle2, &ec_txbuf2, ec_txbuflength2 , 0);
      //  OBS! redundant not ACTIVE for BFIN, just added to compile
      // ASSERT (0);
      hpeAttachTransmitBufferSet(port->redport->handle, port->tx_buffers[idx]);
      port->redport->rxbufstat[idx] = EC_BUF_TX;
      status = hpeStartTransmitter(port->redport->handle);
      if (status != E_OK)
      {
         (*stack->rxbufstat)[idx] = EC_BUF_EMPTY;
      }
   }

   return rval;
}

/** Non blocking read of socket. Put frame in temporary buffer.
 * @param[in] stacknumber = 0=primary 1=secondary stack
 * @return >0 if frame is available and read
 */
static int ecx_recvpkt(ecx_portt *port, int stacknumber)
{
   HPEBUFFER *rxbuffer;
   HPESTATUS status;
   ec_stackT *stack;
   int bytesrx = 0;

   if (!stacknumber)
   {
      stack = &(port->stack);
   }
   else
   {
      stack = &(port->redport->stack);
   }

   log_RT_event('R', (WORD)2);

   status = hpeGetReceiveBuffer(port->handle, &rxbuffer);
   if (status == E_OK)
   {
      memcpy(stack->tempbuf, rxbuffer->ptr, rxbuffer->used);
      bytesrx = rxbuffer->used;
      port->tempinbufs = bytesrx;
      // TODO case no interrupt
   }
   log_RT_event('R', (WORD)3);

   return (bytesrx > 0);
}

/** Non blocking receive frame function. Uses RX buffer and index to combine
 * read frame with transmitted frame. To compensate for received frames that
 * are out-of-order all frames are stored in their respective indexed buffer.
 * If a frame was placed in the buffer previously, the function retrieves it
 * from that buffer index without calling ec_recvpkt. If the requested index
 * is not already in the buffer it calls ec_recvpkt to fetch it. There are
 * three options now, 1 no frame read, so exit. 2 frame read but other
 * than requested index, store in buffer and exit. 3 frame read with matching
 * index, store in buffer, set completed flag in buffer status and exit.
 *
 * @param[in] idx         = requested index of frame
 * @param[in] stacknumber = 0=primary 1=secondary stack
 * @return Workcounter if a frame is found with corresponding index, otherwise
 * EC_NOFRAME or EC_OTHERFRAME.
 */
int ecx_inframe(ecx_portt *port, uint8 idx, int stacknumber)
{
   uint16 l;
   int rval;
   uint8 idxf;
   ec_etherheadert *ehp;
   ec_comt *ecp;
   ec_stackT *stack;
   ec_bufT *rxbuf;

   if (!stacknumber)
   {
      stack = &(port->stack);
   }
   else
   {
      stack = &(port->redport->stack);
   }

   rval = EC_NOFRAME;
   rxbuf = &(*stack->rxbuf)[idx];
   /* check if requested index is already in buffer ? */
   if ((idx < EC_MAXBUF) && ((*stack->rxbufstat)[idx] == EC_BUF_RCVD))
   {
      l = (*rxbuf)[0] + ((uint16)((*rxbuf)[1] & 0x0f) << 8);
      /* return WKC */
      rval = ((*rxbuf)[l] + ((uint16)(*rxbuf)[l + 1] << 8));
      /* mark as completed */
      (*stack->rxbufstat)[idx] = EC_BUF_COMPLETE;
   }
   else
   {
      WaitForRtControl(port->rx_region);
      /* non blocking call to retrieve frame from socket */
      if (ecx_recvpkt(port, stacknumber))
      {
         rval = EC_OTHERFRAME;
         ehp = (ec_etherheadert *)(stack->tempbuf);
         /* check if it is an EtherCAT frame */
         if (ehp->etype == oshw_htons(ETH_P_ECAT))
         {
            ecp = (ec_comt *)(&(*stack->tempbuf)[ETH_HEADERSIZE]);
            l = etohs(ecp->elength) & 0x0fff;
            idxf = ecp->index;
            /* found index equals requested index ? */
            if (idxf == idx)
            {
               /* yes, put it in the buffer array (strip ethernet header) */
               memcpy(rxbuf, &(*stack->tempbuf)[ETH_HEADERSIZE], (*stack->txbuflength)[idx] - ETH_HEADERSIZE);
               /* return WKC */
               rval = ((*rxbuf)[l] + ((uint16)((*rxbuf)[l + 1]) << 8));
               /* mark as completed */
               (*stack->rxbufstat)[idx] = EC_BUF_COMPLETE;
               /* store MAC source word 1 for redundant routing info */
               (*stack->rxsa)[idx] = oshw_ntohs(ehp->sa1);
            }
            else
            {
               /* check if index exist and someone is waiting for it */
               if (idxf < EC_MAXBUF && (*stack->rxbufstat)[idxf] == EC_BUF_TX)
               {
                  rxbuf = &(*stack->rxbuf)[idxf];
                  /* put it in the buffer array (strip ethernet header) */
                  memcpy(rxbuf, &(*stack->tempbuf)[ETH_HEADERSIZE], (*stack->txbuflength)[idxf] - ETH_HEADERSIZE);
                  /* mark as received */
                  (*stack->rxbufstat)[idxf] = EC_BUF_RCVD;
                  (*stack->rxsa)[idxf] = oshw_ntohs(ehp->sa1);
               }
            }
         }
      }
      ReleaseRtControl();
   }

   /* WKC if matching frame found */
   return rval;
}

/** Blocking redundant receive frame function. If redundant mode is not active then
 * it skips the secondary stack and redundancy functions. In redundant mode it waits
 * for both (primary and secondary) frames to come in. The result goes in an decision
 * tree that decides, depending on the route of the packet and its possible missing arrival,
 * how to reroute the original packet to get the data in an other try.
 *
 * @param[in] idx = requested index of frame
 * @param[in] timer = absolute timeout time
 * @return Workcounter if a frame is found with corresponding index, otherwise
 * EC_NOFRAME.
 */
static int ecx_waitinframe_red(ecx_portt *port, uint8 idx, osal_timert *timer)
{
   osal_timert timer2;
   int wkc = EC_NOFRAME;
   int wkc2 = EC_NOFRAME;
   int primrx, secrx;

   /* if not in redundant mode then always assume secondary is OK */
   if (port->redstate == ECT_RED_NONE)
   {
      wkc2 = 0;
   }
   do
   {
      /* only read frame if not already in */
      if (wkc <= EC_NOFRAME)
         wkc = ecx_inframe(port, idx, 0);
      /* only try secondary if in redundant mode */
      if (port->redstate != ECT_RED_NONE)
      {
         /* only read frame if not already in */
         if (wkc2 <= EC_NOFRAME)
         {
            wkc2 = ecx_inframe(port, idx, 1);
         }
      }
      /* wait for both frames to arrive or timeout */
   } while (((wkc <= EC_NOFRAME) || (wkc2 <= EC_NOFRAME)) && !osal_timer_is_expired(timer));
   /* only do redundant functions when in redundant mode */
   if (port->redstate != ECT_RED_NONE)
   {
      /* primrx if the received MAC source on primary socket */
      primrx = 0;
      if (wkc > EC_NOFRAME) primrx = port->rxsa[idx];
      /* secrx if the received MAC source on psecondary socket */
      secrx = 0;
      if (wkc2 > EC_NOFRAME) secrx = port->redport->rxsa[idx];

      /* primary socket got secondary frame and secondary socket got primary frame */
      /* normal situation in redundant mode */
      if (((primrx == RX_SEC) && (secrx == RX_PRIM)))
      {
         /* copy secondary buffer to primary */
         memcpy(&(port->rxbuf[idx]), &(port->redport->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
         wkc = wkc2;
      }
      /* primary socket got nothing or primary frame, and secondary socket got secondary frame */
      /* we need to resend TX packet */
      if (((primrx == 0) && (secrx == RX_SEC)) ||
          ((primrx == RX_PRIM) && (secrx == RX_SEC)))
      {
         /* If both primary and secondary have partial connection retransmit the primary received
          * frame over the secondary socket. The result from the secondary received frame is a combined
          * frame that traversed all slaves in standard order. */
         if ((primrx == RX_PRIM) && (secrx == RX_SEC))
         {
            /* copy primary rx to tx buffer */
            memcpy(&(port->txbuf[idx][ETH_HEADERSIZE]), &(port->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
         }
         osal_timer_start(&timer2, EC_TIMEOUTRET);
         /* resend secondary tx */
         ecx_outframe(port, idx, 1);
         do
         {
            /* retrieve frame */
            wkc2 = ecx_inframe(port, idx, 1);
         } while ((wkc2 <= EC_NOFRAME) && !osal_timer_is_expired(&timer2));
         if (wkc2 > EC_NOFRAME)
         {
            /* copy secondary result to primary rx buffer */
            memcpy(&(port->rxbuf[idx]), &(port->redport->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
            wkc = wkc2;
         }
      }
   }

   /* return WKC or EC_NOFRAME */
   return wkc;
}

/** Blocking receive frame function. Calls ec_waitinframe_red().
 * @param[in] idx       = requested index of frame
 * @param[in] timeout   = timeout in us
 * @return Workcounter if a frame is found with corresponding index, otherwise
 * EC_NOFRAME.
 */
int ecx_waitinframe(ecx_portt *port, uint8 idx, int timeout)
{
   int wkc;
   osal_timert timer;

   if (timeout == 0 && (port->redstate == ECT_RED_NONE))
   {
      int loop_cnt = 0;
      /* Allow us to consume MAX buffer number of frames */
      do
      {
         wkc = ecx_inframe(port, idx, 0);
         loop_cnt++;
      } while (wkc <= EC_NOFRAME && (loop_cnt <= EC_MAXBUF));
   }
   else
   {
      osal_timer_start(&timer, timeout);
      wkc = ecx_waitinframe_red(port, idx, &timer);
   }

   return wkc;
}

/** Blocking send and receive frame function. Used for non processdata frames.
 * A datagram is build into a frame and transmitted via this function. It waits
 * for an answer and returns the workcounter. The function retries if time is
 * left and the result is WKC=0 or no frame received.
 *
 * The function calls ec_outframe_red() and ec_waitinframe_red().
 *
 * @param[in] idx      = index of frame
 * @param[in] timeout  = timeout in us
 * @return Workcounter or EC_NOFRAME
 */
int ecx_srconfirm(ecx_portt *port, uint8 idx, int timeout)
{
   int wkc = EC_NOFRAME;
   osal_timert timer1;

   osal_timer_start(&timer1, timeout);
   /* tx frame on primary and if in redundant mode a dummy on secondary */
   ecx_outframe_red(port, idx);
   wkc = ecx_waitinframe_red(port, idx, &timer1);

   return wkc;
}
