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
 * $Id: osal_defs.h 472 2013-04-08 11:39:51Z rtlaka $
 *------------------------------------------------------------------------------
 */

#ifndef _osal_defs_
#define _osal_defs_

#ifndef PACKED
#define PACKED_BEGIN __pragma(pack(push, 1))
#define PACKED
#define PACKED_END __pragma(pack(pop))
#endif

#define OSAL_THREAD_HANDLE HANDLE
#define OSAL_THREAD_FUNC int WINAPI
#define OSAL_THREAD_FUNC_RT void

#endif
