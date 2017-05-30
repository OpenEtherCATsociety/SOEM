/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatfoe.c
 */

#ifndef _ethercatfoe_
#define _ethercatfoe_

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef EC_VER1
int ec_FOEdefinehook(void *hook);
int ec_FOEread(uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout);
int ec_FOEwrite(uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout);
#endif

int ecx_FOEdefinehook(ecx_contextt *context, void *hook);
int ecx_FOEread(ecx_contextt *context, uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout);
int ecx_FOEwrite(ecx_contextt *context, uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout);

#ifdef __cplusplus
}
#endif

#endif
