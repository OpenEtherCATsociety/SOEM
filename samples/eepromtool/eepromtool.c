/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "soem/soem.h"

#define MAXBUF          524288
#define STDBUF          2048
#define MINBUF          128
#define CRCBUF          14

#define MODE_NONE       0
#define MODE_READBIN    1
#define MODE_READINTEL  2
#define MODE_WRITEBIN   3
#define MODE_WRITEINTEL 4
#define MODE_WRITEALIAS 5
#define MODE_INFO       6

#define MAXSLENGTH      256

uint8 ebuf[MAXBUF];
uint8 ob;
uint16 ow;
int os;
int slave;
int alias;
ec_timet tstart, tend, tdif;
int wkc;
int mode;
char sline[MAXSLENGTH];

static ecx_contextt ctx;

#define IHEXLENGTH 0x20

void calc_crc(uint8 *crc, uint8 b)
{
   int j;
   *crc ^= b;
   for (j = 0; j <= 7; j++)
   {
      if (*crc & 0x80)
         *crc = (*crc << 1) ^ 0x07;
      else
         *crc = (*crc << 1);
   }
}

uint16 SIIcrc(uint8 *buf)
{
   int i;
   uint8 crc;

   crc = 0xff;
   for (i = 0; i <= 13; i++)
   {
      calc_crc(&crc, *(buf++));
   }
   return (uint16)crc;
}

int input_bin(char *fname, int *length)
{
   FILE *fp;

   int cc = 0, c;

   fp = fopen(fname, "rb");
   if (fp == NULL)
      return 0;
   while (((c = fgetc(fp)) != EOF) && (cc < MAXBUF))
      ebuf[cc++] = (uint8)c;
   *length = cc;
   fclose(fp);

   return 1;
}

int input_intelhex(char *fname, int *start, int *length)
{
   FILE *fp;

   int c, sc, retval = 1;
   int ll, ladr, lt, sn, i, lval;
   int hstart, hlength, sum;

   fp = fopen(fname, "r");
   if (fp == NULL)
      return 0;
   hstart = MAXBUF;
   hlength = 0;
   sum = 0;
   do
   {
      memset(sline, 0x00, MAXSLENGTH);
      sc = 0;
      while (((c = fgetc(fp)) != EOF) && (c != 0x0A) && (sc < (MAXSLENGTH - 1)))
         sline[sc++] = (uint8)c;
      if ((c != EOF) && ((sc < 11) || (sline[0] != ':')))
      {
         c = EOF;
         retval = 0;
         printf("Invalid Intel Hex format.\n");
      }
      if (c != EOF)
      {
         sn = sscanf(sline, ":%2x%4x%2x", &ll, &ladr, &lt);
         if ((sn == 3) && ((ladr + ll) <= MAXBUF) && (lt == 0))
         {
            sum = ll + (ladr >> 8) + (ladr & 0xff) + lt;
            if (ladr < hstart) hstart = ladr;
            for (i = 0; i < ll; i++)
            {
               sn = sscanf(&sline[9 + (i << 1)], "%2x", &lval);
               ebuf[ladr + i] = (uint8)lval;
               sum += (uint8)lval;
            }
            if (((ladr + ll) - hstart) > hlength)
               hlength = (ladr + ll) - hstart;
            sum = (0x100 - sum) & 0xff;
            sn = sscanf(&sline[9 + (i << 1)], "%2x", &lval);
            if (!sn || ((sum - lval) != 0))
            {
               c = EOF;
               retval = 0;
               printf("Invalid checksum.\n");
            }
         }
      }
   } while (c != EOF);
   if (retval)
   {
      *length = hlength;
      *start = hstart;
   }
   fclose(fp);

   return retval;
}

int output_bin(char *fname, int length)
{
   FILE *fp;

   int cc;

   fp = fopen(fname, "wb");
   if (fp == NULL)
      return 0;
   for (cc = 0; cc < length; cc++)
      fputc(ebuf[cc], fp);
   fclose(fp);

   return 1;
}

int output_intelhex(char *fname, int length)
{
   FILE *fp;

   int cc = 0, ll, sum, i;

   fp = fopen(fname, "w");
   if (fp == NULL)
      return 0;
   while (cc < length)
   {
      ll = length - cc;
      if (ll > IHEXLENGTH) ll = IHEXLENGTH;
      sum = ll + (cc >> 8) + (cc & 0xff);
      fprintf(fp, ":%2.2X%4.4X00", ll, cc);
      for (i = 0; i < ll; i++)
      {
         fprintf(fp, "%2.2X", ebuf[cc + i]);
         sum += ebuf[cc + i];
      }
      fprintf(fp, "%2.2X\n", (0x100 - sum) & 0xff);
      cc += ll;
   }
   fprintf(fp, ":00000001FF\n");
   fclose(fp);

   return 1;
}

int eeprom_read(int slave, int start, int length)
{
   int i, ainc = 4;
   uint16 estat, aiadr;
   uint32 b4;
   uint64 b8;
   uint8 eepctl;

   if ((ctx.slavecount >= slave) && (slave > 0) && ((start + length) <= MAXBUF))
   {
      aiadr = 1 - slave;
      eepctl = 2;
      ecx_APWR(&ctx.port, aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl, EC_TIMEOUTRET); /* force Eeprom from PDI */
      eepctl = 0;
      ecx_APWR(&ctx.port, aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl, EC_TIMEOUTRET); /* set Eeprom to master */

      estat = 0x0000;
      aiadr = 1 - slave;
      ecx_APRD(&ctx.port, aiadr, ECT_REG_EEPSTAT, sizeof(estat), &estat, EC_TIMEOUTRET); /* read eeprom status */
      estat = etohs(estat);
      if (estat & EC_ESTAT_R64)
      {
         ainc = 8;
         for (i = start; i < (start + length); i += ainc)
         {
            b8 = ecx_readeepromAP(&ctx, aiadr, i >> 1, EC_TIMEOUTEEP);
            ebuf[i] = b8 & 0xFF;
            ebuf[i + 1] = (b8 >> 8) & 0xFF;
            ebuf[i + 2] = (b8 >> 16) & 0xFF;
            ebuf[i + 3] = (b8 >> 24) & 0xFF;
            ebuf[i + 4] = (b8 >> 32) & 0xFF;
            ebuf[i + 5] = (b8 >> 40) & 0xFF;
            ebuf[i + 6] = (b8 >> 48) & 0xFF;
            ebuf[i + 7] = (b8 >> 56) & 0xFF;
         }
      }
      else
      {
         for (i = start; i < (start + length); i += ainc)
         {
            b4 = ecx_readeepromAP(&ctx, aiadr, i >> 1, EC_TIMEOUTEEP) & 0xFFFFFFFF;
            ebuf[i] = b4 & 0xFF;
            ebuf[i + 1] = (b4 >> 8) & 0xFF;
            ebuf[i + 2] = (b4 >> 16) & 0xFF;
            ebuf[i + 3] = (b4 >> 24) & 0xFF;
         }
      }

      return 1;
   }

   return 0;
}

int eeprom_write(int slave, int start, int length)
{
   int i, dc = 0;
   uint16 aiadr, *wbuf;
   uint8 eepctl;

   if ((ctx.slavecount >= slave) && (slave > 0) && ((start + length) <= MAXBUF))
   {
      aiadr = 1 - slave;
      eepctl = 2;
      ecx_APWR(&ctx.port, aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl, EC_TIMEOUTRET); /* force Eeprom from PDI */
      eepctl = 0;
      ecx_APWR(&ctx.port, aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl, EC_TIMEOUTRET); /* set Eeprom to master */

      aiadr = 1 - slave;
      wbuf = (uint16 *)&ebuf[0];
      for (i = start; i < (start + length); i += 2)
      {
         ecx_writeeepromAP(&ctx, aiadr, i >> 1, *(wbuf + (i >> 1)), EC_TIMEOUTEEP);
         if (++dc >= 100)
         {
            dc = 0;
            printf(".");
            fflush(stdout);
         }
      }

      return 1;
   }

   return 0;
}

int eeprom_writealias(int slave, int alias, uint16 crc)
{
   uint16 aiadr;
   uint8 eepctl;
   int ret;

   if ((ctx.slavecount >= slave) && (slave > 0) && (alias <= 0xffff))
   {
      aiadr = 1 - slave;
      eepctl = 2;
      ecx_APWR(&ctx.port, aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl, EC_TIMEOUTRET); /* force Eeprom from PDI */
      eepctl = 0;
      ecx_APWR(&ctx.port, aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl, EC_TIMEOUTRET); /* set Eeprom to master */

      ret = ecx_writeeepromAP(&ctx, aiadr, 0x04, alias, EC_TIMEOUTEEP);
      if (ret)
         ret = ecx_writeeepromAP(&ctx, aiadr, 0x07, crc, EC_TIMEOUTEEP);

      return ret;
   }

   return 0;
}

void eepromtool(char *ifname, int slave, int mode, char *fname)
{
   int w, rc = 0, estart, esize;
   uint16 *wbuf;

   /* initialise SOEM, bind socket to ifname */
   if (ecx_init(&ctx, ifname))
   {
      printf("ecx_init on %s succeeded.\n", ifname);

      w = 0x0000;
      wkc = ecx_BRD(&ctx.port, 0x0000, ECT_REG_TYPE, sizeof(w), &w, EC_TIMEOUTSAFE); /* detect number of slaves */
      if (wkc > 0)
      {
         ctx.slavecount = wkc;

         printf("%d slaves found.\n", ctx.slavecount);
         if ((ctx.slavecount >= slave) && (slave > 0))
         {
            if ((mode == MODE_INFO) || (mode == MODE_READBIN) || (mode == MODE_READINTEL))
            {
               tstart = osal_current_time();
               eeprom_read(slave, 0x0000, MINBUF); // read first 128 bytes

               wbuf = (uint16 *)&ebuf[0];
               printf("Slave %d data\n", slave);
               printf(" PDI Control      : %4.4X\n", *(wbuf + 0x00));
               printf(" PDI Config       : %4.4X\n", *(wbuf + 0x01));
               printf(" Config Alias     : %4.4X\n", *(wbuf + 0x04));
               printf(" Checksum         : %4.4X\n", *(wbuf + 0x07));
               printf("   calculated     : %4.4X\n", SIIcrc(&ebuf[0]));
               printf(" Vendor ID        : %8.8X\n", *(uint32 *)(wbuf + 0x08));
               printf(" Product Code     : %8.8X\n", *(uint32 *)(wbuf + 0x0A));
               printf(" Revision Number  : %8.8X\n", *(uint32 *)(wbuf + 0x0C));
               printf(" Serial Number    : %8.8X\n", *(uint32 *)(wbuf + 0x0E));
               printf(" Mailbox Protocol : %4.4X\n", *(wbuf + 0x1C));
               esize = (*(wbuf + 0x3E) + 1) * 128;
               if (esize > MAXBUF) esize = MAXBUF;
               printf(" Size             : %4.4X = %d bytes\n", *(wbuf + 0x3E), esize);
               printf(" Version          : %4.4X\n", *(wbuf + 0x3F));
            }
            if ((mode == MODE_READBIN) || (mode == MODE_READINTEL))
            {
               if (esize > MINBUF)
                  eeprom_read(slave, MINBUF, esize - MINBUF); // read reminder

               tend = osal_current_time();
               osal_time_diff(&tstart, &tend, &tdif);
               if (mode == MODE_READINTEL) output_intelhex(fname, esize);
               if (mode == MODE_READBIN) output_bin(fname, esize);

               printf("\nTotal EEPROM read time :%dms\n", (int)(tdif.tv_sec * 1000 + tdif.tv_nsec / 1000000));
            }
            if ((mode == MODE_WRITEBIN) || (mode == MODE_WRITEINTEL))
            {
               estart = 0;
               if (mode == MODE_WRITEINTEL) rc = input_intelhex(fname, &estart, &esize);
               if (mode == MODE_WRITEBIN) rc = input_bin(fname, &esize);

               if (rc > 0)
               {
                  wbuf = (uint16 *)&ebuf[0];
                  printf("Slave %d\n", slave);
                  printf(" Vendor ID        : %8.8X\n", *(uint32 *)(wbuf + 0x08));
                  printf(" Product Code     : %8.8X\n", *(uint32 *)(wbuf + 0x0A));
                  printf(" Revision Number  : %8.8X\n", *(uint32 *)(wbuf + 0x0C));
                  printf(" Serial Number    : %8.8X\n", *(uint32 *)(wbuf + 0x0E));

                  printf("Busy");
                  fflush(stdout);
                  tstart = osal_current_time();
                  eeprom_write(slave, estart, esize);
                  tend = osal_current_time();
                  osal_time_diff(&tstart, &tend, &tdif);

                  printf("\nTotal EEPROM write time :%dms\n", (int)(tdif.tv_sec * 1000 + tdif.tv_nsec / 1000000));
               }
               else
                  printf("Error reading file, abort.\n");
            }
            if (mode == MODE_WRITEALIAS)
            {
               if (eeprom_read(slave, 0x0000, CRCBUF)) // read first 14 bytes
               {
                  wbuf = (uint16 *)&ebuf[0];
                  *(wbuf + 0x04) = alias;
                  if (eeprom_writealias(slave, alias, SIIcrc(&ebuf[0])))
                  {
                     printf("Alias %4.4X written successfully to slave %d\n", alias, slave);
                  }
                  else
                  {
                     printf("Alias not written\n");
                  }
               }
               else
               {
                  printf("Could not read slave EEPROM");
               }
            }
         }
         else
         {
            printf("Slave number outside range.\n");
         }
      }
      else
      {
         printf("No slaves found!\n");
      }
      printf("End, close socket\n");
      /* stop SOEM, close socket */
      ecx_close(&ctx);
   }
   else
   {
      printf("No socket connection on %s\nExcecute as root\n", ifname);
   }
}

int main(int argc, char *argv[])
{
   printf("SOEM (Simple Open EtherCAT Master)\nEEPROM tool\n");

   mode = MODE_NONE;
   if (argc > 3)
   {
      slave = atoi(argv[2]);
      if ((strncmp(argv[3], "-i", sizeof("-i")) == 0)) mode = MODE_INFO;
      if (argc > 4)
      {
         if ((strncmp(argv[3], "-r", sizeof("-r")) == 0)) mode = MODE_READBIN;
         if ((strncmp(argv[3], "-ri", sizeof("-ri")) == 0)) mode = MODE_READINTEL;
         if ((strncmp(argv[3], "-w", sizeof("-w")) == 0)) mode = MODE_WRITEBIN;
         if ((strncmp(argv[3], "-wi", sizeof("-wi")) == 0)) mode = MODE_WRITEINTEL;
         if ((strncmp(argv[3], "-walias", sizeof("-walias")) == 0))
         {
            mode = MODE_WRITEALIAS;
            alias = atoi(argv[4]);
         }
      }
      /* start tool */
      eepromtool(argv[1], slave, mode, argv[4]);
   }
   else
   {
      ec_adaptert *adapter = NULL;
      ec_adaptert *head = NULL;

      printf("Usage: eepromtool ifname slave OPTION fname|alias\n");
      printf("ifname = eth0 for example\n");
      printf("slave = slave number in EtherCAT order 1..n\n");
      printf("    -i      display EEPROM information\n");
      printf("    -walias write slave alias\n");
      printf("    -r      read EEPROM, output binary format\n");
      printf("    -ri     read EEPROM, output Intel Hex format\n");
      printf("    -w      write EEPROM, input binary format\n");
      printf("    -wi     write EEPROM, input Intel Hex format\n");

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
