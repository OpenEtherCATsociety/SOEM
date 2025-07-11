/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "soem/soem.h"

#define FWBUFSIZE (8 * 1024 * 1024)

uint8 ob;
uint16 ow;
uint32 data;
char filename[256];
char filebuffer[FWBUFSIZE]; // 8MB buffer
int filesize;
int j;
uint16 argslave;
boolean forceByteAlignment = FALSE;

static ecx_contextt ctx;

int input_bin(char *fname, int *length)
{
   FILE *fp;

   int cc = 0, c;

   fp = fopen(fname, "rb");
   if (fp == NULL)
      return 0;
   while (((c = fgetc(fp)) != EOF) && (cc < FWBUFSIZE))
      filebuffer[cc++] = (uint8)c;
   *length = cc;
   fclose(fp);
   return 1;
}

void boottest(char *ifname, uint16 slave, char *filename)
{
   printf("Starting firmware update example\n");

   /* initialise SOEM, bind socket to ifname */
   if (ecx_init(&ctx, ifname))
   {
      printf("ecx_init on %s succeeded.\n", ifname);

      /* find and auto-config slaves */
      if (ecx_config_init(&ctx) > 0)
      {
         printf("%d slaves found and configured.\n", ctx.slavecount);

         /* wait for all slaves to reach PRE_OP state */
         ecx_statecheck(&ctx, 0, EC_STATE_PRE_OP, EC_TIMEOUTSTATE * 4);

         printf("Request init state for slave %d\n", slave);
         ctx.slavelist[slave].state = EC_STATE_INIT;
         ecx_writestate(&ctx, slave);

         /* wait for slave to reach INIT state */
         ecx_statecheck(&ctx, slave, EC_STATE_INIT, EC_TIMEOUTSTATE * 4);
         printf("Slave %d state to INIT.\n", slave);

         /* read BOOT mailbox data, master -> slave */
         data = ecx_readeeprom(&ctx, slave, ECT_SII_BOOTRXMBX, EC_TIMEOUTEEP);
         ctx.slavelist[slave].SM[0].StartAddr = (uint16)LO_WORD(data);
         ctx.slavelist[slave].SM[0].SMlength = (uint16)HI_WORD(data);
         /* store boot write mailbox address */
         ctx.slavelist[slave].mbx_wo = (uint16)LO_WORD(data);
         /* store boot write mailbox size */
         ctx.slavelist[slave].mbx_l = (uint16)HI_WORD(data);

         /* read BOOT mailbox data, slave -> master */
         data = ecx_readeeprom(&ctx, slave, ECT_SII_BOOTTXMBX, EC_TIMEOUTEEP);
         ctx.slavelist[slave].SM[1].StartAddr = (uint16)LO_WORD(data);
         ctx.slavelist[slave].SM[1].SMlength = (uint16)HI_WORD(data);
         /* store boot read mailbox address */
         ctx.slavelist[slave].mbx_ro = (uint16)LO_WORD(data);
         /* store boot read mailbox size */
         ctx.slavelist[slave].mbx_rl = (uint16)HI_WORD(data);

         printf(" SM0 A:%4.4x L:%4d F:%8.8x\n", ctx.slavelist[slave].SM[0].StartAddr, ctx.slavelist[slave].SM[0].SMlength,
                (int)ctx.slavelist[slave].SM[0].SMflags);
         printf(" SM1 A:%4.4x L:%4d F:%8.8x\n", ctx.slavelist[slave].SM[1].StartAddr, ctx.slavelist[slave].SM[1].SMlength,
                (int)ctx.slavelist[slave].SM[1].SMflags);
         /* program SM0 mailbox in for slave */
         ecx_FPWR(&ctx.port, ctx.slavelist[slave].configadr, ECT_REG_SM0, sizeof(ec_smt), &ctx.slavelist[slave].SM[0], EC_TIMEOUTRET);
         /* program SM1 mailbox out for slave */
         ecx_FPWR(&ctx.port, ctx.slavelist[slave].configadr, ECT_REG_SM1, sizeof(ec_smt), &ctx.slavelist[slave].SM[1], EC_TIMEOUTRET);

         printf("Request BOOT state for slave %d\n", slave);
         ctx.slavelist[slave].state = EC_STATE_BOOT;
         ecx_writestate(&ctx, slave);

         /* wait for slave to reach BOOT state */
         if (ecx_statecheck(&ctx, slave, EC_STATE_BOOT, EC_TIMEOUTSTATE * 10) == EC_STATE_BOOT)
         {
            printf("Slave %d state to BOOT.\n", slave);

            if (input_bin(filename, &filesize))
            {
               printf("File read OK, %d bytes.\n", filesize);
               printf("FoE write....");
               j = ecx_FOEwrite(&ctx, slave, filename, 0, filesize, &filebuffer, EC_TIMEOUTSTATE);
               printf("result %d.\n", j);
               printf("Request init state for slave %d\n", slave);
               ctx.slavelist[slave].state = EC_STATE_INIT;
               ecx_writestate(&ctx, slave);
            }
            else
               printf("File not read OK.\n");
         }
      }
      else
      {
         printf("No slaves found!\n");
      }
      printf("End firmware update example, close socket\n");
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
   printf("SOEM (Simple Open EtherCAT Master)\nFirmware update example\n");

   if (argc > 3)
   {
      argslave = atoi(argv[2]);
      boottest(argv[1], argslave, argv[3]);
   }
   else
   {
      ec_adaptert *adapter = NULL;
      ec_adaptert *head = NULL;
      printf("Usage: firm_update ifname1 slave fname\n");
      printf("ifname = eth0 for example\n");
      printf("slave = slave number in EtherCAT order 1..n\n");
      printf("fname = binary file to store in slave\n");
      printf("CAUTION! Using the wrong file can result in a bricked slave!\n");

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
