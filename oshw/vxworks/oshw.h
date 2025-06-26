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

#include "soem/soem.h"
#include "nicdrv.h"

uint16 oshw_htons(uint16 hostshort);
uint16 oshw_ntohs(uint16 networkshort);
ec_adaptert *oshw_find_adapters(void);
void oshw_free_adapters(ec_adaptert *adapter);

#endif
