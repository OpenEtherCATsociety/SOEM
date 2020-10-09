/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * EtherCAT RAW socket driver.
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

#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

#include "oshw.h"
#include "osal.h"
#include "nicdrv.h"
#include "ee.h"
#include "intel_i210.h"
#include "ee_x86_64_tsc.h"

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
const uint16 priMAC[3] = { 0x0201, 0x0101, 0x0101 };
/** Secondary source MAC address used for EtherCAT. */
const uint16 secMAC[3] = { 0x0604, 0x0404, 0x0404 };

/** second MAC word is used for identification */
#define RX_PRIM priMAC[1]
/** second MAC word is used for identification */
#define RX_SEC secMAC[1]

void ee_port_lock(void);
void ee_port_unlock(void);

static inline void ecx_clear_rxbufstat(int *rxbufstat)
{
   	int i;
   	for(i = 0; i < EC_MAXBUF; i++)
      		rxbufstat[i] = EC_BUF_EMPTY;
}

/** Basic setup to connect NIC to socket.
 * @param[in] port        = port context struct
 * @param[in] ifname      = Name of NIC device, f.e. "eth0"
 * @param[in] secondary   = if >0 then use secondary stack instead of primary
 * @return >0 if succeeded
 */
int ecx_setupnic(ecx_portt *port, const char *ifname, int secondary)
{
   	int d;
	struct eth_device *dev;
	OSEE_PRINT("ecx_setupnic() searching %s...\n", ifname);

	for (d = 0;; ++d) {
		dev = eth_get_device(d);

		if (dev == NULL)
			break; // ERROR: device not found

		if (!strncmp(dev->name, ifname, MAX_DEVICE_NAME)){
			// Device found
			int i;

			eth_setup_device(d, 1, 100);
			port->dev_id		= d;

      			port->sockhandle        = -1;
      			port->lastidx           = 0;
      			port->redstate          = ECT_RED_NONE;
      			port->stack.sock        = &(port->sockhandle);
      			port->stack.txbuf       = &(port->txbuf);
      			port->stack.txbuflength = &(port->txbuflength);
      			port->stack.tempbuf     = &(port->tempinbuf);
      			port->stack.rxbuf       = &(port->rxbuf);
      			port->stack.rxbufstat   = &(port->rxbufstat);
      			port->stack.rxsa        = &(port->rxsa);
      			ecx_clear_rxbufstat(&(port->rxbufstat[0]));

   			/* setup ethernet headers in tx buffers so we don't have to repeat it */
   			for (i = 0; i < EC_MAXBUF; i++)
   			{
      				ec_setupheader(&(port->txbuf[i]));
      				port->rxbufstat[i] = EC_BUF_EMPTY;
   			}
   			ec_setupheader(&(port->txbuf2));

			break; // device found
		}
	}

	return (dev != NULL);
}

/** Close sockets used
 * @param[in] port        = port context struct
 * @return 0
 */
inline int ecx_closenic(ecx_portt *port)
{
	// Nothing to do
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
   	bp->da0 = oshw_htons(0xffff);
   	bp->da1 = oshw_htons(0xffff);
   	bp->da2 = oshw_htons(0xffff);
   	bp->sa0 = oshw_htons(priMAC[0]);
   	bp->sa1 = oshw_htons(priMAC[1]);
   	bp->sa2 = oshw_htons(priMAC[2]);
   	bp->etype = oshw_htons(ETH_P_ECAT);
}

/** Get new frame identifier index and allocate corresponding rx buffer.
 * @param[in] port        = port context struct
 * @return new index.
 */
uint8 ecx_getindex(ecx_portt *port)
{
   	uint8 idx;
   	uint8 cnt = 0;

	ee_port_lock();

   	idx = port->lastidx + 1;
   	/* index can't be larger than buffer array */
   	if (idx >= EC_MAXBUF)
      		idx = 0;

   	/* try to find unused index */
   	while ((port->rxbufstat[idx] != EC_BUF_EMPTY) && (cnt < EC_MAXBUF)) {
      		idx++;
      		cnt++;
      		if (idx >= EC_MAXBUF)
         		idx = 0;
   	}
   	port->rxbufstat[idx] = EC_BUF_ALLOC;
   	if (port->redstate != ECT_RED_NONE)
      		port->redport->rxbufstat[idx] = EC_BUF_ALLOC;
   	port->lastidx = idx;

	ee_port_unlock();

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

/** Transmit buffer over socket (non blocking).
 * @param[in] port        = port context struct
 * @param[in] idx         = index in tx buffer array
 * @param[in] stacknumber  = 0=Primary 1=Secondary stack
 * @return socket send result
 */
int ecx_outframe(ecx_portt *port, uint8 idx, int stacknumber)
{
   	int lp;
   	ec_stackT *stack;

   	if (!stacknumber)
      		stack = &(port->stack);
   	else
      		stack = &(port->redport->stack);
   	lp = (*stack->txbuflength)[idx];
   	(*stack->rxbufstat)[idx] = EC_BUF_TX;

	eth_send_packet(port->dev_id, (*stack->txbuf)[idx], lp, 1);

   	return 1;
}

/** Transmit buffer over socket (non blocking).
 * @param[in] port        = port context struct
 * @param[in] idx = index in tx buffer array
 * @return socket send result
 */
int ecx_outframe_red(ecx_portt *port, uint8 idx)
{
   	ec_comt *datagramP;
   	ec_etherheadert *ehp;
   	int rval;

   	ehp = (ec_etherheadert *)&(port->txbuf[idx]);
   	/* rewrite MAC source address 1 to primary */
   	ehp->sa1 = oshw_htons(priMAC[1]);
   	/* transmit over primary socket*/
   	rval = ecx_outframe(port, idx, 0);
   	if (port->redstate != ECT_RED_NONE) {
      		ee_port_lock();
      		ehp = (ec_etherheadert *)&(port->txbuf2);
      		/* use dummy frame for secondary socket transmit (BRD) */
      		datagramP = (ec_comt*)&(port->txbuf2[ETH_HEADERSIZE]);
      		/* write index to frame */
      		datagramP->index = idx;
      		/* rewrite MAC source address 1 to secondary */
      		ehp->sa1 = oshw_htons(secMAC[1]);
      		/* transmit over secondary socket */
      		port->redport->rxbufstat[idx] = EC_BUF_TX;
      		eth_send_packet(port->dev_id, &(port->txbuf2), port->txbuflength2, 1);
		ee_port_unlock();
   	}

   	return rval;
}

/** Non blocking read of socket. Put frame in temporary buffer.
 * @param[in] port        = port context struct
 * @param[in] stacknumber = 0=primary 1=secondary stack
 * @return >0 if frame is available and read
 */
static int ecx_recvpkt(ecx_portt *port, int stacknumber)
{
   	int lp, bytesrx;
   	ec_stackT *stack;

   	if (!stacknumber)
      		stack = &(port->stack);
   	else
      		stack = &(port->redport->stack);
   	lp = sizeof(port->tempinbuf);
   	bytesrx = eth_receive_packet(port->dev_id, (*stack->tempbuf), lp, 1, 0);
   	port->tempinbufs = bytesrx;

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
 * @param[in] port        = port context struct
 * @param[in] idx         = requested index of frame
 * @param[in] stacknumber = 0=primary 1=secondary stack
 * @return Workcounter if a frame is found with corresponding index, otherwise
 * EC_NOFRAME or EC_OTHERFRAME.
 */
int ecx_inframe(ecx_portt *port, uint8 idx, int stacknumber)
{
   	uint16  l;
   	int     rval;
   	uint8     idxf;
   	ec_etherheadert *ehp;
   	ec_comt *ecp;
   	ec_stackT *stack;
   	ec_bufT *rxbuf;

   	if (!stacknumber)
      		stack = &(port->stack);
   	else
      		stack = &(port->redport->stack);
   	rval = EC_NOFRAME;
   	rxbuf = &(*stack->rxbuf)[idx];
   	/* check if requested index is already in buffer ? */
   	if ((idx < EC_MAXBUF) && ((*stack->rxbufstat)[idx] == EC_BUF_RCVD)) {
      		l = (*rxbuf)[0] + ((uint16)((*rxbuf)[1] & 0x0f) << 8);
      		/* return WKC */
      		rval = ((*rxbuf)[l] + ((uint16)(*rxbuf)[l + 1] << 8));
      		/* mark as completed */
      		(*stack->rxbufstat)[idx] = EC_BUF_COMPLETE;
   	} else {
		ee_port_lock();
      		/* non blocking call to retrieve frame from socket */


		while (1) {
      			if (ecx_recvpkt(port, stacknumber)) {
         			rval = EC_OTHERFRAME;
         			ehp =(ec_etherheadert*)(stack->tempbuf);
         			/* check if it is an EtherCAT frame */
         			if (ehp->etype == oshw_htons(ETH_P_ECAT)) {
            				ecp =(ec_comt*)(&(*stack->tempbuf)[ETH_HEADERSIZE]);
            				l = etohs(ecp->elength) & 0x0fff;
            				idxf = ecp->index;
            				/* found index equals requested index ? */
            				if (idxf == idx) {
               					/* yes, put it in the buffer array (strip ethernet header) */
               					memcpy(rxbuf, &(*stack->tempbuf)[ETH_HEADERSIZE], (*stack->txbuflength)[idx] - ETH_HEADERSIZE);
               					/* return WKC */
               					rval = ((*rxbuf)[l] + ((uint16)((*rxbuf)[l + 1]) << 8));
               					/* mark as completed */
               					(*stack->rxbufstat)[idx] = EC_BUF_COMPLETE;
               					/* store MAC source word 1 for redundant routing info */
               					(*stack->rxsa)[idx] = oshw_ntohs(ehp->sa1);
						break;
            				} else if (idxf < EC_MAXBUF && (*stack->rxbufstat)[idxf] == EC_BUF_TX) {
                  				rxbuf = &(*stack->rxbuf)[idxf];
                  				/* put it in the buffer array (strip ethernet header) */
                  				memcpy(rxbuf, &(*stack->tempbuf)[ETH_HEADERSIZE], (*stack->txbuflength)[idxf] - ETH_HEADERSIZE);
                  				/* mark as received */
                  				(*stack->rxbufstat)[idxf] = EC_BUF_RCVD;
                  				(*stack->rxsa)[idxf] = oshw_ntohs(ehp->sa1);
						break;
               				} else {
						OSEE_PRINT("ecx_inframe(): WARNING: strange things happened\n");
                  				/* strange things happened */
               				}
         			} else {
					OSEE_PRINT("ecx_inframe(): WARNING it is NOT an EtherCAT frame!\n");
				}
      			} else {
				// WARNING: no messages received.
				break;
			}
		}

		ee_port_unlock();

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
 * @param[in] port        = port context struct
 * @param[in] idx = requested index of frame
 * @param[in] timer = absolute timeout time
 * @return Workcounter if a frame is found with corresponding index, otherwise
 * EC_NOFRAME.
 */
static int ecx_waitinframe_red(ecx_portt *port, uint8 idx, osal_timert *timer)
{
   	osal_timert timer2;
   	int wkc  = EC_NOFRAME;
   	int wkc2 = EC_NOFRAME;
   	int primrx, secrx;

   	/* if not in redundant mode then always assume secondary is OK */
   	if (port->redstate == ECT_RED_NONE)
      		wkc2 = 0;
   	do {
      		/* only read frame if not already in */
      		if (wkc <= EC_NOFRAME)
         		wkc  = ecx_inframe(port, idx, 0);
      		/* only try secondary if in redundant mode */
      		if (port->redstate != ECT_RED_NONE) {
         		/* only read frame if not already in */
         		if (wkc2 <= EC_NOFRAME)
            			wkc2 = ecx_inframe(port, idx, 1);
      		}
   		/* wait for both frames to arrive or timeout */
   	} while (((wkc <= EC_NOFRAME) || (wkc2 <= EC_NOFRAME)) && !osal_timer_is_expired(timer));
   	/* only do redundant functions when in redundant mode */
   	if (port->redstate != ECT_RED_NONE) {
      		/* primrx if the received MAC source on primary socket */
      		primrx = 0;
      		if (wkc > EC_NOFRAME) primrx = port->rxsa[idx];
      		/* secrx if the received MAC source on psecondary socket */
      		secrx = 0;
      		if (wkc2 > EC_NOFRAME) secrx = port->redport->rxsa[idx];

      		/* primary socket got secondary frame and secondary socket got primary frame */
      		/* normal situation in redundant mode */
      		if ( ((primrx == RX_SEC) && (secrx == RX_PRIM)) ) {
         		/* copy secondary buffer to primary */
         		memcpy(&(port->rxbuf[idx]), &(port->redport->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
         		wkc = wkc2;
      		}
      		/* primary socket got nothing or primary frame, and secondary socket got secondary frame */
      		/* we need to resend TX packet */
      		if ( ((primrx == 0) && (secrx == RX_SEC)) ||
           			((primrx == RX_PRIM) && (secrx == RX_SEC)) ) {
         		/* If both primary and secondary have partial connection retransmit the primary received
          		 * frame over the secondary socket. The result from the secondary received frame is a combined
          		 * frame that traversed all slaves in standard order. */
         		if ( (primrx == RX_PRIM) && (secrx == RX_SEC) ) {
            			/* copy primary rx to tx buffer */
            			memcpy(&(port->txbuf[idx][ETH_HEADERSIZE]), &(port->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
         		}
         		osal_timer_start (&timer2, EC_TIMEOUTRET);
         		/* resend secondary tx */
         		ecx_outframe(port, idx, 1);
         		do {
            			/* retrieve frame */
            			wkc2 = ecx_inframe(port, idx, 1);
         		} while ((wkc2 <= EC_NOFRAME) && !osal_timer_is_expired(&timer2));
         		if (wkc2 > EC_NOFRAME) {
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
   	wkc = ecx_waitinframe_red(port, idx, &timer);

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
   	do  {
      		/* tx frame on primary and if in redundant mode a dummy on secondary */
      		ecx_outframe_red(port, idx);
      		if (timeout < EC_TIMEOUTRET) {
         		osal_timer_start (&timer2, timeout);
      		} else {
         		/* normally use partial timeout for rx */
         		osal_timer_start (&timer2, EC_TIMEOUTRET);
      		}
      		/* get frame from primary or if in redundant mode possibly
		   from secondary */
      		wkc = ecx_waitinframe_red(port, idx, &timer2);
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

int ec_getindex(void)
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

int ec_inframe(uint8 idx, int stacknumber)
{
   return ecx_inframe(&ecx_port, idx, stacknumber);
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
