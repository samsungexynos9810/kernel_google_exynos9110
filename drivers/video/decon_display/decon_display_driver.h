/* drivers/video/decon_display/decon_display_driver.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_DISPLAY_DRIVER_HEADER__
#define __DECON_DISPLAY_DRIVER_HEADER__

#define COMMAND_MODE	1
#define VIDEO_MODE	0

struct decon_lcd {
	u32	mode;
	u32	vfp;
	u32	vbp;
	u32	hfp;
	u32	hbp;

	u32	vsa;
	u32	hsa;

	u32	xres;
	u32	yres;

	u32	width;
	u32	height;

	u32	hs_clk;
	u32	esc_clk;

	u32	fps;
};

extern struct decon_lcd *decon_get_lcd_info(void);

struct display_driver;

#define MAX_GPIO	10

/* display_gpio - GPIOs resource for the display subsystem */
struct display_gpio {
	int num;
	unsigned id[MAX_GPIO];
};

/* display_controller_ops - operations for controlling power of
 * dispay controller. */
struct display_controller_ops {
	int (*init_display_decon_clocks)(struct device *dev);
	int (*enable_display_decon_clocks)(struct device *dev);
	int (*disable_display_decon_clocks)(struct device *dev);
	int (*enable_display_decon_runtimepm)(struct device *dev);
	int (*disable_display_decon_runtimepm)(struct device *dev);
};

/* display_driverr_ops - operations for controlling power of
 * device */
struct display_driver_ops {
	int (*init_display_driver_clocks)(struct device *dev);
	int (*enable_display_driver_clocks)(struct device *dev);
	int (*enable_display_driver_power)(struct device *dev);
	int (*disable_display_driver_power)(struct device *dev);
};

/* display_dt_ops - operations for parsing device tree */
struct display_dt_ops {
	int (*parse_display_driver_dt)(struct platform_device *np,
		struct display_driver *ddp);
	struct s3c_fb_driverdata *(*get_display_drvdata)(void);
	struct s3c_fb_platdata *(*get_display_platdata)(void);
	struct mipi_dsim_config *(*get_display_dsi_drvdata)(void);
	struct mipi_dsim_lcd_config *(*get_display_lcd_drvdata)(void);
	struct display_gpio *(*get_display_dsi_reset_gpio)(void);
#ifdef CONFIG_DECON_MIC
	struct mic_config *(*get_display_mic_config)(void);
#endif
};

/* display_component_decon - This structure is abstraction of the
 * display controller device drvier. */
struct display_component_decon {
	struct resource *regs;
	int irq_no;
	int fifo_irq_no;
	int i80_irq_no;
	struct s3c_fb *sfb;
	struct display_controller_ops decon_ops;
};

/* display_component_dsi - This structure is abstraction of the
 * MIPI-DSI device drvier. */
struct display_component_dsi {
	struct resource *regs;
	int dsi_irq_no;
	struct display_driver_ops dsi_ops;
	struct mipi_dsim_device *dsim;
};

#ifdef CONFIG_DECON_MIC
struct display_component_mic {
	struct resource *regs;
};
#endif

/* display_driver - Abstraction for display driver controlling
 * all display system in the system */
struct display_driver {
	/* platform driver for display system */
	struct device *display_driver;
	struct display_component_decon decon_driver;
	struct display_component_dsi dsi_driver;
#ifdef CONFIG_DECON_MIC
	struct display_component_mic mic_driver;
#endif
	struct display_dt_ops dt_ops;
};

#define GET_DISPDRV_OPS(p) (p)->dsi_driver.dsi_ops
#define GET_DISPCTL_OPS(p) (p)->decon_driver.decon_ops

struct platform_device;

struct display_driver *get_display_driver(void);
int create_decon_display_controller(struct platform_device *pdev);
int create_mipi_dsi_controller(struct platform_device *pdev);
#ifdef CONFIG_DECON_MIC
int create_decon_mic(struct platform_device *pdev);
#endif

#endif
