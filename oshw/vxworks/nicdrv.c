/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * EtherCAT VxWorks SNARF Mux driver.
 *
 * Low level interface functions to send and receive EtherCAT packets.
 * EtherCAT has the property that packets are only send by the master,
 * and the send packets always return in the receive buffer.
 * There can be multiple packets "on the wire" before they return.
 * To combine the received packets with the original send packets a buffer
 * system is installed. The identifier is put in the index item of the
 * EtherCAT header. The index is stored and compared when a frame is received.
 * If there is a match the packet can be combined with the transmit packet
 * and returned to the higher level function.
 *
 * If EtherCAT is run in parallel with normal IP traffic and EtherCAT have a 
 * dedicated NIC, instantiate an extra tNetX task and redirect the NIC workQ
 * to be handle by the extra tNetX task, if needed raise the tNetX task prio. 
 * This prevents from having tNet0 becoming a bottleneck.
 *
 * The "redundant" option will configure two Mux drivers and two NIC interfaces.
 * Slaves are connected to both interfaces, one on the IN port and one on the
 * OUT port. Packets are send via both interfaces. Any one of the connections
 * (also an interconnect) can be removed and the slaves are still serviced with
 * packets. The software layer will detect the possible failure modes and
 * compensate. If needed the packets from interface A are resend through interface B.
 * This layer if fully transparent for the higher layers.
 */

#include <vxWorks.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <muxLib.h>
#include <ipProto.h>
#include <wvLib.h>
#include <sysLib.h>

#include "oshw.h"
#include "osal.h"
#include "nicdrv.h"

#define NIC_DEBUG                /* Print debugging info? */

// wvEvent flags
#define ECAT_RECV_FAILED    0x664
#define ECAT_RECV_OK        0x665
#define ECAT_RECV_RETRY_OK  0x666
#define ECAT_STACK_RECV     0x667
#define ECAT_SEND_START     0x675
#define ECAT_SEND_COMPLETE  0x676
#define ECAT_SEND_FAILED    0x677

#ifdef NIC_DEBUG

#define NIC_LOGMSG(x,a,b,c,d,e,f)   \
    do  {                           \
        logMsg (x,a,b,c,d,e,f);     \
    } while (0)
#define NIC_WVEVENT(a,b,c)          \
    do  {                           \
        wvEvent(a, b, c);          \
        } while (0)


#else
#define NIC_LOGMSG(x,a,b,c,d,e,f)
#define NIC_WVEVENT(a,b,c)
#endif /* NIC_DEBUG */

#define IF_NAME_SIZE 8

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
const uint16 priMAC[3] = { 0x0101, 0x0101, 0x0101 };
/** Secondary source MAC address used for EtherCAT. */
const uint16 secMAC[3] = { 0x0404, 0x0404, 0x0404 };

/** second MAC word is used for identification */
#define RX_PRIM priMAC[1]
/** second MAC word is used for identification */
#define RX_SEC secMAC[1]

/* usec per tick for timeconversion, default to 1kHz */
#define USECS_PER_SEC 1000000
static unsigned int usec_per_tick = 1000;

/** Receive hook called by Mux driver. */
static int mux_rx_callback(void* pCookie, long type, M_BLK_ID pMblk, LL_HDR_INFO *llHdrInfo, void *muxUserArg);

static void ecx_clear_rxbufstat(int *rxbufstat)
{
   int i;
   for(i = 0; i < EC_MAXBUF; i++)
   {
      rxbufstat[i] = EC_BUF_EMPTY;
   }
}

void print_nicversion(void)
{
    printf("Generic is used\n");
}

/** Basic setup to connect NIC to socket.
* @param[in] port        = port context struct
* @param[in] ifname      = Name of NIC device, f.e. "gei0"
* @param[in] secondary   = if >0 then use secondary stack instead of primary
* @return >0 if succeeded
*/
int ecx_setupnic(ecx_portt *port, const char *ifname, int secondary) 
{
   int i;
   char ifn[IF_NAME_SIZE];
   int unit_no = -1;   
   ETHERCAT_PKT_DEV * pPktDev;

   /* Get systick info, sysClkRateGet return ticks per second */
   usec_per_tick =  USECS_PER_SEC / sysClkRateGet();
   /* Don't allow 0 since it is used in DIV */
   if(usec_per_tick == 0)
      usec_per_tick = 1;
   /* Make reference to packet device struct, keep track if the packet
    * device is the redundant or not.
    */
   if (secondary)
   {
       pPktDev = &(port->redport->pktDev);
       pPktDev->redundant = 1;
   }
   else
   {
       pPktDev = &(port->pktDev);
       pPktDev->redundant = 0;
   }

   /* Clear frame counters*/
   pPktDev->tx_count = 0;
   pPktDev->rx_count = 0;
   pPktDev->overrun_count = 0;

   /* Create multi-thread support semaphores */
   port->sem_get_index = semMCreate(SEM_Q_PRIORITY | SEM_INVERSION_SAFE);
   
   /* Get the dev name and unit from ifname 
    * We assume form gei1, fei0... 
    */
   memset(ifn,0x0,sizeof(ifn));
   
   for(i=0; i < strlen(ifname);i++)
   {
       if(isdigit(ifname[i]))
       {
          strncpy(ifn, ifname, i);
          unit_no = atoi(&ifname[i]);
          break;
       }
   }

   /* Detach IP stack */
   //ipDetach(pktDev.unit,pktDev.name);

   pPktDev->port = port;

   /* Bind to mux driver for given interface, include ethercat driver pointer 
     * as user reference 
     */
   /* Bind to mux */
   pPktDev->pCookie = muxBind(ifn,
                              unit_no, 
                              mux_rx_callback, 
                              NULL, 
                              NULL, 
                              NULL, 
                              MUX_PROTO_SNARF, 
                              "ECAT SNARF", 
                              pPktDev);

   if (pPktDev->pCookie == NULL)
   {
      /* fail */
      NIC_LOGMSG("ecx_setupnic: muxBind init for gei: %d failed\n", 
                 unit_no, 2, 3, 4, 5, 6);
      goto exit;
   }

   /* Get reference tp END obje */
   pPktDev->endObj = endFindByName(ifn, unit_no);

   if (port->pktDev.endObj == NULL)
   {
      /* fail */
      NIC_LOGMSG("error_hook:  endFindByName failed, device gei: %d not found\n",
                 unit_no, 2, 3, 4, 5, 6);
      goto exit;
   }

   if (secondary)
   {
      /* secondary port struct available? */
      if (port->redport)
      {
         port->redstate                   = ECT_RED_DOUBLE;
         port->redport->stack.txbuf       = &(port->txbuf);
         port->redport->stack.txbuflength = &(port->txbuflength);
         port->redport->stack.rxbuf       = &(port->redport->rxbuf);
         port->redport->stack.rxbufstat   = &(port->redport->rxbufstat);
         port->redport->stack.rxsa        = &(port->redport->rxsa);
         /* Create mailboxes for each potential EtherCAT frame index */
         for (i = 0; i < EC_MAXBUF; i++)
         {
             port->redport->msgQId[i] = msgQCreate(1, sizeof(M_BLK_ID), MSG_Q_FIFO);
             if (port->redport->msgQId[i] == MSG_Q_ID_NULL)
             {
                 NIC_LOGMSG("ecx_setupnic: Failed to create redundant MsgQ[%d]",
                     i, 2, 3, 4, 5, 6);
                goto exit;
             }
         }
         ecx_clear_rxbufstat(&(port->redport->rxbufstat[0]));
      }
      else
      {
         /* fail */
         NIC_LOGMSG("ecx_setupnic: Redundant port not allocated",
                    unit_no, 2, 3, 4, 5, 6);
         goto exit;
      }
   }
   else
   {
      port->lastidx           = 0;
      port->redstate          = ECT_RED_NONE;
      port->stack.txbuf       = &(port->txbuf);
      port->stack.txbuflength = &(port->txbuflength);
      port->stack.rxbuf       = &(port->rxbuf);
      port->stack.rxbufstat   = &(port->rxbufstat);
      port->stack.rxsa        = &(port->rxsa);
      /* Create mailboxes for each potential EtherCAT frame index */
      for (i = 0; i < EC_MAXBUF; i++)
      {
         port->msgQId[i] = msgQCreate(1, sizeof(M_BLK_ID), MSG_Q_FIFO);
         if (port->msgQId[i] == MSG_Q_ID_NULL)
         {
            NIC_LOGMSG("ecx_setupnic: Failed to create MsgQ[%d]",
                       i, 2, 3, 4, 5, 6);
            goto exit;
         }
      }
      ecx_clear_rxbufstat(&(port->rxbufstat[0]));
   }

   /* setup ethernet headers in tx buffers so we don't have to repeat it */
   for (i = 0; i < EC_MAXBUF; i++) 
   {
      ec_setupheader(&(port->txbuf[i]));
      port->rxbufstat[i] = EC_BUF_EMPTY;
   }
   ec_setupheader(&(port->txbuf2));

   return 1;

exit:

   return 0;

}

/** Close sockets used
 * @param[in] port        = port context struct
 * @return 0
 */
int ecx_closenic(ecx_portt *port) 
{
   int i;
   ETHERCAT_PKT_DEV * pPktDev;
   M_BLK_ID trash_can;

   pPktDev = &(port->pktDev);

   for (i = 0; i < EC_MAXBUF; i++)
   {
      if (port->msgQId[i] != MSG_Q_ID_NULL)
      {
         if (msgQReceive(port->msgQId[i], 
                         (char *)&trash_can, 
                         sizeof(M_BLK_ID), 
                         NO_WAIT) != ERROR)
         {
            NIC_LOGMSG("ecx_closenic: idx %d MsgQ close\n", i,
                        2, 3, 4, 5, 6);
                  /* Free resources */
            netMblkClChainFree(trash_can);
         }
         msgQDelete(port->msgQId[i]);
       }
   }

   if (pPktDev->pCookie != NULL)
   {
      muxUnbind(pPktDev->pCookie, MUX_PROTO_SNARF, mux_rx_callback);
   }

   /* Clean redundant resources if available*/
   if (port->redport)
   {
      pPktDev = &(port->redport->pktDev);
      for (i = 0; i < EC_MAXBUF; i++)
      {
         if (port->redport->msgQId[i] != MSG_Q_ID_NULL)
         {
            if (msgQReceive(port->redport->msgQId[i], 
                            (char *)&trash_can, 
                            sizeof(M_BLK_ID), 
                            NO_WAIT) != ERROR)
            {
               NIC_LOGMSG("ecx_closenic: idx %d MsgQ close\n", i,
                          2, 3, 4, 5, 6);
               /* Free resources */
               netMblkClChainFree(trash_can);
            }
            msgQDelete(port->redport->msgQId[i]);
         }
      }
      if (pPktDev->pCookie != NULL)
      {
         muxUnbind(pPktDev->pCookie, MUX_PROTO_SNARF, mux_rx_callback);
      }
   }
      
   return 0;
}

/** Fill buffer with ethernet header structure.
 * Destination MAC is always broadcast.
 * Ethertype is always ETH_P_ECAT.
 * @param[out] p = buffer
 */
void ec_setupheader(void *p) 
{
   ec_etherheadert *bp;
   bp = p;
   bp->da0 = htons(0xffff);
   bp->da1 = htons(0xffff);
   bp->da2 = htons(0xffff);
   bp->sa0 = htons(priMAC[0]);
   bp->sa1 = htons(priMAC[1]);
   bp->sa2 = htons(priMAC[2]);
   bp->etype = htons(ETH_P_ECAT);
}

/** Get new frame identifier index and allocate corresponding rx buffer.
 * @param[in] port        = port context struct
 * @return new index.
 */
uint8 ecx_getindex(ecx_portt *port)
{
   uint8 idx;
   uint8 cnt;

   semTake(port->sem_get_index, WAIT_FOREVER);
   
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
   port->lastidx = idx;
   
   semGive(port->sem_get_index);
   
   return idx;
}

/** Set rx buffer status.
 * @param[in] port        = port context struct
 * @param[in] idx      = index in buffer array
 * @param[in] bufstat  = status to set
 */
void ecx_setbufstat(ecx_portt *port, uint8 idx, int bufstat)
{
   port->rxbufstat[idx] = bufstat;
   if (port->redstate != ECT_RED_NONE)
      port->redport->rxbufstat[idx] = bufstat;
}

/** Low level transmit buffer over mux layer 2 driver
* 
* @param[in] pPktDev     = packet device to send buffer over
* @param[in] idx         = index in tx buffer array
* @param[in] buf         = buff to send
* @param[in] len         = bytes to send
* @return driver send result
*/
static int ec_outfram_send(ETHERCAT_PKT_DEV * pPktDev, uint8 idx, void * buf, int len)
{
   STATUS status = OK;
   M_BLK_ID pPacket = NULL;
   int rval = 0;
   END_OBJ *endObj = (END_OBJ *)pPktDev->endObj;
   MSG_Q_ID  msgQId;

   /* Clean up any abandoned frames and re-use the allocated buffer*/
   msgQId = pPktDev->port->msgQId[idx];
   if(msgQNumMsgs(msgQId) > 0)
   {
      pPktDev->abandoned_count++;
      NIC_LOGMSG("ec_outfram_send: idx %d MsgQ abandoned\n", idx,
                 2, 3, 4, 5, 6);
      if (msgQReceive(msgQId, 
                     (char *)&pPacket, 
                     sizeof(M_BLK_ID), 
                     NO_WAIT) == ERROR)
      {
         pPacket = NULL;
         NIC_LOGMSG("ec_outfram_send: idx %d MsgQ mBlk handled by receiver\n", idx,
                    2, 3, 4, 5, 6);
      }
   }

   if (pPacket == NULL)
   {
      /* Allocate m_blk to send */
      if ((pPacket = netTupleGet(endObj->pNetPool,
                                 len,
                                 M_DONTWAIT,
                                 MT_DATA,
                                 TRUE)) == NULL)
      {
         NIC_LOGMSG("ec_outfram_send: Could not allocate MBLK memory!\n", 1, 2, 3, 4, 5, 6);
         return ERROR;
      }
   }

   pPacket->mBlkHdr.mLen = len;
   pPacket->mBlkHdr.mFlags |= M_PKTHDR;
   pPacket->mBlkHdr.mData = pPacket->pClBlk->clNode.pClBuf;
   pPacket->mBlkPktHdr.len = pPacket->m_len;

   netMblkFromBufCopy(pPacket, buf, 0, pPacket->mBlkHdr.mLen, M_DONTWAIT, NULL);
   status = muxSend(endObj, pPacket);

   if (status == OK)
   {
      rval = len;
      NIC_WVEVENT(ECAT_SEND_COMPLETE, (char *)&rval, sizeof(rval));
   }
   else
   {
      netMblkClChainFree(pPacket);
      /* fail */
      static int print_once;
      if (print_once == 0)
      {
         NIC_LOGMSG("ec_outfram_send: failed\n",
                     1, 2, 3, 4, 5, 6);
         print_once = 1;
      }
      NIC_WVEVENT(ECAT_SEND_FAILED, (char *)&rval, sizeof(rval));
   }

   return rval;
}

/** High level transmit buffer over mux layer 2 driver, passing buffer 
* and packet device to send on as arguments
* @param[in] port         = port context holding reference to packet device
* @param[in] idx          = index in tx buffer array
* @param[in] stacknumber  = 0=Primary 1=Secondary stack
* @return socket send result
*/
int ecx_outframe(ecx_portt *port, uint8 idx, int stacknumber)
{
   int rval = 0;
   ec_stackT *stack;
   ETHERCAT_PKT_DEV * pPktDev;

   if (!stacknumber)
   {
      stack = &(port->stack);
      pPktDev = &(port->pktDev);
   }
   else
   {
      stack = &(port->redport->stack);
      pPktDev = &(port->redport->pktDev);
   }

   (*stack->rxbufstat)[idx] = EC_BUF_TX;
   rval = ec_outfram_send(pPktDev, idx, (char*)(*stack->txbuf)[idx], 
                          (*stack->txbuflength)[idx]);
   if (rval > 0)
   {
      port->pktDev.tx_count++;
   }
   else
   {
      (*stack->rxbufstat)[idx] = EC_BUF_EMPTY;
   }
   
   return rval;
}

/** High level transmit frame to send as index.
 * @param[in] port  = port context
 * @param[in] idx   = index in tx buffer array
 * @return socket send result
 */
int ecx_outframe_red(ecx_portt *port, uint8 idx)
{
   ec_comt *datagramP;
   ec_etherheadert *ehp;
   int rval;

   ehp = (ec_etherheadert *)&(port->txbuf[idx]);
   /* rewrite MAC source address 1 to primary */
   ehp->sa1 = htons(priMAC[1]);
   /* transmit over primary socket*/
   rval = ecx_outframe(port, idx, 0);
   if (port->redstate != ECT_RED_NONE)
   {   
      ehp = (ec_etherheadert *)&(port->txbuf2);
      /* use dummy frame for secondary socket transmit (BRD) */
      datagramP = (ec_comt*)&(port->txbuf2[ETH_HEADERSIZE]);
      /* write index to frame */
      datagramP->index = idx;
      /* rewrite MAC source address 1 to secondary */
      ehp->sa1 = htons(secMAC[1]);
      /* transmit over secondary interface */
      port->redport->rxbufstat[idx] = EC_BUF_TX;
      rval = ec_outfram_send(&(port->redport->pktDev), idx, &(port->txbuf2), port->txbuflength2);
      if (rval <= 0)
      {
         port->redport->rxbufstat[idx] = EC_BUF_EMPTY;
      }
   }
   
   return rval;
}


/** Call back routine registered as hook with mux layer 2 driver 
* @param[in] pCookie      = Mux cookie
* @param[in] type         = received type
* @param[in] pMblk        = the received packet reference
* @param[in] llHdrInfo    = header info
* @param[in] muxUserArg   = assigned reference to packet device when init called
* @return TRUE if frame was successfully read and passed to MsgQ
*/
static int mux_rx_callback(void* pCookie, long type, M_BLK_ID pMblk, LL_HDR_INFO *llHdrInfo, void *muxUserArg)
{
   BOOL ret = FALSE;
   uint8 idxf;
   ec_comt *ecp;
   ec_bufT * tempbuf;
   ecx_portt * port;
   MSG_Q_ID  msgQId;
   ETHERCAT_PKT_DEV * pPktDev;
   int  length;
   int bufstat;

   /* check if it is an EtherCAT frame */
   if (type == ETH_P_ECAT)
   {
      length = pMblk->mBlkHdr.mLen;
      tempbuf = (ec_bufT *)pMblk->mBlkHdr.mData;
      pPktDev = (ETHERCAT_PKT_DEV *)muxUserArg;
      port = pPktDev->port;

      /* Get ethercat frame header */
      ecp = (ec_comt*)&(*tempbuf)[ETH_HEADERSIZE];
      idxf = ecp->index;
      if (idxf >= EC_MAXBUF)
      {
         NIC_LOGMSG("mux_rx_callback: idx %d out of bounds\n", idxf,
                    2, 3, 4, 5, 6);
         return ret;
      }

      /* Check if it is the redundant port or not */
      if (pPktDev->redundant == 1)
      {
         bufstat = port->redport->rxbufstat[idxf];
         msgQId = port->redport->msgQId[idxf];
      }
      else
      {
         bufstat = port->rxbufstat[idxf];
         msgQId = port->msgQId[idxf];
      }

      /* Check length and if someone expects the frame */
      if (length > 0 && bufstat == EC_BUF_TX)
      {
         /* Post the frame to the receive Q for the EtherCAT stack */
         STATUS status;
         status = msgQSend(msgQId, (char *)&pMblk, sizeof(M_BLK_ID),
                           NO_WAIT, MSG_PRI_NORMAL);
         if (status == OK)
         {
            NIC_WVEVENT(ECAT_RECV_OK, (char *)&length, sizeof(length));
            ret = TRUE;
         }
         else if ((status == ERROR) && (errno == S_objLib_OBJ_UNAVAILABLE))
         {
            /* Try to empty the MSGQ since we for some strange reason
             * already have a frame in the MsqQ,
             * is it due to timeout when receiving?
             * We want the latest received frame in the buffer
             */
            port->pktDev.overrun_count++;
            NIC_LOGMSG("mux_rx_callback: idx %d MsgQ overrun\n", idxf,
                       2, 3, 4, 5, 6);
            M_BLK_ID trash_can;
            if (msgQReceive(msgQId, 
                            (char *)&trash_can,
                            sizeof(M_BLK_ID), 
                            NO_WAIT) != ERROR)
            {
                /* Free resources */
                netMblkClChainFree(trash_can);
            }
            status = msgQSend(msgQId, 
                             (char *)&pMblk, 
                              sizeof(M_BLK_ID),
                              NO_WAIT, 
                              MSG_PRI_NORMAL);
            if (status == OK)
            {
               NIC_WVEVENT(ECAT_RECV_RETRY_OK, (char *)&length, sizeof(length));
               ret = TRUE;
            }
         }
         else
         {
            NIC_WVEVENT(ECAT_RECV_FAILED, (char *)&length, sizeof(length));
         }
      }
   }

   return ret;
}

/** Non blocking or blocking read, if we use timeout we pass minimum 1 tick.
 * @param[in] port       = port context struct
 * @param[in] pMblk      = mBlk for received frame
 * @param[in] timeout    = timeout in us
 * @return >0 if frame is available and read
 */
static int ecx_recvpkt(ecx_portt *port, uint8 idx, int stacknumber, M_BLK_ID * pMblk, int timeout)
{
   int bytesrx = 0;
   MSG_Q_ID  msgQId;
   int tick_timeout = max((timeout / usec_per_tick), 1);
   
   if (stacknumber == 1)
   {
      msgQId = port->redport->msgQId[idx];
   }
   else
   {
      msgQId = port->msgQId[idx];
   }
   
   if (timeout == 0)
   {
       bytesrx = msgQReceive(msgQId, (void *)pMblk,
                         sizeof(M_BLK_ID), NO_WAIT);
   }
   else
   {
       bytesrx = msgQReceive(msgQId, (void *)pMblk,
                         sizeof(M_BLK_ID), tick_timeout);
   }

   if (bytesrx > 0)
   {
       bytesrx = (*pMblk)->mBlkHdr.mLen;
       NIC_WVEVENT(ECAT_STACK_RECV, (char *)&bytesrx, sizeof(bytesrx));
   }

     
   return (bytesrx > 0);
}

/** Non blocking receive frame function. Uses RX buffer and index to combine
 * read frame with transmitted frame. Frames are received by separate receiver
 * task tNet0 (default), tNet0 fetch what frame index and store a reference to the 
 * received frame in matching MsgQ. The stack user tasks fetch the frame 
 * reference and copies the frame the the RX buffer, when done it free
 * the frame buffer allocated by the Mux.
 * 
 * @param[in] port        = port context struct
 * @param[in] idx         = requested index of frame
 * @param[in] stacknumber = 0=primary 1=secondary stack
 * @param[in] timeout     = timeout in us
 * @return Workcounter if a frame is found with corresponding index, otherwise
 * EC_NOFRAME or EC_OTHERFRAME.
 */
int ecx_inframe(ecx_portt *port, uint8 idx, int stacknumber, int timeout)
{
   uint16  l;
   int     rval;
   ec_etherheadert *ehp;
   ec_comt *ecp;
   ec_stackT *stack;
   ec_bufT *rxbuf;
   ec_bufT *tempinbuf;
   M_BLK_ID pMblk;

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
   /* (non-) blocking call to retrieve frame from Muxlayer */
   if (ecx_recvpkt(port, idx, stacknumber, &pMblk, timeout))
   {
       rval = EC_OTHERFRAME;

       /* Get pointer to the frame */
       tempinbuf = (ec_bufT *)pMblk->mBlkHdr.mData;
       /* Get pointer to the Ethernet header */
       ehp = (ec_etherheadert*)tempinbuf;
       /* Get pointer to the EtherCAT frame as ethernet payload */
       ecp = (ec_comt*)&(*tempinbuf)[ETH_HEADERSIZE];
       l = etohs(ecp->elength) & 0x0fff;
       /* yes, put it in the buffer array (strip ethernet header) */
       netMblkOffsetToBufCopy(pMblk, ETH_HEADERSIZE,(void *) rxbuf,
                             (*stack->txbuflength)[idx] - ETH_HEADERSIZE, NULL);

       /* return WKC */
       rval = ((*rxbuf)[l] + ((uint16)((*rxbuf)[l + 1]) << 8));
       /* mark as completed */
       (*stack->rxbufstat)[idx] = EC_BUF_COMPLETE;
       /* store MAC source word 1 for redundant routing info */
       (*stack->rxsa)[idx] = ntohs(ehp->sa1);
       netMblkClChainFree(pMblk);
       port->pktDev.rx_count++;
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
 * @param[in] port  = port context struct
 * @param[in] idx   = requested index of frame
 * @param[in] timer = absolute timeout time
 * @param[in] timeout     = timeout in us
 * @return Workcounter if a frame is found with corresponding index, otherwise
 * EC_NOFRAME.
 */
static int ecx_waitinframe_red(ecx_portt *port, uint8 idx, osal_timert *timer, int timeout)
{
   osal_timert timer2;
   int wkc  = EC_NOFRAME;
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
      {
         wkc = ecx_inframe(port, idx, 0, timeout);
      }
      /* only try secondary if in redundant mode */
      if (port->redstate != ECT_RED_NONE)
      {   
         /* only read frame if not already in */
         if (wkc2 <= EC_NOFRAME)
         {
            wkc2 = ecx_inframe(port, idx, 1, timeout);
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
      if ( ((primrx == RX_SEC) && (secrx == RX_PRIM)) )
      {
         /* copy secondary buffer to primary */
         memcpy(&(port->rxbuf[idx]), &(port->redport->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
         wkc = wkc2;
      }   
      /* primary socket got nothing or primary frame, and secondary socket got secondary frame */
      /* we need to resend TX packet */ 
      if ( ((primrx == 0) && (secrx == RX_SEC)) ||
           ((primrx == RX_PRIM) && (secrx == RX_SEC)) )
      {
         /* If both primary and secondary have partial connection retransmit the primary received
          * frame over the secondary socket. The result from the secondary received frame is a combined
          * frame that traversed all slaves in standard order. */
         if ( (primrx == RX_PRIM) && (secrx == RX_SEC) )
         {   
            /* copy primary rx to tx buffer */
            memcpy(&(port->txbuf[idx][ETH_HEADERSIZE]), &(port->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
         }
         osal_timer_start (&timer2, EC_TIMEOUTRET);
         /* resend secondary tx */
         ecx_outframe(port, idx, 1);
         do 
         {
            /* retrieve frame */
            wkc2 = ecx_inframe(port, idx, 1, timeout);
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
 * @param[in] port        = port context struct
 * @param[in] idx       = requested index of frame
 * @param[in] timeout   = timeout in us
 * @return Workcounter if a frame is found with corresponding index, otherwise
 * EC_NOFRAME.
 */
int ecx_waitinframe(ecx_portt *port, uint8 idx, int timeout)
{
   int wkc;
   osal_timert timer;
   
   osal_timer_start (&timer, timeout); 
   wkc = ecx_waitinframe_red(port, idx, &timer, timeout);
   
   return wkc;
}

/** Blocking send and receive frame function. Used for non processdata frames.
 * A datagram is build into a frame and transmitted via this function. It waits
 * for an answer and returns the workcounter. The function retries if time is
 * left and the result is WKC=0 or no frame received.
 *
 * The function calls ec_outframe_red() and ec_waitinframe_red().
 *
 * @param[in] port        = port context struct
 * @param[in] idx      = index of frame
 * @param[in] timeout  = timeout in us
 * @return Workcounter or EC_NOFRAME
 */
int ecx_srconfirm(ecx_portt *port, uint8 idx, int timeout)
{
   int wkc = EC_NOFRAME;
   osal_timert timer1, timer2;

   osal_timer_start (&timer1, timeout);
   do 
   {
      /* tx frame on primary and if in redundant mode a dummy on secondary */
      ecx_outframe_red(port, idx);
      if (timeout < EC_TIMEOUTRET) 
      {
         osal_timer_start (&timer2, timeout); 
      }
      else 
      {
         /* normally use partial timeout for rx */
         osal_timer_start (&timer2, EC_TIMEOUTRET); 
      }
      /* get frame from primary or if in redundant mode possibly from secondary */
      wkc = ecx_waitinframe_red(port, idx, &timer2, timeout);
   /* wait for answer with WKC>=0 or otherwise retry until timeout */   
   } while ((wkc <= EC_NOFRAME) && !osal_timer_is_expired (&timer1));

   
   return wkc;
}

#ifdef EC_VER1
int ec_setupnic(const char *ifname, int secondary)
{
   return ecx_setupnic(&ecx_port, ifname, secondary);
}

int ec_closenic(void)
{
   return ecx_closenic(&ecx_port);
}

uint8 ec_getindex(void)
{
   return ecx_getindex(&ecx_port);
}

void ec_setbufstat(uint8 idx, int bufstat)
{
   ecx_setbufstat(&ecx_port, idx, bufstat);
}

int ec_outframe(uint8 idx, int stacknumber)
{
   return ecx_outframe(&ecx_port, idx, stacknumber);
}

int ec_outframe_red(uint8 idx)
{
   return ecx_outframe_red(&ecx_port, idx);
}

int ec_inframe(uint8 idx, int stacknumber, int timeout)
{
   return ecx_inframe(&ecx_port, idx, stacknumber, timeout);
}

int ec_waitinframe(uint8 idx, int timeout)
{
   return ecx_waitinframe(&ecx_port, idx, timeout);
}

int ec_srconfirm(uint8 idx, int timeout)
{
   return ecx_srconfirm(&ecx_port, idx, timeout);
}
#endif
