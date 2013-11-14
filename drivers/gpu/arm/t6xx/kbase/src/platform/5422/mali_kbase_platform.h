/* drivers/gpu/t6xx/kbase/src/platform/5422/mali_kbase_platform.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 platform-dependent codes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_platform.h
 * Platform-dependent init
 */

#ifndef _GPU_PLATFORM_H_
#define _GPU_PLATFORM_H_

#define GPU_LOG(level, msg, args...) \
do { \
	if (level >= gpu_get_debug_level()) { \
		printk(KERN_INFO msg, ## args); \
	} \
} while (0)

typedef enum {
	DVFS_DEBUG = 0,
	DVFS_INFO,
	DVFS_WARNING,
	DVFS_ERROR,
	DVFS_DEBUG_END,
} gpu_dvfs_debug_level;

typedef enum gpu_lock_type {
	TMU_LOCK = 0,
	SYSFS_LOCK,
	NUMBER_LOCK
} gpu_lock_type;

typedef struct _gpu_dvfs_info {
	unsigned int voltage;
	unsigned int clock;
	int min_threshold;
	int max_threshold;
	int stay_count;
	unsigned long long time;
	int mem_freq;
	int int_freq;
	int cpu_freq;
} gpu_dvfs_info;

struct exynos_context {
	/** Indicator if system clock to mail-t604 is active */
	int cmu_pmu_status;

	struct clk *fout_vpll;
	struct clk *mout_vpll_ctrl;
	struct clk *mout_dpll_ctrl;
	struct clk *mout_aclk_g3d;
	struct clk *dout_aclk_g3d;
	struct clk *mout_aclk_g3d_sw;
	struct clk *mout_aclk_g3d_user;
	struct clk *clk_g3d_ip;

	int clk_g3d_status;
	struct regulator *g3d_regulator;

	gpu_dvfs_info *table;
	int table_size;
	int step;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *exynos_pm_domain;
#endif /* CONFIG_PM_RUNTIME */
	struct mutex gpu_set_clock_lock;
	struct mutex gpu_enable_clock_lock;
	spinlock_t gpu_dvfs_spinlock;
#ifdef CONFIG_MALI_T6XX_DVFS
	int utilization;
	int max_lock;
	int min_lock;
	int user_max_lock[NUMBER_LOCK];
	int user_min_lock[NUMBER_LOCK];
	int target_lock_type;
	int down_requirement;
	bool wakeup_lock;
	int governor_num;
	int governor_type;
	char governor_list[100];
	bool dvfs_status;
#endif
	int cur_clock;
	int cur_voltage;
	int voltage_margin;
	bool tmu_status;
	int debug_level;
	int polling_speed;
	struct workqueue_struct *dvfs_wq;
};

void gpu_set_debug_level(int level);
int gpu_get_debug_level(void);

#if SOC_NAME == 5422
#define EXYNOS5422_G3D_CONFIGURATION        EXYNOS5410_G3D_CONFIGURATION
#define EXYNOS5422_G3D_STATUS               EXYNOS5410_G3D_STATUS
#define EXYNOS5422_G3D_OPTION               EXYNOS5410_G3D_OPTION
#endif

#endif /* _GPU_PLATFORM_H_ */
