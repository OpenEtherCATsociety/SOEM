/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#ifndef _osal_defs_
#define _osal_defs_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <sys/time.h>

// define if debug printf is needed
#ifdef EC_DEBUG
#include <stdio.h>
#define EC_PRINT printf
#else
#define EC_PRINT(...) \
   do                 \
   {                  \
   } while (0)
#endif

#ifndef OSAL_PACKED
#define OSAL_PACKED_BEGIN
#define OSAL_PACKED __attribute__((__packed__))
#define OSAL_PACKED_END
#endif

#define ec_timet            struct timespec

#define OSAL_THREAD_HANDLE  pthread_t *
#define OSAL_THREAD_FUNC    void
#define OSAL_THREAD_FUNC_RT void

#define osal_mutext         pthread_mutex_t

#ifdef __cplusplus
}
#endif

#endif
