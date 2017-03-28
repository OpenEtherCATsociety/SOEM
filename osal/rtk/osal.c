/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include <osal.h>
#include <kern.h>
#include <time.h>
#include <sys/time.h>
#include <config.h>

#define  timercmp(a, b, CMP)                                \
  (((a)->tv_sec == (b)->tv_sec) ?                           \
   ((a)->tv_usec CMP (b)->tv_usec) :                        \
   ((a)->tv_sec CMP (b)->tv_sec))
#define  timeradd(a, b, result)                             \
  do {                                                      \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;           \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;        \
    if ((result)->tv_usec >= 1000000)                       \
    {                                                       \
       ++(result)->tv_sec;                                  \
       (result)->tv_usec -= 1000000;                        \
    }                                                       \
  } while (0)
#define  timersub(a, b, result)                             \
  do {                                                      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;           \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;        \
    if ((result)->tv_usec < 0) {                            \
      --(result)->tv_sec;                                   \
      (result)->tv_usec += 1000000;                         \
    }                                                       \
  } while (0)

#define USECS_PER_SEC   1000000
#define USECS_PER_TICK  (USECS_PER_SEC / CFG_TICKS_PER_SECOND)


/* Workaround for rt-labs defect 776.
 * Default implementation of udelay() didn't work correctly when tick was
 * shorter than one millisecond.
 */
void udelay (uint32_t us)
{
   tick_t ticks = (us / USECS_PER_TICK) + 1;
   task_delay (ticks);
}

int gettimeofday(struct timeval *tp, void *tzp)
{
   tick_t tick = tick_get();
   tick_t ticks_left;

   ASSERT (tp != NULL);

   tp->tv_sec = tick / CFG_TICKS_PER_SECOND;

   ticks_left = tick % CFG_TICKS_PER_SECOND;
   tp->tv_usec = ticks_left * USECS_PER_TICK;
   ASSERT (tp->tv_usec < USECS_PER_SEC);

   return 0;
}

int osal_usleep (uint32 usec)
{
   udelay(usec);
   return 0;
}

int osal_gettimeofday(struct timeval *tv, struct timezone *tz)
{
   return gettimeofday(tv, tz);
}

ec_timet osal_current_time (void)
{
   struct timeval current_time;
   ec_timet return_value;

   gettimeofday (&current_time, 0);
   return_value.sec = current_time.tv_sec;
   return_value.usec = current_time.tv_usec;
   return return_value;
}

void osal_timer_start (osal_timert * self, uint32 timeout_usec)
{
   struct timeval start_time;
   struct timeval timeout;
   struct timeval stop_time;

   gettimeofday (&start_time, 0);
   timeout.tv_sec = timeout_usec / USECS_PER_SEC;
   timeout.tv_usec = timeout_usec % USECS_PER_SEC;
   timeradd (&start_time, &timeout, &stop_time);

   self->stop_time.sec = stop_time.tv_sec;
   self->stop_time.usec = stop_time.tv_usec;
}

boolean osal_timer_is_expired (osal_timert * self)
{
   struct timeval current_time;
   struct timeval stop_time;
   int is_not_yet_expired;

   gettimeofday (&current_time, 0);
   stop_time.tv_sec = self->stop_time.sec;
   stop_time.tv_usec = self->stop_time.usec;
   is_not_yet_expired = timercmp (&current_time, &stop_time, <);

   return is_not_yet_expired == false;
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
   thandle = task_spawn ("worker", func, 6,stacksize, param);
   if(!thandle)
   {
      return 0;
   }
   return 1;
}

int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{
   thandle = task_spawn ("worker_rt", func, 15 ,stacksize, param);
   if(!thandle)
   {
      return 0;
   }
   return 1;
}
