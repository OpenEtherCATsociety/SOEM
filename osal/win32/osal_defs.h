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

#define WIN32_LEAN_AND_MEAN // Exclude some conflicting definitions in windows header
#include <windows.h>
// define if debug printf is needed
//#define EC_DEBUG

#ifdef EC_DEBUG
#define EC_PRINT printf
#else
#define EC_PRINT(...) do {} while (0)
#endif

#ifndef PACKED
#define PACKED
#ifdef __GNUC__
#define PACKED_BEGIN _Pragma("pack(push,1)")
#define PACKED_END _Pragma("pack(pop)")
#else
#define PACKED_BEGIN __pragma(pack(push, 1))
#define PACKED_END __pragma(pack(pop))
#endif

#endif

#define OSAL_THREAD_HANDLE HANDLE
#define OSAL_THREAD_FUNC void
#define OSAL_THREAD_FUNC_RT void

#define osal_mutext CRITICAL_SECTION

#ifdef __cplusplus
}
#endif

#endif
