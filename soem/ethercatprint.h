/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatprint.c
 */

#ifndef _ethercatprint_
#define _ethercatprint_

#ifdef __cplusplus
extern "C"
{
#endif

char* ec_sdoerror2string( uint32 sdoerrorcode);
char* ec_ALstatuscode2string( uint16 ALstatuscode);
char* ec_soeerror2string( uint16 errorcode);
char* ec_mbxerror2string( uint16 errorcode);
char* ecx_err2string(const ec_errort Ec);
char* ecx_elist2string(ecx_contextt *context);

#ifdef EC_VER1
char* ec_elist2string(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
