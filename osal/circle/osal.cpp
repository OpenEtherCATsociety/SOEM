#include <circle/sched/scheduler.h>
#include <circle/synchronize.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/alloc.h>
#include <assert.h>
#include <osal.h>
#include <time.h>

int osal_usleep(uint32 usec)
{
    if (CScheduler::IsActive()) {
        // Scheduler-friendly sleep
        CScheduler::Get()->usSleep(usec);
    } else {
        // Busy-waiting sleep
        CTimer::SimpleusDelay(usec);
    }
    return 0;
}

static long long get_monotonic_clock()
{
    // Copied from circle's timer, using both words to get 64bit monotonic clock
    InstructionSyncBarrier();

    u32 nCNTPCTLow, nCNTPCTHigh;
    asm volatile ("mrrc p15, 0, %0, %1, c14" : "=r" (nCNTPCTLow), "=r" (nCNTPCTHigh));

    return (((long long)nCNTPCTHigh) << 32) | nCNTPCTLow;
}

int osal_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    long long usec = get_monotonic_clock();

    tv->tv_sec  = usec / 1000000LL;
    tv->tv_usec = usec % 1000000LL;
    return 0;
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
    } else {
        diff->sec = end->sec - start->sec;
        diff->usec = end->usec - start->usec;
    }
}

void osal_timer_start(osal_timert * self, uint32 timeout_usec)
{
    long long stop_time_usec = get_monotonic_clock() + (long long)timeout_usec;

    self->stop_time.sec  = stop_time_usec / 1000000LL;
    self->stop_time.usec = stop_time_usec % 1000000LL;
}

boolean osal_timer_is_expired(osal_timert * self)
{
    long long now_usec       = get_monotonic_clock();
    long long stop_time_usec = self->stop_time.sec * 1000000LL + self->stop_time.usec;

    return (now_usec >= stop_time_usec);
}

void *osal_malloc(size_t size)
{
    // Use circle's malloc
    return malloc(size);
}

void osal_free(void *ptr)
{
    // Use circle's free
    free(ptr);
}

int osal_thread_create(void *thandle, int stacksize, void *func, void *param)
{
    // Not supported
    return 0;
}

int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{
    // Not supported
    return 0;
}
