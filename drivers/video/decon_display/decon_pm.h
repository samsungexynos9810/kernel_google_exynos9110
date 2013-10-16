/* drivers/video/decon_display/decon_dt.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_DISPLAY_HEADER__
#define __DECON_DISPLAY_HEADER__

int disp_pm_runtime_enable(struct display_driver *dispdrv);
int disp_pm_runtime_get_sync(struct display_driver *dispdrv);
int disp_pm_runtime_put_sync(struct display_driver *dispdrv);

#ifdef CONFIG_SOC_EXYNOS5430
int init_display_decon_clocks_exynos5430(struct device *dev);
int enable_display_decon_clocks_exynos5430(struct device *dev);
int disable_display_decon_clocks_exynos5430(struct device *dev);
int enable_display_decon_runtimepm_exynos5430(struct device *dev);
int disable_display_decon_runtimepm_exynos5430(struct device *dev);

int init_display_dsi_clocks_exynos5430(struct device *dev);
int enable_display_dsi_clocks_exynos5430(struct device *dev);
int enable_display_dsi_power_exynos5430(struct device *dev);
int disable_display_dsi_power_exynos5430(struct device *dev);

#define init_display_decon_clocks(dev) \
	init_display_decon_clocks_exynos5430(dev)
#define enable_display_decon_clocks(dev) \
	enable_display_decon_clocks_exynos5430(dev)
#define disable_display_decon_clocks(dev) \
	disable_display_decon_clocks_exynos5430(dev)
#define enable_display_decon_runtimepm(dev) \
	enable_display_decon_runtimepm_exynos5430(dev)
#define disable_display_decon_runtimepm(dev) \
	disable_display_decon_runtimepm_exynos5430(dev)

#define init_display_dsi_clocks(dev) \
	init_display_dsi_clocks_exynos5430(dev)
#define enable_display_dsi_power(dev) \
	enable_display_dsi_power_exynos5430(dev)
#define disable_display_dsi_power(dev) \
	disable_display_dsi_power_exynos5430(dev)

#define enable_display_dsi_clocks(dev) \
	enable_display_dsi_clocks_exynos5430(dev)
#endif

#endif
