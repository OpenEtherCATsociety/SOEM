/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include "oshw.h"

/**
 * Host to Network byte order (i.e. to big endian).
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
uint16 oshw_htons (uint16 host)
{
   uint16 network = htons (host);
   return network;
}

/**
 * Network (i.e. big endian) to Host byte order.
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
uint16 oshw_ntohs (uint16 network)
{
   uint16 host = ntohs (network);
   return host;
}

/* Create list over available network adapters.
 * @return First element in linked list of adapters
 */
ec_adaptert * oshw_find_adapters (void)
{
   int i = 0;
   int ret = 0;
   pcap_if_t *alldevs;
   pcap_if_t *d;
   ec_adaptert * adapter;
   ec_adaptert * prev_adapter;
   ec_adaptert * ret_adapter = NULL;
   char errbuf[PCAP_ERRBUF_SIZE];
   size_t string_len;

   /* find all devices */
   if (pcap_findalldevs(&alldevs, errbuf) == -1)
   {
      fprintf(stderr,"Error in pcap_findalldevs_ex: %s\n", errbuf);
      return (NULL);
   }
   /* Iterate all devices and create a local copy holding the name and
    * decsription.
    */
   for(d= alldevs; d != NULL; d= d->next)
   {
      adapter = (ec_adaptert *)malloc(sizeof(ec_adaptert));
      /* If we got more than one adapter save link list pointer to previous
       * adapter.
       * Else save as pointer to return.
       */
      if (i)
      {
         prev_adapter->next = adapter;
      }
      else
      {
         ret_adapter = adapter;
      }

      /* fetch description and name of the divice from libpcap */
      adapter->next = NULL;
      if (d->name)
      {
         string_len = strlen(d->name);
         if (string_len > (EC_MAXLEN_ADAPTERNAME - 1))
         {
            string_len = EC_MAXLEN_ADAPTERNAME - 1;
         }
         strncpy(adapter->name, d->name,string_len);
         adapter->name[string_len] = '\0';
      }
      else
      {
         adapter->name[0] = '\0';
      }
      if (d->description)
      {
         string_len = strlen(d->description);
         if (string_len > (EC_MAXLEN_ADAPTERNAME - 1))
         {
            string_len = EC_MAXLEN_ADAPTERNAME - 1;
         }
         strncpy(adapter->desc, d->description,string_len);
         adapter->desc[string_len] = '\0';
      }
      else
      {
          adapter->desc[0] = '\0';
      }
      prev_adapter = adapter;
      i++;
   }
   /* free all devices allocated */
   pcap_freealldevs(alldevs);

   return ret_adapter;
}

/** Free memory allocated memory used by adapter collection.
 * @param[in] adapter = First element in linked list of adapters
 * EC_NOFRAME.
 */
void oshw_free_adapters (ec_adaptert * adapter)
{
   ec_adaptert * next_adapter;
   /* Iterate the linked list and free all elemnts holding
    * adapter information
    */
   if(adapter)
   {
      next_adapter = adapter->next;
      free (adapter);
      while (next_adapter)
      {
         adapter = next_adapter;
         next_adapter = adapter->next;
         free (adapter);
      }
   }
}
