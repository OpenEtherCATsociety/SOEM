/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : status_test [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * This is a test to read out status information from a slave.
 *
 * (c)Daniel Udd 2014
 */

#include <stdio.h>
#include <string.h>
//#include <Mmsystem.h>

#include "osal.h"
#include "ethercattype.h"
#include "nicdrv.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercatdc.h"
#include "ethercatcoe.h"
#include "ethercatfoe.h"
#include "ethercatconfig.h"
#include "ethercatprint.h"
#include "ecat.h"

#define EC_TIMEOUTMON 500

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
boolean inOP;
uint8 currentgroup = 0;

static void print_dc_status(ecat_slave_dc_status_t * status)
{
    printf("Slave dc status:\n");
    printf(" - DC System time:\n");
    printf("    System time: %llu\n", status->time);
    printf("    System time offset: %llu\n", status->offset);
    printf("    System time delay: %u\n", status->delay);
    printf("    System time difference: %d\n", status->difference);
}
static void print_status(ecat_slave_status_t * status)
{
    printf("Slave status:\n");
    printf(" - port 0:\n");
    printf("    Link detected: %u\n", status->port[0].link_detected);
    printf("    Loop port: %u\n", status->port[0].loop_port);
    printf("    Stable communication: %u\n", status->port[0].stable_com);
    printf("    Lost link counter: %u\n", status->port[0].lost_link_counter);
    printf("    Invalid frame counter: %u\n", status->port[0].invalid_frame_counter);
    printf("    RX error counter: %u\n", status->port[0].rx_error_counter);
    printf("    Forwarded RX error counter: %u\n", status->port[0].forwarded_rx_error_counter);
    printf(" - port 1:\n");
    printf("    Link detected: %u\n", status->port[1].link_detected);
    printf("    Loop port: %u\n", status->port[1].loop_port);
    printf("    Stable communication: %u\n", status->port[1].stable_com);
    printf("    Lost link counter: %u\n", status->port[1].lost_link_counter);
    printf("    Invalid frame counter: %u\n", status->port[1].invalid_frame_counter);
    printf("    RX error counter: %u\n", status->port[1].rx_error_counter);
    printf("    Forwarded RX error counter: %u\n", status->port[1].forwarded_rx_error_counter);
    printf(" - port 2:\n");
    printf("    Link detected: %u\n", status->port[2].link_detected);
    printf("    Loop port: %u\n", status->port[2].loop_port);
    printf("    Stable communication: %u\n", status->port[2].stable_com);
    printf("    Lost link counter: %u\n", status->port[2].lost_link_counter);
    printf("    Invalid frame counter: %u\n", status->port[2].invalid_frame_counter);
    printf("    RX error counter: %u\n", status->port[2].rx_error_counter);
    printf("    Forwarded RX error counter: %u\n", status->port[2].forwarded_rx_error_counter);
    printf(" - port 3:\n");
    printf("    Link detected: %u\n", status->port[3].link_detected);
    printf("    Loop port: %u\n", status->port[3].loop_port);
    printf("    Stable communication: %u\n", status->port[3].stable_com);
    printf("    Lost link counter: %u\n", status->port[3].lost_link_counter);
    printf("    Invalid frame counter: %u\n", status->port[3].invalid_frame_counter);
    printf("    RX error counter: %u\n", status->port[3].rx_error_counter);
    printf("    Forwarded RX error counter: %u\n", status->port[3].forwarded_rx_error_counter);
    printf(" - Watchdog Counters:\n");
    printf("    Watchdog counter process data: %u\n", status->wd_cnt.watchdog_counter_process_data);
    printf("    Watchdog counter pdi: %u\n", status->wd_cnt.watchdog_counter_pdi);
    printf(" - ECAT Processing Unit Error Counter: %u\n", status->ecat_processing_unit_error_counter);
    printf(" - PDI Error Counter: %u\n", status->pdi_error_counter);
    printf(" - PDI Error Code: %u\n", status->pdi_error_code);
}

void slave_status_test(char * ibuf)
{
    ecat_slave_status_t slave_status;
    ecat_slave_dc_status_t slave_dc_status;
    int fault;
    ecat_net_t * net;

    printf("Init...\n");
    memset(&slave_status, 0, sizeof(ecat_slave_status_t));
    memset(&slave_dc_status, 0, sizeof(ecat_slave_dc_status_t));
    net = ecat_net_init (ibuf);
    if (!ecat_net_scan (net)) {
        fault = ecat_slave_get_status(net, 1, &slave_status);
        printf("Get status result: %d\n", fault);
        fault = ecat_slave_get_dc_status(net, 1, &slave_dc_status);
        printf("Get dc status result: %d\n", fault);
        print_status(&slave_status);
        print_dc_status(&slave_dc_status);
    }
}

OSAL_THREAD_FUNC ecatcheck(void *lpParam) 
{
    int slave;

    while(1)
    {
        if( inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
        {
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
                  else if(ec_slave[slave].state > 0)
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
                     if (!ec_slave[slave].state)
                     {
                        ec_slave[slave].islost = TRUE;
                        printf("ERROR : slave %d lost\n",slave);                           
                     }
                  }
               }
               if (ec_slave[slave].islost)
               {
                  if(!ec_slave[slave].state)
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

    return 0;
}  

char ifbuf[1024];

int main(int argc, char *argv[])
{
   ec_adaptert * adapter = NULL;   
   printf("SOEM (Simple Open EtherCAT Master)\nStatus test\n");

   if (argc > 1)
   {      
      /* create thread to handle slave error handling in OP */
      osal_thread_create(&thread1, 128000, &ecatcheck, (void*) &ctime);
      strcpy(ifbuf, argv[1]);
      /* start test */
      slave_status_test(ifbuf);
   }
   else
   {
      printf("Usage: status_test ifname1\n");
   	/* Print the list */
      printf ("Available adapters\n");
      adapter = ec_find_adapters ();
      while (adapter != NULL)
      {
         printf ("Description : %s, Device to use for wpcap: %s\n", adapter->desc,adapter->name);
         adapter = adapter->next;
      }
   }   
   
   printf("End program\n");
   return (0);
}
