/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * File over EtherCAT (FoE) module.
 *
 * SDO read / write and SDO service functions
 */

#include <stdio.h>
#include <string.h>
#include "osal.h"
#include "oshw.h"
#include "ethercattype.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercatfoe.h"

#define EC_MAXFOEDATA 512

/** FOE structure.
 * Used for Read, Write, Data, Ack and Error mailbox packets.
 */
PACKED_BEGIN
typedef struct PACKED
{
   ec_mbxheadert MbxHeader;
   uint8         OpCode;
   uint8         Reserved;
   union
   {
      uint32        Password;
      uint32        PacketNumber;
      uint32        ErrorCode;
   };
   union
   {
      char          FileName[EC_MAXFOEDATA];
      uint8         Data[EC_MAXFOEDATA];
      char          ErrorText[EC_MAXFOEDATA];
   };
} ec_FOEt;
PACKED_END

/** FoE progress hook.
 *
 * @param[in]  context        = context struct
 * @param[in]     hook       = Pointer to hook function.
 * @return 1
 */
int ecx_FOEdefinehook(ecx_contextt *context, void *hook)
{
  context->FOEhook = hook;
  return 1;
}

/** FoE read, blocking.
 *
 * @param[in]  context        = context struct
 * @param[in]     slave      = Slave number.
 * @param[in]     filename   = Filename of file to read.
 * @param[in]     password   = password.
 * @param[in,out] psize      = Size in bytes of file buffer, returns bytes read from file.
 * @param[out]    p          = Pointer to file buffer
 * @param[in]     timeout    = Timeout per mailbox cycle in us, standard is EC_TIMEOUTRXM
 * @return Workcounter from last slave response
 */
int ecx_FOEread(ecx_contextt *context, uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout)
{
   ec_FOEt *FOEp, *aFOEp;
   int wkc;
   int32 dataread = 0;
   int32 buffersize, packetnumber, prevpacket = 0;
   uint16 fnsize, maxdata, segmentdata;
   ec_mbxbuft MbxIn, MbxOut;
   uint8 cnt;
   boolean worktodo;

   buffersize = *psize;
   ec_clearmbx(&MbxIn);
   /* Empty slave out mailbox if something is in. Timout set to 0 */
   wkc = ecx_mbxreceive(context, slave, (ec_mbxbuft *)&MbxIn, 0);
   ec_clearmbx(&MbxOut);
   aFOEp = (ec_FOEt *)&MbxIn;
   FOEp = (ec_FOEt *)&MbxOut;
   fnsize = (uint16)strlen(filename);
   maxdata = context->slavelist[slave].mbx_l - 12;
   if (fnsize > maxdata)
   {
      fnsize = maxdata;
   }
   FOEp->MbxHeader.length = htoes(0x0006 + fnsize);
   FOEp->MbxHeader.address = htoes(0x0000);
   FOEp->MbxHeader.priority = 0x00;
   /* get new mailbox count value, used as session handle */
   cnt = ec_nextmbxcnt(context->slavelist[slave].mbx_cnt);
   context->slavelist[slave].mbx_cnt = cnt;
   FOEp->MbxHeader.mbxtype = ECT_MBXT_FOE + (cnt << 4); /* FoE */
   FOEp->OpCode = ECT_FOE_READ;
   FOEp->Password = htoel(password);
   /* copy filename in mailbox */
   memcpy(&FOEp->FileName[0], filename, fnsize);
   /* send FoE request to slave */
   wkc = ecx_mbxsend(context, slave, (ec_mbxbuft *)&MbxOut, EC_TIMEOUTTXM);
   if (wkc > 0) /* succeeded to place mailbox in slave ? */
   {
      do
      {
         worktodo = FALSE;
         /* clean mailboxbuffer */
         ec_clearmbx(&MbxIn);
         /* read slave response */
         wkc = ecx_mbxreceive(context, slave, (ec_mbxbuft *)&MbxIn, timeout);
         if (wkc > 0) /* succeeded to read slave response ? */
         {
            /* slave response should be FoE */
            if ((aFOEp->MbxHeader.mbxtype & 0x0f) == ECT_MBXT_FOE)
            {
               if(aFOEp->OpCode == ECT_FOE_DATA)
               {
                  segmentdata = etohs(aFOEp->MbxHeader.length) - 0x0006;
                  packetnumber = etohl(aFOEp->PacketNumber);
                  if ((packetnumber == ++prevpacket) && (dataread + segmentdata <= buffersize))
                  {
                     memcpy(p, &aFOEp->Data[0], segmentdata);
                     dataread += segmentdata;
                     p = (uint8 *)p + segmentdata;
                     if (segmentdata == maxdata)
                     {
                        worktodo = TRUE;
                     }
                     FOEp->MbxHeader.length = htoes(0x0006);
                     FOEp->MbxHeader.address = htoes(0x0000);
                     FOEp->MbxHeader.priority = 0x00;
                     /* get new mailbox count value */
                     cnt = ec_nextmbxcnt(context->slavelist[slave].mbx_cnt);
                     context->slavelist[slave].mbx_cnt = cnt;
                     FOEp->MbxHeader.mbxtype = ECT_MBXT_FOE + (cnt << 4); /* FoE */
                     FOEp->OpCode = ECT_FOE_ACK;
                     FOEp->PacketNumber = htoel(packetnumber);
                     /* send FoE ack to slave */
                     wkc = ecx_mbxsend(context, slave, (ec_mbxbuft *)&MbxOut, EC_TIMEOUTTXM);
                     if (wkc <= 0)
                     {
                        worktodo = FALSE;
                     }
                     if (context->FOEhook)
                     {
                        context->FOEhook(slave, packetnumber, dataread);
                     }
                  }
                  else
                  {
                     /* FoE error */
                     wkc = -EC_ERR_TYPE_FOE_BUF2SMALL;
                  }
               }
               else
               {
                  if(aFOEp->OpCode == ECT_FOE_ERROR)
                  {
                     /* FoE error */
                     wkc = -EC_ERR_TYPE_FOE_ERROR;
                  }
                  else
                  {
                     /* unexpected mailbox received */
                     wkc = -EC_ERR_TYPE_PACKET_ERROR;
                  }
               }
            }
            else
            {
               /* unexpected mailbox received */
               wkc = -EC_ERR_TYPE_PACKET_ERROR;
            }
            *psize = dataread;
         }
      } while (worktodo);
   }

   return wkc;
}

/** FoE write, blocking.
 *
 * @param[in]  context        = context struct
 * @param[in]  slave      = Slave number.
 * @param[in]  filename   = Filename of file to write.
 * @param[in]  password   = password.
 * @param[in]  psize      = Size in bytes of file buffer.
 * @param[out] p          = Pointer to file buffer
 * @param[in]  timeout    = Timeout per mailbox cycle in us, standard is EC_TIMEOUTRXM
 * @return Workcounter from last slave response
 */
int ecx_FOEwrite(ecx_contextt *context, uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout)
{
   ec_FOEt *FOEp, *aFOEp;
   int wkc;
   int32 packetnumber, sendpacket = 0;
   uint16 fnsize, maxdata;
   int segmentdata;
   ec_mbxbuft MbxIn, MbxOut;
   uint8 cnt;
   boolean worktodo, dofinalzero;
   int tsize;

   ec_clearmbx(&MbxIn);
   /* Empty slave out mailbox if something is in. Timout set to 0 */
   wkc = ecx_mbxreceive(context, slave, (ec_mbxbuft *)&MbxIn, 0);
   ec_clearmbx(&MbxOut);
   aFOEp = (ec_FOEt *)&MbxIn;
   FOEp = (ec_FOEt *)&MbxOut;
   dofinalzero = FALSE;
   fnsize = (uint16)strlen(filename);
   maxdata = context->slavelist[slave].mbx_l - 12;
   if (fnsize > maxdata)
   {
      fnsize = maxdata;
   }
   FOEp->MbxHeader.length = htoes(0x0006 + fnsize);
   FOEp->MbxHeader.address = htoes(0x0000);
   FOEp->MbxHeader.priority = 0x00;
   /* get new mailbox count value, used as session handle */
   cnt = ec_nextmbxcnt(context->slavelist[slave].mbx_cnt);
   context->slavelist[slave].mbx_cnt = cnt;
   FOEp->MbxHeader.mbxtype = ECT_MBXT_FOE + (cnt << 4); /* FoE */
   FOEp->OpCode = ECT_FOE_WRITE;
   FOEp->Password = htoel(password);
   /* copy filename in mailbox */
   memcpy(&FOEp->FileName[0], filename, fnsize);
   /* send FoE request to slave */
   wkc = ecx_mbxsend(context, slave, (ec_mbxbuft *)&MbxOut, EC_TIMEOUTTXM);
   if (wkc > 0) /* succeeded to place mailbox in slave ? */
   {
      do
      {
         worktodo = FALSE;
         /* clean mailboxbuffer */
         ec_clearmbx(&MbxIn);
         /* read slave response */
         wkc = ecx_mbxreceive(context, slave, (ec_mbxbuft *)&MbxIn, timeout);
         if (wkc > 0) /* succeeded to read slave response ? */
         {
            /* slave response should be FoE */
            if ((aFOEp->MbxHeader.mbxtype & 0x0f) == ECT_MBXT_FOE)
            {
               switch (aFOEp->OpCode)
               {
                  case ECT_FOE_ACK:
                  {
                     packetnumber = etohl(aFOEp->PacketNumber);
                     if (packetnumber == sendpacket)
                     {
                        if (context->FOEhook)
                        {
                           context->FOEhook(slave, packetnumber, psize);
                        }
                        tsize = psize;
                        if (tsize > maxdata)
                        {
                           tsize = maxdata;
                        }
                        if(tsize || dofinalzero)
                        {
                           worktodo = TRUE;
                           dofinalzero = FALSE;
                           segmentdata = tsize;
                           psize -= segmentdata;
                           /* if last packet was full size, add a zero size packet as final */
                           /* EOF is defined as packetsize < full packetsize */
                           if (!psize && (segmentdata == maxdata))
                           {
                              dofinalzero = TRUE;
                           }
                           FOEp->MbxHeader.length = htoes(0x0006 + segmentdata);
                           FOEp->MbxHeader.address = htoes(0x0000);
                           FOEp->MbxHeader.priority = 0x00;
                           /* get new mailbox count value */
                           cnt = ec_nextmbxcnt(context->slavelist[slave].mbx_cnt);
                           context->slavelist[slave].mbx_cnt = cnt;
                           FOEp->MbxHeader.mbxtype = ECT_MBXT_FOE + (cnt << 4); /* FoE */
                           FOEp->OpCode = ECT_FOE_DATA;
                           sendpacket++;
                           FOEp->PacketNumber = htoel(sendpacket);
                           memcpy(&FOEp->Data[0], p, segmentdata);
                           p = (uint8 *)p + segmentdata;
                           /* send FoE data to slave */
                           wkc = ecx_mbxsend(context, slave, (ec_mbxbuft *)&MbxOut, EC_TIMEOUTTXM);
                           if (wkc <= 0)
                           {
                              worktodo = FALSE;
                           }
                        }
                     }
                     else
                     {
                        /* FoE error */
                        wkc = -EC_ERR_TYPE_FOE_PACKETNUMBER;
                     }
                     break;
                  }
                  case ECT_FOE_BUSY:
                  {
                     /* resend if data has been send before */
                     /* otherwise ignore */
                     if (sendpacket)
                     {
                        if (!psize)
                        {
                           dofinalzero = TRUE;
                        }
                        psize += segmentdata;
                        p = (uint8 *)p - segmentdata;
                        --sendpacket;
                     }
                     break;
                  }
                  case ECT_FOE_ERROR:
                  {
                     /* FoE error */
                     if (aFOEp->ErrorCode == 0x8001)
                     {
                        wkc = -EC_ERR_TYPE_FOE_FILE_NOTFOUND;
                     }
                     else
                     {
                        wkc = -EC_ERR_TYPE_FOE_ERROR;
                     }
                     break;
                  }
                  default:
                  {
                     /* unexpected mailbox received */
                     wkc = -EC_ERR_TYPE_PACKET_ERROR;
                     break;
                  }
               }
            }
            else
            {
               /* unexpected mailbox received */
               wkc = -EC_ERR_TYPE_PACKET_ERROR;
            }
         }
      } while (worktodo);
   }

   return wkc;
}

#ifdef EC_VER1
int ec_FOEdefinehook(void *hook)
{
   return ecx_FOEdefinehook(&ecx_context, hook);
}

int ec_FOEread(uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout)
{
   return ecx_FOEread(&ecx_context, slave, filename, password, psize, p, timeout);
}

int ec_FOEwrite(uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout)
{
   return ecx_FOEwrite(&ecx_context, slave, filename, password, psize, p, timeout);
}
#endif
