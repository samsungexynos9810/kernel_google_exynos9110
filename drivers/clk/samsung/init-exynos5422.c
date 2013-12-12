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
	struct clk *clk;
#if defined(CMU_PRINT_PLL)
	unsigned long tmp;
#endif
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
	add_enabler("fout_kpll");
	add_enabler("aclk_66_peric");
	add_enabler("aclk_66_psgen");
	add_enabler("aclk_300_jpeg");
	add_enabler("aclk_200_disp1");

	add_enabler("clk_fimd1");
	add_enabler("clk_dp1");
	add_enabler("clk_dsim1");
	add_enabler("clk_smmufimd1x_m1");
	add_enabler("clk_smmufimd1x_m0");

	/* enable list */
	add_enabler("clk_mscl2");
	add_enabler("clk_mscl1");
	add_enabler("clk_mscl0");
	add_enabler("aclk_333_432_gscl");
	add_enabler("aclk_300_gscl");
	add_enabler("pclk_smmu_gscl0");
	add_enabler("pclk_smmu_gscl1");
	add_enabler("sclk_gscl_wrap_b");
	add_enabler("sclk_gscl_wrap_a");
	add_enabler("clk_smmu_gscl1");
	add_enabler("clk_smmu_gscl0");
	add_enabler("clk_gscl1");
	add_enabler("clk_gscl0");
	add_enabler("aclk_noc_fsys");
	add_enabler("aclk_aclk_333_g2d");
	add_enabler("aclk_300_jpeg");

	add_enabler("clk_mdma1");
	add_enabler("clk_smmurotator");
	add_enabler("clk_smmujpeg");
	add_enabler("clk_rotator");

	/* enable list to enter suspend to ram */
	add_enabler("sclk_usbphy300");
	add_enabler("dout_usbphy300");
	add_enabler("sclk_usbdrd300");
	add_enabler("dout_usbdrd300");
	add_enabler("mout_usbdrd300");
	add_enabler("sclk_usbphy301");
	add_enabler("dout_usbphy301");
	add_enabler("sclk_usbdrd301");
	add_enabler("dout_usbdrd301");
	add_enabler("mout_usbdrd301");

	/* enable list to wake up */
	add_enabler("pclk_seckey_apbif");
	add_enabler("pclk_tzpc9");
	add_enabler("pclk_tzpc8");
	add_enabler("pclk_tzpc7");
	add_enabler("pclk_tzpc6");
	add_enabler("pclk_tzpc5");
	add_enabler("pclk_tzpc4");
	add_enabler("pclk_tzpc3");
	add_enabler("pclk_tzpc2");
	add_enabler("pclk_tzpc1");
	add_enabler("pclk_tzpc0");

	list_for_each_entry(ce, &clk_enabler_list, node) {
		clk_prepare(ce->clk);
		clk_enable(ce->clk);
	}

	/* Enable unipro mux to support LPA Mode */
	clk = __clk_lookup("mout_unipro");
	clk_prepare_enable(clk);
#if defined(CMU_PRINT_PLL)
	clk = __clk_lookup("fout_apll");
	tmp = clk_get_rate(clk);
	pr_info("apll %ld\n", tmp);

	clk = __clk_lookup("fout_bpll");
	tmp = clk_get_rate(clk);
	pr_info("bpll %ld\n", tmp);

	clk = __clk_lookup("fout_cpll");
	tmp = clk_get_rate(clk);
	pr_info("cpll %ld\n", tmp);

	clk = __clk_lookup("fout_dpll");
	tmp = clk_get_rate(clk);
	pr_info("dpll %ld\n", tmp);

	clk = __clk_lookup("fout_epll");
	tmp = clk_get_rate(clk);
	pr_info("epll %ld\n", tmp);

	clk = __clk_lookup("fout_rpll");
	tmp = clk_get_rate(clk);
	pr_info("rpll %ld\n", tmp);

	clk = __clk_lookup("fout_ipll");
	tmp = clk_get_rate(clk);
	pr_info("ipll %ld\n", tmp);

	clk = __clk_lookup("fout_spll");
	tmp = clk_get_rate(clk);
	pr_info("spll %ld\n", tmp);

	clk = __clk_lookup("fout_vpll");
	tmp = clk_get_rate(clk);
	pr_info("vpll %ld\n", tmp);

	clk = __clk_lookup("fout_mpll");
	tmp = clk_get_rate(clk);
	pr_info("mpll %ld\n", tmp);
#endif
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

	exynos_set_parent("mout_aclk_400_mscl", "fout_cpll");
	exynos_set_parent("mout_aclk_400_mscl_sw", "dout_aclk_400_mscl");
	exynos_set_parent("mout_aclk_400_mscl_user", "mout_aclk_400_mscl_sw");
	exynos_set_parent("aclk_400_mscl", "mout_aclk_400_mscl_user");

	exynos_set_rate("dout_aclk_400_mscl", 333 * 1000000);

	pr_info("scaler: dout_aclk_400_mscl %d aclk_400_mscl %d\n",
			exynos_get_rate("dout_aclk_400_mscl"),
			exynos_get_rate("aclk_400_mscl"));
}

void g2d_init_clock(void)
{
	int clk_rate1;

	if (exynos_set_parent("mout_aclk_333_g2d", "mout_cpll_ctrl"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_333_g2d", "mout_cpll_ctrl");

	if (exynos_set_parent("mout_aclk_333_g2d_sw", "dout_aclk_333_g2d"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_333_g2d_sw", "dout_aclk_333_g2d");

	if (exynos_set_parent("mout_aclk_333_g2d_user", "mout_aclk_333_g2d_sw"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_333_g2d_user", "mout_aclk_333_g2d_sw");

	if(exynos_set_rate("dout_aclk_333_g2d", 333 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_aclk_g2d_400");

	clk_rate1 = exynos_get_rate("dout_aclk_333_g2d");

	pr_info("[%s:%d] aclk_333_g2d:%d\n"
			, __func__, __LINE__, clk_rate1);
}

void pwm_init_clock(void)
{
	clk_register_fixed_factor(NULL, "pwm-clock",
			"pclk_pwm",CLK_SET_RATE_PARENT, 1, 1);
}

void __init exynos5422_clock_init(void)
{
	top_clk_enable();
	uart_clock_init();
	mscl_init_clock();
	g2d_init_clock();
	pwm_init_clock();
}
