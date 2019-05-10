/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include <winsock2.h>
#include <osal.h>
#include "osal_win32.h"

static int64_t sysfrequency;
static double qpc2usec;

#define USECS_PER_SEC     1000000

int osal_getrelativetime(struct timeval *tv, struct timezone *tz)
{
   int64_t wintime, usecs;
   if(!sysfrequency)
   {
      timeBeginPeriod(1);
      QueryPerformanceFrequency((LARGE_INTEGER *)&sysfrequency);
      qpc2usec = 1000000.0 / sysfrequency;
   }
   QueryPerformanceCounter((LARGE_INTEGER *)&wintime);
   usecs = (int64_t)((double)wintime * qpc2usec);
   tv->tv_sec = (long)(usecs / 1000000);
   tv->tv_usec = (long)(usecs - (tv->tv_sec * 1000000));

   return 1;
}

int osal_gettimeofday(struct timeval *tv, struct timezone *tz)
{
   FILETIME system_time;
   int64 system_time64, usecs;

   /* The offset variable is required to switch from Windows epoch (January 1, 1601) to
    * Unix epoch (January 1, 1970). Number of days between both epochs: 134.774
    *
    * The time returned by GetSystemTimeAsFileTime() changes in 100 ns steps, so the
    * following factors are required for the conversion from days to 100 ns steps:
    *
    * 86.400 seconds per day; 1.000.000 microseconds per second; 10 * 100 ns per microsecond
   */
   int64 offset = -134774LL * 86400LL * 1000000LL * 10LL;

   GetSystemTimeAsFileTime(&system_time);

   system_time64 = ((int64)(system_time.dwHighDateTime) << 32) + (int64)system_time.dwLowDateTime;
   system_time64 += offset;
   usecs = system_time64 / 10;

   tv->tv_sec = (long)(usecs / 1000000);
   tv->tv_usec = (long)(usecs - (tv->tv_sec * 1000000));

   return 1;
}

ec_timet osal_current_time (void)
{
   struct timeval current_time;
   ec_timet return_value;

   osal_gettimeofday (&current_time, 0);
   return_value.sec = current_time.tv_sec;
   return_value.usec = current_time.tv_usec;
   return return_value;
}

void osal_time_diff(ec_timet *start, ec_timet *end, ec_timet *diff)
{
   if (end->usec < start->usec) {
      diff->sec = end->sec - start->sec - 1;
      diff->usec = end->usec + 1000000 - start->usec;
   }
   else {
      diff->sec = end->sec - start->sec;
      diff->usec = end->usec - start->usec;
   }
}

void osal_timer_start (osal_timert *self, uint32 timeout_usec)
{
   struct timeval start_time;
   struct timeval timeout;
   struct timeval stop_time;

   osal_getrelativetime (&start_time, 0);
   timeout.tv_sec = timeout_usec / USECS_PER_SEC;
   timeout.tv_usec = timeout_usec % USECS_PER_SEC;
   timeradd (&start_time, &timeout, &stop_time);

   self->stop_time.sec = stop_time.tv_sec;
   self->stop_time.usec = stop_time.tv_usec;
}

boolean osal_timer_is_expired (osal_timert *self)
{
   struct timeval current_time;
   struct timeval stop_time;
   int is_not_yet_expired;

   osal_getrelativetime (&current_time, 0);
   stop_time.tv_sec = self->stop_time.sec;
   stop_time.tv_usec = self->stop_time.usec;
   is_not_yet_expired = timercmp (&current_time, &stop_time, <);

   return is_not_yet_expired == FALSE;
}

int osal_usleep(uint32 usec)
{
   osal_timert qtime;
   osal_timer_start(&qtime, usec);
   if(usec >= 1000)
   {
      SleepEx(usec / 1000, FALSE);
   }
   while(!osal_timer_is_expired(&qtime));
   return 1;
}

void *osal_malloc(size_t size)
{
   return malloc(size);
}

void osal_free(void *ptr)
{
   free(ptr);
}

int osal_thread_create(void **thandle, int stacksize, void *func, void *param)
{
   *thandle = CreateThread(NULL, stacksize, func, param, 0, NULL);
   if(!thandle)
   {
      return 0;
   }
   return 1;
}

int osal_thread_create_rt(void **thandle, int stacksize, void *func, void *param)
{
   int ret;
   ret = osal_thread_create(thandle, stacksize, func, param);
   if (ret)
   {
      ret = SetThreadPriority(*thandle, THREAD_PRIORITY_TIME_CRITICAL);
   }
   return ret;
}
