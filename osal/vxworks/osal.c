/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <osal.h>
#include <vxWorks.h>
#include <taskLib.h>


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

#define USECS_PER_SEC     1000000

/* OBS! config worker threads must have higher prio that task running ec_configuration */
#define ECAT_TASK_PRIO_HIGH      20     /* Priority for high performance network task */
#define ECAT_TASK_PRIO_LOW       80     /* Priority for high performance network task */
#define ECAT_STACK_SIZE          10000  /* Stack size for high performance task */
static int ecatTaskOptions       = VX_SUPERVISOR_MODE | VX_UNBREAKABLE;
static int ecatTaskIndex         = 0;

#ifndef use_task_delay
#define use_task_delay 1
#endif

int osal_usleep (uint32 usec)
{
#if (use_task_delay == 1)
    /* Task delay 0 only yields */
   taskDelay(usec / 1000);
   return 0;
#else
    /* The suspension may be longer than requested due to the rounding up of 
     * the request to the timer's resolution or to other scheduling activities
     * (e.g., a higher priority task intervenes). 
     */
   struct timespec ts;
   ts.tv_sec = usec / USECS_PER_SEC;
   ts.tv_nsec = (usec % USECS_PER_SEC) * 1000;
   return nanosleep(&ts, NULL);
#endif

}

int osal_gettimeofday(struct timeval *tv, struct timezone *tz)
{
   struct timespec ts;
   int return_value;
   (void)tz;       /* Not used */

   /* Use clock_gettime CLOCK_MONOTONIC to a avoid NTP time adjustments */
   return_value = clock_gettime(CLOCK_MONOTONIC, &ts);
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
   if (end->usec < start->usec) {
      diff->sec = end->sec - start->sec - 1;
      diff->usec = end->usec + 1000000 - start->usec;
   }
   else {
      diff->sec = end->sec - start->sec;
      diff->usec = end->usec - start->usec;
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
   char task_name[20];
   TASK_ID * tid = (TASK_ID *)thandle;
   FUNCPTR  func_ptr = func;
   _Vx_usr_arg_t arg1 = (_Vx_usr_arg_t)param;

   snprintf(task_name,sizeof(task_name),"worker_%d",ecatTaskIndex++);

   *tid = taskSpawn (task_name, ECAT_TASK_PRIO_LOW,
                      ecatTaskOptions, ECAT_STACK_SIZE,
                      func_ptr, arg1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
   if(*tid == TASK_ID_ERROR) 
   {
      return 0;
   }
   
   return 1;
}

int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{
   char task_name[20];
   TASK_ID * tid = (TASK_ID *)thandle; 
   FUNCPTR  func_ptr = func;
   _Vx_usr_arg_t arg1 = (_Vx_usr_arg_t)param;

   snprintf(task_name,sizeof(task_name),"worker_rt_%d",ecatTaskIndex++);

   *tid = taskSpawn (task_name, ECAT_TASK_PRIO_HIGH,
                      ecatTaskOptions, ECAT_STACK_SIZE,
                      func_ptr, arg1, 0, 0, 0, 0, 0, 0, 0, 0, 0);

   if(*tid == TASK_ID_ERROR) 
   {
      return 0;
   }
   return 1;
}

