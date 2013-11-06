/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is/mipi-csi functions
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
#include <linux/platform_device.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include "fimc-is-core.h"
#include "fimc-is-device-csi.h"

/* PMU for FIMC-IS*/
#define MIPICSI0_REG_BASE	(S5P_VA_MIPICSI0)   /* phy : 0x13c2_0000 */
#define MIPICSI1_REG_BASE	(S5P_VA_MIPICSI1)   /* phy : 0x13c3_0000 */
#define MIPICSI2_REG_BASE	(S5P_VA_MIPICSI2)   /* phy : 0x13d1_0000 */

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

static u32 get_hsync_settle(struct fimc_is_settle *settle_table,
	u32 settle_max, u32 width, u32 height, u32 framerate)
{
	u32 settle;
	u32 max_settle;
	u32 proximity_framerate, proximity_settle;
	u32 i;

	settle = 0;
	max_settle = 0;
	proximity_framerate = 0;
	proximity_settle = 0;

	for (i = 0; i < settle_max; i++) {
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

static void s5pcsis_enable_interrupts(unsigned long __iomem *base_reg, bool on)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_INTMSK));

	val = on ? val | S5PCSIS_INTMSK_EN_ALL :
		   val & ~S5PCSIS_INTMSK_EN_ALL;
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_INTMSK));
}

static void s5pcsis_reset(unsigned long __iomem *base_reg)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));

	writel(val | S5PCSIS_CTRL_RESET, base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
	udelay(10);
}

static void s5pcsis_system_enable(unsigned long __iomem *base_reg, int on)
{
	u32 val;

	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));

#if defined(CONFIG_SOC_EXYNOS5420) || defined(CONFIG_SOC_EXYNOS5430)
	val |= S5PCSIS_CTRL_WCLK_EXTCLK;
	val |= S5PCSIS_CTRL_NUMOFDATALANE(0x3);
#endif

	if (on) {
		val |= S5PCSIS_CTRL_ENABLE;
		val |= S5PCSIS_CTRL_WCLK_EXTCLK;
	} else
		val &= ~S5PCSIS_CTRL_ENABLE;
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));

	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));
	if (on)
		val |= S5PCSIS_DPHYCTRL_ENABLE;
	else
		val &= ~S5PCSIS_DPHYCTRL_ENABLE;
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));
}

/* Called with the state.lock mutex held */
static void __s5pcsis_set_format(unsigned long __iomem *base_reg,
	struct fimc_is_image *image)
{
	u32 val;

	BUG_ON(!image);

	/* Color format */
	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CONFIG));

	if (image->format.pixelformat == V4L2_PIX_FMT_SGRBG8)
		val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_RAW8;
	else
		val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_RAW10;

#if defined(CONFIG_SOC_EXYNOS5420) || defined(CONFIG_SOC_EXYNOS5430)
	val |= S5PCSIS_CFG_START_INTERVAL(1);
#endif
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_CONFIG));

	/* Pixel resolution */
	val = (image->window.o_width << 16) | image->window.o_height;
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_RESOL));
}

static void s5pcsis_set_hsync_settle(unsigned long __iomem *base_reg, int settle)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));

	if (soc_is_exynos5250())
		val = (val & ~S5PCSIS_DPHYCTRL_HSS_MASK) | (0x6 << 28);
	else
		val = (val & ~S5PCSIS_DPHYCTRL_HSS_MASK) | (settle << 24);

	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));
}

static void s5pcsis_set_params(unsigned long __iomem *base_reg,
	struct fimc_is_image *image)
{
	u32 val;

#if defined(CONFIG_SOC_EXYNOS5410)
	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CONFIG));
	if (soc_is_exynos5250())
		val = (val & ~S5PCSIS_CFG_NR_LANE_MASK) | (2 - 1);
	else
		val = (val & ~S5PCSIS_CFG_NR_LANE_MASK) | (4 - 1);
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_CONFIG));
#endif

	__s5pcsis_set_format(base_reg, image);

	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
	val &= ~S5PCSIS_CTRL_ALIGN_32BIT;

	/* Not using external clock. */
	val &= ~S5PCSIS_CTRL_WCLK_EXTCLK;

	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));

	/* Update the shadow register. */
	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
	writel(val | S5PCSIS_CTRL_UPDATE_SHADOW(1), base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
}

int fimc_is_csi_open(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;

	BUG_ON(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	csi->settle_max = 0;
	csi->settle_table = NULL;
	memset(&csi->image, 0, sizeof(struct fimc_is_image));

p_err:
	return ret;
}

int fimc_is_csi_close(struct v4l2_subdev *subdev)
{
	return 0;
}

/* value : module enum */
static int csi_init(struct v4l2_subdev *subdev, u32 value)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;
	struct fimc_is_module_enum *module;

	BUG_ON(!subdev);
	BUG_ON(!value);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = (struct fimc_is_module_enum *)value;
	csi->settle_max = module->settle_max;
	csi->settle_table = module->settle_table;
	csi->image.framerate = SENSOR_DEFAULT_FRAMERATE; /* default frame rate */

p_err:
	return ret;
}

static int csi_s_power(struct v4l2_subdev *subdev,
	int on)
{
	int ret = 0;
	void __iomem *addr;
	u32 cfg;
	struct fimc_is_device_csi *csi;

	BUG_ON(!subdev);

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	if (on == 0)
		goto p_err;

	addr = S5P_MIPI_DPHY_CONTROL(0);

	cfg = __raw_readl(addr);
	cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	__raw_writel(cfg, addr);

	if (1) {
		cfg |= S5P_MIPI_DPHY_ENABLE;
	} else if (!(cfg & (S5P_MIPI_DPHY_SRESETN | S5P_MIPI_DPHY_MRESETN)
			& (~S5P_MIPI_DPHY_SRESETN))) {
		cfg &= ~S5P_MIPI_DPHY_ENABLE;
	}

	__raw_writel(cfg, addr);


	addr = S5P_MIPI_DPHY_CONTROL(1);

	cfg = __raw_readl(addr);
	cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	__raw_writel(cfg, addr);

	if (1) {
		cfg |= S5P_MIPI_DPHY_ENABLE;
	} else if (!(cfg & (S5P_MIPI_DPHY_SRESETN | S5P_MIPI_DPHY_MRESETN)
			& (~S5P_MIPI_DPHY_SRESETN))) {
		cfg &= ~S5P_MIPI_DPHY_ENABLE;
	}

	__raw_writel(cfg, addr);

	addr = S5P_MIPI_DPHY_CONTROL(2);

	cfg = __raw_readl(addr);
	cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	__raw_writel(cfg, addr);

	cfg |= S5P_MIPI_DPHY_ENABLE;
	if (!(cfg & (S5P_MIPI_DPHY_SRESETN | S5P_MIPI_DPHY_MRESETN) & (~S5P_MIPI_DPHY_SRESETN)))
		cfg &= ~S5P_MIPI_DPHY_ENABLE;

	__raw_writel(cfg, addr);

p_err:
	mdbgd_front("%s(%d, %d)\n", csi, __func__, on, ret);
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = csi_init,
	.s_power = csi_s_power
};

static int csi_stream_on(struct fimc_is_device_csi *csi)
{
	int ret = 0;
	u32 settle;

	BUG_ON(!csi);
	BUG_ON(!csi->settle_table);

	s5pcsis_reset(csi->base_reg);

	settle = get_hsync_settle(
		csi->settle_table,
		csi->settle_max,
		csi->image.window.width,
		csi->image.window.height,
		csi->image.framerate);

	minfo("[CSI:D:%d] settle(%dx%d@%d) = %d\n",
		csi->instance,
		csi->image.window.width,
		csi->image.window.height,
		csi->image.framerate,
		settle);

	s5pcsis_set_hsync_settle(csi->base_reg, settle);
	s5pcsis_set_params(csi->base_reg, &csi->image);
	s5pcsis_system_enable(csi->base_reg, true);
	s5pcsis_enable_interrupts(csi->base_reg, true);

	return ret;
}

static int csi_stream_off(struct fimc_is_device_csi *csi)
{
	BUG_ON(!csi);

	s5pcsis_enable_interrupts(csi->base_reg, false);
	s5pcsis_system_enable(csi->base_reg, false);

	return 0;
}

static int csi_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;

	BUG_ON(!subdev);

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	if (enable) {
		ret = csi_stream_on(csi);
		if (ret) {
			err("csi_stream_on is fail(%d)", ret);
			goto p_err;
		}
	} else {
		ret = csi_stream_off(csi);
		if (ret) {
			err("csi_stream_off is fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	return 0;
}

static int csi_s_param(struct v4l2_subdev *subdev, struct v4l2_streamparm *param)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;

	BUG_ON(!subdev);
	BUG_ON(!param);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	csi->image.framerate = tpf->denominator / tpf->numerator;

	mdbgd_front("%s(%d, %d)\n", csi, __func__, csi->image.framerate, ret);
	return ret;
}

static int csi_s_format(struct v4l2_subdev *subdev, struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;

	BUG_ON(!subdev);
	BUG_ON(!fmt);

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	csi->image.window.offs_h = 0;
	csi->image.window.offs_v = 0;
	csi->image.window.width = fmt->width;
	csi->image.window.height = fmt->height;
	csi->image.window.o_width = fmt->width;
	csi->image.window.o_height = fmt->height;
	csi->image.format.pixelformat = fmt->code;

	mdbgd_front("%s(%dx%d, %X)\n", csi, __func__, fmt->width, fmt->height, fmt->code);
	return ret;
}

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = csi_s_stream,
	.s_parm = csi_s_param,
	.s_mbus_fmt = csi_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops
};

int fimc_is_csi_probe(struct fimc_is_device_sensor *device,
	u32 instance)
{
	int ret = 0;
	struct v4l2_subdev *subdev_csi;
	struct fimc_is_device_csi *csi;

	BUG_ON(!device);

	subdev_csi = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_csi) {
		merr("subdev_csi is NULL", device);
		ret = -ENOMEM;
		goto p_err;
	}
	device->subdev_csi = subdev_csi;

	csi = kzalloc(sizeof(struct fimc_is_device_csi), GFP_KERNEL);
	if (!csi) {
		merr("csi is NULL", device);
		ret = -ENOMEM;
		goto p_err;
	}

	csi->instance = instance;
	switch(instance) {
	case CSI_ID_A:
		csi->base_reg = (unsigned long *)MIPICSI0_REG_BASE;
		break;
	case CSI_ID_B:
		csi->base_reg = (unsigned long *)MIPICSI1_REG_BASE;
		break;
	case CSI_ID_C:
		csi->base_reg = (unsigned long *)MIPICSI2_REG_BASE;
		break;
	default:
		err("instance is invalid(%d)", instance);
		ret = -EINVAL;
		goto p_err;
	}

	v4l2_subdev_init(subdev_csi, &subdev_ops);
	v4l2_set_subdevdata(subdev_csi, csi);
	v4l2_set_subdev_hostdata(subdev_csi, device);
	snprintf(subdev_csi->name, V4L2_SUBDEV_NAME_SIZE, "csi-subdev.%d", instance);
	ret = v4l2_device_register_subdev(&device->v4l2_dev, subdev_csi);
	if (ret) {
		merr("v4l2_device_register_subdev is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	minfo("[FRT:D:%d] %s(%d)\n", instance, __func__, ret);
	return ret;
}