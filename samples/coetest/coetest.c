/** \file
 * \brief CoE example code for Simple Open EtherCAT master
 *
 * Usage : coetest [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * (c)Arthur Ketels 2010 - 2024
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "soem/soem.h"

#define EC_TIMEOUTMON 500
#define NSEC_PER_SEC  1000000000

static uint8 IOmap[4096];
static OSAL_THREAD_HANDLE threadrt, thread1;
static int expectedWKC;
static int mappingdone, dorun, inOP, dowkccheck;
static int adapterisbound, conf_io_size, currentgroup;
static int64_t cycletime = 1000000;

static ecx_contextt ctx;

/* add ns to ec_timet */
void add_time_ns(ec_timet *ts, int64 addtime)
{
   ec_timet addts;

   addts.tv_nsec = addtime % NSEC_PER_SEC;
   addts.tv_sec = (addtime - addts.tv_nsec) / NSEC_PER_SEC;
   osal_timespecadd(ts, &addts, ts);
}

static float pgain = 0.01f;
static float igain = 0.00002f;
/* set linux sync point 500us later than DC sync, just as example */
static int64 syncoffset = 500000;
int64 timeerror;

/* PI calculation to get linux time synced to DC time */
void ec_sync(int64 reftime, int64 cycletime, int64 *offsettime)
{
   static int64 integral = 0;
   int64 delta;
   delta = (reftime - syncoffset) % cycletime;
   if (delta > (cycletime / 2))
   {
      delta = delta - cycletime;
   }
   timeerror = -delta;
   integral += timeerror;
   *offsettime = (int64)((timeerror * pgain) + (integral * igain));
}

/* RT EtherCAT thread */
OSAL_THREAD_FUNC_RT ecatthread(void)
{
   ec_timet ts;
   int ht, wkc;
   static int64_t toff = 0;

   dorun = 0;
   while (!mappingdone)
   {
      osal_usleep(100);
   }
   osal_get_monotonic_time(&ts);
   ht = (ts.tv_nsec / 1000000) + 1; /* round to nearest ms */
   ts.tv_nsec = ht * 1000000;
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
         if (wkc != expectedWKC)
            dowkccheck++;
         else
            dowkccheck = 0;
         if (ctx.slavelist[0].hasdc && (wkc > 0))
         {
            /* calulate toff to get linux time and DC synced */
            ec_sync(ctx.DCtime, cycletime, &toff);
         }
         ecx_mbxhandler(&ctx, 0, 4);
         ecx_send_processdata(&ctx);
      }
   }
}

OSAL_THREAD_FUNC ecatcheck(void)
{
   int slaveix;

   while (1)
   {
      if (inOP && ((dowkccheck > 2) || ctx.grouplist[currentgroup].docheckstate))
      {
         /* one ore more slaves are not responding */
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
         dowkccheck = 0;
      }
      osal_usleep(10000);
   }
}

void ethercatstartup(char *ifname)
{
   printf("EtherCAT Startup\n");
   int rv = ecx_init(&ctx, ifname);
   if (rv)
   {
      adapterisbound = 1;
      ecx_config_init(&ctx);
      if (ctx.slavecount > 0)
      {
         ec_groupt *group = &ctx.grouplist[0];

         conf_io_size = ecx_config_map_group(&ctx, IOmap, 0);
         expectedWKC = (group->outputsWKC * 2) + group->inputsWKC;
         mappingdone = 1;
         ecx_configdc(&ctx);
         int sdoslave = -1;
         for (int si = 1; si <= ctx.slavecount; si++)
         {
            ec_slavet *slave = &ctx.slavelist[si];
            printf("Slave %d name:%s man:%8.8x id:%8.8x rev:%d ser:%d\n", si, slave->name, slave->eep_man, slave->eep_id, slave->eep_rev, slave->eep_ser);
            if (slave->CoEdetails > 0)
            {
               ecx_slavembxcyclic(&ctx, si);
               sdoslave = si;
               printf(" Slave added to cyclic mailbox handler\n");
            }
         }
         dorun = 1;
         osal_usleep(1000000);
         ctx.slavelist[0].state = EC_STATE_OPERATIONAL;
         ecx_writestate(&ctx, 0);
         ecx_statecheck(&ctx, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);
         inOP = 1;
         printf("EtherCAT OP\n");
         uint32_t aval = 0;
         int avals = sizeof(aval);
         for (int i = 0; i < 100; i++)
         {
            printf("Cycle %d timeerror %" PRId64 "\n", i, timeerror);
            if (sdoslave > 0)
            {
               aval = 0;
               int rval = ecx_SDOread(&ctx, sdoslave, 0x1018, 0x02, FALSE, &avals, &aval, EC_TIMEOUTRXM);
               printf(" Slave %d Rval %d Prodcode %8.8x\n", sdoslave, rval, aval);
            }
            osal_usleep(100000);
         }
         inOP = 0;
         printf("EtherCAT to safe-OP\n");
         ctx.slavelist[0].state = EC_STATE_SAFE_OP;
         ecx_writestate(&ctx, 0);
         ecx_statecheck(&ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);
         printf("EtherCAT to INIT\n");
         ctx.slavelist[0].state = EC_STATE_INIT;
         ecx_writestate(&ctx, 0);
         ecx_statecheck(&ctx, 0, EC_STATE_INIT, EC_TIMEOUTSTATE);
      }
   }
}

int main(int argc, char *argv[])
{
   printf("SOEM (Simple Open EtherCAT Master)\nCoEtest\n");

   if (argc > 1)
   {
      /* create thread to handle slave error handling in OP */
      osal_thread_create_rt(&threadrt, 128000, &ecatthread, NULL);
      /* create thread to handle slave error handling in OP */
      osal_thread_create(&thread1, 128000, &ecatcheck, NULL);
      /* start cyclic part */
      ethercatstartup(argv[1]);
   }
   else
   {
      ec_adaptert *adapter = NULL;
      ec_adaptert *head = NULL;
      printf("Usage: coetest ifname1\nifname = eth0 for example\n");

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
