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
struct exynos5_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	struct exynos5_platform_fimc_is *pdata;
	struct device_node *np = dev->of_node;
	struct fimc_is_gpio_info *gpio_info;
	struct device_node *ctrl_np;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = kzalloc(sizeof(struct exynos5_platform_fimc_is), GFP_KERNEL);
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

	gpio_info->gpio_main_rst = of_get_gpio(np, 0);
	if (!gpio_is_valid(gpio_info->gpio_main_rst)) {
		dev_err(dev, "failed to get main reset gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_main_sda = of_get_gpio(np, 1);
	if (!gpio_is_valid(gpio_info->gpio_main_sda)) {
		dev_err(dev, "failed to get main sda gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_main_scl = of_get_gpio(np, 2);
	if (!gpio_is_valid(gpio_info->gpio_main_scl)) {
		dev_err(dev, "failed to get main scl gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_main_mclk = of_get_gpio(np, 3);
	if (!gpio_is_valid(gpio_info->gpio_main_mclk)) {
		dev_err(dev, "failed to get main mclk gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_main_flash_en = of_get_gpio(np, 4);
	if (!gpio_is_valid(gpio_info->gpio_main_flash_en)) {
		dev_err(dev, "failed to get main flash_en gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_main_flash_torch = of_get_gpio(np, 5);
	if (!gpio_is_valid(gpio_info->gpio_main_flash_torch)) {
		dev_err(dev, "failed to get main flash_torch gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_vt_rst = of_get_gpio(np, 6);
	if (!gpio_is_valid(gpio_info->gpio_vt_rst)) {
		dev_err(dev, "failed to get vt reset gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_vt_sda = of_get_gpio(np, 7);
	if (!gpio_is_valid(gpio_info->gpio_vt_sda)) {
		dev_err(dev, "failed to get vt sda gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_vt_scl = of_get_gpio(np, 8);
	if (!gpio_is_valid(gpio_info->gpio_vt_scl)) {
		dev_err(dev, "failed to get vt scl gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_vt_mclk = of_get_gpio(np, 9);
	if (!gpio_is_valid(gpio_info->gpio_vt_mclk)) {
		dev_err(dev, "failed to get vt mclk gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_spi_clk = of_get_gpio(np, 10);
	if (!gpio_is_valid(gpio_info->gpio_spi_clk)) {
		dev_err(dev, "failed to get spi clk gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_spi_csn = of_get_gpio(np, 11);
	if (!gpio_is_valid(gpio_info->gpio_spi_csn)) {
		dev_err(dev, "failed to get spi csn gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_spi_miso = of_get_gpio(np, 12);
	if (!gpio_is_valid(gpio_info->gpio_spi_miso)) {
		dev_err(dev, "failed to get spi miso gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_spi_mosi = of_get_gpio(np, 13);
	if (!gpio_is_valid(gpio_info->gpio_spi_mosi)) {
		dev_err(dev, "failed to get spi mosi gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_uart_txd = of_get_gpio(np, 14);
	if (!gpio_is_valid(gpio_info->gpio_uart_txd)) {
		dev_err(dev, "failed to get uart txd gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_uart_rxd = of_get_gpio(np, 15);
	if (!gpio_is_valid(gpio_info->gpio_uart_rxd)) {
		dev_err(dev, "failed to get uart rxd gpio\n");
		return ERR_PTR(-EINVAL);
	}

	pdata->_gpio_info = gpio_info;
	dev->platform_data = pdata;

	return pdata;
}
#else
struct exynos5_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
