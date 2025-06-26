/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#ifndef _osal_
#define _osal_

#ifdef __cplusplus
extern "C" {
#endif

#include "osal_defs.h"
#include <stdint.h>
#include <stddef.h>

/* General types */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
typedef uint8_t boolean;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef float float32;
typedef double float64;

typedef struct osal_timer
{
   ec_timet stop_time;
} osal_timert;

/**
 * @brief Returns monotonic time from some unspecified moment in the
 * past.
 *
 * This time must be strictly increasing. It is used for time
 * intervals measurement.
 *
 * @param ts Pointer to an ec_timet structure where the time will be
 * stored.
 */
void osal_get_monotonic_time(ec_timet *ts);

/**
 * @brief Returns the current time.
 *
 * This time is used to set the initial EtherCAT network DC time and
 * for logging purposes.
 *
 * @return ec_timet containing the current time.
 */
ec_timet osal_current_time(void);

/**
 * @brief Calculates the difference between two timestamps.
 *
 * @param start Pointer to the start timestamp.
 * @param end Pointer to the end timestamp.
 * @param diff Pointer to an ec_timet structure where the difference
 * will be stored.
 */
void osal_time_diff(ec_timet *start, ec_timet *end, ec_timet *diff);

/**
 * @brief Starts the timer with a specified timeout.
 *
 * @param self Pointer to the timer object.
 * @param timeout_usec Timeout in microseconds.
 */
void osal_timer_start(osal_timert *self, uint32 timeout_usec);

/**
 * @brief Checks if the timer has expired.
 *
 * @param self Pointer to the timer object.
 * @return True if the timer is expired, false otherwise.
 */
boolean osal_timer_is_expired(osal_timert *self);

/**
 * @brief Sleeps for a specified duration in microseconds.
 *
 * @param usec Duration in microseconds.
 * @return 0 on success, -1 on failure.
 */
int osal_usleep(uint32 usec);

/**
 * @brief Sleeps until the specified monotonic time.
 *
 * @param ts Pointer to an ec_timet structure representing the
 * absolute time to sleep until.
 * @return 0 on success, -1 on failure.
 */
int osal_monotonic_sleep(ec_timet *ts);

/**
 * @brief Allocates memory of the specified size.
 *
 * @param size Size in bytes to allocate.
 * @return Pointer to the allocated memory or NULL on failure.
 */
void *osal_malloc(size_t size);

/**
 * @brief Frees the allocated memory.
 *
 * @param ptr Pointer to the memory to free.
 */
void osal_free(void *ptr);

/**
 * @brief Creates a new thread.
 *
 * @param thandle Pointer to the thread handle which will store the
 * thread ID.
 * @param stacksize Size of the stack for the new thread.
 * @param func Pointer to the function to execute in the new thread.
 * @param param Pointer to parameters to pass to the thread function.
 * @return 1 on success, 0 on failure.
 */
int osal_thread_create(void *thandle, int stacksize, void *func, void *param);

/**
 * @brief Creates a new real-time thread.
 *
 * @param thandle Pointer to the thread handle which will store the
 * thread ID.
 * @param stacksize Size of the stack for the new thread.
 * @param func Pointer to the function to execute in the new thread.
 * @param param Pointer to parameters to pass to the thread function.
 * @return 1 on success, 0 on failure.
 */
int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param);

/**
 * @brief Creates a mutex.
 *
 * @return Pointer to the created mutex or NULL on failure.
 */
void *osal_mutex_create(void);

/**
 * @brief Destroys a mutex.
 *
 * @param mutex Pointer to the mutex to destroy.
 */
void osal_mutex_destroy(void *mutex);

/**
 * @brief Locks the mutex.
 *
 * @param mutex Pointer to the mutex to lock.
 */
void osal_mutex_lock(void *mutex);

/**
 * @brief Unlocks the mutex.
 *
 * @param mutex Pointer to the mutex to unlock.
 */
void osal_mutex_unlock(void *mutex);

#ifndef osal_timespec_from_usec
#define osal_timespec_from_usec(usec, result)      \
   do                                              \
   {                                               \
      (result)->tv_sec = usec / 1000000;           \
      (result)->tv_nsec = (usec % 1000000) * 1000; \
   } while (0)
#endif

#ifndef osal_timespeccmp
#define osal_timespeccmp(a, b, CMP)      \
   (((a)->tv_sec == (b)->tv_sec)         \
        ? ((a)->tv_nsec CMP(b)->tv_nsec) \
        : ((a)->tv_sec CMP(b)->tv_sec))
#endif

#ifndef osal_timespecadd
#define osal_timespecadd(a, b, result)                 \
   do                                                  \
   {                                                   \
      (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;    \
      (result)->tv_nsec = (a)->tv_nsec + (b)->tv_nsec; \
      if ((result)->tv_nsec >= 1000000000)             \
      {                                                \
         ++(result)->tv_sec;                           \
         (result)->tv_nsec -= 1000000000;              \
      }                                                \
   } while (0)
#endif

#ifndef osal_timespecsub
#define osal_timespecsub(a, b, result)                 \
   do                                                  \
   {                                                   \
      (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;    \
      (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
      if ((result)->tv_nsec < 0)                       \
      {                                                \
         --(result)->tv_sec;                           \
         (result)->tv_nsec += 1000000000;              \
      }                                                \
   } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif
