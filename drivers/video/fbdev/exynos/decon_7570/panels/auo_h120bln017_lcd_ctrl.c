/*
	auo_h120bln017_lcd_ctrl.c
*/

#include "auo_h120bln017_param.h"
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

#define ID		0

void lcd_init(struct decon_lcd * lcd)
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
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) cmd_set_6,
		ARRAY_SIZE(cmd_set_6)) == -1)
		dsim_err("********** failed to send cmd_set_6.\n");

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

	/* sleep out */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x11, 0);

	/* 150ms delay */
	usleep_range(150000, 160000);

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
	usleep_range(120000, 130000);
}

void lcd_brightness_set(int brightness)
{
	unsigned char reg_brightness_set[2] = "";

	reg_brightness_set[0] = 0x51;
	reg_brightness_set[1] = brightness;

	/* brightness_set */
	while (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) reg_brightness_set,
		ARRAY_SIZE(reg_brightness_set)) == -1)
		dsim_err("failed to brightness_set.\n");
}
