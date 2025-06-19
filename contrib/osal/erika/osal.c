/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <osal.h>

#include "ee_x86_64_tsc.h"

#define USECS_PER_SEC 1000000
#define NSECS_PER_SEC 1000000000

uint64_t osEE_x86_64_tsc_read(void);

void ee_usleep(uint32 usec);

inline int osal_usleep(uint32 usec)
{
   ee_usleep(usec);
   return 0;
}

int osal_gettimeofday(struct timeval *tv, struct timezone *tz)
{
   uint64_t time = osEE_x86_64_tsc_read();
   tv->tv_sec = time / NSECS_PER_SEC;
   tv->tv_sec += 946684800UL; /* EtherCAT uses 2000-01-01 as epoch start */
   tv->tv_usec = (time % NSECS_PER_SEC) / 1000;
   return 0;
}

ec_timet osal_current_time(void)
{
   struct timeval current_time;
   ec_timet ret;

   osal_gettimeofday(&current_time, 0);
   ret.sec = current_time.tv_sec;
   ret.usec = current_time.tv_usec;
   return ret;
}

void osal_time_diff(ec_timet *start, ec_timet *end, ec_timet *diff)
{
   if (end->usec < start->usec)
   {
      diff->sec = end->sec - start->sec - 1;
      diff->usec = end->usec + USECS_PER_SEC - start->usec;
   }
   else
   {
      diff->sec = end->sec - start->sec;
      diff->usec = end->usec - start->usec;
   }
}

void osal_timer_start(osal_timert *self, uint32 timeout_usec)
{
   struct timeval start_time;
   struct timeval timeout;
   struct timeval stop_time;

   osal_gettimeofday(&start_time, 0);
   timeout.tv_sec = timeout_usec / USECS_PER_SEC;
   timeout.tv_usec = timeout_usec % USECS_PER_SEC;
   timeradd(&start_time, &timeout, &stop_time);

   self->stop_time.sec = stop_time.tv_sec;
   self->stop_time.usec = stop_time.tv_usec;
}

boolean osal_timer_is_expired(osal_timert *self)
{
   struct timeval current_time;
   struct timeval stop_time;
   int is_not_yet_expired;

   osal_gettimeofday(&current_time, 0);
   stop_time.tv_sec = self->stop_time.sec;
   stop_time.tv_usec = self->stop_time.usec;
   is_not_yet_expired = timercmp(&current_time, &stop_time, <);
   /* 	OSEE_PRINT("current: %d:%d -- expire: %d:%d -- result: %d\n", */
   /* 			current_time.tv_sec, */
   /* 			current_time.tv_usec, */
   /* 			stop_time.tv_sec, */
   /* 			stop_time.tv_usec, */
   /* 			is_not_yet_expired); */

   return is_not_yet_expired == FALSE;
}

void *osal_malloc(size_t size)
{
   return malloc(size);
}

void osal_free(void *ptr)
{
   free(ptr);
}
