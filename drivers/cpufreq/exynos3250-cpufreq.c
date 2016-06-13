/*
 * Copyright (c) 2010-2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS3250 - CPU frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "exynos-cpufreq.h"
#include <soc/samsung/asv-exynos.h>

extern void __iomem *reg_base;
#define EXYNOS3_CLK_MUX_STAT_CPU		(reg_base + 0x14400)
#define EXYNOS3_CLK_DIV_CPU0			(reg_base + 0x14500)
#define EXYNOS3_CLK_DIV_CPU1			(reg_base + 0x14504)
#define EXYNOS3_CLK_DIV_STAT_CPU0		(reg_base + 0x14600)
#define EXYNOS3_CLK_DIV_STAT_CPU1		(reg_base + 0x14604)

#define EXYNOS3_CLKSRC_CPU_MUXCORE_SHIFT	(16)
#define EXYNOS3_CLKDIV_CPU0_CORE_SHIFT		(0)
#define EXYNOS3_CLKDIV_CPU0_CORE_MASK		(0x7 << EXYNOS3_CLKDIV_CPU0_CORE_SHIFT)
#define EXYNOS3_CLKDIV_CPU0_COREM0_SHIFT	(4)
#define EXYNOS3_CLKDIV_CPU0_COREM0_MASK		(0x7 << EXYNOS3_CLKDIV_CPU0_COREM0_SHIFT)
#define EXYNOS3_CLKDIV_CPU0_ATB_SHIFT		(16)
#define EXYNOS3_CLKDIV_CPU0_ATB_MASK		(0x7 << EXYNOS3_CLKDIV_CPU0_ATB_SHIFT)
#define EXYNOS3_CLKDIV_CPU0_PCLKDBG_SHIFT	(20)
#define EXYNOS3_CLKDIV_CPU0_PCLKDBG_MASK	(0x7 << EXYNOS3_CLKDIV_CPU0_PCLKDBG_SHIFT)
#define EXYNOS3_CLKDIV_CPU0_APLL_SHIFT		(24)
#define EXYNOS3_CLKDIV_CPU0_APLL_MASK		(0x7 << EXYNOS3_CLKDIV_CPU0_APLL_SHIFT)
#define EXYNOS3_CLKDIV_CPU0_CORE2_SHIFT		28
#define EXYNOS3_CLKDIV_CPU0_CORE2_MASK		(0x7 << EXYNOS3_CLKDIV_CPU0_CORE2_SHIFT)
#define EXYNOS3_CLKDIV_CPU1_COPY_SHIFT		0
#define EXYNOS3_CLKDIV_CPU1_COPY_MASK		(0x7 << EXYNOS3_CLKDIV_CPU1_COPY_SHIFT)
#define EXYNOS3_CLKDIV_CPU1_HPM_SHIFT		4
#define EXYNOS3_CLKDIV_CPU1_HPM_MASK		(0x7 << EXYNOS3_CLKDIV_CPU1_HPM_SHIFT)

#define CPUFREQ_LEVEL_END	(L9 + 1)

static struct clk *cpu_clk;
static struct clk *mout_core;
static struct clk *mout_mpll;
static struct clk *mout_apll;

static unsigned int exynos3250_volt_table[CPUFREQ_LEVEL_END] = {
	1050000,
	1000000,
	 950000,
	 900000,
	 900000,
	 900000,
	 900000,
	 900000,
	 900000,
	 900000,
};

static struct cpufreq_frequency_table exynos3250_freq_table[] = {
	{0, L0, 1000 * 1000},
	{0, L1,  900 * 1000},
	{0, L2,  800 * 1000},
	{0, L3,  700 * 1000},
	{0, L4,  600 * 1000},
	{0, L5,  500 * 1000},
	{0, L6,  400 * 1000},
	{0, L7,  300 * 1000},
	{0, L8,  200 * 1000},
	{0, L9,  100 * 1000},
	{0, 0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos3250_clkdiv_table[CPUFREQ_LEVEL_END];

static unsigned int clkdiv_cpu0_3250[CPUFREQ_LEVEL_END][6] = {
	/*
	 * Clock divider value for following
	 * { DIVCORE, DIVCOREM,
	 *		DIVATB, DIVPCLK_DBG, DIVAPLL, DIVCORE2 }
	 */

	/* ARM L0: 1000Mhz */
	{ 0, 1, 4, 7, 1, 0 },

	/* ARM L1: 900MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L2: 800MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L3: 700MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L4: 600MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L5: 500MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L6: 400MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L7: 300MHz */
	{ 0, 1, 3, 5, 1, 0 },

	/* ARM L8: 200MHz */
	{ 0, 1, 3, 3, 1, 0 },

	/* ARM L9: 100MHz */
	{ 0, 1, 1, 1, 1, 0 },
};

static unsigned int clkdiv_cpu1_3250[CPUFREQ_LEVEL_END][2] = {
	/*
	 *Clock divider value for following
	 * { DIVCOPY, DIVHPM }
	 */
	/* ARM L0: 1000MHz */
	{ 7, 7 },

	/* ARM L1: 900MHz */
	{ 7, 7 },

	/* ARM L2: 800MHz */
	{ 7, 7 },

	/* ARM L3: 700MHz */
	{ 7, 7 },

	/* ARM L4: 600MHz */
	{ 7, 7 },

	/* ARM L5: 500MHz */
	{ 7, 7 },

	/* ARM L6: 400MHz */
	{ 7, 7 },

	/* ARM L7: 300MHz */
	{ 7, 7 },

	/* ARM L8: 200MHz */
	{ 7, 7 },

	/* ARM L9: 100MHz */
	{ 7, 7 },

};

static unsigned int exynos3250_apll_pms_table[CPUFREQ_LEVEL_END] = {

	/* APLL FOUT L0: 1000MHz */
	((250<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L1: 900MHz */
	((300<<16)|(4<<8)|(0x1)),

	/* APLL FOUT L2: 800MHz */
	((200<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L3: 700MHz */
	((175<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L4: 600MHz */
	((400<<16)|(4<<8)|(0x2)),

	/* APLL FOUT L5: 500MHz */
	((250<<16)|(3<<8)|(0x2)),

	/* APLL FOUT L6: 400MHz */
	((200<<16)|(3<<8)|(0x2)),

	/* APLL FOUT L7: 300MHz */
	((400<<16)|(4<<8)|(0x3)),

	/* APLL FOUT L8: 200MHz */
	((200<<16)|(3<<8)|(0x3)),

	/* APLL FOUT L9: 100MHz */
	((200<<16)|(3<<8)|(0x4)),
};

static struct pm_qos_request exynos3250_mif_qos;

static int exynos3250_mif_qos_table[CPUFREQ_LEVEL_END] = {
	[L0] = 200000,		/* KHz */
	[L1] = 200000,
	[L2] = 133000,
	[L3] = 133000,
	[L4] = 100000,
	[L5] = 0,
	[L6] = 0,
	[L7] = 0,
	[L8] = 0,
	[L9] = 0,
};

static void exynos3250_set_clkdiv(unsigned int div_index)
{
	unsigned int tmp;

	/* Change Divider - CPU0 */
	tmp = exynos3250_clkdiv_table[div_index].clkdiv0;

	__raw_writel(tmp, EXYNOS3_CLK_DIV_CPU0);

	while (__raw_readl(EXYNOS3_CLK_DIV_STAT_CPU0) & 0x11110011)
		cpu_relax();

	/* Change Divider - CPU1 */
	tmp = exynos3250_clkdiv_table[div_index].clkdiv1;

	__raw_writel(tmp, EXYNOS3_CLK_DIV_CPU1);

	while (__raw_readl(EXYNOS3_CLK_DIV_STAT_CPU1) & 0x11)
		cpu_relax();
}

static void exynos3250_set_apll(unsigned int index)
{
	unsigned int tmp;
	/*  MUX_CORE_SEL = MPLL, ARMCLK uses MPLL for lock time */
	clk_set_parent(mout_core, mout_mpll);

	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS3_CLK_MUX_STAT_CPU)
			>> EXYNOS3_CLKSRC_CPU_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x2);

	clk_set_rate(cpu_clk, exynos3250_freq_table[index].frequency * 1000);

	/*  MUX_CORE_SEL = APLL */
	clk_set_parent(mout_core, mout_apll);

	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS3_CLK_MUX_STAT_CPU)
			>> EXYNOS3_CLKSRC_CPU_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x1);
}

static void exynos3250_set_frequency(unsigned int old_index,
				  unsigned int new_index)
{
	if (old_index > new_index) {
		/* Clock Configuration Procedure */
		/* 1. Change the system clock divider values */
		exynos3250_set_clkdiv(new_index);
		/* 2. Change the apll m,p,s value */
		exynos3250_set_apll(new_index);
	} else if (old_index < new_index) {
		/* Clock Configuration Procedure */
		/* 1. Change the apll m,p,s value */
		exynos3250_set_apll(new_index);
		/* 2. Change the system clock divider values */
		exynos3250_set_clkdiv(new_index);
	}

	pm_qos_update_request(&exynos3250_mif_qos,
				exynos3250_mif_qos_table[new_index]);
}

bool exynos3250_pms_change(unsigned int old_index, unsigned int new_index)
{
	unsigned int old_pm = exynos3250_apll_pms_table[old_index] >> 8;
	unsigned int new_pm = exynos3250_apll_pms_table[new_index] >> 8;

	return (old_pm == new_pm) ? 0 : 1;
}

#ifdef CONFIG_EXYNOS_ASV
static unsigned int exynos3250_cpu_asv_abb[CPUFREQ_LEVEL_END];
int arm_lock;

static int __init set_volt_table(void)
{
	unsigned int i;

	for (i = 0; i < CPUFREQ_LEVEL_END; i++) {
		exynos3250_volt_table[i] = get_match_volt(ID_ARM,
					exynos3250_freq_table[i].frequency);

		if (arm_lock == 4) {
			if (i >= L7) exynos3250_volt_table[i] =	get_match_volt(
				ID_ARM, exynos3250_freq_table[L7].frequency);
		} else if (arm_lock == 5) {
			if (i >= L6) exynos3250_volt_table[i] = get_match_volt(
				ID_ARM, exynos3250_freq_table[L6].frequency);
		} else if (arm_lock == 6) {
			if (i >= L5) exynos3250_volt_table[i] = get_match_volt(
				ID_ARM, exynos3250_freq_table[L5].frequency);
		} else if (arm_lock == 7) {
			if (i >= L4) exynos3250_volt_table[i] = get_match_volt(
				ID_ARM, exynos3250_freq_table[L4].frequency);
		}

		if (exynos3250_volt_table[i] == 0) {
			pr_err("%s: invalid value\n", __func__);
			return -EINVAL;
		}

		exynos3250_cpu_asv_abb[i] = get_match_abb(ID_ARM,
					exynos3250_freq_table[i].frequency);

	}

	return 0;
}
#endif

static void show_freq_table(void){
	int i;

	for (i = 0; i < CPUFREQ_LEVEL_END; i++)
		pr_info("CPUFREQ L%d: %dKHz, %duV, ABB: %d\n", i,
					exynos3250_freq_table[i].frequency,
					exynos3250_volt_table[i],
#ifdef CONFIG_EXYNOS_ASV
					exynos3250_cpu_asv_abb[i]);
#else
					0);
#endif

}

int exynos3250_cpufreq_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;
#ifdef CONFIG_EXYNOS_LOCK_MAX_CPUFREQ
	unsigned int new_idx = 0, pll_safe_volt = 0;
#endif
	cpu_clk = clk_get(NULL, "fout_apll");
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

	mout_core = clk_get(NULL, "mout_core");
	if (IS_ERR(mout_core))
		goto err_moutcore;

	mout_mpll = clk_get(NULL, "mout_mpll_user_c");
	if (IS_ERR(mout_mpll))
		goto err_mout_mpll;

	rate = clk_get_rate(mout_mpll) / 1000;

	mout_apll = clk_get(NULL, "mout_apll");

	if (IS_ERR(mout_apll))
		goto err_mout_apll;

	for (i = L0; i <  CPUFREQ_LEVEL_END; i++) {
		exynos3250_clkdiv_table[i].index = i;

		tmp = __raw_readl(EXYNOS3_CLK_DIV_CPU0);

		tmp &= ~(EXYNOS3_CLKDIV_CPU0_CORE_MASK |
			EXYNOS3_CLKDIV_CPU0_COREM0_MASK |
			EXYNOS3_CLKDIV_CPU0_ATB_MASK |
			EXYNOS3_CLKDIV_CPU0_PCLKDBG_MASK |
			EXYNOS3_CLKDIV_CPU0_APLL_MASK |
			EXYNOS3_CLKDIV_CPU0_CORE2_MASK);

		tmp |= ((clkdiv_cpu0_3250[i][0] << EXYNOS3_CLKDIV_CPU0_CORE_SHIFT) |
			(clkdiv_cpu0_3250[i][1] << EXYNOS3_CLKDIV_CPU0_COREM0_SHIFT) |
			(clkdiv_cpu0_3250[i][2] << EXYNOS3_CLKDIV_CPU0_ATB_SHIFT) |
			(clkdiv_cpu0_3250[i][3] << EXYNOS3_CLKDIV_CPU0_PCLKDBG_SHIFT) |
			(clkdiv_cpu0_3250[i][4] << EXYNOS3_CLKDIV_CPU0_APLL_SHIFT) |
			(clkdiv_cpu0_3250[i][5] << EXYNOS3_CLKDIV_CPU0_CORE2_SHIFT));

		exynos3250_clkdiv_table[i].clkdiv0 = tmp;

		tmp = __raw_readl(EXYNOS3_CLK_DIV_CPU1);

		tmp &= ~(EXYNOS3_CLKDIV_CPU1_COPY_MASK |
			EXYNOS3_CLKDIV_CPU1_HPM_MASK);
		tmp |= ((clkdiv_cpu1_3250[i][0] << EXYNOS3_CLKDIV_CPU1_COPY_SHIFT) |
			(clkdiv_cpu1_3250[i][1] << EXYNOS3_CLKDIV_CPU1_HPM_SHIFT));
		exynos3250_clkdiv_table[i].clkdiv1 = tmp;
	}

	info->mpll_freq_khz = rate;
	info->pll_safe_idx = L6;
	info->max_support_idx = L0;
	info->min_support_idx = L9;
	info->cpu_clk = cpu_clk;
	info->freq_table = exynos3250_freq_table;
	info->set_freq = exynos3250_set_frequency;
	info->need_apll_change = exynos3250_pms_change;

#ifdef CONFIG_EXYNOS_ASV
	info->abb_table = exynos3250_cpu_asv_abb;
	set_volt_table();
#endif
	info->volt_table = exynos3250_volt_table;

	show_freq_table();

	pm_qos_add_request(&exynos3250_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);

	return 0;

err_mout_apll:
	clk_put(mout_mpll);
err_mout_mpll:
	clk_put(mout_core);
err_moutcore:
	clk_put(cpu_clk);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
EXPORT_SYMBOL(exynos3250_cpufreq_init);
