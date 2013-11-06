/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is-sensor.h>

#include "../fimc-is-core.h"
#include "../fimc-is-device-sensor.h"
#include "fimc-is-device-3l2.h"

#define SENSOR_NAME "IMX135"

static struct fimc_is_settle settle_3l2[] = {
	/* 4144x3106@30fps */
	FIMC_IS_SETTLE(4144, 3106, 30, 23),
	/* 4144x2332@30fps */
	FIMC_IS_SETTLE(4144, 2332, 30, 23),
	/* 1024x584@120fps */
	FIMC_IS_SETTLE(1024, 584, 120, 17),
	/* 2072x1166@60fps */
	FIMC_IS_SETTLE(2072, 1162, 60, 9),
	/* 2072x1166@24fps */
	FIMC_IS_SETTLE(2072, 1166, 24, 5),
	/* 2072x1154@24fps */
	FIMC_IS_SETTLE(2072, 1154, 24, 5),
};

static int sensor_3l2_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);

	pr_info("[MOD:D:%d] %s(%d)\n", module->id, __func__, val);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_3l2_init
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops
};

int sensor_3l2_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[SENSOR_3L2_INSTANCE];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	module = &device->module_enum[SENSOR_NAME_S5K3L2];
	module->id = SENSOR_NAME_S5K3L2;
	module->subdev = subdev_module;
	module->device = SENSOR_3L2_INSTANCE;
	module->client = client;
	module->pixel_width = 4128 + 16;
	module->pixel_height = 3096 + 10;
	module->active_width = 4128;
	module->active_height = 3096;
	module->max_framerate = 120;
	module->position = SENSOR_POSITION_REAR;
	module->setfile_name = "setfile_3l2.bin";
	module->settle_max = ARRAY_SIZE(settle_3l2);
	module->settle_table = settle_3l2;
	module->ops = NULL;
	module->private_data = NULL;

	ext = &module->ext;
	ext->I2CSclk = I2C_L0;

	ext->sensor_con.product_name = SENSOR_NAME_S5K3L2;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C0;
	ext->sensor_con.peri_setting.i2c.slave_address = 0x20;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->actuator_con.product_name = ACTUATOR_NAME_DWXXXX;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C0;
	ext->actuator_con.peri_setting.i2c.slave_address = 0x20;
	ext->actuator_con.peri_setting.i2c.speed = 400000;

	ext->flash_con.product_name = FLADRV_NAME_AS3643;
	ext->flash_con.peri_type = SE_I2C;
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = 2;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = 3;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
 

#ifdef DEFAULT_IMX135_DRIVING
	v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);
#else
	v4l2_subdev_init(subdev_module, &subdev_ops);
#endif
	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->id);

p_err:
	minfo("%s(%d)\n", __func__, ret);
	return ret;
}
