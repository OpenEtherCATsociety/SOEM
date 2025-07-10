/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : red_test [ifname1] [ifname2] [cycletime]
 * ifname is NIC interface, f.e. eth0
 * cycletime in us, f.e. 500
 *
 * This is a redundancy test.
 *
 * (c)Arthur Ketels 2008
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "soem/soem.h"

#define EC_TIMEOUTMON 500
#define NSEC_PER_SEC  1000000000

static uint8 IOmap[4096];
static OSAL_THREAD_HANDLE thread1;
static OSAL_THREAD_HANDLE thread2;
static int expectedWKC;
static boolean needlf;
static volatile int wkc;
static boolean inOP;
static uint8 currentgroup = 0;
static uint8 *digout = NULL;
static int dorun = 0;
static int64 toff, gl_delta;

static ecx_contextt ctx;

void redtest(char *ifname, char *ifname2)
{
   int cnt, i, j, oloop, iloop;

   printf("Starting Redundant test\n");

   /* initialise SOEM, bind socket to ifname */
   (void)ifname2;
   //   if (ecx_init_redundant(&ctx, ifname, ifname2))
   if (ecx_init(&ctx, ifname))
   {
      printf("ecx_init on %s succeeded.\n", ifname);

      /* find and auto-config slaves */
      if (ecx_config_init(&ctx) > 0)
      {
         ec_groupt *group = &ctx.grouplist[0];

         ecx_config_map_group(&ctx, IOmap, 0);

         printf("%d slaves found and configured.\n", ctx.slavecount);

         /* wait for all slaves to reach SAFE_OP state */
         ecx_statecheck(&ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);

         ecx_configdc(&ctx);

         /* read indevidual slave state and store in ctx.slavelist[] */
         ecx_readstate(&ctx);
         for (cnt = 1; cnt <= ctx.slavecount; cnt++)
         {
            printf("Slave:%d Name:%s Output size:%3dbits Input size:%3dbits State:%2d delay:%d.%d\n",
                   cnt, ctx.slavelist[cnt].name, ctx.slavelist[cnt].Obits, ctx.slavelist[cnt].Ibits,
                   ctx.slavelist[cnt].state, (int)ctx.slavelist[cnt].pdelay, ctx.slavelist[cnt].hasdc);
            printf("         Out:%p,%4d In:%p,%4d\n",
                   ctx.slavelist[cnt].outputs, ctx.slavelist[cnt].Obytes, ctx.slavelist[cnt].inputs, ctx.slavelist[cnt].Ibytes);
            /* check for EL2004 or EL2008 */
            if (!digout && ((ctx.slavelist[cnt].eep_id == 0x0af83052) || (ctx.slavelist[cnt].eep_id == 0x07d83052)))
            {
               digout = ctx.slavelist[cnt].outputs;
            }
         }
         expectedWKC = (group->outputsWKC * 2) + group->inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);

         printf("Request operational state for all slaves\n");
         ctx.slavelist[0].state = EC_STATE_OPERATIONAL;
         /* request OP state for all slaves */
         ecx_writestate(&ctx, 0);
         /* activate cyclic process data */
         dorun = 1;
         /* wait for all slaves to reach OP state */
         ecx_statecheck(&ctx, 0, EC_STATE_OPERATIONAL, 5 * EC_TIMEOUTSTATE);

         oloop = group->Obytes;
         if (oloop > 8) oloop = 8;
         iloop = group->Ibytes;
         if (iloop > 8) iloop = 8;

         if (ctx.slavelist[0].state == EC_STATE_OPERATIONAL)
         {
            printf("Operational state reached for all slaves.\n");
            inOP = TRUE;
            /* acyclic loop 5000 x 20ms = 10s */
            for (i = 1; i <= 5000; i++)
            {
               printf("Processdata cycle %5d , Wck %3d, DCtime %12" PRId64 ", dt %12" PRId64 ", O:",
                      dorun, wkc, ctx.DCtime, gl_delta);
               for (j = 0; j < oloop; j++)
               {
                  printf(" %2.2x", *(ctx.slavelist[0].outputs + j));
               }
               printf(" I:");
               for (j = 0; j < iloop; j++)
               {
                  printf(" %2.2x", *(ctx.slavelist[0].inputs + j));
               }
               printf("\r");
               fflush(stdout);
               osal_usleep(20000);
            }
            dorun = 0;
            inOP = FALSE;
         }
         else
         {
            printf("Not all slaves reached operational state.\n");
            ecx_readstate(&ctx);
            for (i = 1; i <= ctx.slavecount; i++)
            {
               ec_slavet *slave = &ctx.slavelist[i];
               if (slave->state != EC_STATE_OPERATIONAL)
               {
                  printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                         i, slave->state, slave->ALstatuscode, ec_ALstatuscode2string(slave->ALstatuscode));
               }
            }
         }
         printf("Request safe operational state for all slaves\n");
         ctx.slavelist[0].state = EC_STATE_SAFE_OP;
         /* request SAFE_OP state for all slaves */
         ecx_writestate(&ctx, 0);
      }
      else
      {
         printf("No slaves found!\n");
      }
      printf("End redundant test, close socket\n");
      /* stop SOEM, close socket */
      ecx_close(&ctx);
   }
   else
   {
      printf("No socket connection on %s\nExcecute as root\n", ifname);
   }
}

/* add ns to ec_timet */
void add_time_ns(ec_timet *ts, int64 addtime)
{
   ec_timet addts;

   addts.tv_nsec = addtime % NSEC_PER_SEC;
   addts.tv_sec = (addtime - addts.tv_nsec) / NSEC_PER_SEC;
   osal_timespecadd(ts, &addts, ts);
}

/* PI calculation to get linux time synced to DC time */
void ec_sync(int64 reftime, int64 cycletime, int64 *offsettime)
{
   static int64 integral = 0;
   int64 delta;
   /* set linux sync point 50us later than DC sync, just as example */
   delta = (reftime - 50000) % cycletime;
   if (delta > (cycletime / 2))
   {
      delta = delta - cycletime;
   }
   if (delta > 0)
   {
      integral++;
   }
   if (delta < 0)
   {
      integral--;
   }
   *offsettime = -(delta / 100) - (integral / 20);
   gl_delta = delta;
}

/* RT EtherCAT thread */
OSAL_THREAD_FUNC_RT ecatthread(void *ptr)
{
   ec_timet ts;
   int ht;
   int64 cycletime;

   osal_get_monotonic_time(&ts);
   ht = (ts.tv_nsec / 1000000) + 1; /* round to nearest ms */
   ts.tv_nsec = ht * 1000000;
   if (ts.tv_nsec >= NSEC_PER_SEC)
   {
      ts.tv_sec++;
      ts.tv_nsec -= NSEC_PER_SEC;
   }
   cycletime = *(int *)ptr * 1000; /* cycletime in ns */
   toff = 0;
   dorun = 0;
   ecx_send_processdata(&ctx);
   while (1)
   {
      /* calculate next cycle start */
      add_time_ns(&ts, cycletime + toff);
      /* wait to cycle start */
      osal_monotonic_sleep(&ts);
      if (dorun > 0)
      {
         wkc = ecx_receive_processdata(&ctx, EC_TIMEOUTRET);

         dorun++;
         /* if we have some digital output, cycle */
         if (digout) *digout = (uint8)((dorun / 16) & 0xff);

         if (ctx.slavelist[0].hasdc)
         {
            /* calculate toff to get linux time and DC synced */
            ec_sync(ctx.DCtime, cycletime, &toff);
         }
         ecx_send_processdata(&ctx);
      }
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
   int ctime;

   printf("SOEM (Simple Open EtherCAT Master)\nRedundancy test\n");

   if (argc > 3)
   {
      dorun = 0;
      ctime = atoi(argv[3]);

      /* create RT thread */
      osal_thread_create_rt(&thread1, 128000, &ecatthread, (void *)&ctime);

      /* create thread to handle slave error handling in OP */
      osal_thread_create(&thread2, 128000, &ecatcheck, NULL);

      /* start acyclic part */
      redtest(argv[1], argv[2]);
   }
   else
   {
      ec_adaptert *adapter = NULL;
      ec_adaptert *head = NULL;
      printf("Usage: red_test ifname1 ifname2 cycletime\nifname = eth0 for example\ncycletime in us\n");

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
