/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for ec_main.c
 */

#ifndef _ec_main_
#define _ec_main_

#ifdef __cplusplus
extern "C" {
#endif

#include "soem/ec_options.h"

typedef struct ec_adapter ec_adaptert;
struct ec_adapter
{
   char name[EC_MAXLEN_ADAPTERNAME];
   char desc[EC_MAXLEN_ADAPTERNAME];
   ec_adaptert *next;
};

/** record for FMMU */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED ec_fmmu
{
   uint32 LogStart;
   uint16 LogLength;
   uint8 LogStartbit;
   uint8 LogEndbit;
   uint16 PhysStart;
   uint8 PhysStartBit;
   uint8 FMMUtype;
   uint8 FMMUactive;
   uint8 unused1;
   uint16 unused2;
} ec_fmmut;
OSAL_PACKED_END

/** record for sync manager */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED ec_sm
{
   uint16 StartAddr;
   uint16 SMlength;
   uint32 SMflags;
} ec_smt;
OSAL_PACKED_END

OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED ec_state_status
{
   uint16 State;
   uint16 Unused;
   uint16 ALstatuscode;
} ec_state_status;
OSAL_PACKED_END

/** mailbox buffer array */
typedef uint8 ec_mbxbuft[EC_MAXMBX + 1];

#define EC_MBXINENABLE (uint8 *)1

typedef struct
{
   int listhead, listtail, listcount;
   int mbxemptylist[EC_MBXPOOLSIZE];
   osal_mutext *mbxmutex;
   ec_mbxbuft mbx[EC_MBXPOOLSIZE];
} ec_mbxpoolt;

#define EC_MBXQUEUESTATE_NONE 0
#define EC_MBXQUEUESTATE_REQ  1
#define EC_MBXQUEUESTATE_FAIL 2
#define EC_MBXQUEUESTATE_DONE 3

typedef struct
{
   int listhead, listtail, listcount;
   ec_mbxbuft *mbx[EC_MBXPOOLSIZE];
   int mbxstate[EC_MBXPOOLSIZE];
   int mbxremove[EC_MBXPOOLSIZE];
   int mbxticket[EC_MBXPOOLSIZE];
   uint16 mbxslave[EC_MBXPOOLSIZE];
   osal_mutext *mbxmutex;
} ec_mbxqueuet;

#define ECT_MBXPROT_AOE      0x0001
#define ECT_MBXPROT_EOE      0x0002
#define ECT_MBXPROT_COE      0x0004
#define ECT_MBXPROT_FOE      0x0008
#define ECT_MBXPROT_SOE      0x0010
#define ECT_MBXPROT_VOE      0x0020

#define ECT_COEDET_SDO       0x01
#define ECT_COEDET_SDOINFO   0x02
#define ECT_COEDET_PDOASSIGN 0x04
#define ECT_COEDET_PDOCONFIG 0x08
#define ECT_COEDET_UPLOAD    0x10
#define ECT_COEDET_SDOCA     0x20

#define EC_SMENABLEMASK      0xfffeffff

typedef struct ecx_context ecx_contextt;

#define ECT_MBXH_NONE   0
#define ECT_MBXH_CYCLIC 1
#define ECT_MBXH_LOST   2

/** Slave state
 * All slave information is put in this structure. Needed for most
 * user interaction with slaves.
 */
typedef struct ec_slave
{
   /** state of slave */
   uint16 state;
   /** AL status code */
   uint16 ALstatuscode;
   /** Configured address */
   uint16 configadr;
   /** Alias address */
   uint16 aliasadr;
   /** Manufacturer from EEprom */
   uint32 eep_man;
   /** ID from EEprom */
   uint32 eep_id;
   /** revision from EEprom */
   uint32 eep_rev;
   /** serial number from EEprom */
   uint32 eep_ser;
   /** Interface type */
   uint16 Itype;
   /** Device type */
   uint16 Dtype;
   /** output bits */
   uint16 Obits;
   /** output bytes, if Obits < 8 then Obytes = 0 */
   uint32 Obytes;
   /** output pointer in IOmap buffer */
   uint8 *outputs;
   /** output offset in IOmap buffer */
   uint32 Ooffset;
   /** startbit in first output byte */
   uint8 Ostartbit;
   /** input bits */
   uint16 Ibits;
   /** input bytes, if Ibits < 8 then Ibytes = 0 */
   uint32 Ibytes;
   /** input pointer in IOmap buffer */
   uint8 *inputs;
   /** input offset in IOmap buffer */
   uint32 Ioffset;
   /** startbit in first input byte */
   uint8 Istartbit;
   /** SM structure */
   ec_smt SM[EC_MAXSM];
   /** SM type 0=unused 1=MbxWr 2=MbxRd 3=Outputs 4=Inputs */
   uint8 SMtype[EC_MAXSM];
   /** FMMU structure */
   ec_fmmut FMMU[EC_MAXFMMU];
   /** FMMU0 function 0=unused 1=outputs 2=inputs 3=SM status*/
   uint8 FMMU0func;
   /** FMMU1 function */
   uint8 FMMU1func;
   /** FMMU2 function */
   uint8 FMMU2func;
   /** FMMU3 function */
   uint8 FMMU3func;
   /** length of write mailbox in bytes, if no mailbox then 0 */
   uint16 mbx_l;
   /** mailbox write offset */
   uint16 mbx_wo;
   /** length of read mailbox in bytes */
   uint16 mbx_rl;
   /** mailbox read offset */
   uint16 mbx_ro;
   /** mailbox supported protocols */
   uint16 mbx_proto;
   /** Counter value of mailbox link layer protocol 1..7 */
   uint8 mbx_cnt;
   /** has DC capability */
   boolean hasdc;
   /** Physical type; Ebus, EtherNet combinations */
   uint8 ptype;
   /** topology: 1 to 3 links */
   uint8 topology;
   /** active ports bitmap : ....3210 , set if respective port is active **/
   uint8 activeports;
   /** consumed ports bitmap : ....3210, used for internal delay measurement **/
   uint8 consumedports;
   /** slave number for parent, 0=master */
   uint16 parent;
   /** port number on parent this slave is connected to **/
   uint8 parentport;
   /** port number on this slave the parent is connected to **/
   uint8 entryport;
   /** DC receivetimes on port A */
   int32 DCrtA;
   /** DC receivetimes on port B */
   int32 DCrtB;
   /** DC receivetimes on port C */
   int32 DCrtC;
   /** DC receivetimes on port D */
   int32 DCrtD;
   /** propagation delay */
   int32 pdelay;
   /** next DC slave */
   uint16 DCnext;
   /** previous DC slave */
   uint16 DCprevious;
   /** DC cycle time in ns */
   int32 DCcycle;
   /** DC shift from clock modulus boundary */
   int32 DCshift;
   /** DC sync activation, 0=off, 1=on */
   uint8 DCactive;
   /** link to SII config */
   uint16 SIIindex;
   /** 1 = 8 bytes per read, 0 = 4 bytes per read */
   uint8 eep_8byte;
   /** 0 = eeprom to master , 1 = eeprom to PDI */
   uint8 eep_pdi;
   /** CoE details */
   uint8 CoEdetails;
   /** FoE details */
   uint8 FoEdetails;
   /** EoE details */
   uint8 EoEdetails;
   /** SoE details */
   uint8 SoEdetails;
   /** E-bus current */
   int16 Ebuscurrent;
   /** if >0 block use of LRW in processdata */
   uint8 blockLRW;
   /** group */
   uint8 group;
   /** first unused FMMU */
   uint8 FMMUunused;
   /** Boolean for tracking whether the slave is (not) responding, not used/set by the SOEM library */
   boolean islost;
   /** registered configuration function PO->SO */
   int (*PO2SOconfig)(ecx_contextt *context, uint16 slave);
   /** mailbox handler state, 0 = no handler, 1 = cyclic task mbx handler, 2 = slave lost */
   int mbxhandlerstate;
   /** mailbox handler robust mailbox protocol state */
   int mbxrmpstate;
   /** mailbox handler RMP extended mbx in state */
   uint16 mbxinstateex;
   /** pointer to CoE mailbox in buffer */
   uint8 *coembxin;
   /** CoE mailbox in flag, true = mailbox full */
   boolean coembxinfull;
   /** CoE mailbox in overrun counter */
   int coembxoverrun;
   /** pointer to SoE mailbox in buffer */
   uint8 *soembxin;
   /** SoE mailbox in flag, true = mailbox full */
   boolean soembxinfull;
   /** SoE mailbox in overrun counter */
   int soembxoverrun;
   /** pointer to FoE mailbox in buffer */
   uint8 *foembxin;
   /** FoE mailbox in flag, true = mailbox full */
   boolean foembxinfull;
   /** FoE mailbox in overrun counter */
   int foembxoverrun;
   /** pointer to EoE mailbox in buffer */
   uint8 *eoembxin;
   /** EoE mailbox in flag, true = mailbox full */
   boolean eoembxinfull;
   /** EoE mailbox in overrun counter */
   int eoembxoverrun;
   /** pointer to VoE mailbox in buffer */
   uint8 *voembxin;
   /** VoE mailbox in flag, true = mailbox full */
   boolean voembxinfull;
   /** VoE mailbox in overrun counter */
   int voembxoverrun;
   /** pointer to AoE mailbox in buffer */
   uint8 *aoembxin;
   /** AoE mailbox in flag, true = mailbox full */
   boolean aoembxinfull;
   /** AoE mailbox in overrun counter */
   int aoembxoverrun;
   /** pointer to out mailbox status register buffer */
   uint8 *mbxstatus;
   /** readable name */
   char name[EC_MAXNAME + 1];
} ec_slavet;

/** for list of ethercat slave groups */
typedef struct ec_group
{
   /** logical start address for this group */
   uint32 logstartaddr;
   /** output bytes, if Obits < 8 then Obytes = 0 */
   uint32 Obytes;
   /** output pointer in IOmap buffer */
   uint8 *outputs;
   /** input bytes, if Ibits < 8 then Ibytes = 0 */
   uint32 Ibytes;
   /** input pointer in IOmap buffer */
   uint8 *inputs;
   /** has DC capability */
   boolean hasdc;
   /** next DC slave */
   uint16 DCnext;
   /** E-bus current */
   int16 Ebuscurrent;
   /** if >0 block use of LRW in processdata */
   uint8 blockLRW;
   /** IO segments used */
   uint16 nsegments;
   /** 1st input segment */
   uint16 Isegment;
   /** Offset in input segment */
   uint16 Ioffset;
   /** Expected workcounter outputs */
   uint16 outputsWKC;
   /** Expected workcounter inputs */
   uint16 inputsWKC;
   /** check slave states */
   boolean docheckstate;
   /** IO segmentation list. Datagrams must not break SM in two. */
   uint32 IOsegment[EC_MAXIOSEGMENTS];
   /** pointer to out mailbox status register buffer */
   uint8 *mbxstatus;
   /** mailbox status register buffer length */
   int32 mbxstatuslength;
   /** mailbox status lookup table */
   uint16 mbxstatuslookup[EC_MAXSLAVE];
   /** mailbox last handled in mxbhandler */
   uint16 lastmbxpos;
   /** mailbox  transmit queue struct */
   ec_mbxqueuet mbxtxqueue;
} ec_groupt;

#define ECT_ESMTRANS_IP 0x0001
#define ECT_ESMTRANS_PS 0x0002
#define ECT_ESMTRANS_PI 0x0004
#define ECT_ESMTRANS_SP 0x0008
#define ECT_ESMTRANS_SO 0x0010
#define ECT_ESMTRANS_SI 0x0020
#define ECT_ESMTRANS_OS 0x0040
#define ECT_ESMTRANS_OP 0x0080
#define ECT_ESMTRANS_OI 0x0100
#define ECT_ESMTRANS_IB 0x0200
#define ECT_ESMTRANS_BI 0x0400
#define ECT_ESMTRANS_II 0x0800
#define ECT_ESMTRANS_PP 0x1000
#define ECT_ESMTRANS_SS 0x2000

/** ENI CoE command structure */
typedef struct ec_enicoecmd
{
   /** transition(s) during which command should be sent */
   uint16 Transition;
   /** complete access flag */
   boolean CA;
   /** ccs (1 = read, 2 = write) */
   uint8 Ccs;
   /** object index */
   uint16 Index;
   /** object subindex */
   uint8 SubIdx;
   /** timeout in us */
   int Timeout;
   /** size in bytes of parameter buffer */
   int DataSize;
   /** pointer to parameter buffer */
   void *Data;
} ec_enicoecmdt;

/** ENI slave structure */
typedef struct ec_enislave
{
   uint16 Slave;
   uint32 VendorId;
   uint32 ProductCode;
   uint32 RevisionNo;
   ec_enicoecmdt *CoECmds;
   int CoECmdCount;
} ec_enislavet;

/** ENI structure */
typedef struct ec_eni
{
   ec_enislavet *slave;
   int slavecount;
} ec_enit;

/** SII FMMU structure */
typedef struct ec_eepromFMMU
{
   uint16 Startpos;
   uint8 nFMMU;
   uint8 FMMU0;
   uint8 FMMU1;
   uint8 FMMU2;
   uint8 FMMU3;
} ec_eepromFMMUt;

/** SII SM structure */
typedef struct ec_eepromSM
{
   uint16 Startpos;
   uint8 nSM;
   uint16 PhStart;
   uint16 Plength;
   uint8 Creg;
   uint8 Sreg; /* don't care */
   uint8 Activate;
   uint8 PDIctrl; /* don't care */
} ec_eepromSMt;

/** record to store rxPDO and txPDO table from eeprom */
typedef struct ec_eepromPDO
{
   uint16 Startpos;
   uint16 Length;
   uint16 nPDO;
   uint16 Index[EC_MAXEEPDO];
   uint16 SyncM[EC_MAXEEPDO];
   uint16 BitSize[EC_MAXEEPDO];
   uint16 SMbitsize[EC_MAXSM];
} ec_eepromPDOt;

/** standard ethercat mailbox header */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED ec_mbxheader
{
   uint16 length;
   uint16 address;
   uint8 priority;
   uint8 mbxtype;
} ec_mbxheadert;
OSAL_PACKED_END

/** ALstatus and ALstatus code */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED ec_alstatus
{
   uint16 alstatus;
   uint16 unused;
   uint16 alstatuscode;
} ec_alstatust;
OSAL_PACKED_END

/** stack structure to store segmented LRD/LWR/LRW constructs */
typedef struct ec_idxstack
{
   uint8 pushed;
   uint8 pulled;
   uint8 idx[EC_MAXBUF];
   void *data[EC_MAXBUF];
   uint16 length[EC_MAXBUF];
   uint16 dcoffset[EC_MAXBUF];
   uint8 type[EC_MAXBUF];
} ec_idxstackT;

/** ringbuf for error storage */
typedef struct ec_ering
{
   int16 head;
   int16 tail;
   ec_errort Error[EC_MAXELIST + 1];
} ec_eringt;

/** SyncManager Communication Type structure for CA */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED ec_SMcommtype
{
   uint8 n;
   uint8 nu1;
   uint8 SMtype[EC_MAXSM];
} ec_SMcommtypet;
OSAL_PACKED_END

/** SDO assign structure for CA */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED ec_PDOassign
{
   uint8 n;
   uint8 nu1;
   uint16 index[256];
} ec_PDOassignt;
OSAL_PACKED_END

/** SDO description structure for CA */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED ec_PDOdesc
{
   uint8 n;
   uint8 nu1;
   uint32 PDO[256];
} ec_PDOdesct;
OSAL_PACKED_END

/** Context structure, referenced by all ecx functions*/
struct ecx_context
{
   /** @publicsection */
   /* Network state */

   /** port, may include red_port */
   ecx_portt port;
   /** list of detected slaves */
   ec_slavet slavelist[EC_MAXSLAVE];
   /** number of slaves found in configuration */
   int slavecount;
   /** list of groups */
   ec_groupt grouplist[EC_MAXGROUP];
   /** ecaterror state */
   boolean ecaterror;
   /** last DC time from slaves */
   int64 DCtime;

   /** @privatesection */
   /* Internal state */

   /** internal, eeprom cache buffer */
   uint8 esibuf[EC_MAXEEPBUF];
   /** internal, eeprom cache map */
   uint32 esimap[EC_MAXEEPBITMAP];
   /** internal, current slave for eeprom cache */
   uint16 esislave;
   /** internal, error list */
   ec_eringt elist;
   /** internal, processdata stack buffer info */
   ec_idxstackT idxstack;
   /** internal, SM buffer */
   ec_SMcommtypet SMcommtype[EC_MAX_MAPT];
   /** internal, PDO assign list */
   ec_PDOassignt PDOassign[EC_MAX_MAPT];
   /** internal, PDO description list */
   ec_PDOdesct PDOdesc[EC_MAX_MAPT];
   /** internal, SM list from eeprom */
   ec_eepromSMt eepSM;
   /** internal, FMMU list from eeprom */
   ec_eepromFMMUt eepFMMU;
   /** internal, mailbox pool */
   ec_mbxpoolt mbxpool;

   /** @publicsection */
   /* Configurable settings */

   /** network information hook */
   ec_enit *ENI;
   /** registered FoE hook */
   int (*FOEhook)(uint16 slave, int packetnumber, int datasize);
   /** registered EoE hook */
   int (*EOEhook)(ecx_contextt *context, uint16 slave, void *eoembx);
   /** flag to control legacy automatic state change or manual state change */
   int manualstatechange;
   /** opaque pointer to application userdata, never used by SOEM. */
   void *userdata;
   /** In overlapped mode, inputs will replace outputs in the incoming
    * frame. Use this mode for TI ESC:s. Processdata is always aligned
    * on a byte boundary. */
   boolean overlappedMode;
   /** Do not map each slave on a byte boundary. May result in smaller
    * frame sizes. Has no effect in overlapped mode. */
   boolean packedMode;
};

ec_adaptert *ec_find_adapters(void);
void ec_free_adapters(ec_adaptert *adapter);
uint8 ec_nextmbxcnt(uint8 cnt);
void ec_clearmbx(ec_mbxbuft *Mbx);
void ecx_pusherror(ecx_contextt *context, const ec_errort *Ec);
boolean ecx_poperror(ecx_contextt *context, ec_errort *Ec);
boolean ecx_iserror(ecx_contextt *context);
void ecx_packeterror(ecx_contextt *context, uint16 Slave, uint16 Index, uint8 SubIdx, uint16 ErrorCode);
int ecx_init(ecx_contextt *context, const char *ifname);
int ecx_init_redundant(ecx_contextt *context, ecx_redportt *redport, const char *ifname, char *if2name);
void ecx_close(ecx_contextt *context);
uint8 ecx_siigetbyte(ecx_contextt *context, uint16 slave, uint16 address);
int16 ecx_siifind(ecx_contextt *context, uint16 slave, uint16 cat);
void ecx_siistring(ecx_contextt *context, char *str, uint16 slave, uint16 Sn);
uint16 ecx_siiFMMU(ecx_contextt *context, uint16 slave, ec_eepromFMMUt *FMMU);
uint16 ecx_siiSM(ecx_contextt *context, uint16 slave, ec_eepromSMt *SM);
uint16 ecx_siiSMnext(ecx_contextt *context, uint16 slave, ec_eepromSMt *SM, uint16 n);
uint32 ecx_siiPDO(ecx_contextt *context, uint16 slave, ec_eepromPDOt *PDO, uint8 t);
int ecx_readstate(ecx_contextt *context);
int ecx_writestate(ecx_contextt *context, uint16 slave);
uint16 ecx_statecheck(ecx_contextt *context, uint16 slave, uint16 reqstate, int timeout);
int ecx_mbxhandler(ecx_contextt *context, uint8 group, int limit);
int ecx_mbxempty(ecx_contextt *context, uint16 slave, int timeout);
int ecx_mbxsend(ecx_contextt *context, uint16 slave, ec_mbxbuft *mbx, int timeout);
int ecx_mbxreceive(ecx_contextt *context, uint16 slave, ec_mbxbuft **mbx, int timeout);
int ecx_mbxENIinitcmds(ecx_contextt *context, uint16 slave, uint16_t transition);
void ecx_esidump(ecx_contextt *context, uint16 slave, uint8 *esibuf);
uint32 ecx_readeeprom(ecx_contextt *context, uint16 slave, uint16 eeproma, int timeout);
int ecx_writeeeprom(ecx_contextt *context, uint16 slave, uint16 eeproma, uint16 data, int timeout);
int ecx_eeprom2master(ecx_contextt *context, uint16 slave);
int ecx_eeprom2pdi(ecx_contextt *context, uint16 slave);
uint64 ecx_readeepromAP(ecx_contextt *context, uint16 aiadr, uint16 eeproma, int timeout);
int ecx_writeeepromAP(ecx_contextt *context, uint16 aiadr, uint16 eeproma, uint16 data, int timeout);
uint64 ecx_readeepromFP(ecx_contextt *context, uint16 configadr, uint16 eeproma, int timeout);
int ecx_writeeepromFP(ecx_contextt *context, uint16 configadr, uint16 eeproma, uint16 data, int timeout);
void ecx_readeeprom1(ecx_contextt *context, uint16 slave, uint16 eeproma);
uint32 ecx_readeeprom2(ecx_contextt *context, uint16 slave, int timeout);
int ecx_receive_processdata_group(ecx_contextt *context, uint8 group, int timeout);
int ecx_send_processdata(ecx_contextt *context);
int ecx_receive_processdata(ecx_contextt *context, int timeout);
int ecx_send_processdata_group(ecx_contextt *context, uint8 group);
ec_mbxbuft *ecx_getmbx(ecx_contextt *context);
int ecx_dropmbx(ecx_contextt *context, ec_mbxbuft *mbx);
int ecx_initmbxpool(ecx_contextt *context);
int ecx_initmbxqueue(ecx_contextt *context, uint8 group);
int ecx_slavembxcyclic(ecx_contextt *context, uint16 slave);

#ifdef __cplusplus
}
#endif

#endif
