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
* Copyright (C) 2007. rt-labs AB, Sweden. All rights reserved.
*------------------------------------------------------------------------------
* $Id: fec_ecat.h 91 2014-04-02 13:32:29Z rtlfrm $
*------------------------------------------------------------------------------
*/

/**
 * \defgroup fec EtherCat Ethernet MAC driver for Frescale K60 SoCs.
 *
 * \{
 */

#ifndef FEC_H
#define FEC_H

#include <types.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct fec_mac_address
{
   uint8_t octet[6];
} fec_mac_address_t;

int fec_ecat_init (const fec_mac_address_t * mac_address, bool phy_loopback_mode);

int fec_ecat_send (const void *payload, size_t tot_len);

int fec_ecat_recv (void * buffer, size_t buffer_length);

#ifdef __cplusplus
}
#endif

#endif /* FEC_H */

/**
 * \}
 */
