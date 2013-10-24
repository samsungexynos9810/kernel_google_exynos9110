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

	add_enabler("aclk_g3d");
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

void __init exynos5422_clock_init(void)
{
	top_clk_enable();
	uart_clock_init();
}
