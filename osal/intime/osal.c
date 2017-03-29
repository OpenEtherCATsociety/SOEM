/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include <rt.h>
#include <sys/time.h>
#include <osal.h>

static int64_t sysfrequency;
static double qpc2usec;

#define USECS_PER_SEC     1000000

int osal_gettimeofday (struct timeval *tv, struct timezone *tz)
{
   return gettimeofday (tv, tz);
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

void osal_timer_start (osal_timert * self, uint32 timeout_usec)
{
   struct timeval start_time;
   struct timeval timeout;
   struct timeval stop_time;

   osal_gettimeofday (&start_time, 0);
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

   osal_gettimeofday (&current_time, 0);
   stop_time.tv_sec = self->stop_time.sec;
   stop_time.tv_usec = self->stop_time.usec;
   is_not_yet_expired = timercmp (&current_time, &stop_time, <);

   return is_not_yet_expired == FALSE;
}

int osal_usleep(uint32 usec)
{
   RtSleepEx (usec / 1000);
   return 1;
}

/* Mutex is not needed when running single threaded */

void osal_mtx_lock(osal_mutex_t * mtx)
{
        /* RtWaitForSingleObject((HANDLE)mtx, INFINITE); */
}

void osal_mtx_unlock(osal_mutex_t * mtx)
{
        /* RtReleaseMutex((HANDLE)mtx); */
}

int osal_mtx_lock_timeout(osal_mutex_t * mtx, uint32_t time_ms)
{
        /* return RtWaitForSingleObject((HANDLE)mtx, time_ms); */
        return 0;
}

osal_mutex_t * osal_mtx_create(void)
{
        /* return (void*)RtCreateMutex(NULL, FALSE, NULL); */
        return (void *)0;
}
