/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "oshw.h"
#include "intel_i210.h"
#include "ethercat.h"

#if !defined(__gnu_linux__)
#include <machine/endian.h>
#else
#include <endian.h>
#define __htons(x) htobe16(x)
#define __ntohs(x) be16toh(x)
#endif

ec_adaptert	adapters [DEVS_MAX_NB];

/**
 * Host to Network byte order (i.e. to big endian).
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
inline uint16 oshw_htons(uint16 host)
{
	// __htons() is provided by the bare-metal x86 compiler
	return __htons(host);
}

/**
 * Network (i.e. big endian) to Host byte order.
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
inline uint16 oshw_ntohs(uint16 network)
{
	// __ntohs() is provided by the bare-metal x86 compiler
	return __ntohs(network);
}

/** Create list over available network adapters.
 * @return First element in linked list of adapters
 */
ec_adaptert* oshw_find_adapters(void)
{
	ec_adaptert *ret = NULL;
	if (eth_discover_devices() >= 0) {
		for (int i = 0;; ++i) {
			struct eth_device *dev = eth_get_device(i);
			if (dev == NULL) {
				adapters[i-1].next = NULL;
				break;
			}
			strncpy(adapters[i].name, dev->name, MAX_DEVICE_NAME);
			adapters[i].next = &adapters[i+1];
		}
		ret = &(adapters[0]);
	}
	return ret;
}

/** Free memory allocated memory used by adapter collection.
 * @param[in] adapter = First element in linked list of adapters
 * EC_NOFRAME.
 */
void oshw_free_adapters(ec_adaptert *adapter)
{
}

extern int ec_slavecount;

void print_slave_info (void)
{
	OSEE_PRINT("Printing slave info for %d slaves...\n", ec_slavecount);
   	for (int i = 1; i < ec_slavecount + 1; ++i) {
	   	OSEE_PRINT("Name: %s\n", ec_slave[i].name);
	   	OSEE_PRINT("\tSlave nb: %d\n", i);
		if ((ec_slave[i].state & 0x0f) == EC_STATE_NONE)
	   		OSEE_PRINT("\tState: NONE\n");
		else if ((ec_slave[i].state & 0x0f) == EC_STATE_INIT)
	   		OSEE_PRINT("\tState: INIT\n");
		else if ((ec_slave[i].state & 0x0f) == EC_STATE_PRE_OP)
	   		OSEE_PRINT("\tState: PRE_OP\n");
		else if ((ec_slave[i].state & 0x0f) == EC_STATE_BOOT)
	   		OSEE_PRINT("\tState: BOOT\n");
		else if ((ec_slave[i].state & 0x0f) == EC_STATE_SAFE_OP)
	   		OSEE_PRINT("\tState: SAFE_OP\n");
		else if ((ec_slave[i].state & 0x0f) == EC_STATE_OPERATIONAL)
	   		OSEE_PRINT("\tState: OPERATIONAL\n");
		if (ec_slave[i].state & 0x10)
	   		OSEE_PRINT("\tState: ACK or ERROR\n");
	   	OSEE_PRINT("\tOutput bytes: %u\n", ec_slave[i].Obytes);
	   	OSEE_PRINT("\tOutput bits: %u\n", ec_slave[i].Obits);
	   	OSEE_PRINT("\tInput bytes: %u\n", ec_slave[i].Ibytes);
	   	OSEE_PRINT("\tInput bits: %u\n", ec_slave[i].Ibits);
	   	OSEE_PRINT("\tConfigured address: %u\n", ec_slave[i].configadr);
	   	OSEE_PRINT("\tOutput address: %p\n", ec_slave[i].outputs);
	   	OSEE_PRINT("\tOstartbit: %x\n", ec_slave[i].Ostartbit);
	   	OSEE_PRINT("\tCoE details: %x\n", ec_slave[i].CoEdetails); // See ECT_COEDET_*
	   	OSEE_PRINT("\tHas DC capability: %d\n\n", ec_slave[i].hasdc);
	}
}

void set_operational (void)
{
    	int chk;
	/* wait for all slaves to reach SAFE_OP state */
        ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4);

        ec_slave[0].state = EC_STATE_OPERATIONAL;
        /* send one valid process data to make outputs in slaves happy*/
        ec_send_processdata();
        ec_receive_processdata(EC_TIMEOUTRET);
        /* request OP state for all slaves */
        ec_writestate(0);
        chk = 40;
        do {
              	ec_send_processdata();
              	ec_receive_processdata(EC_TIMEOUTRET);
              	ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
        } while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));

        if (ec_slave[0].state == EC_STATE_OPERATIONAL ) {
		for (int i = 1; i < ec_slavecount +1; ++i)
			ec_slave[i].state = EC_STATE_OPERATIONAL;
		for (int i = 1; i < ec_slavecount +1; ++i)
			ec_writestate(i);
		ec_send_processdata();
		ec_receive_processdata(EC_TIMEOUTRET);
	} else {
		OSEE_PRINT("WARNING: cannot reach OPERATIONAL state\n");
	}
}

void set_output_int16 (uint16_t slave_nb, uint8_t module_index, int16_t value)
{
   	uint8_t *data_ptr;

   	data_ptr = ec_slave[slave_nb].outputs;
   	/* Move pointer to correct module index*/
   	data_ptr += module_index * 2;
   	/* Read value byte by byte since all targets can't handle misaligned
      	   addresses */
   	*data_ptr++ = (value >> 0) & 0xFF;
   	*data_ptr++ = (value >> 8) & 0xFF;
}

