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
#include <dt-bindings/clk/exynos3250-clk.h>

#include <mach/regs-clock-exynos3.h>

#include "clk.h"
#include "clk-pll.h"

#define E3250_MPLL_LOCK		EXYNOS3_CLK_BUS_TOP_REG(0xC010)
#define E3250_MPLL_CON0		EXYNOS3_CLK_BUS_TOP_REG(0xC110)

static __initdata void *exynos3250_clk_regs[] = {
	GATE_IP_PERIR,
	DIV_RIGHTBUS,
	SRC_RIGHTBUS,
	SRC_MASK_FSYS,
	DIV_FSYS1,
	GATE_BUS_FSYS0,
	GATE_IP_FSYS,
	SRC_FSYS,
	GATE_SCLK_FSYS,
};

PNAME(mout_gdr_p)		= { "mout_mpll_user_r" };
PNAME(mout_mpll_user_r_p)	= { "finpll", "dout_mpll_pre" };
PNAME(group_sclk_p)		= { "xxti", "xusbxti", "none", "none", "none",
					"none",	"dout_mpll_pre",
					"sclk_epll", "sclk_vpll" };
PNAME(mout_mmc_p)		= { "xxti", "xusbsti", "none", "none", "none",
					"none", "half_mpll",
					"sclk_epll", "sclk_vpll" };
PNAME(mout_mpll_p)		= { "fin_pll", "fout_mpll" };
PNAME(group_div_mpll_pre)	= { "dout_mpll_pre" };

#define CMUX(_id, cname, pnames, o, s, w) \
	MUX(_id, cname, pnames, (unsigned long)o, s, w)
#define CMUX_A(_id, cname, pnames, o, s, w, a) \
	MUX_A(_id, cname, pnames, (unsigned long)o, s, w, a)

struct samsung_mux_clock exynos3250_mux_clks[] __initdata = {
	CMUX(CLK_MOUT_MPLL_USER_R, "mout_mpll_user_r", mout_mpll_user_r_p,
		SRC_RIGHTBUS, 4, 1),
	CMUX(CLK_MOUT_GDR, "mout_gdr", mout_gdr_p, SRC_RIGHTBUS, 0, 1),

	CMUX_A(CLK_MOUT_MPLL, "mout_mpll", mout_mpll_p,
		SRC_TOP1, 12, 1, "sclk_mpll"),

	CMUX(CLK_MOUT_ACLK_200, "mout_aclk200", group_div_mpll_pre,
		SRC_TOP0, 24, 1),
	CMUX(CLK_MOUT_ACLK_160, "mout_aclk_160", group_sclk_p, SRC_TOP0, 20, 0),
	CMUX(CLK_MOUT_ACLK_100, "mout_aclk100", group_div_mpll_pre,
		SRC_TOP0, 16, 1),

	CMUX(CLK_MOUT_UART3, "mout_uart3", group_sclk_p, SRC_PERIL0, 12, 4),
	CMUX(CLK_MOUT_UART2, "mout_uart2", group_sclk_p, SRC_PERIL0, 8, 4),
	CMUX(CLK_MOUT_UART1, "mout_uart1", group_sclk_p, SRC_PERIL0, 4, 4),
	CMUX(CLK_MOUT_UART0, "mout_uart0", group_sclk_p, SRC_PERIL0, 0, 4),

	CMUX(CLK_MOUT_MIPI0, "mout_mipi0", group_sclk_p, SRC_LCD, 12, 4),
	CMUX(CLK_MOUT_FIMD0, "mout_fimd0", group_sclk_p, SRC_LCD, 0, 4),

	CMUX(CLK_MOUT_MMC0, "mout_mmc0", mout_mmc_p, SRC_FSYS, 0, 4),
};

#define CDIV(_id, cname, pname, o, s, w) \
			DIV(_id, cname, pname, (unsigned long)o, s, w)

struct samsung_div_clock exynos3250_div_clks[] __initdata = {
	CDIV(CLK_DIV_GPR, "dout_gpr", "dout_gdr", DIV_RIGHTBUS, 4, 3),
	CDIV(CLK_DIV_GDR, "dout_gdr", "mout_gdr", DIV_RIGHTBUS, 0, 4),

	CDIV(CLK_DIV_MPLL_PRE, "dout_mpll_pre", "half_mpll", DIV_TOP, 28, 2),
	CDIV(CLK_DIV_ACLK_200, "dout_aclk_200", "mout_aclk200", DIV_TOP, 12, 4),
	CDIV(CLK_DIV_ACLK_160, "dout_aclk_160", "mout_aclk_160", DIV_TOP, 8, 3),
	CDIV(CLK_DIV_ACLK_100, "dout_aclk_100", "mout_aclk100", DIV_TOP, 4, 4),

	CDIV(CLK_DIV_MMC0_PRE, "dout_mmc0_pre", "dout_mmc0", DIV_FSYS1, 8, 8),
	CDIV(CLK_DIV_MMC0, "dout_mmc0", "mout_mmc0", DIV_FSYS1, 0, 3),

	CDIV(CLK_DIV_UART3, "dout_uart3", "mout_uart3",	DIV_PERIL0, 12, 4),
	CDIV(CLK_DIV_UART2, "dout_uart2", "mout_uart2",	DIV_PERIL0, 8, 4),
	CDIV(CLK_DIV_UART1, "dout_uart1", "mout_uart1",	DIV_PERIL0, 4, 4),
	CDIV(CLK_DIV_UART0, "dout_uart0", "mout_uart0",	DIV_PERIL0, 0, 4),

	CDIV(CLK_DIV_MIPI0_PRE, "dout_mipi0_pre", "dout_mipi0",	DIV_LCD, 20, 4),
	CDIV(CLK_DIV_MIPI0, "dout_mipi0", "mout_mipi0", DIV_LCD, 16, 4),
	CDIV(CLK_DIV_FIMD0, "dout_fimd0", "mout_fimd0", DIV_LCD, 0, 4),
};

#define CGATE(_id, cname, pname, o, b, f, gf) \
	GATE(_id, cname, pname, (unsigned long)o, b, f, gf)

struct samsung_gate_clock exynos3250_gate_clks[] __initdata = {
	CGATE(CLK_RTC, "rtc", "aclk66",
			GATE_IP_PERIR, 15, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_MCT, "mct", "dout_gpr",
			GATE_IP_PERIR, 13, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_UART3, "uart3", "dout_uart3",
			GATE_IP_PERIL, 3, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_UART2, "uart2", "dout_uart2",
			GATE_IP_PERIL, 2, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_UART1, "uart1", "dout_uart1",
			GATE_IP_PERIL, 1, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_UART0, "uart0", "dout_uart0",
			GATE_IP_PERIL, 0, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_I2C7, "i2c7", "dout_aclk_100",
			GATE_IP_PERIL, 13, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_I2C6, "i2c6", "dout_aclk_100",
			GATE_IP_PERIL, 12, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_I2C5, "i2c5", "dout_aclk_100",
			GATE_IP_PERIL, 11, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_I2C4, "i2c4", "dout_aclk_100",
			GATE_IP_PERIL, 10, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_I2C3, "i2c3", "dout_aclk_100",
			GATE_IP_PERIL, 9, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_I2C2, "i2c2", "dout_aclk_100",
			GATE_IP_PERIL, 8, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_I2C1, "i2c1", "dout_aclk_100",
			GATE_IP_PERIL, 7, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_I2C0, "i2c0", "dout_aclk_100",
			GATE_IP_PERIL, 6, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_ACLK_FIMD0, "aclk_fimd0", NULL,
			GATE_BUS_LCD, 0, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_SCLK_MIPI0, "sclk_mipi0", NULL,
			GATE_SCLK_LCD, 3, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_SCLK_FIMD0, "sclk_fimd0", NULL,
			GATE_SCLK_LCD, 0, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_CLK_DSIM0, "gate_clk_dsim0", NULL,
			GATE_IP_LCD, 3, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_FIMD0, "fimd0", NULL,
			GATE_IP_LCD, 0, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_ACLK_MMC0, "aclk_mmc0", "dout_aclk_200",
			GATE_BUS_FSYS0, 5, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_MMC0, "mmc0", "dout_mmc0_pre",
			GATE_SCLK_FSYS, 0, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_PDMA1, "pdma1", "dout_aclk_200",
			GATE_IP_FSYS, 1, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_PDMA0, "pdma0", "dout_aclk_200",
			GATE_IP_FSYS, 0, CLK_IGNORE_UNUSED, 0),
};

/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos3250_fixed_rate_ext_clks[] __initdata = {
	FRATE(CLK_FIN_PLL, "fin_pll", NULL, CLK_IS_ROOT, 0),
	FRATE(HALF_MPLL, "half_mpll", NULL, CLK_IS_ROOT, 0),
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,exynos3250-oscclk", .data = (void *)0, },
	{ .compatible = "samsung,exynos3250-half_mpll", .data = (void *)1, },
	{ },
};

static struct samsung_pll_rate_table exynos3_mpll_div[] = {
	{ 800000000, 3, 200, 1, 0},
};

void __init exynos3250_clk_init(struct device_node *np)
{
	struct clk *mpll;

	mpll = samsung_clk_register_pll35xx("fout_mpll", "fin_pll",
		E3250_MPLL_LOCK, E3250_MPLL_CON0, exynos3_mpll_div,
		sizeof(exynos3_mpll_div));

	samsung_clk_add_lookup(mpll, FOUT_MPLL);

	samsung_clk_init(np, 0, NR_CLKS, (unsigned long *)exynos3250_clk_regs,
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
