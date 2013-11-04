/* drivers/gpu/t6xx/kbase/src/platform/5430/gpu_dvfs_handler.h
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
 * @file gpu_dvfs_handler.h
 * DVFS
 */

#ifndef _GPU_DVFS_HANDLER_H_
#define _GPU_DVFS_HANDLER_H_

#if SOC_NAME == 5430
#define MALI_DVFS_START_FREQ 266
#define MALI_DVFS_BL_CONFIG_FREQ 266
#endif

#ifdef CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ
#define CONFIG_BUS_DEVFREQ
#endif

#define GPU_DVFS_FREQUENCY		100

#define DVFS_ASSERT(x) \
do { if (x) break; \
	printk(KERN_EMERG "### ASSERTION FAILED %s: %s: %d: %s\n", __FILE__, __func__, __LINE__, #x); dump_stack(); \
} while (0)

typedef enum {
	GPU_HANDLER_DVFS_ON = 0,
	GPU_HANDLER_DVFS_OFF,
	GPU_HANDLER_DVFS_GOVERNOR_CHANGE,
	GPU_HANDLER_DVFS_MAX_LOCK,
	GPU_HANDLER_DVFS_MIN_LOCK,
	GPU_HANDLER_DVFS_MAX_UNLOCK,
	GPU_HANDLER_DVFS_MIN_UNLOCK,
	GPU_HANDLER_INIT_TIME_IN_STATE,
	GPU_HANDLER_UPDATE_TIME_IN_STATE,
	GPU_HANDLER_DVFS_GET_LEVEL,
	GPU_HANDLER_DVFS_GET_VOLTAGE,
} gpu_dvfs_handler_command;

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation);
int gpu_dvfs_handler_init(struct kbase_device *kbdev);
int gpu_dvfs_handler_deinit(struct kbase_device *kbdev);
int gpu_dvfs_handler_control(struct kbase_device *kbdev, gpu_dvfs_handler_command command, int param);

#endif /* _GPU_DVFS_HANDLER_H_ */
