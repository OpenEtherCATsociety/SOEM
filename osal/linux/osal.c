/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <osal.h>

#define USECS_PER_SEC     1000000

int osal_usleep (uint32 usec)
{
   struct timespec ts;
   ts.tv_sec = usec / USECS_PER_SEC;
   ts.tv_nsec = (usec % USECS_PER_SEC) * 1000;
   /* usleep is depricated, use nanosleep instead */
   return nanosleep(&ts, NULL);
}

int osal_gettimeofday(struct timeval *tv, struct timezone *tz)
{
   struct timespec ts;
   int return_value;

   /* Use clock_gettime to prevent possible live-lock.
    * Gettimeofday uses CLOCK_REALTIME that can get NTP timeadjust.
    * If this function preempts timeadjust and it uses vpage it live-locks.
    * Also when using XENOMAI, only clock_gettime is RT safe */
   return_value = clock_gettime(CLOCK_MONOTONIC, &ts), 0;
   tv->tv_sec = ts.tv_sec;
   tv->tv_usec = ts.tv_nsec / 1000;
   return return_value;
}

ec_timet osal_current_time(void)
{
   struct timeval current_time;
   ec_timet return_value;

   osal_gettimeofday(&current_time, 0);
   return_value.sec = current_time.tv_sec;
   return_value.usec = current_time.tv_usec;
   return return_value;
}

void osal_time_diff(ec_timet *start, ec_timet *end, ec_timet *diff)
{
   diff->sec = end->sec - start->sec;
   diff->usec = end->usec - start->usec;
   if (diff->usec < 0) {
     --diff->sec;
     diff->usec += 1000000;
   }
}

void osal_timer_start(osal_timert * self, uint32 timeout_usec)
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

boolean osal_timer_is_expired (osal_timert * self)
{
   struct timeval current_time;
   struct timeval stop_time;
   int is_not_yet_expired;

   osal_gettimeofday(&current_time, 0);
   stop_time.tv_sec = self->stop_time.sec;
   stop_time.tv_usec = self->stop_time.usec;
   is_not_yet_expired = timercmp(&current_time, &stop_time, <);

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

int osal_thread_create(void *thandle, int stacksize, void *func, void *param)
{
   int                  ret;
   pthread_attr_t       attr;
   pthread_t            *threadp;

   threadp = thandle;
   pthread_attr_init(&attr);
   pthread_attr_setstacksize(&attr, stacksize);
   ret = pthread_create(threadp, &attr, func, param);
   if(ret < 0)
   {
      return 0;
   }
   return 1;
}

int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{
   int                  ret;
   pthread_attr_t       attr;
   struct sched_param   schparam;
   pthread_t            *threadp;

   threadp = thandle;
   pthread_attr_init(&attr);
   pthread_attr_setstacksize(&attr, stacksize);
   ret = pthread_create(threadp, &attr, func, param);
   pthread_attr_destroy(&attr);
   if(ret < 0)
   {
      return 0;
   }
   memset(&schparam, 0, sizeof(schparam));
   schparam.sched_priority = 40;
   ret = pthread_setschedparam(*threadp, SCHED_FIFO, &schparam);
   if(ret < 0)
   {
      return 0;
   }

   return 1;
}
