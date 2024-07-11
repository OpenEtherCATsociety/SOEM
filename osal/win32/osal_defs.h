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

// define if debug printf is needed
//#define EC_DEBUG

#ifdef EC_DEBUG
#define EC_PRINT printf
#else
#define EC_PRINT(...) do {} while (0)
#endif

#define SOEM_PACKED_BEGIN __pragma(pack(push, 1))
#define SOEM_PACKED
#define SOEM_PACKED_END __pragma(pack(pop))

#define OSAL_THREAD_HANDLE HANDLE
#define OSAL_THREAD_FUNC void
#define OSAL_THREAD_FUNC_RT void

#ifdef __cplusplus
}
#endif

#endif
