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
#define PACKED_BEGIN __pragma(pack(push, 1))
#define PACKED
#define PACKED_END __pragma(pack(pop))
#endif

#define OSAL_THREAD_HANDLE HANDLE
#define OSAL_THREAD_FUNC void
#define OSAL_THREAD_FUNC_RT void

#ifdef __cplusplus
}
#endif

#endif
