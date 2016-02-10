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
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#include "ethercat.h"

#define NSEC_PER_SEC 1000000000

struct sched_param schedp;
char IOmap[4096];
pthread_t thread1;
struct timeval tv, t1, t2;
int dorun = 0;
int deltat, tmax = 0;
int64 toff;
int DCdiff;
int os;
uint8 ob;
uint16 ob2;
pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;
uint8 *digout = 0;
int wcounter;

void redtest(char *ifname, char *ifname2)
{
   int cnt, i, j, oloop, iloop;

   printf("Starting Redundant test\n");

   /* initialise SOEM, bind socket to ifname */
   if (ec_init_redundant(ifname, ifname2))
   {
      printf("ec_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */
      if ( ec_config(FALSE, &IOmap) > 0 )
      {
         printf("%d slaves found and configured.\n",ec_slavecount);
         /* wait for all slaves to reach SAFE_OP state */
         ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE);

         /* configure DC options for every DC capable slave found in the list */
         ec_configdc();

         /* read indevidual slave state and store in ec_slave[] */
         ec_readstate();
         for(cnt = 1; cnt <= ec_slavecount ; cnt++)
         {
            printf("Slave:%d Name:%s Output size:%3dbits Input size:%3dbits State:%2d delay:%d.%d\n",
                  cnt, ec_slave[cnt].name, ec_slave[cnt].Obits, ec_slave[cnt].Ibits,
                  ec_slave[cnt].state, (int)ec_slave[cnt].pdelay, ec_slave[cnt].hasdc);
            printf("         Out:%8.8x,%4d In:%8.8x,%4d\n",
                  (int)ec_slave[cnt].outputs, ec_slave[cnt].Obytes, (int)ec_slave[cnt].inputs, ec_slave[cnt].Ibytes);
            /* check for EL2004 or EL2008 */
            if( !digout && ((ec_slave[cnt].eep_id == 0x07d43052) || (ec_slave[cnt].eep_id == 0x07d83052)))
            {
               digout = ec_slave[cnt].outputs;
            }
         }
         printf("Request operational state for all slaves\n");
         ec_slave[0].state = EC_STATE_OPERATIONAL;
         /* request OP state for all slaves */
         ec_writestate(0);
         /* wait for all slaves to reach OP state */
         ec_statecheck(0, EC_STATE_OPERATIONAL,  EC_TIMEOUTSTATE);
         oloop = ec_slave[0].Obytes;
         if ((oloop == 0) && (ec_slave[0].Obits > 0)) oloop = 1;
         if (oloop > 8) oloop = 8;
         iloop = ec_slave[0].Ibytes;
         if ((iloop == 0) && (ec_slave[0].Ibits > 0)) iloop = 1;
         if (iloop > 8) iloop = 8;
         if (ec_slave[0].state == EC_STATE_OPERATIONAL )
         {
            printf("Operational state reached for all slaves.\n");
            dorun = 1;
            /* acyclic loop 5000 x 20ms = 10s */
            for(i = 1; i <= 5000; i++)
            {
               printf("Processdata cycle %5d , Wck %3d, DCtime %12lld, O:", dorun, wcounter , ec_DCtime);
               for(j = 0 ; j < oloop; j++)
               {
                  printf(" %2.2x", *(ec_slave[0].outputs + j));
               }
               printf(" I:");
               for(j = 0 ; j < iloop; j++)
               {
                  printf(" %2.2x", *(ec_slave[0].inputs + j));
               }
               printf("\n");
               usleep(20000);
            }
            dorun = 0;
         }
         else
         {
            printf("Not all slaves reached operational state.\n");
         }
         printf("Request safe operational state for all slaves\n");
         ec_slave[0].state = EC_STATE_SAFE_OP;
         /* request SAFE_OP state for all slaves */
         ec_writestate(0);
      }
      else
      {
         printf("No slaves found!\n");
      }
      printf("End redundant test, close socket\n");
      /* stop SOEM, close socket */
      ec_close();
   }
   else
   {
      printf("No socket connection on %s\nExcecute as root\n",ifname);
   }
}

/* add ns to timespec */
void add_timespec(struct timespec *ts, int64 addtime)
{
   int64 sec, nsec;

   nsec = addtime % NSEC_PER_SEC;
   sec = (addtime - nsec) / NSEC_PER_SEC;
   ts->tv_sec += sec;
   ts->tv_nsec += nsec;
   if ( ts->tv_nsec > NSEC_PER_SEC )
   {
      nsec = ts->tv_nsec % NSEC_PER_SEC;
      ts->tv_sec += (ts->tv_nsec - nsec) / NSEC_PER_SEC;
      ts->tv_nsec = nsec;
   }
}

/* PI calculation to get linux time synced to DC time */
void ec_sync(int64 reftime, int64 cycletime , int64 *offsettime)
{
   static int64 integral = 0;
   int64 delta;
   /* set linux sync point 50us later than DC sync, just as example */
   delta = (reftime - 50000) % cycletime;
   if(delta> (cycletime / 2)) { delta= delta - cycletime; }
   if(delta>0){ integral++; }
   if(delta<0){ integral--; }
   *offsettime = -(delta / 100) - (integral / 20);
}

/* RT EtherCAT thread */
void ecatthread( void *ptr )
{
   struct timespec   ts;
   struct timeval    tp;
   int rc;
   int ht;
   int64 cycletime;

   rc = pthread_mutex_lock(&mutex);
   rc =  gettimeofday(&tp, NULL);

    /* Convert from timeval to timespec */
   ts.tv_sec  = tp.tv_sec;
   ht = (tp.tv_usec / 1000) + 1; /* round to nearest ms */
   ts.tv_nsec = ht * 1000000;
   cycletime = *(int*)ptr * 1000; /* cycletime in ns */
   toff = 0;
   dorun = 0;
   while(1)
   {
      /* calculate next cycle start */
      add_timespec(&ts, cycletime + toff);
      /* wait to cycle start */
      rc = pthread_cond_timedwait(&cond, &mutex, &ts);
      if (dorun>0)
      {
         rc =  gettimeofday(&tp, NULL);

         ec_send_processdata();

         wcounter = ec_receive_processdata(EC_TIMEOUTRET);

         dorun++;
         /* if we have some digital output, cycle */
         if( digout ) *digout = (uint8) ((dorun / 16) & 0xff);

         if (ec_slave[0].hasdc)
         {
            /* calulate toff to get linux time and DC synced */
            ec_sync(ec_DCtime, cycletime, &toff);
         }
      }
   }
}

int main(int argc, char *argv[])
{
   int iret1;
   int ctime;
   struct sched_param    param;
   int                   policy = SCHED_OTHER;

   printf("SOEM (Simple Open EtherCAT Master)\nRedundancy test\n");

   memset(&schedp, 0, sizeof(schedp));
   /* do not set priority above 49, otherwise sockets are starved */
   schedp.sched_priority = 30;
   sched_setscheduler(0, SCHED_FIFO, &schedp);

   do
   {
      usleep(1000);
   }
   while (dorun);

   if (argc > 3)
   {
      dorun = 1;
      ctime = atoi(argv[3]);
      /* create RT thread */
      iret1 = pthread_create( &thread1, NULL, (void *) &ecatthread, (void*) &ctime);
      memset(&param, 0, sizeof(param));
      /* give it higher priority */
      param.sched_priority = 40;
      iret1 = pthread_setschedparam(thread1, policy, &param);

      /* start acyclic part */
      redtest(argv[1],argv[2]);
   }
   else
   {
      printf("Usage: red_test ifname1 ifname2 cycletime\nifname = eth0 for example\ncycletime in us\n");
   }

   schedp.sched_priority = 0;
   sched_setscheduler(0, SCHED_OTHER, &schedp);

   printf("End program\n");

   return (0);
}
