/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for ec_config.c
 */

#ifndef _ec_config_
#define _ec_config_

#ifdef __cplusplus
extern "C" {
#endif

#define EC_NODEOFFSET 0x1000
#define EC_TEMPNODE   0xffff

int ecx_config_init(ecx_contextt *context);
int ecx_config_map_group(ecx_contextt *context, void *pIOmap, uint8 group);
int ecx_recover_slave(ecx_contextt *context, uint16 slave, int timeout);
int ecx_reconfig_slave(ecx_contextt *context, uint16 slave, int timeout);

#ifdef __cplusplus
}
#endif

#endif
