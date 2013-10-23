/* drivers/video/fimd_display/fimd_dt.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __FIMD_DISPLAY_DT_HEADER__
#define __FIMD_DISPLAY_DT_HEADER__

void dump_driver_data(void);
void dump_s3c_fb_variant(struct s3c_fb_variant *p_fb_variant);
void dump_s3c_fb_win_variant(struct s3c_fb_win_variant *p_fb_win_variant);
void dump_s3c_fb_win_variants(struct s3c_fb_win_variant p_fb_win_variant[],
	int num);

int parse_display_driver_dt_exynos(struct platform_device *np,
	struct display_driver *ddp);

struct s3c_fb_driverdata *get_display_drvdata_exynos(void);
struct s3c_fb_platdata *get_display_platdata_exynos(void);
struct mipi_dsim_config *get_display_dsi_drvdata_exynos(void);
struct mipi_dsim_lcd_config *get_display_lcd_drvdata_exynos(void);

int parse_display_dsi_dt_exynos(struct device_node *np);
int get_display_dsi_lcd_reset_gpio_exynos(void);
int get_display_dsi_lcd_power_gpio_exynos(void);

/* interface for all display nodes in display sub system hierarchy */
#define parse_display_driver_dt(node, ...) \
	parse_display_driver_dt_exynos(node, __VA_ARGS__)

#define get_display_drvdata() get_display_drvdata_exynos()
#define get_display_platdata() get_display_platdata_exynos()
#define get_display_dsi_drvdata() get_display_dsi_drvdata_exynos()
#define get_display_lcd_drvdata() get_display_lcd_drvdata_exynos()

/* Temporary code for parsinng DSI device tree */
#define parse_display_dsi_dt(node) parse_display_dsi_dt_exynos(node)
#define get_display_dsi_reset_gpio() get_display_dsi_reset_gpio_exynos()

#endif
