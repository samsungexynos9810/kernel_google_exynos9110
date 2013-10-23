/* drivers/video/fimd_display/fimd_dt.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __FIMD_DISPLAY_HEADER__
#define __FIMD_DISPLAY_HEADER__

int disp_pm_runtime_enable(struct display_driver *dispdrv);
int disp_pm_runtime_get_sync(struct display_driver *dispdrv);
int disp_pm_runtime_put_sync(struct display_driver *dispdrv);

int init_display_fimd_clocks_exynos(struct device *dev);
int enable_display_fimd_clocks_exynos(struct device *dev);
int disable_display_fimd_clocks_exynos(struct device *dev);
int enable_display_fimd_runtimepm_exynos(struct device *dev);
int disable_display_fimd_runtimepm_exynos(struct device *dev);

int init_display_dsi_clocks_exynos(struct device *dev);
int enable_display_dsi_clocks_exynos(struct device *dev);
int enable_display_dsi_power_exynos(struct device *dev);
int disable_display_dsi_power_exynos(struct device *dev);

void init_display_gpio_exynos(void);

#define init_display_decon_clocks(dev) \
	init_display_fimd_clocks_exynos(dev)
#define enable_display_fimd_clocks(dev) \
	enable_display_fimd_clocks_exynos(dev)
#define disable_display_fimd_clocks(dev) \
	disable_display_fimd_clocks_exynos(dev)
#define enable_display_fimd_runtimepm(dev) \
	enable_display_fimd_runtimepm_exynos(dev)
#define disable_display_fimd_runtimepm(dev) \
	disable_display_fimd_runtimepm_exynos(dev)

#define init_display_dsi_clocks(dev) \
	init_display_dsi_clocks_exynos(dev)
#define enable_display_dsi_power(dev) \
	enable_display_dsi_power_exynos(dev)
#define disable_display_dsi_power(dev) \
	disable_display_dsi_power_exynos(dev)

#define enable_display_dsi_clocks(dev) \
	enable_display_dsi_clocks_exynos(dev)
#endif
