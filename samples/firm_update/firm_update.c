/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage: firm_update ifname1 slave fname
 * ifname is NIC interface, f.e. eth0
 * slave = slave number in EtherCAT order 1..n
 * fname = binary file to store in slave
 * CAUTION! Using the wrong file can result in a bricked slave!
 *
 * This is a slave firmware update test.
 *
 * (c)Arthur Ketels 2011
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

static ec_slavet ec_slave[EC_MAXSLAVE];
static int ec_slavecount;
static ec_groupt ec_group[EC_MAXGROUP];
static uint8 ec_esibuf[EC_MAXEEPBUF];
static uint32 ec_esimap[EC_MAXEEPBITMAP];
static ec_eringt ec_elist;
static ec_idxstackT ec_idxstack;
static ec_SMcommtypet ec_SMcommtype[EC_MAX_MAPT];
static ec_PDOassignt ec_PDOassign[EC_MAX_MAPT];
static ec_PDOdesct ec_PDOdesc[EC_MAX_MAPT];
static ec_eepromSMt ec_SM;
static ec_eepromFMMUt ec_FMMU;
static boolean EcatError = FALSE;
static int64 ec_DCtime;
static ecx_portt ecx_port;
static ec_mbxpoolt ec_mbxpool;

static ecx_contextt ctx = {
    .port = &ecx_port,
    .slavelist = &ec_slave[0],
    .slavecount = &ec_slavecount,
    .maxslave = EC_MAXSLAVE,
    .grouplist = &ec_group[0],
    .maxgroup = EC_MAXGROUP,
    .esibuf = &ec_esibuf[0],
    .esimap = &ec_esimap[0],
    .esislave = 0,
    .elist = &ec_elist,
    .idxstack = &ec_idxstack,
    .ecaterror = &EcatError,
    .DCtime = &ec_DCtime,
    .SMcommtype = &ec_SMcommtype[0],
    .PDOassign = &ec_PDOassign[0],
    .PDOdesc = &ec_PDOdesc[0],
    .eepSM = &ec_SM,
    .eepFMMU = &ec_FMMU,
    .mbxpool = &ec_mbxpool,
    .FOEhook = NULL,
    .EOEhook = NULL,
    .manualstatechange = 0,
    .userdata = NULL,
};

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
         printf("%d slaves found and configured.\n", ec_slavecount);

         /* wait for all slaves to reach PRE_OP state */
         ecx_statecheck(&ctx, 0, EC_STATE_PRE_OP, EC_TIMEOUTSTATE * 4);

         printf("Request init state for slave %d\n", slave);
         ec_slave[slave].state = EC_STATE_INIT;
         ecx_writestate(&ctx, slave);

         /* wait for slave to reach INIT state */
         ecx_statecheck(&ctx, slave, EC_STATE_INIT, EC_TIMEOUTSTATE * 4);
         printf("Slave %d state to INIT.\n", slave);

         /* read BOOT mailbox data, master -> slave */
         data = ecx_readeeprom(&ctx, slave, ECT_SII_BOOTRXMBX, EC_TIMEOUTEEP);
         ec_slave[slave].SM[0].StartAddr = (uint16)LO_WORD(data);
         ec_slave[slave].SM[0].SMlength = (uint16)HI_WORD(data);
         /* store boot write mailbox address */
         ec_slave[slave].mbx_wo = (uint16)LO_WORD(data);
         /* store boot write mailbox size */
         ec_slave[slave].mbx_l = (uint16)HI_WORD(data);

         /* read BOOT mailbox data, slave -> master */
         data = ecx_readeeprom(&ctx, slave, ECT_SII_BOOTTXMBX, EC_TIMEOUTEEP);
         ec_slave[slave].SM[1].StartAddr = (uint16)LO_WORD(data);
         ec_slave[slave].SM[1].SMlength = (uint16)HI_WORD(data);
         /* store boot read mailbox address */
         ec_slave[slave].mbx_ro = (uint16)LO_WORD(data);
         /* store boot read mailbox size */
         ec_slave[slave].mbx_rl = (uint16)HI_WORD(data);

         printf(" SM0 A:%4.4x L:%4d F:%8.8x\n", ec_slave[slave].SM[0].StartAddr, ec_slave[slave].SM[0].SMlength,
                (int)ec_slave[slave].SM[0].SMflags);
         printf(" SM1 A:%4.4x L:%4d F:%8.8x\n", ec_slave[slave].SM[1].StartAddr, ec_slave[slave].SM[1].SMlength,
                (int)ec_slave[slave].SM[1].SMflags);
         /* program SM0 mailbox in for slave */
         ecx_FPWR(ctx.port, ec_slave[slave].configadr, ECT_REG_SM0, sizeof(ec_smt), &ec_slave[slave].SM[0], EC_TIMEOUTRET);
         /* program SM1 mailbox out for slave */
         ecx_FPWR(ctx.port, ec_slave[slave].configadr, ECT_REG_SM1, sizeof(ec_smt), &ec_slave[slave].SM[1], EC_TIMEOUTRET);

         printf("Request BOOT state for slave %d\n", slave);
         ec_slave[slave].state = EC_STATE_BOOT;
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
               ec_slave[slave].state = EC_STATE_INIT;
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
