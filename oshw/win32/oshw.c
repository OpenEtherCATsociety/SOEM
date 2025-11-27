/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#include "oshw.h"
#include <stdlib.h>

/**
 * Host to Network byte order (i.e. to big endian).
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
uint16 oshw_htons(uint16 host)
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
uint16 oshw_ntohs(uint16 network)
{
   uint16 host = ntohs(network);
   return host;
}

/* Create list over available network adapters.
 * @return First element in linked list of adapters
 */
ec_adaptert *oshw_find_adapters(void)
{
   pcap_if_t *alldevs;
   pcap_if_t *d;
   ec_adaptert *adapter;
   ec_adaptert *prev_adapter = NULL;
   ec_adaptert *ret_adapter = NULL;
   char errbuf[PCAP_ERRBUF_SIZE];

   /* find all devices */
   if (pcap_findalldevs(&alldevs, errbuf) == -1)
   {
      fprintf(stderr, "Error in pcap_findalldevs_ex: %s\n", errbuf);
      return (NULL);
   }
   /* Iterate all devices and create a local copy holding the name and
    * description.
    */
   for (d = alldevs; d != NULL; d = d->next)
   {
      adapter = (ec_adaptert *)malloc(sizeof(ec_adaptert));
      /* If we got more than one adapter save link list pointer to previous
       * adapter.
       * Else save as pointer to return.
       */
      if (prev_adapter)
      {
         prev_adapter->next = adapter;
      }
      else
      {
         ret_adapter = adapter;
      }

      /* fetch description and name of the device from libpcap */
      adapter->next = NULL;
      if (d->name)
      {
         strncpy(adapter->name, d->name, EC_MAXLEN_ADAPTERNAME);
         adapter->name[EC_MAXLEN_ADAPTERNAME - 1] = '\0';
      }
      else
      {
         adapter->name[0] = '\0';
      }
      if (d->description)
      {
         strncpy(adapter->desc, d->description, EC_MAXLEN_ADAPTERNAME);
         adapter->desc[EC_MAXLEN_ADAPTERNAME - 1] = '\0';
      }
      else
      {
         adapter->desc[0] = '\0';
      }
      prev_adapter = adapter;
   }
   /* free all devices allocated */
   pcap_freealldevs(alldevs);

   return ret_adapter;
}

/** Free memory allocated memory used by adapter collection.
 * @param[in] adapter = First element in linked list of adapters
 * EC_NOFRAME.
 */
void oshw_free_adapters(ec_adaptert *adapter)
{
   ec_adaptert *next_adapter;
   /* Iterate the linked list and free all elements holding
    * adapter information
    */
   if (adapter)
   {
      next_adapter = adapter->next;
      free(adapter);
      while (next_adapter)
      {
         adapter = next_adapter;
         next_adapter = adapter->next;
         free(adapter);
      }
   }
}
