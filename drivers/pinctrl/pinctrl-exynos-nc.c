/*
 * pinctrl-nc driver to set nc-gpio pin for Samsung's SoC's.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This driver implements the Samsung pinctrl driver. It supports setting up of
 * pinmux and pinconf configurations. The gpiolib interface is also included.
 * External interrupt (gpio and wakeup) support are not included in this driver
 * but provides extensions to which platform specific implementation of the gpio
 * and wakeup interrupts can be hooked to.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/irqdomain.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>

#include <mach/pinctrl-samsung.h>

#define PINCTRL_STATE_NC "nc_pin"

static int samsung_pinctrl_nc_probe(struct platform_device *pdev)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_nc;
	struct device *dev = &pdev->dev;
	int status = 0;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "could not get pinctrl device.\n");
		status = PTR_ERR(pinctrl);
		return status;
	}

	pins_nc = pinctrl_lookup_state(pinctrl, PINCTRL_STATE_NC);
	if (!IS_ERR(pins_nc)) {
		status = pinctrl_select_state(pinctrl, pins_nc);
		if (status) {
			dev_err(dev, "could not set nc-gpio pins.\n");
			return status;
		}
	} else {
		dev_err(dev, "could not get nc-gpio pin state.\n");
		status = PTR_ERR(pins_nc);
		return status;
	}

	dev_info(dev, "Completed to set nc-gpio pins.\n");

	return status;
}

static const struct of_device_id samsung_pinctrl_nc_dt_match[] = {
	{ .compatible = "samsung,exynos-pinctrl-nc",
	},
};
MODULE_DEVICE_TABLE(of, samsung_pinctrl_nc_dt_match);

static struct platform_driver samsung_pinctrl_nc_driver = {
	.probe		= samsung_pinctrl_nc_probe,
	.driver = {
		.name	= "samsung-pinctrl-nc",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_pinctrl_nc_dt_match),
	},
};

static int __init samsung_pinctrl_nc_register(void)
{
	return platform_driver_register(&samsung_pinctrl_nc_driver);
}
postcore_initcall(samsung_pinctrl_nc_register);

static void __exit samsung_pinctrl_nc_unregister(void)
{
	platform_driver_unregister(&samsung_pinctrl_nc_driver);
}
module_exit(samsung_pinctrl_nc_unregister);
