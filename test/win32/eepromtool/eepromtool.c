/** \file
 * \brief EEprom tool for Simple Open EtherCAT master
 *
 * Usage : eepromtool ifname slave OPTION fname
 * ifname is NIC interface, f.e. eth0
 * slave = slave number in EtherCAT order 1..n
 * -r      read EEPROM, output binary format
 * -ri     read EEPROM, output Intel Hex format
 * -w      write EEPROM, input binary format
 * -wi     write EEPROM, input Intel Hex format
 *
 * (c)Arthur Ketels 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethercat.h"

#define MAXBUF 32768
#define STDBUF 2048
#define MINBUF 128

#define MODE_NONE         0
#define MODE_READBIN      1
#define MODE_READINTEL    2
#define MODE_WRITEBIN     3
#define MODE_WRITEINTEL   4

#define MAXSLENGTH        256

uint8 ebuf[MAXBUF];
uint8 ob;
uint16 ow;
int os;
int slave;
ec_timet tstart,tend, tdif;
int wkc;
int mode;
char sline[MAXSLENGTH];

#define IHEXLENGTH 0x20

int input_bin(char *fname, int *length)
{
   FILE *fp;

   int cc = 0, c;

   fp = fopen(fname, "rb");
   if(fp == NULL)
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
   if(fp == NULL)
      return 0;
   hstart = MAXBUF;
   hlength = 0;
   sum = 0;
   do
   {
      memset(sline, 0x00, MAXSLENGTH);
      sc = 0;
      while (((c = fgetc(fp)) != EOF) && (c != 0x0A) && (sc < (MAXSLENGTH -1)))
         sline[sc++] = (uint8)c;
      if ((c != EOF) && ((sc < 11) || (sline[0] != ':')))
      {
         c = EOF;
         retval = 0;
         printf("Invalid Intel Hex format.\n");
      }
      if (c != EOF)
      {
         sn = sscanf(sline , ":%2x%4x%2x", &ll, &ladr, &lt);
         if ((sn == 3) && ((ladr + ll) <= MAXBUF) && (lt == 0))
         {
            sum = ll + (ladr >> 8) + (ladr & 0xff) + lt;
            if(ladr < hstart) hstart = ladr;
            for(i = 0; i < ll ; i++)
            {
               sn = sscanf(&sline[9 + (i << 1)], "%2x", &lval);
               ebuf[ladr + i] = (uint8)lval;
               sum += (uint8)lval;
            }
            if(((ladr + ll) - hstart) > hlength)
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
   }
   while (c != EOF);
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
   if(fp == NULL)
      return 0;
   for (cc = 0 ; cc < length ; cc++)
      fputc( ebuf[cc], fp);
   fclose(fp);

   return 1;
}

int output_intelhex(char *fname, int length)
{
   FILE *fp;

   int cc = 0, ll, sum, i;

   fp = fopen(fname, "w");
   if(fp == NULL)
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
   int i, wkc, ainc = 4;
   uint16 estat, aiadr;
   uint32 b4;
   uint64 b8;
   uint8 eepctl;

   if((ec_slavecount >= slave) && (slave > 0) && ((start + length) <= MAXBUF))
   {
      aiadr = 1 - slave;
      eepctl = 2;
      wkc = ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET3); /* force Eeprom from PDI */
      eepctl = 0;
      wkc = ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET3); /* set Eeprom to master */

      estat = 0x0000;
      aiadr = 1 - slave;
      wkc=ec_APRD(aiadr, ECT_REG_EEPSTAT, sizeof(estat), &estat, EC_TIMEOUTRET3); /* read eeprom status */
      estat = etohs(estat);
      if (estat & EC_ESTAT_R64)
      {
         ainc = 8;
         for (i = start ; i < (start + length) ; i+=ainc)
         {
            b8 = ec_readeepromAP(aiadr, i >> 1 , EC_TIMEOUTEEP);
            ebuf[i] = (uint8) b8;
            ebuf[i+1] = (uint8) (b8 >> 8);
            ebuf[i+2] = (uint8) (b8 >> 16);
            ebuf[i+3] = (uint8) (b8 >> 24);
            ebuf[i+4] = (uint8) (b8 >> 32);
            ebuf[i+5] = (uint8) (b8 >> 40);
            ebuf[i+6] = (uint8) (b8 >> 48);
            ebuf[i+7] = (uint8) (b8 >> 56);
         }
      }
      else
      {
         for (i = start ; i < (start + length) ; i+=ainc)
         {
            b4 = (uint32)ec_readeepromAP(aiadr, i >> 1 , EC_TIMEOUTEEP);
            ebuf[i] = (uint8) b4;
            ebuf[i+1] = (uint8) (b4 >> 8);
            ebuf[i+2] = (uint8) (b4 >> 16);
            ebuf[i+3] = (uint8) (b4 >> 24);
         }
      }

      return 1;
   }

   return 0;
}

int eeprom_write(int slave, int start, int length)
{
   int i, wkc, dc = 0;
   uint16 aiadr, *wbuf;
   uint8 eepctl;
   int ret;

   if((ec_slavecount >= slave) && (slave > 0) && ((start + length) <= MAXBUF))
   {
      aiadr = 1 - slave;
      eepctl = 2;
      wkc = ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET3); /* force Eeprom from PDI */
      eepctl = 0;
      wkc = ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET3); /* set Eeprom to master */

      aiadr = 1 - slave;
      wbuf = (uint16 *)&ebuf[0];
      for (i = start ; i < (start + length) ; i+=2)
      {
         ret = ec_writeeepromAP(aiadr, i >> 1 , *(wbuf + (i >> 1)), EC_TIMEOUTEEP);
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

void eepromtool(char *ifname, int slave, int mode, char *fname)
{
   int w, rc = 0, estart, esize;
   uint16 *wbuf;

   /* initialise SOEM, bind socket to ifname */
   if (ec_init(ifname))
   {
      printf("ec_init on %s succeeded.\n",ifname);

      w = 0x0000;
       wkc = ec_BRD(0x0000, ECT_REG_TYPE, sizeof(w), &w, EC_TIMEOUTSAFE);      /* detect number of slaves */
       if (wkc > 0)
       {
         ec_slavecount = wkc;

         printf("%d slaves found.\n",ec_slavecount);
         if((ec_slavecount >= slave) && (slave > 0))
         {
            if ((mode == MODE_READBIN) || (mode == MODE_READINTEL))
            {
               tstart = osal_current_time();
               eeprom_read(slave, 0x0000, MINBUF); // read first 128 bytes

               wbuf = (uint16 *)&ebuf[0];
               printf("Slave %d data\n", slave);
               printf(" PDI Control      : %4.4X\n",*(wbuf + 0x00));
               printf(" PDI Config       : %4.4X\n",*(wbuf + 0x01));
               printf(" Config Alias     : %4.4X\n",*(wbuf + 0x04));
               printf(" Checksum         : %4.4X\n",*(wbuf + 0x07));
               printf(" Vendor ID        : %8.8X\n",*(uint32 *)(wbuf + 0x08));
               printf(" Product Code     : %8.8X\n",*(uint32 *)(wbuf + 0x0A));
               printf(" Revision Number  : %8.8X\n",*(uint32 *)(wbuf + 0x0C));
               printf(" Serial Number    : %8.8X\n",*(uint32 *)(wbuf + 0x0E));
               printf(" Mailbox Protocol : %4.4X\n",*(wbuf + 0x1C));
               esize = (*(wbuf + 0x3E) + 1) * 128;
               if (esize > MAXBUF) esize = MAXBUF;
               printf(" Size             : %4.4X = %d bytes\n",*(wbuf + 0x3E), esize);
               printf(" Version          : %4.4X\n",*(wbuf + 0x3F));

               if (esize > MINBUF)
                  eeprom_read(slave, MINBUF, esize - MINBUF); // read reminder

               tend = osal_current_time();
               osal_time_diff(&tstart, &tend, &tdif);
               if (mode == MODE_READINTEL) output_intelhex(fname, esize);
               if (mode == MODE_READBIN)    output_bin(fname, esize);

               printf("\nTotal EEPROM read time :%ldms\n", (tdif.usec+(tdif.sec*1000000L)) / 1000);
            }
            if ((mode == MODE_WRITEBIN) || (mode == MODE_WRITEINTEL))
            {
               estart = 0;
               if (mode == MODE_WRITEINTEL) rc = input_intelhex(fname, &estart, &esize);
               if (mode == MODE_WRITEBIN)     rc = input_bin(fname, &esize);

               if (rc > 0)
               {
                  wbuf = (uint16 *)&ebuf[0];
                  printf("Slave %d\n", slave);
                  printf(" Vendor ID        : %8.8X\n",*(uint32 *)(wbuf + 0x08));
                  printf(" Product Code     : %8.8X\n",*(uint32 *)(wbuf + 0x0A));
                  printf(" Revision Number  : %8.8X\n",*(uint32 *)(wbuf + 0x0C));
                  printf(" Serial Number    : %8.8X\n",*(uint32 *)(wbuf + 0x0E));

                  printf("Busy");
                  fflush(stdout);
                  tstart = osal_current_time();
                  eeprom_write(slave, estart, esize);
                  tend = osal_current_time();
                  osal_time_diff(&tstart, &tend, &tdif);

                  printf("\nTotal EEPROM write time :%ldms\n", (tdif.usec+(tdif.sec*1000000L)) / 1000);
               }
               else
                  printf("Error reading file, abort.\n");
            }
         }
         else
            printf("Slave number outside range.\n");
      }
      else
         printf("No slaves found!\n");
      printf("End, close socket\n");
      /* stop SOEM, close socket */
      ec_close();
   }
   else
      printf("No socket connection on %s\nExcecute as root\n",ifname);
}

int main(int argc, char *argv[])
{
   ec_adaptert * adapter = NULL;
   printf("SOEM (Simple Open EtherCAT Master)\nEEPROM tool\n");

   if (argc > 4)
   {
      slave = atoi(argv[2]);
      mode = MODE_NONE;
      if ((strncmp(argv[3], "-r", sizeof("-r")) == 0))   mode = MODE_READBIN;
      if ((strncmp(argv[3], "-ri", sizeof("-ri")) == 0)) mode = MODE_READINTEL;
      if ((strncmp(argv[3], "-w", sizeof("-w")) == 0))   mode = MODE_WRITEBIN;
      if ((strncmp(argv[3], "-wi", sizeof("-wi")) == 0)) mode = MODE_WRITEINTEL;

      /* start tool */
      eepromtool(argv[1],slave,mode,argv[4]);
   }
   else
   {
      printf("Usage: eepromtool ifname slave OPTION fname\n");
      printf("ifname = adapter name\n");
      printf("slave = slave number in EtherCAT order 1..n\n");
      printf("    -r      read EEPROM, output binary format\n");
      printf("    -ri     read EEPROM, output Intel Hex format\n");
      printf("    -w      write EEPROM, input binary format\n");
      printf("    -wi     write EEPROM, input Intel Hex format\n");
   	/* Print the list */
      printf ("Available adapters\n");
      adapter = ec_find_adapters ();
      while (adapter != NULL)
      {
         printf ("Description : %s, Device to use for wpcap: %s\n", adapter->desc,adapter->name);
         adapter = adapter->next;
      }
   }

   printf("End program\n");

   return (0);
}
