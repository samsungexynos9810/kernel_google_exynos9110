/* drivers/gpu/t6xx/kbase/src/platform/5422/gpu_control.h
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
 * @file gpu_control.h
 * DVFS
 */

#ifndef _GPU_CONTROL_H_
#define _GPU_CONTROL_H_

typedef enum {
	GPU_CONTROL_CLOCK_ON = 0,
	GPU_CONTROL_CLOCK_OFF,
	GPU_CONTROL_IS_POWER_ON,
	GPU_CONTROL_SET_MARGIN,
	GPU_CONTROL_CHANGE_CLK_VOL,
	GPU_CONTROL_CMU_PMU_ON,
	GPU_CONTROL_CMU_PMU_OFF,
	GPU_CONTROL_PREPARE_ON,
	GPU_CONTROL_PREPARE_OFF,
} gpu_control_state;

typedef enum {
	GPU_CONTROL_PM_QOS_INIT = 0,
	GPU_CONTROL_PM_QOS_DEINIT,
	GPU_CONTROL_PM_QOS_SET,
	GPU_CONTROL_PM_QOS_RESET,
} gpu_pmqos_state;

int gpu_control_state_set(struct kbase_device *kbdev, gpu_control_state state, int param);
int gpu_control_module_init(kbase_device *kbdev);
void gpu_control_module_term(kbase_device *kbdev);

#endif /* _GPU_CONTROL_H_ */
