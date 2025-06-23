/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#include <osal.h>
#include <stdlib.h>
#include <inttypes.h>
#include <timeapi.h>

static LARGE_INTEGER sysfrequency;

void osal_get_monotonic_time(ec_timet *ts)
{
   LARGE_INTEGER wintime;
   uint64_t sec;
   uint64_t nsec;

   if (!sysfrequency.QuadPart)
   {
      timeBeginPeriod(1);
      QueryPerformanceFrequency(&sysfrequency);
   }

   QueryPerformanceCounter(&wintime);

   /* Compute seconds */
   sec = wintime.QuadPart / sysfrequency.QuadPart;
   wintime.QuadPart = wintime.QuadPart - sec * sysfrequency.QuadPart;

   /* Compute nanoseconds. Multiplying first acts as a guard against
      potential loss of precision during the calculation. */
   nsec = wintime.QuadPart * 1000000000;
   nsec = nsec / sysfrequency.QuadPart;

   ts->tv_sec = sec;
   ts->tv_nsec = (uint32_t)nsec;
}

ec_timet osal_current_time(void)
{
   struct timespec ts;
   timespec_get(&ts, TIME_UTC);
   return ts;
}

void osal_time_diff(ec_timet *start, ec_timet *end, ec_timet *diff)
{
   osal_timespecsub(end, start, diff);
}

void osal_timer_start(osal_timert *self, uint32 timeout_usec)
{
   struct timespec start_time;
   struct timespec timeout;

   osal_get_monotonic_time(&start_time);
   osal_timespec_from_usec(timeout_usec, &timeout);
   osal_timespecadd(&start_time, &timeout, &self->stop_time);
}

boolean osal_timer_is_expired(osal_timert *self)
{
   struct timespec current_time;
   int is_not_yet_expired;

   osal_get_monotonic_time(&current_time);
   is_not_yet_expired = osal_timespeccmp(&current_time, &self->stop_time, <);

   return is_not_yet_expired == FALSE;
}

int osal_usleep(uint32 usec)
{
   struct timespec wakeup;
   struct timespec timeout;

   osal_get_monotonic_time(&wakeup);
   osal_timespec_from_usec(usec, &timeout);
   osal_timespecadd(&wakeup, &timeout, &wakeup);
   osal_monotonic_sleep(&wakeup);
   return 1;
}

/**
 * @brief Suspends the execution of the calling thread until a
 * specified absolute time.
 *
 * @param ts Pointer to a struct that specifies the
 * absolute wakeup time in milliseconds.
 * @return 0 on success, or a negative value on error.
 */
int osal_monotonic_sleep(ec_timet *ts)
{
   uint64_t millis;
   struct timespec now;
   struct timespec delay;

   osal_get_monotonic_time(&now);

   /* Delay already expired? */
   if (!osal_timespeccmp(&now, ts, <))
      return 0;

   /* Sleep for whole milliseconds */
   osal_timespecsub(ts, &now, &delay);
   millis = delay.tv_sec * 1000 + delay.tv_nsec / 1000000;
   if (millis > 0)
   {
      SleepEx((DWORD)millis, FALSE);
   }

   /* Busy wait for remaining time */
   do
   {
      osal_get_monotonic_time(&now);
   } while osal_timespeccmp(&now, ts, <);

   return 0;
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
   *(OSAL_THREAD_HANDLE *)thandle = CreateThread(NULL, stacksize, func, param, 0, NULL);
   if (!thandle)
   {
      return 0;
   }
   return 1;
}

int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{
   int ret;
   ret = osal_thread_create(thandle, stacksize, func, param);
   if (ret)
   {
      ret = SetThreadPriority(thandle, THREAD_PRIORITY_TIME_CRITICAL);
   }
   return ret;
}

void *osal_mutex_create(void)
{
   return CreateMutex(NULL, FALSE, NULL);
}

void osal_mutex_destroy(void *mutex)
{
   CloseHandle(mutex);
}

void osal_mutex_lock(void *mutex)
{
   WaitForSingleObject(mutex, INFINITE);
}

void osal_mutex_unlock(void *mutex)
{
   ReleaseMutex(mutex);
}
