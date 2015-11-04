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
* $Id: fec_ecat.c 138 2014-04-11 09:11:48Z rtlfrm $
*------------------------------------------------------------------------------
*/

#include "fec_ecat.h"

#include <dev.h>
#include <kern.h>
#include <string.h>
#include <uassert.h>
#include <eth/fec_buffer.h>
#include <eth/phy/mii.h>
#include <bsp.h>
#include <config.h>

#undef RTK_DEBUG                /* Print debugging info? */
#undef RTK_DEBUG_DATA           /* Print packets? */

#ifdef RTK_DEBUG
#define DPRINT(...) rprintp ("FEC: "__VA_ARGS__)
#else
#define DPRINT(...)
#endif  /* DEBUG */

#ifdef RTK_DEBUG_DATA
#define DUMP_PACKET(p, l) fec_ecat_dump_packet (p, l)
#else
#define DUMP_PACKET(p, l)
#endif  /* RTK_DEBUG_DATA */

#define ETHERNET_FCS_SIZE_IN_BYTES     4 /* Frame Check Sequence (32-bit CRC) */

#define DELAY(ms) task_delay (tick_from_ms (ms) + 1)

#define ETH_PHY_ADDRESS 0x1 /* Default jumper setting on TWR-K60F */

#define NUM_BUFFERS 2
#define FEC_ALLIGNED __attribute__((section(".dma"),aligned(16)))

/*
 * "The device's system clock is connected to the module clock,
 * as named in the Ethernet chapter. The minimum system clock frequency
 * for 100 Mbps operation is 25 MHz."
 *
 * - Kinetis K60 manual ch. 3.9.1.1 "Ethernet Clocking Options"
 */
#define FEC_MIN_MODULE_CLOCK_Hz (25 * 1000 * 1000)

/* Note that the core clock and the system clock has the same clock frequency */
#define FEC_MODULE_CLOCK_Hz CFG_IPG_CLOCK

//----------------------------------------------------------------------------//

typedef struct fec_mac_t
{
   uint32_t lower; /* octets 0, 1, 2, 3 */
   uint32_t upper; /* octets 4, 5 */
} fec_mac_t;

/* Message Information Block (MIB) Counters */
typedef struct fec_mib
{
   uint32_t rmon_t_drop;      /* MBAR_ETH + 0x200 */
   uint32_t rmon_t_packets;   /* MBAR_ETH + 0x204 */
   uint32_t rmon_t_bc_pkt;    /* MBAR_ETH + 0x208 */
   uint32_t rmon_t_mc_pkt;    /* MBAR_ETH + 0x20C */
   uint32_t rmon_t_crc_align; /* MBAR_ETH + 0x210 */
   uint32_t rmon_t_undersize; /* MBAR_ETH + 0x214 */
   uint32_t rmon_t_oversize;  /* MBAR_ETH + 0x218 */
   uint32_t rmon_t_frag;      /* MBAR_ETH + 0x21C */
   uint32_t rmon_t_jab;    /* MBAR_ETH + 0x220 */
   uint32_t rmon_t_col;    /* MBAR_ETH + 0x224 */
   uint32_t rmon_t_p64;    /* MBAR_ETH + 0x228 */
   uint32_t rmon_t_p65to127;  /* MBAR_ETH + 0x22C */
   uint32_t rmon_t_p128to255; /* MBAR_ETH + 0x230 */
   uint32_t rmon_t_p256to511; /* MBAR_ETH + 0x234 */
   uint32_t rmon_t_p512to1023;   /* MBAR_ETH + 0x238 */
   uint32_t rmon_t_p1024to2047;  /* MBAR_ETH + 0x23C */
   uint32_t rmon_t_p_gte2048; /* MBAR_ETH + 0x240 */
   uint32_t rmon_t_octets;    /* MBAR_ETH + 0x244 */
   uint32_t ieee_t_drop;      /* MBAR_ETH + 0x248 */
   uint32_t ieee_t_frame_ok;  /* MBAR_ETH + 0x24C */
   uint32_t ieee_t_1col;      /* MBAR_ETH + 0x250 */
   uint32_t ieee_t_mcol;      /* MBAR_ETH + 0x254 */
   uint32_t ieee_t_def;    /* MBAR_ETH + 0x258 */
   uint32_t ieee_t_lcol;      /* MBAR_ETH + 0x25C */
   uint32_t ieee_t_excol;     /* MBAR_ETH + 0x260 */
   uint32_t ieee_t_macerr;    /* MBAR_ETH + 0x264 */
   uint32_t ieee_t_cserr;     /* MBAR_ETH + 0x268 */
   uint32_t ieee_t_sqe;    /* MBAR_ETH + 0x26C */
   uint32_t t_fdxfc;    /* MBAR_ETH + 0x270 */
   uint32_t ieee_t_octets_ok; /* MBAR_ETH + 0x274 */
   uint32_t res13[2];      /* MBAR_ETH + 0x278-27C */
   uint32_t rmon_r_drop;      /* MBAR_ETH + 0x280 */
   uint32_t rmon_r_packets;   /* MBAR_ETH + 0x284 */
   uint32_t rmon_r_bc_pkt;    /* MBAR_ETH + 0x288 */
   uint32_t rmon_r_mc_pkt;    /* MBAR_ETH + 0x28C */
   uint32_t rmon_r_crc_align; /* MBAR_ETH + 0x290 */
   uint32_t rmon_r_undersize; /* MBAR_ETH + 0x294 */
   uint32_t rmon_r_oversize;  /* MBAR_ETH + 0x298 */
   uint32_t rmon_r_frag;      /* MBAR_ETH + 0x29C */
   uint32_t rmon_r_jab;    /* MBAR_ETH + 0x2A0 */
   uint32_t rmon_r_resvd_0;   /* MBAR_ETH + 0x2A4 */
   uint32_t rmon_r_p64;    /* MBAR_ETH + 0x2A8 */
   uint32_t rmon_r_p65to127;  /* MBAR_ETH + 0x2AC */
   uint32_t rmon_r_p128to255; /* MBAR_ETH + 0x2B0 */
   uint32_t rmon_r_p256to511; /* MBAR_ETH + 0x2B4 */
   uint32_t rmon_r_p512to1023;   /* MBAR_ETH + 0x2B8 */
   uint32_t rmon_r_p1024to2047;  /* MBAR_ETH + 0x2BC */
   uint32_t rmon_r_p_gte2048; /* MBAR_ETH + 0x2C0 */
   uint32_t rmon_r_octets;    /* MBAR_ETH + 0x2C4 */
   uint32_t ieee_r_drop;      /* MBAR_ETH + 0x2C8 */
   uint32_t ieee_r_frame_ok;  /* MBAR_ETH + 0x2CC */
   uint32_t ieee_r_crc;    /* MBAR_ETH + 0x2D0 */
   uint32_t ieee_r_align;     /* MBAR_ETH + 0x2D4 */
   uint32_t r_macerr;      /* MBAR_ETH + 0x2D8 */
   uint32_t r_fdxfc;    /* MBAR_ETH + 0x2DC */
   uint32_t ieee_r_octets_ok; /* MBAR_ETH + 0x2E0 */
   uint32_t res14[7];      /* MBAR_ETH + 0x2E4-2FC */
} fec_mib_t;

COMPILETIME_ASSERT (sizeof (fec_mib_t) == 0x100);

/* Register read/write struct */
typedef struct reg_fec
{
   uint32_t resv0;
   uint32_t eir;
   uint32_t eimr;
   uint32_t resv1;
   uint32_t rdar;
   uint32_t tdar;
   uint32_t resv2[3];
   uint32_t ecr;
   uint32_t resv3[6];
   uint32_t mmfr;
   uint32_t mscr;
   uint32_t resv4[7];
   uint32_t mibc;
   uint32_t resv5[7];
   uint32_t rcr;
   uint32_t resv6[15];
   uint32_t tcr;
   uint32_t resv7[7];
   fec_mac_t pa;              /* Physical address. PALR/PAUR */
   uint32_t opd;
   uint32_t resv8[10];
   uint32_t iaur;
   uint32_t ialr;
   uint32_t gaur;
   uint32_t galr;
   uint32_t resv9[7];
   uint32_t tfwr;
   uint32_t resv10;
   uint32_t frbr;
   uint32_t frsr;
   uint32_t resv11[11];
   uint32_t erdsr;
   uint32_t etdsr;
   uint32_t emrbr;
   uint32_t resv14;
   uint32_t rx_section_full;  /* Not present on all controllers */
   uint32_t rx_section_empty; /* Not present on all controllers */
   uint32_t rx_almost_empty;  /* Not present on all controllers */
   uint32_t rx_almost_full;   /* Not present on all controllers */
   uint32_t tx_section_empty; /* Not present on all controllers */
   uint32_t tx_almost_empty;  /* Not present on all controllers */
   uint32_t tx_almost_full;   /* Not present on all controllers */
   uint32_t resv12[21];
   const fec_mib_t mib;
   uint32_t fec_miigsk_cfgr;  /* Not present on all controllers */
   uint32_t resv13;
   uint32_t fec_miigsk_enr;   /* Not present on all controllers */
   uint32_t resv15[0x7d];
   fec_mac_t smac[4];         /* Not present on all controllers */
} reg_fec_t;

COMPILETIME_ASSERT (offsetof (reg_fec_t, emrbr) == 0x188);
COMPILETIME_ASSERT (offsetof (reg_fec_t, tx_almost_full) == 0x1A8);
COMPILETIME_ASSERT (offsetof (reg_fec_t, fec_miigsk_cfgr) == 0x300);
COMPILETIME_ASSERT (offsetof (reg_fec_t, smac) == 0x500);
COMPILETIME_ASSERT (offsetof (reg_fec_t, smac[3]) == 0x518);

//----------------------------------------------------------------------------//

/* Bit definitions and macros for FEC_EIR */
#define FEC_EIR_CLEAR_ALL     (0xFFF80000)
#define FEC_EIR_HBERR         (0x80000000)
#define FEC_EIR_BABR          (0x40000000)
#define FEC_EIR_BABT          (0x20000000)
#define FEC_EIR_GRA           (0x10000000)
#define FEC_EIR_TXF           (0x08000000)
#define FEC_EIR_TXB           (0x04000000)
#define FEC_EIR_RXF           (0x02000000)
#define FEC_EIR_RXB           (0x01000000)
#define FEC_EIR_MII           (0x00800000)
#define FEC_EIR_EBERR         (0x00400000)
#define FEC_EIR_LC            (0x00200000)
#define FEC_EIR_RL            (0x00100000)
#define FEC_EIR_UN            (0x00080000)

/* Bit definitions and macros for FEC_RDAR */
#define FEC_RDAR_R_DES_ACTIVE (0x01000000)

/* Bit definitions and macros for FEC_TDAR */
#define FEC_TDAR_X_DES_ACTIVE (0x01000000)

/* Bit definitions and macros for FEC_ECR */
#define FEC_ECR_DBSWP         (0x00000100)
#define FEC_ECR_ETHER_EN      (0x00000002)
#define FEC_ECR_RESET         (0x00000001)

/* Bit definitions and macros for FEC_MMFR */
#define FEC_MMFR_DATA(x)      (((x)&0xFFFF))
#define FEC_MMFR_ST(x)        (((x)&0x03)<<30)
#define FEC_MMFR_ST_01        (0x40000000)
#define FEC_MMFR_OP_RD        (0x20000000)
#define FEC_MMFR_OP_WR        (0x10000000)
#define FEC_MMFR_PA(x)        (((x)&0x1F)<<23)
#define FEC_MMFR_RA(x)        (((x)&0x1F)<<18)
#define FEC_MMFR_TA(x)        (((x)&0x03)<<16)
#define FEC_MMFR_TA_10        (0x00020000)

/* Bit definitions and macros for FEC_MSCR */
#define FEC_MSCR_DIS_PREAMBLE (0x00000080)
#define FEC_MSCR_MII_SPEED(x) (((x)&0x3F)<<1)

/* Bit definitions and macros for FEC_MIBC */
#define FEC_MIBC_MIB_DISABLE      (0x80000000)
#define FEC_MIBC_MIB_IDLE      (0x40000000)

/* Bit definitions and macros for FEC_RCR */
#define FEC_RCR_GRS           (0x80000000)
#define FEC_RCR_NO_LGTH_CHECK (0x40000000)
#define FEC_RCR_MAX_FL(x)     (((x)&0x7FF)<<16)
#define FEC_RCR_CNTL_FRM_ENA  (0x00008000)
#define FEC_RCR_CRC_FWD       (0x00004000)
#define FEC_RCR_PAUSE_FWD     (0x00002000)
#define FEC_RCR_PAD_EN        (0x00001000)
#define FEC_RCR_RMII_ECHO     (0x00000800)
#define FEC_RCR_RMII_LOOP     (0x00000400)
#define FEC_RCR_RMII_10T      (0x00000200)
#define FEC_RCR_RMII_MODE     (0x00000100)
#define FEC_RCR_SGMII_ENA     (0x00000080)
#define FEC_RCR_RGMII_ENA     (0x00000040)
#define FEC_RCR_FCE           (0x00000020)
#define FEC_RCR_BC_REJ        (0x00000010)
#define FEC_RCR_PROM          (0x00000008)
#define FEC_RCR_MII_MODE      (0x00000004)
#define FEC_RCR_DRT           (0x00000002)
#define FEC_RCR_LOOP          (0x00000001)

/* Bit definitions and macros for FEC_TCR */
#define FEC_TCR_RFC_PAUSE     (0x00000010)
#define FEC_TCR_TFC_PAUSE     (0x00000008)
#define FEC_TCR_FDEN          (0x00000004)
#define FEC_TCR_HBC           (0x00000002)
#define FEC_TCR_GTS           (0x00000001)

/* Bit definitions and macros for FEC_PAUR */
#define FEC_PAUR_PADDR2(x)    (((x)&0xFFFF)<<16)
#define FEC_PAUR_TYPE(x)      ((x)&0xFFFF)

/* Bit definitions and macros for FEC_OPD */
#define FEC_OPD_PAUSE_DUR(x)  (((x)&0x0000FFFF)<<0)
#define FEC_OPD_OPCODE(x)     (((x)&0x0000FFFF)<<16)

/* Bit definitions and macros for FEC_TFWR */
#define FEC_TFWR_STR_FWD      BIT (8) /* Present on ENET but not on FEC */
#define FEC_TFWR_X_WMRK(x)    ((x)&0x03)
#define FEC_TFWR_X_WMRK_64    (0x01)
#define FEC_TFWR_X_WMRK_128   (0x02)
#define FEC_TFWR_X_WMRK_192   (0x03)

/* Bit definitions and macros for FEC_FRBR */
#define FEC_FRBR_R_BOUND(x)   (((x)&0xFF)<<2)

/* Bit definitions and macros for FEC_FRSR */
#define FEC_FRSR_R_FSTART(x)  (((x)&0xFF)<<2)

/* Bit definitions and macros for FEC_ERDSR */
#define FEC_ERDSR_R_DES_START(x)    (((x)&0x3FFFFFFF)<<2)

/* Bit definitions and macros for FEC_ETDSR */
#define FEC_ETDSR_X_DES_START(x)    (((x)&0x3FFFFFFF)<<2)

/* Bit definitions and macros for FEC_EMRBR */
#define FEC_EMRBR_R_BUF_SIZE(x)     (((x)&0x7F)<<4)

//----------------------------------------------------------------------------//

/* the defines of MII operation */
#define FEC_MII_ST      0x40000000
#define FEC_MII_OP_OFF  28
#define FEC_MII_OP_MASK 0x03
#define FEC_MII_OP_RD   0x02
#define FEC_MII_OP_WR   0x01
#define FEC_MII_PA_OFF  23
#define FEC_MII_PA_MASK 0xFF
#define FEC_MII_RA_OFF  18
#define FEC_MII_RA_MASK 0xFF
#define FEC_MII_TA      0x00020000
#define FEC_MII_DATA_OFF 0
#define FEC_MII_DATA_MASK 0x0000FFFF

#define FEC_MII_FRAME   (FEC_MII_ST | FEC_MII_TA)
#define FEC_MII_OP(x)   (((x) & FEC_MII_OP_MASK) << FEC_MII_OP_OFF)
#define FEC_MII_PA(pa)  (((pa) & FEC_MII_PA_MASK) << FEC_MII_PA_OFF)
#define FEC_MII_RA(ra)  (((ra) & FEC_MII_RA_MASK) << FEC_MII_RA_OFF)
#define FEC_MII_SET_DATA(v) (((v) & FEC_MII_DATA_MASK) << FEC_MII_DATA_OFF)
#define FEC_MII_GET_DATA(v) (((v) >> FEC_MII_DATA_OFF) & FEC_MII_DATA_MASK)
#define FEC_MII_READ(pa, ra) ((FEC_MII_FRAME | FEC_MII_OP(FEC_MII_OP_RD)) |\
      FEC_MII_PA(pa) | FEC_MII_RA(ra))
#define FEC_MII_WRITE(pa, ra, v) (FEC_MII_FRAME | FEC_MII_OP(FEC_MII_OP_WR)|\
      FEC_MII_PA(pa) | FEC_MII_RA(ra) | FEC_MII_SET_DATA(v))

#define FEC_MII_TIMEOUT		10 // 1 * 10 ms
#define FEC_MII_TICK        1 // 1 ms

//----------------------------------------------------------------------------//

/* Ethernet Transmit and Receive Buffers */
#define PKT_MAXBUF_SIZE		1518

//----------------------------------------------------------------------------//

typedef enum fec_phy_inteface
{
   /**
    * MII, Media Independent Interface
    * 4-bit operating at 25 Mhz
    */
   FEC_PHY_MII,
   /**
    * RMII, Reduced Media Independent Interface
    * 2-bit operating at 50 Mhz
    */
   FEC_PHY_RMII,
   FEC_PHY_RGMII
} fec_phy_inteface_t;

typedef struct fec_cfg
{
   uint32_t base;
   uint32_t clock;
   fec_buffer_bd_t * tx_bd_base;
   fec_buffer_bd_t * rx_bd_base;
   fec_phy_inteface_t phy_interface;
} fec_cfg_t;

typedef struct fec
{
   uint32_t clock;
   fec_buffer_bd_t * tx_bd_base;
   fec_buffer_bd_t * rx_bd_base;
   volatile reg_fec_t *base;
   phy_t * phy;
   fec_phy_inteface_t phy_interface;
} fec_t;

/* Buffer descriptors. */
static fec_buffer_bd_t fec_tx_bd[NUM_BUFFERS] FEC_ALLIGNED;
static fec_buffer_bd_t fec_rx_bd[NUM_BUFFERS] FEC_ALLIGNED;

/* Data buffers. */
static uint8_t fec_tx_data[NUM_BUFFERS * TX_BUFFER_SIZE] FEC_ALLIGNED;
static uint8_t fec_rx_data[NUM_BUFFERS * RX_BUFFER_SIZE] FEC_ALLIGNED;

static fec_t *fec;

//----------------------------------------------------------------------------//

#ifdef RTK_DEBUG
const char * fec_ecat_link_duplex_name (uint8_t link_state)
{
   if (link_state & PHY_LINK_OK)
   {
      if (link_state & PHY_LINK_FULL_DUPLEX)
      {
         return "full duplex";
      }
      else
      {
         return "half duplex";
      }
   }
   else
   {
      return "no duplex (link down)";
   }
}

const char * fec_ecat_link_speed_name (uint8_t link_state)
{
   if (link_state & PHY_LINK_OK)
   {
      if (link_state & PHY_LINK_10MBIT)
      {
         return "10 Mbps";
      }
      else if (link_state & PHY_LINK_100MBIT)
      {
         return "100 Mbps";
      }
      else if (link_state & PHY_LINK_1000MBIT)
      {
         return "1 Gbps";
      }
      else
      {
         return "unknown speed";
      }
   }
   else
   {
      return "0 Mbps (link down)";
   }
}
#endif /* RTK_DEBUG */

#ifdef RTK_DEBUG_DATA
#include <ctype.h>
static void fec_ecat_dump_packet (const uint8_t * payload, size_t len)
{
   size_t i, j, n;
   char s[80];

   ASSERT (payload != NULL);

   for (i = 0; i < len; i += 16)
   {
      n = 0;
      for (j = 0; j < 16 && (i + j) < len; j++)
      {
         ASSERT (n <= sizeof(s));
         n += rsnprintf (s + n, sizeof(s) - n, "%02x ", payload[i + j]);
      }
      for (; j < 16; j++)
      {
         ASSERT (n <= sizeof(s));
         n += rsnprintf (s + n, sizeof(s) - n, "   ");
      }
      ASSERT (n <= sizeof(s));
      n += rsnprintf (s + n, sizeof(s) - n, "|");
      for (j = 0; j < 16 && (i + j) < len; j++)
      {
         uint8_t c = payload[i + j];
         c = (isprint (c)) ? c : '.';
         ASSERT (n <= sizeof(s));
         n += rsnprintf (s + n, sizeof(s) - n, "%c", c);
      }
      ASSERT (n <= sizeof(s));
      n += rsnprintf (s + n, sizeof(s) - n, "|\n");
      ASSERT (n <= sizeof(s));
      DPRINT ("%s", s);
   }
}
#endif  /* DEBUG_DATA */

static uint16_t fec_ecat_read_phy (void * arg, uint8_t address, uint8_t reg)
{
   fec->base->eir = FEC_EIR_MII; // Clear interrupt.
   fec->base->mmfr = FEC_MII_READ(address, reg); // Write read command.
   while (!(fec->base->eir & FEC_EIR_MII))
   {
      ; // Wait for interrupt.
   }
   fec->base->eir = FEC_EIR_MII; // Clear interrupt.
   uint16_t data = FEC_MII_GET_DATA(fec->base->mmfr);
   return data; // Return read data.
}

static void fec_ecat_write_phy (void * arg, uint8_t address, uint8_t reg,
      uint16_t value)
{
   fec->base->eir = FEC_EIR_MII; // Clear interrupt.
   fec->base->mmfr = FEC_MII_WRITE(address, reg, value); // Write data.
   while (!(fec->base->eir & FEC_EIR_MII))
   {
      ; // Wait for interrupt.
   }
   fec->base->eir = FEC_EIR_MII; // Clear interrupt.
}

static void fec_ecat_init_hw (const fec_mac_address_t * mac_address)
{
   uint32_t mii_speed;

   /* Hard reset */
   fec->base->ecr = FEC_ECR_RESET;
   while (fec->base->ecr & FEC_ECR_RESET)
   {
      ;
   }

   // Configure MDC clock.
   mii_speed = (fec->clock + 499999) / 5000000;
   fec->base->mscr = (fec->base->mscr & (~0x7E)) | (mii_speed << 1);

   // Receive control register
   fec->base->rcr = FEC_RCR_MAX_FL(PKT_MAXBUF_SIZE) | FEC_RCR_MII_MODE;

   // set RMII mode in RCR register.
   if (fec->phy_interface == FEC_PHY_RMII)
   {
      fec->base->rcr |= FEC_RCR_RMII_MODE;
   }
   else if(fec->phy_interface == FEC_PHY_RGMII)
   {
      fec->base->rcr &= ~(FEC_RCR_RMII_MODE | FEC_RCR_MII_MODE );
      fec->base->rcr  |= (FEC_RCR_RGMII_ENA | FEC_RCR_MII_MODE);
   }

   // Reset phy
   if (fec->phy->ops->reset)
   {
      fec->phy->ops->reset (fec->phy);
   }

   /* Don't receive any unicast frames except those whose destination address
    * equals our physical address.
    */
   fec->base->iaur = 0;
   fec->base->ialr = 0;

   /* Receive all multicast frames. */
   fec->base->gaur = UINT32_MAX;
   fec->base->galr = UINT32_MAX;

   /* Set our physical address. */
   fec->base->pa.lower = (mac_address->octet[0] << 24) +
                         (mac_address->octet[1] << 16) +
                         (mac_address->octet[2] << 8)  +
                         (mac_address->octet[3] << 0);
   fec->base->pa.upper = (mac_address->octet[4] << 24) +
                         (mac_address->octet[5] << 16) +
                         0x8808;

   /* Start link autonegotiation */
   fec->phy->ops->start (fec->phy);
}

int fec_ecat_send (const void *payload, size_t tot_len)
{
   fec_buffer_bd_t * bd;

   /* Frames larger than the maximum Ethernet frame size are not allowed. */
   ASSERT (tot_len <= PKT_MAXBUF_SIZE);
   ASSERT (tot_len <= TX_BUFFER_SIZE);

   /* Bus errors should never occur, unless the MPU is enabled and forbids
    * the Ethernet MAC DMA from accessing the descriptors or buffers.
    */
   ASSERT ((fec->base->eir & FEC_EIR_EBERR) == false);

   /* Allocate a transmit buffer. We wait here until the MAC has released at
    * least one buffer. This could take a couple of microseconds.
    */
   bd = NULL;
   while (bd == NULL)
   {
      bd = fec_buffer_get_tx ();
   }

   DPRINT ("out (%u):\n", tot_len);
   DUMP_PACKET (payload, tot_len);

   /* Copy frame to allocated buffer */
   memcpy (bd->data, payload, tot_len);
   bd->length = tot_len;

   fec_buffer_produce_tx (bd);

   /* Wait for previous transmissions to complete.
    *
    * This is a workaround for Freescale Kinetis errata e6358.
    * See "Mask Set Errata for Mask 3N96B".
    */
   while (fec->base->tdar)
   {
      ;
   }

   /* Transmit frame */
   fec->base->tdar = 1;

   return tot_len;
}

int fec_ecat_recv (void * buffer, size_t buffer_length)
{
   fec_buffer_bd_t * bd;
   int return_value;
   size_t frame_length_without_fcs;

   /* Bus errors should never occur, unless the MPU is enabled and forbids
    * the Ethernet MAC DMA from accessing the descriptors or buffers.
    */
   ASSERT ((fec->base->eir & FEC_EIR_EBERR) == false);

   bd = fec_buffer_get_rx ();
   if (bd == NULL)
   {
      /* No frame received. Not an error. */
      return_value = 0;
      return return_value;
   }

   /* The FCS CRC should not be returned to user, so subtract that. */
   frame_length_without_fcs = bd->length - ETHERNET_FCS_SIZE_IN_BYTES;

   /* A frame was received. Handle it and then return its buffer to hardware */
   if ((bd->status & BD_RX_L)== 0){
      /* Buffer is not last buffer in frame. This really should never
       * happen as our buffers are large enough to contain any
       * Ethernet frame. Nevertheless, is has been observed to happen.
       * Drop the packet and hope for the best.
       */
      DPRINT ("recv(): End of frame not found. Status: 0x%x\n", bd->status);
      return_value = -1;
   }
   else if (bd->status & (BD_RX_LG | BD_RX_NO | BD_RX_CR | BD_RX_OV))
   {
      DPRINT ("recv(): Frame is damaged. Status: 0x%x\n", bd->status);
      return_value = -1;
   }
   else if (buffer_length >= frame_length_without_fcs)
   {
      /* No errors detected, so frame should be valid. */
      ASSERT (frame_length_without_fcs > 0);

      memcpy (buffer, bd->data, frame_length_without_fcs);
      DPRINT ("in (%u):\n", frame_length_without_fcs);
      DUMP_PACKET (buffer, frame_length_without_fcs);
      return_value = frame_length_without_fcs;
   }
   else
   {
      DPRINT ("received_frame: User buffer is too small.\n");
      return_value = -1;
   }

   /* Tell the HW that there are new free RX buffers. */
   fec_buffer_produce_rx (bd);
   fec->base->rdar = 1;

   return return_value;
}

static void fec_ecat_hotplug (void)
{
   uint8_t link_state = 0;

   /* Disable frame reception/transmission */
   fec->base->ecr &= ~FEC_ECR_ETHER_EN;

   /* Set duplex mode according to link state */

   link_state = fec->phy->ops->get_link_state (fec->phy);

   /* Set duplex */
   if (link_state & PHY_LINK_FULL_DUPLEX)
   {
      fec->base->tcr |= FEC_TCR_FDEN;
      fec->base->rcr &= ~FEC_RCR_DRT;
   }
   else
   {
      fec->base->tcr &= ~FEC_TCR_FDEN;
      fec->base->rcr |= FEC_RCR_DRT;
   }

   /* Set RMII 10-Mbps mode. */
   if (link_state & PHY_LINK_10MBIT)
   {
      fec->base->rcr |= FEC_RCR_RMII_10T;
   }
   else
   {
      fec->base->rcr &= ~FEC_RCR_RMII_10T;
   }

   /* Clear any pending interrupt */
   fec->base->eir = 0xffffffff;

   // Disable all interrupts.
   fec->base->eimr = 0x0;

   /* Configure the amount of data required in the
    * transmit FIFO before transmission of a frame can begin.
    */
   /* All data in frame (i.e. store-and-forward mode). */
   fec->base->tfwr = FEC_TFWR_STR_FWD;

   // Reset buffers.
   fec_buffer_reset ();

   // FEC_ERDSR - Receive buffer descriptor ring start register
   fec->base->erdsr = (uint32_t)fec->rx_bd_base;

   // FEC_ETDSR - Transmit buffer descriptor ring start register
   fec->base->etdsr = (uint32_t)fec->tx_bd_base;

   // FEC_EMRBR - Maximum receive buffer size register
   fec->base->emrbr = RX_BUFFER_SIZE - 1;

   /* Let controller count frames. Useful for debugging */
   fec->base->mibc = 0x0;

   /* Now enable the transmit and receive processing.
    *
    * Also let MAC hardware convert DMA descriptor fields between little endian
    * (as accessed by ARM core) and big endian (as used internally by the
    * MAC hardware. Software would otherwise need to do the conversions.
    */
   fec->base->ecr = FEC_ECR_ETHER_EN | FEC_ECR_DBSWP;

   /* Indicate that there have been empty receive buffers produced */
   // FEC_RDAR  - Receive Descriptor ring - Receive descriptor active register
   fec->base->rdar = 1;

   DPRINT ("Link up. Speed: %s. Mode: %s.\n", fec_ecat_link_speed_name (link_state),
         fec_ecat_link_duplex_name (link_state));
}

static dev_state_t fec_ecat_probe (void)
{
   uint8_t link_state;

   link_state = fec->phy->ops->get_link_state (fec->phy);

   return (link_state & PHY_LINK_OK) ? StateAttached : StateDetached;
}

COMPILETIME_ASSERT (FEC_MODULE_CLOCK_Hz >= FEC_MIN_MODULE_CLOCK_Hz);

int fec_ecat_init (const fec_mac_address_t * mac_address,
      bool phy_loopback_mode)
{
   dev_state_t state;
   static const fec_cfg_t eth_cfg =
   {
      .base          = ENET_BASE,
      .clock         = FEC_MODULE_CLOCK_Hz,
      .tx_bd_base    = fec_tx_bd,
      .rx_bd_base    = fec_rx_bd,
      .phy_interface = FEC_PHY_RMII,
   };
   static phy_cfg_t phy_cfg =
   {
      .address       = ETH_PHY_ADDRESS,
      .read          = NULL, /* Set by MAC driver */
      .write         = NULL, /* Set by MAC driver */
   };

   phy_cfg.loopback_mode = phy_loopback_mode;

   /* Initialise buffers and buffer descriptors.*/
   fec_buffer_init_tx (fec_tx_bd, fec_tx_data, NUM_BUFFERS);
   fec_buffer_init_rx (fec_rx_bd, fec_rx_data, NUM_BUFFERS);

   fec = malloc (sizeof (fec_t));
   UASSERT (fec != NULL, EMEM);

   /* Initialise driver state */
   fec->rx_bd_base        = eth_cfg.rx_bd_base;
   fec->tx_bd_base        = eth_cfg.tx_bd_base;
   fec->clock             = eth_cfg.clock;
   fec->base              = (reg_fec_t *)eth_cfg.base;
   fec->phy_interface     = eth_cfg.phy_interface;
   fec->phy               = mii_init (&phy_cfg);
   fec->phy->arg          = fec;
   fec->phy->read         = fec_ecat_read_phy;
   fec->phy->write        = fec_ecat_write_phy;

   /* Initialize hardware */
   fec_ecat_init_hw (mac_address);
   state = StateDetached;
   while (state == StateDetached)
   {
      state = fec_ecat_probe ();
   }
   fec_ecat_hotplug ();

   return 0;
}
