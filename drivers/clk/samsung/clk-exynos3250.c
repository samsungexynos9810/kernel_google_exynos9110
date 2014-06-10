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

#define SRC_TOP0		EXYNOS3_CLK_BUS_TOP_REG(0xc210)
#define SRC_TOP1		EXYNOS3_CLK_BUS_TOP_REG(0xc214)
#define DIV_TOP			EXYNOS3_CLK_BUS_TOP_REG(0xc510)
#define E3250_MPLL_LOCK		EXYNOS3_CLK_BUS_TOP_REG(0xC010)
#define E3250_MPLL_CON0		EXYNOS3_CLK_BUS_TOP_REG(0xC110)
#define GATE_IP_FSYS		EXYNOS3_CLK_BUS_TOP_REG(0xc940)
#define SRC_CPU			EXYNOS3_CPU_ISP_REG(0x14200)
#define SRC_DMC			EXYNOS3_MIF_L_REG(0x0300)

static __initdata void *exynos3250_clk_regs[] = {
	EXYNOS3_CLKGATE_IP_PERIR,
	EXYNOS3_CLKDIV_RIGHTBUS,
	EXYNOS3_CLKSRC_RIGHTBUS,
	EXYNOS3_CLKSRC_MASK_FSYS,
	EXYNOS3_CLKDIV_FSYS1,
	EXYNOS3_CLKGATE_BUS_FSYS0,
	EXYNOS3_CLKGATE_IP_FSYS,
	EXYNOS3_CLKSRC_FSYS,
	EXYNOS3_CLKGATE_FSYS,
};

#define CGATE_A(_id, cname, pname, o, b, f, gf, a) \
	GATE_A(_id, cname, pname, (unsigned long)o, b, f, gf, a)

#define CGATE(_id, cname, pname, o, b, f, gf) \
	GATE(_id, cname, pname, (unsigned long)o, b, f, gf)

struct samsung_gate_clock exynos3250_gate_clks[] __initdata = {
	GATE_A(CLK_MCT, "clk_mct", "dclk_gpr", (unsigned long) EXYNOS3_CLKGATE_IP_PERIR,
			13, CLK_IGNORE_UNUSED, 0, "mct"),
	CGATE(CLK_RTC, "rtc", "aclk66",
			EXYNOS3_CLKGATE_IP_PERIR, 15, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_UART0, "gate_uart0", "dout_uart0",
			EXYNOS3_CLKGATE_IP_PERIL, 0, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_UART1, "gate_uart1", "dout_uart1",
			EXYNOS3_CLKGATE_IP_PERIL, 1, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_UART2, "gate_uart2", "dout_uart2",
			EXYNOS3_CLKGATE_IP_PERIL, 2, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_UART3, "gate_uart3", "dout_uart3",
			EXYNOS3_CLKGATE_IP_PERIL, 3, CLK_IGNORE_UNUSED, 0),

	CGATE(GATE_I2C0, "gate_i2c0", "aclk100",
			EXYNOS3_CLKGATE_IP_PERIL, 6, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_I2C1, "gate_i2c1", "aclk100",
			EXYNOS3_CLKGATE_IP_PERIL, 7, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_I2C2, "gate_i2c2", "aclk100",
			EXYNOS3_CLKGATE_IP_PERIL, 8, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_I2C3, "gate_i2c3", "aclk100",
			EXYNOS3_CLKGATE_IP_PERIL, 9, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_I2C4, "gate_i2c4", "aclk100",
			EXYNOS3_CLKGATE_IP_PERIL, 10, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_I2C5, "gate_i2c5", "aclk100",
			EXYNOS3_CLKGATE_IP_PERIL, 11, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_I2C6, "gate_i2c6", "aclk100",
			EXYNOS3_CLKGATE_IP_PERIL, 12, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_I2C7, "gate_i2c7", "aclk100",
			EXYNOS3_CLKGATE_IP_PERIL, 13, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_ACLK_FIMD0, "gate_aclk_fimd0", NULL,
			EXYNOS3_CLKGATE_BUS_LCD, 0, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_SCLK_FIMD0, "gate_sclk_fimd0", NULL,
			EXYNOS3_CLKGATE_SCLK_LCD, 0, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_SCLK_MIPI0, "gate_sclk_mipi0", NULL,
			EXYNOS3_CLKGATE_SCLK_LCD, 3, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_CLK_FIMD0, "gate_clk_fimd0", NULL,
			EXYNOS3_CLKGATE_IP_LCD, 0, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_CLK_DSIM0, "gate_clk_dsim0", NULL,
			EXYNOS3_CLKGATE_IP_LCD, 3, CLK_IGNORE_UNUSED, 0),
	CGATE(ACLK_MMC0, "aclk_mmc0", "aclk200",
			EXYNOS3_CLKGATE_BUS_FSYS0, 5, CLK_IGNORE_UNUSED, 0),
	CGATE(GATE_MMC0, "gate_mmc0", "dout_mmc_b",
			EXYNOS3_CLKGATE_FSYS, 0, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_PDMA0, "pdma0", "aclk200", GATE_IP_FSYS, 0, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_PDMA1, "pdma1", "aclk200", GATE_IP_FSYS, 1, CLK_IGNORE_UNUSED, 0),
};

#define CDIV(_id, cname, pname, o, s, w) \
			DIV(_id, cname, pname, (unsigned long)o, s, w)

struct samsung_div_clock exynos3250_div_clks[] __initdata = {
	DIV_A(DCLK_GPR, "dclk_gpr", "dclk_gdr", (unsigned long) EXYNOS3_CLKDIV_RIGHTBUS,
			4, 3, "div_gpr"),
	DIV_A(DCLK_GDR, "dclk_gdr", "mux_gdr", (unsigned long) EXYNOS3_CLKDIV_RIGHTBUS,
			0, 4, "div_gdr"),
	DIV_A(MPLL_PRE_DIV, "mpll_pre_div", "half_mpll",
		(unsigned long)DIV_TOP, 28, 2, "mpll_pre_div"),
	DIV_A(ACLK100, "aclk100", "mout_aclk100", (unsigned long)DIV_TOP, 4, 4, "aclk100"),
	DIV_A(ACLK200, "aclk200", "mout_aclk200", (unsigned long)DIV_TOP, 12, 4, "aclk200"),

	DIV_A(DOUT_MMC_A, "dout_mmc_a", "mout_mmc0",
			(unsigned long)EXYNOS3_CLKDIV_FSYS1, 0, 3, "dout_mmc_a"),
	DIV_A(DOUT_MMC_B, "dout_mmc_b", "dout_mmc_a",
			(unsigned long)EXYNOS3_CLKDIV_FSYS1, 8, 8, "dout_mmc_b"),

	DIV_A(DOUT_UART0, "dout_uart0", "mout_uart0",
			(unsigned long)EXYNOS3_CLKDIV_PERIL0, 0, 4, "dout_uart0"),
	DIV_A(DOUT_UART1, "dout_uart1", "mout_uart1",
			(unsigned long)EXYNOS3_CLKDIV_PERIL0, 4, 4, "dout_uart1"),
	DIV_A(DOUT_UART2, "dout_uart2", "mout_uart2",
			(unsigned long)EXYNOS3_CLKDIV_PERIL0, 8, 4, "dout_uart2"),
	DIV_A(DOUT_UART3, "dout_uart3", "mout_uart3",
			(unsigned long)EXYNOS3_CLKDIV_PERIL0, 12, 4, "dout_uart3"),

	DIV_A(DOUT_FIMD0, "dout_fimd0", "mout_fimd0",
			(unsigned long)EXYNOS3_CLKDIV_LCD, 0, 4, "dout_fimd"),
	DIV_A(DOUT_MIPI0, "dout_mipi0", "mout_mipi0",
			(unsigned long)EXYNOS3_CLKDIV_LCD, 16, 4, "dout_mipi0"),
	DIV_A(DOUT_MIPI0_PRE, "dout_mipi0_pre", "dout_mipi0",
			(unsigned long)EXYNOS3_CLKDIV_LCD, 20, 4, "dout_mipi0_pre"),
	DIV_A(DOUT_ACLK_160, "dout_aclk_160", "mout_aclk_160",
			(unsigned long)EXYNOS3_CLKDIV_TOP, 8, 3, "dout_aclk_160"),
};

PNAME(mout_dclk_gdr)	= { "mout_user_bus_r" };
PNAME(mout_ctrl_user_bus_r)	= { "finpll", "mpll_pre_div" };
PNAME(mout_uart_p)      = { "xxti", "xusbxti", "none", "none", "none", "none",
			"mpll_pre_div", "sclk_epll", "sclk_vpll", };
PNAME(mout_mmc_p)      = { "xxti", "xusbsti", "none", "none", "none", "none",
			"half_mpll", "sclk_epll", "sclk_vpll" };
PNAME(mout_mpll)	= { "fin_pll", "fout_mpll", };
PNAME(sclk_ampll_p3250)	= { "mpll_pre_div", };

#define CMUX(_id, cname, pnames, o, s, w) \
	MUX(_id, cname, pnames, (unsigned long)o, s, w)

struct samsung_mux_clock exynos3250_mux_clks[] __initdata = {
	MUX(MUX_GDR, "mux_gdr", mout_dclk_gdr, (unsigned long) EXYNOS3_CLKSRC_RIGHTBUS, 0, 1),
	MUX(MOUT_USER_BUS_R, "mout_user_bus_r", mout_ctrl_user_bus_r, (unsigned long) EXYNOS3_CLKSRC_RIGHTBUS, 4, 1),
	MUX_A(SCLK_MPLL, "sclk_mpll", mout_mpll, (unsigned long)SRC_TOP1, 12, 1, "sclk_mpll"),
	MUX_A(MOUT_ACLK100, "mout_aclk100", sclk_ampll_p3250, (unsigned long)SRC_TOP0,
					16, 1, "mout_aclk100"),
	MUX_A(MOUT_ACLK200, "mout_aclk200", sclk_ampll_p3250, (unsigned long)SRC_TOP0,
					24, 1, "mout_aclk200"),

	MUX_A(MOUT_UART0, "mout_uart0", mout_uart_p,
			(unsigned long)EXYNOS3_CLKSRC_PERIL0, 0, 4, "mout_uart0"),
	MUX_A(MOUT_UART1, "mout_uart1", mout_uart_p,
			(unsigned long)EXYNOS3_CLKSRC_PERIL0, 4, 4, "mout_uart1"),
	MUX_A(MOUT_UART2, "mout_uart2", mout_uart_p,
			(unsigned long)EXYNOS3_CLKSRC_PERIL0, 8, 4, "mout_uart2"),
	MUX_A(MOUT_UART3, "mout_uart3", mout_uart_p,
			(unsigned long)EXYNOS3_CLKSRC_PERIL0, 12, 4, "mout_uart3"),

	MUX_A(MOUT_FIMD0, "mout_fimd0", mout_uart_p,
			(unsigned long)EXYNOS3_CLKSRC_LCD, 0, 4, "mout_fimd0"),
	MUX_A(MOUT_MIPI0, "mout_mipi0", mout_uart_p,
			(unsigned long)EXYNOS3_CLKSRC_LCD, 12, 4, "mout_mipi0"),
	MUX_A(MOUT_ACLK_160, "mout_aclk_160", mout_uart_p,
			(unsigned long)EXYNOS3_CLKSRC_TOP0, 20, 0, "mout_aclk_160"),
	MUX_A(MOUT_MMC0, "mout_mmc0", mout_mmc_p,
			(unsigned long)EXYNOS3_CLKSRC_FSYS, 0, 4, "mout_mmc0"),
};

/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos3250_fixed_rate_ext_clks[] __initdata = {
	FRATE(FIN_PLL, "fin_pll", NULL, CLK_IS_ROOT, 0),
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

	pr_info("MPLL = %ld\n", _get_rate("sclk_mpll"));
	pr_info("MPLL_PRE_DIV = %ld\n", _get_rate("mpll_pre_div"));
	pr_info("Exynos3250: clock setup completed\n");
}

CLK_OF_DECLARE(exynos3250_clk, "samsung,exynos3250-clock", exynos3250_clk_init);
