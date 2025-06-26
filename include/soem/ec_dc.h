/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for ec_dc.c
 */

#ifndef _EC_ECATDC_H
#define _EC_ECATDC_H

#ifdef __cplusplus
extern "C" {
#endif

boolean ecx_configdc(ecx_contextt *context);
void ecx_dcsync0(ecx_contextt *context, uint16 slave, boolean act, uint32 CyclTime, int32 CyclShift);
void ecx_dcsync01(ecx_contextt *context, uint16 slave, boolean act, uint32 CyclTime0, uint32 CyclTime1, int32 CyclShift);

#ifdef __cplusplus
}
#endif

#endif /* _EC_ECATDC_H */
