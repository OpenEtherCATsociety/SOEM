/******************************************************************************
 *                *          ***                    ***
 *              ***          ***                    ***
 * ***  ****  **********     ***        *****       ***  ****          *****
 * *********  **********     ***      *********     ************     *********
 * ****         ***          ***              ***   ***       ****   ***
 * ***          ***  ******  ***      ***********   ***        ****   *****
 * ***          ***  ******  ***    *************   ***        ****      *****
 * ***          ****         ****   ***       ***   ***       ****          ***
 * ***           *******      ***** **************  *************    *********
 * ***             *****        ***   *******   **  **  ******         *****
 *                           t h e  r e a l t i m e  t a r g e t  e x p e r t s
 *
 * http://www.rt-labs.com
 * Copyright (C) 2009. rt-labs AB, Sweden. All rights reserved.
 *------------------------------------------------------------------------------
 * $Id: oshw.h 452 2013-02-26 21:02:58Z smf.arthur $
 *------------------------------------------------------------------------------
 */

/** \file 
 * \brief
 * Headerfile for ethercatbase.c 
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
