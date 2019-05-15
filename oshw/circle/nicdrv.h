/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for nicdrv.cpp
 */

#ifndef _nicdrvh_
#define _nicdrvh_

#ifdef __cplusplus
extern "C"
{
#endif

// CNetDevice requires buffer size 1600
#define FRAME_BUFFER_SIZE 1600

typedef uint8 ec_buf_circleT[FRAME_BUFFER_SIZE];

/** pointer structure to Tx and Rx stacks */
typedef struct
{
   /** socket connection used */
   void        **device;
   /** tx buffer */
   ec_buf_circleT (*txbuf)[EC_MAXBUF];
   /** tx buffer lengths */
   int         (*txbuflength)[EC_MAXBUF];
   /** temporary receive buffer */
   ec_buf_circleT *tempbuf;
   /** rx buffers */
   ec_buf_circleT (*rxbuf)[EC_MAXBUF];
   /** rx buffer status fields */
   int         (*rxbufstat)[EC_MAXBUF];
   /** received MAC source address (middle word) */
   int         (*rxsa)[EC_MAXBUF];
} ec_stackT;

/** pointer structure to buffers for redundant port */
typedef struct
{
   ec_stackT   stack;
   void        *device;
   /** rx buffers */
   ec_buf_circleT rxbuf[EC_MAXBUF];
   /** rx buffer status */
   int rxbufstat[EC_MAXBUF];
   /** rx MAC source address */
   int rxsa[EC_MAXBUF];
   /** temporary rx buffer */
   ec_buf_circleT tempinbuf;
} ecx_redportt;

/** pointer structure to buffers, vars and mutexes for port instantiation */
typedef struct
{
   ec_stackT   stack;
   void        *device;
   /** rx buffers */
   ec_buf_circleT rxbuf[EC_MAXBUF];
   /** rx buffer status */
   int rxbufstat[EC_MAXBUF];
   /** rx MAC source address */
   int rxsa[EC_MAXBUF];
   /** temporary rx buffer */
   ec_buf_circleT tempinbuf;
   /** temporary rx buffer status */
   int tempinbufs;
   /** transmit buffers */
   ec_buf_circleT txbuf[EC_MAXBUF];
   /** transmit buffer lenghts */
   int txbuflength[EC_MAXBUF];
   /** temporary tx buffer */
   ec_buf_circleT txbuf2;
   /** temporary tx buffer length */
   int txbuflength2;
   /** last used frame index */
   int lastidx;
   /** current redundancy state */
   int redstate;
   /** pointer to redundancy port and buffers */
   ecx_redportt *redport;
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
