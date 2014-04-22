/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5433 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/regs-clock-exynos5433.h>
#include <mach/regs-pmu.h>
#include <mach/regs-audss.h>
#include <mach/map.h>

#include "clk.h"
#include "clk-pll.h"

struct clk_enabler {
	struct clk		*clk;
	struct list_head	node;
};

static LIST_HEAD(clk_enabler_list);

static inline void add_enabler(const char *name)
{
	struct clk_enabler *ce;
	struct clk *clock;
	clock = __clk_lookup(name);

	if (IS_ERR(clock))
		return;

	ce = kzalloc(sizeof(struct clk_enabler), GFP_KERNEL);
	if (!ce)
		return;

	ce->clk = clock;
	list_add(&ce->node, &clk_enabler_list);
}

static void top_clk_enable(void)
{
	struct clk_enabler *ce;

	add_enabler("sclk_mfc_pll");
	add_enabler("sclk_bus_pll");
	add_enabler("sclk_mphy_pll");
	add_enabler("aclk_cpif_200");
	add_enabler("aclk_isp_dis_400");
	add_enabler("aclk_isp_400");
	add_enabler("aclk_cam0_552");
	add_enabler("aclk_cam0_400");
	add_enabler("aclk_cam0_333");
	add_enabler("aclk_cam1_552");
	add_enabler("aclk_cam1_400");
	add_enabler("aclk_cam1_333");
	add_enabler("aclk_gscl_333");
	add_enabler("aclk_gscl_111");
	add_enabler("aclk_mscl_400");
	add_enabler("sclk_jpeg_mscl");
	add_enabler("aclk_g2d_400");
	add_enabler("aclk_g2d_266");
	add_enabler("aclk_hevc_400");
	add_enabler("aclk_disp_333");
	add_enabler("aclk_bus0_400");
	add_enabler("aclk_bus1_400");
	add_enabler("aclk_bus2_400");

	list_for_each_entry(ce, &clk_enabler_list, node) {
		clk_prepare(ce->clk);
		clk_enable(ce->clk);
	}

	pr_info("Clock enables : TOP, MIF\n");
}
static void usb_init_clock(void)
{
	exynos_set_parent("mout_sclk_usbdrd30_user", "oscclk");

	exynos_set_parent("mout_phyclk_usbdrd30_udrd30_phyclock",
			"phyclk_usbdrd30_udrd30_phyclock_phy");
	exynos_set_parent("mout_phyclk_usbdrd30_udrd30_pipe_pclk",
			"phyclk_usbdrd30_udrd30_pipe_pclk_phy");
}

void crypto_init_clock(void)
{
	exynos_set_rate("dout_aclk_imem_sssx_266", 160 * 1000000);
	exynos_set_rate("dout_aclk_imem_200", 160 * 1000000);
}

static void pcie_init_clock(void)
{
	exynos_set_parent("mout_sclk_pcie_100", "mout_bus_pll_user");
	exynos_set_parent("dout_sclk_pcie_100", "mout_sclk_pcie_100");
	exynos_set_parent("sclk_pcie_100_fsys", "dout_sclk_pcie_100");
	exynos_set_parent("mout_sclk_pcie_100_user", "sclk_pcie_100_fsys");
	exynos_set_rate("dout_sclk_pcie_100", 100000000);
}

void g2d_init_clock(void)
{
	int clk_rate1;
	int clk_rate2;

	if (exynos_set_parent("mout_aclk_g2d_400_a", "mout_bus_pll_user"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_400_a", "mout_bus_pll_user");

	if (exynos_set_parent("mout_aclk_g2d_400_b", "mout_aclk_g2d_400_a"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_400_b", "mout_aclk_g2d_400_a");

	if (exynos_set_parent("mout_aclk_g2d_400_user", "aclk_g2d_400"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_400_user", "aclk_g2d_400");

	if (exynos_set_parent("mout_aclk_g2d_266_user", "aclk_g2d_266"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_266_user", "aclk_g2d_266");

	if (exynos_set_rate("dout_aclk_g2d_400", 413 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_aclk_g2d_400");

	if (exynos_set_rate("dout_aclk_g2d_266", 276 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_aclk_g2d_266");

	clk_rate1 = exynos_get_rate("aclk_g2d_400");
	clk_rate2 = exynos_get_rate("aclk_g2d_266");

	pr_info("[%s:%d] aclk_g2d_400:%d, aclk_g2d_266:%d\n"
			, __func__, __LINE__, clk_rate1, clk_rate2);
}

void jpeg_init_clock(void)
{
	if (exynos_set_parent("mout_sclk_jpeg_a", "mout_bus_pll_user"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_bus_pll_user", "mout_sclk_jpeg_a");

	if (exynos_set_parent("mout_sclk_jpeg_b", "mout_sclk_jpeg_a"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpog_a", "mout_sclk_jpeg_b");

	if (exynos_set_parent("mout_sclk_jpeg_c", "mout_sclk_jpeg_b"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_b", "mout_sclk_jpeg_c");

	if (exynos_set_parent("dout_sclk_jpeg", "mout_sclk_jpeg_c"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_c", "dout_sclk_jpeg");

	if (exynos_set_parent("sclk_jpeg_mscl", "dout_sclk_jpeg"))
		pr_err("Unable to set parent %s of clock %s\n",
				"dout_sclk_jpeg", "sclk_jpeg_mscl");

	if (exynos_set_parent("mout_sclk_jpeg_user", "sclk_jpeg_mscl"))
		pr_err("Unable to set parent %s of clock %s\n",
				"sclk_jpeg_mscl", "mout_sclk_jpeg_user");

	if (exynos_set_rate("dout_sclk_jpeg", 413 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_sclk_jpeg");

	pr_debug("jpeg: sclk_jpeg %d\n", exynos_get_rate("dout_sclk_jpeg"));
}

static void clkout_init_clock(void)
{
	writel(0x1000, EXYNOS_PMU_DEBUG);
}

static void aud_init_clock(void)
{
	/* AUD0 */
	exynos_set_parent("mout_aud_pll_user", "fout_aud_pll");

	/* AUD1 */
	exynos_set_parent("mout_sclk_aud_i2s", "mout_aud_pll_user");
	exynos_set_parent("mout_sclk_aud_pcm", "mout_aud_pll_user");

	exynos_set_rate("fout_aud_pll", 196608010);
	exynos_set_rate("dout_aud_ca5", 196608010);
	exynos_set_rate("dout_aclk_aud", 65536010);
	exynos_set_rate("dout_pclk_dbg_aud", 32768010);

	exynos_set_rate("dout_sclk_aud_i2s", 49152004);
	exynos_set_rate("dout_sclk_aud_pcm", 2048002);
	exynos_set_rate("dout_sclk_aud_slimbus", 24576002);
	exynos_set_rate("dout_sclk_aud_uart", 196608010);

	/* TOP1 */
	exynos_set_parent("mout_aud_pll", "fout_aud_pll");
	exynos_set_parent("mout_aud_pll_user_top", "mout_aud_pll");

	/* TOP_PERIC1 */
	exynos_set_parent("mout_sclk_audio0", "mout_aud_pll_user_top");
	exynos_set_parent("mout_sclk_audio1", "mout_aud_pll_user_top");
	exynos_set_parent("mout_sclk_spdif", "dout_sclk_audio0");
	exynos_set_rate("dout_sclk_audio0", 24576002);
	exynos_set_rate("dout_sclk_audio1", 49152004);
	exynos_set_rate("dout_sclk_pcm1", 2048002);
	exynos_set_rate("dout_sclk_i2s1", 49152004);
}

void adma_init_clock(void)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS_LPASSREG(0x8));
	reg &= ~0x1;
	__raw_writel(reg, EXYNOS_LPASSREG(0x8));
	reg |= 0x1;
	__raw_writel(reg, EXYNOS_LPASSREG(0x8));
}

static void spi_init_clock(void)
{
	exynos_set_parent("mout_sclk_spi0", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi1", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi2", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi3", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi4", "mout_bus_pll_user");

	/* dout_sclk_spi_a should be 100Mhz */
	exynos_set_rate("dout_sclk_spi0_a", 100000000);
	exynos_set_rate("dout_sclk_spi1_a", 100000000);
	exynos_set_rate("dout_sclk_spi2_a", 100000000);
	exynos_set_rate("dout_sclk_spi3_a", 100000000);
	exynos_set_rate("dout_sclk_spi4_a", 100000000);
}

static void uart_init_clock(void)
{
	exynos_set_parent("mout_sclk_uart0", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_uart2", "mout_bus_pll_user");

	/* Set dout_sclk_uart to 200Mhz */
	exynos_set_rate("dout_sclk_uart0", 200000000);
	exynos_set_rate("dout_sclk_uart1", 200000000);
}

void __init exynos5433_clock_init(void)
{
	top_clk_enable();
	clkout_init_clock();
	aud_init_clock();
	adma_init_clock();
	usb_init_clock();
	crypto_init_clock();
	pcie_init_clock();
	g2d_init_clock();
	jpeg_init_clock();
	uart_init_clock();
	spi_init_clock();
}
