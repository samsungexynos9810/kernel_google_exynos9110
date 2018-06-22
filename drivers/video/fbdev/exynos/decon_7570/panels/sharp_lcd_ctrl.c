/*
	sharp_lcd_ctrl.c
*/

#include "sharp_lcd_param.h"
#include "sharp_lcd_ctrl.h"

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

static void sharp_mipi_select_page(uint8_t page)
{
	int ret;

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			0xFF, page);
	if (ret)
		printk(KERN_ERR "failed to select page 0x%02x: %d\n", page, ret);
	udelay(300);
}

#if 0
static int sharp_get_display_id1(void)
{
	uint8_t read_data;
	int ret;

	ret = dsim_rd_data(ID, MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE, 0xA1, 0x01, &read_data);
	if (ret) {
		printk(KERN_ERR "failed to read display id1: %d\n", ret);
		return -1;
	}

	return read_data;
}

static int sharp_get_display_id2(void)
{
	uint8_t read_data;
	int ret;

	ret = dsim_rd_data(ID, MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE, 0xC5, 0x01, &read_data);
	if (ret) {
		printk(KERN_ERR "failed to read display id2: %d\n", ret);
		return -1;
	}

	return read_data;
}

static void sharp_init_commands(int id1)
{
	int i, ret, id2;

	sharp_mipi_select_page(0x24);
	id2 = sharp_get_display_id2();
	for (i = 0; i < ARRAY_SIZE(SHARP_INIT_CMDS1); i++) {
		ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
				SHARP_INIT_CMDS1[i][0], SHARP_INIT_CMDS1[i][1]);
		if (ret)
			printk(KERN_ERR "failed to write CMDS1[%d]: %d\n", i, ret);
	}

	sharp_mipi_select_page(0x20);
	if (id1 == 0x01 || id2 == 0xA1) {
		ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
				0xFB, 0x01);
		if (ret)
			printk(KERN_ERR "failed to write extra CMDS2: %d\n", ret);
	}
	for (i = 0; i < ARRAY_SIZE(SHARP_INIT_CMDS2); i++) {
		ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
				SHARP_INIT_CMDS2[i][0], SHARP_INIT_CMDS2[i][1]);
		if (ret)
			printk(KERN_ERR "failed to write CMDS2[%d]: %d\n", i, ret);
	}

	sharp_mipi_select_page(0x21);
	if (id1 == 0x01 || id2 == 0xA1) {
		ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
				0xFB, 0x01);
		if (ret)
			printk(KERN_ERR "failed to write extra CMDS3: %d\n", ret);
	}
	for (i = 0; i < ARRAY_SIZE(SHARP_INIT_CMDS3); i++) {
		ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
				SHARP_INIT_CMDS3[i][0], SHARP_INIT_CMDS3[i][1]);
		if (ret)
			printk(KERN_ERR "failed to write CMDS3[%d]: %d\n", i, ret);
	}

	sharp_mipi_select_page(0xE0);
	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			0x42, 0x21);
	if (ret)
		printk(KERN_ERR "failed to write CMDS4: %d\n", ret);
}
#endif

static void sharp_power_on_sequence(void)
{
	int ret;

	unsigned char PWRON_CMD_01[2]	= {0xFB, 0x01 };
	unsigned char PWRON_CMD_02[2]	= {0xB3, 0x00 };
	unsigned char PWRON_CMD_03[2]	= {0xBB, 0x10 };
	unsigned char PWRON_CMD_04[2]	= {0x3A, 0x06 }; /* PIXEL FORMAT */
	unsigned char PWRON_CMD_05[6]	= {0x3B, 0x00, 0x08, 0x08, 0x20, 0x20 };
	unsigned char PWRON_CMD_06[2]	= {0x35, 0x00 }; /* TEAR ON */
	unsigned char PWRON_CMD_07[2]	= {0x11, 0x00 }; /* EXIT SLEEP MODE */
	unsigned char PWRON_CMD_08[2]	= {0x29, 0x00 }; /* DISPLAY ON */


	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_01[0], PWRON_CMD_01[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD01: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_02[0], PWRON_CMD_02[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD02: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_03[0], PWRON_CMD_03[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD03: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_04[0], PWRON_CMD_04[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD04: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)PWRON_CMD_05, ARRAY_SIZE(PWRON_CMD_05));
	if (ret)
		printk(KERN_ERR "failed to write CMD05: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_06[0], PWRON_CMD_06[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD06: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_07[0], PWRON_CMD_07[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD07: %d\n", ret);
	msleep(130);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_08[0], PWRON_CMD_08[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD08: %d\n", ret);
	msleep(50);
}

static void sharp_power_sequence_off(void)
{
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_SET_DISPLAY_OFF, 0x00);
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_ENTER_SLEEP_MODE, 0x00);
	msleep(110);
}

static void init_lcd(void)
{
	//int ret;

	sharp_mipi_select_page(0x10);
	//ret = sharp_get_display_id1();
	/* if device is older than PVT */
	//if (ret != 0x02) {
	//	sharp_init_commands(ret);
	//	sharp_mipi_select_page(0x10);
	//}
}

#if 0
static int sharp_mipi_reload_params(void)
{
	int ret;

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
				MIPI_DCS_EXIT_SLEEP_MODE, 0x00);
	if (ret)
		printk(KERN_ERR "failed to write exit sleep command: %d\n", ret);

	usleep_range(17000, 18000);
	return 0;
}
#endif

int sharp_lcd_init(struct decon_lcd * lcd)
{
	init_lcd();
	sharp_power_on_sequence();

	return 0;
}

int sharp_lcd_enable(void)
{
	return 0;
}

int sharp_lcd_disable(void)
{
	sharp_power_sequence_off();

	return 0;
}
