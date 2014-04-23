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

void __init exynos5433_clock_init(void)
{
	top_clk_enable();
	usb_init_clock();
	crypto_init_clock();
	pcie_init_clock();
}
