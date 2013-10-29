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
#include <mach/regs-clock-exynos5422.h>
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
	add_enabler("fout_apll");
	add_enabler("fout_bpll");
	add_enabler("fout_cpll");
	add_enabler("fout_dpll");
	add_enabler("fout_ipll");
	add_enabler("fout_mpll");
	add_enabler("fout_spll");
	add_enabler("fout_vpll");
	add_enabler("fout_epll");
	add_enabler("fout_rpll");
	add_enabler("aclk_66_peric");
	add_enabler("aclk_66_psgen");
	add_enabler("aclk_300_jpeg");
	list_for_each_entry(ce, &clk_enabler_list, node) {
		clk_prepare(ce->clk);
		clk_enable(ce->clk);
	}

	pr_info("Clock enables : TOP, MIF\n");
}

static void uart_clock_init(void)
{
	exynos_set_rate("dout_uart0", 150000000);
	exynos_set_rate("dout_uart1", 150000000);
	exynos_set_rate("dout_uart2", 150000000);
	exynos_set_rate("dout_uart3", 150000000);
}

static void mscl_init_clock(void)
{

	exynos_set_parent("mout_aclk_400_mscl", "sclk_cpll");
	exynos_set_parent("dout_aclk_400_mscl", "mout_aclk_400_mscl");
	exynos_set_parent("mout_aclk_400_mscl_sw", "dout_aclk_400_mscl");
	exynos_set_parent("mout_aclk_400_mscl_user", "mout_aclk_400_mscl_sw");
	exynos_set_parent("aclk_400_mscl", "mout_aclk_400_mscl_user");

	exynos_set_rate("dout_aclk_400_mscl", 400 * 1000000);

	pr_info("scaler: dout_aclk_400_mscl %d aclk_400_mscl %d\n",
			exynos_get_rate("dout_aclk_400_mscl"),
			exynos_get_rate("aclk_400_mscl"));
}

void __init exynos5422_clock_init(void)
{
	top_clk_enable();
	uart_clock_init();
	mscl_init_clock();
}
