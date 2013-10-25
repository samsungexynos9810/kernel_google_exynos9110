/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-dvfs.h"

extern struct pm_qos_request exynos_isp_qos_dev;
extern struct pm_qos_request exynos_isp_qos_mem;

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_HIGH_SPEED_FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DIS_ENABLE);

#if defined(CONFIG_SOC_EXYNOS5410) || defined(CONFIG_SOC_EXYNOS5420) || defined(CONFIG_SOC_EXYNOS5430)
/*
 * Static Scenario Set
 * You should describe static scenario by priorities of scenario.
 * And you should name array 'static_scenarios'
 */
static struct fimc_is_dvfs_scenario static_scenarios[] = {
	[0] = {
		.scenario_id		= FIMC_IS_SN_DUAL_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_CAPTURE),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE),
	},
	[1] = {
		.scenario_id		= FIMC_IS_SN_DUAL_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING),
	},
	[2] = {
		.scenario_id		= FIMC_IS_SN_DUAL_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW),
	},
	[3] = {
		.scenario_id		= FIMC_IS_SN_HIGH_SPEED_FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_HIGH_SPEED_FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_HIGH_SPEED_FPS),
	},
	[4] = {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING),
	},
	[5] = {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW),
	},
	[6] = {
		.scenario_id		= FIMC_IS_SN_FRONT_VT1,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_VT1),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1),
	},
	[7] = {
		.scenario_id		= FIMC_IS_SN_FRONT_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW),
	},
};

/*
 * Dynamic Scenario Set
 * You should describe static scenario by priorities of scenario.
 * And you should name array 'dynamic_scenarios'
 */
static struct fimc_is_dvfs_scenario dynamic_scenarios[] = {
	[0] = {
		.scenario_id		= FIMC_IS_SN_REAR_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE),
	},
	[1] = {
		.scenario_id		= FIMC_IS_SN_DIS_ENABLE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DIS_ENABLE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DIS_ENABLE),
	},
};
#endif

static inline int fimc_is_get_open_sensor_cnt(struct fimc_is_core *core) {
	int i, sensor_cnt = 0;

	for (i = 0; i < FIMC_IS_MAX_NODES; i++)
		if (test_bit(FIMC_IS_SENSOR_OPEN, &(core->sensor[i].state)))
			sensor_cnt++;

	return sensor_cnt;
}

/* dual capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE)
{
	struct fimc_is_core *core;
	int sensor_cnt = 0;
	core = (struct fimc_is_core *)device->interface->core;
	sensor_cnt = fimc_is_get_open_sensor_cnt(core);

	if ((device->chain0_width > 2560) && (sensor_cnt >= 2))
		return 1;
	else
		return 0;
}

/* dual camcording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING)
{
	struct fimc_is_core *core;
	int sensor_cnt = 0;
	core = (struct fimc_is_core *)device->interface->core;
	sensor_cnt = fimc_is_get_open_sensor_cnt(core);

	if ((device->chain0_width <= 2560) && (sensor_cnt >= 2) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* dual preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW)
{
	struct fimc_is_core *core;
	int sensor_cnt = 0;
	core = (struct fimc_is_core *)device->interface->core;
	sensor_cnt = fimc_is_get_open_sensor_cnt(core);

	if ((device->chain0_width <= 2560) && (sensor_cnt >= 2) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* high speed fps */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_HIGH_SPEED_FPS)
{
	if ((device->module == SENSOR_NAME_IMX135) &&
			(device->sensor->framerate > 30))
		return 1;
	else
		return 0;
}

/* rear camcording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING)
{
	if ((device->module == SENSOR_NAME_IMX135) &&
			(device->sensor->framerate <= 30) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* rear preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW)
{
	if ((device->module == SENSOR_NAME_IMX135) &&
			(device->sensor->framerate <= 30) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* front vt1 */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1)
{
	if ((device->module == SENSOR_NAME_S5K6B2) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_FRONT_VT1))
		return 1;
	else
		return 0;
}

/* front preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW)
{
	if ((device->module == SENSOR_NAME_S5K6B2) &&
			!(((device->setfile & FIMC_IS_SETFILE_MASK) \
					== ISS_SUB_SCENARIO_FRONT_VT1) ||
				((device->setfile & FIMC_IS_SETFILE_MASK) \
				 == ISS_SUB_SCENARIO_FRONT_VT2)))
		return 1;
	else
		return 0;
}

/* rear capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE)
{
	if ((device->module == SENSOR_NAME_IMX135) &&
			(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)))
		return 1;
	else
		return 0;
}

/* dis */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DIS_ENABLE)
{
	if (test_bit(FIMC_IS_ISDEV_DSTART, &device->dis.state))
		return 1;
	else
		return 0;
}

int fimc_is_dvfs_init(struct fimc_is_core *core)
{
	int i;
	pr_info("%s\n",	__func__);

	if (!core) {
		err("core is NULL\n");
		return -EINVAL;
	}

	core->dvfs_ctrl.cur_int_qos = 0;
	core->dvfs_ctrl.cur_mif_qos = 0;
	core->dvfs_ctrl.cur_i2c_qos = 0;

	if (!(core->dvfs_ctrl.static_ctrl))
		core->dvfs_ctrl.static_ctrl =
			kzalloc(sizeof(struct fimc_is_dvfs_scenario_ctrl), GFP_KERNEL);
	if (!(core->dvfs_ctrl.dynamic_ctrl))
		core->dvfs_ctrl.dynamic_ctrl =
			kzalloc(sizeof(struct fimc_is_dvfs_scenario_ctrl), GFP_KERNEL);

	if (!core->dvfs_ctrl.static_ctrl || !core->dvfs_ctrl.dynamic_ctrl) {
		err("dvfs_ctrl alloc is failed!!\n");
		return -ENOMEM;
	}

	/* set priority by order */
	for (i = 0; i < ARRAY_SIZE(static_scenarios); i++)
		static_scenarios[i].priority = i;
	for (i = 0; i < ARRAY_SIZE(dynamic_scenarios); i++)
		dynamic_scenarios[i].priority = i;

	core->dvfs_ctrl.static_ctrl->cur_scenario_id	= -1;
	core->dvfs_ctrl.static_ctrl->cur_scenario_idx	= -1;
	core->dvfs_ctrl.static_ctrl->scenarios		= static_scenarios;
	core->dvfs_ctrl.static_ctrl->scenario_cnt	= ARRAY_SIZE(static_scenarios);

	core->dvfs_ctrl.dynamic_ctrl->cur_scenario_id	= -1;
	core->dvfs_ctrl.dynamic_ctrl->cur_scenario_idx	= -1;
	core->dvfs_ctrl.dynamic_ctrl->cur_frame_tick	= -1;
	core->dvfs_ctrl.dynamic_ctrl->scenarios		= dynamic_scenarios;
	core->dvfs_ctrl.dynamic_ctrl->scenario_cnt	= ARRAY_SIZE(dynamic_scenarios);

	return 0;
}

int fimc_is_dvfs_sel_scenario(u32 type, struct fimc_is_device_ischain *device)
{
	struct fimc_is_core *core;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl, *dynamic_ctrl;
	struct fimc_is_dvfs_scenario *scenarios;
	struct fimc_is_dvfs_scenario *cur_scenario;
	int i, scenario_id, scenario_cnt;

	if (device == NULL) {
		err("device is NULL\n");
		return -EINVAL;
	}

	core = (struct fimc_is_core *)device->interface->core;
	dvfs_ctrl = &(core->dvfs_ctrl);
	static_ctrl = dvfs_ctrl->static_ctrl;
	dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;

	if (type == FIMC_IS_DYNAMIC_SN) {
		/* dynamic scenario */
		if (!dynamic_ctrl) {
			err("dynamic_dvfs_ctrl is NULL\n");
			return -EINVAL;
		}

		scenarios = dynamic_ctrl->scenarios;
		scenario_cnt = dynamic_ctrl->scenario_cnt;

		if (dynamic_ctrl->cur_frame_tick >= 0) {
			(dynamic_ctrl->cur_frame_tick)--;
			/*
			 * when cur_frame_tick is lower than 0, clear current scenario.
			 * This means that current frame tick to keep dynamic scenario
			 * was expired.
			 */
			if (dynamic_ctrl->cur_frame_tick < 0) {
				dynamic_ctrl->cur_scenario_id = -1;
				dynamic_ctrl->cur_scenario_idx = -1;
			}
		}
	} else {
		/* static scenario */
		if (!static_ctrl) {
			err("static_dvfs_ctrl is NULL\n");
			return -EINVAL;
		}

		scenarios = static_ctrl->scenarios;
		scenario_cnt = static_ctrl->scenario_cnt;
	}

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		if ((scenarios[i].check_func(device)) > 0) {
			scenario_id = scenarios[i].scenario_id;

			if (type == FIMC_IS_DYNAMIC_SN) {
				cur_scenario = &scenarios[dynamic_ctrl->cur_scenario_idx];

				/*
				 * if condition 1 or 2 is true
				 * condition 1 : There's no dynamic scenario applied.
				 * condition 2 : Finded scenario's prority was higher than current
				 */
				if ((dynamic_ctrl->cur_scenario_id <= 0) ||
						(scenarios[i].priority < (cur_scenario->priority))) {
					dynamic_ctrl->cur_scenario_id = scenarios[i].scenario_id;
					dynamic_ctrl->cur_scenario_idx = i;
					dynamic_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
				} else {
					/* if finded scenario is same */
					if (scenarios[i].priority == (cur_scenario->priority))
						dynamic_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
					return -EAGAIN;
				}
			} else {
				static_ctrl->cur_scenario_id = scenario_id;
				static_ctrl->cur_scenario_idx = i;
				static_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
			}
			pr_info("%s: [%d] type:%d selected scenario[%d]\n",
					__func__, device->instance, type, scenario_id);

			return scenario_id;
		}
	}

	if (type == FIMC_IS_DYNAMIC_SN)
		return -EAGAIN;

	static_ctrl->cur_scenario_id = FIMC_IS_SN_DEFAULT;
	static_ctrl->cur_scenario_idx = -1;
	static_ctrl->cur_frame_tick = -1;

	return FIMC_IS_SN_DEFAULT;
}

int fimc_is_get_qos(struct fimc_is_core *core, u32 type, u32 scenario_id)
{
	struct exynos_platform_fimc_is	*pdata;
	int qos = 0;

	pdata = core->pdata;

	if (!pdata->get_int_qos || !pdata->get_mif_qos) {
		err("qos func in platform data is NULL\n");
		return -EINVAL;
	}

	switch (type) {
		case FIMC_IS_DVFS_INT:
			qos = pdata->get_int_qos(scenario_id);
			break;
		case FIMC_IS_DVFS_MIF:
			qos = pdata->get_mif_qos(scenario_id);
			break;
		case FIMC_IS_DVFS_I2C:
			if (pdata->get_i2c_qos)
				qos = pdata->get_i2c_qos(scenario_id);
			break;
	}

	return qos;
}

int fimc_is_set_dvfs(struct fimc_is_device_ischain *device, u32 scenario_id)
{
	int ret = 0;
	int int_qos, mif_qos, i2c_qos = 0;
	int refcount;
	struct fimc_is_core *core;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;

	if (device == NULL) {
		err("device is NULL\n");
		return -EINVAL;
	}

	core = (struct fimc_is_core *)device->interface->core;
	dvfs_ctrl = &(core->dvfs_ctrl);

	refcount = atomic_read(&core->video_isp.refcount);
	if (refcount < 0) {
		err("invalid ischain refcount");
		goto exit;
	}

	int_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_INT, scenario_id);
	mif_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_MIF, scenario_id);
	i2c_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_I2C, scenario_id);

	if (int_qos < 0 || mif_qos < 0 || i2c_qos < 0) {
		err("getting qos value is failed!!\n");
		return -EINVAL;
	}

	/* check current qos */
	if (dvfs_ctrl->cur_int_qos != int_qos) {
		if (!i2c_qos) {
			ret = fimc_is_itf_i2c_lock(device, i2c_qos, true);
			if (ret) {
				err("fimc_is_itf_i2_clock fail\n");
				goto exit;
			}
		}

		pm_qos_update_request(&exynos_isp_qos_dev, int_qos);
		dvfs_ctrl->cur_int_qos = int_qos;
		//ore->clock.dvfs_level = int_level;

		if (!i2c_qos) {
			/* i2c unlock */
			ret = fimc_is_itf_i2c_lock(device, i2c_qos, false);
			if (ret) {
				err("fimc_is_itf_i2c_unlock fail\n");
				goto exit;
			}
		}
		pr_info("[RSC:%d] %s: DVFS INT level(%d) i2c(%d) \n", device->instance,
				__func__, int_qos, i2c_qos);
	}

	if (dvfs_ctrl->cur_mif_qos != mif_qos) {
		pm_qos_update_request(&exynos_isp_qos_mem, mif_qos);
		dvfs_ctrl->cur_mif_qos = mif_qos;

		pr_info("[RSC:%d] %s: DVFS MIF level(%d) \n", device->instance,
				__func__, mif_qos);
	}

	dbg("[RSC:%d] %s: DVFS scenario_id(%d) level(%d), MIF level (%d), I2C clock(%d)\n",
			device->instance, __func__, scenario_id, int_qos, mif_qos, i2c_qos);
exit:
	return ret;
}
