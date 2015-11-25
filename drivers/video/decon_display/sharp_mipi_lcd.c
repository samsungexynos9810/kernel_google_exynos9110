/* linux/drivers/video/decon_drivers/sharp_mipi_lcd.c */

#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <video/mipi_display.h>
#include "decon_mipi_dsi.h"
#include "decon_display_driver.h"

#include "sharp_mipi_lcd_commands.h"

struct decon_lcd sharp_mipi_lcd_info = {
        .mode = COMMAND_MODE,

        .vfp = 1,
        .vbp = 1,
        .hfp = 1,
        .hbp = 1,

        .vsa = 1,
        .hsa = 1,

        .xres = 320,
        .yres = 300,

        .width = 34,
        .height = 32,

        /* Mhz */
        .hs_clk = 168,
        .esc_clk = 7,

        .fps = 58,
};

struct decon_lcd * decon_get_lcd_info()
{
        return &sharp_mipi_lcd_info;
}

static void sharp_mipi_select_page(struct mipi_dsim_device *dsim, uint8_t page)
{
	int ret;

	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			0xFF, page);
	if (ret)
		dev_err(dsim->dev, "failed to select page 0x%02x: %d\n", page, ret);
	udelay(300);
}

static int sharp_get_display_id1(struct mipi_dsim_device *dsim)
{
	uint8_t read_data;
	int ret;

	ret = s5p_mipi_dsi_rd_data(dsim, MIPI_DSI_DCS_READ, 0xA1, 0x01, &read_data);
	if (ret) {
		dev_err(dsim->dev, "failed to read display id1: %d\n", ret);
		return -1;
	}

	return read_data;
}

static int sharp_get_display_id2(struct mipi_dsim_device *dsim)
{
	uint8_t read_data;
	int ret;

	ret = s5p_mipi_dsi_rd_data(dsim, MIPI_DSI_DCS_READ, 0xC5, 0x01, &read_data);
	if (ret) {
		dev_err(dsim->dev, "failed to read display id2: %d\n", ret);
		return -1;
	}

	return read_data;
}

static void sharp_init_commands(struct mipi_dsim_device *dsim, int id1)
{
	int i, ret, id2;

	sharp_mipi_select_page(dsim, 0x24);
	id2 = sharp_get_display_id2(dsim);
	for (i = 0; i < ARRAY_SIZE(SHARP_INIT_CMDS1); i++) {
		ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				SHARP_INIT_CMDS1[i][0], SHARP_INIT_CMDS1[i][1]);
		if (ret)
			dev_err(dsim->dev, "failed to write CMDS1[%d]: %d\n", i, ret);
	}

	sharp_mipi_select_page(dsim, 0x20);
	if (id1 == 0x01 || id2 == 0xA1) {
		ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				0xFB, 0x01);
		if (ret)
			dev_err(dsim->dev, "failed to write extra CMDS2: %d\n", ret);
	}
	for (i = 0; i < ARRAY_SIZE(SHARP_INIT_CMDS2); i++) {
		ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				SHARP_INIT_CMDS2[i][0], SHARP_INIT_CMDS2[i][1]);
		if (ret)
			dev_err(dsim->dev, "failed to write CMDS2[%d]: %d\n", i, ret);
	}

	sharp_mipi_select_page(dsim, 0x21);
	if (id1 == 0x01 || id2 == 0xA1) {
		ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				0xFB, 0x01);
		if (ret)
			dev_err(dsim->dev, "failed to write extra CMDS3: %d\n", ret);
	}
	for (i = 0; i < ARRAY_SIZE(SHARP_INIT_CMDS3); i++) {
		ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				SHARP_INIT_CMDS3[i][0], SHARP_INIT_CMDS3[i][1]);
		if (ret)
			dev_err(dsim->dev, "failed to write CMDS3[%d]: %d\n", i, ret);
	}

	sharp_mipi_select_page(dsim, 0xE0);
	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			0x42, 0x21);
	if (ret)
		dev_err(dsim->dev, "failed to write CMDS4: %d\n", ret);
}

static void sharp_power_on_sequence(struct mipi_dsim_device *dsim)
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


	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_01[0], PWRON_CMD_01[1]);
	if (ret)
		dev_err(dsim->dev, "failed to write CMD01: %d\n", ret);

	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_02[0], PWRON_CMD_02[1]);
	if (ret)
		dev_err(dsim->dev, "failed to write CMD02: %d\n", ret);

	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_03[0], PWRON_CMD_03[1]);
	if (ret)
		dev_err(dsim->dev, "failed to write CMD03: %d\n", ret);

	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_04[0], PWRON_CMD_04[1]);
	if (ret)
		dev_err(dsim->dev, "failed to write CMD04: %d\n", ret);

	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			(unsigned int)PWRON_CMD_05, ARRAY_SIZE(PWRON_CMD_05));
	if (ret)
		dev_err(dsim->dev, "failed to write CMD05: %d\n", ret);

	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_06[0], PWRON_CMD_06[1]);
	if (ret)
		dev_err(dsim->dev, "failed to write CMD06: %d\n", ret);

	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_07[0], PWRON_CMD_07[1]);
	if (ret)
		dev_err(dsim->dev, "failed to write CMD07: %d\n", ret);
	msleep(130);

	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_08[0], PWRON_CMD_08[1]);
	if (ret)
		dev_err(dsim->dev, "failed to write CMD08: %d\n", ret);
	msleep(50);
}

static void sharp_power_sequence_off(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_SET_DISPLAY_OFF, 0x00);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_ENTER_SLEEP_MODE, 0x00);
	msleep(110);
}

static void init_lcd(struct mipi_dsim_device *dsim)
{
	int ret;

	sharp_mipi_select_page(dsim, 0x10);
	ret = sharp_get_display_id1(dsim);
	/* if device is older than PVT */
	if (ret != 0x02) {
		sharp_init_commands(dsim, ret);
		sharp_mipi_select_page(dsim, 0x10);
	}
}

static int sharp_mipi_lcd_suspend(struct mipi_dsim_device *dsim)
{
	s5p_mipi_lp_enable(dsim);
	sharp_power_sequence_off(dsim);
	s5p_mipi_lp_disable(dsim);

	return 1;
}

static int sharp_mipi_lcd_displayon(struct mipi_dsim_device *dsim)
{
	s5p_mipi_lp_enable(dsim);
	init_lcd(dsim);
	sharp_power_on_sequence(dsim);
	s5p_mipi_lp_disable(dsim);

	return 1;
}

static int sharp_mipi_lcd_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

int sharp_mipi_lcd_probe(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver sharp_mipi_lcd_driver = {
	.probe   =  sharp_mipi_lcd_probe,
	.suspend =  sharp_mipi_lcd_suspend,
	.displayon = sharp_mipi_lcd_displayon,
	.resume = sharp_mipi_lcd_resume,
};
