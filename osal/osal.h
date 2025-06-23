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

/* Returns time from some unspecified moment in past,
 * strictly increasing, used for time intervals measurement. */
void osal_get_monotonic_time(ec_timet *tv);
int osal_monotonic_sleep(ec_timet *ts);

void osal_timer_start(osal_timert *self, uint32 timeout_us);
boolean osal_timer_is_expired(osal_timert *self);
int osal_usleep(uint32 usec);
ec_timet osal_current_time(void);
void osal_time_diff(ec_timet *start, ec_timet *end, ec_timet *diff);
int osal_thread_create(void *thandle, int stacksize, void *func, void *param);
int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param);
void *osal_malloc(size_t size);
void osal_free(void *ptr);
void *osal_mutex_create(void);
void osal_mutex_destroy(void *mutex);
void osal_mutex_lock(void *mutex);
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
