/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file 
 * \brief
 * Headerfile for oshw.c 
 */

#ifndef _oshw_
#define _oshw_

#include "ethercattype.h"
#include "nicdrv.h"
#include "ethercatmain.h"

uint16 oshw_htons(uint16 hostshort);
uint16 oshw_ntohs(uint16 networkshort);
ec_adaptert * oshw_find_adapters(void);
void oshw_free_adapters(ec_adaptert * adapter);

#endif
