/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for ec_foe.c
 */

#ifndef _ec_foe_
#define _ec_foe_

#ifdef __cplusplus
extern "C" {
#endif

int ecx_FOEdefinehook(ecx_contextt *context, void *hook);
int ecx_FOEread(ecx_contextt *context, uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout);
int ecx_FOEwrite(ecx_contextt *context, uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout);

#ifdef __cplusplus
}
#endif

#endif
