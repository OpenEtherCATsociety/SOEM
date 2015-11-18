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
 * Copyright (C) 2006. rt-labs AB, Sweden. All rights reserved.
 *------------------------------------------------------------------------------
 * $Id: lw_emac.c 348 2012-10-18 16:41:14Z rtlfrm $
 *
 *
 *------------------------------------------------------------------------------
 */

#include <bsp.h>
#include <kern.h>
#include <config.h>
#include <bfin_dma.h>
#include <string.h>

#include "lw_emac.h"

/* MII standard registers.
   See IEEE Std 802.3-2005 clause 22:
   "Reconciliation Sublayer (RS) and Media Independent Interface (MII)",
   section 2.4.1 to 3.
   http://standards.ieee.org/getieee802/download/802.3-2005_section2.pdf */
#define MII_BMCR    0x00        /* Basic Mode Control Register */
#define MII_BMSR    0x01        /* Basic Mode Status Register */
#define MII_PHYIDR1 0x02        /* PHY Identifier Register 1 */
#define MII_PHYIDR2 0x03        /* PHY Identifier Register 2 */
#define MII_LPAR    0x05        /* Auto-Negotiation Link Partner Ability Register */

/* MII basic mode control register */
#define MII_BMCR_RST                BIT (15)
#define MII_BMCR_ANEG_EN            BIT (12)
#define MII_BMCR_ANEG_RST           BIT (9)

/* MII basic mode status register */
#define MII_BMSR_ANEGACK            BIT (5)
#define MII_BMSR_LINK               BIT (2)

/* MII auto-negotiation advertisement register */
#define MII_ANAR_100_FD             BIT (8)  /* Can do 100BASE-TX full duplex */
#define MII_ANAR_10_FD              BIT (6)  /* Can do 10BASE-T full duplex */

/* Management Data Clock (MDC) frequency.
 * 2.5Mz corresponds to the minimum period for MDC as defined by the standard.
 * This is independent of whether the PHY is in 10Mbit/s or 100Mbit/s mode.
 * See Std. IEEE 802.3 (Ethernet) Clause 22.2.2.11 ("management data clock"). */
#define PHY_MDC_Hz            2500000
#define PHY_RETRIES           3000
/* TODO: This might need changing for different boards. Fix? */
#define PHY_ADDR              0

#define ETH_RX_BUF_SIZE       2
#define ETH_TX_BUF_SIZE       1
#define ETH_FRAME_SIZE        0x614

/* Ethernet MAC register structure */
typedef struct bfin_emac_regs
{
   /* Control-Status Register Group */
   uint32_t opmode;           /* operating mode */
   uint32_t addrlo;           /* address low */
   uint32_t addrhi;           /* address high */
   uint32_t hashlo;           /* multicast hash table low */
   uint32_t hashhi;           /* multicast hash table high */
   uint32_t staadd;           /* station management address */
   uint32_t stadat;           /* station management data */
   uint32_t flc;              /* flow control */
   uint32_t vlan1;            /* VLAN1 tag */
   uint32_t vlan2;            /* VLAN2 tag */
   uint32_t _reserved1;
   uint32_t wkup_ctl;         /* wakeup frame control and status */
   uint32_t wkup_ffmsk[4];    /* wakeup frame n byte mask (n == 0,..,3) */
   uint32_t wkup_ffcmd;       /* wakeup frame filter commands */
   uint32_t wkup_ffoff;       /* wakeup frame filter offsets */
   uint32_t wkup_ffcrc0;      /* wakeup frame filter CRC0/1 */
   uint32_t wkup_ffcrc1;      /* wakeup frame filter CRC2/3 */
   uint32_t _reserved2[4];
   /* System Interface Register Group */
   uint32_t sysctl;           /* system control */
   uint32_t systat;           /* system status */
   /* Ethernet Frame Status Register Group */
   uint32_t rx_stat;          /* RX Current Frame Status */
   uint32_t rx_stky;          /* RX Sticky Frame Status */
   uint32_t rx_irqe;          /* RX Frame Status Interrupt Enable */
   uint32_t tx_stat;          /* TX Current Frame Status Register */
   uint32_t tx_stky;          /* TT Sticky Frame Status */
   uint32_t tx_irqe;          /* TX Frame Status Interrupt Enable */
   /* MAC Management Counter Register Group */
   uint32_t mmc_ctl;          /* Management Counters Control */
   uint32_t mmc_rirqs;        /* MMC RX Interrupt Status */
   uint32_t mmc_rirqe;        /* MMC RX Interrupt Enable */
   uint32_t mmc_tirqs;        /* MMC TX Interrupt Status */
   uint32_t mmc_tirqe;        /* MMC TX Interrupt Enable */
   uint32_t _reserved3[27];
   uint32_t mmc_rxc_ok;       /* FramesReceivedOK */
   uint32_t mmc_rxc_fcs;      /* FrameCheckSequenceErrors */
   uint32_t mmc_rxc_align;    /* AlignmentErrors */
   uint32_t mmc_rxc_octet;    /* OctetsReceivedOK */
   uint32_t mmc_rxc_dmaovf;   /* FramesLostDueToIntMACRcvError */
   uint32_t mmc_rxc_unicst;   /* UnicastFramesReceivedOK */
   uint32_t mmc_rxc_multi;    /* MulticastFramesReceivedOK */
   uint32_t mmc_rxc_broad;    /* BroadcastFramesReceivedOK */
   uint32_t mmc_rxc_lnerri;   /* InRangeLengthErrors */
   uint32_t mmc_rxc_lnerro;   /* OutOfRangeLengthField */
   uint32_t mmc_rxc_long;     /* FrameTooLongErrors */
   uint32_t mmc_rxc_macctl;   /* MACControlFramesReceived */
   uint32_t mmc_rxc_opcode;   /* UnsupportedOpcodesReceived */
   uint32_t mmc_rxc_pause;    /* PAUSEMACCtrlFramesReceived */
   uint32_t mmc_rxc_allfrm;   /* FramesReceivedAll */
   uint32_t mmc_rxc_alloct;   /* OctetsReceivedAll */
   uint32_t mmc_rxc_typed;    /* TypedFramesReceived */
   uint32_t mmc_rxc_short;    /* FramesLenLt64Received */
   uint32_t mmc_rxc_eq64;     /* FramesLenEq64Received */
   uint32_t mmc_rxc_lt128;    /* FramesLen65_127Received */
   uint32_t mmc_rxc_lt256;    /* FramesLen128_255Received */
   uint32_t mmc_rxc_lt512;    /* FramesLen256_511Received */
   uint32_t mmc_rxc_lt1024;   /* FramesLen512_1023Received */
   uint32_t mmc_rxc_ge1024;   /* FramesLen1024_MaxReceived */
   /* TODO: add all registers */
} bfin_emac_regs_t;

COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, opmode) == 0x0);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, staadd) == 0x14);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, wkup_ctl) == 0x2c);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, wkup_ffcrc1) == 0x4c);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, sysctl) == 0x60);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, rx_stat) == 0x68);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, mmc_ctl) == 0x80);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, mmc_tirqe) == 0x90);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, mmc_rxc_ok) == 0x100);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, mmc_rxc_dmaovf) == 0x110);
COMPILETIME_ASSERT (offsetof (bfin_emac_regs_t, mmc_rxc_ge1024) == 0x15c);

/* 32-bit aligned struct for ethernet data */
typedef struct ethernet_data {
   uint16_t length;     /* When TX = frame data length in bytes, not including 'length'
                         * When RX = 0x0000 padding when using RXDWA
                         */
   uint8_t data[ETH_FRAME_SIZE]; /* Hold the ethernet frame */
   uint16_t padding;    /* To make the struct 32-bit aligned */
} ethernet_data_t;

COMPILETIME_ASSERT (sizeof (ethernet_data_t) == 0x618) ;

static volatile bfin_emac_regs_t * pEth = (bfin_emac_regs_t *) EMAC_OPMODE;

/* Buffers for rx and tx DMA operations */
static ethernet_data_t rxBuffer[ETH_RX_BUF_SIZE] __attribute__((section(".dma")));
static ethernet_data_t txBuffer[ETH_TX_BUF_SIZE] __attribute__((section(".dma")));
static volatile uint32_t rxStatusWord[ETH_RX_BUF_SIZE] __attribute__((section(".dma")));
static volatile uint32_t txStatusWord[ETH_TX_BUF_SIZE] __attribute__((section(".dma")));
static bfin_dma_descriptor_t rxDMADesc[ETH_RX_BUF_SIZE * 2] __attribute__((section(".dma")));
static bfin_dma_descriptor_t txDMADesc[ETH_TX_BUF_SIZE * 2] __attribute__((section(".dma")));

/* Hold the current index of rx and tx buffers */
static uint8_t rxIdx, txIdx;


/* Internal function that writes to a PHY register  */
static void lw_emac_write_phy_reg(uint8_t phy_addr, uint8_t reg_addr, uint32_t data) {
   /* Set the flags that should be set reg_addr on PHY */
   pEth->stadat = data;

   /* Wait for our turn then initiate writing of stadat to PHY register */
   pEth->staadd = SET_PHYAD(phy_addr) | SET_REGAD(reg_addr) | STAOP | STABUSY;
   while (pEth->staadd & STABUSY) ;
}

/* Internal function that reads from; and returns a PHY register  */
static uint32_t lw_emac_read_phy_reg(uint8_t phy_addr, uint8_t reg_addr) {
   pEth->staadd = SET_PHYAD(phy_addr) | SET_REGAD(reg_addr) | STABUSY;
   while (pEth->staadd & STABUSY) ;

   return pEth->stadat;
}

/* Internal function that sets the MAC address */
static void lw_emac_set_mac_addr(uint8_t * ethAddr)
{
   pEth->addrlo =
      ethAddr[0] |
      ethAddr[1] << 8 |
      ethAddr[2] << 16 |
      ethAddr[3] << 24;
   pEth->addrhi =
      ethAddr[4] |
      ethAddr[5] << 8;
}

static uint8_t lw_emac_init_registers(uint8_t * ethAddr) {
   uint32_t clock_divisor, sysctl_mdcdiv, phy_stadat, counter;

   /* CONFIGURE MAC MII PINS */

   /* Enable PHY Clock Output */
   *(volatile uint16_t *) VR_CTL |= PHYCLKOE;

   /* Set all bits to 1 to use MII mode */
   *(uint16_t *) PORTH_FER = 0xFFFF;

   /* CONFIGURE MAC REGISTERS */

   /*
    * Set the MMC (MAC Management Counter) control register
    * RTSC = Clear all counters
    *
    * Note that counters are not enabled at this time
    */
   pEth->mmc_ctl = RSTC;

   /* Set MAC address */
   lw_emac_set_mac_addr (ethAddr);

   clock_divisor = SCLK / PHY_MDC_Hz;
   sysctl_mdcdiv = clock_divisor / 2 - 1;
   ASSERT (sysctl_mdcdiv <= 0x3f);

   rprintp ("PHY ID: %04x %04x\n",
         lw_emac_read_phy_reg (PHY_ADDR, MII_PHYIDR1),
         lw_emac_read_phy_reg (PHY_ADDR, MII_PHYIDR2)
         );

   /*
    * Set the system control register
    * SET_MDCDIV(x) = Set MDC to 2.5 MHz
    * RXDWA = Pad incoming frame with 0x0000 as to make the data-part 32-bit aligned
    * RXCKS = Enable Receive Frame TCP/UDP Checksum Computation
    */
   pEth->sysctl = SET_MDCDIV(sysctl_mdcdiv) | RXDWA ;

   /* CONFIGURE PHY */

   /*
    * Set the PHY basic control register
    * MII_BMCR_ANEG_EN = Auto negotiation on
    * MII_BMCR_ANEG_RST = Restart the auto-neg process by setting
    * Speed handled by auto negotiation
    */
   phy_stadat = MII_BMCR_ANEG_EN | MII_BMCR_ANEG_RST;
   lw_emac_write_phy_reg(PHY_ADDR, MII_BMCR, phy_stadat);

   /* Loop until link is up or time out after PHY_RETRIES */
   counter = 0;
   do {
      if (counter > PHY_RETRIES) {
         rprintp("Ethernet link is down\n");
         return -1;
      }

      task_delay (tick_from_ms (10));
      phy_stadat = lw_emac_read_phy_reg(PHY_ADDR, MII_BMSR);

      ++counter;
   } while (!(phy_stadat & MII_BMSR_LINK)) ;

   /* Check whether link partner can do full duplex or not */
   phy_stadat = lw_emac_read_phy_reg(PHY_ADDR, MII_LPAR);

   if (phy_stadat & (MII_ANAR_100_FD | MII_ANAR_10_FD) ) {
      pEth->opmode = FDMODE;
   }
   else {
      pEth->opmode = 0;
   }

   /*
    * Setup DMA MAC receive/transfer channels with XCOUNT 0 (which we use
    * together with description based DMA) and XMODIFY 4 (bytes per transfer)
    */
   bfin_dma_channel_init(DMA_CHANNEL_EMAC_RX, 0, 4);
   bfin_dma_channel_init(DMA_CHANNEL_EMAC_TX, 0, 4);

   return 0;
}

int bfin_EMAC_init (uint8_t *ethAddr)
{
   rxIdx = txIdx = 0;

   if (lw_emac_init_registers(ethAddr) != 0) {
      return -1;
   }

   // Reset status words
   memset ((uint8_t *)rxStatusWord, 0, sizeof(rxStatusWord));
   memset ((uint8_t *)txStatusWord, 0, sizeof(txStatusWord));

   txDMADesc[0].next = &txDMADesc[1];
   txDMADesc[0].start_addr = &txBuffer[0];
   txDMADesc[0].config = DMA_CONFIG_DMA_EN |
                         DMA_CONFIG_WDSIZE(DMA_WDSIZE_32BIT) |
                         DMA_CONFIG_NDSIZE(5) |
                         DMA_CONFIG_FLOW(DMA_FLOW_DESCRIPTOR_LIST_LARGE);

   txDMADesc[1].next = &txDMADesc[0];
   txDMADesc[1].start_addr = &txStatusWord[0];
   txDMADesc[1].config = DMA_CONFIG_DMA_EN |
                         DMA_CONFIG_WNR |
                         DMA_CONFIG_WDSIZE(DMA_WDSIZE_32BIT) |
                         DMA_CONFIG_NDSIZE(0) |
                         DMA_CONFIG_FLOW(DMA_FLOW_STOP);

   rxDMADesc[0].next = &rxDMADesc[1];
   rxDMADesc[0].start_addr = &rxBuffer[0];
   rxDMADesc[0].config = DMA_CONFIG_DMA_EN |
                         DMA_CONFIG_WNR |
                         DMA_CONFIG_WDSIZE(DMA_WDSIZE_32BIT) |
                         DMA_CONFIG_NDSIZE(5) |
                         DMA_CONFIG_FLOW(DMA_FLOW_DESCRIPTOR_LIST_LARGE);

   rxDMADesc[1].next = &rxDMADesc[2];
   rxDMADesc[1].start_addr = &rxStatusWord[0];
   rxDMADesc[1].config = DMA_CONFIG_DMA_EN |
                         DMA_CONFIG_WNR |
                         DMA_CONFIG_WDSIZE(DMA_WDSIZE_32BIT) |
                         DMA_CONFIG_NDSIZE(5) |
                         DMA_CONFIG_FLOW(DMA_FLOW_DESCRIPTOR_LIST_LARGE);

   rxDMADesc[2].next = &rxDMADesc[3];
   rxDMADesc[2].start_addr = &rxBuffer[1];
   rxDMADesc[2].config = DMA_CONFIG_DMA_EN |
                         DMA_CONFIG_WNR |
                         DMA_CONFIG_WDSIZE(DMA_WDSIZE_32BIT) |
                         DMA_CONFIG_NDSIZE(5) |
                         DMA_CONFIG_FLOW(DMA_FLOW_DESCRIPTOR_LIST_LARGE);

   rxDMADesc[3].next = &rxDMADesc[0];
   rxDMADesc[3].start_addr = &rxStatusWord[1];
   rxDMADesc[3].config = DMA_CONFIG_DMA_EN |
                         DMA_CONFIG_WNR |
                         DMA_CONFIG_WDSIZE(DMA_WDSIZE_32BIT) |
                         DMA_CONFIG_NDSIZE(5) |
                         DMA_CONFIG_FLOW(DMA_FLOW_DESCRIPTOR_LIST_LARGE);

   bfin_dma_channel_enable(DMA_CHANNEL_EMAC_RX, &rxDMADesc[0]);

   /* Enable Receiving, Automatic Pad Stripping and receiving of frames with length < 64 butes */
   pEth->opmode |= ASTP | PSF | RE;

   return 0;
}
int bfin_EMAC_send (void *packet, int length)
{
   UASSERT(length > 0, EARG);
   UASSERT(length < ETH_FRAME_SIZE, EARG);

   /* TODO: Check DMA Error in IRQ status */

   while (bfin_dma_channel_interrupt_is_active (DMA_CHANNEL_EMAC_TX));

   txBuffer[txIdx].length = length;
   memcpy(txBuffer[txIdx].data, packet, length);

   bfin_dma_channel_enable(DMA_CHANNEL_EMAC_TX, txDMADesc);
   pEth->opmode |= TE;

   while ((txStatusWord[txIdx] & TX_COMP) == 0);

   ASSERT(txStatusWord[txIdx] & TX_OK);

   txStatusWord[txIdx] = 0;

   ++txIdx;

    if (txIdx == ETH_TX_BUF_SIZE) {
       txIdx = 0;
    }

   return 0;
}

int bfin_EMAC_recv (uint8_t * packet, size_t size)
{
   uint32_t length;
   uint32_t status = rxStatusWord[rxIdx];

   /* Check if rx frame is completed */
   if ((status & RX_COMP) == 0) {
      return -1;
   }
   else if ((status & RX_OK) == 0) {
      ASSERT(0);
      /* TODO: Handle error */
      return -1;
   }
   else if (status & RX_DMAO) {
      ASSERT(0);
      /* TODO: Handle overrun */
      return -1;
   }

   length = status & RX_FRLEN;

   if (size < length) {
      length = size;
   }

   memcpy(packet, rxBuffer[rxIdx].data, length);

   bfin_dma_channel_interrupt_clear (DMA_CHANNEL_EMAC_RX);
   rxStatusWord[rxIdx] = 0;

   ++rxIdx;

   if (rxIdx == ETH_RX_BUF_SIZE) {
      rxIdx = 0;
   }

   return length;
}

