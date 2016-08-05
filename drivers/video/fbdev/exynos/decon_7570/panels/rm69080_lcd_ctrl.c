/* rm69080_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * Manseok Kim, <Manseoks.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "rm69080_param.h"
#include "lcd_ctrl.h"

/* use FW_TEST definition when you test CAL on firmware */
/* #define FW_TEST */
#ifdef FW_TEST
#include "../dsim_fw.h"
#include "mipi_display.h"
#else
#include "../dsim.h"
#include <video/mipi_display.h>
#endif

#define VIDEO_MODE      1
#define COMMAND_MODE    0

#define ID		0

struct decon_lcd rm69080_lcd_info = {
	/* Only availaable VIDEO MODE */
	.mode = VIDEO_MODE,

	.decon_vfp = 0x1,
	.decon_vbp = 0xD,
	.decon_hfp = 0x18,
	.decon_hfp = 0x18,
	.decon_hbp = 0x18,
	.decon_vsa = 0x02,
	.decon_hsa = 0x02,

	.dsim_vfp = 0x1,
	.dsim_vbp = 0xD,
	.dsim_hfp = 0x18,
	.dsim_hfp = 0x18,
	.dsim_hbp = 0x18,
	.dsim_vsa = 0x02,
	.dsim_hsa = 0x02,

	.xres = 800,
	.yres = 1280,

	.width = 71,
	.height = 114,

	/* Mhz */
	.hs_clk = 480,
	.esc_clk = 20,

	.fps = 60,
};

struct decon_lcd *decon_get_lcd_info(void)
{
	return &rm69080_lcd_info;
}

void lcd_init(struct decon_lcd * lcd)
{
	/*cmd_mode_page_1_set (to be added)*/
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_mode_page_1_set,
		ARRAY_SIZE(cmd_mode_page_1_set)) == -1)
		dsim_err("failed to send cmd_mode_page_1_set.\n");

	/*cmd_errors_number_read*/
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_errors_number_read,
		ARRAY_SIZE(cmd_errors_number_read)) == -1)
		dsim_err("failed to send cmd_errors_number_read.\n");

	/*cmd_mode_page_2_set */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_mode_page_2_set,
		ARRAY_SIZE(cmd_mode_page_2_set)) == -1)
		dsim_err("failed to send cmd_mode_page_2_set.\n");

	/*cmd_07_set */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_07_set,
		ARRAY_SIZE(cmd_07_set)) == -1)
		dsim_err("failed to send cmd_07_set.\n");

	/*cmd_mode_page_3_set */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_mode_page_3_set,
		ARRAY_SIZE(cmd_mode_page_3_set)) == -1)
		dsim_err("failed to send cmd_mode_page_3_set.\n");

	/*cmd_1C_set */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_1C_set,
		ARRAY_SIZE(cmd_1C_set)) == -1)
		dsim_err("failed to send cmd_1C_set.\n");

	/*cmd_mode_page_4_set */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_mode_page_4_set,
		ARRAY_SIZE(cmd_mode_page_4_set)) == -1)
		dsim_err("failed to send cmd_mode_page_4_set.\n");

	/*cmd_TE_line_on */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_TE_line_on,
		ARRAY_SIZE(cmd_TE_line_on)) == -1)
		dsim_err("failed to cmd_TE_line_on.\n");

	/*idle mode */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x39, 0);

	/*cmd_brightness_set */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_brightness_set,
		ARRAY_SIZE(cmd_brightness_set)) == -1)
		dsim_err("failed to cmd_brightness_set.\n");


	/* sleep out */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x11, 0);

	/* 300ms delay */
	msleep(300);

	/* display on */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x29, 0);
}


void lcd_enable(void)
{
}

void lcd_disable(void)
{
	/* display off */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x28, 0);

	/* sleep in */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x10, 0);

	/* 120ms delay */
	msleep(120);
}

int lcd_gamma_ctrl(u32 backlightlevel)
{
	return 0;
}

int lcd_gamma_update(void)
{
	return 0;
}

void lcd_brightness_set(int brightness)
{
	unsigned char buffer[1] = "";
	unsigned char reg_brightness_set[2] = "";

	sprintf(buffer, "0x%02X\n", brightness);
	reg_brightness_set[0] = 0x51;
	reg_brightness_set[1] = buffer[0];

	/*brightness_set */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) reg_brightness_set,
		ARRAY_SIZE(reg_brightness_set)) == -1)
		dsim_err("failed to brightness_set.\n");

}
