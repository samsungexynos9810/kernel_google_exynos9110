/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Author: Hyosang Jung
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5430 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/regs-clock-exynos5430.h>

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

	add_enabler("aclk_g2d_400");
	add_enabler("aclk_g2d_266");
	add_enabler("aclk_mfc0_333");
	add_enabler("aclk_mfc1_333");
	add_enabler("aclk_hevc_400");
	add_enabler("aclk_isp_400");
	add_enabler("aclk_isp_dis_400");
	add_enabler("aclk_cam0_552");
	add_enabler("aclk_cam0_400");
	add_enabler("aclk_cam0_333");
	add_enabler("aclk_cam1_552");
	add_enabler("aclk_cam1_400");
	add_enabler("aclk_cam1_333");
	add_enabler("aclk_gscl_333");
	add_enabler("aclk_gscl_111");
	add_enabler("aclk_fsys_200");
	add_enabler("aclk_mscl_400");
	add_enabler("aclk_peris_66");
	add_enabler("aclk_peric_66");
	add_enabler("aclk_imem_266");
	add_enabler("aclk_imem_200");

	add_enabler("sclk_jpeg_top");

#if 0
	add_enabler("sclk_isp_spi0_top");
	add_enabler("sclk_isp_spi1_top");
	add_enabler("sclk_isp_uart_top");
	add_enabler("sclk_isp_sensor0");
	add_enabler("sclk_isp_sensor1");
	add_enabler("sclk_isp_sensor2");
#endif

	add_enabler("sclk_hdmi_spdif_top");

	add_enabler("sclk_usbdrd30_top");
	add_enabler("sclk_ufsunipro_top");
	add_enabler("sclk_mmc0_top");
	add_enabler("sclk_mmc1_top");
	add_enabler("sclk_mmc2_top");

	add_enabler("sclk_spi0_top");
	add_enabler("sclk_spi1_top");
	add_enabler("sclk_spi2_top");
	add_enabler("sclk_uart0_top");
	add_enabler("sclk_uart1_top");
	add_enabler("sclk_uart2_top");
	add_enabler("sclk_pcm1_top");
	add_enabler("sclk_i2s1_top");
	add_enabler("sclk_spdif_top");
	add_enabler("sclk_slimbus_top");

	add_enabler("sclk_hpm_mif");
	add_enabler("aclk_cpif_200");
	add_enabler("aclk_disp_333");
	add_enabler("aclk_disp_222");
	add_enabler("aclk_bus1_400");
	add_enabler("aclk_bus2_400");

	add_enabler("sclk_decon_eclk_mif");
	add_enabler("sclk_decon_vclk_mif");
	add_enabler("sclk_dsd_mif");

	list_for_each_entry(ce, &clk_enabler_list, node) {
		clk_prepare(ce->clk);
		clk_enable(ce->clk);
	}

	pr_info("Clock enables : TOP, MIF\n");
}

void mfc_init_clock(void)
{
	exynos_set_parent("mout_aclk_mfc0_333_user", "aclk_mfc0_333");
	exynos_set_parent("mout_aclk_mfc1_333_user", "aclk_mfc1_333");
	exynos_set_parent("mout_aclk_hevc_400_user", "aclk_hevc_400");

	exynos_set_parent("mout_mfc_pll_user", "dout_mfc_pll");
	exynos_set_parent("mout_bus_pll_user", "dout_bus_pll");
}

void __init exynos5430_clock_init(void)
{
	top_clk_enable();
	mfc_init_clock();
}
