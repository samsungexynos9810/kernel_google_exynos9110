/* drivers/video/decon_display/regs-mipidsim.h
 *
 * Register definition file for Samsung MIPI-DSIM driver
 *
 * Copyright (c) 2011 Samsung Electronics
 * Haowei li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _REGS_MIPIDSIM_H
#define _REGS_MIPIDSIM_H

#define S5P_DSIM_STATUS		(0x4)	/* Status register */
#define DSIM_STOP_STATE_DAT(x)	(((x) & 0xf) << 0)
#define DSIM_STOP_STATE_CLK	(1 << 8)
#define DSIM_TX_READY_HS_CLK	(1 << 10)

#define S5P_DSIM_SWRST		(0xc)	/* Software reset register */
#define DSIM_FUNCRST		(1 << 16)
#define DSIM_SWRST		(1 << 0)

#define S5P_DSIM_CLKCTRL	(0x10)	/* Clock control register */
#define DSIM_LANE_ESC_CLKEN_SHIFT	(19)
#define DSIM_BYTE_CLKEN_SHIFT		(24)
#define DSIM_BYTE_CLK_SRC_SHIFT		(25)
#define DSIM_PLL_BYPASS_SHIFT		(27)
#define DSIM_ESC_CLKEN_SHIFT		(28)
#define DSIM_TX_REQUEST_HSCLK_SHIFT	(31)
#define DSIM_LANE_ESC_CLKEN(x)		(((x) & 0x1f) << \
						DSIM_LANE_ESC_CLKEN_SHIFT)
#define DSIM_BYTE_CLK_ENABLE		(1 << DSIM_BYTE_CLKEN_SHIFT)
#define DSIM_BYTE_CLK_DISABLE		(0 << DSIM_BYTE_CLKEN_SHIFT)
#define DSIM_PLL_BYPASS_EXTERNAL	(1 << DSIM_PLL_BYPASS_SHIFT)
#define DSIM_ESC_CLKEN_ENABLE		(1 << DSIM_ESC_CLKEN_SHIFT)
#define DSIM_ESC_CLKEN_DISABLE		(0 << DSIM_ESC_CLKEN_SHIFT)

#define S5P_DSIM_TIMEOUT	(0x14)	/* Time out register */
#define DSIM_LPDR_TOUT_SHIFT	(0)
#define DSIM_BTA_TOUT_SHIFT	(16)

#define S5P_DSIM_CONFIG		(0x18)	/* Configuration register */
#define DSIM_NUM_OF_DATALANE_SHIFT	(5)
#define DSIM_HSA_MODE_SHIFT		(20)
#define DSIM_HBP_MODE_SHIFT		(21)
#define DSIM_HFP_MODE_SHIFT		(22)
#define DSIM_HSE_MODE_SHIFT		(23)
#define DSIM_AUTO_MODE_SHIFT		(24)
#define DSIM_LANE_ENx(x)		(((x) & 0x1f) << 0)
#define DSIM_NUM_OF_DATA_LANE(x)	((x) << DSIM_NUM_OF_DATALANE_SHIFT)

#define S5P_DSIM_ESCMODE	(0x1c)	/* Escape mode register */
#define DSIM_TX_LPDT_SHIFT		(6)
#define DSIM_CMD_LPDT_SHIFT		(7)
#define DSIM_TX_LPDT_LP			(1 << DSIM_TX_LPDT_SHIFT)
#define DSIM_CMD_LPDT_LP		(1 << DSIM_CMD_LPDT_SHIFT)
#define DSIM_STOP_STATE_CNT_SHIFT	(21)
#define DSIM_FORCE_STOP_STATE_SHIFT	(20)

/* Display image resolution register */
#define S5P_DSIM_MDRESOL	(0x20)
#define DSIM_MAIN_STAND_BY		(1 << 31)
#define DSIM_MAIN_VRESOL(x)		(((x) & 0x7ff) << 16)
#define DSIM_MAIN_HRESOL(x)		(((x) & 0X7ff) << 0)

#define S5P_DSIM_MVPORCH	(0x24)	/* Main display Vporch register */
#define DSIM_CMD_ALLOW_SHIFT		(28)
#define DSIM_STABLE_VFP_SHIFT		(16)
#define DSIM_MAIN_VBP_SHIFT		(0)
#define DSIM_CMD_ALLOW_MASK		(0xf << DSIM_CMD_ALLOW_SHIFT)
#define DSIM_STABLE_VFP_MASK		(0x7ff << DSIM_STABLE_VFP_SHIFT)
#define DSIM_MAIN_VBP_MASK		(0x7ff << DSIM_MAIN_VBP_SHIFT)

#define S5P_DSIM_MHPORCH	(0x28)	/* Main display Hporch register */
#define DSIM_MAIN_HFP_SHIFT		(16)
#define DSIM_MAIN_HBP_SHIFT		(0)
#define DSIM_MAIN_HFP_MASK		((0xffff) << DSIM_MAIN_HFP_SHIFT)
#define DSIM_MAIN_HBP_MASK		((0xffff) << DSIM_MAIN_HBP_SHIFT)

#define S5P_DSIM_MSYNC		(0x2C)	/* Main display sync area register */
#define DSIM_MAIN_VSA_SHIFT		(22)
#define DSIM_MAIN_HSA_SHIFT		(0)
#define DSIM_MAIN_VSA_MASK		((0x3ff) << DSIM_MAIN_VSA_SHIFT)
#define DSIM_MAIN_HSA_MASK		((0xffff) << DSIM_MAIN_HSA_SHIFT)

#define S5P_DSIM_INTSRC		(0x34)	/* Interrupt source register */
#define INTSRC_FRAME_DONE		(1 << 24)
#define INTSRC_PLL_STABLE		(1 << 31)

#define INTSRC_SFR_FIFO_EMPTY		(1 << 29)
#define SFR_HEADER_EMPTY		(1 << 22)

#define S5P_DSIM_INTMSK		(0x38)	/* Interrupt mask register */
#define INTMSK_FRAME_DONE		(1 << 24)

#define S5P_DSIM_PKTHDR		(0x3c)	/* Packet Header FIFO register */
#define S5P_DSIM_PAYLOAD	(0x40)	/* Payload FIFO register */
#define S5P_DSIM_RXFIFO		(0x44)	/* Read FIFO register */

#define S5P_DSIM_FIFOCTRL	(0x4c)	/* FIFO status and control register */

#define S5P_DSIM_STEREO_3D	(0x54)	/* Stereo scope 3D register */
#define S5P_DSIM_PROPRIETARY_3D	(0x58)	/* Proprietary 3D register */

#define S5P_DSIM_MIC		(0x5c)	/* MIC control register */

#define S5P_DSIM_PRO_ON_MIC_OFF_H	(0x60)
#define S5P_DSIM_PRO_OFF_MIC_ON_H	(0x64)
#define S5P_DSIM_PRO_ON_MIC_ON_H	(0x68)

#define S5P_DSIM_PRO_ON_MIC_OFF_HFP	(0x6c)
#define S5P_DSIM_PRO_OFF_MIC_ON_HFP	(0x70)
#define S5P_DSIM_PRO_ON_MIC_ON_HFP	(0x74)

#define S5P_DSIM_MULTI_PKT		(0x78)

#define S5P_DSIM_PLLCTRL	(0x94)	/* PLL control register */
#define DSIM_PLL_EN_SHIFT		(23)
#define DSIM_FREQ_BAND_SHIFT		(24)

#define S5P_DSIM_PLLTMR		(0xa0)	/* PLL timer register */

/* D-PHY Master & Slave Analog block characteristics control register (B_DPHYCTL) */
#define S5P_DSIM_PHYCTRL	(0xa4)
/* D-PHY Master global operating timing register */
#define S5P_DSIM_PHYTIMING	(0xb4)
#define S5P_DSIM_PHYTIMING1	(0xb8)
#define S5P_DSIM_PHYTIMING2	(0xbC)

/* Version register */
#define S5P_DSIM_VERSION	(0x1000)  /* Specifies the DSIM version information */

#endif /* _REGS_MIPIDSIM_H */
