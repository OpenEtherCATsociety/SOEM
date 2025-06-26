/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for ec_print.c
 */

#ifndef _ec_print_
#define _ec_print_

#ifdef __cplusplus
extern "C" {
#endif

const char *ec_sdoerror2string(uint32 sdoerrorcode);
char *ec_ALstatuscode2string(uint16 ALstatuscode);
char *ec_soeerror2string(uint16 errorcode);
char *ec_mbxerror2string(uint16 errorcode);
char *ecx_err2string(const ec_errort Ec);
char *ecx_elist2string(ecx_contextt *context);

#ifdef __cplusplus
}
#endif

#endif
