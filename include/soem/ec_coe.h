/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for ec_coe.c
 */

#ifndef _ec_coe_
#define _ec_coe_

#ifdef __cplusplus
extern "C" {
#endif

/* Storage for object description list */
typedef struct
{
   /** slave number */
   uint16 Slave;
   /** number of entries in list */
   uint16 Entries;
   /** array of indexes */
   uint16 Index[EC_MAXODLIST];
   /** array of datatypes, see EtherCAT specification */
   uint16 DataType[EC_MAXODLIST];
   /** array of object codes, see EtherCAT specification */
   uint8 ObjectCode[EC_MAXODLIST];
   /** number of subindexes for each index */
   uint8 MaxSub[EC_MAXODLIST];
   /** textual description of each index */
   char Name[EC_MAXODLIST][EC_MAXNAME + 1];
} ec_ODlistt;

/* storage for object list entry information */
typedef struct
{
   /** number of entries in list */
   uint16 Entries;
   /** array of value infos, see EtherCAT specification */
   uint8 ValueInfo[EC_MAXOELIST];
   /** array of value infos, see EtherCAT specification */
   uint16 DataType[EC_MAXOELIST];
   /** array of bit lengths, see EtherCAT specification */
   uint16 BitLength[EC_MAXOELIST];
   /** array of object access bits, see EtherCAT specification */
   uint16 ObjAccess[EC_MAXOELIST];
   /** textual description of each index */
   char Name[EC_MAXOELIST][EC_MAXNAME + 1];
} ec_OElistt;

void ecx_SDOerror(ecx_contextt *context, uint16 Slave, uint16 Index, uint8 SubIdx, int32 AbortCode);
int ecx_SDOread(ecx_contextt *context, uint16 slave, uint16 index, uint8 subindex,
                boolean CA, int *psize, void *p, int timeout);
int ecx_SDOwrite(ecx_contextt *context, uint16 Slave, uint16 Index, uint8 SubIndex,
                 boolean CA, int psize, const void *p, int Timeout);
int ecx_RxPDO(ecx_contextt *context, uint16 Slave, uint16 RxPDOnumber, int psize, const void *p);
int ecx_TxPDO(ecx_contextt *context, uint16 slave, uint16 TxPDOnumber, int *psize, void *p, int timeout);
int ecx_readPDOmap(ecx_contextt *context, uint16 Slave, uint32 *Osize, uint32 *Isize);
int ecx_readPDOmapCA(ecx_contextt *context, uint16 Slave, int Thread_n, uint32 *Osize, uint32 *Isize);
int ecx_readODlist(ecx_contextt *context, uint16 Slave, ec_ODlistt *pODlist);
int ecx_readODdescription(ecx_contextt *context, uint16 Item, ec_ODlistt *pODlist);
int ecx_readOEsingle(ecx_contextt *context, uint16 Item, uint8 SubI, ec_ODlistt *pODlist, ec_OElistt *pOElist);
int ecx_readOE(ecx_contextt *context, uint16 Item, ec_ODlistt *pODlist, ec_OElistt *pOElist);

#ifdef __cplusplus
}
#endif

#endif
