/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * This is a minimal example running rt-kernel TTOS. Check
 * tutorial files in documentations for som steps to get your
 * system running.
 * (c)Andreas Karlsson 2012
 */

#include <kern.h>
#include "ethercat.h"
#include "string.h"
#include <oshw.h>
#include <config.h>

#include <defBF537.h>

#define pPORTFIO_SET  ((vuint16_t *)PORTFIO_SET)
#define pPORTFIO_CLEAR  ((vuint16_t *)PORTFIO_CLEAR)
#define pPORTFIO_DIR  ((vuint16_t *)PORTFIO_DIR)

#define EK1100_1           1
#define EL4001_1           2
#define EL3061_1           3
#define EL1008_1           4
#define EL1008_2           5
#define EL2622_1           6
#define EL2622_2           7
#define EL2622_3           8
#define EL2622_4           9
#define NUMBER_OF_SLAVES   9


typedef struct
{
   uint8    in1;
   uint8    in2;
   uint8    in3;
   uint8    in4;
   uint8    in5;
   uint8    in6;
   uint8    in7;
   uint8    in8;
} in_EL1008_t;

typedef struct
{
   uint8    out1;
   uint8    out2;
} out_EL2622_t;

typedef struct
{
   int16    out1;
} out_EL4001_t;

typedef struct
{
   int32    in1;
} in_EL3061_t;

out_EL4001_t   slave_EL4001_1;
in_EL3061_t    slave_EL3061_1;
in_EL1008_t    slave_EL1008_1;
in_EL1008_t    slave_EL1008_2;
out_EL2622_t   slave_EL2622_1;
out_EL2622_t   slave_EL2622_2;
out_EL2622_t   slave_EL2622_3;

uint32 network_configuration(void)
{
   /* Do we got expected number of slaves from config */
   if (ec_slavecount < NUMBER_OF_SLAVES)
      return 0;

   /* Verify slave by slave that it is correct*/
   if (strcmp(ec_slave[EK1100_1].name,"EK1100"))
      return 0;
   else if (strcmp(ec_slave[EL4001_1].name,"EL4001"))
      return 0;
   else if (strcmp(ec_slave[EL3061_1].name,"EL3061"))
      return 0;
   else if (strcmp(ec_slave[EL1008_1].name,"EL1008"))
      return 0;
   else if (strcmp(ec_slave[EL1008_2].name,"EL1008"))
      return 0;
   else if (strcmp(ec_slave[EL2622_1].name,"EL2622"))
      return 0;
   else if (strcmp(ec_slave[EL2622_2].name,"EL2622"))
      return 0;
   else if (strcmp(ec_slave[EL2622_3].name,"EL2622"))
      return 0;
   else if (strcmp(ec_slave[EL2622_4].name,"EL2622"))
      return 0;

  return 1;
}

int32 get_input_int32(uint16 slave_no,uint8 module_index)
{
   int32 return_value;
   uint8 *data_ptr;
   /* Get the IO map pointer from the ec_slave struct */
   data_ptr = ec_slave[slave_no].inputs;
   /* Move pointer to correct module index */
   data_ptr += module_index * 4;
   /* Read value byte by byte since all targets can't handle misaligned
    * addresses
    */
   return_value = *data_ptr++;
   return_value += (*data_ptr++ << 8);
   return_value += (*data_ptr++ << 16);
   return_value += (*data_ptr++ << 24);

   return return_value;
}

void set_input_int32 (uint16 slave_no, uint8 module_index, int32 value)
{
   uint8 *data_ptr;
   /* Get the IO map pointer from the ec_slave struct */
   data_ptr = ec_slave[slave_no].inputs;
   /* Move pointer to correct module index */
   data_ptr += module_index * 4;
   /* Read value byte by byte since all targets can handle misaligned
    * addresses
    */
   *data_ptr++ = (value >> 0) & 0xFF;
   *data_ptr++ = (value >> 8) & 0xFF;
   *data_ptr++ = (value >> 16) & 0xFF;
   *data_ptr++ = (value >> 24) & 0xFF;
}

uint8 get_input_bit (uint16 slave_no,uint8 module_index)
{
   /* Get the the startbit position in slaves IO byte */
   uint8 startbit = ec_slave[slave_no].Istartbit;
   /* Mask bit and return boolean 0 or 1 */
   if (*ec_slave[slave_no].inputs & BIT (module_index - 1  + startbit))
      return 1;
   else
      return 0;
}

int16 get_output_int16(uint16 slave_no,uint8 module_index)
{
   int16 return_value;
   uint8 *data_ptr;

   /* Get the IO map pointer from the ec_slave struct */
   data_ptr = ec_slave[slave_no].outputs;
   /* Move pointer to correct module index */
   data_ptr += module_index * 2;
   /* Read value byte by byte since all targets can handle misaligned
    * addresses
    */
   return_value = *data_ptr++;
   return_value += (*data_ptr++ << 8);

   return return_value;
}

void set_output_int16 (uint16 slave_no, uint8 module_index, int16 value)
{
   uint8 *data_ptr;
   /* Get the IO map pointer from the ec_slave struct */
   data_ptr = ec_slave[slave_no].outputs;
   /* Move pointer to correct module index */
   data_ptr += module_index * 2;
   /* Read value byte by byte since all targets can handle misaligned
    * addresses
    */
   *data_ptr++ = (value >> 0) & 0xFF;
   *data_ptr++ = (value >> 8) & 0xFF;
}

uint8 get_output_bit (uint16 slave_no,uint8 module_index)
{
   /* Get the the startbit position in slaves IO byte */
   uint8 startbit = ec_slave[slave_no].Ostartbit;
   /* Mask bit and return boolean 0 or 1 */
   if (*ec_slave[slave_no].outputs & BIT (module_index - 1  + startbit))
      return 1;
   else
      return 0;
}

void set_output_bit (uint16 slave_no, uint8 module_index, uint8 value)
{
   /* Get the the startbit position in slaves IO byte */
   uint8 startbit = ec_slave[slave_no].Ostartbit;
   /* Set or Clear bit */
   if (value == 0)
      *ec_slave[slave_no].outputs &= ~(1 << (module_index - 1 + startbit));
   else
      *ec_slave[slave_no].outputs |= (1 << (module_index - 1 + startbit));
}


extern tt_sched_t * tt_sched[];

char IOmap[128];
int dorun = 0;

uint8_t load1s, load5s, load10s;
uint32_t error_counter = 0;

void tt_error (uint32_t task_ix);
void tt_error (uint32_t task_ix)
{
   error_counter++;
}

static void my_cyclic_callback (void * arg)
{
   while (1)
   {
      stats_get_load (&load1s, &load5s, &load10s);
      rprintp ("Processor load was %d:%d:%d (1s:5s:10s) with TT errors: %d\n",
               load1s, load5s, load10s,error_counter);
      task_delay(20000);
   }
}


void read_io (void * arg)
{
   /* Function connected to cyclic TTOS task
    * The function is executed cyclic according
    * sceduel specified in schedule.tt
    */
   *pPORTFIO_SET = BIT (6);
   /* Send and receive processdata
    * Note that you need som synchronization
    * case you modifey IO in local application
    */
   ec_send_processdata();
   ec_receive_processdata(EC_TIMEOUTRET);
   *pPORTFIO_CLEAR = BIT (6);
}


void simpletest(void *arg)
{

   char *ifname = arg;
   int cnt, i, j;

   *pPORTFIO_DIR |= BIT (6);

   rprintp("Starting simple test\n");

   /* initialise SOEM */
   if (ec_init(ifname))
   {
      rprintp("ec_init succeeded.\n");

      /* find and auto-config slaves */
      if ( ec_config_init(FALSE) > 0 )
      {
         rprintp("%d slaves found and configured.\n",ec_slavecount);

         /* Check network  setup */
         if (network_configuration())
         {
            /* Run IO mapping */
            ec_config_map(&IOmap);

            rprintp("Slaves mapped, state to SAFE_OP.\n");
            /* wait for all slaves to reach SAFE_OP state */
            ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE);

            /* Print som information on the mapped network */
            for( cnt = 1 ; cnt <= ec_slavecount ; cnt++)
            {
               rprintp("\nSlave:%d\n Name:%s\n Output size: %dbits\n Input size: %dbits\n State: %d\n Delay: %d[ns]\n Has DC: %d\n",
                       cnt, ec_slave[cnt].name, ec_slave[cnt].Obits, ec_slave[cnt].Ibits,
                       ec_slave[cnt].state, ec_slave[cnt].pdelay, ec_slave[cnt].hasdc);
               rprintp(" Configured address: %x\n", ec_slave[cnt].configadr);
               rprintp(" Outputs address: %x\n", ec_slave[cnt].outputs);
               rprintp(" Inputs address: %x\n", ec_slave[cnt].inputs);

               for(j = 0 ; j < ec_slave[cnt].FMMUunused ; j++)
               {
                  rprintp(" FMMU%1d Ls:%x Ll:%4d Lsb:%d Leb:%d Ps:%x Psb:%d Ty:%x Act:%x\n", j,
                          (int)ec_slave[cnt].FMMU[j].LogStart, ec_slave[cnt].FMMU[j].LogLength, ec_slave[cnt].FMMU[j].LogStartbit,
                          ec_slave[cnt].FMMU[j].LogEndbit, ec_slave[cnt].FMMU[j].PhysStart, ec_slave[cnt].FMMU[j].PhysStartBit,
                          ec_slave[cnt].FMMU[j].FMMUtype, ec_slave[cnt].FMMU[j].FMMUactive);
               }
               rprintp(" FMMUfunc 0:%d 1:%d 2:%d 3:%d\n",
                        ec_slave[cnt].FMMU0func, ec_slave[cnt].FMMU2func, ec_slave[cnt].FMMU2func, ec_slave[cnt].FMMU3func);

            }

            rprintp("Request operational state for all slaves\n");
            ec_slave[0].state = EC_STATE_OPERATIONAL;
            /* send one valid process data to make outputs in slaves happy*/
            ec_send_processdata();
            ec_receive_processdata(EC_TIMEOUTRET);
            /* request OP state for all slaves */
            ec_writestate(0);
            /* wait for all slaves to reach OP state */
            ec_statecheck(0, EC_STATE_OPERATIONAL,  EC_TIMEOUTSTATE);
            if (ec_slave[0].state == EC_STATE_OPERATIONAL )
            {
               rprintp("Operational state reached for all slaves.\n");
            }
            else
            {
               rprintp("Not all slaves reached operational state.\n");
               ec_readstate();
               for(i = 1; i<=ec_slavecount ; i++)
               {
                  if(ec_slave[i].state != EC_STATE_OPERATIONAL)
                  {
                     rprintp("Slave %d State=0x%04x StatusCode=0x%04x\n",
                             i, ec_slave[i].state, ec_slave[i].ALstatuscode);
                  }
               }
            }


            /* Simple blinking lamps BOX demo */
            uint8 digout = 0;

            slave_EL4001_1.out1 = (int16)0x3FFF;
            set_output_int16(EL4001_1,0,slave_EL4001_1.out1);

            task_spawn ("t_StatsPrint", my_cyclic_callback, 20, 1024, (void *)NULL);
            tt_start_wait (tt_sched[0]);

            while(1)
            {
               dorun = 0;
               slave_EL1008_1.in1 = get_input_bit(EL1008_1,1); // Start button
               slave_EL1008_1.in2 = get_input_bit(EL1008_1,2);  // Turnkey RIGHT
               slave_EL1008_1.in3 = get_input_bit(EL1008_1,3); // Turnkey LEFT

               /* (Turnkey MIDDLE + Start button) OR Turnkey RIGHT OR Turnkey LEFT
                  Turnkey LEFT: Light positions bottom to top. Loop, slow operation.
                  Turnkey MIDDLE: Press start button to light positions bottom to top. No loop, fast operation.
                  Turnkey RIGHT: Light positions bottom to top. Loop, fast operation.
               */
               if (slave_EL1008_1.in1 || slave_EL1008_1.in2 || slave_EL1008_1.in3)
               {
                  digout = 0;
                  /* *ec_slave[6].outputs = digout; */
                  /* set_output_bit(slave_name #,index as 1 output on module , value */
                  set_output_bit(EL2622_1,1,(digout & BIT (0))); /* Start button */
                  set_output_bit(EL2622_1,2,(digout & BIT (1))); /* Turnkey RIGHT */
                  set_output_bit(EL2622_2,1,(digout & BIT (2))); /* Turnkey LEFT */
                  set_output_bit(EL2622_2,2,(digout & BIT (3)));
                  set_output_bit(EL2622_3,1,(digout & BIT (4)));
                  set_output_bit(EL2622_3,2,(digout & BIT (5)));

                  while(dorun < 95)
                  {
                     dorun++;

                     if (slave_EL1008_1.in3)
                        task_delay(tick_from_ms(20));
                     else
                        task_delay(tick_from_ms(5));

                     digout = (uint8) (digout | BIT((dorun / 16) & 0xFF));

                     set_output_bit(EL2622_1,1,(digout & BIT (0))); /* LED1 */
                     set_output_bit(EL2622_1,2,(digout & BIT (1))); /* LED2 */
                     set_output_bit(EL2622_2,1,(digout & BIT (2))); /* LED3 */
                     set_output_bit(EL2622_2,2,(digout & BIT (3))); /* LED4 */
                     set_output_bit(EL2622_3,1,(digout & BIT (4))); /* LED5 */
                     set_output_bit(EL2622_3,2,(digout & BIT (5))); /* LED6 */

                     slave_EL1008_1.in1 = get_input_bit(EL1008_1,2);  /* Turnkey RIGHT */
                     slave_EL1008_1.in2 = get_input_bit(EL1008_1,3); /* Turnkey LEFT */
                     slave_EL3061_1.in1 = get_input_int32(EL3061_1,0); /* Read AI */
                  }
               }
               task_delay(tick_from_ms(2));

            }
         }
         else
         {
            rprintp("Mismatch of network units!\n");
         }
      }
      else
      {
         rprintp("No slaves found!\n");
      }
      rprintp("End simple test, close socket\n");
      /* stop SOEM, close socket */
      ec_close();
   }
   else
   {
      rprintp("ec_init failed");
   }
   rprintp("End program\n");
}

static void test_osal_timer_timeout_us (const uint32 timeout_us)
{
   osal_timert timer;

   ASSERT (timeout_us > 4000);

   osal_timer_start (&timer, timeout_us);
   ASSERT (osal_timer_is_expired (&timer) == false);
   osal_usleep (timeout_us - 2000);
   ASSERT (osal_timer_is_expired (&timer) == false);
   osal_usleep (4000);
   ASSERT (osal_timer_is_expired (&timer));
}

static void test_osal_timer (void)
{
   test_osal_timer_timeout_us (10*1000);     /* 10ms */
   test_osal_timer_timeout_us (100*1000);    /* 100ms */
   test_osal_timer_timeout_us (1000*1000);   /* 1s */
   test_osal_timer_timeout_us (2000*1000);   /* 2s */
}

#define USECS_PER_SEC   1000000
#define USECS_PER_TICK  (USECS_PER_SEC / CFG_TICKS_PER_SECOND)
#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

static int32 time_difference_us (const ec_timet stop, const ec_timet start)
{
   int32 difference_us;

   ASSERT (stop.sec >= start.sec);
   if (stop.sec == start.sec)
   {
      ASSERT (stop.usec >= start.usec);
   }

   difference_us = (stop.sec - start.sec) * USECS_PER_SEC;
   difference_us += ((int32)stop.usec - (int32)start.usec);

   ASSERT (difference_us >= 0);
   return difference_us;
}

/**
 * Test osal_current_time() by using it for measuring how long an osal_usleep()
 * takes, in specified number of microseconds.
 */
static void test_osal_current_time_for_delay_us (const int32 sleep_time_us)
{
   ec_timet start;
   ec_timet stop;
   int32 measurement_us;
   int32 deviation_us;
   const int32 usleep_accuracy_us = USECS_PER_TICK;
   boolean is_deviation_within_tolerance;

   start = osal_current_time ();
   osal_usleep (sleep_time_us);
   stop = osal_current_time ();

   measurement_us = time_difference_us (stop, start);
   deviation_us = ABS (measurement_us - sleep_time_us);
   is_deviation_within_tolerance = deviation_us <= usleep_accuracy_us;
   ASSERT (is_deviation_within_tolerance);
}

static void test_osal_current_time (void)
{
   test_osal_current_time_for_delay_us (0);
   test_osal_current_time_for_delay_us (1);
   test_osal_current_time_for_delay_us (500);
   test_osal_current_time_for_delay_us (999);
   test_osal_current_time_for_delay_us (USECS_PER_TICK);
   test_osal_current_time_for_delay_us (USECS_PER_TICK-1);
   test_osal_current_time_for_delay_us (USECS_PER_TICK+1);
   test_osal_current_time_for_delay_us (2 * 1000 * 1000);  /* 2s */
}

#include <lwip/inet.h>

static void test_oshw_htons (void)
{
	   uint16 network;
	   uint16 host;

	   host = 0x1234;
	   network = oshw_htons (host);
	   ASSERT (network == htons (host));
}

static void test_oshw_ntohs (void)
{
	   uint16 host;
	   uint16 network;

	   network = 0x1234;
	   host = oshw_ntohs (network);
	   ASSERT (host == ntohs (network));
}

int main (void)
{
   test_oshw_htons ();
   test_oshw_ntohs ();
   test_osal_timer ();
   test_osal_current_time ();

   rprintp("SOEM (Simple Open EtherCAT Master)\nSimple test\n");

   task_spawn ("simpletest", simpletest, 9, 8192, NULL);

   return (0);
}

