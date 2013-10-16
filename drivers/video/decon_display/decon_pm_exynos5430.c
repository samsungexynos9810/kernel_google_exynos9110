/* linux/drivers/video/decon_display/decon_pm_exynos5430.c
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
#include "decon_fb.h"
#include "decon_mipi_dsi.h"
#include "decon_dt.h"

#include <../drivers/clk/samsung/clk.h>

static struct clk *g_mout_sclk_decon_eclk_a, *g_mout_bus_pll_sub;
static struct clk *g_mout_disp_pll, *g_fout_disp_pll;
static struct clk *g_aclk_disp_333, *g_mout_aclk_disp_333_user;
static struct clk *g_aclk_disp_222, *g_mout_aclk_disp_222_user;
static struct clk *g_sclk_decon_eclk_mif, *g_mout_sclk_decon_eclk_user;
static struct clk *g_sclk_decon_vclk_mif, *g_mout_sclk_decon_vclk_user;
static struct clk *g_sclk_dsd_mif, *g_mout_sclk_dsd_user;
static struct clk *g_mout_sclk_decon_eclk_user, *g_mout_sclk_decon_eclk_disp;

static struct clk *g_dout_aclk_disp_333;
static struct clk *g_dout_sclk_decon_eclk;

static struct clk *g_mout_sclk_dsd_a, *g_dout_mfc_pll;

static struct clk *g_phyclk_mipidphy_rxclkesc0_phy,
	*g_mout_phyclk_mipidphy_rxclkesc0_user;
static struct clk *g_phyclk_mipidphy_bitclkdiv8_phy,
	*g_mout_phyclk_mipidphy_bitclkdiv8_user;

#define DISP_RUNTIME_PM_DEBUG

#ifdef DISP_RUNTIME_PM_DEBUG
static int g_debug_pm_count;
#endif

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

#define TEMPORARY_RECOVER_CMU(addr, mask, bits, value) do {\
	regs = ioremap(addr, 0x4); \
	data = readl(regs) & ~((mask) << (bits)); \
	data |= ((value & mask) << (bits)); \
	writel(data, regs); \
	} while (0)

#define pm_debug(params, ...) pr_err("[DISP:PM] " params, ##__VA_ARGS__)

static void additional_clock_setup(void)
{
	void __iomem *regs;

	/* Set: ignore CLK1 was toggling */
	regs = ioremap(0x13B90508, 0x4);
	writel(0, regs);
	iounmap(regs);
}

static void check_display_clocks(void)
{
	void __iomem *regs;
	u32 data = 0x00;

	/* Now check mux values */
	DISPLAY_CHECK_REGS(0x105B0210, 0x00000001, 0x00000001);
	DISPLAY_CHECK_REGS(0x13B90200, 0x00000001, 0x00000001);
	DISPLAY_CHECK_REGS(0x13B90204, 0x000FFFFF, 0x00011111);
	DISPLAY_CHECK_REGS(0x13B90208, 0x00001100, 0x00001100);
	DISPLAY_CHECK_REGS(0x13B9020C, 0x00000001, 0x00000001);

	/* Now check divider value */
	DISPLAY_CHECK_REGS(0x105B060C, 0x00000070, 0x00000020);
}

int init_display_decon_clocks_exynos5430(struct device *dev)
{
	int ret = 0;

	DISPLAY_CLOCK_SET_PARENT(mout_sclk_decon_eclk_a, mout_bus_pll_sub);
	DISPLAY_CLOCK_SET_PARENT(mout_disp_pll, fout_disp_pll);

	DISPLAY_CLOCK_SET_PARENT(mout_aclk_disp_333_user, aclk_disp_333);
	DISPLAY_CLOCK_SET_PARENT(mout_aclk_disp_222_user, aclk_disp_222);
	DISPLAY_CLOCK_SET_PARENT(mout_sclk_decon_eclk_user,
		sclk_decon_eclk_mif);
	DISPLAY_CLOCK_SET_PARENT(mout_sclk_decon_vclk_user,
		sclk_decon_vclk_mif);
	DISPLAY_CLOCK_SET_PARENT(mout_sclk_dsd_user, sclk_dsd_mif);

	DISPLAY_CLOCK_SET_PARENT(mout_sclk_decon_eclk_disp,
		mout_sclk_decon_eclk_user);

	/* setup dsd clock for MFC */
	DISPLAY_CLOCK_SET_PARENT(mout_sclk_dsd_a, dout_mfc_pll);

	DISPLAY_SET_RATE(dout_aclk_disp_333, 222*MHZ);
	DISPLAY_SET_RATE(dout_sclk_decon_eclk, 400*MHZ);

	additional_clock_setup();

	check_display_clocks();
	return ret;
}

int init_display_dsi_clocks_exynos5430(struct device *dev)
{
	int ret = 0;
	void __iomem *regs;

	/* Set DISP_PLL = 136Mhz */
	regs = ioremap(0x13B90100, 0x4);
	writel(0xA0880303, regs);
	iounmap(regs);
	msleep(20);

	DISPLAY_CLOCK_SET_PARENT(mout_phyclk_mipidphy_rxclkesc0_user,
		phyclk_mipidphy_rxclkesc0_phy);
	DISPLAY_CLOCK_SET_PARENT(mout_phyclk_mipidphy_bitclkdiv8_user,
		phyclk_mipidphy_bitclkdiv8_phy);

	return ret;
}

int enable_display_dsi_power_exynos5430(struct device *dev)
{
	unsigned gpio_power;
#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	struct pinctrl *pinctrl;
#endif
	int id;
	int ret = 0;

	gpio_power = get_display_dsi_reset_gpio();
	id = gpio_request(gpio_power, "lcd_power");
	if (id < 0) {
		pr_err("Failed to get gpio number for the lcd power\n");
		return -EINVAL;
	}
	gpio_direction_output(gpio_power, 0x01);

	gpio_set_value(gpio_power, 1);
	usleep_range(5000, 6000);

	gpio_set_value(gpio_power, 0);
	usleep_range(5000, 6000);
	msleep(20);

	gpio_set_value(gpio_power, 1);
	usleep_range(5000, 6000);

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	pinctrl = devm_pinctrl_get_select(dev, "turnon_tes");
	if (IS_ERR(pinctrl))
		pr_err("failed to get tes pinctrl - ON");
#endif

	gpio_free(gpio_power);
	return ret;
}

int disable_display_dsi_power_exynos5430(struct device *dev)
{
	unsigned gpio_power;
#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	struct pinctrl *pinctrl;
#endif
	int id;
	int ret = 0;

	gpio_power = get_display_dsi_reset_gpio();
	id = gpio_request(gpio_power, "lcd_power");
	if (id < 0) {
		pr_err("Failed to get gpio number for the lcd power\n");
		return -EINVAL;
	}
	gpio_set_value(gpio_power, 0);
	usleep_range(5000, 6000);

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	pinctrl = devm_pinctrl_get_select(dev, "turnoff_tes");
	if (IS_ERR(pinctrl))
		pr_err("failed to get tes pinctrl - OFF");
#endif
	gpio_free(gpio_power);

	return ret;
}

#ifdef FAST_CMU_CLOCK_RECOVER
static void temporary_cmu_restore(void)
{
	void __iomem *regs;
	u32 data;

	TEMPORARY_RECOVER_CMU(0x13B90100, 0xFFFFFFFF, 0, 0xA0880303);
	msleep(20);
	TEMPORARY_RECOVER_CMU(0x105B0120, 0xFFFFFFFF, 0, 0xA0DE0400);
	msleep(20);
	TEMPORARY_RECOVER_CMU(0x105B0210, 0x1, 0, 0x01);
	TEMPORARY_RECOVER_CMU(0x105B060C, 0x7, 4, 0x02);
	TEMPORARY_RECOVER_CMU(0x105B0610, 0xF, 0, 0x02);
	TEMPORARY_RECOVER_CMU(0x13B90200, 0xFFFFFFFF, 0, 0x01);
	TEMPORARY_RECOVER_CMU(0x13B90204, 0x11111, 0, 0x11111);
	TEMPORARY_RECOVER_CMU(0x13B90208, 0x1100, 0, 0x1100);
	TEMPORARY_RECOVER_CMU(0x13B9020C, 0xFFFFFFFF, 0, 0x1);
}
#endif

int enable_display_decon_clocks_exynos5430(struct device *dev)
{
	int ret = 0;

	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_eclk_a,
		mout_bus_pll_sub);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_disp_pll, fout_disp_pll);

	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_disp_333_user,
		aclk_disp_333);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_disp_222_user,
		aclk_disp_222);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_eclk_user,
		sclk_decon_eclk_mif);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_vclk_user,
		sclk_decon_vclk_mif);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_dsd_user, sclk_dsd_mif);

	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_eclk_disp,
		mout_sclk_decon_eclk_user);

	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_dsd_a, dout_mfc_pll);

	DISPLAY_INLINE_SET_RATE(dout_aclk_disp_333, 222*MHZ);
	DISPLAY_INLINE_SET_RATE(dout_sclk_decon_eclk, 400*MHZ);

	additional_clock_setup();
	return ret;
}

int enable_display_dsi_clocks_exynos5430(struct device *dev)
{
	int ret = 0;
	void __iomem *regs;
	u32 data;
#ifdef FAST_CMU_CLOCK_RECOVER
	temporary_cmu_restore();
#else
	TEMPORARY_RECOVER_CMU(0x13B90100, 0xFFFFFFFF, 0, 0xA0880303);
	enable_display_decon_clocks_exynos5430(NULL);

	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_phyclk_mipidphy_rxclkesc0_user,
		phyclk_mipidphy_rxclkesc0_phy);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_phyclk_mipidphy_bitclkdiv8_user,
		phyclk_mipidphy_bitclkdiv8_phy);
#endif

	check_display_clocks();

	return ret;
}

int disable_display_decon_clocks_exynos5430(struct device *dev)
{
	return 0;
}

int enable_display_decon_runtimepm_exynos5430(struct device *dev)
{
	return 0;
}

int disable_display_decon_runtimepm_exynos5430(struct device *dev)
{
	return 0;
}

int disp_pm_runtime_enable(struct display_driver *dispdrv)
{
#ifdef DISP_RUNTIME_PM_DEBUG
	pm_debug("runtime pm for disp-driver enabled\n");
#endif
	pm_runtime_enable(dispdrv->display_driver);
	return 0;
}

int disp_pm_runtime_get_sync(struct display_driver *dispdrv)
{
	pm_runtime_get_sync(dispdrv->display_driver);
	return 0;
}

int disp_pm_runtime_put_sync(struct display_driver *dispdrv)
{
	pm_runtime_put_sync(dispdrv->display_driver);
	return 0;
}

#ifdef DISP_RUNTIME_PM_DEBUG
void disp_debug_power_info(void)
{
	pm_debug("pm count : %d\n", g_debug_pm_count);
}
#endif

