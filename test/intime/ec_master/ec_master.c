/*****************************************************************************
*
* FILE NAME:		ec_master.c
*
* DESCRIPTION:		This is the main program module.
*
\*****************************************************************************/

#include <stdio.h>

/*****************************************************************************
*
* FUNCTION:			main
*
* DESCRIPTION:
*  This is the main program module.
*
\*****************************************************************************/

/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : slaveinfo [ifname] [-sdo] [-map]
 * Ifname is NIC interface, f.e. eth0.
 * Optional -sdo to display CoE object dictionary.
 * Optional -map to display slave PDO mapping
 *
 * This shows the configured slave data.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
#include <rt.h>
#include <traceapi.h>

#include "ethercat.h"

char IOmap[4096];
char IOmap2[4096];

/** Main slave data array.
 *  Each slave found on the network gets its own record.
 *  ec_slave[0] is reserved for the master. Structure gets filled
 *  in by the configuration function ec_config().
 */
ec_slavet ec_slave[EC_MAXSLAVE];
ec_slavet ec_slave2[EC_MAXSLAVE];
/** number of slaves found on the network */
int ec_slavecount;
int ec_slavecount2;
/** slave group structure */
ec_groupt ec_groups[EC_MAXGROUP];
ec_groupt ec_groups2[EC_MAXGROUP];

/** cache for EEPROM read functions */
static uint8 esibuf[EC_MAXEEPBUF];
static uint8 esibuf2[EC_MAXEEPBUF];
/** bitmap for filled cache buffer bytes */
static uint32 esimap[EC_MAXEEPBITMAP];
static uint32 esimap2[EC_MAXEEPBITMAP];
/** current slave for EEPROM cache buffer */
static ec_eringt ec_elist;
static ec_eringt ec_elist2;
static ec_idxstackT ec_idxstack;
static ec_idxstackT ec_idxstack2;

/** SyncManager Communication Type struct to store data of one slave */
static ec_SMcommtypet ec_SMcommtype;
static ec_SMcommtypet ec_SMcommtype2;
/** PDO assign struct to store data of one slave */
static ec_PDOassignt ec_PDOassign;
static ec_PDOassignt ec_PDOassign2;
/** PDO description struct to store data of one slave */
static ec_PDOdesct ec_PDOdesc;
static ec_PDOdesct ec_PDOdesc2;

/** buffer for EEPROM SM data */
static ec_eepromSMt ec_SM;
static ec_eepromSMt ec_SM2;
/** buffer for EEPROM FMMU data */
static ec_eepromFMMUt ec_FMMU;
static ec_eepromFMMUt ec_FMMU2;

/** Global variable TRUE if error available in error stack */
static boolean EcatError = FALSE;
static boolean EcatError2 = FALSE;

int64 ec_DCtime;
int64 ec_DCtime2;

static ecx_portt ecx_port;
static ecx_portt ecx_port2;

static ecx_contextt ctx[] = {
    {&ecx_port,
     &ec_slave[0],
     &ec_slavecount,
     EC_MAXSLAVE,
     &ec_groups[0],
     EC_MAXGROUP,
     &esibuf[0],
     &esimap[0],
     0,
     &ec_elist,
     &ec_idxstack,
     &EcatError,
     0,
     0,
     &ec_DCtime,
     &ec_SMcommtype,
     &ec_PDOassign,
     &ec_PDOdesc,
     &ec_SM,
     &ec_FMMU},
    {&ecx_port2,
     &ec_slave2[0],
     &ec_slavecount2,
     EC_MAXSLAVE,
     &ec_groups2[0],
     EC_MAXGROUP,
     &esibuf2[0],
     &esimap2[0],
     0,
     &ec_elist2,
     &ec_idxstack2,
     &EcatError2,
     0,
     0,
     &ec_DCtime2,
     &ec_SMcommtype2,
     &ec_PDOassign2,
     &ec_PDOdesc2,
     &ec_SM2,
     &ec_FMMU2}};

void slaveinfo(char *ifname)
{
   int cnt, i, j, nSM;
   int ctx_count;
   uint16 ssigen;
   int expectedWKC[2];
   int wkc_count;
   int chk;
   volatile int wkc[2];
   boolean inOP;

   printf("Starting slaveinfo\n");

   /* initialise SOEM, bind socket to ifname */
   if (ecx_init(&ctx[0], "ie1g1") && ecx_init(&ctx[1], "ie1g0"))
   {
      printf("ec_init on %s succeeded.\n", ifname);
      /* find and auto-config slaves */
      if (ecx_config_init(&ctx[0], FALSE) > 0 && ecx_config_init(&ctx[1], FALSE) > 0)
      {
         for (ctx_count = 0; ctx_count < 2; ctx_count++)
         {
            ecx_config_map_group(&ctx[ctx_count], IOmap, 0);
            ecx_configdc(&ctx[ctx_count]);
            while (*(ctx[ctx_count].ecaterror))
               printf("%s", ecx_elist2string(&ctx[ctx_count]));
            printf("%d slaves found and configured.\n", *(ctx[ctx_count].slavecount));
            expectedWKC[ctx_count] = (ctx[ctx_count].grouplist[0].outputsWKC * 2) + ctx[ctx_count].grouplist[0].inputsWKC;
            printf("Calculated workcounter %d\n", expectedWKC[ctx_count]);
            /* wait for all slaves to reach SAFE_OP state */
            ecx_statecheck(&ctx[ctx_count], 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 3);
            if (ctx[ctx_count].slavelist[ctx_count].state != EC_STATE_SAFE_OP)
            {
               printf("Not all slaves reached safe operational state.\n");
               ecx_readstate(&ctx[ctx_count]);
               for (i = 1; i <= *(ctx[ctx_count].slavecount); i++)
               {
                  if (ctx[ctx_count].slavelist[i].state != EC_STATE_SAFE_OP)
                  {
                     printf("Slave %d State=%2x StatusCode=%4x : %s\n",
                            i, ctx[ctx_count].slavelist[i].state, ctx[ctx_count].slavelist[i].ALstatuscode, ec_ALstatuscode2string(ctx[ctx_count].slavelist[i].ALstatuscode));
                  }
               }
            }
            ecx_readstate(&ctx[ctx_count]);

            for (cnt = 1; cnt <= *(ctx[ctx_count].slavecount); cnt++)
            {
               printf("\nSlave:%d\n Name:%s\n Output size: %dbits\n Input size: %dbits\n State: %d\n Delay: %d[ns]\n Has DC: %d\n",
                      cnt, ctx[ctx_count].slavelist[cnt].name, ctx[ctx_count].slavelist[cnt].Obits, ctx[ctx_count].slavelist[cnt].Ibits,
                      ctx[ctx_count].slavelist[cnt].state, ctx[ctx_count].slavelist[cnt].pdelay, ctx[ctx_count].slavelist[cnt].hasdc);
               if (ctx[ctx_count].slavelist[cnt].hasdc)
               {
                  printf(" DCParentport:%d\n", ctx[ctx_count].slavelist[cnt].parentport);
               }
               printf(" Activeports:%d.%d.%d.%d\n", (ctx[ctx_count].slavelist[cnt].activeports & 0x01) > 0,
                      (ctx[ctx_count].slavelist[cnt].activeports & 0x02) > 0,
                      (ctx[ctx_count].slavelist[cnt].activeports & 0x04) > 0,
                      (ctx[ctx_count].slavelist[cnt].activeports & 0x08) > 0);
               printf(" Configured address: %4.4x\n", ctx[ctx_count].slavelist[cnt].configadr);
               printf(" Man: %8.8x ID: %8.8x Rev: %8.8x\n", (int)ctx[ctx_count].slavelist[cnt].eep_man,
                      (int)ctx[ctx_count].slavelist[cnt].eep_id, (int)ctx[ctx_count].slavelist[cnt].eep_rev);
               for (nSM = 0; nSM < EC_MAXSM; nSM++)
               {
                  if (ctx[ctx_count].slavelist[cnt].SM[nSM].StartAddr > 0)
                     printf(" SM%1d A:%4.4x L:%4d F:%8.8x Type:%d\n", nSM, ctx[ctx_count].slavelist[cnt].SM[nSM].StartAddr,
                            ctx[ctx_count].slavelist[cnt].SM[nSM].SMlength, (int)ctx[ctx_count].slavelist[cnt].SM[nSM].SMflags,
                            ctx[ctx_count].slavelist[cnt].SMtype[nSM]);
               }
               for (j = 0; j < ctx[ctx_count].slavelist[cnt].FMMUunused; j++)
               {
                  printf(" FMMU%1d Ls:%8.8x Ll:%4d Lsb:%d Leb:%d Ps:%4.4x Psb:%d Ty:%2.2x Act:%2.2x\n", j,
                         (int)ctx[ctx_count].slavelist[cnt].FMMU[j].LogStart, ctx[ctx_count].slavelist[cnt].FMMU[j].LogLength,
                         ctx[ctx_count].slavelist[cnt].FMMU[j].LogStartbit, ctx[ctx_count].slavelist[cnt].FMMU[j].LogEndbit,
                         ctx[ctx_count].slavelist[cnt].FMMU[j].PhysStart, ctx[ctx_count].slavelist[cnt].FMMU[j].PhysStartBit,
                         ctx[ctx_count].slavelist[cnt].FMMU[j].FMMUtype, ctx[ctx_count].slavelist[cnt].FMMU[j].FMMUactive);
               }
               printf(" FMMUfunc 0:%d 1:%d 2:%d 3:%d\n",
                      ctx[ctx_count].slavelist[cnt].FMMU0func, ctx[ctx_count].slavelist[cnt].FMMU1func, ctx[ctx_count].slavelist[cnt].FMMU2func,
                      ctx[ctx_count].slavelist[cnt].FMMU3func);
               printf(" MBX length wr: %d rd: %d MBX protocols : %2.2x\n", ctx[ctx_count].slavelist[cnt].mbx_l,
                      ctx[ctx_count].slavelist[cnt].mbx_rl, ctx[ctx_count].slavelist[cnt].mbx_proto);
               ssigen = ecx_siifind(&ctx[ctx_count], cnt, ECT_SII_GENERAL);
               /* SII general section */
               if (ssigen)
               {
                  ctx[ctx_count].slavelist[cnt].CoEdetails = ecx_siigetbyte(&ctx[ctx_count], cnt, ssigen + 0x07);
                  ctx[ctx_count].slavelist[cnt].FoEdetails = ecx_siigetbyte(&ctx[ctx_count], cnt, ssigen + 0x08);
                  ctx[ctx_count].slavelist[cnt].EoEdetails = ecx_siigetbyte(&ctx[ctx_count], cnt, ssigen + 0x09);
                  ctx[ctx_count].slavelist[cnt].SoEdetails = ecx_siigetbyte(&ctx[ctx_count], cnt, ssigen + 0x0a);
                  if ((ecx_siigetbyte(&ctx[ctx_count], cnt, ssigen + 0x0d) & 0x02) > 0)
                  {
                     ctx[ctx_count].slavelist[cnt].blockLRW = 1;
                     ctx[ctx_count].slavelist[0].blockLRW++;
                  }
                  ctx[ctx_count].slavelist[cnt].Ebuscurrent = ecx_siigetbyte(&ctx[ctx_count], cnt, ssigen + 0x0e);
                  ctx[ctx_count].slavelist[cnt].Ebuscurrent += ecx_siigetbyte(&ctx[ctx_count], cnt, ssigen + 0x0f) << 8;
                  ctx[ctx_count].slavelist[0].Ebuscurrent += ctx[ctx_count].slavelist[cnt].Ebuscurrent;
               }
               printf(" CoE details: %2.2x FoE details: %2.2x EoE details: %2.2x SoE details: %2.2x\n",
                      ctx[ctx_count].slavelist[cnt].CoEdetails, ctx[ctx_count].slavelist[cnt].FoEdetails, ctx[ctx_count].slavelist[cnt].EoEdetails, ctx[ctx_count].slavelist[cnt].SoEdetails);
               printf(" Ebus current: %d[mA]\n only LRD/LWR:%d\n",
                      ctx[ctx_count].slavelist[cnt].Ebuscurrent, ctx[ctx_count].slavelist[cnt].blockLRW);
            }
         }

         inOP = FALSE;

         printf("Request operational state for all slaves\n");
         expectedWKC[0] = (ctx[0].grouplist[0].outputsWKC * 2) + ctx[0].grouplist[0].inputsWKC;
         expectedWKC[1] = (ctx[1].grouplist[0].outputsWKC * 2) + ctx[1].grouplist[0].inputsWKC;
         printf("Calculated workcounter master 1 %d\n", expectedWKC[0]);
         printf("Calculated workcounter master 2 %d\n", expectedWKC[1]);
         ctx[0].slavelist[0].state = EC_STATE_OPERATIONAL;
         ctx[1].slavelist[0].state = EC_STATE_OPERATIONAL;
         /* send one valid process data to make outputs in slaves happy*/
         ecx_send_processdata(&ctx[0]);
         ecx_send_processdata(&ctx[1]);
         ecx_receive_processdata(&ctx[0], EC_TIMEOUTRET);
         ecx_receive_processdata(&ctx[1], EC_TIMEOUTRET);
         /* request OP state for all slaves */
         ecx_writestate(&ctx[0], 0);
         ecx_writestate(&ctx[1], 0);
         chk = 40;
         /* wait for all slaves to reach OP state */
         do
         {
            ecx_send_processdata(&ctx[0]);
            ecx_send_processdata(&ctx[1]);
            ecx_receive_processdata(&ctx[0], EC_TIMEOUTRET);
            ecx_receive_processdata(&ctx[1], EC_TIMEOUTRET);
            ecx_statecheck(&ctx[0], 0, EC_STATE_OPERATIONAL, 50000);
            ecx_statecheck(&ctx[1], 0, EC_STATE_OPERATIONAL, 50000);
         } while (chk-- && (ctx[0].slavelist[0].state != EC_STATE_OPERATIONAL) && (ctx[1].slavelist[0].state != EC_STATE_OPERATIONAL));

         *(ctx[0].slavelist[6].outputs) = 0xFF;
         *(ctx[1].slavelist[1].outputs) = 0x0F;

         if (ctx[0].slavelist[0].state == EC_STATE_OPERATIONAL && ctx[1].slavelist[0].state == EC_STATE_OPERATIONAL)
         {
            printf("Operational state reached for all slaves.\n");
            /* cyclic loop */
            start_RT_trace(1);
            wkc_count = 0;
            inOP = TRUE;
            for (i = 1; i <= 10000; i++)
            {
               for (ctx_count = 0; ctx_count < 2; ctx_count++)
               {
                  /* Receive processdata */
                  log_RT_event('R', (WORD)(1 + ctx_count * 10));
                  wkc[ctx_count] = ecx_receive_processdata(&ctx[ctx_count], 0);
                  log_RT_event('R', (WORD)(99 + ctx_count * 100));
               }
               for (ctx_count = 0; ctx_count < 2; ctx_count++)
               {
                  /* Send processdata */
                  log_RT_event('S', (WORD)(1 + ctx_count * 10));
                  if (!ecx_send_processdata(&ctx[ctx_count]))
                  {
                     printf(" Frame no: %d on master %d,Not sentOK\n", i, ctx_count);
                  }
                  log_RT_event('S', (WORD)(99 + ctx_count * 100));
               }
               knRtSleep(1);

               for (ctx_count = 0; ctx_count < 2; ctx_count++)
               {
                  if (wkc[ctx_count] >= expectedWKC[ctx_count])
                  {
                  }
                  else
                  {
                     printf(" Frame no: %d on master %d , wc not wkc >= expectedWKC, %d\n", i, ctx_count, wkc[ctx_count]);
                  }
               }
            }
            stop_RT_trace();
            inOP = FALSE;
         }
      }
      else
      {
         printf("No slaves found!\n");
      }
      printf("End slaveinfo, close socket\n");
      /* stop SOEM, close socket */
      ecx_close(&ctx[0]);
      ecx_close(&ctx[1]);
   }
   else
   {
      printf("No socket connection on %s\nExcecute as root\n", ifname);
   }
}

void main(int argc, char *argv[])
{
   printf("SOEM (Simple Open EtherCAT Master)\nSlaveinfo\n");

   if (argc > 1)
   {
      /* start slaveinfo */
      slaveinfo(argv[1]);
   }
   else
   {
      printf("Usage: slaveinfo ifname [options]\nifname = eth0 for example\nOptions :\n -sdo : print SDO info\n -map : print mapping\n");
   }

   printf("End program\n");
}
