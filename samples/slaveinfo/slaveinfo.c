/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "soem/soem.h"

static uint8 IOmap[4096];
static ec_ODlistt ODlist;
static ec_OElistt OElist;
static boolean printSDO = FALSE;
static boolean printMAP = FALSE;
static char usdo[128];

static ecx_contextt ctx;

#define OTYPE_VAR    0x0007
#define OTYPE_ARRAY  0x0008
#define OTYPE_RECORD 0x0009

#define ATYPE_Rpre   0x01
#define ATYPE_Rsafe  0x02
#define ATYPE_Rop    0x04
#define ATYPE_Wpre   0x08
#define ATYPE_Wsafe  0x10
#define ATYPE_Wop    0x20

char *dtype2string(uint16 dtype, uint16 bitlen)
{
   static char str[32] = {0};

   switch (dtype)
   {
   case ECT_BOOLEAN:
      sprintf(str, "BOOLEAN");
      break;
   case ECT_INTEGER8:
      sprintf(str, "INTEGER8");
      break;
   case ECT_INTEGER16:
      sprintf(str, "INTEGER16");
      break;
   case ECT_INTEGER32:
      sprintf(str, "INTEGER32");
      break;
   case ECT_INTEGER24:
      sprintf(str, "INTEGER24");
      break;
   case ECT_INTEGER64:
      sprintf(str, "INTEGER64");
      break;
   case ECT_UNSIGNED8:
      sprintf(str, "UNSIGNED8");
      break;
   case ECT_UNSIGNED16:
      sprintf(str, "UNSIGNED16");
      break;
   case ECT_UNSIGNED32:
      sprintf(str, "UNSIGNED32");
      break;
   case ECT_UNSIGNED24:
      sprintf(str, "UNSIGNED24");
      break;
   case ECT_UNSIGNED64:
      sprintf(str, "UNSIGNED64");
      break;
   case ECT_REAL32:
      sprintf(str, "REAL32");
      break;
   case ECT_REAL64:
      sprintf(str, "REAL64");
      break;
   case ECT_BIT1:
      sprintf(str, "BIT1");
      break;
   case ECT_BIT2:
      sprintf(str, "BIT2");
      break;
   case ECT_BIT3:
      sprintf(str, "BIT3");
      break;
   case ECT_BIT4:
      sprintf(str, "BIT4");
      break;
   case ECT_BIT5:
      sprintf(str, "BIT5");
      break;
   case ECT_BIT6:
      sprintf(str, "BIT6");
      break;
   case ECT_BIT7:
      sprintf(str, "BIT7");
      break;
   case ECT_BIT8:
      sprintf(str, "BIT8");
      break;
   case ECT_VISIBLE_STRING:
      sprintf(str, "VISIBLE_STR(%d)", bitlen);
      break;
   case ECT_OCTET_STRING:
      sprintf(str, "OCTET_STR(%d)", bitlen);
      break;
   default:
      sprintf(str, "dt:0x%4.4X (%d)", dtype, bitlen);
   }
   return str;
}

char *otype2string(uint16 otype)
{
   static char str[32] = {0};

   switch (otype)
   {
   case OTYPE_VAR:
      sprintf(str, "VAR");
      break;
   case OTYPE_ARRAY:
      sprintf(str, "ARRAY");
      break;
   case OTYPE_RECORD:
      sprintf(str, "RECORD");
      break;
   default:
      sprintf(str, "ot:0x%4.4X", otype);
   }
   return str;
}

char *access2string(uint16 access)
{
   static char str[32] = {0};

   sprintf(str, "%s%s%s%s%s%s",
           ((access & ATYPE_Rpre) != 0 ? "R" : "_"),
           ((access & ATYPE_Wpre) != 0 ? "W" : "_"),
           ((access & ATYPE_Rsafe) != 0 ? "R" : "_"),
           ((access & ATYPE_Wsafe) != 0 ? "W" : "_"),
           ((access & ATYPE_Rop) != 0 ? "R" : "_"),
           ((access & ATYPE_Wop) != 0 ? "W" : "_"));
   return str;
}

char *SDO2string(uint16 slave, uint16 index, uint8 subidx, uint16 dtype)
{
   int l = sizeof(usdo) - 1, i;
   uint8 *u8;
   int8 *i8;
   uint16 *u16;
   int16 *i16;
   uint32 *u32;
   int32 *i32;
   uint64 *u64;
   int64 *i64;
   float *sr;
   double *dr;
   char *p;
   size_t size;

   memset(&usdo, 0, sizeof(usdo));
   ecx_SDOread(&ctx, slave, index, subidx, FALSE, &l, &usdo, EC_TIMEOUTRXM);
   if (ctx.ecaterror)
   {
      return ecx_elist2string(&ctx);
   }
   else
   {
      static char str[sizeof(usdo) + 3] = {0};
      switch (dtype)
      {
      case ECT_BOOLEAN:
         u8 = (uint8 *)&usdo[0];
         if (*u8)
            snprintf(str, sizeof(str), "TRUE");
         else
            snprintf(str, sizeof(str), "FALSE");
         break;
      case ECT_INTEGER8:
         i8 = (int8 *)&usdo[0];
         snprintf(str, sizeof(str), "0x%2.2x / %d", *i8, *i8);
         break;
      case ECT_INTEGER16:
         i16 = (int16 *)&usdo[0];
         snprintf(str, sizeof(str), "0x%4.4x / %d", *i16, *i16);
         break;
      case ECT_INTEGER32:
      case ECT_INTEGER24:
         i32 = (int32 *)&usdo[0];
         snprintf(str, sizeof(str), "0x%8.8x / %d", *i32, *i32);
         break;
      case ECT_INTEGER64:
         i64 = (int64 *)&usdo[0];
         snprintf(str, sizeof(str), "0x%16.16" PRIx64 " / %" PRId64, *i64, *i64);
         break;
      case ECT_UNSIGNED8:
         u8 = (uint8 *)&usdo[0];
         snprintf(str, sizeof(str), "0x%2.2x / %u", *u8, *u8);
         break;
      case ECT_UNSIGNED16:
         u16 = (uint16 *)&usdo[0];
         snprintf(str, sizeof(str), "0x%4.4x / %u", *u16, *u16);
         break;
      case ECT_UNSIGNED32:
      case ECT_UNSIGNED24:
         u32 = (uint32 *)&usdo[0];
         snprintf(str, sizeof(str), "0x%8.8x / %u", *u32, *u32);
         break;
      case ECT_UNSIGNED64:
         u64 = (uint64 *)&usdo[0];
         snprintf(str, sizeof(str), "0x%16.16" PRIx64 " / %" PRIu64, *u64, *u64);
         break;
      case ECT_REAL32:
         sr = (float *)&usdo[0];
         snprintf(str, sizeof(str), "%f", *sr);
         break;
      case ECT_REAL64:
         dr = (double *)&usdo[0];
         snprintf(str, sizeof(str), "%f", *dr);
         break;
      case ECT_BIT1:
      case ECT_BIT2:
      case ECT_BIT3:
      case ECT_BIT4:
      case ECT_BIT5:
      case ECT_BIT6:
      case ECT_BIT7:
      case ECT_BIT8:
         u8 = (uint8 *)&usdo[0];
         snprintf(str, sizeof(str), "0x%x / %u", *u8, *u8);
         break;
      case ECT_VISIBLE_STRING:
         snprintf(str, sizeof(str), "\"%s\"", usdo);
         strcat(str, "\"");
         break;
      case ECT_OCTET_STRING:
         p = str;
         size = sizeof(str);
         for (i = 0; i < l; i++)
         {
            int n = snprintf(p, size, "0x%2.2x ", usdo[i]);
            if (n > (int)size)
               break;
            p += n;
            size -= n;
         }
         break;
      default:
         snprintf(str, sizeof(str), "Unknown type");
      }
      str[sizeof(str) - 1] = 0;
      return str;
   }
}

/** Read PDO assign structure */
int si_PDOassign(uint16 slave, uint16 PDOassign, int mapoffset, int bitoffset)
{
   uint16 idxloop, nidx, subidxloop, rdat, idx, subidx;
   uint8 subcnt;
   int wkc, bsize = 0, rdl;
   int32 rdat2;
   uint8 bitlen, obj_subidx;
   uint16 obj_idx;
   int abs_offset, abs_bit;

   rdl = sizeof(rdat);
   rdat = 0;
   /* read PDO assign subindex 0 ( = number of PDO's) */
   wkc = ecx_SDOread(&ctx, slave, PDOassign, 0x00, FALSE, &rdl, &rdat, EC_TIMEOUTRXM);
   rdat = etohs(rdat);
   /* positive result from slave ? */
   if ((wkc > 0) && (rdat > 0))
   {
      /* number of available sub indexes */
      nidx = rdat;
      bsize = 0;
      /* read all PDO's */
      for (idxloop = 1; idxloop <= nidx; idxloop++)
      {
         rdl = sizeof(rdat);
         rdat = 0;
         /* read PDO assign */
         wkc = ecx_SDOread(&ctx, slave, PDOassign, (uint8)idxloop, FALSE, &rdl, &rdat, EC_TIMEOUTRXM);
         /* result is index of PDO */
         idx = etohs(rdat);
         if (idx > 0)
         {
            rdl = sizeof(subcnt);
            subcnt = 0;
            /* read number of subindexes of PDO */
            wkc = ecx_SDOread(&ctx, slave, idx, 0x00, FALSE, &rdl, &subcnt, EC_TIMEOUTRXM);
            subidx = subcnt;
            /* for each subindex */
            for (subidxloop = 1; subidxloop <= subidx; subidxloop++)
            {
               rdl = sizeof(rdat2);
               rdat2 = 0;
               /* read SDO that is mapped in PDO */
               wkc = ecx_SDOread(&ctx, slave, idx, (uint8)subidxloop, FALSE, &rdl, &rdat2, EC_TIMEOUTRXM);
               rdat2 = etohl(rdat2);
               /* extract bitlength of SDO */
               bitlen = LO_BYTE(rdat2);
               bsize += bitlen;
               obj_idx = (uint16)(rdat2 >> 16);
               obj_subidx = (uint8)((rdat2 >> 8) & 0x000000ff);
               abs_offset = mapoffset + (bitoffset / 8);
               abs_bit = bitoffset % 8;
               ODlist.Slave = slave;
               ODlist.Index[0] = obj_idx;
               OElist.Entries = 0;
               wkc = 0;
               /* read object entry from dictionary if not a filler (0x0000:0x00) */
               if (obj_idx || obj_subidx)
                  wkc = ecx_readOEsingle(&ctx, 0, obj_subidx, &ODlist, &OElist);
               printf("  [0x%4.4X.%1d] 0x%4.4X:0x%2.2X 0x%2.2X", abs_offset, abs_bit, obj_idx, obj_subidx, bitlen);
               if ((wkc > 0) && OElist.Entries)
               {
                  printf(" %-12s %s\n", dtype2string(OElist.DataType[obj_subidx], bitlen), OElist.Name[obj_subidx]);
               }
               else
                  printf("\n");
               bitoffset += bitlen;
            };
         };
      };
   };
   /* return total found bitlength (PDO) */
   return bsize;
}

int si_map_sdo(int slave)
{
   int wkc, rdl;
   int retVal = 0;
   uint8 nSM, iSM, tSM;
   int Tsize, outputs_bo, inputs_bo;
   uint8 SMt_bug_add;

   printf("PDO mapping according to CoE :\n");
   SMt_bug_add = 0;
   outputs_bo = 0;
   inputs_bo = 0;
   rdl = sizeof(nSM);
   nSM = 0;
   /* read SyncManager Communication Type object count */
   wkc = ecx_SDOread(&ctx, slave, ECT_SDO_SMCOMMTYPE, 0x00, FALSE, &rdl, &nSM, EC_TIMEOUTRXM);
   /* positive result from slave ? */
   if ((wkc > 0) && (nSM > 2))
   {
      /* make nSM equal to number of defined SM */
      nSM--;
      /* limit to maximum number of SM defined, if true the slave can't be configured */
      if (nSM > EC_MAXSM)
         nSM = EC_MAXSM;
      /* iterate for every SM type defined */
      for (iSM = 2; iSM <= nSM; iSM++)
      {
         rdl = sizeof(tSM);
         tSM = 0;
         /* read SyncManager Communication Type */
         wkc = ecx_SDOread(&ctx, slave, ECT_SDO_SMCOMMTYPE, iSM + 1, FALSE, &rdl, &tSM, EC_TIMEOUTRXM);
         if (wkc > 0)
         {
            if ((iSM == 2) && (tSM == 2)) // SM2 has type 2 == mailbox out, this is a bug in the slave!
            {
               SMt_bug_add = 1; // try to correct, this works if the types are 0 1 2 3 and should be 1 2 3 4
               printf("Activated SM type workaround, possible incorrect mapping.\n");
            }
            if (tSM)
               tSM += SMt_bug_add; // only add if SMt > 0

            if (tSM == 3) // outputs
            {
               /* read the assign RXPDO */
               printf("  SM%1d outputs\n     addr b   index: sub bitl data_type    name\n", iSM);
               Tsize = si_PDOassign(slave, ECT_SDO_PDOASSIGN + iSM, (int)(ctx.slavelist[slave].outputs - IOmap), outputs_bo);
               outputs_bo += Tsize;
            }
            if (tSM == 4) // inputs
            {
               /* read the assign TXPDO */
               printf("  SM%1d inputs\n     addr b   index: sub bitl data_type    name\n", iSM);

               Tsize = si_PDOassign(slave, ECT_SDO_PDOASSIGN + iSM, (int)(ctx.slavelist[slave].inputs - IOmap), inputs_bo);
               inputs_bo += Tsize;
            }
         }
      }
   }

   /* found some I/O bits ? */
   if ((outputs_bo > 0) || (inputs_bo > 0))
      retVal = 1;
   return retVal;
}

int si_siiPDO(uint16 slave, uint8 t, int mapoffset, int bitoffset)
{
   uint16 a, w, c, e, er;
   uint8 eectl;
   uint16 obj_idx;
   uint8 obj_subidx;
   uint8 obj_name;
   uint8 obj_datatype;
   uint8 bitlen;
   int totalsize;
   ec_eepromPDOt eepPDO;
   ec_eepromPDOt *PDO;
   int abs_offset, abs_bit;
   char str_name[EC_MAXNAME + 1];

   eectl = ctx.slavelist[slave].eep_pdi;

   totalsize = 0;
   PDO = &eepPDO;
   PDO->nPDO = 0;
   PDO->Length = 0;
   PDO->Index[1] = 0;
   for (c = 0; c < EC_MAXSM; c++)
      PDO->SMbitsize[c] = 0;
   if (t > 1)
      t = 1;
   PDO->Startpos = ecx_siifind(&ctx, slave, ECT_SII_PDO + t);
   if (PDO->Startpos > 0)
   {
      a = PDO->Startpos;
      w = ecx_siigetbyte(&ctx, slave, a++);
      w += (ecx_siigetbyte(&ctx, slave, a++) << 8);
      PDO->Length = w;
      c = 1;
      /* traverse through all PDOs */
      do
      {
         PDO->nPDO++;
         PDO->Index[PDO->nPDO] = ecx_siigetbyte(&ctx, slave, a++);
         PDO->Index[PDO->nPDO] += (ecx_siigetbyte(&ctx, slave, a++) << 8);
         PDO->BitSize[PDO->nPDO] = 0;
         c++;
         /* number of entries in PDO */
         e = ecx_siigetbyte(&ctx, slave, a++);
         PDO->SyncM[PDO->nPDO] = ecx_siigetbyte(&ctx, slave, a++);
         a++;
         obj_name = ecx_siigetbyte(&ctx, slave, a++);
         a += 2;
         c += 2;
         if (PDO->SyncM[PDO->nPDO] < EC_MAXSM) /* active and in range SM? */
         {
            str_name[0] = 0;
            if (obj_name)
               ecx_siistring(&ctx, str_name, slave, obj_name);
            if (t)
               printf("  SM%1d RXPDO 0x%4.4X %s\n", PDO->SyncM[PDO->nPDO], PDO->Index[PDO->nPDO], str_name);
            else
               printf("  SM%1d TXPDO 0x%4.4X %s\n", PDO->SyncM[PDO->nPDO], PDO->Index[PDO->nPDO], str_name);
            printf("     addr b   index: sub bitl data_type    name\n");
            /* read all entries defined in PDO */
            for (er = 1; er <= e; er++)
            {
               c += 4;
               obj_idx = ecx_siigetbyte(&ctx, slave, a++);
               obj_idx += (ecx_siigetbyte(&ctx, slave, a++) << 8);
               obj_subidx = ecx_siigetbyte(&ctx, slave, a++);
               obj_name = ecx_siigetbyte(&ctx, slave, a++);
               obj_datatype = ecx_siigetbyte(&ctx, slave, a++);
               bitlen = ecx_siigetbyte(&ctx, slave, a++);
               abs_offset = mapoffset + (bitoffset / 8);
               abs_bit = bitoffset % 8;

               PDO->BitSize[PDO->nPDO] += bitlen;
               a += 2;

               /* skip entry if filler (0x0000:0x00) */
               if (obj_idx || obj_subidx)
               {
                  str_name[0] = 0;
                  if (obj_name)
                     ecx_siistring(&ctx, str_name, slave, obj_name);

                  printf("  [0x%4.4X.%1d] 0x%4.4X:0x%2.2X 0x%2.2X", abs_offset, abs_bit, obj_idx, obj_subidx, bitlen);
                  printf(" %-12s %s\n", dtype2string(obj_datatype, bitlen), str_name);
               }
               bitoffset += bitlen;
               totalsize += bitlen;
            }
            PDO->SMbitsize[PDO->SyncM[PDO->nPDO]] += PDO->BitSize[PDO->nPDO];
            c++;
         }
         else /* PDO deactivated because SM is 0xff or > EC_MAXSM */
         {
            c += 4 * e;
            a += 8 * e;
            c++;
         }
         if (PDO->nPDO >= (EC_MAXEEPDO - 1)) c = PDO->Length; /* limit number of PDO entries in buffer */
      } while (c < PDO->Length);
   }
   if (eectl) ecx_eeprom2pdi(&ctx, slave); /* if eeprom control was previously pdi then restore */
   return totalsize;
}

int si_map_sii(int slave)
{
   int retVal = 0;
   int Tsize, outputs_bo, inputs_bo;

   printf("PDO mapping according to SII :\n");

   outputs_bo = 0;
   inputs_bo = 0;
   /* read the assign RXPDOs */
   Tsize = si_siiPDO(slave, 1, (int)(ctx.slavelist[slave].outputs - IOmap), outputs_bo);
   outputs_bo += Tsize;
   /* read the assign TXPDOs */
   Tsize = si_siiPDO(slave, 0, (int)(ctx.slavelist[slave].inputs - IOmap), inputs_bo);
   inputs_bo += Tsize;
   /* found some I/O bits ? */
   if ((outputs_bo > 0) || (inputs_bo > 0))
      retVal = 1;
   return retVal;
}

void si_sdo(int cnt)
{
   int i, j;

   ODlist.Entries = 0;
   memset(&ODlist, 0, sizeof(ODlist));
   if (ecx_readODlist(&ctx, cnt, &ODlist))
   {
      printf(" CoE Object Description found, %d entries.\n", ODlist.Entries);
      for (i = 0; i < ODlist.Entries; i++)
      {
         uint16_t max_sub;
         char name[128] = {0};

         ecx_readODdescription(&ctx, i, &ODlist);
         while (ctx.ecaterror)
            printf(" - %s\n", ecx_elist2string(&ctx));
         snprintf(name, sizeof(name) - 1, "\"%s\"", ODlist.Name[i]);
         if (ODlist.ObjectCode[i] == OTYPE_VAR)
         {
            printf("0x%04x      %-40s      [%s]\n", ODlist.Index[i], name,
                   otype2string(ODlist.ObjectCode[i]));
         }
         else
         {
            printf("0x%04x      %-40s      [%s  maxsub(0x%02x / %d)]\n",
                   ODlist.Index[i], name, otype2string(ODlist.ObjectCode[i]),
                   ODlist.MaxSub[i], ODlist.MaxSub[i]);
         }
         memset(&OElist, 0, sizeof(OElist));
         ecx_readOE(&ctx, i, &ODlist, &OElist);
         while (ctx.ecaterror)
            printf("- %s\n", ecx_elist2string(&ctx));

         if (ODlist.ObjectCode[i] != OTYPE_VAR)
         {
            int l = sizeof(max_sub);
            ecx_SDOread(&ctx, cnt, ODlist.Index[i], 0, FALSE, &l, &max_sub, EC_TIMEOUTRXM);
         }
         else
         {
            max_sub = ODlist.MaxSub[i];
         }

         for (j = 0; j < max_sub + 1; j++)
         {
            if ((OElist.DataType[j] > 0) && (OElist.BitLength[j] > 0))
            {
               snprintf(name, sizeof(name) - 1, "\"%s\"", OElist.Name[j]);
               printf("    0x%02x      %-40s      [%-16s %6s]      ", j, name,
                      dtype2string(OElist.DataType[j], OElist.BitLength[j]),
                      access2string(OElist.ObjAccess[j]));
               if ((OElist.ObjAccess[j] & 0x0007))
               {
                  printf("%s", SDO2string(cnt, ODlist.Index[i], j, OElist.DataType[j]));
               }
               printf("\n");
            }
         }
      }
   }
   else
   {
      while (ctx.ecaterror)
         printf("%s", ecx_elist2string(&ctx));
   }
}

void slaveinfo(char *ifname)
{
   int cnt, i, j, nSM;
   uint16 ssigen;
   int expectedWKC;

   printf("Starting slaveinfo\n");

   /* initialise SOEM, bind socket to ifname */
   if (ecx_init(&ctx, ifname))
   {
      printf("ecx_init on %s succeeded.\n", ifname);

      /* find and auto-config slaves */
      if (ecx_config_init(&ctx) > 0)
      {
         ec_groupt *group = &ctx.grouplist[0];

         ecx_config_map_group(&ctx, IOmap, 0);

         ecx_configdc(&ctx);
         while (ctx.ecaterror)
            printf("%s", ecx_elist2string(&ctx));
         printf("%d slaves found and configured.\n", ctx.slavecount);
         expectedWKC = (group->outputsWKC * 2) + group->inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);
         /* wait for all slaves to reach SAFE_OP state */
         ecx_statecheck(&ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 3);
         if (ctx.slavelist[0].state != EC_STATE_SAFE_OP)
         {
            printf("Not all slaves reached safe operational state.\n");
            ecx_readstate(&ctx);
            for (i = 1; i <= ctx.slavecount; i++)
            {
               if (ctx.slavelist[i].state != EC_STATE_SAFE_OP)
               {
                  printf("Slave %d State=%2x StatusCode=%4x : %s\n",
                         i, ctx.slavelist[i].state, ctx.slavelist[i].ALstatuscode, ec_ALstatuscode2string(ctx.slavelist[i].ALstatuscode));
               }
            }
         }

         ecx_readstate(&ctx);
         for (cnt = 1; cnt <= ctx.slavecount; cnt++)
         {
            printf("\nSlave:%d\n Name:%s\n Output size: %dbits\n Input size: %dbits\n State: %d\n Delay: %d[ns]\n Has DC: %d\n",
                   cnt, ctx.slavelist[cnt].name, ctx.slavelist[cnt].Obits, ctx.slavelist[cnt].Ibits,
                   ctx.slavelist[cnt].state, ctx.slavelist[cnt].pdelay, ctx.slavelist[cnt].hasdc);
            if (ctx.slavelist[cnt].hasdc) printf(" DCParentport:%d\n", ctx.slavelist[cnt].parentport);
            printf(" Activeports:%d.%d.%d.%d\n", (ctx.slavelist[cnt].activeports & 0x01) > 0,
                   (ctx.slavelist[cnt].activeports & 0x02) > 0,
                   (ctx.slavelist[cnt].activeports & 0x04) > 0,
                   (ctx.slavelist[cnt].activeports & 0x08) > 0);
            printf(" Configured address: %4.4x\n", ctx.slavelist[cnt].configadr);
            printf(" Man: %8.8x ID: %8.8x Rev: %8.8x\n", (int)ctx.slavelist[cnt].eep_man, (int)ctx.slavelist[cnt].eep_id, (int)ctx.slavelist[cnt].eep_rev);
            for (nSM = 0; nSM < EC_MAXSM; nSM++)
            {
               if (ctx.slavelist[cnt].SM[nSM].StartAddr > 0)
                  printf(" SM%1d A:%4.4x L:%4d F:%8.8x Type:%d\n", nSM, etohs(ctx.slavelist[cnt].SM[nSM].StartAddr), etohs(ctx.slavelist[cnt].SM[nSM].SMlength),
                         etohl(ctx.slavelist[cnt].SM[nSM].SMflags), ctx.slavelist[cnt].SMtype[nSM]);
            }
            for (j = 0; j < ctx.slavelist[cnt].FMMUunused; j++)
            {
               printf(" FMMU%1d Ls:%8.8x Ll:%4d Lsb:%d Leb:%d Ps:%4.4x Psb:%d Ty:%2.2x Act:%2.2x\n", j,
                      etohl(ctx.slavelist[cnt].FMMU[j].LogStart), etohs(ctx.slavelist[cnt].FMMU[j].LogLength), ctx.slavelist[cnt].FMMU[j].LogStartbit,
                      ctx.slavelist[cnt].FMMU[j].LogEndbit, etohs(ctx.slavelist[cnt].FMMU[j].PhysStart), ctx.slavelist[cnt].FMMU[j].PhysStartBit,
                      ctx.slavelist[cnt].FMMU[j].FMMUtype, ctx.slavelist[cnt].FMMU[j].FMMUactive);
            }
            printf(" FMMUfunc 0:%d 1:%d 2:%d 3:%d\n",
                   ctx.slavelist[cnt].FMMU0func, ctx.slavelist[cnt].FMMU1func, ctx.slavelist[cnt].FMMU2func, ctx.slavelist[cnt].FMMU3func);
            printf(" MBX length wr: %d rd: %d MBX protocols : %2.2x\n", ctx.slavelist[cnt].mbx_l, ctx.slavelist[cnt].mbx_rl, ctx.slavelist[cnt].mbx_proto);
            ssigen = ecx_siifind(&ctx, cnt, ECT_SII_GENERAL);
            /* SII general section */
            if (ssigen)
            {
               ctx.slavelist[cnt].CoEdetails = ecx_siigetbyte(&ctx, cnt, ssigen + 0x07);
               ctx.slavelist[cnt].FoEdetails = ecx_siigetbyte(&ctx, cnt, ssigen + 0x08);
               ctx.slavelist[cnt].EoEdetails = ecx_siigetbyte(&ctx, cnt, ssigen + 0x09);
               ctx.slavelist[cnt].SoEdetails = ecx_siigetbyte(&ctx, cnt, ssigen + 0x0a);
               if ((ecx_siigetbyte(&ctx, cnt, ssigen + 0x0d) & 0x02) > 0)
               {
                  ctx.slavelist[cnt].blockLRW = 1;
                  ctx.slavelist[0].blockLRW++;
               }
               ctx.slavelist[cnt].Ebuscurrent = ecx_siigetbyte(&ctx, cnt, ssigen + 0x0e);
               ctx.slavelist[cnt].Ebuscurrent += ecx_siigetbyte(&ctx, cnt, ssigen + 0x0f) << 8;
               ctx.slavelist[0].Ebuscurrent += ctx.slavelist[cnt].Ebuscurrent;
            }
            printf(" CoE details: %2.2x FoE details: %2.2x EoE details: %2.2x SoE details: %2.2x\n",
                   ctx.slavelist[cnt].CoEdetails, ctx.slavelist[cnt].FoEdetails, ctx.slavelist[cnt].EoEdetails, ctx.slavelist[cnt].SoEdetails);
            printf(" Ebus current: %d[mA]\n only LRD/LWR:%d\n",
                   ctx.slavelist[cnt].Ebuscurrent, ctx.slavelist[cnt].blockLRW);
            if ((ctx.slavelist[cnt].mbx_proto & ECT_MBXPROT_COE) && printSDO)
               si_sdo(cnt);
            if (printMAP)
            {
               if (ctx.slavelist[cnt].mbx_proto & ECT_MBXPROT_COE)
                  si_map_sdo(cnt);
               else
                  si_map_sii(cnt);
            }
         }
      }
      else
      {
         printf("No slaves found!\n");
      }
      printf("End slaveinfo, close socket\n");
      /* stop SOEM, close socket */
      ecx_close(&ctx);
   }
   else
   {
      printf("No socket connection on %s\nExcecute as root\n", ifname);
   }
}

char ifbuf[1024];

int main(int argc, char *argv[])
{
   ec_adaptert *adapter = NULL;
   ec_adaptert *head = NULL;
   printf("SOEM (Simple Open EtherCAT Master)\nSlaveinfo\n");

   if (argc > 1)
   {
      if ((argc > 2) && (strncmp(argv[2], "-sdo", sizeof("-sdo")) == 0)) printSDO = TRUE;
      if ((argc > 2) && (strncmp(argv[2], "-map", sizeof("-map")) == 0)) printMAP = TRUE;
      /* start slaveinfo */
      strcpy(ifbuf, argv[1]);
      slaveinfo(ifbuf);
   }
   else
   {
      printf("Usage: slaveinfo ifname [options]\nifname = eth0 for example\nOptions :\n -sdo : print SDO info\n -map : print mapping\n");

      printf("\nAvailable adapters:\n");
      head = adapter = ec_find_adapters();
      while (adapter != NULL)
      {
         printf("    - %s  (%s)\n", adapter->name, adapter->desc);
         adapter = adapter->next;
      }
      ec_free_adapters(head);
   }

   printf("End program\n");
   return (0);
}
