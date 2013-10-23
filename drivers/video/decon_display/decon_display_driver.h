/* drivers/video/decon_display/decon_display_driver.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_DISPLAY_DRIVER_HEADER__
#define __DECON_DISPLAY_DRIVER_HEADER__

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

/* display_driver - Abstraction for display driver controlling
 * all display system in the system */
struct display_driver {
	/* platform driver for display system */
	struct device *display_driver;
	struct display_component_decon decon_driver;
	struct display_component_dsi dsi_driver;
};

struct platform_device;

struct display_driver *get_display_driver(void);
int create_decon_display_controller(struct platform_device *pdev);
int create_mipi_dsi_controller(struct platform_device *pdev);

#endif
