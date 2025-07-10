/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/**
 * \file
 * \brief
 * Main EtherCAT functions.
 *
 * Initialisation, state set and read, mailbox primitives, EEPROM primitives,
 * SII reading and processdata exchange.
 */

#include "soem/soem.h"
#include <string.h>
#include "osal.h"
#include "oshw.h"

/** delay in us for eeprom ready loop */
#define EC_LOCALDELAY 200

/** record for ethercat eeprom communications */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED
{
   uint16 comm;
   uint16 addr;
   uint16 d2;
} ec_eepromt;
OSAL_PACKED_END

/** mailbox error structure */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED
{
   ec_mbxheadert MbxHeader;
   uint16 Type;
   uint16 Detail;
} ec_mbxerrort;
OSAL_PACKED_END

/** emergency request structure */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED
{
   ec_mbxheadert MbxHeader;
   uint16 CANOpen;
   uint16 ErrorCode;
   uint8 ErrorReg;
   uint8 bData;
   uint16 w1, w2;
} ec_emcyt;
OSAL_PACKED_END

/** Create list over available network adapters.
 *
 * @return First element in list over available network adapters.
 */
ec_adaptert *ec_find_adapters(void)
{
   ec_adaptert *ret_adapter;

   ret_adapter = oshw_find_adapters();

   return ret_adapter;
}

/** Free dynamically allocated list over available network adapters.
 *
 * @param[in] adapter Struct holding adapter name, description and pointer to next.
 */
void ec_free_adapters(ec_adaptert *adapter)
{
   oshw_free_adapters(adapter);
}

/** Pushes an error on the error list.
 *
 * @param[in] context        context struct
 * @param[in] Ec pointer describing the error.
 */
void ecx_pusherror(ecx_contextt *context, const ec_errort *Ec)
{
   context->elist.Error[context->elist.head] = *Ec;
   context->elist.Error[context->elist.head].Signal = TRUE;
   context->elist.head++;
   if (context->elist.head > EC_MAXELIST)
   {
      context->elist.head = 0;
   }
   if (context->elist.head == context->elist.tail)
   {
      context->elist.tail++;
   }
   if (context->elist.tail > EC_MAXELIST)
   {
      context->elist.tail = 0;
   }
   context->ecaterror = TRUE;
}

/** Pops an error from the list.
 *
 * @param[in] context        context struct
 * @param[out] Ec Struct describing the error.
 * @return TRUE if an error was popped.
 */
boolean ecx_poperror(ecx_contextt *context, ec_errort *Ec)
{
   boolean notEmpty = (context->elist.head != context->elist.tail);

   *Ec = context->elist.Error[context->elist.tail];
   context->elist.Error[context->elist.tail].Signal = FALSE;
   if (notEmpty)
   {
      context->elist.tail++;
      if (context->elist.tail > EC_MAXELIST)
      {
         context->elist.tail = 0;
      }
   }
   else
   {
      context->ecaterror = FALSE;
   }
   return notEmpty;
}

/** Check if error list has entries.
 *
 * @param[in] context        context struct
 * @return TRUE if error list contains entries.
 */
boolean ecx_iserror(ecx_contextt *context)
{
   return (context->elist.head != context->elist.tail);
}

/** Report packet error
 *
 * @param[in]  context    context struct
 * @param[in]  Slave      Slave number
 * @param[in]  Index      Index that generated error
 * @param[in]  SubIdx     Subindex that generated error
 * @param[in]  ErrorCode  Error code
 */
void ecx_packeterror(ecx_contextt *context, uint16 Slave, uint16 Index, uint8 SubIdx, uint16 ErrorCode)
{
   ec_errort Ec;

   memset(&Ec, 0, sizeof(Ec));
   Ec.Time = osal_current_time();
   Ec.Slave = Slave;
   Ec.Index = Index;
   Ec.SubIdx = SubIdx;
   context->ecaterror = TRUE;
   Ec.Etype = EC_ERR_TYPE_PACKET_ERROR;
   Ec.ErrorCode = ErrorCode;
   ecx_pusherror(context, &Ec);
}

/** Report Mailbox Error
 *
 * @param[in]  context      context struct
 * @param[in]  Slave        Slave number
 * @param[in]  Detail       Following EtherCAT specification
 */
static void ecx_mbxerror(ecx_contextt *context, uint16 Slave, uint16 Detail)
{
   ec_errort Ec;

   memset(&Ec, 0, sizeof(Ec));
   Ec.Time = osal_current_time();
   Ec.Slave = Slave;
   Ec.Index = 0;
   Ec.SubIdx = 0;
   Ec.Etype = EC_ERR_TYPE_MBX_ERROR;
   Ec.ErrorCode = Detail;
   ecx_pusherror(context, &Ec);
}

/** Report Mailbox Emergency Error
 *
 * @param[in]  context    context struct
 * @param[in]  Slave      Slave number
 * @param[in]  ErrorCode  Following EtherCAT specification
 * @param[in]  ErrorReg
 * @param[in]  b1
 * @param[in]  w1
 * @param[in]  w2
 */
static void ecx_mbxemergencyerror(ecx_contextt *context, uint16 Slave, uint16 ErrorCode, uint16 ErrorReg,
                                  uint8 b1, uint16 w1, uint16 w2)
{
   ec_errort Ec;

   memset(&Ec, 0, sizeof(Ec));
   Ec.Time = osal_current_time();
   Ec.Slave = Slave;
   Ec.Index = 0;
   Ec.SubIdx = 0;
   Ec.Etype = EC_ERR_TYPE_EMERGENCY;
   Ec.ErrorCode = ErrorCode;
   Ec.ErrorReg = (uint8)ErrorReg;
   Ec.b1 = b1;
   Ec.w1 = w1;
   Ec.w2 = w2;
   ecx_pusherror(context, &Ec);
}

/** Initialise lib in single NIC mode
 * @param[in]  context context struct
 * @param[in] ifname   Dev name, f.e. "eth0"
 * @return >0 if OK
 */
int ecx_init(ecx_contextt *context, const char *ifname)
{
   ecx_initmbxpool(context);
   return ecx_setupnic(&context->port, ifname, FALSE);
}

/** Initialise lib in redundant NIC mode
 * @param[in]  context  context struct
 * @param[in]  redport  pointer to redport, redundant port data
 * @param[in]  ifname   Primary Dev name, f.e. "eth0"
 * @param[in]  if2name  Secondary Dev name, f.e. "eth1"
 * @return >0 if OK
 */
int ecx_init_redundant(ecx_contextt *context, ecx_redportt *redport, const char *ifname, char *if2name)
{
   int rval, zbuf;
   ec_etherheadert *ehp;

   ecx_initmbxpool(context);
   context->port.redport = redport;
   ecx_setupnic(&context->port, ifname, FALSE);
   rval = ecx_setupnic(&context->port, if2name, TRUE);
   /* prepare "dummy" BRD tx frame for redundant operation */
   ehp = (ec_etherheadert *)&(context->port.txbuf2);
   ehp->sa1 = oshw_htons(secMAC[0]);
   zbuf = 0;
   ecx_setupdatagram(&context->port, &(context->port.txbuf2), EC_CMD_BRD, 0, 0x0000, 0x0000, 2, &zbuf);
   context->port.txbuflength2 = ETH_HEADERSIZE + EC_HEADERSIZE + EC_WKCSIZE + 2;

   return rval;
}

/** Close lib.
 * @param[in]  context   context struct
 */
void ecx_close(ecx_contextt *context)
{
   osal_mutex_destroy(context->mbxpool.mbxmutex);
   ecx_closenic(&context->port);
}

/**
 * Get a mailbox from the mailbox pool.
 * @param[in]  context   context struct
 * @return Pointer to the mailbox if available, otherwise NULL.
 */
ec_mbxbuft *ecx_getmbx(ecx_contextt *context)
{
   ec_mbxbuft *mbx = NULL;
   ec_mbxpoolt *mbxpool = &context->mbxpool;
   osal_mutex_lock(mbxpool->mbxmutex);
   if (mbxpool->listcount > 0)
   {
      mbx = (ec_mbxbuft *)&(mbxpool->mbx[mbxpool->mbxemptylist[mbxpool->listtail]]);
      EC_PRINT("getmbx item:%d mbx:%p\n\r", mbxpool->mbxemptylist[mbxpool->listtail], mbx);
      mbxpool->listtail++;
      if (mbxpool->listtail >= EC_MBXPOOLSIZE) mbxpool->listtail = 0;
      mbxpool->listcount--;
   }
   osal_mutex_unlock(mbxpool->mbxmutex);
   return mbx;
}

/** Drop a mailbox back to the mailbox pool.
 * @param[in]  context   context struct
 * @param[in]  mbx       Pointer to mailbox to be dropped
 * @return 1 on success, 0 if the mailbox is invalid.
 */
int ecx_dropmbx(ecx_contextt *context, ec_mbxbuft *mbx)
{
   ec_mbxpoolt *mbxpool = &context->mbxpool;
   int item = (int)(mbx - &(mbxpool->mbx[0]));
   EC_PRINT("dropmbx item:%d mbx:%p\n\r", item, mbx);
   if ((item >= 0) && (item < EC_MBXPOOLSIZE))
   {
      osal_mutex_lock(mbxpool->mbxmutex);
      mbxpool->mbxemptylist[mbxpool->listhead++] = item;
      if (mbxpool->listhead >= EC_MBXPOOLSIZE) mbxpool->listhead = 0;
      mbxpool->listcount++;
      osal_mutex_unlock(mbxpool->mbxmutex);
      return 1;
   }
   return 0;
}

/**
 * Initialize the mailbox pool.
 *
 * Sets up the mailbox pool mutex and initializes the empty list with
 * all available mailboxes.
 *
 * @param[in] context        context struct
 * @return 0 on success.
 */
int ecx_initmbxpool(ecx_contextt *context)
{
   int retval = 0;
   ec_mbxpoolt *mbxpool = &context->mbxpool;
   mbxpool->mbxmutex = (osal_mutext *)osal_mutex_create();
   for (int item = 0; item < EC_MBXPOOLSIZE; item++)
   {
      mbxpool->mbxemptylist[item] = item;
   }
   mbxpool->listhead = 0;
   mbxpool->listtail = 0;
   mbxpool->listcount = EC_MBXPOOLSIZE;
   EC_PRINT("intmbxpool mbxp:%p mutex:%p\n\r", mbxpool->mbx[0], mbxpool->mbxmutex);
   return retval;
}

/** Initialize mailbox queue.
 * @param[in]  context        context struct
 * @param[in]  group          group number
 * @return 0 on success.
 */
int ecx_initmbxqueue(ecx_contextt *context, uint8 group)
{
   int retval = 0;
   int cnt;
   ec_mbxqueuet *mbxqueue = &(context->grouplist[group].mbxtxqueue);
   mbxqueue->mbxmutex = (osal_mutext *)osal_mutex_create();
   mbxqueue->listhead = 0;
   mbxqueue->listtail = 0;
   mbxqueue->listcount = 0;
   for (cnt = 0; cnt < EC_MBXPOOLSIZE; cnt++)
      mbxqueue->mbxticket[cnt] = -1;
   return retval;
}

/** Add a mailbox to the queue for a specific slave.
 * @param[in]  context        context struct
 * @param[in]  slave          Slave number
 * @param[in]  mbx            Pointer to mailbox
 * @return Ticket number of the added mailbox, or -1 on failure.
 */
int ecx_mbxaddqueue(ecx_contextt *context, uint16 slave, ec_mbxbuft *mbx)
{
   int ticketloc;
   int ticket = -1;
   uint8 group = context->slavelist[slave].group;
   ec_mbxqueuet *mbxqueue = &(context->grouplist[group].mbxtxqueue);
   osal_mutex_lock(mbxqueue->mbxmutex);
   if ((mbxqueue->listcount < EC_MBXPOOLSIZE))
   {
      ticketloc = mbxqueue->listhead;
      while ((++ticket < EC_MBXPOOLSIZE) && (mbxqueue->mbxticket[ticket] >= 0))
      {
      };
      mbxqueue->mbxticket[ticket] = ticketloc;
      mbxqueue->mbxslave[ticketloc] = slave;
      mbxqueue->mbx[ticketloc] = mbx;
      mbxqueue->listhead++;
      if (mbxqueue->listhead >= EC_MBXPOOLSIZE) mbxqueue->listhead = 0;
      mbxqueue->mbxremove[ticketloc] = 0;
      mbxqueue->mbxstate[ticketloc] = EC_MBXQUEUESTATE_REQ;
      mbxqueue->listcount++;
   }
   osal_mutex_unlock(mbxqueue->mbxmutex);
   return ticket;
}

/** Mark a mailbox in the queue as done.
 * @param[in]  context        context struct
 * @param[in]  slave          Slave number
 * @param[in]  ticket         Ticket number of the mailbox
 * @return 1 on success, 0 if the ticket is invalid or not done.
 */
int ecx_mbxdonequeue(ecx_contextt *context, uint16 slave, int ticket)
{
   int retval = 0;
   if ((ticket >= 0) && (ticket < EC_MBXPOOLSIZE))
   {
      uint8 group = context->slavelist[slave].group;
      ec_mbxqueuet *mbxqueue = &(context->grouplist[group].mbxtxqueue);
      osal_mutex_lock(mbxqueue->mbxmutex);
      if (mbxqueue->mbxstate[mbxqueue->mbxticket[ticket]] == EC_MBXQUEUESTATE_DONE)
      {
         mbxqueue->mbxremove[mbxqueue->mbxticket[ticket]] = 1;
         mbxqueue->mbxticket[ticket] = -1;
         retval = 1;
      }
      osal_mutex_unlock(mbxqueue->mbxmutex);
   }
   return retval;
}

/** Expire a mailbox in the queue.
 * @param[in]  context        context struct
 * @param[in]  slave          Slave number
 * @param[in]  ticket         Ticket number of the mailbox
 * @return 1 on success, 0 if the ticket is invalid or not expired.
 */
int ecx_mbxexpirequeue(ecx_contextt *context, uint16 slave, int ticket)
{
   int retval = 0;
   if ((ticket >= 0) && (ticket < EC_MBXPOOLSIZE))
   {
      uint8 group = context->slavelist[slave].group;
      ec_mbxqueuet *mbxqueue = &(context->grouplist[group].mbxtxqueue);
      osal_mutex_lock(mbxqueue->mbxmutex);
      if (mbxqueue->mbxstate[mbxqueue->mbxticket[ticket]] > EC_MBXQUEUESTATE_NONE)
      {
         mbxqueue->mbxremove[mbxqueue->mbxticket[ticket]] = 1;
         mbxqueue->mbxticket[ticket] = -1;
         retval = 1;
      }
      osal_mutex_unlock(mbxqueue->mbxmutex);
   }
   return retval;
}

/** Rotate a mailbox in the queue.
 * @param[in]  context        context struct
 * @param[in]  group          Group number
 * @param[in]  ticketloc      Ticket location in the queue
 * @return 1 on success, 0 if rotation is not possible.
 */
int ecx_mbxrotatequeue(ecx_contextt *context, uint8 group, int ticketloc)
{
   int retval = 0;
   int cnt = 0;
   int ticket;
   ec_mbxqueuet *mbxqueue = &(context->grouplist[group].mbxtxqueue);
   osal_mutex_lock(mbxqueue->mbxmutex);
   int head = mbxqueue->listhead;
   int tail = mbxqueue->listtail;
   if (head != tail)
   {
      while ((cnt < EC_MBXPOOLSIZE) && (mbxqueue->mbxticket[cnt] != ticketloc))
         cnt++;
      ticket = cnt;
      if ((ticket >= 0) && (ticket < EC_MBXPOOLSIZE))
      {
         mbxqueue->mbxticket[ticket] = head;
         mbxqueue->mbxremove[head] = mbxqueue->mbxremove[tail];
         mbxqueue->mbxstate[head] = mbxqueue->mbxstate[tail];
         mbxqueue->mbxslave[head] = mbxqueue->mbxslave[tail];
         mbxqueue->mbx[head] = mbxqueue->mbx[tail];
         mbxqueue->listhead++;
         if (mbxqueue->listhead >= EC_MBXPOOLSIZE) mbxqueue->listhead = 0;
         mbxqueue->listtail++;
         if (mbxqueue->listtail >= EC_MBXPOOLSIZE) mbxqueue->listtail = 0;
         retval = 1;
      }
   }
   else
   {
      retval = 1;
   }
   osal_mutex_unlock(mbxqueue->mbxmutex);
   return retval;
}

/** Set a slave's mailbox to be cyclic.
 * @param[in]  context        context struct
 * @param[in]  slave          Slave number
 * @return 1 if the mailbox was set to cyclic, 0 if it cannot be set.
 */
int ecx_slavembxcyclic(ecx_contextt *context, uint16 slave)
{
   if (context->slavelist[slave].mbxstatus)
   {
      context->slavelist[slave].coembxin = EC_MBXINENABLE;
      context->slavelist[slave].mbxhandlerstate = ECT_MBXH_CYCLIC;
      return 1;
   }
   return 0;
}

/** Drop a mailbox from the queue.
 * @param[in]  context        context struct
 * @param[in]  group          Group number
 * @param[in]  ticketloc      Ticket location in the queue
 * @return Pointer to the dropped mailbox
 */
ec_mbxbuft *ecx_mbxdropqueue(ecx_contextt *context, uint8 group, int ticketloc)
{
   ec_mbxbuft *mbx;
   ec_mbxqueuet *mbxqueue = &(context->grouplist[group].mbxtxqueue);
   osal_mutex_lock(mbxqueue->mbxmutex);
   mbxqueue->mbxstate[ticketloc] = EC_MBXQUEUESTATE_NONE;
   mbxqueue->mbxremove[ticketloc] = 0;
   mbxqueue->mbxslave[ticketloc] = 0;
   mbx = mbxqueue->mbx[ticketloc];
   mbxqueue->mbx[ticketloc] = NULL;
   mbxqueue->listtail++;
   if (mbxqueue->listtail >= EC_MBXPOOLSIZE) mbxqueue->listtail = 0;
   mbxqueue->listcount--;
   osal_mutex_unlock(mbxqueue->mbxmutex);
   return mbx;
}

/** Read one byte from slave EEPROM via cache.
 *  If the cache location is empty then a read request is made to the slave.
 *  Depending on the slave capabilities the request is 4 or 8 bytes.
 *  @param[in] context context struct
 *  @param[in] slave   slave number
 *  @param[in] address eeprom address in bytes (slave uses words)
 *  @return requested byte, if not available then 0xff
 */
uint8 ecx_siigetbyte(ecx_contextt *context, uint16 slave, uint16 address)
{
   uint16 configadr, eadr;
   uint64 edat64;
   uint32 edat32;
   uint16 mapw, mapb;
   int lp, cnt;
   uint8 retval;

   retval = 0xff;
   if (slave != context->esislave) /* not the same slave? */
   {
      memset(context->esimap, 0x00, EC_MAXEEPBITMAP * sizeof(uint32)); /* clear esibuf cache map */
      context->esislave = slave;
   }
   if (address < EC_MAXEEPBUF)
   {
      mapw = address >> 5;
      mapb = (uint16)(address - (mapw << 5));
      if (context->esimap[mapw] & (1U << mapb))
      {
         /* byte is already in buffer */
         retval = context->esibuf[address];
      }
      else
      {
         /* byte is not in buffer, put it there */
         configadr = context->slavelist[slave].configadr;
         ecx_eeprom2master(context, slave); /* set eeprom control to master */
         eadr = address >> 1;
         edat64 = ecx_readeepromFP(context, configadr, eadr, EC_TIMEOUTEEP);
         /* 8 byte response */
         if (context->slavelist[slave].eep_8byte)
         {
            put_unaligned64(edat64, &(context->esibuf[eadr << 1]));
            cnt = 8;
         }
         /* 4 byte response */
         else
         {
            edat32 = (uint32)edat64;
            put_unaligned32(edat32, &(context->esibuf[eadr << 1]));
            cnt = 4;
         }
         /* find bitmap location */
         mapw = eadr >> 4;
         mapb = (uint16)((eadr << 1) - (mapw << 5));
         for (lp = 0; lp < cnt; lp++)
         {
            /* set bitmap for each byte that is read */
            context->esimap[mapw] |= (1U << mapb);
            mapb++;
            if (mapb > 31)
            {
               mapb = 0;
               mapw++;
            }
         }
         retval = context->esibuf[address];
      }
   }

   return retval;
}

/** Find SII section header in slave EEPROM.
 *  @param[in] context context struct
 *  @param[in] slave   slave number
 *  @param[in] cat     section category
 *  @return byte address of section at section length entry, if not available then 0
 */
int16 ecx_siifind(ecx_contextt *context, uint16 slave, uint16 cat)
{
   int16 a;
   uint16 p;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   a = ECT_SII_START << 1;
   /* read first SII section category */
   p = ecx_siigetbyte(context, slave, a++);
   p += (ecx_siigetbyte(context, slave, a++) << 8);
   /* traverse SII while category is not found and not EOF */
   while ((p != cat) && (p != 0xffff))
   {
      /* read section length */
      p = ecx_siigetbyte(context, slave, a++);
      p += (ecx_siigetbyte(context, slave, a++) << 8);
      /* locate next section category */
      a += p << 1;
      /* read section category */
      p = ecx_siigetbyte(context, slave, a++);
      p += (ecx_siigetbyte(context, slave, a++) << 8);
   }
   if (p != cat)
   {
      a = 0;
   }
   if (eectl)
   {
      ecx_eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return a;
}

/** Get string from SII string section in slave EEPROM.
 *  @param[in]  context context struct
 *  @param[out] str     requested string, 0x00 if not found
 *  @param[in]  slave   slave number
 *  @param[in]  Sn      string number
 */
void ecx_siistring(ecx_contextt *context, char *str, uint16 slave, uint16 Sn)
{
   uint16 a, i, j, l, n, ba;
   char *ptr;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   ptr = str;
   a = ecx_siifind(context, slave, ECT_SII_STRING); /* find string section */
   if (a > 0)
   {
      ba = a + 2;                               /* skip SII section header */
      n = ecx_siigetbyte(context, slave, ba++); /* read number of strings in section */
      if (Sn <= n)                              /* is req string available? */
      {
         for (i = 1; i <= Sn; i++) /* walk through strings */
         {
            l = ecx_siigetbyte(context, slave, ba++); /* length of this string */
            if (i < Sn)
            {
               ba += l;
            }
            else
            {
               ptr = str;
               for (j = 1; j <= l; j++) /* copy one string */
               {
                  if (j <= EC_MAXNAME)
                  {
                     *ptr = (char)ecx_siigetbyte(context, slave, ba++);
                     ptr++;
                  }
                  else
                  {
                     ba++;
                  }
               }
            }
         }
         *ptr = 0; /* add zero terminator */
      }
      else
      {
         ptr = str;
         *ptr = 0; /* empty string */
      }
   }
   if (eectl)
   {
      ecx_eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }
}

/** Get FMMU data from SII FMMU section in slave EEPROM.
 *  @param[in]  context context struct
 *  @param[in]  slave   slave number
 *  @param[out] FMMU    FMMU struct from SII, max. 4 FMMU's
 *  @return number of FMMU's defined in section
 */
uint16 ecx_siiFMMU(ecx_contextt *context, uint16 slave, ec_eepromFMMUt *FMMU)
{
   uint16 a;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   FMMU->nFMMU = 0;
   FMMU->FMMU0 = 0;
   FMMU->FMMU1 = 0;
   FMMU->FMMU2 = 0;
   FMMU->FMMU3 = 0;
   FMMU->Startpos = ecx_siifind(context, slave, ECT_SII_FMMU);

   if (FMMU->Startpos > 0)
   {
      a = FMMU->Startpos;
      FMMU->nFMMU = ecx_siigetbyte(context, slave, a++);
      FMMU->nFMMU += (ecx_siigetbyte(context, slave, a++) << 8);
      FMMU->nFMMU *= 2;
      FMMU->FMMU0 = ecx_siigetbyte(context, slave, a++);
      FMMU->FMMU1 = ecx_siigetbyte(context, slave, a++);
      if (FMMU->nFMMU > 2)
      {
         FMMU->FMMU2 = ecx_siigetbyte(context, slave, a++);
         FMMU->FMMU3 = ecx_siigetbyte(context, slave, a++);
      }
   }
   if (eectl)
   {
      ecx_eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return FMMU->nFMMU;
}

/** Get SM data from SII SM section in slave EEPROM.
 *  @param[in]  context context struct
 *  @param[in]  slave   slave number
 *  @param[out] SM      first SM struct from SII
 *  @return number of SM's defined in section
 */
uint16 ecx_siiSM(ecx_contextt *context, uint16 slave, ec_eepromSMt *SM)
{
   uint16 a, w;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   SM->nSM = 0;
   SM->Startpos = ecx_siifind(context, slave, ECT_SII_SM);
   if (SM->Startpos > 0)
   {
      a = SM->Startpos;
      w = ecx_siigetbyte(context, slave, a++);
      w += (ecx_siigetbyte(context, slave, a++) << 8);
      SM->nSM = (uint8)(w / 4);
      SM->PhStart = ecx_siigetbyte(context, slave, a++);
      SM->PhStart += (ecx_siigetbyte(context, slave, a++) << 8);
      SM->Plength = ecx_siigetbyte(context, slave, a++);
      SM->Plength += (ecx_siigetbyte(context, slave, a++) << 8);
      SM->Creg = ecx_siigetbyte(context, slave, a++);
      SM->Sreg = ecx_siigetbyte(context, slave, a++);
      SM->Activate = ecx_siigetbyte(context, slave, a++);
      SM->PDIctrl = ecx_siigetbyte(context, slave, a++);
   }
   if (eectl)
   {
      ecx_eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return SM->nSM;
}

/** Get next SM data from SII SM section in slave EEPROM.
 *  @param[in]  context context struct
 *  @param[in]  slave   slave number
 *  @param[out] SM      first SM struct from SII
 *  @param[in]  n       SM number
 *  @return >0 if OK
 */
uint16 ecx_siiSMnext(ecx_contextt *context, uint16 slave, ec_eepromSMt *SM, uint16 n)
{
   uint16 a;
   uint16 retVal = 0;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   if (n < SM->nSM)
   {
      a = SM->Startpos + 2 + (n * 8);
      SM->PhStart = ecx_siigetbyte(context, slave, a++);
      SM->PhStart += (ecx_siigetbyte(context, slave, a++) << 8);
      SM->Plength = ecx_siigetbyte(context, slave, a++);
      SM->Plength += (ecx_siigetbyte(context, slave, a++) << 8);
      SM->Creg = ecx_siigetbyte(context, slave, a++);
      SM->Sreg = ecx_siigetbyte(context, slave, a++);
      SM->Activate = ecx_siigetbyte(context, slave, a++);
      SM->PDIctrl = ecx_siigetbyte(context, slave, a++);
      retVal = 1;
   }
   if (eectl)
   {
      ecx_eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return retVal;
}

/** Get PDO data from SII PDO section in slave EEPROM.
 *  @param[in]  context context struct
 *  @param[in]  slave   slave number
 *  @param[out] PDO     PDO struct from SII
 *  @param[in]  t       0=RXPDO 1=TXPDO
 *  @return mapping size in bits of PDO
 */
uint32 ecx_siiPDO(ecx_contextt *context, uint16 slave, ec_eepromPDOt *PDO, uint8 t)
{
   uint16 a, w, c, e, er, Size;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   Size = 0;
   PDO->nPDO = 0;
   PDO->Length = 0;
   PDO->Index[1] = 0;
   for (c = 0; c < EC_MAXSM; c++)
      PDO->SMbitsize[c] = 0;
   if (t > 1)
      t = 1;
   PDO->Startpos = ecx_siifind(context, slave, ECT_SII_PDO + t);
   if (PDO->Startpos > 0)
   {
      a = PDO->Startpos;
      w = ecx_siigetbyte(context, slave, a++);
      w += (ecx_siigetbyte(context, slave, a++) << 8);
      PDO->Length = w;
      c = 1;
      /* traverse through all PDOs */
      do
      {
         PDO->nPDO++;
         PDO->Index[PDO->nPDO] = ecx_siigetbyte(context, slave, a++);
         PDO->Index[PDO->nPDO] += (ecx_siigetbyte(context, slave, a++) << 8);
         PDO->BitSize[PDO->nPDO] = 0;
         c++;
         e = ecx_siigetbyte(context, slave, a++);
         PDO->SyncM[PDO->nPDO] = ecx_siigetbyte(context, slave, a++);
         a += 4;
         c += 2;
         if (PDO->SyncM[PDO->nPDO] < EC_MAXSM) /* active and in range SM? */
         {
            /* read all entries defined in PDO */
            for (er = 1; er <= e; er++)
            {
               c += 4;
               a += 5;
               PDO->BitSize[PDO->nPDO] += ecx_siigetbyte(context, slave, a++);
               a += 2;
            }
            PDO->SMbitsize[PDO->SyncM[PDO->nPDO]] += PDO->BitSize[PDO->nPDO];
            Size += PDO->BitSize[PDO->nPDO];
            c++;
         }
         else /* PDO deactivated because SM is 0xff or > EC_MAXSM */
         {
            c += 4 * e;
            a += 8 * e;
            c++;
         }
         if (PDO->nPDO >= (EC_MAXEEPDO - 1))
         {
            c = PDO->Length; /* limit number of PDO entries in buffer */
         }
      } while (c < PDO->Length);
   }
   if (eectl)
   {
      ecx_eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return (Size);
}

#define MAX_FPRD_MULTI 64

int ecx_FPRD_multi(ecx_contextt *context, int n, uint16 *configlst, ec_alstatust *slstatlst, int timeout)
{
   int wkc;
   uint8 idx;
   ecx_portt *port;
   uint16 sldatapos[MAX_FPRD_MULTI];
   int slcnt;

   port = &context->port;
   idx = ecx_getindex(port);
   slcnt = 0;
   ecx_setupdatagram(port, &(port->txbuf[idx]), EC_CMD_FPRD, idx,
                     *(configlst + slcnt), ECT_REG_ALSTAT, sizeof(ec_alstatust), slstatlst + slcnt);
   sldatapos[slcnt] = EC_HEADERSIZE;
   while (++slcnt < (n - 1))
   {
      sldatapos[slcnt] = ecx_adddatagram(port, &(port->txbuf[idx]), EC_CMD_FPRD, idx, TRUE,
                                         *(configlst + slcnt), ECT_REG_ALSTAT, sizeof(ec_alstatust), slstatlst + slcnt);
   }
   if (slcnt < n)
   {
      sldatapos[slcnt] = ecx_adddatagram(port, &(port->txbuf[idx]), EC_CMD_FPRD, idx, FALSE,
                                         *(configlst + slcnt), ECT_REG_ALSTAT, sizeof(ec_alstatust), slstatlst + slcnt);
   }
   wkc = ecx_srconfirm(port, idx, timeout);
   if (wkc >= 0)
   {
      for (slcnt = 0; slcnt < n; slcnt++)
      {
         memcpy(slstatlst + slcnt, &(port->rxbuf[idx][sldatapos[slcnt]]), sizeof(ec_alstatust));
      }
   }
   ecx_setbufstat(port, idx, EC_BUF_EMPTY);
   return wkc;
}

/** Read all slave states in slavelist.
 * @warning The BOOT state is actually higher than INIT and PRE_OP (see state representation)
 * @param[in] context context struct
 * @return lowest state found
 */
int ecx_readstate(ecx_contextt *context)
{
   uint16 slave, fslave, lslave, configadr, lowest, rval, bitwisestate;
   ec_alstatust sl[MAX_FPRD_MULTI];
   uint16 slca[MAX_FPRD_MULTI];
   boolean noerrorflag, allslavessamestate;
   boolean allslavespresent = FALSE;
   int wkc;

   /* Try to establish the state of all slaves sending only one broadcast datagram.
    * This way a number of datagrams equal to the number of slaves will be sent only if needed.*/
   rval = 0;
   wkc = ecx_BRD(&context->port, 0, ECT_REG_ALSTAT, sizeof(rval), &rval, EC_TIMEOUTRET);

   if (wkc >= context->slavecount)
   {
      allslavespresent = TRUE;
   }

   rval = etohs(rval);
   bitwisestate = (rval & 0x0f);

   if ((rval & EC_STATE_ERROR) == 0)
   {
      noerrorflag = TRUE;
      context->slavelist[0].ALstatuscode = 0;
   }
   else
   {
      noerrorflag = FALSE;
   }

   switch (bitwisestate)
   {
      /* Note: BOOT State collides with PRE_OP | INIT and cannot be used here */
   case EC_STATE_INIT:
   case EC_STATE_PRE_OP:
   case EC_STATE_SAFE_OP:
   case EC_STATE_OPERATIONAL:
      allslavessamestate = TRUE;
      context->slavelist[0].state = bitwisestate;
      break;
   default:
      allslavessamestate = FALSE;
      break;
   }

   if (noerrorflag && allslavessamestate && allslavespresent)
   {
      /* No slave has toggled the error flag so the alstatuscode
       * (even if different from 0) should be ignored and
       * the slaves have reached the same state so the internal state
       * can be updated without sending any datagram. */
      for (slave = 1; slave <= context->slavecount; slave++)
      {
         context->slavelist[slave].ALstatuscode = 0x0000;
         context->slavelist[slave].state = bitwisestate;
      }
      lowest = bitwisestate;
   }
   else
   {
      /* Not all slaves have the same state or at least one is in error so one datagram per slave
       * is needed. */
      context->slavelist[0].ALstatuscode = 0;
      lowest = 0xff;
      fslave = 1;
      do
      {
         lslave = (uint16)context->slavecount;
         if ((lslave - fslave) >= MAX_FPRD_MULTI)
         {
            lslave = fslave + MAX_FPRD_MULTI - 1;
         }
         for (slave = fslave; slave <= lslave; slave++)
         {
            const ec_alstatust zero = {0, 0, 0};

            configadr = context->slavelist[slave].configadr;
            slca[slave - fslave] = configadr;
            sl[slave - fslave] = zero;
         }
         ecx_FPRD_multi(context, (lslave - fslave) + 1, &(slca[0]), &(sl[0]), EC_TIMEOUTRET3);
         for (slave = fslave; slave <= lslave; slave++)
         {
            configadr = context->slavelist[slave].configadr;
            rval = etohs(sl[slave - fslave].alstatus);
            context->slavelist[slave].ALstatuscode = etohs(sl[slave - fslave].alstatuscode);
            if ((rval & 0xf) < lowest)
            {
               lowest = (rval & 0xf);
            }
            context->slavelist[slave].state = rval;
            context->slavelist[0].ALstatuscode |= context->slavelist[slave].ALstatuscode;
         }
         fslave = lslave + 1;
      } while (lslave < context->slavecount);
      context->slavelist[0].state = lowest;
   }

   return lowest;
}

/** Write slave state, if slave = 0 then write to all slaves.
 * The function does not check if the actual state is changed.
 * @param[in]  context context struct
 * @param[in] slave    Slave number, 0 master
 * @return Workcounter or EC_NOFRAME
 */
int ecx_writestate(ecx_contextt *context, uint16 slave)
{
   int ret;
   uint16 configadr, slstate;

   if (slave == 0)
   {
      slstate = htoes(context->slavelist[slave].state);
      ret = ecx_BWR(&context->port, 0, ECT_REG_ALCTL, sizeof(slstate),
                    &slstate, EC_TIMEOUTRET3);
   }
   else
   {
      configadr = context->slavelist[slave].configadr;

      ret = ecx_FPWRw(&context->port, configadr, ECT_REG_ALCTL,
                      htoes(context->slavelist[slave].state), EC_TIMEOUTRET3);
   }
   return ret;
}

/** Check actual slave state.
 * This is a blocking function.
 * To refresh the state of all slaves ecx_readstate() should be called
 * @warning If this is used for slave 0 (=all slaves), the state of all slaves is read by an bitwise OR operation.
 * The returned value is also the bitwise OR state of all slaves.
 * This has some implications for the BOOT state. The Boot state representation collides with INIT | PRE_OP so this
 * function cannot be used for slave = 0 and reqstate = EC_STATE_BOOT and also, if the returned state is BOOT, some
 * slaves might actually be in INIT and PRE_OP and not in BOOT.
 * @param[in] context     context struct
 * @param[in] slave       Slave number, 0 all slaves (only the "slavelist[0].state" is refreshed)
 * @param[in] reqstate    Requested state
 * @param[in] timeout     Timeout value in us
 * @return Requested state, or found state after timeout.
 */
uint16 ecx_statecheck(ecx_contextt *context, uint16 slave, uint16 reqstate, int timeout)
{
   uint16 configadr, state, rval;
   ec_alstatust slstat;
   osal_timert timer;

   if (slave > context->slavecount)
   {
      return 0;
   }
   osal_timer_start(&timer, timeout);
   configadr = context->slavelist[slave].configadr;
   do
   {
      if (slave < 1)
      {
         rval = 0;
         ecx_BRD(&context->port, 0, ECT_REG_ALSTAT, sizeof(rval), &rval, EC_TIMEOUTRET);
         rval = etohs(rval);
      }
      else
      {
         slstat.alstatus = 0;
         slstat.alstatuscode = 0;
         ecx_FPRD(&context->port, configadr, ECT_REG_ALSTAT, sizeof(slstat), &slstat, EC_TIMEOUTRET);
         rval = etohs(slstat.alstatus);
         context->slavelist[slave].ALstatuscode = etohs(slstat.alstatuscode);
      }
      state = rval & 0x000f; /* read slave status */
      if (state != reqstate)
      {
         osal_usleep(1000);
      }
   } while ((state != reqstate) && (osal_timer_is_expired(&timer) == FALSE));
   context->slavelist[slave].state = rval;

   return state;
}

/** Get index of next mailbox counter value.
 * Used for Mailbox Link Layer.
 * @param[in] cnt     Mailbox counter value [0..7]
 * @return next mailbox counter value
 */
uint8 ec_nextmbxcnt(uint8 cnt)
{
   cnt++;
   if (cnt > 7)
   {
      cnt = 1; /* wrap around to 1, not 0 */
   }

   return cnt;
}

/** Clear mailbox buffer.
 * @param[out] Mbx     Mailbox buffer to clear
 */
void ec_clearmbx(ec_mbxbuft *Mbx)
{
   if (Mbx)
      memset(Mbx, 0x00, EC_MAXMBX);
}

/** Clear mailbox status for a specific group.
 * @param[in] context context struct
 * @param[in] group   Group number
 * @return 1 if successfully cleared, 0 otherwise
 */
int ecx_clearmbxstatus(ecx_contextt *context, uint8 group)
{
   if (context->grouplist[group].mbxstatus && context->grouplist[group].mbxstatuslength)
   {
      memset(context->grouplist[group].mbxstatus, 0x00, context->grouplist[group].mbxstatuslength);
      return 1;
   }
   return 0;
}

/**
 * Read mailbox status from slave.
 *
 * @param[in]  context  context struct
 * @param[in]  slave    Slave number
 * @param[out] SMstat   Pointer to store mailbox status
 * @return Workcounter or EC_NOFRAME
 */
int ecx_readmbxstatus(ecx_contextt *context, uint16 slave, uint8 *SMstat)
{
   int wkc = 0;
   if (context->slavelist[slave].mbxhandlerstate == ECT_MBXH_CYCLIC)
   {
      *SMstat = *(context->slavelist[slave].mbxstatus);
      wkc = 1;
   }
   else
   {
      uint16 configadr = context->slavelist[slave].configadr;
      wkc = ecx_FPRD(&context->port, configadr, ECT_REG_SM1STAT, sizeof(uint8), SMstat, EC_TIMEOUTRET);
   }
   return wkc;
}

/**
 * Read extended mailbox status for a specified slave.
 * @param[in]  context        context struct
 * @param[in]  slave          Slave number
 * @param[out] SMstatex       Pointer to store extended mailbox status
 * @return Workcounter from slave response
 */
int ecx_readmbxstatusex(ecx_contextt *context, uint16 slave, uint16 *SMstatex)
{
   uint16 hu16;
   uint16 configadr = context->slavelist[slave].configadr;
   int wkc = ecx_FPRD(&context->port, configadr, ECT_REG_SM1STAT, sizeof(hu16), &hu16, EC_TIMEOUTRET);
   *SMstatex = etohs(hu16);
   return wkc;
}

/** Check if IN mailbox of slave is empty.
 * @param[in] context  context struct
 * @param[in] slave    Slave number
 * @param[in] timeout  Timeout in us
 * @return >0 is success
 */
int ecx_mbxempty(ecx_contextt *context, uint16 slave, int timeout)
{
   uint16 configadr;
   uint8 SMstat;
   int wkc;
   osal_timert timer;

   osal_timer_start(&timer, timeout);
   configadr = context->slavelist[slave].configadr;
   do
   {
      SMstat = 0;
      wkc = ecx_FPRD(&context->port, configadr, ECT_REG_SM0STAT, sizeof(SMstat), &SMstat, EC_TIMEOUTRET);
      SMstat = etohs(SMstat);
      if (((SMstat & 0x08) != 0) && (timeout > EC_LOCALDELAY))
      {
         osal_usleep(EC_LOCALDELAY);
      }
   } while (((wkc <= 0) || ((SMstat & 0x08) != 0)) && (osal_timer_is_expired(&timer) == FALSE));

   if ((wkc > 0) && ((SMstat & 0x08) == 0))
   {
      return 1;
   }

   return 0;
}

/**
 * Handles incoming mailbox messages for a specified group.
 *
 * This function processes mailbox messages for the given group. It
 * checks if slaves have messages in their mailboxes and handles them
 * according to their type (CoE, SoE, EoE, etc.).  Also manages the
 * robust mailbox protocol state machine for error handling. It keeps
 * track of a work limit to prevent excessive processing in a single
 * call.
 *
 * @param[in]  context  context struct
 * @param[in]  group    group number
 * @param[in]  limit    maximum number of mailbox operations to process
 * @return Number of mailbox operations processed
 */
int ecx_mbxinhandler(ecx_contextt *context, uint8 group, int limit)
{
   int cnt, cntoffset, wkc, wkc2, limitcnt;
   int maxcnt = context->grouplist[group].mbxstatuslength;
   ec_mbxbuft *mbx;
   ec_mbxheadert *mbxh;
   ec_emcyt *EMp;
   ec_mbxerrort *MBXEp;
   uint8 SMcontr;
   uint16 SMstatex;

   limitcnt = 0;
   int firstmbxpos = context->grouplist[group].lastmbxpos + 1;
   int maxcntstored = maxcnt;
   /* iterate over all possible mailbox slaves */
   for (cnt = 0; cnt < maxcnt; cnt++)
   {
      /* start from last stored slave position to allow fair handling under load */
      cntoffset = firstmbxpos + cnt;
      if (cntoffset >= maxcntstored) cntoffset -= maxcntstored;
      context->grouplist[group].lastmbxpos = cntoffset;
      uint16 slave = context->grouplist[group].mbxstatuslookup[cntoffset];
      ec_slavet *slaveitem = &context->slavelist[slave];
      uint16 configadr = slaveitem->configadr;
      /* cyclic handler enabled for this slave */
      if (slaveitem->mbxhandlerstate == ECT_MBXH_CYCLIC)
      {
         /* handle robust mailbox protocol state machine */
         if (slaveitem->mbxrmpstate)
         {
            if (slaveitem->islost)
            {
               slaveitem->mbxrmpstate = 0;
            }
            else
            {
               switch (slaveitem->mbxrmpstate)
               {
               case 1:
                  if (ecx_readmbxstatusex(context, slave, &(slaveitem->mbxinstateex)) > 0)
                  {
                     slaveitem->mbxinstateex ^= 0x0200; /* toggle repeat request */
                     slaveitem->mbxrmpstate++;
                  }
                  break;
               case 2:
                  SMstatex = htoes(slaveitem->mbxinstateex);
                  if (ecx_FPWR(&context->port, configadr, ECT_REG_SM1STAT, sizeof(SMstatex), &(slaveitem->mbxinstateex), EC_TIMEOUTRET) > 0)
                  {
                     slaveitem->mbxrmpstate++;
                  }
                  break;
               case 3:
                  /* wait for repeat ack */
                  SMstatex = htoes(slaveitem->mbxinstateex);
                  wkc2 = ecx_FPRD(&context->port, configadr, ECT_REG_SM1CONTR, sizeof(SMcontr), &SMcontr, EC_TIMEOUTRET);
                  if ((wkc2 > 0) && ((SMcontr & 0x02) == (HI_BYTE(SMstatex) & 0x02)))
                  {
                     slaveitem->mbxrmpstate = 0;
                  }
                  break;
               }
               /* keep track of work limit */
               if (++limitcnt >= limit) maxcnt = 0;
            }
         }
         /* mbxin full detected */
         else if ((*(context->grouplist[group].mbxstatus + cntoffset) & 0x08) > 0)
         {
            uint16 mbxl = slaveitem->mbx_rl;
            uint16 mbxro = slaveitem->mbx_ro;
            if ((mbxl > 0) && (mbx = ecx_getmbx(context)))
            {
               /* keep track of work limit */
               if (++limitcnt >= limit) maxcnt = 0;
               wkc = ecx_FPRD(&context->port, configadr, mbxro, mbxl, mbx, EC_TIMEOUTRET); /* get mailbox */
               if (wkc > 0)
               {
                  mbxh = (ec_mbxheadert *)mbx;
                  if ((mbxh->mbxtype & 0x0f) == ECT_MBXT_ERR) /* Mailbox error response? */
                  {
                     MBXEp = (ec_mbxerrort *)mbx;
                     ecx_mbxerror(context, slave, etohs(MBXEp->Detail));
                  }
                  else if ((mbxh->mbxtype & 0x0f) == ECT_MBXT_COE) /* CoE response? */
                  {
                     EMp = (ec_emcyt *)mbx;
                     if ((etohs(EMp->CANOpen) >> 12) == 0x01) /* Emergency request? */
                     {
                        ecx_mbxemergencyerror(context, slave, etohs(EMp->ErrorCode), EMp->ErrorReg,
                                              EMp->bData, etohs(EMp->w1), etohs(EMp->w2));
                     }
                     else
                     {
                        if (slaveitem->coembxin && (slaveitem->coembxinfull == FALSE))
                        {
                           slaveitem->coembxin = (uint8 *)mbx;
                           mbx = NULL;
                           slaveitem->coembxinfull = TRUE;
                        }
                        else
                        {
                           slaveitem->coembxoverrun++;
                        }
                     }
                  }
                  else if ((mbxh->mbxtype & 0x0f) == ECT_MBXT_SOE) /* SoE response? */
                  {
                     if (slaveitem->soembxin && (slaveitem->soembxinfull == FALSE))
                     {
                        slaveitem->soembxin = (uint8 *)mbx;
                        mbx = NULL;
                        slaveitem->soembxinfull = TRUE;
                     }
                     else
                     {
                        slaveitem->soembxoverrun++;
                     }
                  }
                  else if ((mbxh->mbxtype & 0x0f) == ECT_MBXT_EOE) /* EoE response? */
                  {
                     ec_EOEt *eoembx = (ec_EOEt *)mbx;
                     uint16 frameinfo1 = etohs(eoembx->frameinfo1);
                     /* All non fragment data frame types are expected to be handled by
                      * slave send/receive API if the EoE hook is set
                      */
                     if (EOE_HDR_FRAME_TYPE_GET(frameinfo1) == EOE_FRAG_DATA)
                     {
                        if (context->EOEhook)
                        {
                           if (context->EOEhook(context, slave, eoembx) > 0)
                           {
                              /* Fragment handled by EoE hook */
                              wkc = 0;
                           }
                        }
                     }
                     /* Not handled by hook */
                     if ((wkc > 0) && slaveitem->eoembxin && (slaveitem->eoembxinfull == FALSE))
                     {
                        slaveitem->eoembxin = (uint8 *)mbx;
                        mbx = NULL;
                        slaveitem->eoembxinfull = TRUE;
                     }
                     else
                     {
                        slaveitem->eoembxoverrun++;
                     }
                  }
                  else if ((mbxh->mbxtype & 0x0f) == ECT_MBXT_FOE) /* FoE response? */
                  {
                     if (slaveitem->foembxin && (slaveitem->foembxinfull == FALSE))
                     {
                        slaveitem->foembxin = (uint8 *)mbx;
                        mbx = NULL;
                        slaveitem->foembxinfull = TRUE;
                     }
                     else
                     {
                        slaveitem->foembxoverrun++;
                     }
                  }
                  else if ((mbxh->mbxtype & 0x0f) == ECT_MBXT_VOE) /* VoE response? */
                  {
                     if (slaveitem->voembxin && (slaveitem->voembxinfull == FALSE))
                     {
                        slaveitem->voembxin = (uint8 *)mbx;
                        mbx = NULL;
                        slaveitem->voembxinfull = TRUE;
                     }
                     else
                     {
                        slaveitem->voembxoverrun++;
                     }
                  }
                  else if ((mbxh->mbxtype & 0x0f) == ECT_MBXT_AOE) /* AoE response? */
                  {
                     if (slaveitem->aoembxin && (slaveitem->aoembxinfull == FALSE))
                     {
                        slaveitem->aoembxin = (uint8 *)mbx;
                        mbx = NULL;
                        slaveitem->aoembxinfull = TRUE;
                     }
                     else
                     {
                        slaveitem->aoembxoverrun++;
                     }
                  }
               }
               else
               {
                  /* mailbox lost, initiate robust mailbox protocol */
                  slaveitem->mbxrmpstate = 1;
               }
               /* release mailbox to pool if still owner */
               if (mbx)
               {
                  ecx_dropmbx(context, mbx);
               }
            }
         }
      }
   }
   return limitcnt;
}

/**
 * Handles outgoing mailbox messages for a specified group.
 *
 * This function processes outgoing mailbox messages for the given group,
 * checking the state of each message in the queue and sending appropriate
 * requests to the slaves. It supports retrying for failed requests.
 *
 * @param[in] context context struct
 * @param[in] group   group number
 * @param[in] limit   maximum number of mailboxes to process
 * @return Number of processed mailboxes
 */
int ecx_mbxouthandler(ecx_contextt *context, uint8 group, int limit)
{
   int wkc;
   int limitcnt = 0;
   int ticketloc, state;
   uint16 slave, mbxl, mbxwo, configadr;
   ec_mbxbuft *mbx;
   ec_mbxqueuet *mbxqueue = &(context->grouplist[group].mbxtxqueue);
   int listcount = mbxqueue->listcount;
   while ((limitcnt <= limit) && listcount)
   {
      listcount--;
      ticketloc = mbxqueue->listtail;
      state = mbxqueue->mbxstate[ticketloc];
      switch (state)
      {
      case EC_MBXQUEUESTATE_REQ:
      case EC_MBXQUEUESTATE_FAIL:
         slave = mbxqueue->mbxslave[ticketloc];
         mbx = mbxqueue->mbx[ticketloc];
         mbxl = context->slavelist[slave].mbx_l;
         configadr = context->slavelist[slave].configadr;
         mbxwo = context->slavelist[slave].mbx_wo;
         limitcnt++;
         if (context->slavelist[slave].state >= EC_STATE_PRE_OP)
         {
            /* write slave in mailbox 1st try*/
            wkc = ecx_FPWR(&context->port, configadr, mbxwo, mbxl, mbx, EC_TIMEOUTRET);
            if (wkc > 0)
            {
               mbxqueue->mbxstate[ticketloc] = EC_MBXQUEUESTATE_DONE; // mbx tx ok
               ecx_dropmbx(context, mbx);
               mbxqueue->mbx[ticketloc] = NULL;
            }
            else
            {
               if (state != EC_MBXQUEUESTATE_FAIL)
                  mbxqueue->mbxstate[ticketloc] = EC_MBXQUEUESTATE_FAIL; // mbx tx fail, retry
            }
         }
         /* fall through */
      case EC_MBXQUEUESTATE_DONE: // mbx tx ok
         ecx_mbxrotatequeue(context, group, ticketloc);
         break;
      }
      if (mbxqueue->mbxremove[ticketloc])
      {
         mbx = ecx_mbxdropqueue(context, group, ticketloc);
         if (mbx) ecx_dropmbx(context, mbx);
      }
   }
   return limitcnt;
}

/**
 * Combined handler for both incoming and outgoing mailbox messages.
 *
 * This function processes both incoming and outgoing mailbox messages
 * for the specified group, dividing the processing limit between them.
 * It first handles incoming messages and then uses the remaining limit
 * for outgoing messages.
 *
 * @param[in] context context tructure.
 * @param[in] group Group number.
 * @param[in] limit The maximum number of mailboxes to process.
 *
 * @return count of processed outgoing mailboxes.
 */
int ecx_mbxhandler(ecx_contextt *context, uint8 group, int limit)
{
   int limitcnt;
   limitcnt = ecx_mbxinhandler(context, group, limit);
   return ecx_mbxouthandler(context, group, (limit - limitcnt));
}

/** Write IN mailbox to slave.
 * Mailbox is fetched from pool by caller, ownership is transferred and dropped back to pool automatically.
 * @param[in]  context    context struct
 * @param[in]  slave      Slave number
 * @param[out] mbx        Pointer to mailbox data
 * @param[in]  timeout    Timeout in us
 * @return Work counter (>0 is success)
 */
int ecx_mbxsend(ecx_contextt *context, uint16 slave, ec_mbxbuft *mbx, int timeout)
{
   uint16 mbxwo, mbxl, configadr;
   int wkc, ticket;
   osal_timert timer;
   ec_slavet *slavelist = &(context->slavelist[slave]);

   wkc = 0;
   configadr = slavelist->configadr;
   mbxl = slavelist->mbx_l;
   if (slavelist->mbxhandlerstate == ECT_MBXH_CYCLIC)
   {
      osal_timer_start(&timer, timeout);
      wkc = 0;
      if (mbxl > 0)
      {
         do
         {
            if ((ticket = ecx_mbxaddqueue(context, slave, mbx)) >= 0)
            {
               mbx = NULL;
               do
               {
                  wkc = ecx_mbxdonequeue(context, slave, ticket);
                  if (!wkc && (timeout > EC_LOCALDELAY))
                  {
                     osal_usleep(EC_LOCALDELAY);
                  }
               } while ((wkc <= 0) && (osal_timer_is_expired(&timer) == FALSE));
               if (wkc <= 0)
               {
                  if (!ecx_mbxexpirequeue(context, slave, ticket))
                  {
                     EC_PRINT("expirequeue failed\n\r");
                  }
               }
            }
            else if ((timeout > EC_LOCALDELAY))
            {
               osal_usleep(EC_LOCALDELAY);
            }
         } while ((wkc <= 0) && (osal_timer_is_expired(&timer) == FALSE));
      }
   }
   else if ((mbxl > 0) && (mbxl <= EC_MAXMBX) && (slavelist->state >= EC_STATE_PRE_OP))
   {
      mbxwo = context->slavelist[slave].mbx_wo;
      /* write slave in mailbox 1st try*/
      wkc = ecx_FPWR(&context->port, configadr, mbxwo, mbxl, mbx, EC_TIMEOUTRET3);
      /* if failed wait for empty mailbox */
      if ((wkc <= 0) && ecx_mbxempty(context, slave, timeout))
      {
         /* retry */
         wkc = ecx_FPWR(&context->port, configadr, mbxwo, mbxl, mbx, EC_TIMEOUTRET3);
      }
      if (wkc < 0) wkc = 0;
   }
   if (mbx) ecx_dropmbx(context, mbx);
   return wkc;
}

/** Read OUT mailbox from slave.
 * Supports Mailbox Link Layer with repeat requests.
 * Mailbox is fetched from pool, caller is owner after return
 * and therefore should drop it back to the pool when finished.
 * @param[in]  context    context struct
 * @param[in]  slave      Slave number
 * @param[out] mbx        Double pointer to mailbox data
 * @param[in]  timeout    Timeout in us
 * @return Work counter (>0 is success)
 */
int ecx_mbxreceive(ecx_contextt *context, uint16 slave, ec_mbxbuft **mbx, int timeout)
{
   uint16 mbxro, mbxl, configadr;
   int wkc = 0;
   int wkc2;
   uint8 SMstat;
   uint16 SMstatex;
   uint8 SMcontr;
   ec_mbxbuft *mbxin;
   ec_mbxheadert *mbxh;
   ec_emcyt *EMp;
   ec_mbxerrort *MBXEp;
   osal_timert timer;
   ec_slavet *slavelist = &(context->slavelist[slave]);

   configadr = slavelist->configadr;
   mbxl = slavelist->mbx_rl;
   if (slavelist->mbxhandlerstate == ECT_MBXH_CYCLIC)
   {
      osal_timer_start(&timer, timeout);
      wkc = 0;
      do
      {
         if (slavelist->coembxinfull == TRUE)
         {
            *mbx = (ec_mbxbuft *)slavelist->coembxin;
            slavelist->coembxin = EC_MBXINENABLE;
            slavelist->coembxinfull = FALSE;
            wkc = 1;
         }
         else if (slavelist->soembxinfull == TRUE)
         {
            *mbx = (ec_mbxbuft *)slavelist->soembxin;
            slavelist->soembxin = EC_MBXINENABLE;
            slavelist->soembxinfull = FALSE;
            wkc = 1;
         }
         else if (slavelist->foembxinfull == TRUE)
         {
            *mbx = (ec_mbxbuft *)slavelist->foembxin;
            slavelist->foembxin = EC_MBXINENABLE;
            slavelist->foembxinfull = FALSE;
            wkc = 1;
         }
         else if (slavelist->eoembxinfull == TRUE)
         {
            *mbx = (ec_mbxbuft *)slavelist->eoembxin;
            slavelist->eoembxin = EC_MBXINENABLE;
            slavelist->eoembxinfull = FALSE;
            wkc = 1;
         }
         if (!wkc && (timeout > EC_LOCALDELAY))
         {
            osal_usleep(EC_LOCALDELAY);
         }
      } while ((wkc <= 0) && (osal_timer_is_expired(&timer) == FALSE));
   }
   else if ((mbxl > 0) && (mbxl <= EC_MAXMBX))
   {
      osal_timer_start(&timer, timeout);
      wkc = 0;
      do /* wait for read mailbox available */
      {
         SMstat = 0;
         wkc = ecx_readmbxstatus(context, slave, &SMstat);
         if (((SMstat & 0x08) == 0) && (timeout > EC_LOCALDELAY))
         {
            osal_usleep(EC_LOCALDELAY);
         }
      } while (((wkc <= 0) || ((SMstat & 0x08) == 0)) && (osal_timer_is_expired(&timer) == FALSE));

      if ((wkc > 0) && ((SMstat & 0x08) > 0)) /* read mailbox available ? */
      {
         mbxro = slavelist->mbx_ro;
         mbxin = ecx_getmbx(context);
         mbxh = (ec_mbxheadert *)mbxin;
         do
         {
            wkc = ecx_FPRD(&context->port, configadr, mbxro, mbxl, mbxin, EC_TIMEOUTRET); /* get mailbox */
            if ((wkc > 0) && ((mbxh->mbxtype & 0x0f) == 0x00))                            /* Mailbox error response? */
            {
               MBXEp = (ec_mbxerrort *)mbxin;
               ecx_mbxerror(context, slave, etohs(MBXEp->Detail));
               ecx_dropmbx(context, mbxin);
               mbxin = NULL;
               wkc = 0; /* prevent emergency to cascade up, it is already handled. */
            }
            else if ((wkc > 0) && ((mbxh->mbxtype & 0x0f) == ECT_MBXT_COE)) /* CoE response? */
            {
               EMp = (ec_emcyt *)mbx;
               if ((etohs(EMp->CANOpen) >> 12) == 0x01) /* Emergency request? */
               {
                  ecx_mbxemergencyerror(context, slave, etohs(EMp->ErrorCode), EMp->ErrorReg,
                                        EMp->bData, etohs(EMp->w1), etohs(EMp->w2));
                  ecx_dropmbx(context, mbxin);
                  mbxin = NULL;
                  wkc = 0; /* prevent emergency to cascade up, it is already handled. */
               }
               else
               {
                  *mbx = mbxin;
                  mbxin = NULL;
               }
            }
            else if ((wkc > 0) && ((mbxh->mbxtype & 0x0f) == ECT_MBXT_EOE)) /* EoE response? */
            {
               ec_EOEt *eoembx = (ec_EOEt *)mbx;
               uint16 frameinfo1 = etohs(eoembx->frameinfo1);
               /* All non fragment data frame types are expected to be handled by
                * slave send/receive API if the EoE hook is set
                */
               if (EOE_HDR_FRAME_TYPE_GET(frameinfo1) == EOE_FRAG_DATA)
               {
                  if (context->EOEhook)
                  {
                     if (context->EOEhook(context, slave, eoembx) > 0)
                     {
                        /* Fragment handled by EoE hook */
                        ecx_dropmbx(context, mbxin);
                        mbxin = NULL;
                        wkc = 0;
                     }
                  }
               }
               /* Not handled by EoE hook*/
               if (wkc > 0)
               {
                  *mbx = mbxin;
                  mbxin = NULL;
               }
            }
            else if (wkc > 0)
            {
               *mbx = mbxin;
               mbxin = NULL;
            }
            else /* read mailbox lost */
            {
               do /* read extended mailbox status */
               {
                  wkc2 = ecx_readmbxstatusex(context, slave, &SMstatex);
               } while ((wkc2 <= 0) && (osal_timer_is_expired(&timer) == FALSE));
               SMstatex ^= 0x0200; /* toggle repeat request */
               SMstatex = htoes(SMstatex);
               wkc2 = ecx_FPWR(&context->port, configadr, ECT_REG_SM1STAT, sizeof(SMstatex), &SMstatex, EC_TIMEOUTRET);
               SMstatex = etohs(SMstatex);
               do /* wait for toggle ack */
               {
                  wkc2 = ecx_FPRD(&context->port, configadr, ECT_REG_SM1CONTR, sizeof(SMcontr), &SMcontr, EC_TIMEOUTRET);
               } while (((wkc2 <= 0) || ((SMcontr & 0x02) != (HI_BYTE(SMstatex) & 0x02))) && (osal_timer_is_expired(&timer) == FALSE));
               do /* wait for read mailbox available */
               {
                  wkc2 = ecx_readmbxstatusex(context, slave, &SMstatex);
                  if (((SMstatex & 0x08) == 0) && (timeout > EC_LOCALDELAY))
                  {
                     osal_usleep(EC_LOCALDELAY);
                  }
               } while (((wkc2 <= 0) || ((SMstatex & 0x08) == 0)) && (osal_timer_is_expired(&timer) == FALSE));
            }
         } while ((wkc <= 0) && (osal_timer_is_expired(&timer) == FALSE)); /* if WKC<=0 repeat */
         if (mbxin) ecx_dropmbx(context, mbxin);
      }
      else /* no read mailbox available */
      {
         if (wkc > 0)
            wkc = EC_TIMEOUT;
      }
   }

   return wkc;
}

/** Send ENI mailbox protocol initcmds to a slave for a given transition.
 * Currently, only CoE commands are supported.
 * @param[in]  context    context struct
 * @param[in]  slave      Slave number
 * @param[in]  transition transition (ECT_ESMTRANS_*) for which to send commands
 * @return 1 on success, 0 on failure
 */
int ecx_mbxENIinitcmds(ecx_contextt *context, uint16 slave, uint16_t transition)
{
   int slavecount = (context->ENI ? context->ENI->slavecount : 0);

   if (slavecount > 0)
   {
      int i;
      ec_enislavet *eni_slave = context->ENI->slave;

      for (i = 0; i < (slavecount - 1); ++i, ++eni_slave)
      {
         if (slave <= eni_slave->Slave)
         {
            break;
         }
      }
      if (slave == eni_slave->Slave)
      {
         if ((eni_slave->VendorId == context->slavelist[slave].eep_man) &&
             (eni_slave->ProductCode == context->slavelist[slave].eep_id) &&
             (eni_slave->RevisionNo == context->slavelist[slave].eep_rev))
         {
            int wkc;

            EC_PRINT("Apply ENI config for slave %d\n", slave);

            if (context->slavelist[slave].mbx_proto & ECT_MBXPROT_COE)
            {
               ec_enicoecmdt *cmd = eni_slave->CoECmds;
               for (i = 0; i < eni_slave->CoECmdCount; ++i, ++cmd)
               {
                  if (cmd->Transition & transition)
                  {
                     if (cmd->Ccs == 2)
                     {
                        wkc = ecx_SDOwrite(context, slave, cmd->Index, cmd->SubIdx,
                                           cmd->CA, cmd->DataSize, cmd->Data, cmd->Timeout);
                        if (wkc < 1)
                        {
                           return 0;
                        }
                     }
                     else if (cmd->Ccs == 1)
                     {
                        int size = cmd->DataSize;

                        wkc = ecx_SDOread(context, slave, cmd->Index, cmd->SubIdx,
                                          cmd->CA, &size, cmd->Data, cmd->Timeout);
                        if (wkc < 1)
                        {
                           return 0;
                        }
                     }
                  }
               }
            }
         }
      }
   }
   return 1;
}

/** Dump complete EEPROM data from slave in buffer.
 * @param[in]  context  context struct
 * @param[in]  slave    Slave number
 * @param[out] esibuf   EEPROM data buffer, make sure it is big enough.
 */
void ecx_esidump(ecx_contextt *context, uint16 slave, uint8 *esibuf)
{
   uint16 configadr, address, incr;
   uint64 *p64;
   uint16 *p16;
   uint64 edat;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   ecx_eeprom2master(context, slave); /* set eeprom control to master */
   configadr = context->slavelist[slave].configadr;
   address = ECT_SII_START;
   p16 = (uint16 *)esibuf;
   if (context->slavelist[slave].eep_8byte)
   {
      incr = 4;
   }
   else
   {
      incr = 2;
   }
   do
   {
      edat = ecx_readeepromFP(context, configadr, address, EC_TIMEOUTEEP);
      p64 = (uint64 *)p16;
      *p64 = edat;
      p16 += incr;
      address += incr;
   } while ((address <= (EC_MAXEEPBUF >> 1)) && ((uint32)edat != 0xffffffff));

   if (eectl)
   {
      ecx_eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }
}

/** Read EEPROM from slave bypassing cache.
 * @param[in] context   context struct
 * @param[in] slave     Slave number
 * @param[in] eeproma   (WORD) Address in the EEPROM
 * @param[in] timeout   Timeout in us.
 * @return EEPROM data 32bit
 */
uint32 ecx_readeeprom(ecx_contextt *context, uint16 slave, uint16 eeproma, int timeout)
{
   uint16 configadr;

   ecx_eeprom2master(context, slave); /* set eeprom control to master */
   configadr = context->slavelist[slave].configadr;

   return ((uint32)ecx_readeepromFP(context, configadr, eeproma, timeout));
}

/** Write EEPROM to slave bypassing cache.
 * @param[in] context   context struct
 * @param[in] slave     Slave number
 * @param[in] eeproma   (WORD) Address in the EEPROM
 * @param[in] data      16bit data
 * @param[in] timeout   Timeout in us.
 * @return >0 if OK
 */
int ecx_writeeeprom(ecx_contextt *context, uint16 slave, uint16 eeproma, uint16 data, int timeout)
{
   uint16 configadr;

   ecx_eeprom2master(context, slave); /* set eeprom control to master */
   configadr = context->slavelist[slave].configadr;
   return (ecx_writeeepromFP(context, configadr, eeproma, data, timeout));
}

/** Set eeprom control to master. Only if set to PDI.
 * @param[in] context   context struct
 * @param[in] slave     Slave number
 * @return >0 if OK
 */
int ecx_eeprom2master(ecx_contextt *context, uint16 slave)
{
   int wkc = 1, cnt = 0;
   uint16 configadr;
   uint8 eepctl;

   if (context->slavelist[slave].eep_pdi)
   {
      configadr = context->slavelist[slave].configadr;
      eepctl = 2;
      do
      {
         wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl, EC_TIMEOUTRET); /* force Eeprom from PDI */
      } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
      eepctl = 0;
      cnt = 0;
      do
      {
         wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl, EC_TIMEOUTRET); /* set Eeprom to master */
      } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
      context->slavelist[slave].eep_pdi = 0;
   }

   return wkc;
}

/** Set eeprom control to PDI. Only if set to master.
 * @param[in]  context  context struct
 * @param[in] slave     Slave number
 * @return >0 if OK
 */
int ecx_eeprom2pdi(ecx_contextt *context, uint16 slave)
{
   int wkc = 1, cnt = 0;
   uint16 configadr;
   uint8 eepctl;

   if (!context->slavelist[slave].eep_pdi)
   {
      configadr = context->slavelist[slave].configadr;
      eepctl = 1;
      do
      {
         wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl, EC_TIMEOUTRET); /* set Eeprom to PDI */
      } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
      context->slavelist[slave].eep_pdi = 1;
   }

   return wkc;
}

uint16 ecx_eeprom_waitnotbusyAP(ecx_contextt *context, uint16 aiadr, uint16 *estat, int timeout)
{
   int wkc, cnt = 0;
   uint16 retval = 0;
   osal_timert timer;

   osal_timer_start(&timer, timeout);
   do
   {
      if (cnt++)
      {
         osal_usleep(EC_LOCALDELAY);
      }
      *estat = 0;
      wkc = ecx_APRD(&context->port, aiadr, ECT_REG_EEPSTAT, sizeof(*estat), estat, EC_TIMEOUTRET);
      *estat = etohs(*estat);
   } while (((wkc <= 0) || ((*estat & EC_ESTAT_BUSY) > 0)) && (osal_timer_is_expired(&timer) == FALSE)); /* wait for eeprom ready */
   if ((*estat & EC_ESTAT_BUSY) == 0)
   {
      retval = 1;
   }

   return retval;
}

/** Read EEPROM from slave bypassing cache. APRD method.
 * @param[in] context     context struct
 * @param[in] aiadr       auto increment address of slave
 * @param[in] eeproma     (WORD) Address in the EEPROM
 * @param[in] timeout     Timeout in us.
 * @return EEPROM data 64bit or 32bit
 */
uint64 ecx_readeepromAP(ecx_contextt *context, uint16 aiadr, uint16 eeproma, int timeout)
{
   uint16 estat;
   uint32 edat32;
   uint64 edat64;
   ec_eepromt ed;
   int wkc, cnt, nackcnt = 0;

   edat64 = 0;
   edat32 = 0;
   if (ecx_eeprom_waitnotbusyAP(context, aiadr, &estat, timeout))
   {
      if (estat & EC_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(EC_ECMD_NOP); /* clear error bits */
         wkc = ecx_APWR(&context->port, aiadr, ECT_REG_EEPCTL, sizeof(estat), &estat, EC_TIMEOUTRET3);
      }

      do
      {
         ed.comm = htoes(EC_ECMD_READ);
         ed.addr = htoes(eeproma);
         ed.d2 = 0x0000;
         cnt = 0;
         do
         {
            wkc = ecx_APWR(&context->port, aiadr, ECT_REG_EEPCTL, sizeof(ed), &ed, EC_TIMEOUTRET);
         } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
         if (wkc)
         {
            osal_usleep(EC_LOCALDELAY);
            estat = 0x0000;
            if (ecx_eeprom_waitnotbusyAP(context, aiadr, &estat, timeout))
            {
               if (estat & EC_ESTAT_NACK)
               {
                  nackcnt++;
                  osal_usleep(EC_LOCALDELAY * 5);
               }
               else
               {
                  nackcnt = 0;
                  if (estat & EC_ESTAT_R64)
                  {
                     cnt = 0;
                     do
                     {
                        wkc = ecx_APRD(&context->port, aiadr, ECT_REG_EEPDAT, sizeof(edat64), &edat64, EC_TIMEOUTRET);
                     } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
                  }
                  else
                  {
                     cnt = 0;
                     do
                     {
                        wkc = ecx_APRD(&context->port, aiadr, ECT_REG_EEPDAT, sizeof(edat32), &edat32, EC_TIMEOUTRET);
                     } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
                     edat64 = (uint64)edat32;
                  }
               }
            }
         }
      } while ((nackcnt > 0) && (nackcnt < 3));
   }

   return edat64;
}

/** Write EEPROM to slave bypassing cache. APWR method.
 * @param[in] context   context struct
 * @param[in] aiadr     configured address of slave
 * @param[in] eeproma   (WORD) Address in the EEPROM
 * @param[in] data      16bit data
 * @param[in] timeout   Timeout in us.
 * @return >0 if OK
 */
int ecx_writeeepromAP(ecx_contextt *context, uint16 aiadr, uint16 eeproma, uint16 data, int timeout)
{
   uint16 estat;
   ec_eepromt ed;
   int wkc, rval = 0, cnt = 0, nackcnt = 0;

   if (ecx_eeprom_waitnotbusyAP(context, aiadr, &estat, timeout))
   {
      if (estat & EC_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(EC_ECMD_NOP); /* clear error bits */
         wkc = ecx_APWR(&context->port, aiadr, ECT_REG_EEPCTL, sizeof(estat), &estat, EC_TIMEOUTRET3);
      }
      do
      {
         cnt = 0;
         do
         {
            wkc = ecx_APWR(&context->port, aiadr, ECT_REG_EEPDAT, sizeof(data), &data, EC_TIMEOUTRET);
         } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));

         ed.comm = EC_ECMD_WRITE;
         ed.addr = eeproma;
         ed.d2 = 0x0000;
         cnt = 0;
         do
         {
            wkc = ecx_APWR(&context->port, aiadr, ECT_REG_EEPCTL, sizeof(ed), &ed, EC_TIMEOUTRET);
         } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
         if (wkc)
         {
            osal_usleep(EC_LOCALDELAY * 2);
            estat = 0x0000;
            if (ecx_eeprom_waitnotbusyAP(context, aiadr, &estat, timeout))
            {
               if (estat & EC_ESTAT_NACK)
               {
                  nackcnt++;
                  osal_usleep(EC_LOCALDELAY * 25);
               }
               else
               {
                  nackcnt = 0;
                  rval = 1;
               }
            }
         }

      } while ((nackcnt > 0) && (nackcnt < 3));
   }

   return rval;
}

uint16 ecx_eeprom_waitnotbusyFP(ecx_contextt *context, uint16 configadr, uint16 *estat, int timeout)
{
   int wkc, cnt = 0;
   uint16 retval = 0;
   osal_timert timer;

   osal_timer_start(&timer, timeout);
   do
   {
      if (cnt++)
      {
         osal_usleep(EC_LOCALDELAY);
      }
      *estat = 0;
      wkc = ecx_FPRD(&context->port, configadr, ECT_REG_EEPSTAT, sizeof(*estat), estat, EC_TIMEOUTRET);
      *estat = etohs(*estat);
   } while (((wkc <= 0) || ((*estat & EC_ESTAT_BUSY) > 0)) && (osal_timer_is_expired(&timer) == FALSE)); /* wait for eeprom ready */
   if ((*estat & EC_ESTAT_BUSY) == 0)
   {
      retval = 1;
   }

   return retval;
}

/** Read EEPROM from slave bypassing cache. FPRD method.
 * @param[in] context     context struct
 * @param[in] configadr   configured address of slave
 * @param[in] eeproma     (WORD) Address in the EEPROM
 * @param[in] timeout     Timeout in us.
 * @return EEPROM data 64bit or 32bit
 */
uint64 ecx_readeepromFP(ecx_contextt *context, uint16 configadr, uint16 eeproma, int timeout)
{
   uint16 estat;
   uint32 edat32;
   uint64 edat64;
   ec_eepromt ed;
   int wkc, cnt, nackcnt = 0;

   edat64 = 0;
   edat32 = 0;
   if (ecx_eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
   {
      if (estat & EC_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(EC_ECMD_NOP); /* clear error bits */
         wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPCTL, sizeof(estat), &estat, EC_TIMEOUTRET3);
      }

      do
      {
         ed.comm = htoes(EC_ECMD_READ);
         ed.addr = htoes(eeproma);
         ed.d2 = 0x0000;
         cnt = 0;
         do
         {
            wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPCTL, sizeof(ed), &ed, EC_TIMEOUTRET);
         } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
         if (wkc)
         {
            osal_usleep(EC_LOCALDELAY);
            estat = 0x0000;
            if (ecx_eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
            {
               if (estat & EC_ESTAT_NACK)
               {
                  nackcnt++;
                  osal_usleep(EC_LOCALDELAY * 5);
               }
               else
               {
                  nackcnt = 0;
                  if (estat & EC_ESTAT_R64)
                  {
                     cnt = 0;
                     do
                     {
                        wkc = ecx_FPRD(&context->port, configadr, ECT_REG_EEPDAT, sizeof(edat64), &edat64, EC_TIMEOUTRET);
                     } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
                  }
                  else
                  {
                     cnt = 0;
                     do
                     {
                        wkc = ecx_FPRD(&context->port, configadr, ECT_REG_EEPDAT, sizeof(edat32), &edat32, EC_TIMEOUTRET);
                     } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
                     edat64 = (uint64)edat32;
                  }
               }
            }
         }
      } while ((nackcnt > 0) && (nackcnt < 3));
   }

   return edat64;
}

/** Write EEPROM to slave bypassing cache. FPWR method.
 * @param[in]  context    context struct
 * @param[in] configadr   configured address of slave
 * @param[in] eeproma     (WORD) Address in the EEPROM
 * @param[in] data        16bit data
 * @param[in] timeout     Timeout in us.
 * @return >0 if OK
 */
int ecx_writeeepromFP(ecx_contextt *context, uint16 configadr, uint16 eeproma, uint16 data, int timeout)
{
   uint16 estat;
   ec_eepromt ed;
   int wkc, rval = 0, cnt = 0, nackcnt = 0;

   if (ecx_eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
   {
      if (estat & EC_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(EC_ECMD_NOP); /* clear error bits */
         wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPCTL, sizeof(estat), &estat, EC_TIMEOUTRET3);
      }
      do
      {
         cnt = 0;
         do
         {
            wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPDAT, sizeof(data), &data, EC_TIMEOUTRET);
         } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
         ed.comm = EC_ECMD_WRITE;
         ed.addr = eeproma;
         ed.d2 = 0x0000;
         cnt = 0;
         do
         {
            wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPCTL, sizeof(ed), &ed, EC_TIMEOUTRET);
         } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
         if (wkc)
         {
            osal_usleep(EC_LOCALDELAY * 2);
            estat = 0x0000;
            if (ecx_eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
            {
               if (estat & EC_ESTAT_NACK)
               {
                  nackcnt++;
                  osal_usleep(EC_LOCALDELAY * 5);
               }
               else
               {
                  nackcnt = 0;
                  rval = 1;
               }
            }
         }
      } while ((nackcnt > 0) && (nackcnt < 3));
   }

   return rval;
}

/** Read EEPROM from slave bypassing cache.
 * Parallel read step 1, make request to slave.
 * @param[in] context     context struct
 * @param[in] slave       Slave number
 * @param[in] eeproma     (WORD) Address in the EEPROM
 */
void ecx_readeeprom1(ecx_contextt *context, uint16 slave, uint16 eeproma)
{
   uint16 configadr, estat;
   ec_eepromt ed;
   int wkc, cnt = 0;

   ecx_eeprom2master(context, slave); /* set eeprom control to master */
   configadr = context->slavelist[slave].configadr;
   if (ecx_eeprom_waitnotbusyFP(context, configadr, &estat, EC_TIMEOUTEEP))
   {
      if (estat & EC_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(EC_ECMD_NOP); /* clear error bits */
         wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPCTL, sizeof(estat), &estat, EC_TIMEOUTRET3);
      }
      ed.comm = htoes(EC_ECMD_READ);
      ed.addr = htoes(eeproma);
      ed.d2 = 0x0000;
      do
      {
         wkc = ecx_FPWR(&context->port, configadr, ECT_REG_EEPCTL, sizeof(ed), &ed, EC_TIMEOUTRET);
      } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
   }
}

/** Read EEPROM from slave bypassing cache.
 * Parallel read step 2, actual read from slave.
 * @param[in]  context    context struct
 * @param[in] slave       Slave number
 * @param[in] timeout     Timeout in us.
 * @return EEPROM data 32bit
 */
uint32 ecx_readeeprom2(ecx_contextt *context, uint16 slave, int timeout)
{
   uint16 estat, configadr;
   uint32 edat;
   int wkc, cnt = 0;

   configadr = context->slavelist[slave].configadr;
   edat = 0;
   estat = 0x0000;
   if (ecx_eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
   {
      do
      {
         wkc = ecx_FPRD(&context->port, configadr, ECT_REG_EEPDAT, sizeof(edat), &edat, EC_TIMEOUTRET);
      } while ((wkc <= 0) && (cnt++ < EC_DEFAULTRETRIES));
   }

   return edat;
}

/** Push index of segmented LRD/LWR/LRW combination.
 * @param[in]  context    context struct
 * @param[in] idx         Used datagram index.
 * @param[in] data        Pointer to process data segment.
 * @param[in] length      Length of data segment in bytes.
 * @param[in] DCO         Offset position of DC frame.
 */
static void ecx_pushindex(ecx_contextt *context, uint8 idx, void *data, uint16 length, uint16 DCO)
{
   if (context->idxstack.pushed < EC_MAXBUF)
   {
      context->idxstack.idx[context->idxstack.pushed] = idx;
      context->idxstack.data[context->idxstack.pushed] = data;
      context->idxstack.length[context->idxstack.pushed] = length;
      context->idxstack.dcoffset[context->idxstack.pushed] = DCO;
      context->idxstack.pushed++;
   }
}

/** Pull index of segmented LRD/LWR/LRW combination.
 * @param[in]  context        context struct
 * @return Stack location, -1 if stack is empty.
 */
static int ecx_pullindex(ecx_contextt *context)
{
   int rval = -1;
   if (context->idxstack.pulled < context->idxstack.pushed)
   {
      rval = context->idxstack.pulled;
      context->idxstack.pulled++;
   }

   return rval;
}

/**
 * Clear the idx stack.
 *
 * @param context           context struct
 */
static void ecx_clearindex(ecx_contextt *context)
{

   context->idxstack.pushed = 0;
   context->idxstack.pulled = 0;
}

/** Transmit processdata to slaves.
 *
 * Uses LRW, or LRD/LWR if LRW is not allowed (blockLRW).
 *
 * In overlap mode, outputs are sent in the outgoing frame and inputs
 * will replace outputs in the incoming frame.
 *
 * In non-overlap mode, outputs are followed by extra space for inputs
 * in the incoming frame.
 *
 * The inputs are gathered with the receive processdata function.
 * In contrast to the base LRW function this function is non-blocking.
 * If the processdata does not fit in one datagram, multiple are used.
 * In order to recombine the slave response, a stack is used.
 * @param[in]  context        context struct
 * @param[in]  group          group number
 * @return >0 if processdata is transmitted.
 */
int ecx_send_processdata_group(ecx_contextt *context, uint8 group)
{
   uint32 LogAdr;
   uint16 w1, w2;
   int length;
   uint16 sublength;
   uint8 idx;
   int wkc;
   uint8 *data;
   boolean first = FALSE;
   uint16 currentsegment = 0;
   uint32 iomapinputoffset;
   uint16 DCO;

   wkc = 0;
   if (context->grouplist[group].hasdc)
   {
      first = TRUE;
   }

   ecx_clearmbxstatus(context, group);

   /* For overlapping IO map use the biggest */
   if (context->overlappedMode == TRUE)
   {
      /* For overlap IOmap make the frame EQ big to biggest part */
      length = (context->grouplist[group].Obytes > context->grouplist[group].Ibytes) ? context->grouplist[group].Obytes : context->grouplist[group].Ibytes;
      length += context->grouplist[group].mbxstatuslength;
      /* Save the offset used to compensate where to save inputs when frame returns */
      iomapinputoffset = context->grouplist[group].Obytes;
   }
   else
   {
      length = context->grouplist[group].Obytes +
               context->grouplist[group].Ibytes +
               context->grouplist[group].mbxstatuslength;
      iomapinputoffset = 0;
   }

   LogAdr = context->grouplist[group].logstartaddr;
   if (length)
   {

      wkc = 1;
      /* LRW blocked by one or more slaves ? */
      if (context->grouplist[group].blockLRW)
      {
         /* if inputs available generate LRD */
         if (context->grouplist[group].Ibytes)
         {
            currentsegment = context->grouplist[group].Isegment;
            data = context->grouplist[group].inputs;
            length = context->grouplist[group].Ibytes;
            LogAdr += context->grouplist[group].Obytes;
            /* segment transfer if needed */
            do
            {
               if (currentsegment == context->grouplist[group].Isegment)
               {
                  sublength = (uint16)(context->grouplist[group].IOsegment[currentsegment++] - context->grouplist[group].Ioffset);
               }
               else
               {
                  sublength = (uint16)context->grouplist[group].IOsegment[currentsegment++];
               }
               /* get new index */
               idx = ecx_getindex(&context->port);
               w1 = LO_WORD(LogAdr);
               w2 = HI_WORD(LogAdr);
               DCO = 0;
               ecx_setupdatagram(&context->port, &(context->port.txbuf[idx]), EC_CMD_LRD, idx, w1, w2, sublength, data);
               if (first)
               {
                  /* FPRMW in second datagram */
                  DCO = ecx_adddatagram(&context->port, &(context->port.txbuf[idx]), EC_CMD_FRMW, idx, FALSE,
                                        context->slavelist[context->grouplist[group].DCnext].configadr,
                                        ECT_REG_DCSYSTIME, sizeof(int64), &context->DCtime);
                  first = FALSE;
               }
               /* send frame */
               ecx_outframe_red(&context->port, idx);
               /* push index and data pointer on stack */
               ecx_pushindex(context, idx, data, sublength, DCO);
               length -= sublength;
               LogAdr += sublength;
               data += sublength;
            } while (length && (currentsegment < context->grouplist[group].nsegments));
         }
         /* if outputs available generate LWR */
         if (context->grouplist[group].Obytes)
         {
            data = context->grouplist[group].outputs;
            length = context->grouplist[group].Obytes;
            LogAdr = context->grouplist[group].logstartaddr;
            currentsegment = 0;
            /* segment transfer if needed */
            do
            {
               sublength = (uint16)context->grouplist[group].IOsegment[currentsegment++];
               if ((length - sublength) < 0)
               {
                  sublength = (uint16)length;
               }
               /* get new index */
               idx = ecx_getindex(&context->port);
               w1 = LO_WORD(LogAdr);
               w2 = HI_WORD(LogAdr);
               DCO = 0;
               ecx_setupdatagram(&context->port, &(context->port.txbuf[idx]), EC_CMD_LWR, idx, w1, w2, sublength, data);
               if (first)
               {
                  /* FPRMW in second datagram */
                  DCO = ecx_adddatagram(&context->port, &(context->port.txbuf[idx]), EC_CMD_FRMW, idx, FALSE,
                                        context->slavelist[context->grouplist[group].DCnext].configadr,
                                        ECT_REG_DCSYSTIME, sizeof(int64), &context->DCtime);
                  first = FALSE;
               }
               /* send frame */
               ecx_outframe_red(&context->port, idx);
               /* push index and data pointer on stack */
               ecx_pushindex(context, idx, data, sublength, DCO);
               length -= sublength;
               LogAdr += sublength;
               data += sublength;
            } while (length && (currentsegment < context->grouplist[group].nsegments));
         }
      }
      /* LRW can be used */
      else
      {
         if (context->grouplist[group].Obytes)
         {
            data = context->grouplist[group].outputs;
         }
         else
         {
            data = context->grouplist[group].inputs;
            /* Clear offset, don't compensate for overlapping IOmap if we only got inputs */
            iomapinputoffset = 0;
         }
         /* segment transfer if needed */
         do
         {
            sublength = (uint16)context->grouplist[group].IOsegment[currentsegment++];
            /* get new index */
            idx = ecx_getindex(&context->port);
            w1 = LO_WORD(LogAdr);
            w2 = HI_WORD(LogAdr);
            DCO = 0;
            ecx_setupdatagram(&context->port, &(context->port.txbuf[idx]), EC_CMD_LRW, idx, w1, w2, sublength, data);
            if (first)
            {
               /* FPRMW in second datagram */
               DCO = ecx_adddatagram(&context->port, &(context->port.txbuf[idx]), EC_CMD_FRMW, idx, FALSE,
                                     context->slavelist[context->grouplist[group].DCnext].configadr,
                                     ECT_REG_DCSYSTIME, sizeof(int64), &context->DCtime);
               first = FALSE;
            }
            /* send frame */
            ecx_outframe_red(&context->port, idx);
            /* push index and data pointer on stack.
             * the iomapinputoffset compensate for where the inputs are stored
             * in the IOmap if we use an overlapping IOmap. If a regular IOmap
             * is used it should always be 0.
             */
            ecx_pushindex(context, idx, (data + iomapinputoffset), sublength, DCO);
            length -= sublength;
            LogAdr += sublength;
            data += sublength;
         } while (length && (currentsegment < context->grouplist[group].nsegments));
      }
   }

   return wkc;
}

/** Receive processdata from slaves.
 * Second part from ec_send_processdata().
 * Received datagrams are recombined with the processdata with help from the stack.
 * If a datagram contains input processdata it copies it to the processdata structure.
 * @param[in]  context        context struct
 * @param[in]  group          group number
 * @param[in]  timeout        Timeout in us.
 * @return Work counter.
 */
int ecx_receive_processdata_group(ecx_contextt *context, uint8 group, int timeout)
{
   uint8 idx;
   int pos;
   int wkc = 0, wkc2;
   uint16 le_wkc = 0;
   int valid_wkc = 0;
   int64 le_DCtime;
   ec_idxstackT *idxstack;
   ec_bufT *rxbuf;

   /* just to prevent compiler warning for unused group */
   wkc2 = group;

   idxstack = &context->idxstack;
   rxbuf = context->port.rxbuf;
   /* get first index */
   pos = ecx_pullindex(context);
   /* read the same number of frames as send */
   while (pos >= 0)
   {
      idx = idxstack->idx[pos];
      wkc2 = ecx_waitinframe(&context->port, idx, timeout);
      /* check if there is input data in frame */
      if (wkc2 > EC_NOFRAME)
      {
         if ((rxbuf[idx][EC_CMDOFFSET] == EC_CMD_LRD) || (rxbuf[idx][EC_CMDOFFSET] == EC_CMD_LRW))
         {
            if (idxstack->dcoffset[pos] > 0)
            {
               memcpy(idxstack->data[pos], &(rxbuf[idx][EC_HEADERSIZE]), idxstack->length[pos]);
               memcpy(&le_wkc, &(rxbuf[idx][EC_HEADERSIZE + idxstack->length[pos]]), EC_WKCSIZE);
               wkc = etohs(le_wkc);
               memcpy(&le_DCtime, &(rxbuf[idx][idxstack->dcoffset[pos]]), sizeof(le_DCtime));
               context->DCtime = etohll(le_DCtime);
            }
            else
            {
               /* copy input data back to process data buffer */
               memcpy(idxstack->data[pos], &(rxbuf[idx][EC_HEADERSIZE]), idxstack->length[pos]);
               wkc += wkc2;
            }
            valid_wkc = 1;
         }
         else if (rxbuf[idx][EC_CMDOFFSET] == EC_CMD_LWR)
         {
            if (idxstack->dcoffset[pos] > 0)
            {
               memcpy(&le_wkc, &(rxbuf[idx][EC_HEADERSIZE + idxstack->length[pos]]), EC_WKCSIZE);
               /* output WKC counts 2 times when using LRW, emulate the same for LWR */
               wkc = etohs(le_wkc) * 2;
               memcpy(&le_DCtime, &(rxbuf[idx][idxstack->dcoffset[pos]]), sizeof(le_DCtime));
               context->DCtime = etohll(le_DCtime);
            }
            else
            {
               /* output WKC counts 2 times when using LRW, emulate the same for LWR */
               wkc += wkc2 * 2;
            }
            valid_wkc = 1;
         }
      }
      /* release buffer */
      ecx_setbufstat(&context->port, idx, EC_BUF_EMPTY);
      /* get next index */
      pos = ecx_pullindex(context);
   }

   ecx_clearindex(context);

   /* if no frames has arrived */
   if (valid_wkc == 0)
   {
      return EC_NOFRAME;
   }
   return wkc;
}

/**
 * Send processdata to slaves.
 * Group number is zero (default).
 * @param[in]  context        context struct
 * @return Work counter.
 */
int ecx_send_processdata(ecx_contextt *context)
{
   return ecx_send_processdata_group(context, 0);
}

/**
 * Receive processdata from slaves.
 * Group number is zero (default).
 * @param[in]  context        context struct
 * @param[in]  timeout        Timeout in us.
 * @return Work counter.
 */
int ecx_receive_processdata(ecx_contextt *context, int timeout)
{
   return ecx_receive_processdata_group(context, 0, timeout);
}
