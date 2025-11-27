/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * Headerfile for ec_foe.c
 */

#ifndef _ec_eoe_
#define _ec_eoe_

#ifdef __cplusplus
extern "C" {
#endif

#include "soem/soem.h"

/* use maximum size for EOE mailbox data - mbxheader and 2x frameinfo */
#define EC_MAXEOEDATA (EC_MAXMBX - (sizeof(ec_mbxheadert) + \
                                    sizeof(uint16_t) +      \
                                    sizeof(uint16_t)))

/** DNS length according to ETG 1000.6 */
#define EOE_DNS_NAME_LENGTH     32
/** Ethernet address length not including VLAN */
#define EOE_ETHADDR_LENGTH      6
/** IPv4 address length */
#define EOE_IP4_LENGTH          sizeof(uint32_t)

#define EOE_MAKEU32(a, b, c, d) (((uint32_t)((a) & 0xff) << 24) | \
                                 ((uint32_t)((b) & 0xff) << 16) | \
                                 ((uint32_t)((c) & 0xff) << 8) |  \
                                 (uint32_t)((d) & 0xff))

#if !defined(EC_BIG_ENDIAN) && defined(EC_LITTLE_ENDIAN)

#define EOE_HTONS(x) ((((x) & 0x00ffUL) << 8) | (((x) & 0xff00UL) >> 8))
#define EOE_NTOHS(x) EOE_HTONS(x)
#define EOE_HTONL(x) ((((x) & 0x000000ffUL) << 24) | \
                      (((x) & 0x0000ff00UL) << 8) |  \
                      (((x) & 0x00ff0000UL) >> 8) |  \
                      (((x) & 0xff000000UL) >> 24))
#define EOE_NTOHL(x) EOE_HTONL(x)
#else
#define EOE_HTONS(x) (x)
#define EOE_NTOHS(x) (x)
#define EOE_HTONL(x) (x)
#define EOE_NTOHL(x) (x)
#endif /* !defined(EC_BIG_ENDIAN) && defined(EC_LITTLE_ENDIAN) */

/** Get one byte from the 4-byte address */
#define eoe_ip4_addr1(ipaddr) (((const uint8_t *)(&(ipaddr)->addr))[0])
#define eoe_ip4_addr2(ipaddr) (((const uint8_t *)(&(ipaddr)->addr))[1])
#define eoe_ip4_addr3(ipaddr) (((const uint8_t *)(&(ipaddr)->addr))[2])
#define eoe_ip4_addr4(ipaddr) (((const uint8_t *)(&(ipaddr)->addr))[3])

/** Set an IP address given by the four byte-parts */
#define EOE_IP4_ADDR_TO_U32(ipaddr, a, b, c, d) \
   (ipaddr)->addr = EOE_HTONL(EOE_MAKEU32(a, b, c, d))

/** Header frame info 1 */
#define EOE_HDR_FRAME_TYPE_OFFSET         0
#define EOE_HDR_FRAME_TYPE                (0xF << 0)
#define EOE_HDR_FRAME_TYPE_SET(x)         (((x) & 0xF) << 0)
#define EOE_HDR_FRAME_TYPE_GET(x)         (((x) >> 0) & 0xF)
#define EOE_HDR_FRAME_PORT_OFFSET         4
#define EOE_HDR_FRAME_PORT                (0xF << 4)
#define EOE_HDR_FRAME_PORT_SET(x)         ((uint16)(((x) & 0xF) << 4))
#define EOE_HDR_FRAME_PORT_GET(x)         (((x) >> 4) & 0xF)
#define EOE_HDR_LAST_FRAGMENT_OFFSET      8
#define EOE_HDR_LAST_FRAGMENT             (0x1 << 8)
#define EOE_HDR_LAST_FRAGMENT_SET(x)      (((x) & 0x1) << 8)
#define EOE_HDR_LAST_FRAGMENT_GET(x)      (((x) >> 8) & 0x1)
#define EOE_HDR_TIME_APPEND_OFFSET        9
#define EOE_HDR_TIME_APPEND               (0x1 << 9)
#define EOE_HDR_TIME_APPEND_SET(x)        (((x) & 0x1) << 9)
#define EOE_HDR_TIME_APPEND_GET(x)        (((x) >> 9) & 0x1)
#define EOE_HDR_TIME_REQUEST_OFFSET       10
#define EOE_HDR_TIME_REQUEST              (0x1 << 10)
#define EOE_HDR_TIME_REQUEST_SET(x)       (((x) & 0x1) << 10)
#define EOE_HDR_TIME_REQUEST_GET(x)       (((x) >> 10) & 0x1)

/** Header frame info 2 */
#define EOE_HDR_FRAG_NO_OFFSET            0
#define EOE_HDR_FRAG_NO                   (0x3F << 0)
#define EOE_HDR_FRAG_NO_SET(x)            ((uint16)(((x) & 0x3F) << 0))
#define EOE_HDR_FRAG_NO_GET(x)            (((x) >> 0) & 0x3F)
#define EOE_HDR_FRAME_OFFSET_OFFSET       6
#define EOE_HDR_FRAME_OFFSET              (0x3F << 6)
#define EOE_HDR_FRAME_OFFSET_SET(x)       ((uint16)(((x) & 0x3F) << 6))
#define EOE_HDR_FRAME_OFFSET_GET(x)       (((x) >> 6) & 0x3F)
#define EOE_HDR_FRAME_NO_OFFSET           12
#define EOE_HDR_FRAME_NO                  (0xF << 12)
#define EOE_HDR_FRAME_NO_SET(x)           ((uint16)(((x) & 0xF) << 12))
#define EOE_HDR_FRAME_NO_GET(x)           (((x) >> 12) & 0xF)

/** EOE param */
#define EOE_PARAM_OFFSET                  4
#define EOE_PARAM_MAC_INCLUDE             (0x1 << 0)
#define EOE_PARAM_IP_INCLUDE              (0x1 << 1)
#define EOE_PARAM_SUBNET_IP_INCLUDE       (0x1 << 2)
#define EOE_PARAM_DEFAULT_GATEWAY_INCLUDE (0x1 << 3)
#define EOE_PARAM_DNS_IP_INCLUDE          (0x1 << 4)
#define EOE_PARAM_DNS_NAME_INCLUDE        (0x1 << 5)

/** EoE frame types */
#define EOE_FRAG_DATA                     0
#define EOE_INIT_RESP_TIMESTAMP           1
#define EOE_INIT_REQ                      2 /* Spec SET IP REQ */
#define EOE_INIT_RESP                     3 /* Spec SET IP RESP */
#define EOE_SET_ADDR_FILTER_REQ           4
#define EOE_SET_ADDR_FILTER_RESP          5
#define EOE_GET_IP_PARAM_REQ              6
#define EOE_GET_IP_PARAM_RESP             7
#define EOE_GET_ADDR_FILTER_REQ           8
#define EOE_GET_ADDR_FILTER_RESP          9

/** EoE parameter result codes */
#define EOE_RESULT_SUCCESS                0x0000
#define EOE_RESULT_UNSPECIFIED_ERROR      0x0001
#define EOE_RESULT_UNSUPPORTED_FRAME_TYPE 0x0002
#define EOE_RESULT_NO_IP_SUPPORT          0x0201
#define EOE_RESULT_NO_DHCP_SUPPORT        0x0202
#define EOE_RESULT_NO_FILTER_SUPPORT      0x0401

/** EOE ip4 address in network order */
typedef struct eoe_ip4_addr
{
   uint32_t addr;
} eoe_ip4_addr_t;

/** EOE ethernet address */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED eoe_ethaddr
{
   uint8_t addr[EOE_ETHADDR_LENGTH];
} eoe_ethaddr_t;
OSAL_PACKED_END

/** EoE IP request structure, storage only, no need to pack */
typedef struct eoe_param
{
   uint8_t mac_set : 1;
   uint8_t ip_set : 1;
   uint8_t subnet_set : 1;
   uint8_t default_gateway_set : 1;
   uint8_t dns_ip_set : 1;
   uint8_t dns_name_set : 1;
   eoe_ethaddr_t mac;
   eoe_ip4_addr_t ip;
   eoe_ip4_addr_t subnet;
   eoe_ip4_addr_t default_gateway;
   eoe_ip4_addr_t dns_ip;
   char dns_name[EOE_DNS_NAME_LENGTH];
} eoe_param_t;

/** EOE structure.
 * Used to interpret EoE mailbox packets.
 */
OSAL_PACKED_BEGIN
typedef struct OSAL_PACKED
{
   ec_mbxheadert mbxheader;
   uint16_t frameinfo1;
   union
   {
      uint16_t frameinfo2;
      uint16_t result;
   };
   uint8 data[EC_MAXEOEDATA];
} ec_EOEt;
OSAL_PACKED_END

int ecx_EOEdefinehook(ecx_contextt *context, void *hook);
int ecx_EOEsetIp(ecx_contextt *context,
                 uint16 slave,
                 uint8 port,
                 eoe_param_t *ipparam,
                 int timeout);
int ecx_EOEgetIp(ecx_contextt *context,
                 uint16 slave,
                 uint8 port,
                 eoe_param_t *ipparam,
                 int timeout);
int ecx_EOEsend(ecx_contextt *context,
                uint16 slave,
                uint8 port,
                int psize,
                void *p,
                int timeout);
int ecx_EOErecv(ecx_contextt *context,
                uint16 slave,
                uint8 port,
                int *psize,
                void *p,
                int timeout);
int ecx_EOEreadfragment(
    ec_mbxbuft *MbxIn,
    uint8 *rxfragmentno,
    uint16 *rxframesize,
    uint16 *rxframeoffset,
    uint16 *rxframeno,
    int *psize,
    void *p);

#ifdef __cplusplus
}
#endif

#endif
