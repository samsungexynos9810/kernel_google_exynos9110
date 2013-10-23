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

#include <linux/sched.h>
#include <mach/exynos-fimc-is.h>
#include <media/exynos_mc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "fimc-is-dt.h"

#ifdef CONFIG_OF
static int board_rev = 0;
static int get_board_rev(struct device *dev)
{
	int ret = 0;
	int board_rev_pin0, board_rev_pin1;
	struct device_node *np = dev->of_node;

	board_rev_pin0 = of_get_named_gpio(np, "gpios_board_rev", 0);
	if (!gpio_is_valid(board_rev_pin0)) {
		dev_err(dev, "failed to get main board_rev_pin0\n");
		ret = -EINVAL;
		goto p_err;
	}

	board_rev_pin1 = of_get_named_gpio(np, "gpios_board_rev", 1);
	if (!gpio_is_valid(board_rev_pin1)) {
		dev_err(dev, "failed to get main board_rev_pin1\n");
		ret = -EINVAL;
		goto p_err;
	}

	gpio_request_one(board_rev_pin0, GPIOF_IN, "BOARD_REV_PIN0");
	gpio_request_one(board_rev_pin1, GPIOF_IN, "BOARD_REV_PIN1");
	board_rev = __gpio_get_value(board_rev_pin0) << 0;
	board_rev |= __gpio_get_value(board_rev_pin1) << 1;

p_err:
	return ret;
}

static int parse_sensor_info(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	u32 i;
	u32 temp;
	char *pprop;
	const char *name;

	for (i = 0; i < FIMC_IS_MAX_CAMIF_CLIENTS; i++) {
		pdata->sensor_info[i] = kzalloc(sizeof(struct exynos_fimc_is_sensor_info), GFP_KERNEL);
		if (!pdata->sensor_info[i]) {
			printk(KERN_ERR "%s: no memory for sensor_info[%d]\n", __func__, i);
			return -EINVAL;
		}
	}

	DT_READ_STR(np, "rear_sensor_name", pdata->sensor_info[SENSOR_POSITION_REAR]->sensor_name);
	DT_READ_U32(np, "rear_sensor_position", pdata->sensor_info[SENSOR_POSITION_REAR]->sensor_position);
	DT_READ_U32(np, "rear_sensor_id", pdata->sensor_info[SENSOR_POSITION_REAR]->sensor_id);
	DT_READ_U32(np, "rear_clk_src", pdata->sensor_info[SENSOR_POSITION_REAR]->clk_src);
	DT_READ_U32(np, "rear_csis_id", pdata->sensor_info[SENSOR_POSITION_REAR]->csi_id);
	DT_READ_U32(np, "rear_flite_id", pdata->sensor_info[SENSOR_POSITION_REAR]->flite_id);
	DT_READ_U32(np, "rear_i2c_channel", pdata->sensor_info[SENSOR_POSITION_REAR]->i2c_channel);
	DT_READ_U32(np, "rear_sensor_slave_address", pdata->sensor_info[SENSOR_POSITION_REAR]->sensor_slave_address);
	DT_READ_U32(np, "rear_actuator_id", pdata->sensor_info[SENSOR_POSITION_REAR]->actuator_id);
	DT_READ_U32(np, "rear_actuator_i2c", pdata->sensor_info[SENSOR_POSITION_REAR]->actuator_i2c);
	DT_READ_U32(np, "rear_flash_id", pdata->sensor_info[SENSOR_POSITION_REAR]->flash_id);
	DT_READ_U32(np, "rear_flash_peri_type", pdata->sensor_info[SENSOR_POSITION_REAR]->flash_peri_type);
	DT_READ_U32(np, "rear_flash_first_gpio", pdata->sensor_info[SENSOR_POSITION_REAR]->flash_first_gpio);
	DT_READ_U32(np, "rear_flash_second_gpio", pdata->sensor_info[SENSOR_POSITION_REAR]->flash_second_gpio);

	DT_READ_STR(np, "front_sensor_name", pdata->sensor_info[SENSOR_POSITION_FRONT]->sensor_name);
	DT_READ_U32(np, "front_sensor_position", pdata->sensor_info[SENSOR_POSITION_FRONT]->sensor_position);
	DT_READ_U32(np, "front_sensor_id", pdata->sensor_info[SENSOR_POSITION_FRONT]->sensor_id);
	DT_READ_U32(np, "front_clk_src", pdata->sensor_info[SENSOR_POSITION_FRONT]->clk_src);
	DT_READ_U32(np, "front_csis_id", pdata->sensor_info[SENSOR_POSITION_FRONT]->csi_id);
	DT_READ_U32(np, "front_flite_id", pdata->sensor_info[SENSOR_POSITION_FRONT]->flite_id);
	DT_READ_U32(np, "front_i2c_channel", pdata->sensor_info[SENSOR_POSITION_FRONT]->i2c_channel);
	DT_READ_U32(np, "front_sensor_slave_address", pdata->sensor_info[SENSOR_POSITION_FRONT]->sensor_slave_address);

	/* Xyref5430 board revision config */
	if (board_rev) {
		pdata->sensor_info[SENSOR_POSITION_FRONT]->clk_src = 2;
		pdata->sensor_info[SENSOR_POSITION_FRONT]->csi_id = 2;
		pdata->sensor_info[SENSOR_POSITION_FRONT]->flite_id = 2;
		pdata->sensor_info[SENSOR_POSITION_FRONT]->i2c_channel = 2;
	}

	return 0;

p_err:
	pr_err("%s: no property in the node, sensor_info.\n", pprop);
	return -EINVAL;
}

static int fimc_is_parse_ctrl_dt(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	int ret = 0;
	struct device_node *sensor_np;

	if (!np) {
		pr_err("%s: no devicenode given\n", of_node_full_name(np));
		ret = -EINVAL;
		goto p_err;
	}

	sensor_np = of_find_node_by_name(np, "fimc_is_sensor");
	if (!sensor_np) {
		pr_err("%s: could not find fimc_is_sensor node\n",
			of_node_full_name(np));
		ret = -EINVAL;
		goto p_err;
	}

	ret = parse_sensor_info(pdata, sensor_np);
	if (ret < 0) {
		pr_err("parsing sensor_data is failed.\n");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	struct exynos_platform_fimc_is *pdata;
	struct device_node *np = dev->of_node;
	struct fimc_is_gpio_info *gpio_info;
	struct device_node *ctrl_np;

	if (!np)
		return ERR_PTR(-ENOENT);

	if (get_board_rev(dev) < 0)
		pr_warn("%s: Failed to get_board_rev\n", __func__);

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	pdata->cfg_gpio = exynos5_fimc_is_cfg_gpio;
	pdata->clk_cfg = exynos5430_fimc_is_cfg_clk;
	pdata->clk_on = exynos5430_fimc_is_clk_on;
	pdata->clk_off = exynos5430_fimc_is_clk_off;
	pdata->sensor_clock_on = exynos5430_fimc_is_sensor_clk_on;
	pdata->sensor_clock_off = exynos5430_fimc_is_sensor_clk_off;
	pdata->sensor_power_on = exynos5_fimc_is_sensor_power_on;
	pdata->sensor_power_off = exynos5_fimc_is_sensor_power_off;
	pdata->print_cfg = exynos5_fimc_is_print_cfg;

	gpio_info = kzalloc(sizeof(struct fimc_is_gpio_info), GFP_KERNEL);
	if (!gpio_info) {
		printk(KERN_ERR "%s: no memory for gpio_info\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	gpio_info->gpio_main_rst = of_get_named_gpio(np, "gpios_main_reset", 0);
	if (!gpio_is_valid(gpio_info->gpio_main_rst)) {
		dev_err(dev, "failed to get main reset gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_vt_rst = of_get_named_gpio(np, "gpios_vt_reset", 0);
	if (!gpio_is_valid(gpio_info->gpio_vt_rst)) {
		dev_err(dev, "failed to get vt reset gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_comp_en = of_get_named_gpio(np, "gpios_comp_en", 0);
	if (!gpio_is_valid(gpio_info->gpio_comp_en))
		dev_err(dev, "failed to get main comp en gpio\n");

	gpio_info->gpio_comp_rst = of_get_named_gpio(np, "gpios_comp_reset", 0);
	if (!gpio_is_valid(gpio_info->gpio_comp_rst))
		dev_err(dev, "failed to get main comp reset gpio\n");

	pdata->_gpio_info = gpio_info;

	ctrl_np = of_get_child_by_name(np, "fimc_is_ctrl");
	if (!ctrl_np) {
		pr_err("device tree errror : empty dt node\n");
		return ERR_PTR(-EINVAL);
	}

	fimc_is_parse_ctrl_dt(pdata, ctrl_np);

	dev->platform_data = pdata;

	return pdata;
}
#else
struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
