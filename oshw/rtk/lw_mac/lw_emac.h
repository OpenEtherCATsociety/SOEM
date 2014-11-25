/*
 * author: Tomas Vestelind
 */

#ifndef LWIP_MAC_H
#define LWIP_MAC_H

int bfin_EMAC_init(uint8_t *enetaddr);
int bfin_EMAC_send(void *packet, int length);
int bfin_EMAC_recv(uint8_t * packet, size_t size);

#endif /* LWIP_MAC_H */
