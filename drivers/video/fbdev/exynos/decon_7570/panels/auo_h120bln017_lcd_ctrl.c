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

int auo_h120bln017_lcd_init(struct decon_lcd * lcd)
{
	/* cmd_set_1 */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_1,
		ARRAY_SIZE(cmd_set_1)) < 0)
		return -1;

	/* cmd_set_2 */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_2,
		ARRAY_SIZE(cmd_set_2)) < 0)
		return -1;

	/* cmd_set_3 */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_3,
		ARRAY_SIZE(cmd_set_3)) < 0)
		return -1;

	/* cmd_set_4 */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_4,
		ARRAY_SIZE(cmd_set_4)) < 0)
		return -1;

	/* cmd_set_5 */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_5,
		ARRAY_SIZE(cmd_set_5)) < 0)
		return -1;

	/* cmd_set_6 */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE, 0x12, 0) < 0)
		return -1;

	/* cmd_set_7 */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_7,
		ARRAY_SIZE(cmd_set_7)) < 0)
		return -1;

	/* cmd_set_8 */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_8,
		ARRAY_SIZE(cmd_set_8)) < 0)
		return -1;

	/* cmd_set_9 */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_9,
		ARRAY_SIZE(cmd_set_9)) < 0)
		return -1;

	/* sleep out */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE, 0x11, 0) < 0)
		return -1;

	/* 150ms delay */
	usleep_range(150000, 160000);

	/* display on */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE, 0x29, 0) < 0)
		return -1;

	return 0;
}

int auo_h120bln017_lcd_enable(void)
{
	return 0;
}

int auo_h120bln017_lcd_disable(void)
{
	/* display off */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE, 0x28, 0) < 0)
		return -1;

	/* sleep in */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE, 0x10, 0) < 0)
		return -1;

	/* 120ms delay */
	usleep_range(120000, 130000);

	return 0;
}

int auo_h120bln017_lcd_brightness_set(int brightness)
{
	unsigned char reg_brightness_set[2] = "";

	reg_brightness_set[0] = 0x51;
	reg_brightness_set[1] = brightness;
	//printk(KERN_INFO "---------- brightness:%d\n", brightness);

	/* brightness_set */
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) reg_brightness_set,
		ARRAY_SIZE(reg_brightness_set)) < 0) {
		return -1;
	}

	if (brightness == 255) {
		auo_h120bln017_lcd_highbrightness_mode(1);
		sunlightmode = 1;
	} else {
		if (sunlightmode == 1) {
			auo_h120bln017_lcd_highbrightness_mode(0);
			sunlightmode = 0;
		}
	}
	return 0;
}

int auo_h120bln017_lcd_idle_mode(int on)
{
	unsigned char reg_idle_mode_set[2] = "";

	if (on) {
		reg_idle_mode_set[0] = 0xFE;
		reg_idle_mode_set[1] = 0x01;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_idle_mode_set,
			ARRAY_SIZE(reg_idle_mode_set)) < 0)
			return -1;

		reg_idle_mode_set[0] = 0x30;
		reg_idle_mode_set[1] = 0x41;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_idle_mode_set,
			ARRAY_SIZE(reg_idle_mode_set)) < 0)
			return -1;

		reg_idle_mode_set[0] = 0xFE;
		reg_idle_mode_set[1] = 0x00;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_idle_mode_set,
			ARRAY_SIZE(reg_idle_mode_set)) < 0)
			return -1;

		if (dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE, 0x39, 0x00) < 0)
			return -1;
		msleep(100);
	} else {
		if (dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE, 0x38, 0x00) < 0)
			return -1;

		reg_idle_mode_set[0] = 0xFE;
		reg_idle_mode_set[1] = 0x01;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_idle_mode_set,
			ARRAY_SIZE(reg_idle_mode_set)) < 0)
			return -1;

		reg_idle_mode_set[0] = 0x30;
		reg_idle_mode_set[1] = 0x43;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_idle_mode_set,
			ARRAY_SIZE(reg_idle_mode_set)) < 0)
			return -1;

		reg_idle_mode_set[0] = 0xFE;
		reg_idle_mode_set[1] = 0x00;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_idle_mode_set,
			ARRAY_SIZE(reg_idle_mode_set)) < 0)
			return -1;
	}
	return 0;
}

int auo_h120bln017_lcd_highbrightness_mode(int on)
{
	unsigned char reg_hbm_set[2] = "";

	if (on) {
		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x01;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x29;
		reg_hbm_set[1] = 0x43;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x05;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x2A;
		reg_hbm_set[1] = 0x02;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x30;
		reg_hbm_set[1] = 0x33;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x01;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x11;
		reg_hbm_set[1] = 0x93;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x19;
		reg_hbm_set[1] = 0x44;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x00;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xB0;
		reg_hbm_set[1] = 0x06;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		usleep_range(30000, 31000);

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x01;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x29;
		reg_hbm_set[1] = 0x40;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x00;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;
	} else {
		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x01;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x29;
		reg_hbm_set[1] = 0x43;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x05;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x2A;
		reg_hbm_set[1] = 0x02;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x30;
		reg_hbm_set[1] = 0x41;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x01;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x11;
		reg_hbm_set[1] = 0x80;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x19;
		reg_hbm_set[1] = 0x22;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x00;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xB0;
		reg_hbm_set[1] = 0x04;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		usleep_range(30000, 31000);

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x01;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0x29;
		reg_hbm_set[1] = 0x40;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;

		reg_hbm_set[0] = 0xFE;
		reg_hbm_set[1] = 0x00;
		if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) reg_hbm_set,
			ARRAY_SIZE(reg_hbm_set)) < 0)
			return -1;
	}
	return 0;
}
