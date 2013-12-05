#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sort.h>
#include <linux/reboot.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

#include <mach/cpufreq.h>
#include <linux/suspend.h>

#define NORMALMIN_FREQ	500000
#define POLLING_MSEC	100

struct cpu_load_info {
	cputime64_t cpu_idle;
	cputime64_t cpu_iowait;
	cputime64_t cpu_wall;
	cputime64_t cpu_nice;
};

static DEFINE_PER_CPU(struct cpu_load_info, cur_cpu_info);
static DEFINE_MUTEX(dm_hotplug_lock);

static int cpu_util[NR_CPUS];
static unsigned int cur_load_freq = 0;
static bool lcd_is_on;
static bool forced_hotplug;
static bool do_enable_hotplug;
static bool do_disable_hotplug;
static bool big_hotpluged;
static bool do_big_hotplug;
static bool in_low_power_mode;

enum hotplug_mode {
	CHP_NORMAL = 1,
	CHP_LOW_POWER,
	CHP_BIG_IN,
	CHP_BIG_OUT,
};

static int dynamic_hotplug(enum hotplug_mode mode);

static enum hotplug_mode prev_mode;
static unsigned int delay = POLLING_MSEC;

static bool dm_hotplug_enable;

static bool exynos_dm_hotplug_enabled(void)
{
	return dm_hotplug_enable;
}

static void exynos_dm_hotplug_enable(void)
{
	mutex_lock(&dm_hotplug_lock);
	if (exynos_dm_hotplug_enabled()) {
		pr_info("%s: dynamic hotplug already enabled\n",
				__func__);
		mutex_unlock(&dm_hotplug_lock);
		return;
	}
	dm_hotplug_enable = true;
	mutex_unlock(&dm_hotplug_lock);
}

static void exynos_dm_hotplug_disable(void)
{
	mutex_lock(&dm_hotplug_lock);
	if (!exynos_dm_hotplug_enabled()) {
		pr_info("%s: dynamic hotplug already disabled\n",
				__func__);
		mutex_unlock(&dm_hotplug_lock);
		return;
	}
	dm_hotplug_enable = false;
	mutex_unlock(&dm_hotplug_lock);
}

#ifdef CONFIG_PM
static ssize_t show_enable_dm_hotplug(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	bool enabled = exynos_dm_hotplug_enabled();

	return snprintf(buf, 10, "%s\n", enabled ? "enabled" : "disabled");
}

static ssize_t store_enable_dm_hotplug(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int enable_input;

	if (!sscanf(buf, "%d", &enable_input))
		return -EINVAL;

	if (enable_input > 1 || enable_input < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, enable_input);
		return -EINVAL;
	}

	if (enable_input) {
		do_enable_hotplug = true;
		exynos_dm_hotplug_enable();

		dynamic_hotplug(CHP_BIG_OUT);
		do_enable_hotplug = false;
	} else {
		do_disable_hotplug = true;
		dynamic_hotplug(CHP_NORMAL);
		prev_mode = CHP_NORMAL;

		exynos_dm_hotplug_disable();
		do_disable_hotplug = false;
	}

	return count;
}

static struct global_attr enable_dm_hotplug =
		__ATTR(enable_dm_hotplug, S_IRUGO | S_IWUSR,
			show_enable_dm_hotplug, store_enable_dm_hotplug);
#endif

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static int fb_state_change(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	struct fb_info *info = evdata->info;
	unsigned int blank;

	if (val != FB_EVENT_BLANK &&
		val != FB_R_EARLY_EVENT_BLANK)
		return 0;

	/*
	 * If FBNODE is not zero, it is not primary display(LCD)
	 * and don't need to process these scheduling.
	 */
	if (info->node)
		return NOTIFY_OK;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		lcd_is_on = false;
		pr_info("LCD is off\n");
		break;
	case FB_BLANK_UNBLANK:
		/*
		 * LCD blank CPU qos is set by exynos-ikcs-cpufreq
		 * This line of code release max limit when LCD is
		 * turned on.
		 */
		lcd_is_on = true;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
	.notifier_call = fb_state_change,
};

static int __ref __cpu_hotplug(struct cpumask *be_out_cpus,
				bool out_flag, enum hotplug_mode mode)
{
	int i = 0;
	int ret = 0;
	int hotplug_cpus = NR_CPUS;
	int end_hotplug_cpu = 1;

	mutex_lock(&dm_hotplug_lock);
	if (!exynos_dm_hotplug_enabled() ||
			cpumask_weight(be_out_cpus) >= NR_CPUS) {
		ret = -EPERM;
		goto out;
	}

	if (big_hotpluged) {
		if (out_flag) {
			if (mode == CHP_BIG_OUT && in_low_power_mode)
				goto out;

			if (do_enable_hotplug) {
				if (!in_low_power_mode)
					end_hotplug_cpu = NR_CPUS - NR_CA15;
				hotplug_cpus = NR_CPUS;
			} else {
				hotplug_cpus = NR_CPUS - NR_CA15;
			}
		} else {
			if (mode == CHP_BIG_IN && in_low_power_mode)
				goto out;

			if (do_disable_hotplug ||
				(mode == CHP_BIG_IN && do_big_hotplug)) {
				if (!in_low_power_mode)
					end_hotplug_cpu = 1;
				hotplug_cpus = NR_CPUS;
			} else {
				hotplug_cpus = NR_CPUS - NR_CA15;
			}
		}
	}

	if (do_big_hotplug) {
		if (in_low_power_mode)
			goto out;

		end_hotplug_cpu = NR_CPUS - NR_CA15;
	}

	if (do_enable_hotplug && !big_hotpluged)
		goto out;

	if (out_flag) {
		for (i = hotplug_cpus - 1; i >= end_hotplug_cpu; i--) {
			ret = cpu_down(i);
			if (ret)
				break;
		}
	} else {
		if (big_hotpluged && !do_disable_hotplug) {
			for (i = end_hotplug_cpu; i < hotplug_cpus; i++) {
				ret = cpu_up(i);
				if (ret)
					break;
			}
		} else {
			for (i = end_hotplug_cpu + (NR_CA7 - 1); i < hotplug_cpus; i++) {
				ret = cpu_up(i);
				if (ret)
					break;
			}
		}

		if (big_hotpluged && !in_low_power_mode)
			goto out;

		if (big_hotpluged && !do_disable_hotplug) {
			for (i = end_hotplug_cpu; i < hotplug_cpus; i++) {
				ret = cpu_up(i);
				if (ret)
					break;
			}
		} else {
			for (i = end_hotplug_cpu; i < (hotplug_cpus - NR_CA15); i++) {
				ret = cpu_up(i);
				if (ret)
					break;
			}
		}
	}

out:
	mutex_unlock(&dm_hotplug_lock);

	return ret;
}

int force_dynamic_hotplug(bool out_flag)
{
	int ret = 0;
	struct cpumask out_target;

	cpumask_clear(&out_target);

	ret = __cpu_hotplug(&out_target, out_flag, CHP_LOW_POWER);

	prev_mode = CHP_LOW_POWER;
	forced_hotplug = out_flag;

	return ret;
}

static int dynamic_hotplug(enum hotplug_mode mode)
{
	int i;
	struct cpumask out_target;
	enum hotplug_mode ret = 0;

	cpumask_clear(&out_target);

	switch (mode) {
	case CHP_BIG_OUT:
		delay = POLLING_MSEC;
		for (i = 1; i < NR_CPUS; i++)
			cpumask_set_cpu(i, &out_target);
		ret = __cpu_hotplug(&out_target, true, mode);
		break;
	case CHP_BIG_IN:
		delay = POLLING_MSEC;
		if (cpumask_weight(cpu_online_mask) < NR_CPUS)
			ret = __cpu_hotplug(&out_target, false, mode);
		break;
	case CHP_LOW_POWER:
		delay = POLLING_MSEC;
		for (i = 1; i < NR_CPUS; i++)
			cpumask_set_cpu(i, &out_target);
		ret = __cpu_hotplug(&out_target, true, mode);
		in_low_power_mode = true;
		break;
	case CHP_NORMAL:
	default:
		delay = POLLING_MSEC;
		if (cpumask_weight(cpu_online_mask) < NR_CPUS)
			ret = __cpu_hotplug(&out_target, false, mode);
		in_low_power_mode = false;
		break;
	}

	return ret;
}

int big_cores_hotplug(bool out_flag)
{
	int ret;

	do_big_hotplug = true;

	if (out_flag) {
		ret = dynamic_hotplug(CHP_BIG_OUT);
		if (!ret)
			big_hotpluged = true;
	} else {
		ret = dynamic_hotplug(CHP_BIG_IN);
		if (!ret)
			big_hotpluged = false;
	}

	do_big_hotplug = false;

	return ret;
}

static int exynos_dm_hotplug_notifier(struct notifier_block *notifier,
					unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		exynos_dm_hotplug_disable();
		break;

	case PM_POST_SUSPEND:
		exynos_dm_hotplug_enable();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_dm_hotplug_nb = {
	.notifier_call = exynos_dm_hotplug_notifier,
	.priority = 1,
};

static int exynos_dm_hotplut_reboot_notifier(struct notifier_block *this,
				unsigned long code, void *_cmd)
{
	switch (code) {
	case SYSTEM_POWER_OFF:
	case SYS_RESTART:
		exynos_dm_hotplug_disable();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_dm_hotplug_reboot_nb = {
	.notifier_call = exynos_dm_hotplut_reboot_notifier,
};

static int low_stay = 0;

static enum hotplug_mode diagnose_condition(void)
{
	int ret = prev_mode;

	if (cur_load_freq > NORMALMIN_FREQ) {
		low_stay = 0;
		ret = CHP_NORMAL;
	} else if (cur_load_freq <= NORMALMIN_FREQ && low_stay <= 5) {
		low_stay++;
	}
	if ((low_stay > 5) && (!lcd_is_on || forced_hotplug))
		ret = CHP_LOW_POWER;
	else
		ret = CHP_NORMAL;

	return ret;
}

static void calc_load(void)
{
	struct cpufreq_policy *policy;
	unsigned int cpu_util_sum = 0;
	int cpu = 0;
	unsigned int i;

	policy = cpufreq_cpu_get(cpu);

	if (!policy) {
		pr_err("Invalid policy\n");
		return;
	}

	cur_load_freq = 0;

	for_each_cpu(i, policy->cpus) {
		struct cpu_load_info	*i_load_info;
		cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load, load_freq;

		i_load_info = &per_cpu(cur_cpu_info, i);

		cur_idle_time = get_cpu_idle_time(i, &cur_wall_time);
		cur_iowait_time = get_cpu_iowait_time(i, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - i_load_info->cpu_wall);
		i_load_info->cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - i_load_info->cpu_idle);
		i_load_info->cpu_idle = cur_idle_time;

		iowait_time = (unsigned int)
			(cur_iowait_time - i_load_info->cpu_iowait);
		i_load_info->cpu_iowait = cur_iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;
		cpu_util[i] = load;
		cpu_util_sum += load;

		load_freq = load * policy->cur;

		if (policy->cur > cur_load_freq)
			cur_load_freq = policy->cur;
	}

	cpufreq_cpu_put(policy);
	return;
}

static int thread_run_flag;

static int on_run(void *data)
{
	int on_cpu = 0;
	enum hotplug_mode exe_mode;

	struct cpumask thread_cpumask;

	cpumask_clear(&thread_cpumask);
	cpumask_set_cpu(on_cpu, &thread_cpumask);
	sched_setaffinity(0, &thread_cpumask);

	prev_mode = CHP_NORMAL;
	thread_run_flag = 1;

	while (thread_run_flag) {
		calc_load();
		exe_mode = diagnose_condition();

		if (exe_mode != prev_mode) {
#ifdef DM_HOTPLUG_DEBUG
			pr_info("frequency info : %d, prev_mode %d, exe_mode %d\n",
					cur_load_freq, prev_mode, exe_mode);
#endif
			if (!exynos_dm_hotplug_enabled())
				continue;

			if (dynamic_hotplug(exe_mode) < 0)
				exe_mode = prev_mode;
		}

		prev_mode = exe_mode;
		msleep(delay);
	}

	return 0;
}

void dm_cpu_hotplug_exit(void)
{
	thread_run_flag = 0;
}

static int __init dm_cpu_hotplug_init(void)
{
	struct task_struct *k;
	int ret = 0;

	k = kthread_run(&on_run, NULL, "thread_hotplug_func");
	if (IS_ERR(k)) {
		pr_err("Failed in creation of thread.\n");
		return -EINVAL;
	}

	fb_register_client(&fb_block);
	lcd_is_on = true;
	forced_hotplug = false;
	do_enable_hotplug = false;
	do_disable_hotplug = false;

	dm_hotplug_enable = true;
	big_hotpluged = false;
	do_big_hotplug = false;
	in_low_power_mode = false;

#ifdef CONFIG_PM
	ret = sysfs_create_file(power_kobj, &enable_dm_hotplug.attr);
	if (ret) {
		pr_err("%s: failed to create enable_dm_hotplug sysfs interface\n",
			__func__);
		goto err_enable_dm_hotplug;
	}
#endif

	register_pm_notifier(&exynos_dm_hotplug_nb);
	register_reboot_notifier(&exynos_dm_hotplug_reboot_nb);

	return ret;

err_enable_dm_hotplug:
	fb_unregister_client(&fb_block);
	kthread_stop(k);
	put_task_struct(k);

	return ret;
}

late_initcall(dm_cpu_hotplug_init);
