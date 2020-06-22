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
	uint8_t cmd[2];

	cmd[0] = 0xFF;
	cmd[1] = page;
	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)cmd, ARRAY_SIZE(cmd));
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

#if defined(LCD_SAMPLE_PRODUCT)
/*
static int sharp_lcd_get_reg(uint8_t reg)
{
	uint8_t read_data;
	int ret;

	ret = dsim_rd_data(ID, MIPI_DSI_DCS_READ, reg, 1, &read_data);
	if (ret) {
		printk(KERN_ERR "# error sharp_lcd_get_reg: %d\n", ret);
		return -1;
	}
	printk(KERN_INFO "# 0x%02x, 0x%02x\n", reg, read_data);

	return read_data;
}
*/
static void sharp_lcd_sample_setting(void)
{
	int i, ret;
	uint8_t cmd[2];

	sharp_mipi_select_page(0x24);
	for (i=0; i<ARRAY_SIZE(SHARP_SAMPLE_CMDS1); i++) {
		cmd[0] = SHARP_SAMPLE_CMDS1[i][0];
		cmd[1] = SHARP_SAMPLE_CMDS1[i][1];
		ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)cmd, ARRAY_SIZE(cmd));
		if (ret)
			printk(KERN_ERR "failed to write CMDS1[%d]: %d\n", i, ret);
	}

	sharp_mipi_select_page(0xE0);
	for (i=0; i<ARRAY_SIZE(SHARP_SAMPLE_CMDS2); i++) {
		cmd[0] = SHARP_SAMPLE_CMDS2[i][0];
		cmd[1] = SHARP_SAMPLE_CMDS2[i][1];
		ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)cmd, ARRAY_SIZE(cmd));
		if (ret)
			printk(KERN_ERR "failed to write CMDS2[%d]: %d\n", i, ret);
	}

	sharp_mipi_select_page(0x20);
	for (i=0; i<ARRAY_SIZE(SHARP_SAMPLE_CMDS3); i++) {
		cmd[0] = SHARP_SAMPLE_CMDS3[i][0];
		cmd[1] = SHARP_SAMPLE_CMDS3[i][1];
		ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)cmd, ARRAY_SIZE(cmd));
		if (ret)
			printk(KERN_ERR "failed to write CMDS3[%d]: %d\n", i, ret);
	}

	sharp_mipi_select_page(0x21);
	for (i=0; i<ARRAY_SIZE(SHARP_SAMPLE_CMDS4); i++) {
		cmd[0] = SHARP_SAMPLE_CMDS4[i][0];
		cmd[1] = SHARP_SAMPLE_CMDS4[i][1];
		ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)cmd, ARRAY_SIZE(cmd));
		if (ret)
			printk(KERN_ERR "failed to write CMDS4[%d]: %d\n", i, ret);
	}
}
#endif

static void sharp_power_on_sequence(void)
{
	int ret;

#if defined(LCD_SAMPLE_PRODUCT)
	unsigned char PWRON_CMD_01[2]	= {0xFB, 0x01};
	unsigned char PWRON_CMD_02[2]	= {0xB3, 0x00};
	unsigned char PWRON_CMD_03[2]	= {0xBB, 0x10};
	unsigned char PWRON_CMD_04[2]	= {0x3A, 0x06};
	unsigned char PWRON_CMD_05[2]	= {0xC0, 0x00};
	unsigned char PWRON_CMD_06[2]	= {0x36, 0x02};
	unsigned char PWRON_CMD_07[6]	= {0x3B, 0x00, 0x08, 0x08, 0x20, 0x20};
	unsigned char PWRON_CMD_08[2]	= {0x35, 0x00};
	unsigned char PWRON_CMD_09[2]	= {0x11, 0x00};
	unsigned char PWRON_CMD_10[2]	= {0x29, 0x00};

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)PWRON_CMD_01, ARRAY_SIZE(PWRON_CMD_01));
	if (ret)
		printk(KERN_ERR "failed to write CMD01: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)PWRON_CMD_02, ARRAY_SIZE(PWRON_CMD_02));
	if (ret)
		printk(KERN_ERR "failed to write CMD02: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)PWRON_CMD_03, ARRAY_SIZE(PWRON_CMD_03));
	if (ret)
		printk(KERN_ERR "failed to write CMD03: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)PWRON_CMD_04, ARRAY_SIZE(PWRON_CMD_04));
	if (ret)
		printk(KERN_ERR "failed to write CMD04: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)PWRON_CMD_05, ARRAY_SIZE(PWRON_CMD_05));
	if (ret)
		printk(KERN_ERR "failed to write CMD05: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)PWRON_CMD_06, ARRAY_SIZE(PWRON_CMD_06));
	if (ret)
		printk(KERN_ERR "failed to write CMD06: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)PWRON_CMD_07, ARRAY_SIZE(PWRON_CMD_07));
	if (ret)
		printk(KERN_ERR "failed to write CMD07: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)PWRON_CMD_08, ARRAY_SIZE(PWRON_CMD_08));
	if (ret)
		printk(KERN_ERR "failed to write CMD08: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_09[0], PWRON_CMD_09[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD09: %d\n", ret);
	msleep(130);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_10[0], PWRON_CMD_10[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD10: %d\n", ret);
	msleep(50);
#else
	unsigned char PWRON_CMD_01[2]	= {0x11, 0x00};
	unsigned char PWRON_CMD_02[2]	= {0x29, 0x00};

	sharp_mipi_select_page(0x10);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_01[0], PWRON_CMD_01[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD01: %d\n", ret);

	msleep(130);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_02[0], PWRON_CMD_02[1]);
	if (ret)
		printk(KERN_ERR "failed to write CMD02: %d\n", ret);

	msleep(50);
#endif
}

static void sharp_power_sequence_off(void)
{
	int ret;
	unsigned char cmd_set_1[2] = {0x30, 0x30};
	unsigned char cmd_set_2[2] = {0x0B, 0x80};
	unsigned char cmd_set_3[2] = {0x0C, 0x80};

	sharp_mipi_select_page(0x10);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_SET_DISPLAY_OFF, 0x00);
	if (ret)
		printk(KERN_ERR "MIPI_DCS_SET_DISPLAY_OFF: %d\n", ret);

	msleep(35);

	/* prevent floating vdd_31 && lcd flicker */
	sharp_mipi_select_page(0x20);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)cmd_set_1, ARRAY_SIZE(cmd_set_1));
	if (ret)
		printk(KERN_ERR "failed to write CMD01: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)cmd_set_2, ARRAY_SIZE(cmd_set_2));
	if (ret)
		printk(KERN_ERR "failed to write CMD02: %d\n", ret);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)cmd_set_3, ARRAY_SIZE(cmd_set_3));
	if (ret)
		printk(KERN_ERR "failed to write CMD03: %d\n", ret);

	msleep(65);
}

static void init_lcd(void)
{
#if defined(LCD_SAMPLE_PRODUCT)
	sharp_lcd_sample_setting();
	sharp_mipi_select_page(0x10);
#endif
}

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

int decon_get_shutdown_state(void);

int sharp_lcd_disable(void)
{
	if (decon_get_shutdown_state())
		msleep(1500);
	else
		msleep(200);
	sharp_power_sequence_off();

	return 0;
}

int sharp_lcd_idle_mode(int on)
{
	int i, ret;
	uint8_t cmd[2];

	if (on) {
		msleep(100);
	} else {
		sharp_mipi_select_page(0x20);
		for (i=0; i<ARRAY_SIZE(SHARP_RELOAD_PARAM_1); i++) {
			cmd[0] = SHARP_RELOAD_PARAM_1[i][0];
			cmd[1] = SHARP_RELOAD_PARAM_1[i][1];
			ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)cmd, ARRAY_SIZE(cmd));
			if (ret)
				printk(KERN_ERR "failed to write SHARP_RELOAD_PARAM_1[%d]: %d\n", i, ret);
		}

		sharp_mipi_select_page(0x21);
		for (i=0; i<ARRAY_SIZE(SHARP_RELOAD_PARAM_2); i++) {
			cmd[0] = SHARP_RELOAD_PARAM_2[i][0];
			cmd[1] = SHARP_RELOAD_PARAM_2[i][1];
			ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)cmd, ARRAY_SIZE(cmd));
			if (ret)
				printk(KERN_ERR "failed to write SHARP_RELOAD_PARAM_2[%d]: %d\n", i, ret);
		}
	}

	return ret;
}
