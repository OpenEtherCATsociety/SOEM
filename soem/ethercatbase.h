/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatbase.c
 */

#ifndef _ethercatbase_
#define _ethercatbase_

#ifdef __cplusplus
extern "C"
{
#endif

int ecx_setupdatagram(ecx_portt *port, void *frame, uint8 com, uint8 idx, uint16 ADP, uint16 ADO, uint16 length, void *data);
uint16 ecx_adddatagram(ecx_portt *port, void *frame, uint8 com, uint8 idx, boolean more, uint16 ADP, uint16 ADO, uint16 length, void *data);
int ecx_BWR(ecx_portt *port, uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int ecx_BRD(ecx_portt *port, uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
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

#ifdef EC_VER1
int ec_setupdatagram(void *frame, uint8 com, uint8 idx, uint16 ADP, uint16 ADO, uint16 length, void *data);
uint16 ec_adddatagram(void *frame, uint8 com, uint8 idx, boolean more, uint16 ADP, uint16 ADO, uint16 length, void *data);
int ec_BWR(uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int ec_BRD(uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int ec_APRD(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ec_ARMW(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ec_FRMW(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 ec_APRDw(uint16 ADP, uint16 ADO, int timeout);
int ec_FPRD(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 ec_FPRDw(uint16 ADP, uint16 ADO, int timeout);
int ec_APWRw(uint16 ADP, uint16 ADO, uint16 data, int timeout);
int ec_APWR(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ec_FPWRw(uint16 ADP, uint16 ADO, uint16 data, int timeout);
int ec_FPWR(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int ec_LRW(uint32 LogAdr, uint16 length, void *data, int timeout);
int ec_LRD(uint32 LogAdr, uint16 length, void *data, int timeout);
int ec_LWR(uint32 LogAdr, uint16 length, void *data, int timeout);
int ec_LRWDC(uint32 LogAdr, uint16 length, void *data, uint16 DCrs, int64 *DCtime, int timeout);
#endif

#ifdef __cplusplus
}
#endif

#endif
