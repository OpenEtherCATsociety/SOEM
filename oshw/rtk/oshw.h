/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for oshw.c
 */

#ifndef _oshw_
#define _oshw_

#ifdef __cplusplus
extern "C" {
#endif

#include <kern/kern.h>
#include "soem/soem.h"
#include "nicdrv.h"

int oshw_mac_init(const uint8_t *mac_address);
int oshw_mac_send(const void *payload, size_t tot_len);
int oshw_mac_recv(void *buffer, size_t buffer_length);

uint16 oshw_htons(uint16 host);
uint16 oshw_ntohs(uint16 network);

ec_adaptert *oshw_find_adapters(void);
void oshw_free_adapters(ec_adaptert *adapter);

#ifdef __cplusplus
}
#endif

#endif
