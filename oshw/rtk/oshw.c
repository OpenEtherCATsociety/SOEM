/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#include "oshw.h"
#include <stdlib.h>
#include <lwip/inet.h>

/**
 * Host to Network byte order (i.e. to big endian).
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
uint16 oshw_htons(const uint16 host)
{
   uint16 network = htons(host);
   return network;
}

/**
 * Network (i.e. big endian) to Host byte order.
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
uint16 oshw_ntohs(const uint16 network)
{
   uint16 host = ntohs(network);
   return host;
}

/* Create list over available network adapters.
 * @return First element in linked list of adapters
 */
ec_adaptert *oshw_find_adapters(void)
{
   ec_adaptert *ret_adapter = NULL;

   /* TODO if needed */

   return ret_adapter;
}

/** Free memory allocated memory used by adapter collection.
 * @param[in] adapter = First element in linked list of adapters
 * EC_NOFRAME.
 */
void oshw_free_adapters(ec_adaptert *adapter)
{
   /* TODO if needed */
}
