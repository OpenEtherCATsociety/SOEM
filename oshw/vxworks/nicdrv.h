/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file 
 * \brief
 * Headerfile for nicdrv.c 
 */

#ifndef _nicdrvh_
#define _nicdrvh_

#ifdef __cplusplus
extern "C"
{
#endif

#include <vxWorks.h>
#include <muxLib.h>

/** structure to connect EtherCAT stack and VxWorks device */
typedef struct ETHERCAT_PKT_DEV
{
   struct ecx_port  *port;
   void             *pCookie;
   END_OBJ          *endObj; 
   UINT32           redundant;
   UINT32           tx_count;
   UINT32           rx_count;
   UINT32           overrun_count;
}ETHERCAT_PKT_DEV;

/** pointer structure to Tx and Rx stacks */
typedef struct
{
   /** tx buffer */
   ec_bufT     (*txbuf)[EC_MAXBUF];
   /** tx buffer lengths */
   int         (*txbuflength)[EC_MAXBUF];
   /** rx buffers */
   ec_bufT     (*rxbuf)[EC_MAXBUF];
   /** rx buffer status fields */
   int         (*rxbufstat)[EC_MAXBUF];
   /** received MAC source address (middle word) */
   int         (*rxsa)[EC_MAXBUF];
} ec_stackT;   

/** pointer structure to buffers for redundant port */
typedef struct ecx_redport
{
   /** Stack reference */   
   ec_stackT stack;
   /** Packet device instance */
   ETHERCAT_PKT_DEV pktDev;
   /** rx buffers */
   ec_bufT rxbuf[EC_MAXBUF];
   /** rx buffer status */
   int rxbufstat[EC_MAXBUF];
   /** rx MAC source address */
   int rxsa[EC_MAXBUF];
   /** MSG Q for receive callbacks to post into */
   MSG_Q_ID  msgQId[EC_MAXBUF];
} ecx_redportt;

/** pointer structure to buffers, vars and mutexes for port instantiation */
typedef struct ecx_port
{
   /** Stack reference */   
   ec_stackT stack;
   /** Packet device instance */
   ETHERCAT_PKT_DEV pktDev;
   /** rx buffers */
   ec_bufT rxbuf[EC_MAXBUF];
   /** rx buffer status */
   int rxbufstat[EC_MAXBUF];
   /** rx MAC source address */
   int rxsa[EC_MAXBUF];
   /** transmit buffers */
   ec_bufT txbuf[EC_MAXBUF];
   /** transmit buffer lenghts */
   int txbuflength[EC_MAXBUF];
   /** temporary tx buffer */
   ec_bufT txbuf2;
   /** temporary tx buffer length */
   int txbuflength2;
   /** last used frame index */
   int lastidx;
   /** current redundancy state */
   int redstate;
   /** pointer to redundancy port and buffers */
   ecx_redportt *redport;   
   /** Semaphore to protect single resources */
   SEM_ID  sem_get_index;
   /** MSG Q for receive callbacks to post into */
   MSG_Q_ID  msgQId[EC_MAXBUF];
} ecx_portt;

extern const uint16 priMAC[3];
extern const uint16 secMAC[3];

#ifdef EC_VER1
extern ecx_portt     ecx_port;
extern ecx_redportt  ecx_redport;

int ec_setupnic(const char * ifname, int secondary);
int ec_closenic(void);
void ec_setbufstat(int idx, int bufstat);
int ec_getindex(void);
int ec_outframe(int idx, int sock);
int ec_outframe_red(int idx);
int ec_waitinframe(int idx, int timeout);
int ec_srconfirm(int idx,int timeout);
#endif

void ec_setupheader(void *p);
int ecx_setupnic(ecx_portt *port, const char * ifname, int secondary);
int ecx_closenic(ecx_portt *port);
void ecx_setbufstat(ecx_portt *port, int idx, int bufstat);
int ecx_getindex(ecx_portt *port);
int ecx_outframe(ecx_portt *port, int idx, int sock);
int ecx_outframe_red(ecx_portt *port, int idx);
int ecx_waitinframe(ecx_portt *port, int idx, int timeout);
int ecx_srconfirm(ecx_portt *port, int idx,int timeout);

#ifdef __cplusplus
}
#endif

#endif
