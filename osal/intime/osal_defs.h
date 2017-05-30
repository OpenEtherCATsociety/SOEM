/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#ifndef _osal_defs_
#define _osal_defs_

#ifdef __cplusplus
extern "C"
{
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
