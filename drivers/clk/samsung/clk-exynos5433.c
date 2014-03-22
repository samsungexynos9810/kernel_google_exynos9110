/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Authors: Thomas Abraham <thomas.ab@samsung.com>
 *	    Chander Kashyap <k.chander@samsung.com>
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

#include "clk.h"
#include "clk-pll.h"

enum exynos5433_clks {
	none,

	/* core clocks */
	fin_pll,

	/* Temporary clocks */
	clk_uart_baud0 = 10, uart0, uart1, uart2, mct,
	gate_uart0 = 20, gate_uart1, gate_uart2,

	nr_clks,
};

/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */
static __initdata unsigned long exynos5433_clk_regs[] = {
};

/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos5433_fixed_rate_ext_clks[] __initdata = {
	FRATE(fin_pll, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

/* After complete to prepare clock tree, these will be removed. */
struct samsung_fixed_rate_clock exynos5433_fixed_rate_clks[] __initdata = {
	FRATE(none, "gate_uart0", NULL, CLK_IS_ROOT, 10000000),
	FRATE(none, "gate_uart1", NULL, CLK_IS_ROOT, 10000000),
	FRATE(none, "gate_uart2", NULL, CLK_IS_ROOT, 10000000),
	FRATE(none, "clk_uart_baud0", NULL, CLK_IS_ROOT, 10000000),
	FRATE(none, "uart0", NULL, CLK_IS_ROOT, 10000000),
	FRATE(none, "uart1", NULL, CLK_IS_ROOT, 10000000),
	FRATE(none, "uart2", NULL, CLK_IS_ROOT, 10000000),
	FRATE(none, "mct", NULL, CLK_IS_ROOT, 10000000),
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,exynos5433-oscclk", .data = (void *)0, },
	{ },
};

/* register exynos5433 clocks */
void __init exynos5433_clk_init(struct device_node *np)
{
	void __iomem *reg_base;

	if (np) {
		reg_base = of_iomap(np, 0);
		if (!reg_base)
			panic("%s: failed to map registers\n", __func__);
	} else {
		panic("%s: unable to determine soc\n", __func__);
	}

	samsung_clk_init(np, reg_base, nr_clks,
			exynos5433_clk_regs, ARRAY_SIZE(exynos5433_clk_regs),
			NULL, 0);

	samsung_clk_of_register_fixed_ext(exynos5433_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos5433_fixed_rate_ext_clks),
			ext_clk_match);

	samsung_clk_register_fixed_rate(exynos5433_fixed_rate_clks,
			ARRAY_SIZE(exynos5433_fixed_rate_clks));
}
CLK_OF_DECLARE(exynos5433_clk, "samsung,exynos5433-clock", exynos5433_clk_init);
