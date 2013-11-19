/* linux/drivers/decon_display/decon_mic.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Haowei Li <haowei.li@samsung.com>
 *
 * Samsung MIC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>

#include "decon_mic.h"
#include "decon_display_driver.h"
#include "decon_mipi_dsi_lowlevel.h"
#include "decon_mipi_dsi.h"
#include "regs-mipidsim.h"
#include "decon_fb.h"
#include "decon_dt.h"
#include "decon_pm.h"

#ifdef CONFIG_OF
static const struct of_device_id exynos5_mic[] = {
	{ .compatible = "samsung,exynos5-mic" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos5_mic);
#endif

enum mic_on_off {
	DECON_MIC_OFF = 0,
	DECON_MIC_ON = 1
};

struct decon_mic {
	struct device *dev;
	void __iomem *reg_base;
	struct decon_lcd *lcd;
	struct mic_config *mic_config;
	bool decon_mic_on;
};

struct decon_mic *mic_for_decon;
EXPORT_SYMBOL(mic_for_decon);

#ifdef CONFIG_FB_I80_COMMAND_MODE
int decon_mic_hibernation_power_on(struct display_driver *dispdrv);
int decon_mic_hibernation_power_off(struct display_driver *dispdrv);
#endif

static int decon_mic_set_sys_reg(struct decon_mic *mic, bool enable)
{
	u32 data;
	void __iomem *sysreg_va;

	sysreg_va = ioremap(mic->mic_config->sysreg1, 0x4);

	if (enable) {
		data = readl(sysreg_va) & ~(0xf);
		data |= (1 << 3) | (1 << 2) | (1 << 1) | (1 << 0);
		writel(data, sysreg_va);
		iounmap(sysreg_va);

		sysreg_va = ioremap(mic->mic_config->sysreg2, 0x4);
		writel(0x80000000, sysreg_va);
		iounmap(sysreg_va);
	} else {
		data = readl(sysreg_va) & ~(0xf);
		writel(data, sysreg_va);
		iounmap(sysreg_va);
	}

	return 0;
}

static void decon_mic_set_image_size(struct decon_mic *mic)
{
	u32 data = 0;
	struct decon_lcd *lcd = mic->lcd;

	data = (lcd->yres << DECON_MIC_IMG_V_SIZE_SHIFT)
		| (lcd->xres << DECON_MIC_IMG_H_SIZE_SHIFT);

	writel(data, mic->reg_base + DECON_MIC_IMG_SIZE);
}

static unsigned int decon_mic_calc_bs_size(struct decon_mic *mic)
{
	struct decon_lcd *lcd = mic->lcd;
	u32 temp1, temp2, bs_size;

	temp1 = lcd->xres / 4 * 2;
	temp2 = lcd->xres % 4;
	bs_size = temp1 + temp2;

	return bs_size;
}

static void decon_mic_set_2d_bit_stream_size(struct decon_mic *mic)
{
	u32 data;

	data = decon_mic_calc_bs_size(mic);

	writel(data, mic->reg_base + DECON_MIC_2D_OUTPUT_TIMING_2);
}

static void decon_mic_set_mic_base_operation(struct decon_mic *mic, bool enable)
{
	u32 data = readl(mic->reg_base);
	struct decon_lcd *lcd = mic->lcd;

	if (enable) {
		data |= DECON_MIC_OLD_CORE | DECON_MIC_CORE_ENABLE
			| DECON_MIC_UPDATE_REG | DECON_MIC_ON_REG;

		if (lcd->mode == COMMAND_MODE)
			data |= DECON_MIC_COMMAND_MODE;
		else
			data |= DECON_MIC_VIDEO_MODE;
	} else {
		data &= ~DECON_MIC_CORE_ENABLE;
		data |= DECON_MIC_UPDATE_REG;
	}

	writel(data, mic->reg_base);
}

int create_decon_mic(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct display_driver *dispdrv;
	struct decon_mic *mic;
	struct resource *res;
	int ret;

	dispdrv = get_display_driver();

	mic = devm_kzalloc(dev, sizeof(struct decon_mic), GFP_KERNEL);
	if (!mic) {
		dev_err(dev, "no memory for mic driver");
		return -ENOMEM;
	}

	mic->dev = dev;

	mic->lcd = decon_get_lcd_info();

	mic->mic_config = dispdrv->dt_ops.get_display_mic_config();

	mic->decon_mic_on = false;

	res = dispdrv->mic_driver.regs;
	if (!res) {
		dev_err(dev, "failed to find resource\n");
		ret = -ENOENT;
	}

	mic->reg_base = ioremap(res->start, resource_size(res));
	if (!mic->reg_base) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENXIO;
	}

	decon_mic_set_sys_reg(mic, DECON_MIC_ON);

	decon_mic_set_image_size(mic);

	decon_mic_set_2d_bit_stream_size(mic);

	decon_mic_set_mic_base_operation(mic, DECON_MIC_ON);

	mic_for_decon = mic;

	mic->decon_mic_on = true;

	dispdrv->mic_driver.mic = mic;
#ifdef CONFIG_FB_I80_COMMAND_MODE
	dispdrv->mic_driver.ops->pwr_on = decon_mic_hibernation_power_on;
	dispdrv->mic_driver.ops->pwr_off = decon_mic_hibernation_power_off;
#endif

	dev_info(dev, "MIC driver has been probed\n");
	return 0;
}

int decon_mic_enable(struct decon_mic *mic)
{
	if (mic->decon_mic_on == true)
		return 0;

	decon_mic_set_sys_reg(mic, DECON_MIC_ON);

	decon_mic_set_image_size(mic);

	decon_mic_set_2d_bit_stream_size(mic);

	decon_mic_set_mic_base_operation(mic, DECON_MIC_ON);

	mic->decon_mic_on = true;

	dev_info(mic->dev, "MIC driver is ON;\n");

	return 0;
}

int decon_mic_disable(struct decon_mic *mic)
{
	if (mic->decon_mic_on == false)
		return 0;
	decon_mic_set_sys_reg(mic, DECON_MIC_OFF);

	decon_mic_set_mic_base_operation(mic, DECON_MIC_OFF);

	mic->decon_mic_on = false;

	dev_info(mic->dev, "MIC driver is OFF;\n");

	return 0;
}

int decon_mic_sw_reset(struct decon_mic *mic)
{
	void __iomem *regs = mic->reg_base + DECON_MIC_OP;

	u32 data = readl(regs);

	data |= DECON_MIC_SW_RST;
	writel(data, regs);

	return 0;
}

#ifdef CONFIG_FB_I80_COMMAND_MODE
int decon_mic_hibernation_power_on(struct display_driver *dispdrv)
{
	struct decon_mic *mic = dispdrv->mic_driver.mic;

	decon_mic_set_sys_reg(mic, DECON_MIC_ON);
	decon_mic_set_image_size(mic);
	decon_mic_set_2d_bit_stream_size(mic);
	decon_mic_set_mic_base_operation(mic, DECON_MIC_ON);

	mic->decon_mic_on = true;

	return 0;
}

int decon_mic_hibernation_power_off(struct display_driver *dispdrv)
{
	struct decon_mic *mic = dispdrv->mic_driver.mic;

	decon_mic_set_mic_base_operation(mic, DECON_MIC_OFF);
	decon_mic_sw_reset(mic);
	decon_mic_set_sys_reg(mic, DECON_MIC_OFF);

	mic->decon_mic_on = false;

	return 0;
}
#endif

MODULE_AUTHOR("Haowei Li <Haowei.li@samsung.com>");
MODULE_DESCRIPTION("Samsung MIC driver");
MODULE_LICENSE("GPL");
