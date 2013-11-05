/* linux/drivers/video/decon_display/fimd_pm_exynos.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#include <linux/platform_device.h>
#include "decon_display_driver.h"
#include "fimd_fb.h"
#include "decon_mipi_dsi.h"
#include "fimd_dt.h"
#include <mach/map.h>

#include <../drivers/clk/samsung/clk.h>

#define DISPLAY_CLOCK_SET_PARENT(child, parent) do {\
	g_##child = clk_get(dev, #child); \
	g_##parent = clk_get(dev, #parent); \
	if (IS_ERR(g_##child)) { \
		pr_err("Failed to clk_get - " #child "\n"); \
		return PTR_ERR(g_##child); \
	} \
	if (IS_ERR(g_##parent)) { \
		pr_err("Failed to clk_get - " #parent "\n"); \
		return PTR_ERR(g_##parent); \
	} \
	ret = clk_set_parent(g_##child, g_##parent); \
	if (ret < 0) { \
		pr_err("failed to set parent " #parent " of " #child "\n"); \
	} \
	} while (0)

#define DISPLAY_CLOCK_INLINE_SET_PARENT(child, parent) do {\
	ret = clk_set_parent(g_##child, g_##parent); \
	if (ret < 0) { \
		pr_err("failed to set parent " #parent " of " #child "\n"); \
	} \
	} while (0)

#define DISPLAY_CHECK_REGS(addr, mask, val) do {\
	regs = ioremap(addr, 0x4); \
	data = readl(regs); \
	if ((data & mask) != val) { \
		pr_err("[ERROR] Masked value, (0x%08X & " \
			"val(0x%08X)) SHOULD BE 0x%08X, but 0x%08X\n", \
			mask, addr, val, (data&mask)); \
	} \
	iounmap(regs); \
	} while (0)

#define DISPLAY_INLINE_SET_RATE(node, target) \
	clk_set_rate(g_##node, target);

#define DISPLAY_SET_RATE(node, target) do {\
	g_##node = clk_get(dev, #node); \
	if (IS_ERR(g_##node)) { \
		pr_err("Failed to clk_get - " #node "\n"); \
		return PTR_ERR(g_##node); \
	} \
	clk_set_rate(g_##node, target); \
	} while (0)

int init_display_fimd_clocks_exynos(struct device *dev)
{
	int ret = 0;

	ret = exynos_set_parent("mout_fimd1", "mout_rpll_ctrl");
	if (ret < 0)
		pr_err("DISPLAY_CLOCK_SET_PARENT: ret %d\n", ret);

	ret = exynos_set_parent("mout_fimd1_mdnie1", "mout_fimd1");
	if (ret < 0)
		pr_err("DISPLAY_CLOCK_SET_PARENT: ret %d\n", ret);

#ifdef CONFIG_DECON_LCD_S6E8AA0
	ret = exynos_set_rate("sclk_fimd1", 67 * MHZ);
#else
        ret = exynos_set_rate("sclk_fimd1", 266 * MHZ);
#endif
        if (ret < 0)
                printk("exynos_set_rate failed: ret %d \n", ret);

        ret = exynos_set_rate("dout_aclk_300_disp1", 300 * MHZ);
	if (ret < 0)
		pr_err("exynos_set_rate failed: ret %d\n", ret);

	pr_info("%s: clk_rate: %d\n", __func__, exynos_get_rate("sclk_fimd1"));
	return ret;
}

int init_display_dsi_clocks_exynos(struct device *dev)
{
	int ret = 0;
	return ret;
}

void init_display_gpio_exynos(void)
{
	unsigned int reg = 0;

#if defined(CONFIG_S5P_DP)
	unsigned gpio_dp_hotplug = 0;

	gpio_dp_hotplug = get_display_dp_hotplug_gpio_exynos();
	/* Set Hotplug detect for DP */
	gpio_request(gpio_dp_hotplug, "dp_hotplug");
	/* TO DO */
	s3c_gpio_cfgpin(gpio_dp_hotplug, S3C_GPIO_SFN(3));
#endif

	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * FIMD of DISP1_BLK Bypass selection : DISP1BLK_CFG[15]
	 * ---------------------
	 *  0 | MIE/MDNIE
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1 << 15);	/* To save other reset values */
	reg |= (1 << 15);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#if defined(CONFIG_S5P_DP)
	/* Reference clcok selection for DPTX_PHY: PAD_OSC_IN */
	reg = __raw_readl(S3C_VA_SYS + 0x04d4);
	reg &= ~(1 << 0);
	__raw_writel(reg, S3C_VA_SYS + 0x04d4);

	/* DPTX_PHY: XXTI */
	reg = __raw_readl(S3C_VA_SYS + 0x04d8);
	reg &= ~(1 << 3);
	__raw_writel(reg, S3C_VA_SYS + 0x04d8);
#endif
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * MIC of DISP1_BLK Bypass selection: DISP1BLK_CFG[11]
	 * --------------------
	 *  0 | MIC
	 *  1 | Bypass : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg |= (1 << 11);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#ifdef CONFIG_FB_I80_COMMAND_MODE
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg |= (1 << 24);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#endif
}


int enable_display_dsi_power_exynos(struct device *dev)
{
	unsigned gpio_power;
	unsigned int gpio_reset;
#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	struct pinctrl *pinctrl;
#endif
	int ret = 0;

	gpio_power = get_display_dsi_lcd_power_gpio_exynos();
	gpio_request_one(gpio_power, GPIOF_OUT_INIT_HIGH, "lcd_power");
	usleep_range(5000, 6000);
	gpio_free(gpio_power);

	gpio_reset = get_display_dsi_lcd_reset_gpio_exynos();
	gpio_request_one(gpio_reset,
				GPIOF_OUT_INIT_HIGH, "lcd_reset");
	usleep_range(5000, 6000);
	gpio_set_value(gpio_reset, 0);
	usleep_range(5000, 6000);
	gpio_set_value(gpio_reset, 1);
	usleep_range(5000, 6000);
	gpio_free(gpio_reset);

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	pinctrl = devm_pinctrl_get_select(dev, "turnon_tes");
	if (IS_ERR(pinctrl))
		pr_err("failed to get tes pinctrl - ON");
#endif
	return ret;
}

int disable_display_dsi_power_exynos(struct device *dev)
{
	unsigned gpio_power;
	unsigned int gpio_reset;
	int ret = 0;

	/* Reset */
	gpio_reset = get_display_dsi_lcd_reset_gpio_exynos();
	gpio_request_one(gpio_reset,
				GPIOF_OUT_INIT_LOW, "lcd_reset");
	usleep_range(5000, 6000);
	gpio_free(gpio_reset);
	/* Power */
	gpio_power = get_display_dsi_lcd_power_gpio_exynos();
	gpio_request_one(gpio_power,
			GPIOF_OUT_INIT_LOW, "lcd_power");
	usleep_range(5000, 6000);
	gpio_free(gpio_power);

	return ret;
}

int enable_display_fimd_clocks_exynos(struct device *dev)
{
	int ret = 0;
#if 0
	DISPLAY_CLOCK_INLINE_SET_PARENT(sclk_fimd1, mout_fimd1);
	DISPLAY_INLINE_SET_RATE(sclk_fimd1, 67 * MHZ);
#else
	ret = exynos_set_parent("mout_fimd1_mdnie1", "mout_fimd1");
	if (ret < 0)
		pr_err("DISPLAY_CLOCK_SET_PARENT: ret %d\n", ret);
	ret = exynos_set_parent("mout_fimd1", "sclk_rpll");
	if (ret < 0)
		pr_err("DISPLAY_CLOCK_SET_PARENT: ret %d\n", ret);
	ret = exynos_set_rate("sclk_fimd1", 67 * MHZ);
	if (ret < 0)
		pr_err("exynos_set_rate failed: ret %d\n", ret);
#endif

	return ret;
}

int enable_display_dsi_clocks_exynos(struct device *dev)
{
	int ret = 0;
	return ret;
}

int disable_display_fimd_clocks_exynos(struct device *dev)
{
	return 0;
}

int enable_display_fimd_runtimepm_exynos(struct device *dev)
{
	return 0;
}

int disable_display_fimd_runtimepm_exynos(struct device *dev)
{
	return 0;
}

