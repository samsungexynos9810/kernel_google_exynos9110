/* linux/arch/arm/mach-exynos/setup-fimc-is.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <mach/regs-gpio.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/gpio-cfg.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#include <mach/exynos-fimc-is.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_SOC_EXYNOS5430)
#include <mach/regs-clock-exynos5430.h>
#endif

struct platform_device; /* don't need the contents */

#if defined(CONFIG_ARCH_EXYNOS5)
/*------------------------------------------------------*/
/*		Exynos5 series - FIMC-IS		*/
/*------------------------------------------------------*/
int exynos5_fimc_is_print_cfg(struct platform_device *pdev, u32 channel)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* utility function to set parent with names */
int fimc_is_set_parent(const char *child, const char *parent)
{
	struct clk *p;
	struct clk *c;

	p= __clk_lookup(parent);
	if (IS_ERR(p)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, parent);
		return -EINVAL;
	}

	c= __clk_lookup(child);
	if (IS_ERR(c)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return -EINVAL;
	}

	return clk_set_parent(c, p);
}

/* utility function to get parent name with name */
struct clk *fimc_is_get_parent(const char *child)
{
	struct clk *c;

	c = __clk_lookup(child);
	if (IS_ERR(c)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return NULL;
	}

	return clk_get_parent(c);
}

/* utility function to set rate with name */
int fimc_is_set_rate(const char *conid, unsigned int rate)
{
	struct clk *target;

	target = __clk_lookup(conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	return clk_set_rate(target, rate);
}

/* utility function to get rate with name */
unsigned int  fimc_is_get_rate(const char *conid)
{
	struct clk *target;
	unsigned int rate_target;

	target = __clk_lookup(conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	rate_target = clk_get_rate(target);
	pr_debug("%s : %d\n", conid, rate_target);

	return rate_target;
}

/* utility function to eable with name */
unsigned int  fimc_is_enable(const char *conid)
{
	struct clk *target;

	target = __clk_lookup(conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_prepare(target);

	return clk_enable(target);
}

/* utility function to set parent with DT */
int fimc_is_set_parent_dt(struct platform_device *pdev,
	const char *child, const char *parent)
{
	struct clk *p;
	struct clk *c;

	p= clk_get(&pdev->dev, parent);
	if (IS_ERR(p)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, parent);
		return -EINVAL;
	}

	c= clk_get(&pdev->dev, child);
	if (IS_ERR(c)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return -EINVAL;
	}

	return clk_set_parent(c, p);
}

/* utility function to get parent with DT */
struct clk *fimc_is_get_parent_dt(struct platform_device *pdev,
	const char *child)
{
	struct clk *c;

	c = clk_get(&pdev->dev, child);
	if (IS_ERR(c)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return NULL;
	}

	return clk_get_parent(c);
}

/* utility function to set rate with DT */
int fimc_is_set_rate_dt(struct platform_device *pdev,
	const char *conid, unsigned int rate)
{
	struct clk *target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	return clk_set_rate(target, rate);
}

/* utility function to get rate with DT */
unsigned int  fimc_is_get_rate_dt(struct platform_device *pdev,
	const char *conid)
{
	struct clk *target;
	unsigned int rate_target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	rate_target = clk_get_rate(target);
	pr_info("%s : %d\n", conid, rate_target);

	return rate_target;
}

/* utility function to eable with DT */
unsigned int  fimc_is_enable_dt(struct platform_device *pdev,
		const char *conid)
{
	struct clk *target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_prepare(target);

	return clk_enable(target);
}

int exynos5430_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	int cfg = 0;
	u32 value = 0;

	if (clk_gate_id == 0)
		return 0;

	/* CAM00 */
	if (clk_gate_id & (1 << FIMC_IS_GATE_3AA1_IP))
		value |= (1 << 4);
	if (clk_gate_id & (1 << FIMC_IS_GATE_3AA0_IP))
		value |= (1 << 3);

	if (value > 0) {
		cfg = readl(EXYNOS5430_ENABLE_IP_CAM00);
		if (is_on)
			writel(cfg | value, EXYNOS5430_ENABLE_IP_CAM00);
		else
			writel(cfg & ~(value), EXYNOS5430_ENABLE_IP_CAM00);
		pr_debug("%s :1 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}


	/* ISP 0 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_ISP_IP))
		value |= (1 << 0);
	if (clk_gate_id & (1 << FIMC_IS_GATE_DRC_IP))
		value |= (1 << 1);
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCC_IP))
		value |= (1 << 2);
	if (clk_gate_id & (1 << FIMC_IS_GATE_DIS_IP))
		value |= (1 << 3);
	if (clk_gate_id & (1 << FIMC_IS_GATE_3DNR_IP))
		value |= (1 << 4);
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCP_IP))
		value |= (1 << 5);

	if (value > 0) {
		cfg = readl(EXYNOS5430_ENABLE_IP_ISP0);
		if (is_on)
			writel(cfg | value, EXYNOS5430_ENABLE_IP_ISP0);
		else
			writel(cfg & ~(value), EXYNOS5430_ENABLE_IP_ISP0);
		pr_debug("%s :2 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	/* CAM 10 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_FD_IP))
		value |= (1 << 3);

	if (value > 0) {
		cfg = readl(EXYNOS5430_ENABLE_IP_CAM10);
		if (is_on)
			writel(cfg | value, EXYNOS5430_ENABLE_IP_CAM10);
		else
			writel(cfg & ~(value), EXYNOS5430_ENABLE_IP_CAM10);
		pr_debug("%s :3 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}
/*
	pr_info("%s : [%s] gate(%d) (0x%x)\n", __func__,
			is_on ? "ON" : "OFF",
			clk_gate_id,
			cfg);
*/
	return 0;
}

/* utility function to disable with DT */
void  fimc_is_disable_dt(struct platform_device *pdev,
		const char *conid)
{
	struct clk *target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
	}

	clk_disable(target);
	clk_unprepare(target);
}

int cfg_clk_div_max(struct platform_device *pdev)
{
	/* SCLK */
	/* SCLK_SPI0 */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0", "oscclk");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_a", 1);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_b", 1);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0_user", "oscclk");

	/* SCLK_SPI1 */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi1", "oscclk");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi1_a", 1);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi1_b", 1);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi1_user", "oscclk");

	/* SCLK_UART */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_uart", "oscclk");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_uart", 1);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_uart_user", "oscclk");

	/* CAM0 */
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "oscclk");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "oscclk");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "oscclk");
	fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s4", "oscclk");
	fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2a", "oscclk");
	fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2b", "oscclk");
	/* LITE A */
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_a", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_a", 1);
	/* LITE B */
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_b", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_b", 1);
	/* LITE D */
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_d", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_d", 1);
	/* LITE C PIXELASYNC */
	fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c_init", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_pixelasync_lite_c", 1);
	fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c", 1);
	/* 3AA 0 */
	fimc_is_set_rate_dt(pdev, "dout_aclk_3aa0", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_3aa0", 1);
	/* 3AA 0 */
	fimc_is_set_rate_dt(pdev, "dout_aclk_3aa1", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_3aa1", 1);
	/* CSI 0 */
	fimc_is_set_rate_dt(pdev, "dout_aclk_csis0", 1);
	/* CSI 1 */
	fimc_is_set_rate_dt(pdev, "dout_aclk_csis1", 1);
	/* CAM0 400 */
	fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_400", 1);
	fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_200", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam0_50", 1);

	/* CAM1 */
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_552_user", "oscclk");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_400_user", "oscclk");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_333_user", "oscclk");
	/* C-A5 */
	fimc_is_set_rate_dt(pdev, "dout_atclk_cam1", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_dbg_cam1", 1);
	/* LITE A */
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_c", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_c", 1);
	/* FD */
	fimc_is_set_rate_dt(pdev, "dout_aclk_fd", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_fd", 1);
	/* CSI 2 */
	fimc_is_set_rate_dt(pdev, "dout_aclk_csis2_a", 1);

	/* CMU_ISP */
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_isp_400_user", "oscclk");
	fimc_is_set_parent_dt(pdev, "mout_aclk_isp_dis_400_user", "oscclk");
	/* ISP */
	fimc_is_set_rate_dt(pdev, "dout_aclk_isp_c_200", 1);
	fimc_is_set_rate_dt(pdev, "dout_aclk_isp_d_200", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_isp", 1);
	/* DIS */
	fimc_is_set_rate_dt(pdev, "dout_pclk_isp_dis", 1);

	return 0;
}

int cfg_clk_sclk(struct platform_device *pdev)
{
	/* SCLK_SPI0 */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0", "oscclk");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_a", 24 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_b", 24 * 1000000);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0_user", "sclk_isp_spi0_top");

	/* SCLK_SPI1 */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi1", "oscclk");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi1_a", 24 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi1_b", 24 * 1000000);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi1_user", "sclk_isp_spi1_top");

	/* SCLK_UART */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_uart", "mout_bus_pll_user");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_uart", 207 * 1000000);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_uart_user", "sclk_isp_uart_top");

	return 0;
}

int cfg_clk_cam0(struct platform_device *pdev)
{
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "aclk_cam0_552");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "aclk_cam0_400");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "aclk_cam0_333");
	fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s4", "phyclk_rxbyteclkhs0_s4");
	fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2a", "phyclk_rxbyteclkhs0_s2a");
	fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2b", "phyclk_rxbyteclkhs0_s2b");

	/* LITE A */
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_a_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_a_b", "mout_aclk_lite_a_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_a", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_a", 276 * 1000000);

	/* LITE B */
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_b_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_b_b", "mout_aclk_lite_b_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_b", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_b", 276 * 1000000);

	/* LITE D */
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_d_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_d_b", "mout_aclk_lite_d_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_d", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_d", 276 * 1000000);

	/* LITE C PIXELASYNC */
	fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_init_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_init_b", "mout_sclk_pixelasync_lite_c_init_a");
	fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c_init", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_pixelasync_lite_c", 276 * 1000000);

	fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_b", "mout_aclk_cam0_333_user");
	fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c", 333 * 1000000);

	/* 3AA 0 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_3aa0_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_3aa0_b", "mout_aclk_3aa0_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_3aa0", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_3aa0", 276 * 1000000);

	/* 3AA 0 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_3aa1_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_3aa1_b", "mout_aclk_3aa1_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_3aa1", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_3aa1", 276 * 1000000);

	/* CSI 0 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis0_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis0_b", "mout_aclk_csis0_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_csis0", 552 * 1000000);

	/* CSI 1 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis1_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis1_b", "mout_aclk_csis1_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_csis1", 552 * 1000000);

	/* CAM0 400 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400", "mout_aclk_cam0_400_user");
	fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_400", 413 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_200", 207 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam0_50", 52 * 1000000);

	return 0;
}

int cfg_clk_cam1(struct platform_device *pdev)
{
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_552_user", "aclk_cam1_552");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_400_user", "aclk_cam1_400");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_333_user", "aclk_cam1_333");

	/* C-A5 */
	fimc_is_set_rate_dt(pdev, "dout_atclk_cam1", 276 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_dbg_cam1", 138 * 1000000);

	/* LITE A */
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_c_a", "mout_aclk_cam1_400_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_c_b", "mout_aclk_cam1_333_user");
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_c", 333 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_c", 166 * 1000000);

	/* FD */
	fimc_is_set_parent_dt(pdev, "mout_aclk_fd_a", "mout_aclk_cam1_400_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_fd_b", "mout_aclk_fd_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_fd", 413 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_fd", 207 * 1000000);

	/* CSI 2 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis2_a", "mout_aclk_cam1_400_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis2_b", "mout_aclk_cam1_333_user");
	fimc_is_set_rate_dt(pdev, "dout_aclk_csis2_a", 333 * 1000000);

	/* MPWM */
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam1_166", 166 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam1_83", 83 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_mpwm", 83 * 1000000);

	return 0;
}

int cfg_clk_isp(struct platform_device *pdev)
{
	/* CMU_ISP */
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_isp_400_user", "aclk_isp_400");
	fimc_is_set_parent_dt(pdev, "mout_aclk_isp_dis_400_user", "aclk_isp_dis_400");
	/* ISP */
	fimc_is_set_rate_dt(pdev, "dout_aclk_isp_c_200", 207 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_aclk_isp_d_200", 207 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_isp", 83 * 1000000);
	/* DIS */
	fimc_is_set_rate_dt(pdev, "dout_pclk_isp_dis", 207 * 1000000);

	return 0;
}

int exynos5430_fimc_is_cfg_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	cfg_clk_div_max(pdev);

	/* initialize Clocks */
	cfg_clk_sclk(pdev);
	cfg_clk_cam0(pdev);
	cfg_clk_cam1(pdev);
	cfg_clk_isp(pdev);

	//exynos5430_fimc_is_print_clk(pdev);

	return 0;
}

int exynos5430_fimc_is_clk_on(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_clk_off(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_print_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	/* SCLK */
	/* SCLK_SPI0 */
	fimc_is_get_rate_dt(pdev, "sclk_isp_spi0_top");
	fimc_is_get_rate_dt(pdev, "sclk_isp_spi0");
	/* SCLK_SPI1 */
	fimc_is_get_rate_dt(pdev, "sclk_isp_spi1_top");
	fimc_is_get_rate_dt(pdev, "sclk_isp_spi1");
	/* SCLK_UART */
	fimc_is_get_rate_dt(pdev, "sclk_isp_uart_top");
	fimc_is_get_rate_dt(pdev, "sclk_isp_uart");

	/* CAM0 */
	/* CMU_TOP */
	fimc_is_get_rate_dt(pdev, "aclk_cam0_552");
	fimc_is_get_rate_dt(pdev, "aclk_cam0_400");
	fimc_is_get_rate_dt(pdev, "aclk_cam0_333");
	/* LITE A */
	fimc_is_get_rate_dt(pdev, "dout_aclk_lite_a");
	fimc_is_get_rate_dt(pdev, "dout_pclk_lite_a");
	/* LITE B */
	fimc_is_get_rate_dt(pdev, "dout_aclk_lite_b");
	fimc_is_get_rate_dt(pdev, "dout_pclk_lite_b");
	/* LITE D */
	fimc_is_get_rate_dt(pdev, "dout_aclk_lite_d");
	fimc_is_get_rate_dt(pdev, "dout_pclk_lite_d");
	/* LITE C PIXELASYNC */
	fimc_is_get_rate_dt(pdev, "dout_sclk_pixelasync_lite_c_init");
	fimc_is_get_rate_dt(pdev, "dout_pclk_pixelasync_lite_c");
	fimc_is_get_rate_dt(pdev, "dout_sclk_pixelasync_lite_c");
	/* 3AA 0 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_3aa0");
	fimc_is_get_rate_dt(pdev, "dout_pclk_3aa0");
	/* 3AA 0 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_3aa1");
	fimc_is_get_rate_dt(pdev, "dout_pclk_3aa1");
	/* CSI 0 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_csis0");
	/* CSI 1 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_csis1");
	/* CAM0 400 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_400");
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_200");
	fimc_is_get_rate_dt(pdev, "dout_pclk_cam0_50");

	/* CAM1 */
	/* CMU_TOP */
	fimc_is_get_rate_dt(pdev, "aclk_cam1_552");
	fimc_is_get_rate_dt(pdev, "aclk_cam1_400");
	fimc_is_get_rate_dt(pdev, "aclk_cam1_333");
	/* C-A5 */
	fimc_is_get_rate_dt(pdev, "dout_atclk_cam1");
	fimc_is_get_rate_dt(pdev, "dout_pclk_dbg_cam1");
	/* LITE A */
	fimc_is_get_rate_dt(pdev, "dout_aclk_lite_c");
	fimc_is_get_rate_dt(pdev, "dout_pclk_lite_c");
	/* FD */
	fimc_is_get_rate_dt(pdev, "dout_aclk_fd");
	fimc_is_get_rate_dt(pdev, "dout_pclk_fd");
	/* CSI 2 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_csis2_a");
	/* MPWM */
	fimc_is_get_rate_dt(pdev, "dout_pclk_cam1_166");
	fimc_is_get_rate_dt(pdev, "dout_pclk_cam1_83");
	fimc_is_get_rate_dt(pdev, "dout_sclk_isp_mpwm");

	/* ISP */
	/* CMU_TOP */
	fimc_is_get_rate_dt(pdev, "aclk_isp_400");
	fimc_is_get_rate_dt(pdev, "aclk_isp_dis_400");
	/* ISP */
	fimc_is_get_rate_dt(pdev, "dout_aclk_isp_c_200");
	fimc_is_get_rate_dt(pdev, "dout_aclk_isp_d_200");
	fimc_is_get_rate_dt(pdev, "dout_pclk_isp");
	/* DIS */
	fimc_is_get_rate_dt(pdev, "dout_pclk_isp_dis");

	/* CMU_TOP_DUMP */
	pr_info("EXYNOS5430_SRC_SEL_TOP1(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_TOP1));
	pr_info("EXYNOS5430_SRC_SEL_TOP2(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_TOP2));
	pr_info("EXYNOS5430_SRC_SEL_TOP_CAM1(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_TOP_CAM1));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP0(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP0));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP1(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP1));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP2(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP2));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP3(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP3));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP_CAM1(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP_CAM1));
	pr_info("EXYNOS5430_DIV_TOP0(0x%08X)\n", readl(EXYNOS5430_DIV_TOP0));
	pr_info("EXYNOS5430_DIV_TOP_CAM10(0x%08X)\n", readl(EXYNOS5430_DIV_TOP_CAM10));
	pr_info("EXYNOS5430_DIV_TOP_CAM11(0x%08X)\n", readl(EXYNOS5430_DIV_TOP_CAM11));
	pr_info("EXYNOS5430_ENABLE_IP_TOP(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_TOP));
	/* CMU_CAM0_DUMP */
	pr_info("EXYNOS5430_SRC_SEL_CAM00(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM00));
	pr_info("EXYNOS5430_SRC_SEL_CAM01(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM01));
	pr_info("EXYNOS5430_SRC_SEL_CAM02(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM02));
	pr_info("EXYNOS5430_SRC_SEL_CAM03(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM03));
	pr_info("EXYNOS5430_SRC_SEL_CAM04(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM04));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM00(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM00));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM01(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM01));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM02(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM02));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM03(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM03));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM04(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM04));
	pr_info("EXYNOS5430_SRC_STAT_CAM00(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM00));
	pr_info("EXYNOS5430_SRC_STAT_CAM01(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM01));
	pr_info("EXYNOS5430_SRC_STAT_CAM02(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM02));
	pr_info("EXYNOS5430_SRC_STAT_CAM03(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM03));
	pr_info("EXYNOS5430_SRC_STAT_CAM04(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM04));
	pr_info("EXYNOS5430_SRC_IGNORE_CAM01(0x%08X)\n", readl(EXYNOS5430_SRC_IGNORE_CAM01));
	pr_info("EXYNOS5430_DIV_CAM00(0x%08X)\n", readl(EXYNOS5430_DIV_CAM00));
	pr_info("EXYNOS5430_DIV_CAM01(0x%08X)\n", readl(EXYNOS5430_DIV_CAM01));
	pr_info("EXYNOS5430_DIV_CAM02(0x%08X)\n", readl(EXYNOS5430_DIV_CAM02));
	pr_info("EXYNOS5430_DIV_CAM03(0x%08X)\n", readl(EXYNOS5430_DIV_CAM03));
	pr_info("EXYNOS5430_DIV_STAT_CAM00(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM00));
	pr_info("EXYNOS5430_DIV_STAT_CAM01(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM01));
	pr_info("EXYNOS5430_DIV_STAT_CAM02(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM02));
	pr_info("EXYNOS5430_DIV_STAT_CAM03(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM03));
	pr_info("EXYNOS5430_ENABLE_IP_CAM00(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM00));
	pr_info("EXYNOS5430_ENABLE_IP_CAM01(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM01));
	pr_info("EXYNOS5430_ENABLE_IP_CAM02(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM02));
	pr_info("EXYNOS5430_ENABLE_IP_CAM03(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM03));
	/* CMU_CAM1_DUMP */
	pr_info("EXYNOS5430_SRC_SEL_CAM10(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM10));
	pr_info("EXYNOS5430_SRC_SEL_CAM11(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM11));
	pr_info("EXYNOS5430_SRC_SEL_CAM12(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM12));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM10(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM10));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM11(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM11));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM12(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM12));
	pr_info("EXYNOS5430_SRC_STAT_CAM10(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM10));
	pr_info("EXYNOS5430_SRC_STAT_CAM11(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM11));
	pr_info("EXYNOS5430_SRC_STAT_CAM12(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM12));
	pr_info("EXYNOS5430_SRC_IGNORE_CAM11(0x%08X)\n", readl(EXYNOS5430_SRC_IGNORE_CAM11));
	pr_info("EXYNOS5430_DIV_CAM10(0x%08X)\n", readl(EXYNOS5430_DIV_CAM10));
	pr_info("EXYNOS5430_DIV_CAM11(0x%08X)\n", readl(EXYNOS5430_DIV_CAM11));
	pr_info("EXYNOS5430_DIV_STAT_CAM10(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM10));
	pr_info("EXYNOS5430_DIV_STAT_CAM11(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM11));
	pr_info("EXYNOS5430_ENABLE_IP_CAM10(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM10));
	pr_info("EXYNOS5430_ENABLE_IP_CAM11(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM11));
	pr_info("EXYNOS5430_ENABLE_IP_CAM12(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM12));
	/* CMU_ISP_DUMP */
	pr_info("EXYNOS5430_SRC_SEL_ISP(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_ISP));
	pr_info("EXYNOS5430_SRC_ENABLE_ISP(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_ISP));
	pr_info("EXYNOS5430_SRC_STAT_ISP(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_ISP));
	pr_info("EXYNOS5430_DIV_ISP(0x%08X)\n", readl(EXYNOS5430_DIV_ISP));
	pr_info("EXYNOS5430_DIV_STAT_ISP(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_ISP));
	pr_info("EXYNOS5430_ENABLE_ACLK_ISP0(0x%08X)\n", readl(EXYNOS5430_ENABLE_ACLK_ISP0));
	pr_info("EXYNOS5430_ENABLE_ACLK_ISP1(0x%08X)\n", readl(EXYNOS5430_ENABLE_ACLK_ISP1));
	pr_info("EXYNOS5430_ENABLE_ACLK_ISP2(0x%08X)\n", readl(EXYNOS5430_ENABLE_ACLK_ISP2));
	pr_info("EXYNOS5430_ENABLE_PCLK_ISP(0x%08X)\n", readl(EXYNOS5430_ENABLE_PCLK_ISP));
	pr_info("EXYNOS5430_ENABLE_SCLK_ISP(0x%08X)\n", readl(EXYNOS5430_ENABLE_SCLK_ISP));
	pr_info("EXYNOS5430_ENABLE_IP_ISP0(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP0));
	pr_info("EXYNOS5430_ENABLE_IP_ISP1(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP1));
	pr_info("EXYNOS5430_ENABLE_IP_ISP2(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP2));
	pr_info("EXYNOS5430_ENABLE_IP_ISP3(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP3));
	/* CMU_ENABLE_DUMP */
	pr_info("EXYNOS5430_ENABLE_SCLK_TOP_CAM1(0x%08X)\n", readl(EXYNOS5430_ENABLE_SCLK_TOP_CAM1));
	pr_info("EXYNOS5430_ENABLE_IP_TOP(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_TOP));
	pr_info("EXYNOS5430_ENABLE_IP_CAM00(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM00));
	pr_info("EXYNOS5430_ENABLE_IP_CAM01(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM01));
	pr_info("EXYNOS5430_ENABLE_IP_CAM02(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM02));
	pr_info("EXYNOS5430_ENABLE_IP_CAM03(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM03));
	pr_info("EXYNOS5430_ENABLE_IP_CAM10(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM10));
	pr_info("EXYNOS5430_ENABLE_IP_CAM11(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM11));
	pr_info("EXYNOS5430_ENABLE_IP_CAM12(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM12));
	pr_info("EXYNOS5430_ENABLE_IP_ISP0(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP0));
	pr_info("EXYNOS5430_ENABLE_IP_ISP1(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP1));
	pr_info("EXYNOS5430_ENABLE_IP_ISP2(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP2));
	pr_info("EXYNOS5430_ENABLE_IP_ISP3(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP3));

	return 0;
}

/* sequence is important, don't change order */
int exynos5_fimc_is_sensor_power_on(struct platform_device *pdev, int sensor_id)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* sequence is important, don't change order */
int exynos5_fimc_is_sensor_power_off(struct platform_device *pdev, int sensor_id)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_set_user_clk_gate(u32 group_id,
		bool is_on,
		u32 user_scenario_id,
		unsigned long msk_state,
		struct exynos_fimc_is_clk_gate_info *gate_info) {
	/* if you want to skip clock on/off, let this func return -1 */
	int ret = -1;

	switch (user_scenario_id) {
	case CLK_GATE_NOT_FULL_BYPASS_SN:
		if (is_on == true)
			gate_info->groups[group_id].mask_clk_on_mod &=
				~((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		else
			gate_info->groups[group_id].mask_clk_on_mod |=
				((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		ret = 0;
		break;
	case CLK_GATE_DIS_SN:
		if (is_on == true)
			gate_info->groups[group_id].mask_clk_on_mod |=
				((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		else
			gate_info->groups[group_id].mask_clk_on_mod &=
				~((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		ret = 0;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}
#endif
