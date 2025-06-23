/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#include <osal.h>
#include <kern/kern.h>

void osal_get_monotonic_time(ec_timet *tv)
{
   tick_t tick = tick_get();
   uint64_t usec = (uint64_t)(tick_to_ms(tick)) * 1000;

   osal_timespec_from_usec(usec, tv);
}

ec_timet osal_current_time(void)
{
   struct timeval tv;
   struct timespec ts;

   gettimeofday(&tv, NULL);
   TIMEVAL_TO_TIMESPEC(&tv, &ts);
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
   tick_t ticks = tick_from_ms(usec / 1000) + 1;
   task_delay(ticks);
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
   thandle = task_spawn("worker", func, 6, stacksize, param);
   if (!thandle)
   {
      return 0;
   }
   return 1;
}

int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{
   thandle = task_spawn("worker_rt", func, 15, stacksize, param);
   if (!thandle)
   {
      return 0;
   }
   return 1;
}

void *osal_mutex_create(void)
{
   return (void *)mtx_create();
}

void osal_mutex_destroy(void *mutex)
{
   mtx_destroy(mutex);
}

void osal_mutex_lock(void *mutex)
{
   mtx_lock(mutex);
}

void osal_mutex_unlock(void *mutex)
{
   mtx_unlock(mutex);
}
