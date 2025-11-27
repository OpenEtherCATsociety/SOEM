/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 * full license information.
 */

/** \file
 * \brief
 * Ethernet over EtherCAT (EoE) module.
 *
 * Set / Get IP functions
 * Blocking send/receive Ethernet Frame
 * Read incoming EoE fragment to Ethernet Frame
 */

#include "soem/soem.h"
#include <string.h>
#include "osal.h"
#include "oshw.h"

/** EoE utility function to convert uint32 to eoe ip bytes.
 * @param[in] ip       ip in uint32
 * @param[out] byte_ip eoe ip 4th octet, 3ed octet, 2nd octet, 1st octet
 */
static void EOE_ip_uint32_to_byte(eoe_ip4_addr_t *ip, uint8_t *byte_ip)
{
   byte_ip[3] = eoe_ip4_addr1(ip); /* 1st octet */
   byte_ip[2] = eoe_ip4_addr2(ip); /* 2nd octet */
   byte_ip[1] = eoe_ip4_addr3(ip); /* 3ed octet */
   byte_ip[0] = eoe_ip4_addr4(ip); /* 4th octet */
}

/** EoE utility function to convert eoe ip bytes to uint32.
 * @param[in] byte_ip eoe ip 4th octet, 3ed octet, 2nd octet, 1st octet
 * @param[out] ip     ip in uint32
 */
static void EOE_ip_byte_to_uint32(uint8_t *byte_ip, eoe_ip4_addr_t *ip)
{
   EOE_IP4_ADDR_TO_U32(ip,
                       byte_ip[3],  /* 1st octet */
                       byte_ip[2],  /* 2nd octet */
                       byte_ip[1],  /* 3ed octet */
                       byte_ip[0]); /* 4th octet */
}

/** EoE fragment data handler hook. Should not block.
 *
 * @param[in]  context context struct
 * @param[in]  hook    Pointer to hook function.
 * @return 1
 */
int ecx_EOEdefinehook(ecx_contextt *context, void *hook)
{
   context->EOEhook = hook;
   return 1;
}

/** EoE EOE set IP, blocking. Waits for response from the slave.
 *
 * @param[in]  context    Context struct
 * @param[in]  slave      Slave number
 * @param[in]  port       Port number on slave if applicable
 * @param[in]  ipparam    IP parameter data to be sent
 * @param[in]  timeout    Timeout in us, standard is EC_TIMEOUTRXM
 * @return Workcounter from last slave response or returned result code
 */
int ecx_EOEsetIp(ecx_contextt *context, uint16 slave, uint8 port, eoe_param_t *ipparam, int timeout)
{
   ec_EOEt *EOEp, *aEOEp;
   ec_mbxbuft *MbxIn, *MbxOut;
   uint16 frameinfo1, result;
   uint8 cnt, data_offset;
   uint8 flags = 0;
   int wkc;

   MbxIn = NULL;
   MbxOut = NULL;
   /* Empty slave out mailbox if something is in. Timeout set to 0 */
   wkc = ecx_mbxreceive(context, slave, &MbxIn, 0);
   MbxOut = ecx_getmbx(context);
   ec_clearmbx(MbxOut);
   EOEp = (ec_EOEt *)MbxOut;
   EOEp->mbxheader.address = htoes(0x0000);
   EOEp->mbxheader.priority = 0x00;
   data_offset = EOE_PARAM_OFFSET;

   /* get new mailbox count value, used as session handle */
   cnt = ec_nextmbxcnt(context->slavelist[slave].mbx_cnt);
   context->slavelist[slave].mbx_cnt = cnt;

   EOEp->mbxheader.mbxtype = ECT_MBXT_EOE + MBX_HDR_SET_CNT(cnt); /* EoE */

   EOEp->frameinfo1 = htoes(EOE_HDR_FRAME_TYPE_SET(EOE_INIT_REQ) |
                            EOE_HDR_FRAME_PORT_SET(port) |
                            EOE_HDR_LAST_FRAGMENT);
   EOEp->frameinfo2 = 0;

   if (ipparam->mac_set)
   {
      flags |= EOE_PARAM_MAC_INCLUDE;
      memcpy(&EOEp->data[data_offset], ipparam->mac.addr, EOE_ETHADDR_LENGTH);
      data_offset += EOE_ETHADDR_LENGTH;
   }
   if (ipparam->ip_set)
   {
      flags |= EOE_PARAM_IP_INCLUDE;
      EOE_ip_uint32_to_byte(&ipparam->ip, &EOEp->data[data_offset]);
      data_offset += EOE_IP4_LENGTH;
   }
   if (ipparam->subnet_set)
   {
      flags |= EOE_PARAM_SUBNET_IP_INCLUDE;
      EOE_ip_uint32_to_byte(&ipparam->subnet, &EOEp->data[data_offset]);
      data_offset += EOE_IP4_LENGTH;
   }
   if (ipparam->default_gateway_set)
   {
      flags |= EOE_PARAM_DEFAULT_GATEWAY_INCLUDE;
      EOE_ip_uint32_to_byte(&ipparam->default_gateway, &EOEp->data[data_offset]);
      data_offset += EOE_IP4_LENGTH;
   }
   if (ipparam->dns_ip_set)
   {
      flags |= EOE_PARAM_DNS_IP_INCLUDE;
      EOE_ip_uint32_to_byte(&ipparam->dns_ip, &EOEp->data[data_offset]);
      data_offset += EOE_IP4_LENGTH;
   }
   if (ipparam->dns_name_set)
   {
      /* TwinCAT include EOE_DNS_NAME_LENGTH chars even if name is shorter */
      flags |= EOE_PARAM_DNS_NAME_INCLUDE;
      memcpy(&EOEp->data[data_offset], (void *)ipparam->dns_name, EOE_DNS_NAME_LENGTH);
      data_offset += EOE_DNS_NAME_LENGTH;
   }

   EOEp->mbxheader.length = htoes(EOE_PARAM_OFFSET + data_offset);
   EOEp->data[0] = flags;

   /* send EoE request to slave */
   wkc = ecx_mbxsend(context, slave, MbxOut, EC_TIMEOUTTXM);
   MbxOut = NULL;

   if (wkc > 0) /* succeeded to place mailbox in slave ? */
   {
      if (MbxIn) ecx_dropmbx(context, MbxIn);
      MbxIn = NULL;
      /* read slave response */
      wkc = ecx_mbxreceive(context, slave, &MbxIn, timeout);
      if (wkc > 0) /* succeeded to read slave response ? */
      {
         aEOEp = (ec_EOEt *)MbxIn;
         /* slave response should be EoE */
         if ((aEOEp->mbxheader.mbxtype & 0x0f) == ECT_MBXT_EOE)
         {
            frameinfo1 = etohs(aEOEp->frameinfo1);
            result = etohs(aEOEp->result);
            if ((EOE_HDR_FRAME_TYPE_GET(frameinfo1) != EOE_INIT_RESP) ||
                (result != EOE_RESULT_SUCCESS))
            {
               wkc = -result;
            }
         }
         else
         {
            /* unexpected mailbox received */
            wkc = -EC_ERR_TYPE_PACKET_ERROR;
         }
      }
   }

   if (MbxIn) ecx_dropmbx(context, MbxIn);
   if (MbxOut) ecx_dropmbx(context, MbxOut);

   return wkc;
}

/** EoE EOE get IP, blocking. Waits for response from the slave.
 *
 * @param[in]  context    Context struct
 * @param[in]  slave      Slave number
 * @param[in]  port       Port number on slave if applicable
 * @param[out] ipparam    IP parameter data retrieved from slave
 * @param[in]  timeout    Timeout in us, standard is EC_TIMEOUTRXM
 * @return Workcounter from last slave response or returned result code
 */
int ecx_EOEgetIp(ecx_contextt *context, uint16 slave, uint8 port, eoe_param_t *ipparam, int timeout)
{
   ec_EOEt *EOEp, *aEOEp;
   ec_mbxbuft *MbxIn, *MbxOut;
   uint16 frameinfo1, eoedatasize;
   uint8 cnt, data_offset;
   uint8 flags = 0;
   int wkc;

   MbxIn = NULL;
   MbxOut = NULL;
   /* Empty slave out mailbox if something is in. Timeout set to 0 */
   wkc = ecx_mbxreceive(context, slave, &MbxIn, 0);
   MbxOut = ecx_getmbx(context);
   ec_clearmbx(MbxOut);
   EOEp = (ec_EOEt *)MbxOut;
   EOEp->mbxheader.address = htoes(0x0000);
   EOEp->mbxheader.priority = 0x00;
   data_offset = EOE_PARAM_OFFSET;

   /* get new mailbox count value, used as session handle */
   cnt = ec_nextmbxcnt(context->slavelist[slave].mbx_cnt);
   context->slavelist[slave].mbx_cnt = cnt;

   EOEp->mbxheader.mbxtype = ECT_MBXT_EOE + MBX_HDR_SET_CNT(cnt); /* EoE */

   EOEp->frameinfo1 = htoes(EOE_HDR_FRAME_TYPE_SET(EOE_GET_IP_PARAM_REQ) |
                            EOE_HDR_FRAME_PORT_SET(port) |
                            EOE_HDR_LAST_FRAGMENT);
   EOEp->frameinfo2 = 0;

   EOEp->mbxheader.length = htoes(0x0004);
   EOEp->data[0] = flags;

   /* send EoE request to slave */
   wkc = ecx_mbxsend(context, slave, MbxOut, EC_TIMEOUTTXM);
   MbxOut = NULL;
   if (wkc > 0) /* succeeded to place mailbox in slave ? */
   {
      if (MbxIn) ecx_dropmbx(context, MbxIn);
      MbxIn = NULL;
      /* read slave response */
      wkc = ecx_mbxreceive(context, slave, &MbxIn, timeout);
      if (wkc > 0) /* succeeded to read slave response ? */
      {
         aEOEp = (ec_EOEt *)MbxIn;
         /* slave response should be FoE */
         if ((aEOEp->mbxheader.mbxtype & 0x0f) == ECT_MBXT_EOE)
         {
            frameinfo1 = etohs(aEOEp->frameinfo1);
            eoedatasize = etohs(aEOEp->mbxheader.length) - 0x0004;
            if (EOE_HDR_FRAME_TYPE_GET(frameinfo1) != EOE_GET_IP_PARAM_RESP)
            {
               wkc = -EOE_RESULT_UNSUPPORTED_FRAME_TYPE;
            }
            else
            {
               flags = aEOEp->data[0];
               if (flags & EOE_PARAM_MAC_INCLUDE)
               {
                  memcpy(ipparam->mac.addr,
                         &aEOEp->data[data_offset],
                         EOE_ETHADDR_LENGTH);
                  ipparam->mac_set = 1;
                  data_offset += EOE_ETHADDR_LENGTH;
               }
               if (flags & EOE_PARAM_IP_INCLUDE)
               {
                  EOE_ip_byte_to_uint32(&aEOEp->data[data_offset],
                                        &ipparam->ip);
                  ipparam->ip_set = 1;
                  data_offset += EOE_IP4_LENGTH;
               }
               if (flags & EOE_PARAM_SUBNET_IP_INCLUDE)
               {
                  EOE_ip_byte_to_uint32(&aEOEp->data[data_offset],
                                        &ipparam->subnet);
                  ipparam->subnet_set = 1;
                  data_offset += EOE_IP4_LENGTH;
               }
               if (flags & EOE_PARAM_DEFAULT_GATEWAY_INCLUDE)
               {
                  EOE_ip_byte_to_uint32(&aEOEp->data[data_offset],
                                        &ipparam->default_gateway);
                  ipparam->default_gateway_set = 1;
                  data_offset += EOE_IP4_LENGTH;
               }
               if (flags & EOE_PARAM_DNS_IP_INCLUDE)
               {
                  EOE_ip_byte_to_uint32(&aEOEp->data[data_offset],
                                        &ipparam->dns_ip);
                  ipparam->dns_ip_set = 1;
                  data_offset += EOE_IP4_LENGTH;
               }
               if (flags & EOE_PARAM_DNS_NAME_INCLUDE)
               {
                  uint16_t dns_len;
                  if ((eoedatasize - data_offset) < EOE_DNS_NAME_LENGTH)
                  {
                     dns_len = (eoedatasize - data_offset);
                  }
                  else
                  {
                     dns_len = EOE_DNS_NAME_LENGTH;
                  }
                  /* Assume ZERO terminated string */
                  memcpy(ipparam->dns_name, &aEOEp->data[data_offset], dns_len);
                  ipparam->dns_name_set = 1;
                  data_offset += EOE_DNS_NAME_LENGTH;
               }
               /* Something os not correct, flag the error */
               if (data_offset > eoedatasize)
               {
                  wkc = -EC_ERR_TYPE_MBX_ERROR;
               }
            }
         }
         else
         {
            /* unexpected mailbox received */
            wkc = -EC_ERR_TYPE_PACKET_ERROR;
         }
      }
   }

   if (MbxIn) ecx_dropmbx(context, MbxIn);
   if (MbxOut) ecx_dropmbx(context, MbxOut);

   return wkc;
}

/** EoE ethernet buffer write, blocking.
 *
 * If the buffer is larger than the mailbox size then the buffer is sent in
 * several fragments. The function will split the buf data in fragments and
 * send them to the slave one by one.
 *
 * @param[in]  context    context struct
 * @param[in]  slave      Slave number
 * @param[in]  port       Port number on slave if applicable
 * @param[in]  psize      Size in bytes of parameter buffer.
 * @param[in]  p          Pointer to parameter buffer
 * @param[in]  timeout    Timeout in us, standard is EC_TIMEOUTRXM
 * @return Workcounter from last slave transmission
 */
int ecx_EOEsend(ecx_contextt *context, uint16 slave, uint8 port, int psize, void *p, int timeout)
{
   ec_EOEt *EOEp;
   ec_mbxbuft *MbxOut;
   uint16 frameinfo1, frameinfo2;
   uint8 cnt, txfragmentno;
   boolean NotLast;
   int wkc, maxdata, txframesize, txframeoffset;
   const uint8 *buf = p;
   static uint8_t txframeno = 0;

   MbxOut = NULL;
   /* data section=mailbox size - 6 mbx - 4 EoEh */
   maxdata = context->slavelist[slave].mbx_l - 0x0A;
   txframesize = psize;
   txfragmentno = 0;
   txframeoffset = 0;
   NotLast = TRUE;

   /* Sanity check size of slave mailbox */
   if (maxdata < 0)
   {
      /* This slave does not have a suitable mailbox */
      EC_PRINT("EoE: Bad mailbox size\n");
      return -1;
   }

   do
   {
      MbxOut = ecx_getmbx(context);
      ec_clearmbx(MbxOut);
      EOEp = (ec_EOEt *)MbxOut;
      EOEp->mbxheader.address = htoes(0x0000);
      EOEp->mbxheader.priority = 0x00;

      txframesize = psize - txframeoffset;
      if (txframesize > maxdata)
      {
         /* Adjust to even 32-octect blocks */
         txframesize = ((maxdata >> 5) << 5);
      }

      if (txframesize == (psize - txframeoffset))
      {
         frameinfo1 = (EOE_HDR_LAST_FRAGMENT_SET(1) | EOE_HDR_FRAME_PORT_SET(port));
         NotLast = FALSE;
      }
      else
      {
         frameinfo1 = EOE_HDR_FRAME_PORT_SET(port);
      }

      frameinfo2 = EOE_HDR_FRAG_NO_SET(txfragmentno);
      if (txfragmentno > 0)
      {
         frameinfo2 = frameinfo2 | (EOE_HDR_FRAME_OFFSET_SET((txframeoffset >> 5)));
      }
      else
      {
         frameinfo2 = frameinfo2 | (EOE_HDR_FRAME_OFFSET_SET(((psize + 31) >> 5)));
         txframeno++;
      }
      frameinfo2 = frameinfo2 | EOE_HDR_FRAME_NO_SET(txframeno);

      /* get new mailbox count value, used as session handle */
      cnt = ec_nextmbxcnt(context->slavelist[slave].mbx_cnt);
      context->slavelist[slave].mbx_cnt = cnt;

      EOEp->mbxheader.length = htoes((uint16)(4 + txframesize));     /* no timestamp */
      EOEp->mbxheader.mbxtype = ECT_MBXT_EOE + MBX_HDR_SET_CNT(cnt); /* EoE */

      EOEp->frameinfo1 = htoes(frameinfo1);
      EOEp->frameinfo2 = htoes(frameinfo2);

      memcpy(EOEp->data, &buf[txframeoffset], txframesize);

      /* send EoE request to slave */
      wkc = ecx_mbxsend(context, slave, MbxOut, timeout);
      MbxOut = NULL;
      if ((NotLast == TRUE) && (wkc > 0))
      {
         txframeoffset += txframesize;
         txfragmentno++;
      }
   } while ((NotLast == TRUE) && (wkc > 0));

   if (MbxOut) ecx_dropmbx(context, MbxOut);

   return wkc;
}

/** EoE ethernet buffer read, blocking.
 *
 * If the buffer is larger than the mailbox size then the buffer is received
 * by several fragments. The function will assamble the fragments into
 * a complete Ethernet buffer.
 *
 * @param[in]     context context struct
 * @param[in]     slave   Slave number
 * @param[in]     port    Port number on slave if applicable
 * @param[in,out] psize   Size in bytes of parameter buffer.
 * @param[in]     p       Pointer to parameter buffer
 * @param[in]     timeout Timeout in us, standard is EC_TIMEOUTRXM
 * @return Workcounter from last slave response or error code
 */
int ecx_EOErecv(ecx_contextt *context, uint16 slave, uint8 port, int *psize, void *p, int timeout)
{
   ec_EOEt *aEOEp;
   ec_mbxbuft *MbxIn;
   uint16 frameinfo1, frameinfo2;
   uint8 rxfragmentno, rxframeno;
   boolean NotLast;
   int wkc, buffersize, rxframesize, rxframeoffset, eoedatasize;
   uint8 *buf = p;

   MbxIn = NULL;
   NotLast = TRUE;
   buffersize = *psize;
   rxfragmentno = 0;
   rxframeno = 0xff;
   rxframeoffset = 0;

   /* Hang for a while if nothing is in */
   wkc = ecx_mbxreceive(context, slave, &MbxIn, timeout);

   while ((wkc > 0) && (NotLast == TRUE))
   {
      aEOEp = (ec_EOEt *)MbxIn;
      /* slave response should be FoE */
      if ((aEOEp->mbxheader.mbxtype & 0x0f) == ECT_MBXT_EOE)
      {
         eoedatasize = etohs(aEOEp->mbxheader.length) - 0x00004;
         frameinfo1 = etohs(aEOEp->frameinfo1);
         frameinfo2 = etohs(aEOEp->frameinfo2);

         if (rxfragmentno != EOE_HDR_FRAG_NO_GET(frameinfo2))
         {
            if (EOE_HDR_FRAG_NO_GET(frameinfo2) > 0)
            {
               wkc = -EC_ERR_TYPE_EOE_INVALID_RX_DATA;
               /* Exit here*/
               break;
            }
         }

         if (rxfragmentno == 0)
         {
            rxframeno = EOE_HDR_FRAME_NO_GET(frameinfo2);
            rxframesize = (EOE_HDR_FRAME_OFFSET_GET(frameinfo2) << 5);
            if (rxframesize > buffersize)
            {
               wkc = -EC_ERR_TYPE_EOE_INVALID_RX_DATA;
               /* Exit here*/
               break;
            }
            if (port != EOE_HDR_FRAME_PORT_GET(frameinfo1))
            {
               wkc = -EC_ERR_TYPE_EOE_INVALID_RX_DATA;
               /* Exit here*/
               break;
            }
         }
         else
         {
            if (rxframeno != EOE_HDR_FRAME_NO_GET(frameinfo2))
            {
               wkc = -EC_ERR_TYPE_EOE_INVALID_RX_DATA;
               /* Exit here*/
               break;
            }
            else if (rxframeoffset != (EOE_HDR_FRAME_OFFSET_GET(frameinfo2) << 5))
            {
               wkc = -EC_ERR_TYPE_EOE_INVALID_RX_DATA;
               /* Exit here*/
               break;
            }
         }

         if ((rxframeoffset + eoedatasize) <= buffersize)
         {
            memcpy(&buf[rxframeoffset], aEOEp->data, eoedatasize);
            rxframeoffset += eoedatasize;
            rxfragmentno++;
         }

         if (EOE_HDR_LAST_FRAGMENT_GET(frameinfo1))
         {
            /* Remove timestamp */
            if (EOE_HDR_TIME_APPEND_GET(frameinfo1))
            {
               rxframeoffset -= 4;
            }
            NotLast = FALSE;
            *psize = rxframeoffset;
         }
         else
         {
            if (MbxIn) ecx_dropmbx(context, MbxIn);
            MbxIn = NULL;
            /* Hang for a while if nothing is in */
            wkc = ecx_mbxreceive(context, slave, &MbxIn, timeout);
         }
      }
      else
      {
         /* unexpected mailbox received */
         wkc = -EC_ERR_TYPE_PACKET_ERROR;
      }
   }
   if (MbxIn) ecx_dropmbx(context, MbxIn);
   return wkc;
}

/** EoE mailbox fragment read
 *
 * Will take the data in incoming mailbox buffer and copy to destination
 * Ethernet frame buffer at given offset and update current fragment variables
 *
 * @param[in] MbxIn             Received mailbox containing fragment data
 * @param[in,out] rxfragmentno  Fragment number
 * @param[in,out] rxframesize   Frame size
 * @param[in,out] rxframeoffset Frame offset
 * @param[in,out] rxframeno     Frame number
 * @param[in,out] psize         Size in bytes of frame buffer.
 * @param[out] p                Pointer to frame buffer
 * @return 0= if fragment OK, >0 if last fragment, <0 on error
 */
int ecx_EOEreadfragment(
    ec_mbxbuft *MbxIn,
    uint8 *rxfragmentno,
    uint16 *rxframesize,
    uint16 *rxframeoffset,
    uint16 *rxframeno,
    int *psize,
    void *p)
{
   uint16 frameinfo1, frameinfo2, eoedatasize;
   int wkc;
   ec_EOEt *aEOEp;
   uint8 *buf;

   aEOEp = (ec_EOEt *)MbxIn;
   buf = p;
   wkc = 0;

   /* slave response should be EoE */
   if ((aEOEp->mbxheader.mbxtype & 0x0f) == ECT_MBXT_EOE)
   {
      eoedatasize = etohs(aEOEp->mbxheader.length) - 0x00004;
      frameinfo1 = etohs(aEOEp->frameinfo1);
      frameinfo2 = etohs(aEOEp->frameinfo2);

      /* Retrieve fragment number, is it what we expect? */
      if (*rxfragmentno != EOE_HDR_FRAG_NO_GET(frameinfo2))
      {
         /* If expected fragment number is not 0, reset working variables */
         if (*rxfragmentno != 0)
         {
            *rxfragmentno = 0;
            *rxframesize = 0;
            *rxframeoffset = 0;
            *rxframeno = 0;
         }

         /* If incoming fragment number is not 0 we can't recover, exit */
         if (EOE_HDR_FRAG_NO_GET(frameinfo2) > 0)
         {
            wkc = -EC_ERR_TYPE_EOE_INVALID_RX_DATA;
            return wkc;
         }
      }

      /* Is it a new frame?*/
      if (*rxfragmentno == 0)
      {
         *rxframesize = (uint16)(EOE_HDR_FRAME_OFFSET_GET(frameinfo2) << 5);
         *rxframeoffset = 0;
         *rxframeno = EOE_HDR_FRAME_NO_GET(frameinfo2);
      }
      else
      {
         /* If we're inside a frame, make sure it is the same */
         if (*rxframeno != EOE_HDR_FRAME_NO_GET(frameinfo2))
         {
            *rxfragmentno = 0;
            *rxframesize = 0;
            *rxframeoffset = 0;
            *rxframeno = 0;
            wkc = -EC_ERR_TYPE_EOE_INVALID_RX_DATA;
            return wkc;
         }
         else if (*rxframeoffset != (EOE_HDR_FRAME_OFFSET_GET(frameinfo2) << 5))
         {
            *rxfragmentno = 0;
            *rxframesize = 0;
            *rxframeoffset = 0;
            *rxframeno = 0;
            wkc = -EC_ERR_TYPE_EOE_INVALID_RX_DATA;
            return wkc;
         }
      }

      /* Make sure we're inside expected frame size */
      if (((*rxframeoffset + eoedatasize) <= *rxframesize) &&
          ((*rxframeoffset + eoedatasize) <= *psize))
      {
         memcpy(&buf[*rxframeoffset], aEOEp->data, eoedatasize);
         *rxframeoffset += eoedatasize;
         *rxfragmentno += 1;
      }

      /* Is it the last fragment */
      if (EOE_HDR_LAST_FRAGMENT_GET(frameinfo1))
      {
         /* Remove timestamp */
         if (EOE_HDR_TIME_APPEND_GET(frameinfo1))
         {
            *rxframeoffset -= 4;
         }
         *psize = *rxframeoffset;
         *rxfragmentno = 0;
         *rxframesize = 0;
         *rxframeoffset = 0;
         *rxframeno = 0;
         wkc = 1;
      }
   }
   else
   {
      /* unexpected mailbox received */
      wkc = -EC_ERR_TYPE_PACKET_ERROR;
   }
   return wkc;
}
