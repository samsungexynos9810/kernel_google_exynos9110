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
#include <mach/exynos-fimc-is-sensor.h>
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

static int parse_subip_info(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	u32 temp;
	char *pprop;
	struct exynos_fimc_is_subip_info *subip_info;

	/* get subip of fimc-is info */
	subip_info = kzalloc(sizeof(struct exynos_fimc_is_subip_info), GFP_KERNEL);
	if (!subip_info) {
		printk(KERN_ERR "%s: no memory for fimc_is subip_info\n", __func__);
		return -EINVAL;
	}

	DT_READ_U32(np, "num_of_3a0", subip_info->num_of_3a0 );
	DT_READ_U32(np, "num_of_3a1", subip_info->num_of_3a1 );
	DT_READ_U32(np, "num_of_isp", subip_info->num_of_isp );
	DT_READ_U32(np, "num_of_drc", subip_info->num_of_drc );
	DT_READ_U32(np, "num_of_scc", subip_info->num_of_scc );
	DT_READ_U32(np, "num_of_odc", subip_info->num_of_odc );
	DT_READ_U32(np, "num_of_dis", subip_info->num_of_dis );
	DT_READ_U32(np, "num_of_3dnr",subip_info->num_of_3dnr);
	DT_READ_U32(np, "num_of_scp", subip_info->num_of_scp );
	DT_READ_U32(np, "num_of_fd",  subip_info->num_of_fd  );

	pdata->subip_info = subip_info;

	return 0;

p_err:
	pr_err("%s: no property in the node, subip_info.\n", pprop);
	return -EINVAL;
}

struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	struct exynos_platform_fimc_is *pdata;
	struct device_node *subip_info_np;
	struct device_node *np = dev->of_node;

	if (!np)
		return ERR_PTR(-ENOENT);

	if (get_board_rev(dev) < 0)
		pr_warn("%s: Failed to get_board_rev\n", __func__);

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	pdata->clk_cfg = exynos5430_fimc_is_cfg_clk;
	pdata->clk_on = exynos5430_fimc_is_clk_on;
	pdata->clk_off = exynos5430_fimc_is_clk_off;
	pdata->print_cfg = exynos5_fimc_is_print_cfg;

	dev->platform_data = pdata;

	subip_info_np = of_find_node_by_name(np, "subip_info");
	if (!subip_info_np) {
		printk(KERN_ERR "%s: can't find fimc_is subip_info node\n", __func__);
		return ERR_PTR(-ENOENT);
	}
	parse_subip_info(pdata, subip_info_np);

	return pdata;
}

int fimc_is_sensor_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;
	int gpio_reset, gpio_standby;
	int gpio_comp_en, gpio_comp_rst;
	int gpio_none = 0;
	u32 id;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	if (get_board_rev(dev) < 0)
		pr_warn("%s: Failed to get_board_rev\n", __func__);

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_sensor_pins_cfg;
	pdata->iclk_cfg = exynos_fimc_is_sensor_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_sensor_iclk_on;
	pdata->iclk_off = exynos_fimc_is_sensor_iclk_off;
	pdata->mclk_on = exynos_fimc_is_sensor_mclk_on;
	pdata->mclk_off = exynos_fimc_is_sensor_mclk_off;

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "csi_ch", &pdata->csi_ch);
	if (ret) {
		err("csi_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "flite_ch", &pdata->flite_ch);
	if (ret) {
		err("flite_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "i2c_ch", &pdata->i2c_ch);
	if (ret) {
		err("i2c_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "i2c_addr", &pdata->i2c_addr);
	if (ret) {
		err("i2c_addr read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "id", &id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* Optional Feature */
	gpio_comp_en = of_get_named_gpio(dnode, "gpios_comp_en", 0);
	if (!gpio_is_valid(gpio_comp_en))
	dev_err(dev, "failed to get main comp en gpio\n");

	gpio_comp_rst = of_get_named_gpio(dnode, "gpios_comp_reset", 0);
	if (!gpio_is_valid(gpio_comp_rst))
	dev_err(dev, "failed to get main comp reset gpio\n");

	gpio_standby = of_get_named_gpio(dnode, "gpio_standby", 0);
	if (!gpio_is_valid(gpio_standby))
		dev_err(dev, "failed to get gpio_standby\n");

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_reset, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_comp_en, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpio_comp_rst, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_none, PIN_END);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_reset, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_reset, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_comp_rst, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_comp_rst, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_comp_en, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_none, PIN_END);

	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0, gpio_reset, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1, gpio_standby, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2, gpio_none, PIN_END);

	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0, gpio_reset, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1, gpio_reset, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2, gpio_standby, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3, gpio_none, PIN_END);

	/* Xyref5430 board revision config */
	if ((id == SENSOR_POSITION_FRONT) && board_rev) {
		pdata->mclk_ch = 2;
		pdata->csi_ch = 2;
		pdata->flite_ch = 2;
		pdata->i2c_ch = 2;
	}

	pdev->id = id;

	dev->platform_data = pdata;

p_err:
	return ret;
}
#else
struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
