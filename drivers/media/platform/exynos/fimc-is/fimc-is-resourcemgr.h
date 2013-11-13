/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_RESOURCE_MGR_H
#define FIMC_IS_RESOURCE_MGR_H

#include "fimc-is-groupmgr.h"

struct fimc_is_clock {
	struct mutex				lock;
	unsigned long				msk_state;
	u32					msk_cnt[GROUP_ID_MAX];
	bool					state_3a0;
	int					dvfs_level;
	int					dvfs_skipcnt;
	unsigned long				dvfs_state;
};

struct fimc_is_dvfs_ctrl {
	int cur_int_qos;
	int cur_mif_qos;
	int cur_cam_qos;
	int cur_i2c_qos;

	struct fimc_is_dvfs_scenario_ctrl *static_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *dynamic_ctrl;
};

struct fimc_is_resourcemgr {
	spinlock_t				slock_clock;

	atomic_t				rsccount;
	atomic_t				rsccount_sensor;
	atomic_t				rsccount_ischain;

	struct fimc_is_clock			clock;
	struct fimc_is_dvfs_ctrl		dvfs_ctrl;

	void					*private_data;
};

int fimc_is_resource_probe(struct fimc_is_resourcemgr *resourcemgr,
	void *private_data);
int fimc_is_resource_get(struct fimc_is_resourcemgr *resourcemgr);
int fimc_is_resource_put(struct fimc_is_resourcemgr *resourcemgr);

int fimc_is_clock_set(struct fimc_is_resourcemgr *resourcemgr,
	int group_id,
	bool on);

#endif
