/* s6e3ha2k_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * SeungBeom, Park <sb1.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s6e36w1x01_param.h"
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

#define LDI_ID_REG	0x04
#define LDI_ID_LEN	3

#define ID		0

#define VIDEO_MODE      1
#define COMMAND_MODE    0

struct decon_lcd s6e36w1x01_lcd_info = {
	/* Only availaable COMMAND MODE */
	.mode = COMMAND_MODE,

	.decon_vfp = 2,
	.decon_vbp = 1,
	.decon_hfp = 1,
	.decon_hbp = 1,
	.decon_vsa = 1,
	.decon_hsa = 1,

	.dsim_vfp = 2,
	.dsim_vbp = 1,
	.dsim_hfp = 1,
	.dsim_hbp = 1,
	.dsim_vsa = 1,
	.dsim_hsa = 1,

	.xres = 360,
	.yres = 360,

	.width = 71,
	.height = 114,

	/* Mhz */
	.hs_clk = 384,
	.esc_clk = 8,

	.fps = 60,
};

struct decon_lcd *decon_get_lcd_info(void)
{
	return &s6e36w1x01_lcd_info;
}

void lcd_init(struct decon_lcd * lcd)
{
	/* Test key enable */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) == -1)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_1,
		ARRAY_SIZE(TEST_KEY_ON_1)) == -1)
		dsim_err("failed to send TEST_KEY_ON_1.\n");

	/* sleep out */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) SLPOUT,
		ARRAY_SIZE(SLPOUT)) == -1)
		dsim_err("failed to send SLPOUTs.\n");

	/* 120ms delay */
	msleep(120);

	/* Module Information Read */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) GAMMA_360,
		ARRAY_SIZE(GAMMA_360)) == -1)
		dsim_err("failed to send GAMMA_360.\n");

	/* Brightness Setting */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) AOR_360,
		ARRAY_SIZE(AOR_360)) == -1)
		dsim_err("failed to send AOR_360.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) ELVSS_360,
		ARRAY_SIZE(ELVSS_360)) == -1)
		dsim_err("failed to send ELVSS_360.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) VINT_360,
		ARRAY_SIZE(VINT_360)) == -1)
		dsim_err("failed to send VINT_360.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) PANEL_UPDATE,
		ARRAY_SIZE(PANEL_UPDATE)) == -1)
		dsim_err("failed to send PANEL_UPDATE.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEON,
		ARRAY_SIZE(TEON)) == -1)
		dsim_err("failed to send TEON.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) HIDDEN_KEY_ON,
		ARRAY_SIZE(HIDDEN_KEY_ON)) == -1)
		dsim_err("failed to send HIDDEN_KEY_ON.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) ETC_GPARA,
		ARRAY_SIZE(ETC_GPARA)) == -1)
		dsim_err("failed to send ETC_GPARA.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) ETC_SET,
		ARRAY_SIZE(ETC_SET)) == -1)
		dsim_err("failed to send ETC_SET.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) HIDDEN_KEY_OFF,
		ARRAY_SIZE(HIDDEN_KEY_OFF)) == -1)
		dsim_err("failed to send HIDDEN_KEY_OFF.\n");

	/* Test key disable */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_1,
		ARRAY_SIZE(TEST_KEY_OFF_1)) == -1)
		dsim_err("failed to send TEST_KEY_OFF_1.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) == -1)
		dsim_err("failed to send TEST_KEY_OFF_0.\n");

	/* 120ms delay */
	msleep(120);

	/* display on */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) DISPON,
		ARRAY_SIZE(DISPON)) == -1)
		dsim_err("failed to send SEQ_DISPLAY_ON.\n");
}

void lcd_enable(void)
{
}

void lcd_disable(void)
{
}

int lcd_gamma_ctrl(u32 backlightlevel)
{
	return 0;
}

int lcd_gamma_update(void)
{
	return 0;
}
