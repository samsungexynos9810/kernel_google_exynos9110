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
};

extern struct decon_lcd *decon_get_lcd_info(void);

/* display_component_decon - This structure is abstraction of the
 * display controller device drvier. */
struct display_component_decon {
	struct resource *regs;
	int irq_no;
	int fifo_irq_no;
	int i80_irq_no;
	struct s3c_fb *sfb;
};

/* display_component_dsi - This structure is abstraction of the
 * MIPI-DSI device drvier. */
struct display_component_dsi {
	struct resource *regs;
	int dsi_irq_no;
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
};

struct platform_device;

struct display_driver *get_display_driver(void);
int create_decon_display_controller(struct platform_device *pdev);
int create_mipi_dsi_controller(struct platform_device *pdev);
#ifdef CONFIG_DECON_MIC
int create_decon_mic(struct platform_device *pdev);
#endif

#endif
