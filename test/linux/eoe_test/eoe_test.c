/** \file
 * \brief Example code for Simple Open EtherCAT master EoE
 * 
 * This example will run the follwing EoE functions
 * SetIP
 * GetIP
 *
 * Loop
 *    Send fragment data (Layer 2 0x88A4)
 *    Receive fragment data (Layer 2 0x88A4)
 * 
 * For this to work, a special slave test code is running that
 * will bounce the sent 0x88A4 back to receive.
 * 
 * Usage : eoe_test [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * This is a minimal test.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "ethercat.h"

#define EC_TIMEOUTMON 500

char IOmap[4096];

int expectedWKC;
boolean needlf;
volatile int globalwkc;
boolean inOP;
uint8 currentgroup = 0;
OSAL_THREAD_HANDLE thread1;
OSAL_THREAD_HANDLE thread2;
uint8 txbuf[1024];

/** Current RX fragment number */
uint8_t rxfragmentno = 0;
/** Complete RX frame size of current frame */
uint16_t rxframesize = 0;
/** Current RX data offset in frame */
uint16_t rxframeoffset = 0;
/** Current RX frame number */
uint16_t rxframeno = 0;
uint8 rxbuf[1024];
int size_of_rx = sizeof(rxbuf);

/** registered EoE hook */
int eoe_hook(ecx_contextt * context, uint16 slave, void * eoembx)
{
   int wkc;
   /* Pass received Mbx data to EoE recevive fragment function that
   * that will start/continue fill an Ethernet frame buffer
   */
   size_of_rx = sizeof(rxbuf);
   wkc = ecx_EOEreadfragment(eoembx,
      &rxfragmentno,
      &rxframesize,
      &rxframeoffset,
      &rxframeno,
      &size_of_rx,
      rxbuf);

   printf("Read frameno %d, fragmentno %d\n", rxframeno, rxfragmentno);

   /* wkc == 1 would mean a frame is complete , last fragment flag have been set and all
   * other checks must have past
   */
   if (wkc > 0)
   {
      ec_etherheadert *bp = (ec_etherheadert *)rxbuf;
      uint16 type = ntohs(bp->etype);
      printf("Frameno %d, type 0x%x complete\n", rxframeno, type);
      if (type == ETH_P_ECAT)
      {
         /* Sanity check that received buffer still is OK */
         if (sizeof(txbuf) != size_of_rx)
         {
            printf("Size differs, expected %d , received %d\n", sizeof(txbuf), size_of_rx);
         }
         else
         {
            printf("Size OK, expected %d , received %d\n", sizeof(txbuf), size_of_rx);
         }
         /* Check that the TX and RX frames are EQ */
         if (memcmp(rxbuf, txbuf, size_of_rx))
         {
            printf("memcmp result != 0\n");
         }
         else
         {
            printf("memcmp result == 0\n");
         }
         /* Send a new frame */
         int ixme;
         for (ixme = ETH_HEADERSIZE; ixme < sizeof(txbuf); ixme++)
         {
            txbuf[ixme] = (uint8)rand();
         }
         printf("Send a new frame\n");
         ecx_EOEsend(context, 1, 0, sizeof(txbuf), txbuf, EC_TIMEOUTRXM);
      }
      else
      {
         printf("Skip type 0x%x\n", type);
      }
   }
   /* No point in returning as unhandled */
   return 1;
}

OSAL_THREAD_FUNC mailbox_reader(void *lpParam)
{
   ecx_contextt *context = (ecx_contextt *)lpParam;
   int wkc;
   ec_mbxbuft MbxIn;
   ec_mbxheadert * MbxHdr = (ec_mbxheadert *)MbxIn;

   int ixme;
   ec_setupheader(&txbuf);
   for (ixme = ETH_HEADERSIZE; ixme < sizeof(txbuf); ixme++)
   {
      txbuf[ixme] = (uint8)rand();
   }
   /* Send a made up frame to trigger a fragmented transfer
   * Used with a special bound impelmentaion of SOES. Will
   * trigger a fragmented transfer back of the same frame.
   */
   ecx_EOEsend(context, 1, 0, sizeof(txbuf), txbuf, EC_TIMEOUTRXM);

   for (;;)
   {
      /* Read mailbox if no other mailbox conversation is ongoing  eg. SDOwrite/SDOwrite etc.*/
      wkc = ecx_mbxreceive(context, 1, (ec_mbxbuft *)&MbxIn, 0);
      if (wkc > 0)
      {
         printf("Unhandled mailbox response 0x%x\n", MbxHdr->mbxtype);
      }
      osal_usleep(1 * 1000 * 1000);
   }
}

void test_eoe(ecx_contextt * context)
{
   /* Set the HOOK */
   ecx_EOEdefinehook(context, eoe_hook);

   eoe_param_t ipsettings, re_ipsettings;
   memset(&ipsettings, 0, sizeof(ipsettings));
   memset(&re_ipsettings, 0, sizeof(re_ipsettings));

   ipsettings.ip_set = 1;
   ipsettings.subnet_set = 1;
   ipsettings.default_gateway_set = 1;

   EOE_IP4_ADDR_TO_U32(&ipsettings.ip, 192, 168, 9, 200);
   EOE_IP4_ADDR_TO_U32(&ipsettings.subnet, 255, 255, 255, 0);
   EOE_IP4_ADDR_TO_U32(&ipsettings.default_gateway, 0, 0, 0, 0);
   
   /* Send a set IP request */
   ecx_EOEsetIp(context, 1, 0, &ipsettings, EC_TIMEOUTRXM);

   /* Send a get IP request, should return the expected IP back */
   ecx_EOEgetIp(context, 1, 0, &re_ipsettings, EC_TIMEOUTRXM);

   /* Trigger an MBX read request, to be replaced by slave Mbx
   * full notification via polling of FMMU status process data
   */
   printf("recieved IP (%d.%d.%d.%d)\n",
      eoe_ip4_addr1(&re_ipsettings.ip),
      eoe_ip4_addr2(&re_ipsettings.ip),
      eoe_ip4_addr3(&re_ipsettings.ip),
      eoe_ip4_addr4(&re_ipsettings.ip));
   printf("recieved subnet (%d.%d.%d.%d)\n",
      eoe_ip4_addr1(&re_ipsettings.subnet),
      eoe_ip4_addr2(&re_ipsettings.subnet),
      eoe_ip4_addr3(&re_ipsettings.subnet),
      eoe_ip4_addr4(&re_ipsettings.subnet));
   printf("recieved gateway (%d.%d.%d.%d)\n",
      eoe_ip4_addr1(&re_ipsettings.default_gateway),
      eoe_ip4_addr2(&re_ipsettings.default_gateway),
      eoe_ip4_addr3(&re_ipsettings.default_gateway),
      eoe_ip4_addr4(&re_ipsettings.default_gateway));

   /* Create a asyncronous EoE reader */
   osal_thread_create(&thread2, 128000, &mailbox_reader, &ecx_context);
}

void teststarter(char *ifname)
{
   int i, oloop, iloop, chk;
   needlf = FALSE;
   inOP = FALSE;

   printf("Starting eoe test\n");

   /* initialise SOEM, bind socket to ifname */
   if (ec_init(ifname))
   {
      printf("ec_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */
      if ( ec_config_init(FALSE) > 0 )
      {
         printf("%d slaves found and configured.\n",ec_slavecount);

         ec_config_map(&IOmap);

         ec_configdc();

         printf("Slaves mapped, state to SAFE_OP.\n");
         /* wait for all slaves to reach SAFE_OP state */
         ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4);

         oloop = ec_slave[0].Obytes;
         if ((oloop == 0) && (ec_slave[0].Obits > 0)) oloop = 1;
         if (oloop > 8) oloop = 8;
         iloop = ec_slave[0].Ibytes;
         if ((iloop == 0) && (ec_slave[0].Ibits > 0)) iloop = 1;
         if (iloop > 8) iloop = 8;

         printf("segments : %d : %d %d %d %d\n",ec_group[0].nsegments ,ec_group[0].IOsegment[0],ec_group[0].IOsegment[1],ec_group[0].IOsegment[2],ec_group[0].IOsegment[3]);

         printf("Request operational state for all slaves\n");
         expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);
         ec_slave[0].state = EC_STATE_OPERATIONAL;
         /* send one valid process data to make outputs in slaves happy*/
         ec_send_processdata();
         ec_receive_processdata(EC_TIMEOUTRET);

         /* Simple EoE test */
         test_eoe(&ecx_context);
   
         /* request OP state for all slaves */
         ec_writestate(0);
         chk = 200;
         /* wait for all slaves to reach OP state */
         do
         {
            ec_send_processdata();
            ec_receive_processdata(EC_TIMEOUTRET);
            ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
         }
         while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
         if (ec_slave[0].state == EC_STATE_OPERATIONAL )
         {
            printf("Operational state reached for all slaves.\n");
            globalwkc = expectedWKC;
            inOP = TRUE;
            /* cyclic loop */
            for(i = 1; i <= 10000; i++)
            {
               ec_send_processdata();
               globalwkc = ec_receive_processdata(EC_TIMEOUTRET * 5);
#if PRINT_EOE_INFO_INSTEAD
               int j;
               if(globalwkc >= expectedWKC)
               {
                  printf("Processdata cycle %4d, WKC %d , O:", i, globalwkc);
                  for(j = 0 ; j < oloop; j++)
                  {
                     printf(" %2.2x", *(ec_slave[0].outputs + j));
                  }
                  printf(" I:");
                  for(j = 0 ; j < iloop; j++)
                  {
                     printf(" %2.2x", *(ec_slave[0].inputs + j));
                  }
                  printf(" T:%"PRId64"\r",ec_DCtime);
                  needlf = TRUE;
               }
#endif
               osal_usleep(1000);
            }
            inOP = FALSE;
         }
         else
         {
            printf("Not all slaves reached operational state.\n");
            ec_readstate();
            for(i = 1; i<=ec_slavecount ; i++)
            {
               if(ec_slave[i].state != EC_STATE_OPERATIONAL)
               {
                  printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                        i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
               }
            }
         }
         printf("\nRequest init state for all slaves\n");
         ec_slave[0].state = EC_STATE_INIT;
         /* request INIT state for all slaves */
         ec_writestate(0);
      }
      else
      {
         printf("No slaves found!\n");
      }
      printf("End eoe test, close socket\n");
      /* stop SOEM, close socket */
      ec_close();
   }
   else
   {
      printf("No socket connection on %s\nExcecute as root\n",ifname);
   }
}

OSAL_THREAD_FUNC ecatcheck( void *ptr )
{
   int slave;
   (void)ptr;                  /* Not used */

   while(1)
   {
      if( inOP && ((globalwkc < expectedWKC) || ec_group[currentgroup].docheckstate))
      {
         if (globalwkc < expectedWKC)
         {
            printf("(wkc < expectedWKC) , globalwkc %d\n", globalwkc);
         }
         if (ec_group[currentgroup].docheckstate)
         {
            printf("(ec_group[currentgroup].docheckstate)\n");
         }

         if (needlf)
         {
            needlf = FALSE;
            printf("\n");
         }
         /* one ore more slaves are not responding */
         ec_group[currentgroup].docheckstate = FALSE;
         ec_readstate();
         for (slave = 1; slave <= ec_slavecount; slave++)
         {
            if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
            {
               ec_group[currentgroup].docheckstate = TRUE;
               if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
               {
                  printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                  ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                  ec_writestate(slave);
               }
               else if(ec_slave[slave].state == EC_STATE_SAFE_OP)
               {
                  printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                  ec_slave[slave].state = EC_STATE_OPERATIONAL;
                  ec_writestate(slave);
               }
               else if(ec_slave[slave].state > EC_STATE_NONE)
               {
                  if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d reconfigured\n",slave);
                  }
               }
               else if(!ec_slave[slave].islost)
               {
                  /* re-check state */
                  ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                  if (ec_slave[slave].state == EC_STATE_NONE)
                  {
                     ec_slave[slave].islost = TRUE;
                     printf("ERROR : slave %d lost\n",slave);
                  }
               }
            }
            if (ec_slave[slave].islost)
            {
               if(ec_slave[slave].state == EC_STATE_NONE)
               {
                  if (ec_recover_slave(slave, EC_TIMEOUTMON))
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d recovered\n",slave);
                  }
               }
               else
               {
                  ec_slave[slave].islost = FALSE;
                  printf("MESSAGE : slave %d found\n",slave);
               }
            }
         }
         if(!ec_group[currentgroup].docheckstate)
            printf("OK : all slaves resumed OPERATIONAL.\n");
      }
      osal_usleep(10000);
   }
}

int main(int argc, char *argv[])
{
   printf("SOEM (Simple Open EtherCAT Master)\nEoE test\n");

   if (argc > 1)
   {
      /* create thread to handle slave error handling in OP */
//      pthread_create( &thread1, NULL, (void *) &ecatcheck, (void*) &ctime);
      osal_thread_create(&thread1, 128000, &ecatcheck, (void*) &ctime);

      /* start cyclic part */
      teststarter(argv[1]);
   }
   else
   {
      printf("Usage: eoe_test ifname1\nifname = eth0 for example\n");
   }

   printf("End program\n");
   return (0);
}
