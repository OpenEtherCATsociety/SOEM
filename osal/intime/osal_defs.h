/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#ifndef _osal_defs_
#define _osal_defs_

#ifdef __cplusplus
extern "C"
{
#endif

// define if debug printf is needed
//#define EC_DEBUG

#ifdef EC_DEBUG
#define EC_PRINT printf
#else
#define EC_PRINT(...) do {} while (0)
#endif

#ifndef PACKED
    #ifdef _MSC_VER
    #define PACKED_BEGIN __pragma(pack(push, 1))
    #define PACKED
    #define PACKED_END __pragma(pack(pop))
    #elif defined(__GNUC__)
    #define PACKED_BEGIN
    #define PACKED  __attribute__((__packed__))
    #define PACKED_END
    #endif
#endif

#define OSAL_THREAD_HANDLE   RTHANDLE
#define OSAL_THREAD_FUNC     void
#define OSAL_THREAD_FUNC_RT  void

#ifdef __cplusplus
}
#endif

#endif
