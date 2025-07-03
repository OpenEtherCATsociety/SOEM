/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : eni_test [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * This is a minimal test.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "soem/soem.h"

#define EC_TIMEOUTMON 500

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
boolean inOP;
uint8 currentgroup = 0;

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

extern ec_enit ec_eni;

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
    .ENI = &ec_eni,
    .FOEhook = NULL,
    .EOEhook = NULL,
    .manualstatechange = 0,
    .userdata = NULL,
};

void eni_test(char *ifname)
{
   int i, j, oloop, iloop, chk;
   needlf = FALSE;
   inOP = FALSE;

   printf("Starting eni test\n");

   /* initialise SOEM, bind socket to ifname */
   if (ecx_init(&ctx, ifname))
   {
      printf("ecx_init on %s succeeded.\n", ifname);

      /* find and auto-config slaves */
      if (ecx_config_init(&ctx, FALSE) > 0)
      {
         ecx_config_map_group(&ctx, &IOmap, 0);

         ecx_configdc(&ctx);

         printf("%d slaves found and configured.\n", ec_slavecount);

         /* wait for all slaves to reach SAFE_OP state */
         ecx_statecheck(&ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

         oloop = ec_group[0].Obytes;
         if (oloop > 8) oloop = 8;
         iloop = ec_group[0].Ibytes;
         if (iloop > 8) iloop = 8;

         printf("segments : %d : %d %d %d %d\n", ec_group[0].nsegments, ec_group[0].IOsegment[0], ec_group[0].IOsegment[1], ec_group[0].IOsegment[2], ec_group[0].IOsegment[3]);

         printf("Request operational state for all slaves\n");
         expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);
         ec_slave[0].state = EC_STATE_OPERATIONAL;
         /* send one valid process data to make outputs in slaves happy*/
         ecx_send_processdata(&ctx);
         ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
         /* request OP state for all slaves */
         ecx_writestate(&ctx, 0);
         chk = 200;
         /* wait for all slaves to reach OP state */
         do
         {
            ecx_send_processdata(&ctx);
            ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
            ecx_statecheck(&ctx, 0, EC_STATE_OPERATIONAL, 50000);
         } while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
         if (ec_slave[0].state == EC_STATE_OPERATIONAL)
         {
            printf("Operational state reached for all slaves.\n");
            inOP = TRUE;
            /* cyclic loop */
            for (i = 1; i <= 10000; i++)
            {
               ecx_send_processdata(&ctx);
               wkc = ecx_receive_processdata(&ctx, EC_TIMEOUTRET);

               if (wkc >= expectedWKC)
               {
                  printf("Processdata cycle %4d, WKC %d , O:", i, wkc);

                  for (j = 0; j < oloop; j++)
                  {
                     printf(" %2.2x", *(ec_slave[0].outputs + j));
                  }

                  printf(" I:");
                  for (j = 0; j < iloop; j++)
                  {
                     printf(" %2.2x", *(ec_slave[0].inputs + j));
                  }
                  printf(" T:%" PRId64 "\r", ec_DCtime);
                  needlf = TRUE;
               }
               osal_usleep(5000);
            }
            inOP = FALSE;
         }
         else
         {
            printf("Not all slaves reached operational state.\n");
            ecx_readstate(&ctx);
            for (i = 1; i <= ec_slavecount; i++)
            {
               if (ec_slave[i].state != EC_STATE_OPERATIONAL)
               {
                  printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                         i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
               }
            }
         }
         printf("\nRequest init state for all slaves\n");
         ec_slave[0].state = EC_STATE_INIT;
         /* request INIT state for all slaves */
         ecx_writestate(&ctx, 0);
      }
      else
      {
         printf("No slaves found!\n");
      }
      printf("End eni test, close socket\n");
      /* stop SOEM, close socket */
      ecx_close(&ctx);
   }
   else
   {
      printf("No socket connection on %s\nExecute as root\n", ifname);
   }
}

OSAL_THREAD_FUNC ecatcheck(void *ptr)
{
   int slave;
   (void)ptr; /* Not used */

   while (1)
   {
      if (inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
      {
         printf("check wkc=%d/%d docheckstate=%d!\n", wkc, expectedWKC, ec_group[currentgroup].docheckstate);
         if (needlf)
         {
            needlf = FALSE;
            printf("\n");
         }
         /* one ore more slaves are not responding */
         ec_group[currentgroup].docheckstate = FALSE;
         ecx_readstate(&ctx);
         for (slave = 1; slave <= ec_slavecount; slave++)
         {
            if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
            {
               ec_group[currentgroup].docheckstate = TRUE;
               if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
               {
                  printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                  ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                  ecx_writestate(&ctx, slave);
               }
               else if (ec_slave[slave].state == EC_STATE_SAFE_OP)
               {
                  printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                  ec_slave[slave].state = EC_STATE_OPERATIONAL;
                  if (ec_slave[slave].mbxhandlerstate == ECT_MBXH_LOST) ec_slave[slave].mbxhandlerstate = ECT_MBXH_CYCLIC;
                  ecx_writestate(&ctx, slave);
               }
               else if (ec_slave[slave].state > EC_STATE_NONE)
               {
                  if (ecx_reconfig_slave(&ctx, slave, EC_TIMEOUTMON) >= EC_STATE_PRE_OP)
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d reconfigured\n", slave);
                  }
               }
               else if (!ec_slave[slave].islost)
               {
                  /* re-check state */
                  ecx_statecheck(&ctx, slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                  if (ec_slave[slave].state == EC_STATE_NONE)
                  {
                     ec_slave[slave].islost = TRUE;
                     ec_slave[slave].mbxhandlerstate = ECT_MBXH_LOST;
                     /* zero input data for this slave */
                     if (ec_slave[slave].Ibytes)
                     {
                        memset(ec_slave[slave].inputs, 0x00, ec_slave[slave].Ibytes);
                        printf("zero inputs %p %d\n\r", ec_slave[slave].inputs, ec_slave[slave].Ibytes);
                     }
                     printf("ERROR : slave %d lost\n", slave);
                  }
               }
            }
            if (ec_slave[slave].islost)
            {
               if (ec_slave[slave].state <= EC_STATE_INIT)
               {
                  if (ecx_recover_slave(&ctx, slave, EC_TIMEOUTMON))
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d recovered\n", slave);
                  }
               }
               else
               {
                  ec_slave[slave].islost = FALSE;
                  printf("MESSAGE : slave %d found\n", slave);
               }
            }
         }
         if (!ec_group[currentgroup].docheckstate)
            printf("OK : all slaves resumed OPERATIONAL.\n");
      }
      osal_usleep(10000);
   }
}

int main(int argc, char *argv[])
{
   printf("SOEM (Simple Open EtherCAT Master)\nENI test\n");

   if (argc > 1)
   {
      /* create thread to handle slave error handling in OP */
      osal_thread_create(&thread1, 128000, &ecatcheck, NULL);
      /* start cyclic part */
      eni_test(argv[1]);
   }
   else
   {
      ec_adaptert *adapter = NULL;
      ec_adaptert *head = NULL;
      printf("Usage: eni_test ifname1\nifname = eth0 for example\n");

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
