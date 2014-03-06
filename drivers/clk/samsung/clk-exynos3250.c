/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos3250 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <mach/regs-clock-exynos3.h>

#include "clk.h"
#include "clk-pll.h"

enum exynos3250_clks {
	none,

	fin_pll = 1,

	mout_user_bus_r = 40, mux_gdr,

	dclk_gpr = 60, dclk_gdr,

	clk_mct = 80,

	nr_clks,
};

static __initdata void *exynos3250_clk_regs[] = {
	EXYNOS3_CLKGATE_IP_PERIR,
	EXYNOS3_CLKDIV_RIGHTBUS,
	EXYNOS3_CLKSRC_RIGHTBUS,
};

struct samsung_gate_clock exynos3250_gate_clks[] __initdata = {
	GATE_A(clk_mct, "clk_mct", "dclk_gpr", (unsigned long) EXYNOS3_CLKGATE_IP_PERIR, 13, CLK_IGNORE_UNUSED, 0, "mct"),
};

PNAME(mout_dclk_gdr)	= { "mout_user_bus_r" };
PNAME(mout_ctrl_user_bus_r)	= { "fin_pll" };

struct samsung_div_clock exynos3250_div_clks[] __initdata = {
	DIV_A(dclk_gpr, "dclk_gpr", "dclk_gdr", (unsigned long) EXYNOS3_CLKDIV_RIGHTBUS, 4, 3, "div_gpr"),
	DIV_A(dclk_gdr, "mux_gdr", "dclk_gdr", (unsigned long) EXYNOS3_CLKDIV_RIGHTBUS, 0, 4, "div_gdr"),
};

struct samsung_mux_clock exynos3250_mux_clks[] __initdata = {
	MUX(mux_gdr, "mux_gdr", mout_dclk_gdr, (unsigned long) EXYNOS3_CLKSRC_RIGHTBUS, 0, 1),
	MUX(mout_user_bus_r, "mout_user_bus_r", mout_ctrl_user_bus_r, (unsigned long) EXYNOS3_CLKSRC_RIGHTBUS, 4, 1),
};

/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos3250_fixed_rate_ext_clks[] __initdata = {
	FRATE(fin_pll, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,exynos3250-oscclk", .data = (void *)0, },
	{ },
};

void __init exynos3250_clk_init(struct device_node *np)
{

	samsung_clk_init(np, 0, nr_clks, (unsigned long *)exynos3250_clk_regs,
			ARRAY_SIZE(exynos3250_clk_regs), NULL, 0);

	samsung_clk_of_register_fixed_ext(exynos3250_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos3250_fixed_rate_ext_clks),
			ext_clk_match);
	samsung_clk_register_mux(exynos3250_mux_clks,
			ARRAY_SIZE(exynos3250_mux_clks));
	samsung_clk_register_div(exynos3250_div_clks,
			ARRAY_SIZE(exynos3250_div_clks));
	samsung_clk_register_gate(exynos3250_gate_clks,
			ARRAY_SIZE(exynos3250_gate_clks));

	pr_info("Exynos3250: clock setup completed\n");
}

CLK_OF_DECLARE(exynos3250_clk, "samsung,exynos3250-clock", exynos3250_clk_init);
