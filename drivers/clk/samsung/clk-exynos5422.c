/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Author: Hyosang Jung
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5422 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk.h"
#include "clk-pll.h"

enum exynos5422_clks {
	none,

	/* core clocks */
	fin_pll = 1, mct,

	/* uart clocks */
	sclk_uart0 = 10, sclk_uart1, sclk_uart2, sclk_uart3,
	gate_uart0, gate_uart1, gate_uart2, gate_uart3,

	nr_clks,
};

static __initdata void *exynos5422_clk_regs[] = {
};

/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos5422_fixed_rate_ext_clks[] __initdata = {
	FRATE(fin_pll, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,exynos5422-oscclk", .data = (void *)0, },
	{ },
};

/* register exynos5422 clocks */
void __init exynos5422_clk_init(struct device_node *np)
{
	samsung_clk_init(np, 0, nr_clks, (unsigned long *) exynos5422_clk_regs,
			 ARRAY_SIZE(exynos5422_clk_regs), NULL, 0);

	samsung_clk_of_register_fixed_ext(exynos5422_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos5422_fixed_rate_ext_clks),
			ext_clk_match);

	pr_info("Exynos5422: clock setup completed\n");
}
CLK_OF_DECLARE(exynos5422_clk, "samsung,exynos5422-clock", exynos5422_clk_init);
