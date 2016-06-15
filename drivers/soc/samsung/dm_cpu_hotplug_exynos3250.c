#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/suspend.h>
#include <soc/samsung/exynos-psmw.h>

#define NORMALMIN_FREQ	500000
#define POLLING_MSEC	200
#define LOW_STAY_THRSHD	3
#define HI_CNT_THRSHD	3

static DEFINE_MUTEX(dm_hotplug_lock);
#ifdef CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
static struct task_struct *dm_hotplug_task;
static struct psmw_listener_info psmw_dm_cpu_info;
extern int ambient_enter;
#endif

static unsigned int cur_load_freq;

enum hotplug_mode {
	CHP_NORMAL,
	CHP_LOW_POWER,
};

static enum hotplug_mode prev_mode;
static bool exynos_dm_hotplug_disable;

static int __ref __cpu_hotplug(struct cpumask *be_out_cpus)
{
	int i = 0;
	int ret = 0;

	mutex_lock(&dm_hotplug_lock);
	if (exynos_dm_hotplug_disable ||
			cpumask_weight(be_out_cpus) >= NR_CPUS) {
		ret = -EPERM;
		goto out;
	}

	for (i = 1; i < NR_CPUS; i++) {
		if (cpumask_test_cpu(i, be_out_cpus)) {
			PSMW_DBG("CPU[%d] DOWN \n", i);
			ret = cpu_down(i);
			if (ret)
				break;
		} else {
			PSMW_DBG("CPU[%d] UP \n", i);
			ret = cpu_up(i);
			if (ret)
				break;
		}
	}

out:
	mutex_unlock(&dm_hotplug_lock);

	return ret;
}

static int dynamic_hotplug(enum hotplug_mode mode)
{
	int i;
	struct cpumask out_target;
	enum hotplug_mode ret = 0;

	cpumask_clear(&out_target);

	switch (mode) {
	case CHP_LOW_POWER:
		for (i = 1; i < NR_CPUS; i++)
			cpumask_set_cpu(i, &out_target);
		ret = __cpu_hotplug(&out_target);
		break;
	case CHP_NORMAL:
	default:
		if (cpumask_weight(cpu_online_mask) < NR_CPUS)
			ret = __cpu_hotplug(&out_target);
		break;
	}

	return ret;
}

static int exynos_dm_hotplug_notifier(struct notifier_block *notifier,
					unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&dm_hotplug_lock);
		exynos_dm_hotplug_disable = true;
		mutex_unlock(&dm_hotplug_lock);
		break;

	case PM_POST_SUSPEND:
		mutex_lock(&dm_hotplug_lock);
		exynos_dm_hotplug_disable = false;
		mutex_unlock(&dm_hotplug_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_dm_hotplug_nb = {
	.notifier_call = exynos_dm_hotplug_notifier,
	.priority = 1,
};

static int low_stay = 0;
static int prev_ret = 0;
#ifdef CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
static int hi_cnt = 0;
#endif

static enum hotplug_mode diagnose_condition(void)
{
	int ret;
	unsigned int normal_max_freq = cpufreq_interactive_get_hispeed_freq(0);
#ifdef CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
	PSMW_DBG("\ncur_load_freq [%d] MHz  \n", cur_load_freq/1000);
	if (!atomic_read(&psmw_dm_cpu_info.is_vsync_requested) || ambient_enter) {
		low_stay = LOW_STAY_THRSHD + 1;
		hi_cnt = 0;
		prev_ret = CHP_LOW_POWER;
		PSMW_DBG("just return CHP_LOW_POWER\n");
		return CHP_LOW_POWER;
	}
#endif

	ret = CHP_NORMAL;

#ifdef CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
	if (atomic_read(&psmw_dm_cpu_info.is_vsync_requested)) {
		if (cur_load_freq >= NORMALMIN_FREQ) {
			low_stay = 0;
			if (hi_cnt <= HI_CNT_THRSHD)
				hi_cnt++;
		}
		else if (low_stay <= LOW_STAY_THRSHD) {
			low_stay++;
			hi_cnt = 0;
		}
	} else {
		if (cur_load_freq >= normal_max_freq)
			low_stay = 0;
			if (hi_cnt <= HI_CNT_THRSHD)
				hi_cnt++;
		else if (low_stay <= LOW_STAY_THRSHD) {
			low_stay += 2;
			hi_cnt = 0;
		}
	}
	if (low_stay >= LOW_STAY_THRSHD) {
		ret = CHP_LOW_POWER;
	}
	else if (!low_stay) {
		if (prev_ret == CHP_LOW_POWER && hi_cnt < HI_CNT_THRSHD)
			ret = CHP_LOW_POWER;
	}
	else {
		if (prev_ret == CHP_LOW_POWER && low_stay < LOW_STAY_THRSHD)
			ret = CHP_LOW_POWER;
	}
#else
	if (cur_load_freq >= normal_max_freq)
		low_stay = 0;
	else if (low_stay <= LOW_STAY_THRSHD)
		low_stay++;
	if (low_stay >= LOW_STAY_THRSHD)
		ret = CHP_LOW_POWER;
#endif	// CONFIG_EXYNOS_PSMW_CPU_HOTPLUG

	PSMW_DBG("low_stay [%d] hi_cnt [%d] ,return [%s] \n", low_stay, hi_cnt,
					(ret == CHP_LOW_POWER) ? "CHP_LOW_POWER" :" CHP_NORMAL");
	prev_ret = ret;
	return ret;
}

static void calc_load(void)
{
	struct cpufreq_policy *policy;
	int cpu = 0;

	policy = cpufreq_cpu_get(cpu);

	if (!policy) {
		pr_err("Invalid policy\n");
		return;
	}

	cur_load_freq = policy->cur;

	cpufreq_cpu_put(policy);
	return;
}

#ifdef CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
static int psmw_dm_cpu_hotplut_vsync_notifier(struct notifier_block *this,
				unsigned long vsync, void *_cmd)
{
	PSMW_DBG("Got vsync %s\n", vsync ? "ON" : "OFF");
	if (!vsync) {
		atomic_set(&psmw_dm_cpu_info.is_vsync_requested, 0);
	}
	else {
		atomic_set(&psmw_dm_cpu_info.is_vsync_requested, 1);
		if (atomic_read(&psmw_dm_cpu_info.sleep))
			wake_up_process(dm_hotplug_task);
	}
	return NOTIFY_DONE;
}

static void dm_cpu_hotplug_psmw_init(void)
{
	atomic_set(&psmw_dm_cpu_info.is_vsync_requested, 1);
	atomic_set(&psmw_dm_cpu_info.sleep, 0);
	psmw_dm_cpu_info.nb.notifier_call = psmw_dm_cpu_hotplut_vsync_notifier;
	register_psmw_notifier(&psmw_dm_cpu_info.nb);
}
#endif // CONFIG_EXYNOS_PSMW_CPU_HOTPLUG

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

		if (exe_mode != prev_mode)
			if (dynamic_hotplug(exe_mode) < 0)
				exe_mode = prev_mode;

		prev_mode = exe_mode;
#ifdef CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
		if (atomic_read(&psmw_dm_cpu_info.is_vsync_requested) ||
							num_online_cpus() != 1) {
			atomic_set(&psmw_dm_cpu_info.sleep, 0);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout_interruptible(msecs_to_jiffies(POLLING_MSEC));
			set_current_state(TASK_RUNNING);
		} else {
			atomic_set(&psmw_dm_cpu_info.sleep, 1);
			PSMW_INFO("thread_hotplug_func: sleep\n");
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			set_current_state(TASK_RUNNING);
			PSMW_INFO("thread_hotplug_func: wakeup\n");
		}
#else
		msleep(POLLING_MSEC);
#endif // CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
	}

	return 0;
}

void dm_cpu_hotplug_exit(void)
{
	thread_run_flag = 0;
}

static int __init dm_cpu_hotplug_init(void)
{
#ifdef CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
	dm_hotplug_task = kthread_run(&on_run, NULL, "thread_hotplug_func");
	if (IS_ERR(dm_hotplug_task)) {
		pr_err("Failed in creation of thread.\n");
		goto err_hotplug_init;
	}
	dm_cpu_hotplug_psmw_init();
#else // CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
	struct task_struct *k;

	k = kthread_run(&on_run, NULL, "thread_hotplug_func");
	if (IS_ERR(k))
		pr_err("Failed in creation of thread.\n");
#endif // CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
	exynos_dm_hotplug_disable = false;

	register_pm_notifier(&exynos_dm_hotplug_nb);

#ifdef CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
	wake_up_process(dm_hotplug_task);
	return 0;

err_hotplug_init:
	kthread_stop(dm_hotplug_task);
	return -EBUSY;
#else
	return 0;
#endif //CONFIG_EXYNOS_PSMW_CPU_HOTPLUG
}

late_initcall(dm_cpu_hotplug_init);
