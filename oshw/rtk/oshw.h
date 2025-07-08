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
extern "C"
{
#endif

#include <kern.h>
#include "ethercattype.h"
#include "nicdrv.h"
#include "ethercatmain.h"


uint16 oshw_htons(uint16 host);
uint16 oshw_ntohs(uint16 network);

ec_adaptert * oshw_find_adapters(void);
void oshw_free_adapters(ec_adaptert * adapter);

#ifdef __cplusplus
}
#endif

#endif
