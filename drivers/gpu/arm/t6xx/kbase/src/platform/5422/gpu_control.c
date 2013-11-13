/* drivers/gpu/t6xx/kbase/src/platform/5422/gpu_control.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_control.c
 * DVFS
 */

#include <kbase/src/common/mali_kbase.h>

#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/regulator/driver.h>

#include <mach/regs-pmu.h>
#include <mach/asv-exynos.h>
#include <mach/regs-clock-exynos5422.h>
#include <mach/pm_domains.h>

#include <../drivers/clk/samsung/clk.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_control.h"

#define COLD_MINIMUM_VOL		950000
#define GPU_DEFAULT_VOLTAGE		1037500

#define MALI_T6XX_DEFAULT_CLOCK (MALI_DVFS_START_FREQ*MHZ)

struct kbase_device *pkbdev;

#ifdef CONFIG_PM_RUNTIME
struct exynos_pm_domain *gpu_get_pm_domain(kbase_device *kbdev)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	struct exynos_pm_domain *pd_temp, *pd = NULL;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd")
	{
		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd_temp = platform_get_drvdata(pdev);
		if (!strcmp("pd-g3d", pd_temp->genpd.name)) {
			pd = pd_temp;
			break;
		}
	}

	return pd;
}
#endif

int get_cpu_clock_speed(u32 *cpu_clock)
{
	struct clk *cpu_clk;
	u32 freq = 0;
	cpu_clk = clk_get(NULL, "armclk");
	if (IS_ERR(cpu_clk))
		return -1;
	freq = clk_get_rate(cpu_clk);
	*cpu_clock = (freq/MHZ);
	return 0;
}

static int gpu_is_power_on(void)
{
	return ((__raw_readl(EXYNOS5422_G3D_STATUS) & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN) ? 1 : 0;
}

static int gpu_power_on(void)
{
	int timeout;

	/* Turn on G3D  */
	__raw_writel(EXYNOS_INT_LOCAL_PWR_EN, EXYNOS5422_G3D_CONFIGURATION);

	/* Wait for G3D power stability */
	timeout = 1000;

	while ((__raw_readl(EXYNOS5422_G3D_STATUS) & EXYNOS_INT_LOCAL_PWR_EN) != EXYNOS_INT_LOCAL_PWR_EN) {
		if (timeout == 0) {
			/* need to call panic  */
			panic("failed to turn on g3d via g3d_configuration\n");
			return -ETIMEDOUT;
		}
		timeout--;
		udelay(10);
	}

	return 0;
}

static int gpu_power_off(void)
{
	int timeout;

	/* Turn off G3D  */
	__raw_writel(0x0, EXYNOS5422_G3D_CONFIGURATION);

	/* Wait for G3D power stability */
	timeout = 1000;

	while (__raw_readl(EXYNOS5422_G3D_STATUS) & EXYNOS_INT_LOCAL_PWR_EN) {
		if (timeout == 0) {
			/* need to call panic */
			panic("failed to turn off g3d via g3d_configuration\n");
			return -ETIMEDOUT;
		}
		timeout--;
		udelay(10);
	}

	return 0;
}

static int gpu_power_init(void)
{
	GPU_LOG(DVFS_INFO, "g3d power initialized\n");

	return 0;
}

static int gpu_update_clock(struct exynos_context *platform)
{
	if (!platform->clk_g3d_ip) {
		GPU_LOG(DVFS_ERROR, "clk_g3d_ip is not initialized\n");
		return -1;
	}

	platform->cur_clock = clk_get_rate(platform->clk_g3d_ip)/MHZ;
	return 0;
}

static int gpu_clock_on(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_WARNING, "can't set clock on in g3d power off status\n");
		return -1;
	}

	if (platform->clk_g3d_status == 1)
		return 0;

	if (platform->clk_g3d_ip)
		(void) clk_prepare_enable(platform->clk_g3d_ip);

	platform->clk_g3d_status = 1;

	return 0;
}

static int gpu_clock_off(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	if (platform->clk_g3d_status == 0)
		return 0;

	if (platform->clk_g3d_ip)
		(void)clk_disable_unprepare(platform->clk_g3d_ip);

	platform->clk_g3d_status = 0;

	return 0;
}

static int gpu_set_clock(struct exynos_context *platform, int freq)
{
	long g3d_rate_prev = -1;
	unsigned long g3d_rate = freq * MHZ;
	unsigned long tmp = 0;
	int ret;

	if (platform->clk_g3d_ip == 0)
		return -1;

#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_WARNING, "gpu_set_clk_vol in the G3D power-off state!\n");
		goto err;
	}

	g3d_rate_prev = clk_get_rate(platform->clk_g3d_ip)/MHZ;

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (g3d_rate != g3d_rate_prev) {
		/*for stable clock input.*/
		ret = clk_set_rate(platform->dout_aclk_g3d, 100000);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_rate [dout_aclk_g3d]\n");
			goto err;
		}

		/*change here for future stable clock changing*/
		ret = clk_set_parent(platform->mout_aclk_g3d, platform->mout_dpll_ctrl);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_parent [mout_aclk_g3d]\n");
			goto err;
		}

		/*change g3d pll*/
		ret = clk_set_rate(platform->fout_vpll, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_rate [fout_vpll]\n");
			goto err;
		}

		/*restore parent*/
		clk_set_parent(platform->mout_aclk_g3d, platform->mout_vpll_ctrl);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_parent [mout_aclk_g3d]\n");
			goto err;
		}

		g3d_rate_prev = g3d_rate;
	}

	ret = clk_set_rate(platform->dout_aclk_g3d, g3d_rate);
	if (ret < 0) {
		GPU_LOG(DVFS_ERROR, "failed to clk_set_rate [dout_aclk_g3d]\n");
		goto err;
	}

	/* Waiting for clock is stable */
	do {
		tmp = __raw_readl(EXYNOS5_CLK_DIV_STAT_TOP2);
	} while (tmp & 0x10000);

	gpu_update_clock(platform);
	GPU_LOG(DVFS_DEBUG, "[G3D] clock set: %ld\n", g3d_rate / MHZ);
	GPU_LOG(DVFS_DEBUG, "[G3D] clock get: %d\n", platform->cur_clock);
err:
#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */
	return ret;
}

static int gpu_get_clock(kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	/*
	 * EXYNOS5422 3D clock description
	 * normal usage: mux(vpll) -> divider -> mux_sw -> mux_user -> aclk_g3d
	 * on clock changing: mux(dpll) -> divider(3) -> mux_sw -> mux_user -> aclk_g3d
	 */

	platform->fout_vpll = clk_get(NULL, "fout_vpll");
	if (IS_ERR(platform->fout_vpll)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [fout_vpll]\n");
		return -1;
	}

	platform->mout_vpll_ctrl = clk_get(kbdev->osdev.dev, "mout_vpll_ctrl"); /* same as sclk_vpll */
	if (IS_ERR(platform->mout_vpll_ctrl)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_vpll_ctrl]\n");
		return -1;
	}

	platform->mout_dpll_ctrl = clk_get(kbdev->osdev.dev, "mout_dpll_ctrl"); /* same as sclk_dpll */
	if (IS_ERR(platform->mout_dpll_ctrl)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_dpll_ctrl]\n");
		return -1;
	}

	platform->mout_aclk_g3d = clk_get(kbdev->osdev.dev, "mout_aclk_g3d"); /* set parents v or d pll */
	if (IS_ERR(platform->mout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_aclk_g3d]\n");
		return -1;
	}

	platform->dout_aclk_g3d = clk_get(kbdev->osdev.dev, "dout_aclk_g3d"); /* divider usage */
	if (IS_ERR(platform->dout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [dout_aclk_g3d]\n");
		return -1;
	}

	platform->mout_aclk_g3d_sw = clk_get(kbdev->osdev.dev, "mout_aclk_g3d_sw");
	if (IS_ERR(platform->mout_aclk_g3d_sw)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_aclk_g3d_sw]\n");
		return -1;
	}

	platform->mout_aclk_g3d_user = clk_get(kbdev->osdev.dev, "mout_aclk_g3d_user");
	if (IS_ERR(platform->mout_aclk_g3d_user)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_aclk_g3d_user]\n");
		return -1;
	}

	platform->clk_g3d_ip = clk_get(kbdev->osdev.dev, "clk_g3d_ip");
	clk_prepare_enable(platform->clk_g3d_ip);
	if (IS_ERR(platform->clk_g3d_ip)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [clk_g3d_ip]\n");
		return -1;
	}

	return 0;
}

static int gpu_clock_init(kbase_device *kbdev)
{
	int ret;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	ret = gpu_get_clock(kbdev);
	if (ret < 0)
		return -1;

	GPU_LOG(DVFS_INFO, "g3d clock initialized\n");

	return 0;
}

static int gpu_update_voltage(struct exynos_context *platform)
{
#ifdef CONFIG_REGULATOR
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -1;
	}

	platform->cur_voltage = regulator_get_voltage(platform->g3d_regulator);
#endif /* CONFIG_REGULATOR */
	return 0;
}

static int gpu_set_voltage(struct exynos_context *platform, int vol)
{
	static int _vol = -1;

	if (_vol == vol)
		return 0;

#ifdef CONFIG_REGULATOR
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -1;
	}

	if (regulator_set_voltage(platform->g3d_regulator, vol, vol) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to set voltage, voltage: %d\n", vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	_vol = vol;

	gpu_update_voltage(platform);
	GPU_LOG(DVFS_DEBUG, "[G3D] voltage set:%d\n", vol);
	GPU_LOG(DVFS_DEBUG, "[G3D] voltage get:%d\n", platform->cur_voltage);

	return 0;
}

#ifdef CONFIG_REGULATOR
static int gpu_regulator_enable(struct exynos_context *platform)
{
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -1;
	}

	if (regulator_enable(platform->g3d_regulator) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to enable g3d regulator\n");
		return -1;
	}
	return 0;
}

static int gpu_regulator_disable(struct exynos_context *platform)
{
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -1;
	}

	if (regulator_disable(platform->g3d_regulator) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to disable g3d regulator\n");
		return -1;
	}
	return 0;
}

static int gpu_regulator_init(struct exynos_context *platform)
{
	int gpu_voltage = 0;

	platform->g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR(platform->g3d_regulator)) {
		GPU_LOG(DVFS_ERROR, "failed to get mali t6xx regulator, 0x%p\n", platform->g3d_regulator);
		platform->g3d_regulator = NULL;
		return -1;
	}

	if (gpu_regulator_enable(platform) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to enable mali t6xx regulator\n");
		platform->g3d_regulator = NULL;
		return -1;
	}

	gpu_voltage = get_match_volt(ID_G3D, MALI_DVFS_BL_CONFIG_FREQ*1000);
	if (gpu_voltage == 0)
		gpu_voltage = GPU_DEFAULT_VOLTAGE;

	if (gpu_set_voltage(platform, gpu_voltage) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to set mali t6xx operating voltage [%d]\n", gpu_voltage);
		return -1;
	}

	GPU_LOG(DVFS_INFO, "g3d regulator initialized\n");

	return 0;
}
#endif /* CONFIG_REGULATOR */

#ifdef CONFIG_MALI_T6XX_DVFS
#ifdef CONFIG_BUS_DEVFREQ
static struct pm_qos_request exynos5_g3d_mif_qos;
static struct pm_qos_request exynos5_g3d_int_qos;
static struct pm_qos_request exynos5_g3d_cpu_qos;
#endif /* CONFIG_BUS_DEVFREQ */

static int gpu_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state)
{
#ifdef CONFIG_BUS_DEVFREQ
	switch (state) {
	case GPU_CONTROL_PM_QOS_INIT:
		pm_qos_add_request(&exynos5_g3d_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
		pm_qos_add_request(&exynos5_g3d_int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
		pm_qos_add_request(&exynos5_g3d_cpu_qos, PM_QOS_CPU_FREQ_MIN, 0);
		break;
	case GPU_CONTROL_PM_QOS_DEINIT:
		pm_qos_remove_request(&exynos5_g3d_mif_qos);
		pm_qos_remove_request(&exynos5_g3d_int_qos);
		pm_qos_remove_request(&exynos5_g3d_cpu_qos);
		break;
	case GPU_CONTROL_PM_QOS_SET:
		pm_qos_update_request(&exynos5_g3d_mif_qos, platform->table[platform->step].mem_freq);
		pm_qos_update_request(&exynos5_g3d_int_qos, platform->table[platform->step].int_freq);
		pm_qos_update_request(&exynos5_g3d_cpu_qos, platform->table[platform->step].cpu_freq);
		break;
	case GPU_CONTROL_PM_QOS_RESET:
		pm_qos_update_request(&exynos5_g3d_mif_qos, 0);
		pm_qos_update_request(&exynos5_g3d_int_qos, 0);
		pm_qos_update_request(&exynos5_g3d_cpu_qos, 0);
	default:
		break;
	}
#endif /* CONFIG_BUS_DEVFREQ */
	return 0;
}
#endif /* CONFIG_MALI_T6XX_DVFS */

static int gpu_set_clk_vol(struct kbase_device *kbdev, int clock, int voltage)
{
	static int prev_clock = -1;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pd = NULL;
#endif
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (clock == prev_clock)
		return 0;

	if ((clock > platform->table[platform->table_size-1].clock) || (clock < platform->table[0].clock)) {
		GPU_LOG(DVFS_ERROR, "Mismatch clock error (%d)\n", clock);
		return 0;
	}

	if (clock > prev_clock) {
		gpu_set_voltage(platform, voltage + platform->voltage_margin);
		gpu_set_clock(platform, clock);
	} else {
		gpu_set_clock(platform, clock);
		gpu_set_voltage(platform, voltage + platform->voltage_margin);
	}
	GPU_LOG(DVFS_INFO, "[G3D] clock changed [%d -> %d]\n", prev_clock, clock);

#ifdef CONFIG_MALI_T6XX_DVFS
	gpu_dvfs_handler_control(kbdev, GPU_HANDLER_UPDATE_TIME_IN_STATE, prev_clock);
#endif /* CONFIG_MALI_T6XX_DVFS */

	prev_clock = clock;

#ifdef CONFIG_PM_RUNTIME
	if (pd)
		mutex_unlock(&pd->access_lock);
#endif

	return 0;
}

int gpu_control_state_set(struct kbase_device *kbdev, gpu_control_state state, int param)
{
	int ret = 0, voltage;
#ifdef CONFIG_MALI_T6XX_DVFS
	unsigned long flags;
#endif /* CONFIG_MALI_T6XX_DVFS */
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	switch (state) {
	case GPU_CONTROL_CLOCK_ON:
		mutex_lock(&platform->gpu_set_clock_lock);
		ret = gpu_set_clock(platform, platform->cur_clock);
#ifdef CONFIG_MALI_T6XX_DVFS
		if (ret == 0) {
			spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
			ret = gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_GET_LEVEL, platform->cur_clock);
			spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
			if (ret >= 0)
				platform->step = ret;
			else
				GPU_LOG(DVFS_ERROR, "Invalid dvfs level returned [%d]\n", GPU_CONTROL_CLOCK_ON);
		}
#endif /* CONFIG_MALI_T6XX_DVFS */
		ret = gpu_clock_on(platform);
		mutex_unlock(&platform->gpu_set_clock_lock);
#ifdef CONFIG_MALI_T6XX_DVFS
		gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_SET);
#endif /* CONFIG_MALI_T6XX_DVFS */
		break;
	case GPU_CONTROL_CLOCK_OFF:
		mutex_lock(&platform->gpu_set_clock_lock);
		ret = gpu_clock_off(platform);
		mutex_unlock(&platform->gpu_set_clock_lock);
#ifdef CONFIG_MALI_T6XX_DVFS
		gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_RESET);
#endif /* CONFIG_MALI_T6XX_DVFS */
		break;
	case GPU_CONTROL_IS_POWER_ON:
		ret = gpu_is_power_on();
		break;
	case GPU_CONTROL_SET_MARGIN:
		mutex_lock(&platform->gpu_set_clock_lock);
		voltage = platform->table[platform->step].voltage + platform->voltage_margin;
		if (voltage <= COLD_MINIMUM_VOL)
			voltage = COLD_MINIMUM_VOL;
		gpu_set_voltage(platform, voltage);
		mutex_unlock(&platform->gpu_set_clock_lock);
		GPU_LOG(DVFS_DEBUG, "we set the voltage: %d\n", voltage);
		break;
	case GPU_CONTROL_CHANGE_CLK_VOL:
		mutex_lock(&platform->gpu_set_clock_lock);
		ret = gpu_set_clk_vol(kbdev, param, gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_GET_VOLTAGE, param));
#ifdef CONFIG_MALI_T6XX_DVFS
		if (ret == 0) {
			spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
			ret = gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_GET_LEVEL, platform->cur_clock);
			spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
			if (ret >= 0)
				platform->step = ret;
			else
				GPU_LOG(DVFS_ERROR, "Invalid dvfs level returned [%d]\n", GPU_CONTROL_CHANGE_CLK_VOL);
		}
#endif /* CONFIG_MALI_T6XX_DVFS */
		mutex_unlock(&platform->gpu_set_clock_lock);
#ifdef CONFIG_MALI_T6XX_DVFS
		gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_SET);
#endif /* CONFIG_MALI_T6XX_DVFS */
		break;
	case GPU_CONTROL_CMU_PMU_ON:
		mutex_lock(&platform->gpu_set_clock_lock);
		if (platform->cmu_pmu_status == 1) {
			mutex_unlock(&platform->gpu_set_clock_lock);
			ret = -1;
		}
		ret = gpu_set_clock(platform, platform->cur_clock);
#ifdef CONFIG_MALI_T6XX_DVFS
		if (ret == 0) {
			spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
			ret = gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_GET_LEVEL, platform->cur_clock);
			spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
			if (ret >= 0)
				platform->step = ret;
			else
				GPU_LOG(DVFS_ERROR, "Invalid dvfs level returned [%d]\n", GPU_CONTROL_CMU_PMU_ON);
		}
#endif /* CONFIG_MALI_T6XX_DVFS */
		ret = gpu_clock_on(platform);
		gpu_power_on();
		platform->cmu_pmu_status = 1;
		mutex_unlock(&platform->gpu_set_clock_lock);
		break;
	case GPU_CONTROL_CMU_PMU_OFF:
		mutex_lock(&platform->gpu_set_clock_lock);
		if (platform->cmu_pmu_status == 0) {
			mutex_unlock(&platform->gpu_set_clock_lock);
			ret = -1;
		}
		gpu_power_off();
		gpu_clock_off(platform);
		platform->cmu_pmu_status = 0;
		mutex_unlock(&platform->gpu_set_clock_lock);
		break;
	case GPU_CONTROL_PREPARE_ON:
#ifdef CONFIG_MALI_T6XX_DVFS
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		if ((platform->dvfs_status && platform->wakeup_lock) &&
				(platform->table[platform->step].clock < MALI_DVFS_START_FREQ))
			platform->cur_clock = MALI_DVFS_START_FREQ;

		if (platform->min_lock > 0)
			platform->cur_clock = MAX(platform->min_lock, platform->cur_clock);
		else if (platform->max_lock > 0)
			platform->cur_clock = MIN(platform->max_lock, platform->cur_clock);

		platform->down_requirement = platform->table[platform->step].stay_count;
		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

		if (!kbdev->pm.metrics.timer_active) {
			spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
			kbdev->pm.metrics.timer_active = true;
			spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
			hrtimer_start(&kbdev->pm.metrics.timer, HR_TIMER_DELAY_MSEC(platform->polling_speed), HRTIMER_MODE_REL);
		}
#endif /* CONFIG_MALI_T6XX_DVFS */
		break;
	case GPU_CONTROL_PREPARE_OFF:
#ifdef CONFIG_MALI_T6XX_DVFS
		if (platform->dvfs_status && kbdev->pm.metrics.timer_active) {
			spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
			kbdev->pm.metrics.timer_active = false;
			spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
			hrtimer_cancel(&kbdev->pm.metrics.timer);
		}
#endif /* CONFIG_MALI_T6XX_DVFS */
		break;
	default:
		return -1;
	}

	return ret;
}

int gpu_control_module_init(kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	mutex_init(&platform->gpu_set_clock_lock);
	mutex_init(&platform->gpu_enable_clock_lock);
#ifdef CONFIG_PM_RUNTIME
	platform->exynos_pm_domain = gpu_get_pm_domain(kbdev);
#endif /* CONFIG_PM_RUNTIME */

	pkbdev = kbdev;

	if (gpu_power_init() < 0) {
		GPU_LOG(DVFS_ERROR, "failed to initialize g3d power\n");
		goto out;
	}

	if (gpu_clock_init(kbdev) < 0) {
		GPU_LOG(DVFS_ERROR, "failed to initialize g3d clock\n");
		goto out;
	}

#ifdef CONFIG_REGULATOR
	if (gpu_regulator_init(platform) < 0) {
		GPU_LOG(DVFS_ERROR, "failed to initialize g3d regulator\n");
		goto regulator_init_fail;
	}
#endif /* CONFIG_REGULATOR */

#ifdef CONFIG_MALI_T6XX_DVFS
	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_INIT);
#endif /* CONFIG_MALI_T6XX_DVFS */

	return 0;
#ifdef CONFIG_REGULATOR
regulator_init_fail:
	gpu_regulator_disable(platform);
#endif /* CONFIG_REGULATOR */
out:
	return -EPERM;
}

void gpu_control_module_term(kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return;

#ifdef CONFIG_PM_RUNTIME
	platform->exynos_pm_domain = NULL;
#endif /* CONFIG_PM_RUNTIME */
#ifdef CONFIG_REGULATOR
	gpu_regulator_disable(platform);
#endif /* CONFIG_REGULATOR */

#ifdef CONFIG_MALI_T6XX_DVFS
	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_DEINIT);
#endif /* CONFIG_MALI_T6XX_DVFS */
}

