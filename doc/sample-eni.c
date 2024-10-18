#include "osal.h"
#include "oshw.h"
#include "ethercattype.h"
#include "ethercatmain.h"

static uint8 s1_coeData[6] = {
   0,
   2, 26,
   5, 26,
   2
};

static ec_enicoecmdt s1_coeCmds[4] = {
   {
      // Clear SM3 PDO count
      .Transition = ECT_ESMTRANS_PS,
      .CA = FALSE,
      .Ccs = 2,
      .Index = 0x1C13,
      .SubIdx = 0,
      .Timeout = 3000,
      .DataSize = 1,
      .Data = (s1_coeData + 0)
   },
   {
      // SM3 RxPDO #1: 0x1A02
      .Transition = ECT_ESMTRANS_PS,
      .CA = FALSE,
      .Ccs = 2,
      .Index = 0x1C13,
      .SubIdx = 1,
      .Timeout = 3000,
      .DataSize = 2,
      .Data = (s1_coeData + 1)
   },
   {
      // SM3 RxPDO #2: 0x1A05
      .Transition = ECT_ESMTRANS_PS,
      .CA = FALSE,
      .Ccs = 2,
      .Index = 0x1C13,
      .SubIdx = 2,
      .Timeout = 3000,
      .DataSize = 2,
      .Data = (s1_coeData + 3)
   },
   {
      // Set SM3 PDO count
      .Transition = ECT_ESMTRANS_PS,
      .CA = FALSE,
      .Ccs = 2,
      .Index = 0x1C13,
      .SubIdx = 0,
      .Timeout = 3000,
      .DataSize = 1,
      .Data = (s1_coeData + 5)
   }
};

static uint8 s2_coeData[8] = {
   3, 0, 1, 22, 7, 22, 28, 22
};

static ec_enicoecmdt s2_coeCmds[1] = {
   {
      // SM2 PDO assignment, using CA
      .Transition = ECT_ESMTRANS_PS,
      .CA = TRUE,
      .Ccs = 2,
      .Index = 0x1C12,
      .SubIdx = 0,
      .Timeout = 3000,
      .DataSize = 8,
      .Data = (s2_coeData + 0)
   }
};

static ec_enislavet eniSlave[2] = {
   {
      .Slave = 1,
      .VendorId = 1,
      .ProductCode = 4660,
      .RevisionNo = 0,
      .CoECmds = s1_coeCmds,
      .CoECmdCount = 4
   },
   {
      .Slave = 2,
      .VendorId = 1,
      .ProductCode = 16799146,
      .RevisionNo = 0,
      .CoECmds = s2_coeCmds,
      .CoECmdCount = 1
   }
};

ec_enit ec_eni = {
   .slave = eniSlave,
   .slavecount = 2
};

