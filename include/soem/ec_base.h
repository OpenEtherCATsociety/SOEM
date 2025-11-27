/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for ec_base.c
 */

#ifndef _ec_base_
#define _ec_base_

#ifdef __cplusplus
extern "C" {
#endif

int ecx_setupdatagram(ecx_portt *port, void *frame, uint8 com, uint8 idx, uint16 ADP, uint16 ADO, uint16 length, void *data);
uint16 ecx_adddatagram(ecx_portt *port, void *frame, uint8 com, uint8 idx, boolean more, uint16 ADP, uint16 ADO, uint16 length, void *data);
int ecx_BWR(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ecx_BRD(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ecx_APRD(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ecx_ARMW(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ecx_FRMW(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 ecx_APRDw(ecx_portt *port, uint16 ADP, uint16 ADO, int timeout);
int ecx_FPRD(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 ecx_FPRDw(ecx_portt *port, uint16 ADP, uint16 ADO, int timeout);
int ecx_APWRw(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 data, int timeout);
int ecx_APWR(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ecx_FPWRw(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 data, int timeout);
int ecx_FPWR(ecx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ecx_LRW(ecx_portt *port, uint32 LogAdr, uint16 length, void *data, int timeout);
int ecx_LRD(ecx_portt *port, uint32 LogAdr, uint16 length, void *data, int timeout);
int ecx_LWR(ecx_portt *port, uint32 LogAdr, uint16 length, void *data, int timeout);
int ecx_LRWDC(ecx_portt *port, uint32 LogAdr, uint16 length, void *data, uint16 DCrs, int64 *DCtime, int timeout);

#ifdef __cplusplus
}
#endif

#endif
