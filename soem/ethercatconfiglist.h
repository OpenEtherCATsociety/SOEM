/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * DEPRICATED Configuration list of known EtherCAT slave devices.
 *
 * If a slave is found in this list it is configured according to the parameters
 * in the list. Otherwise the configuration info is read directly from the slave
 * EEPROM (SII or Slave Information Interface).
 */

#ifndef _ethercatconfiglist_
#define _ethercatconfiglist_

#ifdef __cplusplus
extern "C"
{
#endif

/*
   explanation of dev:
    1: static device with no IO mapping ie EK1100
    2: input device no mailbox ie simple IO device
    3: output device no mailbox
    4: input device with mailbox configuration
    5: output device with mailbox configuration
    6: input/output device no mailbox
    7: input.output device with mailbox configuration
*/
#define EC_CONFIGEND 0xffffffff

ec_configlist_t ec_configlist[] = {
      {/*Man=*/0x00000000,/*ID=*/0x00000000,/*Name=*/""          ,/*dtype=*/0,/*Ibits=*/ 0,/*Obits=*/ 0,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x044c2c52,/*Name=*/"EK1100"    ,/*dtype=*/1,/*Ibits=*/ 0,/*Obits=*/ 0,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x03ea3052,/*Name=*/"EL1002"    ,/*dtype=*/2,/*Ibits=*/ 2,/*Obits=*/ 0,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x03ec3052,/*Name=*/"EL1004"    ,/*dtype=*/2,/*Ibits=*/ 4,/*Obits=*/ 0,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x03f43052,/*Name=*/"EL1012"    ,/*dtype=*/2,/*Ibits=*/ 2,/*Obits=*/ 0,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x03f63052,/*Name=*/"EL1014"    ,/*dtype=*/2,/*Ibits=*/ 4,/*Obits=*/ 0,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x03fa3052,/*Name=*/"EL1018"    ,/*dtype=*/2,/*Ibits=*/ 8,/*Obits=*/ 0,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x07d23052,/*Name=*/"EL2002"    ,/*dtype=*/3,/*Ibits=*/ 0,/*Obits=*/ 2,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x07d43052,/*Name=*/"EL2004"    ,/*dtype=*/3,/*Ibits=*/ 0,/*Obits=*/ 4,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x07d83052,/*Name=*/"EL2008"    ,/*dtype=*/3,/*Ibits=*/ 0,/*Obits=*/ 8,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x07f03052,/*Name=*/"EL2032"    ,/*dtype=*/6,/*Ibits=*/ 2,/*Obits=*/ 2,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x0c1e3052,/*Name=*/"EL3102"    ,/*dtype=*/4,/*Ibits=*/48,/*Obits=*/ 0,/*SM2a*/0x1000,/*SM2f*/0x00000024,/*SM3a*/0x1100,/*SM3f*/0x00010020,/*FM0ac*/0,/*FM1ac*/1},
      {/*Man=*/0x00000002,/*ID=*/0x0c283052,/*Name=*/"EL3112"    ,/*dtype=*/4,/*Ibits=*/48,/*Obits=*/ 0,/*SM2a*/0x1000,/*SM2f*/0x00000024,/*SM3a*/0x1100,/*SM3f*/0x00010020,/*FM0ac*/0,/*FM1ac*/1},
      {/*Man=*/0x00000002,/*ID=*/0x0c323052,/*Name=*/"EL3122"    ,/*dtype=*/4,/*Ibits=*/48,/*Obits=*/ 0,/*SM2a*/0x1000,/*SM2f*/0x00000024,/*SM3a*/0x1100,/*SM3f*/0x00010020,/*FM0ac*/0,/*FM1ac*/1},
      {/*Man=*/0x00000002,/*ID=*/0x0c463052,/*Name=*/"EL3142"    ,/*dtype=*/4,/*Ibits=*/48,/*Obits=*/ 0,/*SM2a*/0x1000,/*SM2f*/0x00000024,/*SM3a*/0x1100,/*SM3f*/0x00010020,/*FM0ac*/0,/*FM1ac*/1},
      {/*Man=*/0x00000002,/*ID=*/0x0c503052,/*Name=*/"EL3152"    ,/*dtype=*/4,/*Ibits=*/48,/*Obits=*/ 0,/*SM2a*/0x1000,/*SM2f*/0x00000024,/*SM3a*/0x1100,/*SM3f*/0x00010020,/*FM0ac*/0,/*FM1ac*/1},
      {/*Man=*/0x00000002,/*ID=*/0x0c5a3052,/*Name=*/"EL3162"    ,/*dtype=*/4,/*Ibits=*/48,/*Obits=*/ 0,/*SM2a*/0x1000,/*SM2f*/0x00000024,/*SM3a*/0x1100,/*SM3f*/0x00010020,/*FM0ac*/0,/*FM1ac*/1},
      {/*Man=*/0x00000002,/*ID=*/0x0fc03052,/*Name=*/"EL4032"    ,/*dtype=*/5,/*Ibits=*/ 0,/*Obits=*/32,/*SM2a*/0x1100,/*SM2f*/0x00010024,/*SM3a*/0x1180,/*SM3f*/0x00000022,/*FM0ac*/1,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x10063052,/*Name=*/"EL4102"    ,/*dtype=*/5,/*Ibits=*/ 0,/*Obits=*/32,/*SM2a*/0x1000,/*SM2f*/0x00010024,/*SM3a*/0x1100,/*SM3f*/0x00000022,/*FM0ac*/1,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x10103052,/*Name=*/"EL4112"    ,/*dtype=*/5,/*Ibits=*/ 0,/*Obits=*/32,/*SM2a*/0x1000,/*SM2f*/0x00010024,/*SM3a*/0x1100,/*SM3f*/0x00000022,/*FM0ac*/1,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x101a3052,/*Name=*/"EL4122"    ,/*dtype=*/5,/*Ibits=*/ 0,/*Obits=*/32,/*SM2a*/0x1000,/*SM2f*/0x00010024,/*SM3a*/0x1100,/*SM3f*/0x00000022,/*FM0ac*/1,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x10243052,/*Name=*/"EL4132"    ,/*dtype=*/5,/*Ibits=*/ 0,/*Obits=*/32,/*SM2a*/0x1000,/*SM2f*/0x00010024,/*SM3a*/0x1100,/*SM3f*/0x00000022,/*FM0ac*/1,/*FM1ac*/0},
      {/*Man=*/0x00000002,/*ID=*/0x13ed3052,/*Name=*/"EL5101"    ,/*dtype=*/7,/*Ibits=*/40,/*Obits=*/24,/*SM2a*/0x1000,/*SM2f*/0x00010024,/*SM3a*/0x1100,/*SM3f*/0x00010020,/*FM0ac*/1,/*FM1ac*/1},
      {/*Man=*/EC_CONFIGEND,/*ID=*/0x00000000,/*Name=*/""        ,/*dtype=*/0,/*Ibits=*/ 0,/*Obits=*/ 0,/*SM2a*/     0,/*SM2f*/         0,/*SM3a*/     0,/*SM3f*/         0,/*FM0ac*/0,/*FM1ac*/0}
};

#ifdef __cplusplus
}
#endif

#endif
