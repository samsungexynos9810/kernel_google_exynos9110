/*
	auo_h120bln017_lcd_ctrl.c
*/

#include "auo_h120bln017_param.h"
#include "auo_h120bln017_lcd_ctrl.h"

/* use FW_TEST definition when you test CAL on firmware */
/* #define FW_TEST */
#ifdef FW_TEST
#include "../dsim_fw.h"
#include "mipi_display.h"
#else
#include "../dsim.h"
#include <video/mipi_display.h>
#endif

#define ID		0

static int sunlightmode = 0;

void auo_h120bln017_lcd_init(struct decon_lcd * lcd)
{
	/* cmd_set_1 */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_1,
		ARRAY_SIZE(cmd_set_1)) == -1)
		dsim_err("********** failed to send cmd_set_1.\n");

	/* cmd_set_2 */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_2,
		ARRAY_SIZE(cmd_set_2)) == -1)
		dsim_err("********** failed to send cmd_set_2.\n");

	/* cmd_set_3 */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_3,
		ARRAY_SIZE(cmd_set_3)) == -1)
		dsim_err("********** failed to send cmd_set_3.\n");

	/* cmd_set_4 */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_4,
		ARRAY_SIZE(cmd_set_4)) == -1)
		dsim_err("********** failed to send cmd_set_4.\n");

	/* cmd_set_5 */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_5,
		ARRAY_SIZE(cmd_set_5)) == -1)
		dsim_err("********** failed to send cmd_set_5.\n");

	/* cmd_set_6 */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x12, 0);

	/* cmd_set_7 */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_7,
		ARRAY_SIZE(cmd_set_7)) == -1)
		dsim_err("********** failed to send cmd_set_7.\n");

	/* cmd_set_8 */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_8,
		ARRAY_SIZE(cmd_set_8)) == -1)
		dsim_err("********** failed to send cmd_set_8.\n");

	/* cmd_set_9 */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_9,
		ARRAY_SIZE(cmd_set_9)) == -1)
		dsim_err("********** failed to send cmd_set_9.\n");

	/* sleep out */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x11, 0);

	/* 150ms delay */
	usleep_range(150000, 160000);

	/* display on */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x29, 0);
}

void auo_h120bln017_lcd_enable(void)
{
}

void auo_h120bln017_lcd_disable(void)
{
	/* display off */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x28, 0);

	/* sleep in */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x10, 0);

	/* 120ms delay */
	usleep_range(120000, 130000);
}

void auo_h120bln017_lcd_brightness_set(int brightness)
{
	unsigned char reg_brightness_set[2] = "";

	reg_brightness_set[0] = 0x51;
	reg_brightness_set[1] = brightness;

	/* brightness_set */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) reg_brightness_set,
		ARRAY_SIZE(reg_brightness_set)) == -1)
		dsim_err("failed to brightness_set.\n");

	if (brightness == 255) {
		auo_h120bln017_lcd_highbrightness_mode(1);
		sunlightmode = 1;
	} else {
		if (sunlightmode == 1) {
			auo_h120bln017_lcd_highbrightness_mode(0);
			sunlightmode = 0;
		}
	}
}

void auo_h120bln017_lcd_idle_mode(int on)
{
	if (on) {
		dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			0x39, 0x00);
	} else {
		dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			0x38, 0x00);
	}
}

void auo_h120bln017_lcd_highbrightness_mode(int on)
{
	unsigned char reg_hbm_set[2] = "";

	if (on) {
		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x00;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_on_1.\n");

		reg_hbm_set[0] = 0xB0;
		reg_hbm_set[1] = 0x06;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_on_2.\n");

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x01;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_on_3.\n");

		reg_hbm_set[0] = 0x25;
		reg_hbm_set[1] = 0x83;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_on_4.\n");

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x00;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_on_5.\n");
	} else {
		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x00;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_off_1.\n");

		reg_hbm_set[0] = 0xB0;
		reg_hbm_set[1] = 0x04;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_off_2.\n");

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x01;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_off_3.\n");

		reg_hbm_set[0] = 0x25;
		reg_hbm_set[1] = 0x03;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_off_4.\n");

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x00;
		while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) == -1)
			dsim_err("********** failed to send cmd_hbm_off_5.\n");
	}
}
