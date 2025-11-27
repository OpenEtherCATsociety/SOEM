/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
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

/* Log EoE packets if set, process data otherwise */
#ifndef PACKET_LOG
#define PACKET_LOG 0
#endif

#define ETHERNET_FRAME_SIZE 1518
#define EC_TIMEOUTMON       500

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

static uint8 IOmap[4096];
static OSAL_THREAD_HANDLE thread1;
static OSAL_THREAD_HANDLE thread2;
static int expectedWKC;
static boolean needlf;
static volatile int wkc;
static boolean inOP;
static uint8 currentgroup = 0;
static int tap;
static int eoe_slave;

/** Current RX fragment number */
static uint8_t rxfragmentno = 0;
/** Complete RX frame size of current frame */
static uint16_t rxframesize = 0;
/** Current RX data offset in frame */
static uint16_t rxframeoffset = 0;
/** Current RX frame number */
static uint16_t rxframeno = 0;

static uint8_t rx[ETHERNET_FRAME_SIZE];
static uint8_t tx[ETHERNET_FRAME_SIZE];

static ecx_contextt ctx;

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
      int wkc = ecx_EOEsend(context, eoe_slave, 0, count, tx, EC_TIMEOUTRXM);
      if (wkc <= 0)
      {
         printf("EOE send failure (wkc=%d)\n", wkc);
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

   /* Create an asynchronous EoE reader */
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
      if (ecx_config_init(&ctx) > 0)
      {
         ec_groupt *group = &ctx.grouplist[0];

         ecx_config_map_group(&ctx, IOmap, 0);

         ecx_configdc(&ctx);

         for (int si = 1; si <= ctx.slavecount; si++)
         {
            if (ctx.slavelist[si].CoEdetails > 0)
            {
               ecx_slavembxcyclic(&ctx, si);
               printf(" Slave %d added to cyclic mailbox handler\n", si);
            }
         }

         test_eoe(&ctx);

         printf("%d slaves found and configured.\n", ctx.slavecount);

         /* wait for all slaves to reach SAFE_OP state */
         ecx_statecheck(&ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

         oloop = group->Obytes;
         if (oloop > 8) oloop = 8;
         iloop = group->Ibytes;
         if (iloop > 8) iloop = 8;

         printf("segments : %d : %d %d %d %d\n", group->nsegments, group->IOsegment[0], group->IOsegment[1], group->IOsegment[2], group->IOsegment[3]);

         printf("Request operational state for all slaves\n");
         expectedWKC = (group->outputsWKC * 2) + group->inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);
         ctx.slavelist[0].state = EC_STATE_OPERATIONAL;
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
         } while (chk-- && (ctx.slavelist[0].state != EC_STATE_OPERATIONAL));
         if (ctx.slavelist[0].state == EC_STATE_OPERATIONAL)
         {
            printf("Operational state reached for all slaves.\n");
            inOP = TRUE;

            /* cyclic loop */
            for (i = 1; i <= 1000000; i++)
            {
               ecx_send_processdata(&ctx);
               wkc = ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
               ecx_mbxhandler(&ctx, 0, 4);

#if PACKET_LOG == 0
               if (wkc >= expectedWKC)
               {
                  printf("Processdata cycle %4d, WKC %d , O:", i, wkc);

                  for (int j = 0; j < oloop; j++)
                  {
                     printf(" %2.2x", *(ctx.slavelist[0].outputs + j));
                  }

                  printf(" I:");
                  for (int j = 0; j < iloop; j++)
                  {
                     printf(" %2.2x", *(ctx.slavelist[0].inputs + j));
                  }
                  printf(" T:%" PRId64 "\r", ctx.DCtime);
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
            for (i = 1; i <= ctx.slavecount; i++)
            {
               if (ctx.slavelist[i].state != EC_STATE_OPERATIONAL)
               {
                  printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                         i, ctx.slavelist[i].state, ctx.slavelist[i].ALstatuscode, ec_ALstatuscode2string(ctx.slavelist[i].ALstatuscode));
               }
            }
         }
         printf("\nRequest init state for all slaves\n");
         ctx.slavelist[0].state = EC_STATE_INIT;
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
   int slaveix;
   (void)ptr; /* Not used */

   while (1)
   {
      if (inOP && ((wkc < expectedWKC) || ctx.grouplist[currentgroup].docheckstate))
      {
         printf("check wkc=%d/%d docheckstate=%d!\n", wkc, expectedWKC, ctx.grouplist[currentgroup].docheckstate);
         if (needlf)
         {
            needlf = FALSE;
            printf("\n");
         }
         /* one or more slaves are not responding */
         ctx.grouplist[currentgroup].docheckstate = FALSE;
         ecx_readstate(&ctx);
         for (slaveix = 1; slaveix <= ctx.slavecount; slaveix++)
         {
            ec_slavet *slave = &ctx.slavelist[slaveix];

            if ((slave->group == currentgroup) && (slave->state != EC_STATE_OPERATIONAL))
            {
               ctx.grouplist[currentgroup].docheckstate = TRUE;
               if (slave->state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
               {
                  printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slaveix);
                  slave->state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                  ecx_writestate(&ctx, slaveix);
               }
               else if (slave->state == EC_STATE_SAFE_OP)
               {
                  printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slaveix);
                  slave->state = EC_STATE_OPERATIONAL;
                  if (slave->mbxhandlerstate == ECT_MBXH_LOST) slave->mbxhandlerstate = ECT_MBXH_CYCLIC;
                  ecx_writestate(&ctx, slaveix);
               }
               else if (slave->state > EC_STATE_NONE)
               {
                  if (ecx_reconfig_slave(&ctx, slaveix, EC_TIMEOUTMON) >= EC_STATE_PRE_OP)
                  {
                     slave->islost = FALSE;
                     printf("MESSAGE : slave %d reconfigured\n", slaveix);
                  }
               }
               else if (!slave->islost)
               {
                  /* re-check state */
                  ecx_statecheck(&ctx, slaveix, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                  if (slave->state == EC_STATE_NONE)
                  {
                     slave->islost = TRUE;
                     slave->mbxhandlerstate = ECT_MBXH_LOST;
                     /* zero input data for this slave */
                     if (slave->Ibytes)
                     {
                        memset(slave->inputs, 0x00, slave->Ibytes);
                        printf("zero inputs %p %d\n\r", slave->inputs, slave->Ibytes);
                     }
                     printf("ERROR : slave %d lost\n", slaveix);
                  }
               }
            }
            if (slave->islost)
            {
               if (slave->state <= EC_STATE_INIT)
               {
                  if (ecx_recover_slave(&ctx, slaveix, EC_TIMEOUTMON))
                  {
                     slave->islost = FALSE;
                     printf("MESSAGE : slave %d recovered\n", slaveix);
                  }
               }
               else
               {
                  slave->islost = FALSE;
                  printf("MESSAGE : slave %d found\n", slaveix);
               }
            }
         }
         if (!ctx.grouplist[currentgroup].docheckstate)
            printf("OK : all slaves resumed OPERATIONAL.\n");
      }
      osal_usleep(10000);
   }
}

int main(int argc, char *argv[])
{
   printf("SOEM (Simple Open EtherCAT Master)\nEoE test\n");

   if (argc > 2)
   {
      eoe_slave = atoi(argv[2]);

      /* create thread to handle slave error handling in OP */
      osal_thread_create(&thread1, 128000, &ecatcheck, NULL);

      /* start cyclic part */
      teststarter(argv[1]);
   }
   else
   {
      ec_adaptert *adapter = NULL;
      ec_adaptert *head = NULL;
      printf("Usage: eoe_test ifname1 slave\n");
      printf("ifname = eth0 for example\n");
      printf("slave = EoE slave number in EtherCAT order 1..n\n");

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
