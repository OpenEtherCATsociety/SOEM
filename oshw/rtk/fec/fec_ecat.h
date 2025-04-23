/*********************************************************************
 *        _       _         _
 *  _ __ | |_  _ | |  __ _ | |__   ___
 * | '__|| __|(_)| | / _` || '_ \ / __|
 * | |   | |_  _ | || (_| || |_) |\__ \
 * |_|    \__|(_)|_| \__,_||_.__/ |___/
 *
 * http://www.rt-labs.com
 * Copyright 2011 rt-labs AB, Sweden.
 * See LICENSE file in the project root for full license information.
 ********************************************************************/

/**
 * \defgroup fec EtherCat Ethernet MAC driver for Frescale SoCs.
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

#ifdef __cplusplus
}
#endif

#endif /* FEC_H */

/**
 * \}
 */
