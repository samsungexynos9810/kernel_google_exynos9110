/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5430 - EAGLE Core frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/clk-private.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>

#define CPUFREQ_LEVEL_END_CA15	(L23 + 1)

#undef PRINT_DIV_VAL
#undef ENABLE_CLKOUT

static int max_support_idx_CA15;
static int min_support_idx_CA15 = (CPUFREQ_LEVEL_END_CA15 - 1);

static struct clk *mout_egl2;
static struct clk *mout_egl1;
static struct clk *mout_egl_pll;
static struct clk *sclk_bus_pll;
static struct clk *mout_bus_pll_user;
static struct clk *fout_egl_pll;

static unsigned int exynos5430_volt_table_CA15[CPUFREQ_LEVEL_END_CA15];

static struct cpufreq_frequency_table exynos5430_freq_table_CA15[] = {
	{L0,  2500 * 1000},
	{L1,  2400 * 1000},
	{L2,  2300 * 1000},
	{L3,  2200 * 1000},
	{L4,  2100 * 1000},
	{L5,  2000 * 1000},
	{L6,  1900 * 1000},
	{L7,  1800 * 1000},
	{L8,  1700 * 1000},
	{L9,  1600 * 1000},
	{L10, 1500 * 1000},
	{L11, 1400 * 1000},
	{L12, 1300 * 1000},
	{L13, 1200 * 1000},
	{L14, 1100 * 1000},
	{L15, 1000 * 1000},
	{L16,  900 * 1000},
	{L17,  800 * 1000},
	{L18,  700 * 1000},
	{L19,  600 * 1000},
	{L20,  500 * 1000},
	{L21,  400 * 1000},
	{L22,  300 * 1000},
	{L23,  200 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos5430_clkdiv_table_CA15[CPUFREQ_LEVEL_END_CA15];

static unsigned int clkdiv_cpu0_5430_CA15[CPUFREQ_LEVEL_END_CA15][6] = {
	/*
	 * Clock divider value for following
	 * { EGL1, EGL2, ACLK_EGL, PCLK_EGL,
	 *   ATCLK, PCLK_DBG_EGL }
	 */

	/* ARM L0: 2.5GHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L1: 2.4GMHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L2: 2.3GMHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L3: 2.2GHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L4: 2.1GHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L5: 2.0GHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L6: 1.9GHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L7: 1.8GHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L8: 1.7GHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L9: 1.6GHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L10: 1.5GHz */
	{ 0, 0, 2, 0, 7, 0 },

	/* ARM L11: 1.4GHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L12: 1.3GHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L13: 1.2GHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L14: 1.1GHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L15: 1.0GHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L16: 900MHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L17: 800MHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L18: 700MHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L19: 600MHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L20: 500MHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L21: 400MHz */
	{ 0, 0, 1, 0, 7, 0 },

	/* ARM L22: 300MHz */
	{ 0, 0, 0, 0, 7, 0 },

	/* ARM L23: 200MHz */
	{ 0, 0, 0, 0, 7, 0 },
};

static unsigned int clkdiv_cpu1_5430_CA15[CPUFREQ_LEVEL_END_CA15][2] = {
	/*
	 * Clock divider value for following
	 * { SCLK_EGL_PLL, SCLK_HPM_EGL }
	 */

	/* ARM L0: 2.5GHz */
	{ 0, 7 },

	/* ARM L1: 2.4GMHz */
	{ 0, 7 },

	/* ARM L2: 2.3GMHz */
	{ 0, 7 },

	/* ARM L3: 2.2GHz */
	{ 0, 7 },

	/* ARM L4: 2.1GHz */
	{ 0, 7 },

	/* ARM L5: 2.0GHz */
	{ 0, 7 },

	/* ARM L6: 1.9GHz */
	{ 0, 7 },

	/* ARM L7: 1.8GHz */
	{ 0, 7 },

	/* ARM L8: 1.7GHz */
	{ 0, 7 },

	/* ARM L9: 1.6GHz */
	{ 0, 7 },

	/* ARM L10: 1.5GHz */
	{ 0, 7 },

	/* ARM L11: 1.4GHz */
	{ 0, 7 },

	/* ARM L12: 1.3GHz */
	{ 0, 7 },

	/* ARM L13: 1.2GHz */
	{ 0, 7 },

	/* ARM L14: 1.1GHz */
	{ 0, 7 },

	/* ARM L15: 1.0GHz */
	{ 0, 7 },

	/* ARM L16: 900MHz */
	{ 0, 7 },

	/* ARM L17: 800MHz */
	{ 0, 7 },

	/* ARM L18: 700MHz */
	{ 0, 7 },

	/* ARM L19: 600MHz */
	{ 0, 7 },

	/* ARM L20: 500MHz */
	{ 0, 7 },

	/* ARM L21: 400MHz */
	{ 0, 7 },

	/* ARM L22: 300MHz */
	{ 0, 7 },

	/* ARM L23: 200MHz */
	{ 0, 7 },
};

static unsigned int exynos5430_egl_pll_pms_table_CA15[CPUFREQ_LEVEL_END_CA15] = {
	/* MDIV | PDIV | SDIV */
	/* EGL_PLL FOUT L0: 2.5GHz */
	PLL2450X_PMS(625, 6, 0),

	/* EGL_PLL FOUT L1: 2.4GHz */
	PLL2450X_PMS(500, 5, 0),

	/* EGL_PLL FOUT L2: 2.3GHz */
	PLL2450X_PMS(575, 6, 0),

	/* EGL_PLL FOUT L3: 2.2GHz */
	PLL2450X_PMS(550, 6, 0),

	/* EGL_PLL FOUT L4: 2.1GHz */
	PLL2450X_PMS(350, 4, 0),

	/* EGL_PLL FOUT L5: 2.0GHz */
	PLL2450X_PMS(500, 6, 0),

	/* EGL_PLL FOUT L6: 1.9GHz */
	PLL2450X_PMS(475, 6, 0),

	/* EGL_PLL FOUT L7: 1.8GHz */
	PLL2450X_PMS(375, 5, 0),

	/* EGL_PLL FOUT L8: 1.7GHz */
	PLL2450X_PMS(425, 6, 0),

	/* EGL_PLL FOUT L9: 1.6GHz */
	PLL2450X_PMS(400, 6, 0),

	/* EGL_PLL FOUT L10: 1.5GHz */
	PLL2450X_PMS(250, 4, 0),

	/* EGL_PLL FOUT L11: 1.4GHz */
	PLL2450X_PMS(350, 6, 0),

	/* EGL_PLL FOUT L12: 1.3GHz */
	PLL2450X_PMS(325, 6, 0),

	/* EGL_PLL FOUT L13: 1.2GHz */
	PLL2450X_PMS(500, 5, 1),

	/* EGL_PLL FOUT L14: 1.1GHz */
	PLL2450X_PMS(550, 6, 1),

	/* EGL_PLL FOUT L15: 1.0GHz */
	PLL2450X_PMS(500, 6, 1),

	/* EGL_PLL FOUT L16: 900MHz */
	PLL2450X_PMS(375, 5, 1),

	/* EGL_PLL FOUT L17: 800MHz */
	PLL2450X_PMS(400, 6, 1),

	/* EGL_PLL FOUT L18: 700MHz */
	PLL2450X_PMS(350, 6, 1),

	/* EGL_PLL FOUT L19: 600MHz */
	PLL2450X_PMS(500, 5, 2),

	/* EGL_PLL FOUT L20: 500MHz */
	PLL2450X_PMS(500, 6, 2),

	/* EGL_PLL FOUT L21: 400MHz */
	PLL2450X_PMS(400, 6, 2),

	/* EGL_PLL FOUT L22: 300MHz */
	PLL2450X_PMS(500, 5, 3),

	/* EGL_PLL FOUT L23: 200MHz */
	PLL2450X_PMS(400, 6, 3),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5430_CA15[CPUFREQ_LEVEL_END_CA15] = {
	1300000,	/* LO  2500 */
	1300000,	/* L1  2400 */
	1300000,	/* L2  2300 */
	1300000,	/* L3  2200 */
	1300000,	/* L4  2100 */
	1300000,	/* L5  2000 */
	1300000,	/* L6  1900 */
	1300000,	/* L7  1800 */
	1300000,	/* L8  1700 */
	1300000,	/* L9  1600 */
	1300000,	/* L10 1500 */
	1275000,	/* L11 1400 */
	1250000,	/* L12 1300 */
	1225000,	/* L13 1200 */
	1200000,	/* L14 1100 */
	1175000,	/* L15 1000 */
	1150000,	/* L16  900 */
	1125000,	/* L17  800 */
	1100000,	/* L18  700 */
	1075000,	/* L19  600 */
	1050000,	/* L20  500 */
	1000000,	/* L21  400 */
	 950000,	/* L22  300 */
	 950000,	/* L23  200 */
};

/* Minimum memory throughput in megabytes per second */
static int exynos5430_bus_table_CA15[CPUFREQ_LEVEL_END_CA15] = {
	1066000,	/* 2.5 GHz */
	1066000,	/* 2.4 GHz */
	1066000,	/* 2.3 GHz */
	1066000,	/* 2.2 GHz */
	1066000,	/* 2.1 GHz */
	 933000,	/* 2.0 GHz */
	 933000,	/* 1.9 GHz */
	 800000,	/* 1.8 GHz */
	 800000,	/* 1.7 MHz */
	 733000,	/* 1.6 GHz */
	 733000,	/* 1.5 GHz */
	 733000,	/* 1.4 GHz */
	 667000,	/* 1.3 GHz */
	 667000,	/* 1.2 GHz */
	 533000,	/* 1.1 GHz */
	 533000,	/* 1.0 GHz */
	 400000,	/* 900 MHz */
	 400000,	/* 800 MHz */
	 400000,	/* 700 MHz */
	 400000,	/* 600 MHz */
	 400000,	/* 500 MHz */
	 400000,	/* 400 MHz */
	 400000,	/* 300 MHz */
	 400000,	/* 200 MHz */
};

static void exynos5430_set_clkdiv_CA15(unsigned int div_index)
{
	unsigned int tmp, tmp1;

	/* Change Divider - EGL0 */
	tmp = exynos5430_clkdiv_table_CA15[div_index].clkdiv0;

	__raw_writel(tmp, EXYNOS5430_DIV_EGL0);

	while (__raw_readl(EXYNOS5430_DIV_STAT_EGL0) & 0x111111)
		cpu_relax();

	/* Change Divider - EGL1 */
	tmp1 = exynos5430_clkdiv_table_CA15[div_index].clkdiv1;

	__raw_writel(tmp1, EXYNOS5430_DIV_EGL1);

	while (__raw_readl(EXYNOS5430_DIV_STAT_EGL1) & 0x11)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5430_DIV_EGL0);
	tmp1 = __raw_readl(EXYNOS5430_DIV_EGL1);

	pr_info("%s: DIV_EGL0[0x%08x], DIV_EGL1[0x%08x]\n",
					__func__, tmp, tmp1);
#endif
}

static void exynos5430_set_egl_pll_CA15(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;

	/* 1. before change to BUS_PLL, set div for BUS_PLL output */
	if ((new_index < L17) && (old_index < L17))
		exynos5430_set_clkdiv_CA15(L17); /* pll_safe_idx of CA15 */

	/* 2. CLKMUX_SEL_EGL2 = MOUT_BUS_PLL_USER, EGLCLK uses BUS_PLL_USER for lock time */
	if (clk_set_parent(mout_egl2, mout_bus_pll_user))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_bus_pll_user->name, mout_egl2->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_SRC_STAT_EGL2);
		tmp &= EXYNOS5430_SRC_STAT_EGL2_EGL2_MASK;
		tmp >>= EXYNOS5430_SRC_STAT_EGL2_EGL2_SHIFT;
	} while (tmp != 0x2);

	/* 3. Set EGL_PLL Lock time */
	pdiv = ((exynos5430_egl_pll_pms_table_CA15[new_index] &
		EXYNOS5430_PLL_PDIV_MASK) >> EXYNOS5430_PLL_PDIV_SHIFT);

	__raw_writel((pdiv * 150), EXYNOS5430_EGL_PLL_LOCK);

	/* 4. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5430_EGL_PLL_CON0);
	tmp &= ~(EXYNOS5430_PLL_MDIV_MASK |
		EXYNOS5430_PLL_PDIV_MASK |
		EXYNOS5430_PLL_SDIV_MASK);
	tmp |= exynos5430_egl_pll_pms_table_CA15[new_index];
	__raw_writel(tmp, EXYNOS5430_EGL_PLL_CON0);

	/* 5. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_EGL_PLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5430_EGL_PLL_CON0_LOCKED_SHIFT)));

	/* 6. CLKMUX_SEL_EGL2 = MOUT_EGL_PLL */
	if (clk_set_parent(mout_egl2, mout_egl1))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_egl1->name, mout_egl2->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_SRC_STAT_EGL2);
		tmp &= EXYNOS5430_SRC_STAT_EGL2_EGL2_MASK;
		tmp >>= EXYNOS5430_SRC_STAT_EGL2_EGL2_SHIFT;
	} while (tmp != 0x1);

	/* 7. restore original div value */
	if ((new_index < L17) && (old_index < L17))
		exynos5430_set_clkdiv_CA15(new_index);
}

static bool exynos5430_pms_change_CA15(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm = (exynos5430_egl_pll_pms_table_CA15[old_index] >>
				EXYNOS5430_PLL_PDIV_SHIFT);
	unsigned int new_pm = (exynos5430_egl_pll_pms_table_CA15[new_index] >>
				EXYNOS5430_PLL_PDIV_SHIFT);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos5430_set_frequency_CA15(unsigned int old_index,
					 unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos5430_pms_change_CA15(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos5430_set_clkdiv_CA15(new_index);
			/* 2. Change just s value in egl_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5430_EGL_PLL_CON0);
			tmp &= ~EXYNOS5430_PLL_SDIV_MASK;
			tmp |= (exynos5430_egl_pll_pms_table_CA15[new_index] & EXYNOS5430_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5430_EGL_PLL_CON0);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos5430_set_clkdiv_CA15(new_index);
			/* 2. Change the egl_pll m,p,s value */
			exynos5430_set_egl_pll_CA15(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5430_pms_change_CA15(old_index, new_index)) {
			/* 1. Change just s value in egl_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5430_EGL_PLL_CON0);
			tmp &= ~EXYNOS5430_PLL_SDIV_MASK;
			tmp |= (exynos5430_egl_pll_pms_table_CA15[new_index] & EXYNOS5430_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5430_EGL_PLL_CON0);
			/* 2. Change the system clock divider values */
			exynos5430_set_clkdiv_CA15(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the egl_pll m,p,s value */
			exynos5430_set_egl_pll_CA15(new_index, old_index);
			/* 2. Change the system clock divider values */
			exynos5430_set_clkdiv_CA15(new_index);
		}
	}

	clk_set_rate(fout_egl_pll, exynos5430_freq_table_CA15[new_index].frequency * 1000);
}

static void __init set_volt_table_CA15(void)
{
	unsigned int i;
	unsigned int asv_volt = 0;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA15; i++) {
		asv_volt = get_match_volt(ID_ARM, exynos5430_freq_table_CA15[i].frequency);
		if (!asv_volt)
			exynos5430_volt_table_CA15[i] = asv_voltage_5430_CA15[i];
		else
			exynos5430_volt_table_CA15[i] = asv_volt;

		pr_info("CPUFREQ of CA15  L%d : %d uV\n", i,
				exynos5430_volt_table_CA15[i]);
	}

	exynos5430_freq_table_CA15[L0].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L1].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L2].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L3].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L4].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L5].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L6].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L7].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L8].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L9].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L10].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L11].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L12].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L13].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L14].frequency = CPUFREQ_ENTRY_INVALID;
	max_support_idx_CA15 = L15;
	min_support_idx_CA15 = L18;
	exynos5430_freq_table_CA15[L19].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L20].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L21].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L22].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5430_freq_table_CA15[L23].frequency = CPUFREQ_ENTRY_INVALID;
}

int __init exynos5_cpufreq_CA15_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;

	set_volt_table_CA15();

	mout_egl2 = __clk_lookup("mout_egl2");
	if (!mout_egl2) {
		pr_err("failed get mout_egl2 clk\n");
		return -EINVAL;
	}

	mout_egl1 = __clk_lookup("mout_egl1");
	if (!mout_egl1) {
		pr_err("failed get mout_egl1 clk\n");
		goto err_mout_egl1;
	}

	mout_egl_pll = __clk_lookup("mout_egl_pll");
	if (!mout_egl_pll) {
		pr_err("failed get mout_egl_pll clk\n");
		goto err_mout_egl_pll;
	}

	if (clk_set_parent(mout_egl1, mout_egl_pll)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_egl_pll->name, mout_egl1->name);
		goto err_clk_set_parent_egl;
	}

	sclk_bus_pll = __clk_lookup("mout_bus_pll_sub");
	if (!sclk_bus_pll) {
		pr_err("failed get mout_bus_pll_sub clk\n");
		goto err_sclk_bus_pll;
	}

	mout_bus_pll_user = __clk_lookup("mout_bus_pll_user_egl");
	if (!mout_bus_pll_user) {
		pr_err("failed get mout_bus_pll_user_egl clk\n");
		goto err_mout_bus_pll_user;
	}

	if (clk_set_parent(mout_bus_pll_user, sclk_bus_pll)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_bus_pll->name, mout_bus_pll_user->name);
		goto err_clk_set_parent_bus_pll;
	}

	rate = clk_get_rate(mout_bus_pll_user) / 1000;

	fout_egl_pll = __clk_lookup("fout_egl_pll");
	if (!fout_egl_pll) {
		pr_err("failed get fout_egl_pll clk\n");
		goto err_fout_egl_pll;
	}

	clk_put(sclk_bus_pll);
	clk_put(mout_egl_pll);

	for (i = L0; i < CPUFREQ_LEVEL_END_CA15; i++) {
		exynos5430_clkdiv_table_CA15[i].index = i;

		/* CLK_DIV_EGL0 */
		tmp = __raw_readl(EXYNOS5430_DIV_EGL0);

		tmp &= ~(EXYNOS5430_DIV_EGL0_EGL1_MASK |
			EXYNOS5430_DIV_EGL0_EGL2_MASK |
			EXYNOS5430_DIV_EGL0_ACLK_EGL_MASK |
			EXYNOS5430_DIV_EGL0_PCLK_EGL_MASK |
			EXYNOS5430_DIV_EGL0_ATCLK_EGL_MASK |
			EXYNOS5430_DIV_EGL0_PCLK_DBG_EGL_MASK);

		tmp |= ((clkdiv_cpu0_5430_CA15[i][0] << EXYNOS5430_DIV_EGL0_EGL1_SHIFT) |
			(clkdiv_cpu0_5430_CA15[i][1] << EXYNOS5430_DIV_EGL0_EGL2_SHIFT) |
			(clkdiv_cpu0_5430_CA15[i][2] << EXYNOS5430_DIV_EGL0_ACLK_EGL_SHIFT) |
			(clkdiv_cpu0_5430_CA15[i][3] << EXYNOS5430_DIV_EGL0_PCLK_EGL_SHIFT) |
			(clkdiv_cpu0_5430_CA15[i][4] << EXYNOS5430_DIV_EGL0_ATCLK_EGL_SHIFT) |
			(clkdiv_cpu0_5430_CA15[i][5] << EXYNOS5430_DIV_EGL0_PCLK_DBG_EGL_SHIFT));

		exynos5430_clkdiv_table_CA15[i].clkdiv0 = tmp;

		/* CLK_DIV_EGL1 */
		tmp = __raw_readl(EXYNOS5430_DIV_EGL1);

		tmp &= ~(EXYNOS5430_DIV_EGL1_EGL_PLL_MASK |
			EXYNOS5430_DIV_EGL1_SCLK_HPM_EGL_MASK);

		tmp |= ((clkdiv_cpu1_5430_CA15[i][0] << EXYNOS5430_DIV_EGL1_EGL_PLL_SHIFT) |
			(clkdiv_cpu1_5430_CA15[i][1] << EXYNOS5430_DIV_EGL1_SCLK_HPM_EGL_SHIFT));

		exynos5430_clkdiv_table_CA15[i].clkdiv1 = tmp;
	}

	info->mpll_freq_khz = rate;
	info->pm_lock_idx = L0;
	info->pll_safe_idx = L17;
	info->max_support_idx = max_support_idx_CA15;
	info->min_support_idx = min_support_idx_CA15;
	info->bus_table = exynos5430_bus_table_CA15;
	info->cpu_clk = fout_egl_pll;

	info->volt_table = exynos5430_volt_table_CA15;
	info->freq_table = exynos5430_freq_table_CA15;
	info->set_freq = exynos5430_set_frequency_CA15;
	info->need_apll_change = exynos5430_pms_change_CA15;

#ifdef ENABLE_CLKOUT
	/* dividing EGL_CLK to 1/16 */
	tmp = __raw_readl(EXYNOS5430_CLKOUT_CMU_EGL);
	tmp &= ~0xfff;
	tmp |= 0xf02;
	__raw_writel(tmp, EXYNOS5430_CLKOUT_CMU_EGL);
#endif

	return 0;

err_fout_egl_pll:
err_clk_set_parent_bus_pll:
	clk_put(mout_bus_pll_user);
err_mout_bus_pll_user:
	clk_put(sclk_bus_pll);
err_sclk_bus_pll:
err_clk_set_parent_egl:
	clk_put(mout_egl_pll);
err_mout_egl_pll:
	clk_put(mout_egl1);
err_mout_egl1:
	clk_put(mout_egl2);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
