/* drivers/video/decon_display/decon_dt.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_DISPLAY_DT_HEADER__
#define __DECON_DISPLAY_DT_HEADER__

#ifdef CONFIG_SOC_EXYNOS5430
void dump_driver_data(void);
void dump_s3c_fb_variant(struct s3c_fb_variant *p_fb_variant);
void dump_s3c_fb_win_variant(struct s3c_fb_win_variant *p_fb_win_variant);
void dump_s3c_fb_win_variants(struct s3c_fb_win_variant p_fb_win_variant[],
	int num);

int parse_display_driver_dt_exynos5430(struct platform_device *np,
	struct display_driver *ddp);

struct s3c_fb_driverdata *get_display_drvdata_exynos5430(void);
struct s3c_fb_platdata *get_display_platdata_exynos5430(void);
struct mipi_dsim_config *get_display_dsi_drvdata_exynos5430(void);
struct mipi_dsim_lcd_config *get_display_lcd_drvdata_exynos5430(void);
#ifdef CONFIG_DECON_MIC
struct mic_config *get_display_mic_config_exynos5430(void);
#endif

int parse_display_dsi_dt_exynos5430(struct device_node *np);
int get_display_dsi_reset_gpio_exynos5430(void);

/* interface for all display nodes in display sub system hierarchy */
#define parse_display_driver_dt(node, ...) \
	parse_display_driver_dt_exynos5430(node, __VA_ARGS__)

#define get_display_drvdata() get_display_drvdata_exynos5430()
#define get_display_platdata() get_display_platdata_exynos5430()
#define get_display_dsi_drvdata() get_display_dsi_drvdata_exynos5430()
#define get_display_lcd_drvdata() get_display_lcd_drvdata_exynos5430()
#ifdef CONFIG_DECON_MIC
#define get_display_mic_config() get_display_mic_config_exynos5430()
#endif

/* Temporary code for parsinng DSI device tree */
#define parse_display_dsi_dt(node) parse_display_dsi_dt_exynos5430(node)
#define get_display_dsi_reset_gpio() get_display_dsi_reset_gpio_exynos5430()

#endif

#endif
