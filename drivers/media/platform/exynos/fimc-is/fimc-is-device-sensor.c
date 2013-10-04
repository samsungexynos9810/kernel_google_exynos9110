/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"

#include "fimc-is-device-sensor.h"

/* PMU for FIMC-IS*/
#define MIPICSI0_REG_BASE	(S5P_VA_MIPICSI0)   /* phy : 0x13c2_0000(5410) 0x1212_0000(5430)*/
#define MIPICSI1_REG_BASE	(S5P_VA_MIPICSI1)   /* phy : 0x13c3_0000(5410) 0x1213_0000(5430)*/
#define MIPICSI2_REG_BASE	(S5P_VA_MIPICSI2)   /* phy : 0x13d1_0000(5410) 0x141d_0000(5430)*/

/*MIPI*/
#if defined(CONFIG_SOC_EXYNOS5250) || defined(CONFIG_SOC_EXYNOS5410)
/* CSIS global control */
#define S5PCSIS_CTRL					(0x00)
#define S5PCSIS_CTRL_DPDN_DEFAULT			(0 << 31)
#define S5PCSIS_CTRL_DPDN_SWAP				(1 << 31)
#define S5PCSIS_CTRL_ALIGN_32BIT			(1 << 20)
#define S5PCSIS_CTRL_UPDATE_SHADOW(x)			(x << 16)
#define S5PCSIS_CTRL_WCLK_EXTCLK			(1 << 8)
#define S5PCSIS_CTRL_RESET				(1 << 4)
#define S5PCSIS_CTRL_ENABLE				(1 << 0)

/* D-PHY control */
#define S5PCSIS_DPHYCTRL				(0x04)
#define S5PCSIS_DPHYCTRL_HSS_MASK			(soc_is_exynos5250() ? \
							0x1f << 27 : \
							0xff << 24)
#define S5PCSIS_DPHYCTRL_ENABLE				(soc_is_exynos5250() ? \
							0x7 << 0 : \
							0x1f << 0)

#define S5PCSIS_CONFIG					(0x08)
#define S5PCSIS_CFG_FMT_YCBCR422_8BIT			(0x1e << 2)
#define S5PCSIS_CFG_FMT_RAW8				(0x2a << 2)
#define S5PCSIS_CFG_FMT_RAW10				(0x2b << 2)
#define S5PCSIS_CFG_FMT_RAW12				(0x2c << 2)
/* User defined formats, x = 1...4 */
#define S5PCSIS_CFG_FMT_USER(x)				((0x30 + x - 1) << 2)
#define S5PCSIS_CFG_FMT_MASK				(0x3f << 2)
#define S5PCSIS_CFG_NR_LANE_MASK			(3)

/* Interrupt mask. */
#define S5PCSIS_INTMSK					(0x10)
#define S5PCSIS_INTMSK_EN_ALL				(0xfc00103f)
#define S5PCSIS_INTSRC					(0x14)

/* Pixel resolution */
#define S5PCSIS_RESOL					(0x2c)
#define CSIS_MAX_PIX_WIDTH				(0xffff)
#define CSIS_MAX_PIX_HEIGHT				(0xffff)

#else
/* CSIS global control */
#define S5PCSIS_CTRL					(0x00)
#define S5PCSIS_CTRL_DPDN_SWAP_CLOCK_DEFAULT		(0 << 31)
#define S5PCSIS_CTRL_DPDN_SWAP_CLOCK			(1 << 31)
#define S5PCSIS_CTRL_DPDN_SWAP_DATA_DEFAULT		(0 << 30)
#define S5PCSIS_CTRL_DPDN_SWAP_DATA			(1 << 30)
#define S5PCSIS_CTRL_INTERLEAVE_MODE(x)			((x & 0x3) << 22)
#define S5PCSIS_CTRL_ALIGN_32BIT			(1 << 20)
#define S5PCSIS_CTRL_UPDATE_SHADOW(x)			(x << 16)
#define S5PCSIS_CTRL_WCLK_EXTCLK			(1 << 8)
#define S5PCSIS_CTRL_RESET				(1 << 4)
#define S5PCSIS_CTRL_NUMOFDATALANE(x)			(x << 2)
#define S5PCSIS_CTRL_ENABLE				(1 << 0)

/* D-PHY control */
#define S5PCSIS_DPHYCTRL				(0x04)
#define S5PCSIS_DPHYCTRL_HSS_MASK			(0xff << 24)
#define S5PCSIS_DPHYCTRL_CLKSETTLEMASK			(0x3 << 22)
#define S5PCSIS_DPHYCTRL_ENABLE				(0x1f << 0)

/* Configuration */
#define S5PCSIS_CONFIG					(0x08)
#define S5PCSIS_CFG_LINE_INTERVAL(x)			(x << 26)
#define S5PCSIS_CFG_START_INTERVAL(x)			(x << 20)
#define S5PCSIS_CFG_END_INTERVAL(x)			(x << 8)
#define S5PCSIS_CFG_FMT_YCBCR422_8BIT			(0x1e << 2)
#define S5PCSIS_CFG_FMT_RAW8				(0x2a << 2)
#define S5PCSIS_CFG_FMT_RAW10				(0x2b << 2)
#define S5PCSIS_CFG_FMT_RAW12				(0x2c << 2)
/* User defined formats, x = 1...4 */
#define S5PCSIS_CFG_FMT_USER(x)				((0x30 + x - 1) << 2)
#define S5PCSIS_CFG_FMT_MASK				(0x3f << 2)
#define S5PCSIS_CFG_VIRTUAL_CH(x)			(x << 0)

/* Interrupt mask. */
#define S5PCSIS_INTMSK					(0x10)
#define S5PCSIS_INTMSK_EN_ALL				(0xf1101117)
#define S5PCSIS_INTMSK_EVEN_BEFORE			(1 << 31)
#define S5PCSIS_INTMSK_EVEN_AFTER			(1 << 30)
#define S5PCSIS_INTMSK_ODD_BEFORE			(1 << 29)
#define S5PCSIS_INTMSK_ODD_AFTER			(1 << 28)
#define S5PCSIS_INTMSK_FRAME_START_CH3			(1 << 27)
#define S5PCSIS_INTMSK_FRAME_START_CH2			(1 << 26)
#define S5PCSIS_INTMSK_FRAME_START_CH1			(1 << 25)
#define S5PCSIS_INTMSK_FRAME_START_CH0			(1 << 24)
#define S5PCSIS_INTMSK_FRAME_END_CH3			(1 << 23)
#define S5PCSIS_INTMSK_FRAME_END_CH2			(1 << 22)
#define S5PCSIS_INTMSK_FRAME_END_CH1			(1 << 21)
#define S5PCSIS_INTMSK_FRAME_END_CH0			(1 << 20)
#define S5PCSIS_INTMSK_ERR_SOT_HS			(1 << 16)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH3			(1 << 15)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH2			(1 << 14)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH1			(1 << 13)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH0			(1 << 12)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH3			(1 << 11)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH2			(1 << 10)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH1			(1 << 9)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH0			(1 << 8)
#define S5PCSIS_INTMSK_ERR_OVER_CH3			(1 << 7)
#define S5PCSIS_INTMSK_ERR_OVER_CH2			(1 << 6)
#define S5PCSIS_INTMSK_ERR_OVER_CH1			(1 << 5)
#define S5PCSIS_INTMSK_ERR_OVER_CH0			(1 << 4)
#define S5PCSIS_INTMSK_ERR_ECC				(1 << 2)
#define S5PCSIS_INTMSK_ERR_CRC				(1 << 1)
#define S5PCSIS_INTMSK_ERR_ID				(1 << 0)

/* Interrupt source */
#define S5PCSIS_INTSRC					(0x14)
#define S5PCSIS_INTSRC_EVEN_BEFORE			(1 << 31)
#define S5PCSIS_INTSRC_EVEN_AFTER			(1 << 30)
#define S5PCSIS_INTSRC_EVEN				(0x3 << 30)
#define S5PCSIS_INTSRC_ODD_BEFORE			(1 << 29)
#define S5PCSIS_INTSRC_ODD_AFTER			(1 << 28)
#define S5PCSIS_INTSRC_ODD				(0x3 << 28)
#define S5PCSIS_INTSRC_FRAME_START			(0xf << 24)
#define S5PCSIS_INTSRC_FRAME_END			(0xf << 20)
#define S5PCSIS_INTSRC_ERR_SOT_HS			(0xf << 16)
#define S5PCSIS_INTSRC_ERR_LOST_FS			(0xf << 12)
#define S5PCSIS_INTSRC_ERR_LOST_FE			(0xf << 8)
#define S5PCSIS_INTSRC_ERR_OVER				(0xf << 4)
#define S5PCSIS_INTSRC_ERR_ECC				(1 << 2)
#define S5PCSIS_INTSRC_ERR_CRC				(1 << 1)
#define S5PCSIS_INTSRC_ERR_ID				(1 << 0)
#define S5PCSIS_INTSRC_ERRORS				(0xf1111117)

/* Pixel resolution */
#define S5PCSIS_RESOL					(0x2c)
#define S5PCSIS_RESO_MAX_PIX_WIDTH			(0xffff)
#define S5PCSIS_RESO_MAX_PIX_HEIGHT			(0xffff)
#endif

#define FIMC_IS_SETTLE(w, h, f, s)	{	\
	.width		= w,			\
	.height		= h,			\
	.framerate	= f,			\
	.settle		= s,			\
}

static struct fimc_is_settle settle_3l2[] = {
	/* 4144x3106@30fps */
	FIMC_IS_SETTLE(4144, 3106, 30, 23),
	/* 4144x2332@30fps */
	FIMC_IS_SETTLE(4144, 2332, 30, 23),
	/* 1024x584@120fps */
	FIMC_IS_SETTLE(1024, 584, 120, 17),
	/* 2072x1166@60fps */
	FIMC_IS_SETTLE(2072, 1162, 60, 9),
	/* 2072x1166@24fps */
	FIMC_IS_SETTLE(2072, 1166, 24, 5),
	/* 2072x1154@24fps */
	FIMC_IS_SETTLE(2072, 1154, 24, 5),
};

static struct fimc_is_settle settle_imx135[] = {
	/* 1936x1090@24fps */
	FIMC_IS_SETTLE(1936, 1090, 24, 7),
	/* 1936x1090@30fps */
	FIMC_IS_SETTLE(1936, 1090, 30, 5),
	/* 1936x1450@24fps */
	FIMC_IS_SETTLE(1936, 1450, 24, 9),
	/* 2064x1162@24fps */
	FIMC_IS_SETTLE(2064, 1162, 24, 8),
	/* 1936x1090@60fps */
	FIMC_IS_SETTLE(1936, 1090, 60, 9),
	/* 816x460@60fps */
	FIMC_IS_SETTLE(816, 460, 60, 9),
	/* 736x490@120fps */
	FIMC_IS_SETTLE(736, 490, 120, 11),
	/* 1296x730@120fps */
	FIMC_IS_SETTLE(1296, 730, 120, 11),
	/* 4112x2314@24ps */
	FIMC_IS_SETTLE(4112, 2314, 24, 14),
	/* 816x460@120fps */
	FIMC_IS_SETTLE(816, 460, 120, 18),
	/* 4144x2332@24fps */
	FIMC_IS_SETTLE(4144, 2332, 24, 19),
	/* 4144x2332@30fps */
	FIMC_IS_SETTLE(4144, 2332, 30, 18),
	/* 4112x3082@24fps */
	FIMC_IS_SETTLE(4112, 3082, 24, 19),
	/* 4144x3106@24fps */
	FIMC_IS_SETTLE(4144, 3106, 24, 23),
	/* 4144x3106@30fps */
	FIMC_IS_SETTLE(4144, 3106, 30, 23),
	/* 2048x1152@60fps */
	FIMC_IS_SETTLE(2048, 1152, 60, 9),
	/* 1024x576@120fps */
	FIMC_IS_SETTLE(1024, 576, 120, 9),
	/* 1024x576@60fps */
	FIMC_IS_SETTLE(1024, 576, 60, 9),
	/* 1936x1090@15fps */
	FIMC_IS_SETTLE(1936, 1090, 15, 7),
};

/* TODO: This code will be moved machine part */
#if !defined(CONFIG_SOC_EXYNOS5430)
static struct fimc_is_settle settle_imx134[] = {
	/* 3280x2458@30fps */
	FIMC_IS_SETTLE(3280, 2458, 30, 15),
	/* 3280x2458@24fps */
	FIMC_IS_SETTLE(3280, 2458, 24, 12),
	/* 1936x1450@24fps */
	FIMC_IS_SETTLE(1936, 1450, 24, 12),
	/* 3280x1846@30fps */
	FIMC_IS_SETTLE(3280, 1846, 30, 11),
	/* 3280x1846@60fps */
	FIMC_IS_SETTLE(3280, 1846, 24, 9),
	/* 1936x1090@60fps */
	FIMC_IS_SETTLE(1936, 1090, 24, 9),
	/* 816x460@120fps */
	FIMC_IS_SETTLE(816, 460, 120, 7),
};
#endif

static struct fimc_is_settle settle_6b2[] = {
	/* 1456x1090@24fps */
	FIMC_IS_SETTLE(1456, 1090, 24, 13),
	/* 1936x1090@24fps */
	FIMC_IS_SETTLE(1936, 1090, 24, 13),
	/* 1456x1090@30fps */
	FIMC_IS_SETTLE(1456, 1090, 30, 16),
	/* 1936x1090@30fps */
	FIMC_IS_SETTLE(1936, 1090, 30, 16),
	/* 1456x1090@24fps */
	FIMC_IS_SETTLE(1456, 1090, 24, 13),
	/* 1936x1090@24fps */
	FIMC_IS_SETTLE(1936, 1090, 24, 13),
	/* 1456x1090@30fps */
	FIMC_IS_SETTLE(1456, 1090, 30, 16),
	/* 1936x1090@30fps */
	FIMC_IS_SETTLE(1936, 1090, 30, 16),
};

static u32 get_hsync_settle(struct fimc_is_settle *settle_table,
	u32 settles, u32 width, u32 height, u32 framerate)
{
	u32 settle;
	u32 max_settle;
	u32 proximity_framerate, proximity_settle;
	u32 i;

	settle = 0;
	max_settle = 0;
	proximity_framerate = 0;
	proximity_settle = 0;

	for (i = 0; i < settles; i++) {
		if ((settle_table[i].width == width) &&
		    (settle_table[i].height == height) &&
		    (settle_table[i].framerate == framerate)) {
			settle = settle_table[i].settle;
			break;
		}

		if ((settle_table[i].width == width) &&
		    (settle_table[i].height == height) &&
		    (settle_table[i].framerate > proximity_framerate)) {
			proximity_settle = settle_table[i].settle;
			proximity_framerate = settle_table[i].framerate;
		}

		if (settle_table[i].settle > max_settle)
			max_settle = settle_table[i].settle;
	}

	if (!settle) {
		if (proximity_settle) {
			settle = proximity_settle;
		} else {
			/*
			 * return a max settle time value in above table
			 * as a default depending on the channel
			 */
			settle = max_settle;

			warn("could not find proper settle time: %dx%d@%dfps",
				width, height, framerate);
		}
	}

	return settle;
}

static void s5pcsis_enable_interrupts(unsigned long mipi_reg_base, bool on)
{
	u32 val = is_readl(mipi_reg_base + S5PCSIS_INTMSK);

	val = on ? val | S5PCSIS_INTMSK_EN_ALL :
		   val & ~S5PCSIS_INTMSK_EN_ALL;
	is_writel(val, mipi_reg_base + S5PCSIS_INTMSK);
}

static void s5pcsis_reset(unsigned long mipi_reg_base)
{
	u32 val = is_readl(mipi_reg_base + S5PCSIS_CTRL);

	is_writel(val | S5PCSIS_CTRL_RESET, mipi_reg_base + S5PCSIS_CTRL);
	udelay(10);
}

static void s5pcsis_system_enable(unsigned long mipi_reg_base, int on)
{
	u32 val;

	val = is_readl(mipi_reg_base + S5PCSIS_CTRL);

#if defined(CONFIG_SOC_EXYNOS5420) || defined(CONFIG_SOC_EXYNOS5430)
	val |= S5PCSIS_CTRL_WCLK_EXTCLK;
	val |= S5PCSIS_CTRL_NUMOFDATALANE(0x3);
#endif

	if (on) {
		val |= S5PCSIS_CTRL_ENABLE;
		val |= S5PCSIS_CTRL_WCLK_EXTCLK;
	} else
		val &= ~S5PCSIS_CTRL_ENABLE;
	is_writel(val, mipi_reg_base + S5PCSIS_CTRL);

	val = is_readl(mipi_reg_base + S5PCSIS_DPHYCTRL);
	if (on)
		val |= S5PCSIS_DPHYCTRL_ENABLE;
	else
		val &= ~S5PCSIS_DPHYCTRL_ENABLE;
	is_writel(val, mipi_reg_base + S5PCSIS_DPHYCTRL);
}

/* Called with the state.lock mutex held */
static void __s5pcsis_set_format(unsigned long mipi_reg_base,
				struct fimc_is_frame_info *f_frame)
{
	u32 val;

	/* Color format */
	val = is_readl(mipi_reg_base + S5PCSIS_CONFIG);
	val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_RAW10;
#if defined(CONFIG_SOC_EXYNOS5420) || defined(CONFIG_SOC_EXYNOS5430)
	val |= S5PCSIS_CFG_START_INTERVAL(1);
#endif
	is_writel(val, mipi_reg_base + S5PCSIS_CONFIG);

	/* Pixel resolution */
	val = (f_frame->o_width << 16) | f_frame->o_height;
	is_writel(val, mipi_reg_base + S5PCSIS_RESOL);
}

static void s5pcsis_set_hsync_settle(unsigned long mipi_reg_base, int settle)
{
	u32 val = is_readl(mipi_reg_base + S5PCSIS_DPHYCTRL);

	if (soc_is_exynos5250())
		val = (val & ~S5PCSIS_DPHYCTRL_HSS_MASK) | (0x6 << 28);
	else
		val = (val & ~S5PCSIS_DPHYCTRL_HSS_MASK) | (settle << 24);

	is_writel(val, mipi_reg_base + S5PCSIS_DPHYCTRL);
}

static void s5pcsis_set_params(unsigned long mipi_reg_base,
				struct fimc_is_frame_info *f_frame)
{
	u32 val;

#if defined(CONFIG_SOC_EXYNOS5410)
	val = is_readl(mipi_reg_base + S5PCSIS_CONFIG);
	if (soc_is_exynos5250())
		val = (val & ~S5PCSIS_CFG_NR_LANE_MASK) | (2 - 1);
	else
		val = (val & ~S5PCSIS_CFG_NR_LANE_MASK) | (4 - 1);
	is_writel(val, mipi_reg_base + S5PCSIS_CONFIG);
#endif

	__s5pcsis_set_format(mipi_reg_base, f_frame);

	val = is_readl(mipi_reg_base + S5PCSIS_CTRL);
	val &= ~S5PCSIS_CTRL_ALIGN_32BIT;

	/* Not using external clock. */
	val &= ~S5PCSIS_CTRL_WCLK_EXTCLK;

	is_writel(val, mipi_reg_base + S5PCSIS_CTRL);

	/* Update the shadow register. */
	val = is_readl(mipi_reg_base + S5PCSIS_CTRL);
	is_writel(val | S5PCSIS_CTRL_UPDATE_SHADOW(1),
			mipi_reg_base + S5PCSIS_CTRL);
}

int enable_mipi(void)
{
	void __iomem *addr;
	u32 cfg;

	addr = S5P_MIPI_DPHY_CONTROL(0);
	cfg = __raw_readl(addr);
	if (!soc_is_exynos5430())
		cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	cfg |= S5P_MIPI_DPHY_ENABLE;
	__raw_writel(cfg, addr);

	addr = S5P_MIPI_DPHY_CONTROL(1);
	cfg = __raw_readl(addr);
	if (!soc_is_exynos5430())
		cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	cfg |= S5P_MIPI_DPHY_ENABLE;
	__raw_writel(cfg, addr);

	addr = S5P_MIPI_DPHY_CONTROL(2);
	cfg = __raw_readl(addr);
	if (!soc_is_exynos5430())
		cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	cfg |= S5P_MIPI_DPHY_ENABLE;
	__raw_writel(cfg, addr);

	return 0;
}

static int start_mipi_csi(u32 channel,
	struct fimc_is_settle *settle_table,
	u32 settles,
	struct fimc_is_frame_info *f_frame,
	int framerate)
{
	u32 settle;
	u32 width, height;
	unsigned long base_reg = (unsigned long)MIPICSI0_REG_BASE;

	width = f_frame->width;
	height = f_frame->height;

	if (channel == CSI_ID_A)
		base_reg = (unsigned long)MIPICSI0_REG_BASE;
	else if (channel == CSI_ID_B)
		base_reg = (unsigned long)MIPICSI1_REG_BASE;
	else if (channel == CSI_ID_C)
		base_reg = (unsigned long)MIPICSI2_REG_BASE;

	/* HACK: This should be removed. */
	if (is_readl(base_reg + S5PCSIS_DPHYCTRL) & 0x1f)
		goto exit;

	s5pcsis_reset(base_reg);

	settle = get_hsync_settle(settle_table, settles, width, height, framerate);
	pr_info("[SEN:D:%d] settle(%dx%d@%d) = %d\n", channel, width, height, framerate, settle);

	s5pcsis_set_hsync_settle(base_reg, settle);
	s5pcsis_set_params(base_reg, f_frame);
	s5pcsis_system_enable(base_reg, true);
	s5pcsis_enable_interrupts(base_reg, true);

exit:
	return 0;
}

int stop_mipi_csi(int channel)
{
	unsigned long base_reg = (unsigned long)MIPICSI0_REG_BASE;

	if (channel == CSI_ID_A)
		base_reg = (unsigned long)MIPICSI0_REG_BASE;
	else if (channel == CSI_ID_B)
		base_reg = (unsigned long)MIPICSI1_REG_BASE;
	else if (channel == CSI_ID_C)
		base_reg = (unsigned long)MIPICSI2_REG_BASE;

	s5pcsis_enable_interrupts(base_reg, false);
	s5pcsis_system_enable(base_reg, false);

	return 0;
}

int fimc_is_sensor_clock_on(struct fimc_is_device_sensor *device, u32 source)
{
	int ret = 0;
	struct fimc_is_core *core;

	BUG_ON(!device);

	if (test_and_set_bit(FIMC_IS_SENSOR_CLOCK_ON, &device->state)) {
		merr("%s : already clk on", device, __func__);
		goto p_err;
	}

	if (!device->vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!device->vctx->video) {
		merr("video is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	core = device->vctx->video->core;
	if (!core->pdata) {
		merr("pdata is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!core->pdata->sensor_clock_on) {
		merr("sensor_clock_on is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = core->pdata->sensor_clock_on(core->pdev, source);
	if (ret)
		merr("sensor_clock_on is fail(%d)", device, ret);

p_err:
	return ret;
}

int fimc_is_sensor_clock_off(struct fimc_is_device_sensor *device, u32 source)
{
	int ret = 0;
	struct fimc_is_core *core;

	BUG_ON(!device);

	if (!test_and_clear_bit(FIMC_IS_SENSOR_CLOCK_ON, &device->state)) {
		merr("%s : already clk off", device, __func__);
		goto p_err;
	}

	if (!device->vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!device->vctx->video) {
		merr("video is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	core = device->vctx->video->core;
	if (!core->pdata) {
		merr("pdata is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!core->pdata->sensor_clock_off) {
		merr("sensor_clock_off is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = core->pdata->sensor_clock_off(core->pdev, source);
	if (ret)
		merr("sensor_clock_off is fail(%d)", device, ret);

p_err:
	return ret;
}

#ifdef ENABLE_DTP
static void fimc_is_sensor_dtp(unsigned long data)
{
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_device_sensor *device;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_queue *queue;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	unsigned long flags;
	u32 i;

	BUG_ON(!data);

	err("DTP is detected, forcely reset");

	device = (struct fimc_is_device_sensor *)data;
	vctx = device->vctx;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	ischain = device->ischain;
	if (!ischain) {
		err("ischain is NULL");
		return;
	}

	queue = GET_DST_QUEUE(vctx);
	framemgr = &queue->framemgr;
	if ((framemgr->frame_cnt == 0) || (framemgr->frame_cnt >= FRAMEMGR_MAX_REQUEST)) {
		err("frame count of framemgr is invalid(%d)", framemgr->frame_cnt);
		return;
	}

	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);
	set_bit(FIMC_IS_GROUP_FORCE_STOP, &ischain->group_3ax.state);
	set_bit(FIMC_IS_GROUP_FORCE_STOP, &ischain->group_isp.state);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &ischain->group_3ax.state))
		up(&ischain->group_3ax.smp_trigger);

	framemgr_e_barrier_irqs(framemgr, 0, flags);

	for (i = 0; i < framemgr->frame_cnt; i++) {
		frame = &framemgr->frame[i];
		if (frame->state == FIMC_IS_FRAME_STATE_REQUEST) {
			pr_err("%s buffer done1!!!! %d \n", __func__, i);
			fimc_is_frame_trans_req_to_com(framemgr, frame);
			queue_done(vctx, queue, i, VB2_BUF_STATE_ERROR);
		} else if (frame->state == FIMC_IS_FRAME_STATE_PROCESS) {
			pr_err("%s buffer done2!!!! %d \n", __func__, i);
			fimc_is_frame_trans_pro_to_com(framemgr, frame);
			queue_done(vctx, queue, i, VB2_BUF_STATE_ERROR);
		}
	}

	framemgr_x_barrier_irqr(framemgr, 0, flags);
}
#endif

static void fimc_is_sensor_instanton(struct work_struct *data)
{
	int ret = 0;
	u32 instant_cnt;
	struct fimc_is_device_sensor *device;
	struct fimc_is_device_ischain *ischain;

	BUG_ON(!data);

	device = container_of(data, struct fimc_is_device_sensor, instant_work);
	instant_cnt = device->instant_cnt;
	ischain = device->ischain;
	if (!ischain) {
		merr("ischain is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_stream_on(device->ischain);
	if (ret) {
		merr("fimc_is_itf_stream_on is fail(%d)\n", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

#ifdef ENABLE_DTP
	if (device->dtp_check) {
		setup_timer(&device->dtp_timer, fimc_is_sensor_dtp, (unsigned long)device);
		mod_timer(&device->dtp_timer, jiffies +  msecs_to_jiffies(300));
		pr_info("DTP checking...\n");
	}
#endif

	if (instant_cnt) {
		u32 timetowait, timeout;

		timeout = FIMC_IS_FLITE_STOP_TIMEOUT + msecs_to_jiffies(instant_cnt*60);
		timetowait = wait_event_timeout(device->instant_wait,
			(device->instant_cnt <= 1),
			timeout);
		if (!timetowait) {
			merr("wait_event_timeout is invalid", device);
			ret = -ETIME;
		}

		fimc_is_sensor_front_stop(device);

		pr_info("[FRT:D:%d] instant off(fcount : %d, time : %dms)", device->instance,
			device->instant_cnt,
			(jiffies_to_msecs(timeout) - jiffies_to_msecs(timetowait)));
	}

p_err:
	device->instant_ret = ret;
}

int fimc_is_sensor_probe(struct fimc_is_device_sensor *device,
	struct exynos_fimc_is_sensor_info *sensor_info)
{
	int ret = 0;
	struct sensor_open_extended *ext;
	struct fimc_is_enum_sensor *enum_sensor;

	BUG_ON(!device);
	BUG_ON(!sensor_info);

	device->clk_source = sensor_info->clk_src;
	device->csi.channel = sensor_info->csi_id;
	device->flite.channel = sensor_info->flite_id;
	device->id_position = sensor_info->sensor_position;
	init_waitqueue_head(&device->instant_wait);
	INIT_WORK(&device->instant_work, fimc_is_sensor_instanton);
	enum_sensor = device->enum_sensor;

	/*sensor init*/
	clear_bit(FIMC_IS_SENSOR_OPEN, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_START, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

	ret = fimc_is_flite_probe(&device->flite, (u32)device);
	if (ret) {
		merr("fimc_is_flite%d_probe is fail", device,
			device->flite.channel);
		goto p_err;
	}

#ifdef ENABLE_DTP
	device->dtp_check = false;
#endif

	/* S5K3H2 */
	enum_sensor[SENSOR_NAME_S5K3H2].sensor = SENSOR_NAME_S5K3H2;
	enum_sensor[SENSOR_NAME_S5K3H2].pixel_width = 0;
	enum_sensor[SENSOR_NAME_S5K3H2].pixel_height = 0;
	enum_sensor[SENSOR_NAME_S5K3H2].active_width = 0;
	enum_sensor[SENSOR_NAME_S5K3H2].active_height = 0;
	enum_sensor[SENSOR_NAME_S5K3H2].max_framerate = 0;
	enum_sensor[SENSOR_NAME_S5K3H2].csi_ch = sensor_info->csi_id;
	enum_sensor[SENSOR_NAME_S5K3H2].flite_ch = sensor_info->flite_id;
	enum_sensor[SENSOR_NAME_S5K3H2].i2c_ch = sensor_info->i2c_channel;
	enum_sensor[SENSOR_NAME_S5K3H2].setfile_name =
			"setfile_3h2.bin";

	ext = &enum_sensor[SENSOR_NAME_S5K3H2].ext;
	memset(ext, 0x0, sizeof(struct sensor_open_extended));

	/* S5K6A3 */
	enum_sensor[SENSOR_NAME_S5K6A3].sensor = SENSOR_NAME_S5K6A3;
	enum_sensor[SENSOR_NAME_S5K6A3].pixel_width = 1392 + 16;
	enum_sensor[SENSOR_NAME_S5K6A3].pixel_height = 1402 + 10;
	enum_sensor[SENSOR_NAME_S5K6A3].active_width = 1392;
	enum_sensor[SENSOR_NAME_S5K6A3].active_height = 1402;
	enum_sensor[SENSOR_NAME_S5K6A3].max_framerate = 30;
	enum_sensor[SENSOR_NAME_S5K6A3].csi_ch = sensor_info->csi_id;
	enum_sensor[SENSOR_NAME_S5K6A3].flite_ch = sensor_info->flite_id;
	enum_sensor[SENSOR_NAME_S5K6A3].i2c_ch = sensor_info->i2c_channel;
	enum_sensor[SENSOR_NAME_S5K6A3].setfile_name =
			"setfile_6a3.bin";

	ext = &enum_sensor[SENSOR_NAME_S5K6A3].ext;
	memset(ext, 0x0, sizeof(struct sensor_open_extended));

	ext->sensor_con.product_name = 0;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = sensor_info->i2c_channel;
	ext->sensor_con.peri_setting.i2c.slave_address = sensor_info->sensor_slave_address;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->I2CSclk = I2C_L0;

	/* S5K4E5 */
	enum_sensor[SENSOR_NAME_S5K4E5].sensor = SENSOR_NAME_S5K4E5;
	enum_sensor[SENSOR_NAME_S5K4E5].pixel_width = 2560 + 16;
	enum_sensor[SENSOR_NAME_S5K4E5].pixel_height = 1920 + 10;
	enum_sensor[SENSOR_NAME_S5K4E5].active_width = 2560;
	enum_sensor[SENSOR_NAME_S5K4E5].active_height = 1920;
	enum_sensor[SENSOR_NAME_S5K4E5].max_framerate = 30;
	enum_sensor[SENSOR_NAME_S5K4E5].csi_ch = sensor_info->csi_id;
	enum_sensor[SENSOR_NAME_S5K4E5].flite_ch = sensor_info->flite_id;
	enum_sensor[SENSOR_NAME_S5K4E5].i2c_ch = sensor_info->i2c_channel;
	enum_sensor[SENSOR_NAME_S5K4E5].setfile_name =
			"setfile_4e5.bin";

	ext = &enum_sensor[SENSOR_NAME_S5K4E5].ext;
	ext->actuator_con.product_name = ACTUATOR_NAME_DWXXXX;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel = sensor_info->actuator_i2c;

	ext->flash_con.product_name = sensor_info->flash_id;
	ext->flash_con.peri_type = SE_GPIO;
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = sensor_info->flash_first_gpio;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = sensor_info->flash_second_gpio;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->mclk = 0;
	ext->mipi_lane_num = 0;
	ext->mipi_speed = 0;
	ext->fast_open_sensor = 0;
	ext->self_calibration_mode = 0;
	ext->I2CSclk = I2C_L0;

	/* S5K3H5 */
	enum_sensor[SENSOR_NAME_S5K3H5].sensor = SENSOR_NAME_S5K3H5;
	enum_sensor[SENSOR_NAME_S5K3H5].pixel_width = 3248 + 16;
	enum_sensor[SENSOR_NAME_S5K3H5].pixel_height = 2438 + 10;
	enum_sensor[SENSOR_NAME_S5K3H5].active_width = 3248;
	enum_sensor[SENSOR_NAME_S5K3H5].active_height = 2438;
	enum_sensor[SENSOR_NAME_S5K3H5].max_framerate = 30;
	enum_sensor[SENSOR_NAME_S5K3H5].csi_ch = sensor_info->csi_id;
	enum_sensor[SENSOR_NAME_S5K3H5].flite_ch = sensor_info->flite_id;
	enum_sensor[SENSOR_NAME_S5K3H5].i2c_ch = sensor_info->i2c_channel;
	enum_sensor[SENSOR_NAME_S5K3H5].setfile_name =
			"setfile_3h5.bin";

	ext = &enum_sensor[SENSOR_NAME_S5K3H5].ext;

	ext->actuator_con.product_name = ACTUATOR_NAME_AK7343;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel = sensor_info->actuator_i2c;

	ext->flash_con.product_name = sensor_info->flash_id;
	ext->flash_con.peri_type = SE_GPIO;
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = sensor_info->flash_first_gpio;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = sensor_info->flash_second_gpio;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->mclk = 0;
	ext->mipi_lane_num = 0;
	ext->mipi_speed = 0;
	ext->fast_open_sensor = 0;
	ext->self_calibration_mode = 0;
	ext->I2CSclk = I2C_L0;

	/* S5K3H7 */
	enum_sensor[SENSOR_NAME_S5K3H7].sensor = SENSOR_NAME_S5K3H7;
	enum_sensor[SENSOR_NAME_S5K3H7].pixel_width = 3248 + 16;
	enum_sensor[SENSOR_NAME_S5K3H7].pixel_height = 2438 + 10;
	enum_sensor[SENSOR_NAME_S5K3H7].active_width = 3248;
	enum_sensor[SENSOR_NAME_S5K3H7].active_height = 2438;
	enum_sensor[SENSOR_NAME_S5K3H7].max_framerate = 30;
	enum_sensor[SENSOR_NAME_S5K3H7].csi_ch = sensor_info->csi_id;
	enum_sensor[SENSOR_NAME_S5K3H7].flite_ch = sensor_info->flite_id;
	enum_sensor[SENSOR_NAME_S5K3H7].i2c_ch = sensor_info->i2c_channel;
	enum_sensor[SENSOR_NAME_S5K3H7].setfile_name =
			"setfile_3h7.bin";

	ext = &enum_sensor[SENSOR_NAME_S5K3H7].ext;

	ext->actuator_con.product_name = ACTUATOR_NAME_AK7343;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel = sensor_info->actuator_i2c;

	ext->flash_con.product_name = sensor_info->flash_id;
	ext->flash_con.peri_type = SE_GPIO;
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = sensor_info->flash_first_gpio;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = sensor_info->flash_second_gpio;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->mclk = 0;
	ext->mipi_lane_num = 0;
	ext->mipi_speed = 0;
	ext->fast_open_sensor = 0;
	ext->self_calibration_mode = 0;
	ext->I2CSclk = I2C_L0;

	/* S5K3H7_SUNNY */
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].sensor = SENSOR_NAME_S5K3H7_SUNNY;
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].pixel_width = 3248 + 16;
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].pixel_height = 2438 + 10;
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].active_width = 3248;
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].active_height = 2438;
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].max_framerate = 30;
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].csi_ch = 0;
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].flite_ch = FLITE_ID_B;
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].i2c_ch = 0;
	enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].setfile_name =
			"setfile_3h7.bin";

	ext = &enum_sensor[SENSOR_NAME_S5K3H7_SUNNY].ext;

	ext->actuator_con.product_name = ACTUATOR_NAME_NOTHING;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel
		= SENSOR_CONTROL_I2C0;

	ext->flash_con.product_name = FLADRV_NAME_NOTHING;
	ext->flash_con.peri_type = SE_GPIO;
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = 17;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = 16;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->mclk = 0;
	ext->mipi_lane_num = 0;
	ext->mipi_speed = 0;
	ext->fast_open_sensor = 0;
	ext->self_calibration_mode = 0;
	ext->I2CSclk = I2C_L0;

	/* IMX135 */
	enum_sensor[SENSOR_NAME_IMX135].sensor = SENSOR_NAME_IMX135;
	enum_sensor[SENSOR_NAME_IMX135].pixel_width = 4128 + 16;
	enum_sensor[SENSOR_NAME_IMX135].pixel_height = 3096 + 10;
	enum_sensor[SENSOR_NAME_IMX135].active_width = 4128;
	enum_sensor[SENSOR_NAME_IMX135].active_height = 3096;
	enum_sensor[SENSOR_NAME_IMX135].max_framerate = 120;
	enum_sensor[SENSOR_NAME_IMX135].csi_ch = sensor_info->csi_id;
	enum_sensor[SENSOR_NAME_IMX135].flite_ch = sensor_info->flite_id;
	enum_sensor[SENSOR_NAME_IMX135].i2c_ch = sensor_info->i2c_channel;
	enum_sensor[SENSOR_NAME_IMX135].setfile_name = "setfile_imx135.bin";
	enum_sensor[SENSOR_NAME_IMX135].settles = ARRAY_SIZE(settle_imx135);
	enum_sensor[SENSOR_NAME_IMX135].settle_table = settle_imx135;

	ext = &enum_sensor[SENSOR_NAME_IMX135].ext;

	ext->sensor_con.product_name = 0;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = sensor_info->i2c_channel;
	ext->sensor_con.peri_setting.i2c.slave_address = sensor_info->sensor_slave_address;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->actuator_con.product_name = ACTUATOR_NAME_AK7345;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel = sensor_info->actuator_i2c;

	ext->flash_con.product_name = sensor_info->flash_id;
	ext->flash_con.peri_type = SE_GPIO;
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = sensor_info->flash_first_gpio;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = sensor_info->flash_second_gpio;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->mclk = 0;
	ext->mipi_lane_num = 0;
	ext->mipi_speed = 0;
	ext->fast_open_sensor = 0;
	ext->self_calibration_mode = 0;
	ext->I2CSclk = I2C_L0;

	/* S5K6B2 */
	enum_sensor[SENSOR_NAME_S5K6B2].sensor = SENSOR_NAME_S5K6B2;
	enum_sensor[SENSOR_NAME_S5K6B2].pixel_width = 1920 + 16;
	enum_sensor[SENSOR_NAME_S5K6B2].pixel_height = 1080 + 10;
	enum_sensor[SENSOR_NAME_S5K6B2].active_width = 1920;
	enum_sensor[SENSOR_NAME_S5K6B2].active_height = 1080;
	enum_sensor[SENSOR_NAME_S5K6B2].max_framerate = 30;
	enum_sensor[SENSOR_NAME_S5K6B2].csi_ch = sensor_info->csi_id;
	enum_sensor[SENSOR_NAME_S5K6B2].flite_ch = sensor_info->flite_id;
	enum_sensor[SENSOR_NAME_S5K6B2].i2c_ch = sensor_info->i2c_channel;
	enum_sensor[SENSOR_NAME_S5K6B2].setfile_name = "setfile_6b2.bin";
	enum_sensor[SENSOR_NAME_S5K6B2].settles = ARRAY_SIZE(settle_6b2);
	enum_sensor[SENSOR_NAME_S5K6B2].settle_table = settle_6b2;

	ext = &enum_sensor[SENSOR_NAME_S5K6B2].ext;
	memset(ext, 0x0, sizeof(struct sensor_open_extended));

	ext->sensor_con.product_name = 0;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = sensor_info->i2c_channel;
	ext->sensor_con.peri_setting.i2c.slave_address = sensor_info->sensor_slave_address;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->I2CSclk = I2C_L0;

	/* 3L2 */
	enum_sensor[SENSOR_NAME_S5K3L2].sensor = SENSOR_NAME_S5K3L2;
	enum_sensor[SENSOR_NAME_S5K3L2].pixel_width = 4128 + 16;
	enum_sensor[SENSOR_NAME_S5K3L2].pixel_height = 3096 + 10;
	enum_sensor[SENSOR_NAME_S5K3L2].active_width = 4128;
	enum_sensor[SENSOR_NAME_S5K3L2].active_height = 3096;
	enum_sensor[SENSOR_NAME_S5K3L2].max_framerate = 120;
	enum_sensor[SENSOR_NAME_S5K3L2].csi_ch = sensor_info->csi_id;
	enum_sensor[SENSOR_NAME_S5K3L2].flite_ch = sensor_info->flite_id;
	enum_sensor[SENSOR_NAME_S5K3L2].i2c_ch = sensor_info->i2c_channel;
	enum_sensor[SENSOR_NAME_S5K3L2].setfile_name = "setfile_3l2.bin";
	enum_sensor[SENSOR_NAME_S5K3L2].settles = ARRAY_SIZE(settle_3l2);
	enum_sensor[SENSOR_NAME_S5K3L2].settle_table = settle_3l2;

	ext = &enum_sensor[SENSOR_NAME_S5K3L2].ext;
	memset(ext, 0x0, sizeof(struct sensor_open_extended));

	ext->sensor_con.product_name = 0;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = sensor_info->i2c_channel;
	ext->sensor_con.peri_setting.i2c.slave_address = sensor_info->sensor_slave_address;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->actuator_con.product_name = ACTUATOR_NAME_DWXXXX;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel = sensor_info->actuator_i2c;

	ext->flash_con.product_name = sensor_info->flash_id;
	ext->flash_con.peri_type = sensor_info->flash_peri_type;
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = sensor_info->flash_first_gpio;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = sensor_info->flash_second_gpio;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->mclk = 0;
	ext->mipi_lane_num = 0;
	ext->mipi_speed = 0;
	ext->fast_open_sensor = 0;
	ext->self_calibration_mode = 0;
	ext->I2CSclk = I2C_L0;

	/* 2P2 */
	enum_sensor[SENSOR_NAME_S5K2P2].sensor = SENSOR_NAME_S5K2P2;
	enum_sensor[SENSOR_NAME_S5K2P2].pixel_width = 5312 + 16;
	enum_sensor[SENSOR_NAME_S5K2P2].pixel_height = 2990 + 10;
	enum_sensor[SENSOR_NAME_S5K2P2].active_width = 5312;
	enum_sensor[SENSOR_NAME_S5K2P2].active_height = 2990;
	enum_sensor[SENSOR_NAME_S5K2P2].max_framerate = 120;
	enum_sensor[SENSOR_NAME_S5K2P2].csi_ch = sensor_info->csi_id;
	enum_sensor[SENSOR_NAME_S5K2P2].flite_ch = sensor_info->flite_id;
	enum_sensor[SENSOR_NAME_S5K2P2].i2c_ch = sensor_info->i2c_channel;
	enum_sensor[SENSOR_NAME_S5K2P2].setfile_name = "setfile_2p2.bin";
	enum_sensor[SENSOR_NAME_S5K2P2].settles = ARRAY_SIZE(settle_imx135);
	enum_sensor[SENSOR_NAME_S5K2P2].settle_table = settle_imx135;

	ext = &enum_sensor[SENSOR_NAME_S5K2P2].ext;

	ext->sensor_con.product_name = 0;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = sensor_info->i2c_channel;
	ext->sensor_con.peri_setting.i2c.slave_address = sensor_info->sensor_slave_address;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->actuator_con.product_name = ACTUATOR_NAME_AK7345;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel = sensor_info->actuator_i2c;

	ext->flash_con.product_name = sensor_info->flash_id;
	ext->flash_con.peri_type = sensor_info->flash_peri_type;
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = sensor_info->flash_first_gpio;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = sensor_info->flash_second_gpio;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->mclk = 0;
	ext->mipi_lane_num = 0;
	ext->mipi_speed = 0;
	ext->fast_open_sensor = 0;
	ext->self_calibration_mode = 0;
	ext->I2CSclk = I2C_L0;

p_err:
	return ret;
}

int fimc_is_sensor_open(struct fimc_is_device_sensor *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_core *core;

	if (test_and_set_bit(FIMC_IS_SENSOR_OPEN, &device->state)) {
		merr("already open", device);
		ret = -EMFILE;
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_START, &device->state);
	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);
	device->vctx = vctx;
	device->instant_cnt = 0;
	device->instant_ret = 0;
	device->active_sensor = NULL;
	device->ischain = NULL;

	core = (struct fimc_is_core *)vctx->video->core;

	/* for mediaserver force close */
	ret = fimc_is_resource_get(core);
	if (ret) {
		merr("fimc_is_resource_get is fail", device);
		goto p_err;
	}

	/* Sensor clock on */
	ret = fimc_is_sensor_clock_on(device, device->clk_source);
	if (ret) {
		merr("fimc_is_sensor_clock_on is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_flite_open(&device->flite, vctx);
	if (ret != 0) {
		merr("fimc_is_flite_open(%d) fail",
			device, device->flite.channel);
		goto p_err;
	}

	/* Sensor power on */
	if (core->pdata->cfg_gpio) {
		core->pdata->cfg_gpio(core->pdev,
					device->flite.channel,
					true);
	} else {
		err("sensor power on is fail");
		ret = -EINVAL;
		goto p_err;
	}

#ifdef ENABLE_DTP
	device->dtp_check = true;
#endif

p_err:
	pr_info("[SEN:D:%d] %s(%d)\n", device->instance, __func__, ret);

	return ret;
}

int fimc_is_sensor_close(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_group *group_3ax;

	BUG_ON(!device);

	core = (struct fimc_is_core *)device->vctx->video->core;

	if (!test_and_clear_bit(FIMC_IS_SENSOR_OPEN, &device->state)) {
		merr("already close", device);
		ret = -EMFILE;
		goto p_err;
	}

	/* for mediaserver force close */
	ischain = device->ischain;
	if (ischain) {
		group_3ax = &ischain->group_3ax;
		if (test_bit(FIMC_IS_GROUP_READY, &group_3ax->state)) {
			pr_info("media server is dead, 3ax forcely done\n");
			set_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group_3ax->state);
		}
	}

	fimc_is_sensor_back_stop(device);
	fimc_is_sensor_front_stop(device);

	/* HACK: Move to fimc_is_sensor_front_stop() */
	stop_mipi_csi(device->active_sensor->csi_ch);

	ret = fimc_is_flite_close(&device->flite);
	if (ret != 0) {
		merr("fimc_is_flite_close(%d) fail",
			device, device->flite.channel);
		goto exit_rsc;
	}

	/* Sensor power off */
	if (core->pdata->cfg_gpio) {
		core->pdata->cfg_gpio(core->pdev,
					device->flite.channel,
					false);
	} else {
		err("sesnor power off is fail");
		ret = -EINVAL;
		goto exit_rsc;
	}

	/* Sensor clock off */
	ret = fimc_is_sensor_clock_off(device, device->clk_source);
	if (ret) {
		merr("fimc_is_sensor_clock_off is fail(%d)", device, ret);
		goto p_err;
	}

exit_rsc:
	/* for mediaserver force close */
	ret = fimc_is_resource_put(core);
	if (ret) {
		merr("fimc_is_resource_put is fail", device);
		goto p_err;
	}

p_err:
	pr_info("[SEN:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return ret;
}

int fimc_is_sensor_s_format(struct fimc_is_device_sensor *device,
	u32 width, u32 height)
{
	device->width = width;
	device->height = height;

	return 0;
}

int fimc_is_sensor_s_active_sensor(struct fimc_is_device_sensor *device,
	struct fimc_is_video_ctx *vctx,
	struct fimc_is_framemgr *framemgr,
	u32 input)
{
	int ret = 0;
	int module = 0;

	module = input & MODULE_MASK;

	mdbgd_sensor("%s(%08X) module(%d)\n", device, __func__, input, module);

	device->active_sensor = &device->enum_sensor[module];
	device->framerate = min_t(unsigned int, SENSOR_DEFAULT_FRAMERATE,
				device->active_sensor->max_framerate);
	device->width = device->active_sensor->pixel_width;
	device->height = device->active_sensor->pixel_height;

	return ret;
}

int fimc_is_sensor_buffer_queue(struct fimc_is_device_sensor *device,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto exit;
	}

	framemgr = &device->vctx->q_dst.framemgr;
	if (framemgr == NULL) {
		err("framemgr is null\n");
		ret = EINVAL;
		goto exit;
	}

	frame = &framemgr->frame[index];
	if (frame == NULL) {
		err("frame is null\n");
		ret = EINVAL;
		goto exit;
	}

	if (unlikely(frame->memory == FRAME_UNI_MEM)) {
		err("frame %d is NOT init", index);
		ret = EINVAL;
		goto exit;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_2 + index, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_FREE) {
		fimc_is_frame_trans_fre_to_req(framemgr, frame);
	} else {
		err("frame(%d) is not free state(%d)", index, frame->state);
		fimc_is_frame_print_all(framemgr);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_2 + index, flags);

exit:
	return ret;
}

int fimc_is_sensor_buffer_finish(struct fimc_is_device_sensor *device,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto exit;
	}

	framemgr = &device->vctx->q_dst.framemgr;
	frame = &framemgr->frame[index];

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_3 + index, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_COMPLETE) {
		if (!frame->shot->dm.request.frameCount)
			err("request.frameCount is 0\n");
		fimc_is_frame_trans_com_to_fre(framemgr, frame);

		frame->shot_ext->free_cnt = framemgr->frame_fre_cnt;
		frame->shot_ext->request_cnt = framemgr->frame_req_cnt;
		frame->shot_ext->process_cnt = framemgr->frame_pro_cnt;
		frame->shot_ext->complete_cnt = framemgr->frame_com_cnt;
	} else {
		err("frame(%d) is not com state(%d)", index, frame->state);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_3 + index, flags);

exit:
	return ret;
}

int fimc_is_sensor_back_start(struct fimc_is_device_sensor *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_frame_info frame;

	dbg_back("%s\n", __func__);

	if (test_and_set_bit(FIMC_IS_SENSOR_BACK_START, &device->state)) {
		err("already back start");
		ret = -EINVAL;
		goto exit;
	}

	frame.o_width = device->width;
	frame.o_height = device->height;
	frame.offs_h = 0;
	frame.offs_v = 0;
	frame.width = device->width;
	frame.height = device->height;

	/*start flite*/
	fimc_is_flite_start(&device->flite, &frame, vctx);

	pr_info("[BAK:D:%d] start(%dx%d)\n", device->instance,
		frame.width, frame.height);

exit:
	return ret;
}

int fimc_is_sensor_back_stop(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	bool wait = true;
	unsigned long flags;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_device_flite *flite;

	dbg_back("%s\n", __func__);

	if (!test_and_clear_bit(FIMC_IS_SENSOR_BACK_START, &device->state)) {
		warn("already back stop");
		goto exit;
	}

	framemgr = GET_DST_FRAMEMGR(device->vctx);
	flite = &device->flite;

	if (test_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state)) {
		warn("fimc_is_flite_stop, no waiting...");
		wait = false;
	}

	ret = fimc_is_flite_stop(flite, wait);
	if (ret)
		err("fimc_is_flite_stop is fail");

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_3, flags);

	fimc_is_frame_complete_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_com_to_fre(framemgr, frame);
		fimc_is_frame_complete_head(framemgr, &frame);
	}

	fimc_is_frame_process_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_pro_to_fre(framemgr, frame);
		fimc_is_frame_process_head(framemgr, &frame);
	}

	fimc_is_frame_request_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_req_to_fre(framemgr, frame);
		fimc_is_frame_request_head(framemgr, &frame);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_3, flags);

exit:
	pr_info("[BAK:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return ret;
}

int fimc_is_sensor_back_pause(struct fimc_is_device_sensor *device)
{
	int ret = 0;
#if (defined(DEBUG) && defined(DBG_DEVICE))
	struct fimc_is_framemgr *framemgr = GET_DST_FRAMEMGR(device->vctx);
#endif

	dbg_back("%s\n", __func__);

	ret = fimc_is_flite_stop(&device->flite, false);
	if (ret)
		err("fimc_is_flite_stop is fail");

	dbg_back("framemgr cnt: %d, fre: %d, req: %d, pro: %d, com: %d\n",
			framemgr->frame_cnt, framemgr->frame_fre_cnt,
			framemgr->frame_req_cnt, framemgr->frame_pro_cnt,
			framemgr->frame_com_cnt);

	/* need to re-arrange frames in manager */

	return ret;
}

void fimc_is_sensor_back_restart(struct fimc_is_device_sensor *device)
{
	struct fimc_is_frame_info frame;

	dbg_back("%s\n", __func__);

	frame.o_width = device->width;
	frame.o_height = device->height;
	frame.offs_h = 0;
	frame.offs_v = 0;
	frame.width = device->width;
	frame.height = device->height;

	fimc_is_flite_restart(&device->flite, &frame, device->vctx);

	dbg_back("restart flite (pos:%d) (port:%d) : %d x %d\n",
		device->active_sensor->sensor,
		device->active_sensor->flite_ch,
		frame.width, frame.height);
}

int fimc_is_sensor_front_start(struct fimc_is_device_sensor *device,
	u32 instant_cnt,
	u32 nonblock)
{
	int ret = 0;
	struct fimc_is_frame_info frame;

	dbg_front("%s\n", __func__);

	if (test_and_set_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
		merr("already front start", device);
		ret = -EINVAL;
		goto p_err;
	}

	device->instant_cnt = instant_cnt;
	frame.o_width = device->width;
	frame.o_height = device->height;
	frame.offs_h = 0;
	frame.offs_v = 0;
	frame.width = device->width;
	frame.height = device->height;

	start_mipi_csi(device->active_sensor->csi_ch,
		device->active_sensor->settle_table,
		device->active_sensor->settles,
		&frame,
		device->framerate);

	/*start mipi*/
	dbg_front("start mipi (snesor id:%d) (port:%d) : %d x %d\n",
		device->active_sensor->sensor,
		device->active_sensor->csi_ch,
		frame.width, frame.height);

	if (nonblock) {
		schedule_work(&device->instant_work);
	} else {
		fimc_is_sensor_instanton(&device->instant_work);
		if (device->instant_ret) {
			merr("fimc_is_sensor_instanton is fail(%d)", device, device->instant_ret);
			ret = device->instant_ret;
			goto p_err;
		}
	}

p_err:
	return ret;
}

int fimc_is_sensor_front_stop(struct fimc_is_device_sensor *device)
{
	int ret = 0;

	dbg_front("%s\n", __func__);

	if (!test_and_clear_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
		warn("already front stop");
		goto exit;
	}

	if (!device->ischain) {
		mwarn("ischain is NULL", device);
		goto exit;
	}

	ret = fimc_is_itf_stream_off(device->ischain);
	if (ret)
		err("sensor stream off is failed(error %d)\n", ret);
	else
		dbg_front("sensor stream off\n");

	if (!device->active_sensor) {
		mwarn("active_sensor is NULL", device);
		goto exit;
	}

	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

exit:
	pr_info("[FRT:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return ret;
}
