/*
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - CPU frequency scaling support for EXYNOS series
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
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "exynos-cpufreq.h"
#include <soc/samsung/asv-exynos.h>

static struct exynos_dvfs_info *exynos_info;
static struct regulator *arm_regulator;
static unsigned int locking_frequency;
static DEFINE_MUTEX(cpufreq_lock);

static int exynos_cpufreq_get_index(unsigned int freq)
{
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
	struct cpufreq_frequency_table *pos;

	cpufreq_for_each_entry(pos, freq_table)
		if (pos->frequency == freq)
			break;

	if (pos->frequency == CPUFREQ_TABLE_END)
		return -EINVAL;

	return pos - freq_table;
}

static int exynos_cpufreq_scale(unsigned int target_freq, bool use_target_index)
{
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
	unsigned int *volt_table = exynos_info->volt_table;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	unsigned int arm_volt, safe_arm_volt = 0;
	unsigned int mpll_freq_khz = exynos_info->mpll_freq_khz;
	struct device *dev = exynos_info->dev;
	int new_index, old_index;
	struct cpufreq_freqs freqs;
	int ret = 0;

	if (!use_target_index) {
		freqs.cpu = policy->cpu;
		freqs.old = exynos_info->old_freq;
		freqs.new = target_freq;
	}

	/*
	 * The policy max have been changed so that we cannot get proper
	 * old_index with cpufreq_frequency_table_target(). Thus, ignore
	 * policy and get the index from the raw frequency table.
	 */
	old_index = exynos_cpufreq_get_index(exynos_info->old_freq);
	if (old_index < 0) {
		ret = old_index;
		goto out;
	}

	new_index = exynos_cpufreq_get_index(target_freq);
	if (new_index < 0 || new_index == old_index) {
		ret = new_index;
		goto out;
	}

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * required voltage level
	 */
	if (exynos_info->need_apll_change != NULL) {
		if (exynos_info->need_apll_change(old_index, new_index) &&
		   (freq_table[new_index].frequency < mpll_freq_khz) &&
		   (freq_table[old_index].frequency < mpll_freq_khz))
			safe_arm_volt = volt_table[exynos_info->pll_safe_idx];
	}
	arm_volt = volt_table[new_index];

	if (!use_target_index)
		cpufreq_freq_transition_begin(policy, &freqs);

	/* When the new frequency is higher than current frequency */
	if ((old_index > new_index) && !safe_arm_volt) {
		/* Firstly, voltage up to increase frequency */
		ret = regulator_set_voltage(arm_regulator, arm_volt, arm_volt);
		if (ret) {
			dev_err(dev, "failed to set cpu voltage to %d\n",
				arm_volt);
			goto fail_dvfs;
		}
#if defined(CONFIG_EXYNOS_ASV)
		exynos_set_abb(ID_ARM, exynos_info->abb_table[new_index]);
#endif
	}

	if (safe_arm_volt) {
		ret = regulator_set_voltage(arm_regulator, safe_arm_volt, safe_arm_volt);
		if (ret) {
			dev_err(dev, "failed to set cpu voltage to %d\n",
				safe_arm_volt);
			goto fail_dvfs;
		}
	}

	exynos_info->set_freq(old_index, new_index);

	if (!use_target_index)
		cpufreq_freq_transition_end(policy, &freqs, 0);

	/* When the new frequency is lower than current frequency */
	if ((old_index < new_index) ||
	   ((old_index > new_index) && safe_arm_volt)) {
		/* down the voltage after frequency change */
		ret = regulator_set_voltage(arm_regulator, arm_volt, arm_volt);
		if (ret) {
			dev_err(dev, "failed to set cpu voltage to %d\n",
				arm_volt);
			goto out;
		}
#if defined(CONFIG_EXYNOS_ASV)
		exynos_set_abb(ID_ARM, exynos_info->abb_table[new_index]);
#endif
	}

	cpufreq_cpu_put(policy);

	return 0;

fail_dvfs:
	cpufreq_freq_transition_end(policy, &freqs, ret);
out:
	cpufreq_cpu_put(policy);

	return ret;
}

#ifdef CONFIG_ARM_EXYNOS3250_CPUFREQ
static int exynos_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
	unsigned int index;
	unsigned int new_freq;
	int ret = 0;

	mutex_lock(&cpufreq_lock);

	if (cpufreq_frequency_table_target(policy, freq_table,
					   target_freq, relation, &index)) {
		ret = -EINVAL;
		goto out;
	}

	new_freq = freq_table[index].frequency;

	ret = exynos_cpufreq_scale(new_freq, 0);
	if (ret) {
		pr_info("failed exynos_cpufrqe_scale(%d)\n", ret);
		goto out;
	}

	/* update old freq */
	exynos_info->old_freq = new_freq;
out:

	mutex_unlock(&cpufreq_lock);

	return ret;
}
#else
static int exynos_target(struct cpufreq_policy *policy, unsigned int index)
{
	return exynos_cpufreq_scale(exynos_info->freq_table[index].frequency, 1);
}
#endif

static int exynos_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	policy->cur = exynos_info->old_freq = policy->suspend_freq = locking_frequency;
	policy->clk = exynos_info->cpu_clk;
	return cpufreq_generic_init(policy, exynos_info->freq_table, 100000);
}

static struct cpufreq_driver exynos_driver = {
	.flags		= CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.verify		= cpufreq_generic_frequency_table_verify,
#ifdef CONFIG_ARM_EXYNOS3250_CPUFREQ
	.target		= exynos_target,
#else
	.target_index	= exynos_target,
#endif
	.get		= cpufreq_generic_get,
	.init		= exynos_cpufreq_cpu_init,
	.name		= "exynos_cpufreq",
	.attr		= cpufreq_generic_attr,
#ifdef CONFIG_ARM_EXYNOS_CPU_FREQ_BOOST_SW
	.boost_supported = true,
#endif
#ifdef CONFIG_PM
	.suspend	= cpufreq_generic_suspend,
#endif
};

static int exynos_cpufreq_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;

	exynos_info = kzalloc(sizeof(*exynos_info), GFP_KERNEL);
	if (!exynos_info)
		return -ENOMEM;

	exynos_info->dev = &pdev->dev;

	if (of_machine_is_compatible("samsung,exynos4210")) {
		exynos_info->type = EXYNOS_SOC_4210;
		ret = exynos4210_cpufreq_init(exynos_info);
	} else if (of_machine_is_compatible("samsung,exynos4212")) {
		exynos_info->type = EXYNOS_SOC_4212;
		ret = exynos4x12_cpufreq_init(exynos_info);
	} else if (of_machine_is_compatible("samsung,exynos4412")) {
		exynos_info->type = EXYNOS_SOC_4412;
		ret = exynos4x12_cpufreq_init(exynos_info);
	} else if (of_machine_is_compatible("samsung,exynos5250")) {
		exynos_info->type = EXYNOS_SOC_5250;
		ret = exynos5250_cpufreq_init(exynos_info);
	} else if (of_machine_is_compatible("samsung,exynos3250")) {
		exynos_info->type = EXYNOS_SOC_3250;
		ret = exynos3250_cpufreq_init(exynos_info);
	} else {
		pr_err("%s: Unknown SoC type\n", __func__);
		return -ENODEV;
	}

	if (ret)
		goto err_vdd_arm;

	if (exynos_info->set_freq == NULL) {
		dev_err(&pdev->dev, "No set_freq function (ERR)\n");
		goto err_vdd_arm;
	}

	arm_regulator = regulator_get(NULL, "vdd_arm");
	if (IS_ERR(arm_regulator)) {
		dev_err(&pdev->dev, "failed to get resource vdd_arm\n");
		goto err_vdd_arm;
	}

	/* Done here as we want to capture boot frequency */
	locking_frequency = clk_get_rate(exynos_info->cpu_clk) / 1000;

	if (!cpufreq_register_driver(&exynos_driver)) {
		pr_info("CPUFreq initialized complete\n");
		return 0;
	}

	dev_err(&pdev->dev, "failed to register cpufreq driver\n");
	regulator_put(arm_regulator);
err_vdd_arm:
	kfree(exynos_info);
	return -EINVAL;
}

static struct platform_driver exynos_cpufreq_platdrv = {
	.driver = {
		.name	= "exynos-cpufreq",
		.owner	= THIS_MODULE,
	},
	.probe = exynos_cpufreq_probe,
};
module_platform_driver(exynos_cpufreq_platdrv);
