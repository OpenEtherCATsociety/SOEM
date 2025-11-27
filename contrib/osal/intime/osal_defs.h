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

// define if debug printf is needed
#ifdef EC_DEBUG
#define EC_PRINT printf
#else
#define EC_PRINT(...) \
   do                 \
   {                  \
   } while (0)
#endif

#ifndef OSAL_PACKED
#ifdef _MSC_VER
#define OSAL_PACKED_BEGIN __pragma(pack(push, 1))
#define OSAL_PACKED
#define OSAL_PACKED_END __pragma(pack(pop))
#elif defined(__GNUC__)
#define OSAL_PACKED_BEGIN
#define OSAL_PACKED __attribute__((__packed__))
#define OSAL_PACKED_END
#endif
#endif

#define OSAL_THREAD_HANDLE  RTHANDLE
#define OSAL_THREAD_FUNC    void
#define OSAL_THREAD_FUNC_RT void

#ifdef __cplusplus
}
#endif

#endif
