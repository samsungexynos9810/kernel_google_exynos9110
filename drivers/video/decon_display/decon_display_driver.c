/* linux/drivers/video/decon_display/decon_display_driver.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/lcd.h>
#include <linux/gpio.h>
#include <linux/exynos_iovmm.h>

#include "decon_display_driver.h"
#ifdef CONFIG_SOC_EXYNOS5430
#include "decon_fb.h"
#include "decon_dt.h"
#include "decon_pm.h"
#else
#include "fimd_fb.h"
#include "fimd_dt.h"
#include "fimd_pm.h"
#endif
#ifdef CONFIG_OF
static const struct of_device_id decon_disp_device_table[] = {
	{ .compatible = "samsung,exynos5-disp_driver" },
	{},
};
MODULE_DEVICE_TABLE(of, decon_disp_device_table);
#endif

static struct display_driver g_display_driver;

/* create_disp_components - create all components in display sub-system.
 * */
static int create_disp_components(struct platform_device *pdev)
{
	int ret = 0;

	/* IMPORTANT: MIPI-DSI component should be 1'st created. */
	ret = create_mipi_dsi_controller(pdev);
	if (ret < 0) {
		pr_err("display error: mipi-dsi controller create failed.");
		return ret;
	}

	ret = create_decon_display_controller(pdev);
	if (ret < 0) {
		pr_err("display error: display controller create failed.");
		return ret;
	}

	return ret;
}

/* disp_driver_fault_handler - fault handler for display device driver */
int disp_driver_fault_handler(struct iommu_domain *iodmn, struct device *dev,
	unsigned long addr, int id, void *param)
{
	struct display_driver *dispdrv;

	dispdrv = (struct display_driver*)param;
	decon_dump_registers(dispdrv);
	return 0;
}

/* register_debug_features - for registering debug features.
 * currently registered features are like as follows...
 * - iovmm falult handler
 * - ... */
static void register_debug_features(void)
{
	/* 1. fault handler registration */
	iovmm_set_fault_handler(g_display_driver.display_driver,
		disp_driver_fault_handler, &g_display_driver);
}

/* s5p_decon_disp_probe - probe function of the display driver */
static int s5p_decon_disp_probe(struct platform_device *pdev)
{
	int ret = -1;

	/* parse display driver device tree & convers it to objects
	 * for each platform device */
	ret = parse_display_driver_dt(pdev, &g_display_driver);

	init_display_dsi_clocks(&pdev->dev);
	init_display_decon_clocks(&pdev->dev);

	create_disp_components(pdev);

	register_debug_features();

	return ret;
}

static int s5p_decon_disp_remove(struct platform_device *pdev)
{
	return 0;
}

static int display_driver_runtime_suspend(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	return s3c_fb_runtime_suspend(dev);
#else
	return 0;
#endif
}

static int display_driver_runtime_resume(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	return s3c_fb_runtime_resume(dev);
#else
	return 0;
#endif
}

#ifdef CONFIG_PM_SLEEP
static int display_driver_resume(struct device *dev)
{
	return s3c_fb_resume(dev);
}

static int display_driver_suspend(struct device *dev)
{
	return s3c_fb_suspend(dev);
}
#endif

static const struct dev_pm_ops s5p_decon_disp_ops = {
#ifdef CONFIG_PM_SLEEP
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = display_driver_suspend,
	.resume = display_driver_resume,
#endif
#endif
	.runtime_suspend	= display_driver_runtime_suspend,
	.runtime_resume		= display_driver_runtime_resume,
};

/* get_display_driver - for returning reference of display
 * driver context */
struct display_driver *get_display_driver(void)
{
	return &g_display_driver;
}



static struct platform_driver s5p_decon_disp_driver = {
	.probe = s5p_decon_disp_probe,
	.remove = s5p_decon_disp_remove,
	.driver = {
		.name = "s5p-decon-display",
		.owner = THIS_MODULE,
		.pm = &s5p_decon_disp_ops,
		.of_match_table = of_match_ptr(decon_disp_device_table),
	},
};

static int s5p_decon_disp_register(void)
{
	platform_driver_register(&s5p_decon_disp_driver);

	return 0;
}

static void s5p_decon_disp_unregister(void)
{
	platform_driver_unregister(&s5p_decon_disp_driver);
}
late_initcall(s5p_decon_disp_register);
module_exit(s5p_decon_disp_unregister);

MODULE_AUTHOR("Donggyun, ko <donggyun.ko@samsung.com>");
MODULE_DESCRIPTION("Samusung DECON-DISP driver");
MODULE_LICENSE("GPL");
