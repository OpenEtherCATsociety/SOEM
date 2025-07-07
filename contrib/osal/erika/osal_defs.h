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

#include <sys/time.h>
#include <stdlib.h>
#include <ee.h>

// define if debug print is needed
#ifdef EC_DEBUG
#define EC_PRINT OSEE_PRINT
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

int osal_gettimeofday(struct timeval *tv, struct timezone *tz);
void *osal_malloc(size_t size);
void osal_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
