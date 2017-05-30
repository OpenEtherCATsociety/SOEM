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

#define HAVE_REMOTE

#include <rt.h>
#include "hpeif2.h"

/** pointer structure to Tx and Rx stacks */
typedef struct
{
   /** socket connection used */
   int         *sock;
   /** tx buffer */
   ec_bufT     (*txbuf)[EC_MAXBUF];
   /** tx buffer lengths */
   int         (*txbuflength)[EC_MAXBUF];
   /** temporary receive buffer */
   ec_bufT     *tempbuf;
   /** rx buffers */
   ec_bufT     (*rxbuf)[EC_MAXBUF];
   /** rx buffer status fields */
   int         (*rxbufstat)[EC_MAXBUF];
   /** received MAC source address (middle word) */
   int         (*rxsa)[EC_MAXBUF];
} ec_stackT;

typedef struct
{
   ec_stackT   stack;
   /** rx buffers */
   ec_bufT rxbuf[EC_MAXBUF];
   /** rx buffer status */
   int rxbufstat[EC_MAXBUF];
   /** rx MAC source address */
   int rxsa[EC_MAXBUF];
   /** temporary rx buffer */
   ec_bufT tempinbuf;
   /* Intime */
   HPEHANDLE      handle;
   HPERXBUFFERSET *rx_buffers;
   HPETXBUFFERSET *tx_buffers[EC_MAXBUF];
} ecx_redportt;

typedef struct
{
   ec_stackT      stack;
   /** rx buffers */
   ec_bufT        rxbuf[EC_MAXBUF];
   /** rx buffer status */
   int            rxbufstat[EC_MAXBUF];
   /** rx MAC source address */
   int            rxsa[EC_MAXBUF];
   /** temporary rx buffer */
   ec_bufT        tempinbuf;
   /** temporary rx buffer status */
   int            tempinbufs;
   /** transmit buffers */
   ec_bufT        txbuf[EC_MAXBUF];
   /** transmit buffer lenghts */
   int            txbuflength[EC_MAXBUF];
   /** temporary tx buffer */
   ec_bufT        txbuf2;
   /** temporary tx buffer length */
   int            txbuflength2;
   /** last used frame index */
   int            lastidx;
   /** current redundancy state */
   int            redstate;
   /** pointer to redundancy port and buffers */
   ecx_redportt   *redport;
   RTHANDLE       getindex_region;
   RTHANDLE       tx_region;
   RTHANDLE       rx_region;
   /* Intime */
   HPEHANDLE      handle;
   HPERXBUFFERSET *rx_buffers;
   HPETXBUFFERSET *tx_buffers[EC_MAXBUF];
} ecx_portt;

extern const uint16 priMAC[3];
extern const uint16 secMAC[3];

//extern ecx_portt     ecx_port;
//extern ecx_redportt  ecx_redport;

int ec_setupnic(const char * ifname, int secondary);
int ec_closenic(void);
void ec_setupheader(void *p);
void ec_setbufstat(int idx, int bufstat);
int ec_getindex(void);
int ec_outframe(int idx, int sock);
int ec_outframe_red(int idx);
int ec_waitinframe(int idx, int timeout);
int ec_srconfirm(int idx,int timeout);

int ecx_setupnic(ecx_portt *port, const char * ifname, int secondary);
int ecx_closenic(ecx_portt *port);
void ecx_setbufstat(ecx_portt *port, int idx, int bufstat);
int ecx_getindex(ecx_portt *port);
int ecx_outframe(ecx_portt *port, int idx, int sock);
int ecx_outframe_red(ecx_portt *port, int idx);
int ecx_waitinframe(ecx_portt *port, int idx, int timeout);
int ecx_srconfirm(ecx_portt *port, int idx,int timeout);

#endif
