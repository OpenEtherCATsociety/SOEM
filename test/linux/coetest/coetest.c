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
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include "ethercat.h"

boolean forceByteAlignment = FALSE;
#define EC_TIMEOUTMON 500

char IOmap[4096];
OSAL_THREAD_HANDLE threadrt, thread1;
int expectedWKC;
int mappingdone, dorun, inOP, dowkccheck;
int adapterisbound, conf_io_size, currentgroup;
int64_t cycletime = 1000000;
#define NSEC_PER_MSEC 1000000
#define NSEC_PER_SEC  1000000000

/* add ns to timespec */
void add_timespec(struct timespec *ts, int64 addtime)
{
   int64 sec, nsec;

   nsec = addtime % NSEC_PER_SEC;
   sec = (addtime - nsec) / NSEC_PER_SEC;
   ts->tv_sec += sec;
   ts->tv_nsec += nsec;
   if (ts->tv_nsec >= NSEC_PER_SEC)
   {
      nsec = ts->tv_nsec % NSEC_PER_SEC;
      ts->tv_sec += (ts->tv_nsec - nsec) / NSEC_PER_SEC;
      ts->tv_nsec = nsec;
   }
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
   *offsettime = (timeerror * pgain) + (integral * igain);
}

/* RT EtherCAT thread */
OSAL_THREAD_FUNC_RT ecatthread(void)
{
   struct timespec ts, tleft;
   int ht, wkc;
   static int64_t toff = 0;

   dorun = 0;
   while (!mappingdone)
   {
      osal_usleep(100);
   }
   clock_gettime(CLOCK_MONOTONIC, &ts);
   ht = (ts.tv_nsec / 1000000) + 1; /* round to nearest ms */
   ts.tv_nsec = ht * 1000000;
   ecx_send_processdata(&ecx_context);
   while (1)
   {
      /* calculate next cycle start */
      add_timespec(&ts, cycletime + toff);
      /* wait to cycle start */
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, &tleft);
      if (dorun > 0)
      {
         wkc = ecx_receive_processdata(&ecx_context, EC_TIMEOUTRET);
         if (wkc != expectedWKC)
            dowkccheck++;
         else
            dowkccheck = 0;
         if (ec_slave[0].hasdc && (wkc > 0))
         {
            /* calulate toff to get linux time and DC synced */
            ec_sync(ec_DCtime, cycletime, &toff);
         }
         ecx_mbxhandler(&ecx_context, 0, 4);
         ecx_send_processdata(&ecx_context);
      }
   }
}

OSAL_THREAD_FUNC ecatcheck(void)
{
   int slave;

   while (1)
   {
      if (inOP && ((dowkccheck > 2) || ec_group[currentgroup].docheckstate))
      {
         /* one ore more slaves are not responding */
         ec_group[currentgroup].docheckstate = FALSE;
         ec_readstate();
         for (slave = 1; slave <= ec_slavecount; slave++)
         {
            if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
            {
               ec_group[currentgroup].docheckstate = TRUE;
               if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
               {
                  printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                  ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                  ec_writestate(slave);
               }
               else if (ec_slave[slave].state == EC_STATE_SAFE_OP)
               {
                  printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                  ec_slave[slave].state = EC_STATE_OPERATIONAL;
                  ec_writestate(slave);
               }
               else if (ec_slave[slave].state > EC_STATE_NONE)
               {
                  if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d reconfigured\n", slave);
                  }
               }
               else if (!ec_slave[slave].islost)
               {
                  /* re-check state */
                  ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                  if (ec_slave[slave].state == EC_STATE_NONE)
                  {
                     ec_slave[slave].islost = TRUE;
                     printf("ERROR : slave %d lost\n", slave);
                  }
               }
            }
            if (ec_slave[slave].islost)
            {
               if (ec_slave[slave].state <= EC_STATE_INIT)
               {
                  if (ec_recover_slave(slave, EC_TIMEOUTMON))
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d recovered\n", slave);
                  }
               }
               else
               {
                  ec_slave[slave].islost = FALSE;
                  printf("MESSAGE : slave %d found. State %2.2x\n", slave, ec_slave[slave].state);
               }
            }
         }
         if (!ec_group[currentgroup].docheckstate)
            printf("OK : all slaves resumed OPERATIONAL.\n");
         dowkccheck = 0;
      }
      osal_usleep(10000);
   }
}

void ethercatstartup(char *ifname)
{
   printf("EtherCAT Startup\n");
   int rv = ecx_init(&ecx_context, ifname);
   if (rv)
   {
      adapterisbound = 1;
      ecx_config_init(&ecx_context, FALSE);
      if (ec_slavecount > 0)
      {
         if (forceByteAlignment)
            conf_io_size = ecx_config_map_group_aligned(&ecx_context, &IOmap, 0);
         else
            conf_io_size = ecx_config_map_group(&ecx_context, &IOmap, 0);
         expectedWKC = (ecx_context.grouplist[0].outputsWKC * 2) + ecx_context.grouplist[0].inputsWKC;
         mappingdone = 1;
         ecx_configdc(&ecx_context);
         int sdoslave = -1;
         for (int si = 1; si <= *ecx_context.slavecount; si++)
         {
            ec_slavet *slave = &ecx_context.slavelist[si];
            printf("Slave %d name:%s man:%8.8x id:%8.8x rev:%d ser:%d\n", si, slave->name, slave->eep_man, slave->eep_id, slave->eep_rev, slave->eep_ser);
            if (slave->CoEdetails > 0)
            {
               ecx_slavembxcyclic(&ecx_context, si);
               sdoslave = si;
               printf(" Slave added to cyclic mailbox handler\n");
            }
         }
         dorun = 1;
         osal_usleep(1000000);
         ecx_context.slavelist[0].state = EC_STATE_OPERATIONAL;
         ecx_writestate(&ecx_context, 0);
         ecx_statecheck(&ecx_context, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);
         inOP = 1;
         printf("EtherCAT OP\n");
         uint32_t aval = 0;
         int avals = sizeof(aval);
         for (int i = 0; i < 100; i++)
         {
            printf("Cycle %d timeerror %ld\n", i, timeerror);
            if (sdoslave > 0)
            {
               aval = 0;
               int rval = ecx_SDOread(&ecx_context, sdoslave, 0x1018, 0x02, FALSE, &avals, &aval, EC_TIMEOUTRXM);
               printf(" Slave %d Rval %d Prodcode %8.8x\n", sdoslave, rval, aval);
            }
            osal_usleep(100000);
         }
         inOP = 0;
         printf("EtherCAT to safe-OP\n");
         ecx_context.slavelist[0].state = EC_STATE_SAFE_OP;
         ecx_writestate(&ecx_context, 0);
         ecx_statecheck(&ecx_context, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);
         printf("EtherCAT to INIT\n");
         ecx_context.slavelist[0].state = EC_STATE_INIT;
         ecx_writestate(&ecx_context, 0);
         ecx_statecheck(&ecx_context, 0, EC_STATE_INIT, EC_TIMEOUTSTATE);
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
      printf("Usage: coetest ifname1\nifname = eth0 for example\n");

      printf("\nAvailable adapters:\n");
      adapter = ec_find_adapters();
      while (adapter != NULL)
      {
         printf("    - %s  (%s)\n", adapter->name, adapter->desc);
         adapter = adapter->next;
      }
      ec_free_adapters(adapter);
   }

   printf("End program\n");
   return (0);
}
