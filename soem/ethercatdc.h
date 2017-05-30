/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatdc.c
 */

#ifndef _EC_ECATDC_H
#define _EC_ECATDC_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef EC_VER1
boolean ec_configdc();
void ec_dcsync0(uint16 slave, boolean act, uint32 CyclTime, int32 CyclShift);
void ec_dcsync01(uint16 slave, boolean act, uint32 CyclTime0, uint32 CyclTime1, int32 CyclShift);
#endif

boolean ecx_configdc(ecx_contextt *context);
void ecx_dcsync0(ecx_contextt *context, uint16 slave, boolean act, uint32 CyclTime, int32 CyclShift);
void ecx_dcsync01(ecx_contextt *context, uint16 slave, boolean act, uint32 CyclTime0, uint32 CyclTime1, int32 CyclShift);

#ifdef __cplusplus
}
#endif

#endif /* _EC_ECATDC_H */
