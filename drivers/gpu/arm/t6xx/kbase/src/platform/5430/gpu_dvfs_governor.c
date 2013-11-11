/* drivers/gpu/t6xx/kbase/src/platform/5430/gpu_dvfs_governor.c
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
 * @file gpu_dvfs_governor.c
 * DVFS
 */

#include <kbase/src/common/mali_kbase.h>

#include <linux/io.h>
#include <mach/asv-exynos.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"

#ifdef CONFIG_MALI_T6XX_DVFS
typedef void (*GET_NEXT_FREQ)(struct kbase_device *kbdev, int utilization);
GET_NEXT_FREQ gpu_dvfs_get_next_freq;

static char *governor_list[G3D_MAX_GOVERNOR_NUM] = {"Default", "Static"};
#endif /* CONFIG_MALI_T6XX_DVFS */

#define GPU_DVFS_TABLE_SIZE(X)  ARRAY_SIZE(X)

static gpu_dvfs_info gpu_dvfs_infotbl_default[] = {
/*  vol,clk,min,max,down stay, pm_qos mem, pm_qos int, pm_qos cpu */
	{1000000,   160, 0,   90, 3, 0, 160000,  83000,  250000},
	{1000000,   266, 54, 100, 3, 0, 160000,  83000,  250000},
#if 0 /* temporarily disabled */
	{1025000,  350, 60,  90, 3, 0, 400000, 222000,  250000},
	{1025000,  420, 70,  90, 2, 0, 667000, 333000,  250000},
	{1075000,  500, 78, 100, 1, 0, 800000, 400000,  250000},
	{1125000,  550, 78, 100, 1, 0, 800000, 400000,  250000},
	{1150000,  600, 78, 100, 1, 0, 800000, 400000,  250000},
#endif
};

#ifdef CONFIG_MALI_T6XX_DVFS

#define G3D_GOVERNOR_DEFAULT_CLOCK_DEFAULT		266
static int gpu_dvfs_governor_default(struct kbase_device *kbdev, int utilization)
{
	struct exynos_context *platform;

	platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if ((platform->step < platform->table_size-1) &&
			(utilization > platform->table[platform->step].max_threshold)) {
		platform->step++;
		platform->down_requirement = platform->table[platform->step].stay_count;
		DVFS_ASSERT(platform->step < platform->table_size);
	} else if ((platform->step > 0) && (utilization < platform->table[platform->step].min_threshold)) {
		DVFS_ASSERT(platform->step > 0);
		platform->down_requirement--;
		if (platform->down_requirement == 0) {
			platform->step--;
			platform->down_requirement = platform->table[platform->step].stay_count;
		}
	} else {
		platform->down_requirement = platform->table[platform->step].stay_count;
	}

	return 0;
}

#define G3D_GOVERNOR_DEFAULT_CLOCK_STATIC		266
#define G3D_GOVERNOR_STATIC_PERIOD				10
static int gpu_dvfs_governor_static(struct kbase_device *kbdev, int utilization)
{
	struct exynos_context *platform;
	static bool increase = true;
	static int count;

	platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (count == G3D_GOVERNOR_STATIC_PERIOD) {
		if (increase) {
			if (platform->step < platform->table_size-1)
				platform->step++;
			if (((platform->max_lock > 0) && (platform->table[platform->step].clock == platform->max_lock))
					|| (platform->step == platform->table_size-1))
				increase = false;
		} else {
			if (platform->step > 0)
				platform->step--;
			if (((platform->min_lock > 0) && (platform->table[platform->step].clock == platform->min_lock))
					|| (platform->step == 0))
				increase = true;
		}

		count = 0;
	} else {
		count++;
	}

	return 0;
}
#endif /* CONFIG_MALI_T6XX_DVFS */

static int gpu_dvfs_update_asv_table(struct exynos_context *platform, int governor_type)
{
	int i, voltage;

	for (i = 0; i < platform->table_size; i++) {
		voltage = get_match_volt(ID_G3D, platform->table[i].clock*1000);
		if (voltage > 0)
			platform->table[i].voltage = voltage;
		GPU_LOG(DVFS_INFO, "G3D %dKhz ASV is %duV\n", platform->table[i].clock*1000, platform->table[i].voltage);
	}
	return 0;
}

int gpu_dvfs_governor_init(struct kbase_device *kbdev, int governor_type)
{
	unsigned long flags;
#ifdef CONFIG_MALI_T6XX_DVFS
	int i, total = 0;
#endif /* CONFIG_MALI_T6XX_DVFS */
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);

#ifdef CONFIG_MALI_T6XX_DVFS
	switch (governor_type) {
	case G3D_DVFS_GOVERNOR_DEFAULT:
		gpu_dvfs_get_next_freq = (GET_NEXT_FREQ)&gpu_dvfs_governor_default;
		platform->table = gpu_dvfs_infotbl_default;
		platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
		platform->step = gpu_dvfs_get_level(platform, G3D_GOVERNOR_DEFAULT_CLOCK_DEFAULT);
		break;
	case G3D_DVFS_GOVERNOR_STATIC:
		gpu_dvfs_get_next_freq = (GET_NEXT_FREQ)&gpu_dvfs_governor_static;
		platform->table = gpu_dvfs_infotbl_default;
		platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
		platform->step = gpu_dvfs_get_level(platform, G3D_GOVERNOR_DEFAULT_CLOCK_STATIC);
		break;
	default:
		GPU_LOG(DVFS_WARNING, "[gpu_dvfs_governor_init] invalid governor type\n");
		gpu_dvfs_get_next_freq = (GET_NEXT_FREQ)&gpu_dvfs_governor_default;
		platform->table = gpu_dvfs_infotbl_default;
		platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
		platform->step = gpu_dvfs_get_level(platform, G3D_GOVERNOR_DEFAULT_CLOCK_DEFAULT);
		break;
	}

	platform->utilization = 100;
	platform->target_lock_type = -1;
	platform->max_lock = 0;
	platform->min_lock = 0;
	for (i = 0; i < NUMBER_LOCK; i++) {
		platform->user_max_lock[i] = 0;
		platform->user_min_lock[i] = 0;
	}

	platform->down_requirement = 1;
	platform->wakeup_lock = 0;

	platform->governor_type = governor_type;
	platform->governor_num = G3D_MAX_GOVERNOR_NUM;

	for (i = 0; i < G3D_MAX_GOVERNOR_NUM; i++)
		total += snprintf(platform->governor_list+total,
			sizeof(platform->governor_list), "[%d] %s\n", i, governor_list[i]);

	gpu_dvfs_init_time_in_state(platform);
#else
	platform->table = gpu_dvfs_infotbl_default;
	platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
	platform->step = gpu_dvfs_get_level(platform, MALI_DVFS_START_FREQ);
#endif /* CONFIG_MALI_T6XX_DVFS */

	platform->cur_clock = platform->table[platform->step].clock;

	/* asv info update */
	gpu_dvfs_update_asv_table(platform, governor_type);

	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return 1;
}

#ifdef CONFIG_MALI_T6XX_DVFS
int gpu_dvfs_init_time_in_state(struct exynos_context *platform)
{
#ifdef CONFIG_MALI_T6XX_DEBUG_SYS
	int i;

	for (i = 0; i < platform->table_size; i++)
		platform->table[i].time = 0;
#endif /* CONFIG_MALI_T6XX_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_update_time_in_state(struct exynos_context *platform, int freq)
{
#ifdef CONFIG_MALI_T6XX_DEBUG_SYS
	u64 current_time;
	static u64 prev_time;

	if (prev_time == 0)
		prev_time = get_jiffies_64();

	current_time = get_jiffies_64();
	platform->table[gpu_dvfs_get_level(platform, freq)].time += current_time-prev_time;
	prev_time = current_time;
#endif /* CONFIG_MALI_T6XX_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_decide_next_level(struct kbase_device *kbdev, int utilization)
{
	unsigned long flags;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	gpu_dvfs_get_next_freq(kbdev, utilization);
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return 0;
}
#endif /* CONFIG_MALI_T6XX_DVFS */

int gpu_dvfs_get_level(struct exynos_context *platform, int freq)
{
	int i;

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == freq)
			return i;
	}

	return -1;
}

int gpu_dvfs_get_voltage(struct exynos_context *platform, int freq)
{
	int i;

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == freq)
			return platform->table[i].voltage;
	}

	return -1;
}
