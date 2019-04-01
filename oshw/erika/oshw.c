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

