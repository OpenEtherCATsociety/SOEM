/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for ec_soe.c
 */

#ifndef _ec_soe_
#define _ec_soe_

#ifdef __cplusplus
extern "C" {
#endif

#define EC_SOE_DATASTATE_B 0x01
#define EC_SOE_NAME_B      0x02
#define EC_SOE_ATTRIBUTE_B 0x04
#define EC_SOE_UNIT_B      0x08
#define EC_SOE_MIN_B       0x10
#define EC_SOE_MAX_B       0x20
#define EC_SOE_VALUE_B     0x40
#define EC_SOE_DEFAULT_B   0x80

#define EC_IDN_MDTCONFIG   24
#define EC_IDN_ATCONFIG    16

/** SoE name structure */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED
{
   /** current length in bytes of list */
   uint16 currentlength;
   /** maximum length in bytes of list */
   uint16 maxlength;
   char name[EC_SOE_MAXNAME];
} ec_SoEnamet;
OSAL_PACKED_END

/** SoE list structure */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED
{
   /** current length in bytes of list */
   uint16 currentlength;
   /** maximum length in bytes of list */
   uint16 maxlength;
   union
   {
      uint8 byte[8];
      uint16 word[4];
      uint32 dword[2];
      uint64 lword[1];
   };
} ec_SoElistt;
OSAL_PACKED_END

/** SoE IDN mapping structure */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED
{
   /** current length in bytes of list */
   uint16 currentlength;
   /** maximum length in bytes of list */
   uint16 maxlength;
   uint16 idn[EC_SOE_MAXMAPPING];
} ec_SoEmappingt;
OSAL_PACKED_END

#define EC_SOE_LENGTH_1       0x00
#define EC_SOE_LENGTH_2       0x01
#define EC_SOE_LENGTH_4       0x02
#define EC_SOE_LENGTH_8       0x03
#define EC_SOE_TYPE_BINARY    0x00
#define EC_SOE_TYPE_UINT      0x01
#define EC_SOE_TYPE_INT       0x02
#define EC_SOE_TYPE_HEX       0x03
#define EC_SOE_TYPE_STRING    0x04
#define EC_SOE_TYPE_IDN       0x05
#define EC_SOE_TYPE_FLOAT     0x06
#define EC_SOE_TYPE_PARAMETER 0x07

/** SoE attribute structure */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED
{
   /** evaluation factor for display purposes */
   uint32 evafactor : 16;
   /** length of IDN element(s) */
   uint32 length : 2;
   /** IDN is list */
   uint32 list : 1;
   /** IDN is command */
   uint32 command : 1;
   /** datatype */
   uint32 datatype : 3;
   uint32 reserved1 : 1;
   /** decimals to display if float datatype */
   uint32 decimals : 4;
   /** write protected in pre-op */
   uint32 wppreop : 1;
   /** write protected in safe-op */
   uint32 wpsafeop : 1;
   /** write protected in op */
   uint32 wpop : 1;
   uint32 reserved2 : 1;
} ec_SoEattributet;
OSAL_PACKED_END

int ecx_SoEread(ecx_contextt *context, uint16 slave, uint8 driveNo, uint8 elementflags, uint16 idn, int *psize, void *p, int timeout);
int ecx_SoEwrite(ecx_contextt *context, uint16 slave, uint8 driveNo, uint8 elementflags, uint16 idn, int psize, void *p, int timeout);
int ecx_readIDNmap(ecx_contextt *context, uint16 slave, uint32 *Osize, uint32 *Isize);

#ifdef __cplusplus
}
#endif

#endif
