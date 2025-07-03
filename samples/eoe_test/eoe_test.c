/** \file
 * \brief Example code for Simple Open EtherCAT master EoE
 *
 * This example creates a TAP device to send and receive Ethernet
 * packets over an EtherCAT network. It has been tested with an EL6601
 * Ethernet switch.
 *
 * This is a minimal test.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <linux/if_tun.h>

#include "soem/soem.h"

#define EC_TIMEOUTMON 500

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
OSAL_THREAD_HANDLE thread2;
int expectedWKC;
boolean needlf;
volatile int wkc;
boolean inOP;
uint8 currentgroup = 0;
boolean forceByteAlignment = FALSE;

#ifndef PACKET_LOG
#define PACKET_LOG 0
#endif

#if PACKET_LOG
#define LOG_PKT(dir, p, size)                     \
   printf("EOE(" dir ") "                         \
          "%02x:%02x:%02x:%02x:%02x:%02x "        \
          "%02x:%02x:%02x:%02x:%02x:%02x (%d)\n", \
          p[0], p[1], p[2], p[3], p[4], p[5],     \
          p[6], p[7], p[8], p[9], p[10], p[11],   \
          size);
#else
#define LOG_PKT(dir, p, size)
#endif

#define ETHERNET_FRAME_SIZE 1518
#define EOE_SLAVE           2

int tap;

/** Current RX fragment number */
uint8_t rxfragmentno = 0;
/** Complete RX frame size of current frame */
uint16_t rxframesize = 0;
/** Current RX data offset in frame */
uint16_t rxframeoffset = 0;
/** Current RX frame number */
uint16_t rxframeno = 0;

uint8_t rx[ETHERNET_FRAME_SIZE];
uint8_t tx[ETHERNET_FRAME_SIZE];

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

/** registered EoE hook */
int eoe_hook(ecx_contextt *context, uint16 slave, void *eoembx)
{
   (void)context;
   (void)slave;
   int wkc;

   /* Pass received Mbx data to EoE receive fragment function that
    * that will start/continue fill an Ethernet frame buffer
    */
   int size = ETHERNET_FRAME_SIZE;
   wkc = ecx_EOEreadfragment(
       eoembx,
       &rxfragmentno,
       &rxframesize,
       &rxframeoffset,
       &rxframeno,
       &size,
       rx);

   /* wkc == 1 would mean a frame is complete , last fragment flag have been set and all
    * other checks must have passed
    */
   if (wkc > 0)
   {
      LOG_PKT("rx", rx, size);
      int n = write(tap, rx, size);
      if (n < 0)
      {
         printf("TAP write failed (%d)\n", errno);
      }
   }

   /* No point in returning as unhandled */
   return 1;
}

OSAL_THREAD_FUNC mailbox_writer(void *arg)
{
   ecx_contextt *context = (ecx_contextt *)arg;

   for (;;)
   {
      int count = read(tap, tx, sizeof(tx));
      if (count < 0)
      {
         printf("Error reading from TAP device: %d\n", errno);
         continue;
      }
      LOG_PKT("tx", tx, count);

      /* Process the read data here */
      if (ecx_EOEsend(context, EOE_SLAVE, 0, count, tx, EC_TIMEOUTRXM))
      {
         // printf("EOE send returned WKC > 0\n");
      }
      else
      {
         printf("EOE send returned WKC == 0\n");
      }
   }
}

void test_eoe(ecx_contextt *context)
{
   memset(&tx, 0, sizeof(tx));
   memset(&rx, 0, sizeof(rx));

   /* Create the TAP device */

   tap = open("/dev/net/tun", O_RDWR);
   if (tap == -1)
      exit(1);

   struct ifreq ifr;
   memset(&ifr, 0, sizeof(ifr));
   ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
   strncpy(ifr.ifr_name, "tap0", IFNAMSIZ);
   int res = ioctl(tap, TUNSETIFF, &ifr);
   if (res == -1)
      exit(1);

   /* Set the HOOK */
   ecx_EOEdefinehook(context, eoe_hook);

   /* Create a asyncronous EoE reader */
   osal_thread_create(&thread2, 128000, &mailbox_writer, context);
}

void teststarter(char *ifname)
{
   int i, oloop, iloop, chk;
   needlf = FALSE;
   inOP = FALSE;

   printf("Starting eoe test\n");

   /* initialise SOEM, bind socket to ifname */
   if (ecx_init(&ctx, ifname))
   {
      printf("ecx_init on %s succeeded.\n", ifname);

      /* find and auto-config slaves */
      if (ecx_config_init(&ctx, FALSE) > 0)
      {
         if (forceByteAlignment)
         {
            ecx_config_map_group_aligned(&ctx, &IOmap, 0);
         }
         else
         {
            ecx_config_map_group(&ctx, &IOmap, 0);
         }

         ecx_configdc(&ctx);

         for (int si = 1; si <= ec_slavecount; si++)
         {
            if (ec_slave[si].CoEdetails > 0)
            {
               ecx_slavembxcyclic(&ctx, si);
               printf(" Slave added to cyclic mailbox handler\n");
            }
         }

         test_eoe(&ctx);

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
            for (i = 1; i <= 1000000; i++)
            {
               ecx_send_processdata(&ctx);
               wkc = ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
               ecx_mbxhandler(&ctx, 0, 4);

#if 1
               if (wkc >= expectedWKC)
               {
                  printf("Processdata cycle %4d, WKC %d , O:", i, wkc);

                  for (int j = 0; j < oloop; j++)
                  {
                     printf(" %2.2x", *(ec_slave[0].outputs + j));
                  }

                  printf(" I:");
                  for (int j = 0; j < iloop; j++)
                  {
                     printf(" %2.2x", *(ec_slave[0].inputs + j));
                  }
                  printf(" T:%" PRId64 "\r", ec_DCtime);
                  needlf = TRUE;
               }
#endif

               /* Lower the delay to serve the mailboxes faster and
                  increase the EoE speed. */
               osal_usleep(500);
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
      printf("End eoe test, close socket\n");
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
   printf("SOEM (Simple Open EtherCAT Master)\nEoE test\n");

   if (argc > 1)
   {
      /* create thread to handle slave error handling in OP */
      osal_thread_create(&thread1, 128000, &ecatcheck, NULL);

      /* start cyclic part */
      teststarter(argv[1]);
   }
   else
   {
      ec_adaptert *adapter = NULL;
      ec_adaptert *head = NULL;
      printf("Usage: simple_test ifname1\nifname = eth0 for example\n");

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
