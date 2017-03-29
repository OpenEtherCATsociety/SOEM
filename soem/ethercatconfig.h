/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatconfig.c
 */

#ifndef _ethercatconfig_
#define _ethercatconfig_

#ifdef __cplusplus
extern "C"
{
#endif

#define EC_NODEOFFSET      0x1000
#define EC_TEMPNODE        0xffff

#ifdef EC_VER1
int ec_config_init(uint8 usetable);
int ec_config_map(void *pIOmap);
int ec_config_map_group(void *pIOmap, uint8 group);
int ec_config(uint8 usetable, void *pIOmap);
int ec_recover_slave(uint16 slave, int timeout);
int ec_reconfig_slave(uint16 slave, int timeout);
#endif

int ecx_config_init(ecx_contextt *context, uint8 usetable);
int ecx_config_map_group(ecx_contextt *context, void *pIOmap, uint8 group);
int ecx_recover_slave(ecx_contextt *context, uint16 slave, int timeout);
int ecx_reconfig_slave(ecx_contextt *context, uint16 slave, int timeout);

#ifdef __cplusplus
}
#endif

#endif
