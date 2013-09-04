/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - MP CPU frequency scaling support for EXYNOS big.Little series
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/cpumask.h>

#include <asm/smp_plat.h>
#include <asm/cputype.h>

#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>
#include <mach/regs-pmu.h>
#include <plat/cpu.h>

#ifdef CONFIG_SMP
struct lpj_info {
	unsigned long   ref;
	unsigned int    freq;
};

static struct lpj_info global_lpj_ref;
#endif

/* For switcher */
static unsigned int freq_min[CA_END] __read_mostly;	/* Minimum (Big/Little) clock frequency */
static unsigned int freq_max[CA_END] __read_mostly;	/* Maximum (Big/Little) clock frequency */

/*
 * This value is based on the difference between the dmips value of A15/A7
 * It is used to revise cpu frequency when changing cluster
 */
#ifdef CONFIG_ARM_EXYNOS5430_CPUFREQ
#define STEP_LEVEL_CA7_MAX	650000
#define COLD_VOLT_OFFSET	37500
#define ENABLE_MIN_COLD		1
#define LIMIT_COLD_VOLTAGE	1250000
#define MIN_COLD_VOLTAGE	950000

#define CA7_L2_STATUS		EXYNOS5430_KFC_L2_STATUS
#define CA15_L2_STATUS		EXYNOS5430_EAGLE_L2_STATUS
#define L2_LOCAL_PWR_EN		0x7
#endif

static struct exynos_dvfs_info *exynos_info[CA_END];
static struct exynos_dvfs_info exynos_info_CA7;
static struct exynos_dvfs_info exynos_info_CA15;

static struct regulator *arm_regulator;
static struct regulator *kfc_regulator;
static unsigned int volt_offset;

static struct cpufreq_freqs *freqs[CA_END];

static DEFINE_MUTEX(cpufreq_lock);
static DEFINE_MUTEX(cpufreq_scale_lock);

static bool exynos_cpufreq_init_done;
static bool suspend_prepared = false;

/* Include CPU mask of each cluster */
cluster_type exynos_boot_cluster;
static cluster_type boot_cluster;
static struct cpumask cluster_cpus[CA_END];

DEFINE_PER_CPU(cluster_type, cpu_cur_cluster);

#if 0	/* FIXME */
static struct pm_qos_request boot_cpu_qos;
#endif
static struct pm_qos_request min_cpu_qos;
static struct pm_qos_request max_cpu_qos;
static struct pm_qos_request min_kfc_qos;
static struct pm_qos_request max_kfc_qos;
static struct pm_qos_request exynos_mif_qos_CA7;
static struct pm_qos_request exynos_mif_qos_CA15;

static struct workqueue_struct *cluster_monitor_wq;
static struct delayed_work monitor_cluster_on;
static bool CA7_cluster_on = false;
static bool CA15_cluster_on = false;

static unsigned int get_limit_voltage(unsigned int voltage)
{
	BUG_ON(!voltage);
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	if (ENABLE_MIN_COLD && volt_offset
		&& (voltage + volt_offset) < MIN_COLD_VOLTAGE)
		return MIN_COLD_VOLTAGE;

	return voltage + volt_offset;
}

static void init_cpumask_cluster_set(cluster_type cluster)
{
	unsigned int i;

	for_each_cpu(i, cpu_possible_mask) {
		if (cluster == CA7) {
			if (i >= NR_CA7) {
				cpumask_set_cpu(i, &cluster_cpus[CA15]);
				per_cpu(cpu_cur_cluster, i) = CA15;
			} else {
				cpumask_set_cpu(i, &cluster_cpus[CA7]);
				per_cpu(cpu_cur_cluster, i) = CA7;
			}
		} else {
			if (i >= NR_CA15) {
				cpumask_set_cpu(i, &cluster_cpus[CA7]);
				per_cpu(cpu_cur_cluster, i) = CA7;
			} else {
				cpumask_set_cpu(i, &cluster_cpus[CA15]);
				per_cpu(cpu_cur_cluster, i) = CA15;
			}
		}
	}
}

/*
 * get_cur_cluster - return current cluster
 *
 * You may reference this fuction directly, but it cannot be
 * standard of judging current cluster. If you make a decision
 * of operation by this function, it occurs system hang.
 */
static cluster_type get_cur_cluster(unsigned int cpu)
{
	return per_cpu(cpu_cur_cluster, cpu);
}

static void set_boot_freq(void)
{
	int i;

	for (i = 0; i < CA_END; i++) {
		if (exynos_info[i] == NULL)
			continue;

		exynos_info[i]->boot_freq
				= clk_get_rate(exynos_info[i]->cpu_clk) / 1000;
	}
}

static bool is_alive(unsigned int cluster)
{
	unsigned int tmp;
	tmp = __raw_readl(cluster == CA15 ? CA15_L2_STATUS : CA7_L2_STATUS) & L2_LOCAL_PWR_EN;

	return tmp ? true : false;
}

static void cluster_onoff_monitor(struct work_struct *work)
{
	struct cpufreq_frequency_table *CA15_freq_table = exynos_info[CA15]->freq_table;
	struct cpufreq_frequency_table *CA7_freq_table = exynos_info[CA7]->freq_table;
	unsigned int CA15_old_index = 0, CA7_old_index = 0;
	unsigned int freq;
	int i;

	if (exynos_info[CA15]->bus_table) {
		if (!is_alive(CA15) && CA15_cluster_on) {
			pm_qos_update_request(&exynos_mif_qos_CA15, 0);
			CA15_cluster_on = false;
		} else if (is_alive(CA15) && !CA15_cluster_on) {
			for (i = 0; (CA15_freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
				freq = CA15_freq_table[i].frequency;
				if (freq == CPUFREQ_ENTRY_INVALID)
					continue;
				if (freqs[CA15]->old == freq) {
					CA15_old_index = i;
					break;
				}
			}

			pm_qos_update_request(&exynos_mif_qos_CA15,
					exynos_info[CA15]->bus_table[CA15_old_index]);
			CA15_cluster_on = true;
		}
	}

	if (exynos_info[CA7]->bus_table) {
		if (!is_alive(CA7) && CA7_cluster_on) {
			pm_qos_update_request(&exynos_mif_qos_CA7, 0);
			CA7_cluster_on = false;
		} else if (is_alive(CA7) && !CA7_cluster_on) {
			for (i = 0; (CA7_freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
				freq = CA7_freq_table[i].frequency;
				if (freq == CPUFREQ_ENTRY_INVALID)
					continue;
				if (freqs[CA7]->old == freq) {
					CA7_old_index = i;
					break;
				}
			}

			pm_qos_update_request(&exynos_mif_qos_CA7,
					exynos_info[CA7]->bus_table[CA7_old_index]);
			CA7_cluster_on = true;
		}
	}

	queue_delayed_work_on(0, cluster_monitor_wq, &monitor_cluster_on, msecs_to_jiffies(100));
}

static unsigned int get_freq_volt(int cluster, unsigned int target_freq)
{
	int index;
	int i;

	struct cpufreq_frequency_table *table = exynos_info[cluster]->freq_table;

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		if (target_freq == freq) {
			index = i;
			break;
		}
	}

	if (table[i].frequency == CPUFREQ_TABLE_END)
		return -EINVAL;

	return exynos_info[cluster]->volt_table[index];
}

static unsigned int get_boot_freq(unsigned int cluster)
{
	if (exynos_info[cluster] == NULL)
		return 0;

	return exynos_info[cluster]->boot_freq;
}

static unsigned int get_boot_volt(int cluster)
{
	unsigned int boot_freq = get_boot_freq(cluster);

	return get_freq_volt(cluster, boot_freq);
}

int exynos_verify_speed(struct cpufreq_policy *policy)
{
	unsigned int cur = get_cur_cluster(policy->cpu);

	return cpufreq_frequency_table_verify(policy,
				exynos_info[cur]->freq_table);
}

unsigned int exynos_getspeed_cluster(cluster_type cluster)
{
	return clk_get_rate(exynos_info[cluster]->cpu_clk) / 1000;
}

unsigned int exynos_getspeed(unsigned int cpu)
{
	unsigned int cur = get_cur_cluster(cpu);

	return exynos_getspeed_cluster(cur);
}

static unsigned int exynos_get_safe_volt(unsigned int old_index,
					unsigned int new_index,
					unsigned int cur)
{
	unsigned int safe_arm_volt = 0;
	struct cpufreq_frequency_table *freq_table
					= exynos_info[cur]->freq_table;
	unsigned int *volt_table = exynos_info[cur]->volt_table;

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * reguired voltage level
	 */
	if (exynos_info[cur]->need_apll_change != NULL) {
		if (exynos_info[cur]->need_apll_change(old_index, new_index) &&
			(freq_table[new_index].frequency < exynos_info[cur]->mpll_freq_khz) &&
			(freq_table[old_index].frequency < exynos_info[cur]->mpll_freq_khz)) {
				safe_arm_volt = volt_table[exynos_info[cur]->pll_safe_idx];
		}
	}

	return safe_arm_volt;
}

/* Determine valid target frequency using freq_table */
int exynos5_frequency_table_target(struct cpufreq_policy *policy,
				   struct cpufreq_frequency_table *table,
				   unsigned int target_freq,
				   unsigned int relation,
				   unsigned int *index)
{
	unsigned int i;

	if (!cpu_online(policy->cpu))
		return -EINVAL;

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		if (target_freq == freq) {
			*index = i;
			break;
		}
	}

	if (table[i].frequency == CPUFREQ_TABLE_END)
		return -EINVAL;

	return 0;
}

static int exynos_cpufreq_scale(unsigned int target_freq,
				unsigned int curr_freq, unsigned int cpu)
{
	unsigned int cur = get_cur_cluster(cpu);
	struct cpufreq_frequency_table *freq_table = exynos_info[cur]->freq_table;
	unsigned int *volt_table = exynos_info[cur]->volt_table;
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
	struct regulator *regulator = exynos_info[cur]->regulator;
	unsigned int new_index, old_index;
	unsigned int volt, safe_volt = 0;
	int ret = 0;

	if (!policy) {
		ret = -EINVAL;
		goto out;
	}

	freqs[cur]->cpu = cpu;
	freqs[cur]->new = target_freq;

	if (exynos5_frequency_table_target(policy, freq_table,
				curr_freq, CPUFREQ_RELATION_L, &old_index)) {
		ret = -EINVAL;
		goto out;
	}

	if (exynos5_frequency_table_target(policy, freq_table,
				freqs[cur]->new, CPUFREQ_RELATION_L, &new_index)) {
		ret = -EINVAL;
		goto out;
	}

	if (old_index == new_index)
		goto out;

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * required voltage level
	 */
	safe_volt = exynos_get_safe_volt(old_index, new_index, cur);
	if (safe_volt)
		safe_volt = get_limit_voltage(safe_volt);

	volt = get_limit_voltage(volt_table[new_index]);

	/* Update policy current frequency */
	cpufreq_notify_transition(policy, freqs[cur], CPUFREQ_PRECHANGE);

	/* When the new frequency is higher than current frequency */
	if ((freqs[cur]->new > freqs[cur]->old) && !safe_volt){
		/* Firstly, voltage up to increase frequency */
		regulator_set_voltage(regulator, volt, volt);

		if (exynos_info[cur]->set_ema)
			exynos_info[cur]->set_ema(volt);
	}

	if (safe_volt) {
		regulator_set_voltage(regulator, safe_volt, safe_volt);

		if (exynos_info[cur]->set_ema)
			exynos_info[cur]->set_ema(safe_volt);
	}

	if (old_index > new_index) {
		if (cur == CA15) {
			if (pm_qos_request_active(&exynos_mif_qos_CA15))
				pm_qos_update_request(&exynos_mif_qos_CA15,
						exynos_info[cur]->bus_table[new_index]);
		} else {
			if (pm_qos_request_active(&exynos_mif_qos_CA7))
				pm_qos_update_request(&exynos_mif_qos_CA7,
						exynos_info[cur]->bus_table[new_index]);
		}
	}

	exynos_info[cur]->set_freq(old_index, new_index);

	if (old_index < new_index) {
		if (cur == CA15) {
			if (pm_qos_request_active(&exynos_mif_qos_CA15))
				pm_qos_update_request(&exynos_mif_qos_CA15,
						exynos_info[cur]->bus_table[new_index]);
		} else {
			if (pm_qos_request_active(&exynos_mif_qos_CA7))
				pm_qos_update_request(&exynos_mif_qos_CA7,
						exynos_info[cur]->bus_table[new_index]);
		}
	}

#ifdef CONFIG_SMP
	if (!global_lpj_ref.freq) {
		global_lpj_ref.ref = loops_per_jiffy;
		global_lpj_ref.freq = freqs[cur]->old;
	}

	loops_per_jiffy = cpufreq_scale(global_lpj_ref.ref,
			global_lpj_ref.freq, freqs[cur]->new);
#endif

	cpufreq_notify_transition(policy, freqs[cur], CPUFREQ_POSTCHANGE);

	/* When the new frequency is lower than current frequency */
	if ((freqs[cur]->new < freqs[cur]->old) ||
		((freqs[cur]->new > freqs[cur]->old) && safe_volt)) {
		/* down the voltage after frequency change */
		if (exynos_info[cur]->set_ema)
			 exynos_info[cur]->set_ema(volt);

		regulator_set_voltage(regulator, volt, volt);
	}

out:
	cpufreq_cpu_put(policy);
	return ret;
}

unsigned int g_cpufreq;
unsigned int g_kfcfreq;
/* Set clock frequency */
static int exynos_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	cluster_type cur = get_cur_cluster(policy->cpu);
	struct cpufreq_frequency_table *freq_table = exynos_info[cur]->freq_table;
	unsigned int index;
	int ret = 0;

	mutex_lock(&cpufreq_lock);

	if (exynos_info[cur]->blocked)
		goto out;

	/* verify old frequency */
	BUG_ON(freqs[cur]->old != exynos_getspeed(policy->cpu));

	if (cur == CA15) {
		target_freq = max((unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MIN), target_freq);
		target_freq = min((unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MAX), target_freq);
	} else {
		target_freq = max((unsigned int)pm_qos_request(PM_QOS_KFC_FREQ_MIN), target_freq);
		target_freq = min((unsigned int)pm_qos_request(PM_QOS_KFC_FREQ_MAX), target_freq);
	}

	if (cpufreq_frequency_table_target(policy, freq_table,
				target_freq, relation, &index)) {
		ret = -EINVAL;
		goto out;
	}

	pr_debug("%s[%d]: new_freq[%d], index[%d]\n",
				__func__, cur, target_freq, index);

	/* frequency and volt scaling */
	ret = exynos_cpufreq_scale(target_freq, freqs[cur]->old, policy->cpu);

	/* save current frequency */
	freqs[cur]->old = target_freq;

out:
	mutex_unlock(&cpufreq_lock);

	if (cur == CA15)
		g_cpufreq = target_freq;
	else
		g_kfcfreq = target_freq;

	return ret;
}

#ifdef CONFIG_PM
static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	freqs[CA7]->old = get_boot_freq(CA7);
	freqs[CA15]->old = get_boot_freq(CA15);
	return 0;
}
#endif

static int __cpuinit exynos_cpufreq_cpu_notifier(struct notifier_block *notifier,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;

	if (suspend_prepared)
		return NOTIFY_OK;

	dev = get_cpu_device(cpu);
	if (dev) {
		switch (action) {
		case CPU_DOWN_PREPARE:
		case CPU_DOWN_PREPARE_FROZEN:
			mutex_lock(&cpufreq_lock);
			exynos_info[CA7]->blocked = true;
			exynos_info[CA15]->blocked = true;
			mutex_unlock(&cpufreq_lock);
			break;
		case CPU_DOWN_FAILED:
		case CPU_DOWN_FAILED_FROZEN:
		case CPU_DEAD:
			mutex_lock(&cpufreq_lock);
			exynos_info[CA7]->blocked = false;
			exynos_info[CA15]->blocked = false;
			mutex_unlock(&cpufreq_lock);
			break;
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block __refdata exynos_cpufreq_cpu_nb = {
	.notifier_call = exynos_cpufreq_cpu_notifier,
};

/*
 * exynos_cpufreq_pm_notifier - block CPUFREQ's activities in suspend-resume
 *			context
 * @notifier
 * @pm_event
 * @v
 *
 * While cpufreq_disable == true, target() ignores every frequency but
 * boot_freq. The boot_freq value is the initial frequency,
 * which is set by the bootloader. In order to eliminate possible
 * inconsistency in clock values, we save and restore frequencies during
 * suspend and resume and block CPUFREQ activities. Note that the standard
 * suspend/resume cannot be used as they are too deep (syscore_ops) for
 * regulator actions.
 */
static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	unsigned int freqCA7, freqCA15;
	unsigned int bootfreqCA7, bootfreqCA15;
	int volt;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&cpufreq_lock);
		exynos_info[CA7]->blocked = true;
		exynos_info[CA15]->blocked = true;
		mutex_unlock(&cpufreq_lock);

		bootfreqCA7 = get_boot_freq(CA7);
		bootfreqCA15 = get_boot_freq(CA15);

		freqCA7 = freqs[CA7]->old;
		freqCA15 = freqs[CA15]->old;

		volt = max(get_boot_volt(CA7),
				get_freq_volt(CA7, freqCA7));
		BUG_ON(volt <= 0);
		volt = get_limit_voltage(volt);

		if (regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt))
			goto err;

		volt = max(get_boot_volt(CA15),
				get_freq_volt(CA15, freqCA15));
		BUG_ON(volt <= 0);
		volt = get_limit_voltage(volt);

		if (regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt))
			goto err;

		suspend_prepared = true;

		pr_debug("PM_SUSPEND_PREPARE for CPUFREQ\n");

		break;
	case PM_POST_SUSPEND:
		pr_debug("PM_POST_SUSPEND for CPUFREQ\n");

		mutex_lock(&cpufreq_lock);
		exynos_info[CA7]->blocked = false;
		exynos_info[CA15]->blocked = false;
		mutex_unlock(&cpufreq_lock);

		suspend_prepared = false;

		break;
	}
	return NOTIFY_OK;
err:
	pr_err("%s: failed to set voltage\n", __func__);

	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpufreq_nb = {
	.notifier_call = exynos_cpufreq_pm_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos_cpufreq_tmu_notifier(struct notifier_block *notifier,
				       unsigned long event, void *v)
{
	int volt;
	int *on = v;

#if 0 /* FIXME */
	if (event != TMU_COLD)
		return NOTIFY_OK;
#endif

	mutex_lock(&cpufreq_lock);
	if (*on) {
		if (volt_offset)
			goto out;
		else
			volt_offset = COLD_VOLT_OFFSET;

		volt = get_limit_voltage(regulator_get_voltage(exynos_info[CA15]->regulator));
		regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt);
		if (exynos_info[CA15]->set_ema)
			exynos_info[CA15]->set_ema(volt);

		volt = get_limit_voltage(regulator_get_voltage(exynos_info[CA7]->regulator));
		regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt);
		if (exynos_info[CA7]->set_ema)
			exynos_info[CA7]->set_ema(volt);
	} else {
		if (!volt_offset)
			goto out;
		else
			volt_offset = 0;

		volt = get_limit_voltage(regulator_get_voltage(exynos_info[CA15]->regulator)
							- COLD_VOLT_OFFSET);
		if (exynos_info[CA15]->set_ema)
			exynos_info[CA15]->set_ema(volt);
		regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt);

		volt = get_limit_voltage(regulator_get_voltage(exynos_info[CA7]->regulator)
							- COLD_VOLT_OFFSET);
		regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt);
		if (exynos_info[CA7]->set_ema)
			exynos_info[CA7]->set_ema(volt);
	}

out:
	mutex_unlock(&cpufreq_lock);

	return NOTIFY_OK;
}

static struct notifier_block exynos_tmu_nb = {
	.notifier_call = exynos_cpufreq_tmu_notifier,
};
#endif

static int exynos_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	unsigned int cur = get_cur_cluster(policy->cpu);

	pr_debug("%s: cpu[%d]\n", __func__, policy->cpu);

	policy->cur = policy->min = policy->max = exynos_getspeed(policy->cpu);

	cpufreq_frequency_table_get_attr(exynos_info[cur]->freq_table, policy->cpu);

	/* set the transition latency value */
	policy->cpuinfo.transition_latency = 100000;

	if (cpumask_test_cpu(policy->cpu, &cluster_cpus[CA15])) {
		cpumask_copy(policy->cpus, &cluster_cpus[CA15]);
		cpumask_copy(policy->related_cpus, &cluster_cpus[CA15]);
	} else {
		cpumask_copy(policy->cpus, &cluster_cpus[CA7]);
		cpumask_copy(policy->related_cpus, &cluster_cpus[CA7]);
	}

	if (boot_cluster == CA7) {
		if (policy->cpu >= NR_CA7)
#if defined(CONFIG_CPU_FREQ_DEFAULT_GOV_INTERACTIVE)
			policy->governor = &cpufreq_gov_interactive_eagle;
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND)
			policy->governor = &cpufreq_gov_ondemand_eagle;
#endif
	} else {
		if (policy->cpu < NR_CA15)
#if defined(CONFIG_CPU_FREQ_DEFAULT_GOV_INTERACTIVE)
			policy->governor = &cpufreq_gov_interactive_eagle;
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND)
			policy->governor = &cpufreq_gov_ondemand_eagle;
#endif
	}

	return cpufreq_frequency_table_cpuinfo(policy, exynos_info[cur]->freq_table);
}

static struct cpufreq_driver exynos_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= exynos_verify_speed,
	.target		= exynos_target,
	.get		= exynos_getspeed,
	.init		= exynos_cpufreq_cpu_init,
	.name		= "exynos_cpufreq",
#ifdef CONFIG_PM
	.suspend	= exynos_cpufreq_suspend,
	.resume		= exynos_cpufreq_resume,
#endif
};

/************************** sysfs interface ************************/

static ssize_t show_cpu_freq_table(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	int i, count = 0;
	size_t tbl_sz = 0, pr_len;
	struct cpufreq_frequency_table *freq_table = exynos_info[CA15]->freq_table;

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;
	pr_len = (size_t)((PAGE_SIZE - 2) / tbl_sz);

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
						freq_table[i].frequency);
        }

        count += snprintf(&buf[count], 2, "\n");
        return count;
}

static ssize_t show_cpu_min_freq(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MIN));
}

static ssize_t show_cpu_max_freq(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MAX));
}

static ssize_t store_cpu_min_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = min(input, (int)freq_max[CA15]);

	if (pm_qos_request_active(&min_cpu_qos))
		pm_qos_update_request(&min_cpu_qos, input);
	else
		pm_qos_add_request(&min_cpu_qos, PM_QOS_CPU_FREQ_MIN, input);

	return count;
}

static ssize_t store_cpu_max_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = max(input, (int)freq_min[CA15]);

	if (pm_qos_request_active(&max_cpu_qos))
		pm_qos_update_request(&max_cpu_qos, input);
	else
		pm_qos_add_request(&max_cpu_qos, PM_QOS_CPU_FREQ_MAX, input);

	return count;
}

static ssize_t show_kfc_freq_table(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	int i, count = 0;
	size_t tbl_sz = 0, pr_len;
	struct cpufreq_frequency_table *freq_table = exynos_info[CA7]->freq_table;

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;
	pr_len = (size_t)((PAGE_SIZE - 2) / tbl_sz);

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
						freq_table[i].frequency);
        }

        count += snprintf(&buf[count], 2, "\n");
        return count;
}

static ssize_t show_kfc_min_freq(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (unsigned int)pm_qos_request(PM_QOS_KFC_FREQ_MIN));
}

static ssize_t show_kfc_max_freq(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (unsigned int)pm_qos_request(PM_QOS_KFC_FREQ_MAX));
}

static ssize_t store_kfc_min_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = min(input, (int)freq_max[CA7]);

	if (pm_qos_request_active(&min_kfc_qos))
		pm_qos_update_request(&min_kfc_qos, input);
	else
		pm_qos_add_request(&min_kfc_qos, PM_QOS_KFC_FREQ_MIN, input);

	return count;
}

static ssize_t store_kfc_max_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = max(input, (int)freq_min[CA7]);

	if (pm_qos_request_active(&max_kfc_qos))
		pm_qos_update_request(&max_kfc_qos, input);
	else
		pm_qos_add_request(&max_kfc_qos, PM_QOS_KFC_FREQ_MAX, input);

	return count;
}

define_one_global_ro(cpu_freq_table);
define_one_global_rw(cpu_min_freq);
define_one_global_rw(cpu_max_freq);
define_one_global_ro(kfc_freq_table);
define_one_global_rw(kfc_min_freq);
define_one_global_rw(kfc_max_freq);

static struct attribute *mp_attributes[] = {
	&cpu_freq_table.attr,
	&cpu_min_freq.attr,
	&cpu_max_freq.attr,
	&kfc_freq_table.attr,
	&kfc_min_freq.attr,
	&kfc_max_freq.attr,
	NULL
};

static struct attribute_group mp_attr_group = {
	.attrs = mp_attributes,
	.name = "mp-cpufreq",
};

#ifdef CONFIG_PM
static struct global_attr cpufreq_cpu_table =
		__ATTR(cpufreq_cpu_table, S_IRUGO, show_cpu_freq_table, NULL);
static struct global_attr cpufreq_cpu_min_limit =
		__ATTR(cpufreq_cpu_min_limit, S_IRUGO | S_IWUSR,
			show_cpu_min_freq, store_cpu_min_freq);
static struct global_attr cpufreq_cpu_max_limit =
		__ATTR(cpufreq_cpu_max_limit, S_IRUGO | S_IWUSR,
			show_cpu_max_freq, store_cpu_max_freq);
static struct global_attr cpufreq_kfc_table =
		__ATTR(cpufreq_kfc_table, S_IRUGO, show_kfc_freq_table, NULL);
static struct global_attr cpufreq_kfc_min_limit =
		__ATTR(cpufreq_kfc_min_limit, S_IRUGO | S_IWUSR,
			show_kfc_min_freq, store_kfc_min_freq);
static struct global_attr cpufreq_kfc_max_limit =
		__ATTR(cpufreq_kfc_max_limit, S_IRUGO | S_IWUSR,
			show_kfc_max_freq, store_kfc_max_freq);
#endif

/************************** sysfs end ************************/

static int exynos_cpufreq_reboot_notifier_call(struct notifier_block *this,
				   unsigned long code, void *_cmd)
{
	unsigned int freqCA7, freqCA15;
	unsigned int bootfreqCA7, bootfreqCA15;
	int volt;

	mutex_lock(&cpufreq_lock);
	exynos_info[CA7]->blocked = true;
	exynos_info[CA15]->blocked = true;
	mutex_unlock(&cpufreq_lock);

	bootfreqCA7 = get_boot_freq(CA7);
	bootfreqCA15 = get_boot_freq(CA15);

	freqCA7 = freqs[CA7]->old;
	freqCA15 = freqs[CA15]->old;

	volt = max(get_boot_volt(CA7),
			get_freq_volt(CA7, freqCA7));
	volt = get_limit_voltage(volt);

	if (regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt))
		goto err;

	if (exynos_info[CA7]->set_ema)
		exynos_info[CA7]->set_ema(volt);

	volt = max(get_boot_volt(CA15),
			get_freq_volt(CA15, freqCA15));
	volt = get_limit_voltage(volt);

	if (regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt))
		goto err;

	if (exynos_info[CA15]->set_ema)
		exynos_info[CA15]->set_ema(volt);

	return NOTIFY_DONE;
err:
	pr_err("%s: failed to set voltage\n", __func__);

	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpufreq_reboot_notifier = {
	.notifier_call = exynos_cpufreq_reboot_notifier_call,
};

static int exynos_cpu_min_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? 0 : NR_CA7;

	freq = exynos_getspeed(cpu);

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpu_min_qos_notifier = {
	.notifier_call = exynos_cpu_min_qos_handler,
};

static int exynos_cpu_max_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? 0 : NR_CA7;

	freq = exynos_getspeed(cpu);
	if (freq <= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpu_max_qos_notifier = {
	.notifier_call = exynos_cpu_max_qos_handler,
};

static int exynos_kfc_min_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? NR_CA15 : 0;

	freq = exynos_getspeed(cpu);

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_kfc_min_qos_notifier = {
	.notifier_call = exynos_kfc_min_qos_handler,
};

static int exynos_kfc_max_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? NR_CA15 : 0;

	freq = exynos_getspeed(cpu);
	if (freq <= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_kfc_max_qos_notifier = {
	.notifier_call = exynos_kfc_max_qos_handler,
};

static int __init exynos_cpufreq_init(void)
{
	int ret = -EINVAL;

	boot_cluster = 0;

	exynos_info[CA7] = kzalloc(sizeof(struct exynos_dvfs_info), GFP_KERNEL);
	if (!exynos_info[CA7]) {
		ret = -ENOMEM;
		goto err_alloc_info_CA7;
	}

	exynos_info[CA15] = kzalloc(sizeof(struct exynos_dvfs_info), GFP_KERNEL);
	if (!exynos_info[CA15]) {
		ret = -ENOMEM;
		goto err_alloc_info_CA15;
	}

	freqs[CA7] = kzalloc(sizeof(struct cpufreq_freqs), GFP_KERNEL);
	if (!freqs[CA7]) {
		ret = -ENOMEM;
		goto err_alloc_freqs_CA7;
	}

	freqs[CA15] = kzalloc(sizeof(struct cpufreq_freqs), GFP_KERNEL);
	if (!freqs[CA15]) {
		ret = -ENOMEM;
		goto err_alloc_freqs_CA15;
	}

	/* Get to boot_cluster_num - 0 for CA7; 1 for CA15 */
	boot_cluster = !MPIDR_AFFINITY_LEVEL(cpu_logical_map(0), 1);
	pr_debug("%s: boot_cluster is %s\n", __func__,
					boot_cluster == CA7 ? "CA7" : "CA15");
	exynos_boot_cluster = boot_cluster;

	init_cpumask_cluster_set(boot_cluster);

	ret = exynos5_cpufreq_CA7_init(&exynos_info_CA7);
	if (ret)
		goto err_init_cpufreq;

	ret = exynos5_cpufreq_CA15_init(&exynos_info_CA15);
	if (ret)
		goto err_init_cpufreq;

	arm_regulator = regulator_get(NULL, "vdd_eagle");
	if (IS_ERR(arm_regulator)) {
		pr_err("%s: failed to get resource vdd_eagle\n", __func__);
		goto err_vdd_eagle;
	}

	kfc_regulator = regulator_get(NULL, "vdd_kfc");
	if (IS_ERR(kfc_regulator)) {
		pr_err("%s:failed to get resource vdd_kfc\n", __func__);
		goto err_vdd_kfc;
	}

	memcpy(exynos_info[CA7], &exynos_info_CA7,
				sizeof(struct exynos_dvfs_info));
	exynos_info[CA7]->regulator = kfc_regulator;

	memcpy(exynos_info[CA15], &exynos_info_CA15,
				sizeof(struct exynos_dvfs_info));
	exynos_info[CA15]->regulator = arm_regulator;

	if (exynos_info[CA7]->set_freq == NULL) {
		pr_err("%s: No set_freq function (ERR)\n", __func__);
		goto err_set_freq;
	}

	freq_max[CA15] = exynos_info[CA15]->
		freq_table[exynos_info[CA15]->max_support_idx].frequency;
	freq_min[CA15] = exynos_info[CA15]->
		freq_table[exynos_info[CA15]->min_support_idx].frequency;
	freq_max[CA7] = exynos_info[CA7]->
		freq_table[exynos_info[CA7]->max_support_idx].frequency;
	freq_min[CA7] = exynos_info[CA7]->
		freq_table[exynos_info[CA7]->min_support_idx].frequency;

	set_boot_freq();

	/* set initial old frequency */
	freqs[CA7]->old = exynos_getspeed_cluster(CA7);
	freqs[CA15]->old = exynos_getspeed_cluster(CA15);

	register_hotcpu_notifier(&exynos_cpufreq_cpu_nb);
	register_pm_notifier(&exynos_cpufreq_nb);
	register_reboot_notifier(&exynos_cpufreq_reboot_notifier);
#if 0 /* FIXME */
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_add_notifier(&exynos_tmu_nb);
#endif
#endif

	pm_qos_add_notifier(PM_QOS_CPU_FREQ_MIN, &exynos_cpu_min_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CPU_FREQ_MAX, &exynos_cpu_max_qos_notifier);
	pm_qos_add_notifier(PM_QOS_KFC_FREQ_MIN, &exynos_kfc_min_qos_notifier);
	pm_qos_add_notifier(PM_QOS_KFC_FREQ_MAX, &exynos_kfc_max_qos_notifier);

	if (cpufreq_register_driver(&exynos_driver)) {
		pr_err("%s: failed to register cpufreq driver\n", __func__);
		goto err_cpufreq;
	}

	ret = sysfs_create_group(cpufreq_global_kobject, &mp_attr_group);
	if (ret) {
		pr_err("%s: failed to create iks-cpufreq sysfs interface\n", __func__);
		goto err_mp_attr;
	}

#ifdef CONFIG_PM
	ret = sysfs_create_file(power_kobj, &cpufreq_cpu_table.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_cpu_table sysfs interface\n", __func__);
		goto err_cpu_table;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_cpu_min_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_cpu_min_limit sysfs interface\n", __func__);
		goto err_cpu_min_limit;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_cpu_max_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_cpu_max_limit sysfs interface\n", __func__);
		goto err_cpu_max_limit;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_kfc_table.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_kfc_table sysfs interface\n", __func__);
		goto err_kfc_table;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_kfc_min_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_kfc_min_limit sysfs interface\n", __func__);
		goto err_kfc_min_limit;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_kfc_max_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_kfc_max_limit sysfs interface\n", __func__);
		goto err_kfc_max_limit;
	}
#endif

#if 0	/* FIXME */
	pm_qos_add_request(&boot_cpu_qos, PM_QOS_CPU_FREQ_MIN, 0);
	pm_qos_update_request_timeout(&boot_cpu_qos, 1000000, 40000 * 1000);
#endif

	if (exynos_info[CA7]->bus_table)
		pm_qos_add_request(&exynos_mif_qos_CA7, PM_QOS_BUS_THROUGHPUT, 0);
	if (exynos_info[CA15]->bus_table)
		pm_qos_add_request(&exynos_mif_qos_CA15, PM_QOS_BUS_THROUGHPUT, 0);

	if (exynos_info[CA7]->bus_table || exynos_info[CA15]->bus_table) {
		INIT_DEFERRABLE_WORK(&monitor_cluster_on, cluster_onoff_monitor);

		cluster_monitor_wq = create_workqueue("cluster_monitor");
		if (!cluster_monitor_wq) {
			pr_err("%s: failed to create cluster_monitor_wq\n", __func__);
			goto err_workqueue;
		}

		queue_delayed_work_on(0, cluster_monitor_wq, &monitor_cluster_on,
						msecs_to_jiffies(100));
	}

	exynos_cpufreq_init_done = true;

	return 0;

err_workqueue:
	if (exynos_info[CA15]->bus_table)
		pm_qos_remove_request(&exynos_mif_qos_CA15);
	if (exynos_info[CA7]->bus_table)
		pm_qos_remove_request(&exynos_mif_qos_CA7);
#if 0	/* FIXME */
	pm_qos_remove_request(&boot_cpu_qos);
#endif
#ifdef CONFIG_PM
err_kfc_max_limit:
	sysfs_remove_file(power_kobj, &cpufreq_kfc_min_limit.attr);
err_kfc_min_limit:
	sysfs_remove_file(power_kobj, &cpufreq_kfc_table.attr);
err_kfc_table:
	sysfs_remove_file(power_kobj, &cpufreq_cpu_max_limit.attr);
err_cpu_max_limit:
	sysfs_remove_file(power_kobj, &cpufreq_cpu_min_limit.attr);
err_cpu_min_limit:
	sysfs_remove_file(power_kobj, &cpufreq_cpu_table.attr);
err_cpu_table:
	sysfs_remove_group(cpufreq_global_kobject, &mp_attr_group);
#endif
err_mp_attr:
	cpufreq_unregister_driver(&exynos_driver);
err_cpufreq:
	pm_qos_remove_notifier(PM_QOS_CPU_FREQ_MIN, &exynos_cpu_min_qos_notifier);
	pm_qos_remove_notifier(PM_QOS_CPU_FREQ_MAX, &exynos_cpu_max_qos_notifier);
	pm_qos_remove_notifier(PM_QOS_KFC_FREQ_MIN, &exynos_kfc_min_qos_notifier);
	pm_qos_remove_notifier(PM_QOS_KFC_FREQ_MAX, &exynos_kfc_max_qos_notifier);
	unregister_reboot_notifier(&exynos_cpufreq_reboot_notifier);
	unregister_pm_notifier(&exynos_cpufreq_nb);
	unregister_hotcpu_notifier(&exynos_cpufreq_cpu_nb);
err_set_freq:
	regulator_put(kfc_regulator);
err_vdd_kfc:
	regulator_put(arm_regulator);
err_vdd_eagle:
err_init_cpufreq:
	kfree(freqs[CA15]);
err_alloc_freqs_CA15:
	kfree(freqs[CA7]);
err_alloc_freqs_CA7:
	kfree(exynos_info[CA15]);
err_alloc_info_CA15:
	kfree(exynos_info[CA7]);
err_alloc_info_CA7:
	pr_err("%s: failed initialization\n", __func__);

	return ret;
}

late_initcall(exynos_cpufreq_init);
