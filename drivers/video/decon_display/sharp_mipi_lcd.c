/* linux/drivers/video/decon_drivers/sharp_mipi_lcd.c */

#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <video/mipi_display.h>
#include "decon_mipi_dsi.h"
#include "decon_display_driver.h"

#include "sharp_mipi_lcd.h"

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

static void sharp_power_sequence_on(struct mipi_dsim_device *dsim)
{
	int i;

	unsigned char PWRON_CMD_01[2]	= {0xFF, 0x24 };
	// SHARP_PWRON_CMDS1 (defined in the header file)
	// SHARP_PWRON_CMDS2 (defined in the header file)
	// SHARP_PWRON_CMDS3 (defined in the header file)
	unsigned char PWRON_CMD_02[2]	= {0x42, 0x21 };
	unsigned char PWRON_CMD_03[2]	= {0xFF, 0x10 };
	unsigned char PWRON_CMD_04[2]	= {0xB3, 0x00 };
	unsigned char PWRON_CMD_05[2]	= {0xBB, 0x10 };
	unsigned char PWRON_CMD_06[2]	= {0x3A, 0x06 }; // PIXEL FORMAT
	unsigned char PWRON_CMD_07[6]	= {0x3B, 0x00, 0x08, 0x08, 0x20, 0x20 };
	unsigned char PWRON_CMD_08[2]	= {0x35, 0x00 }; // TEAR ON
	unsigned char PWRON_CMD_09[2]	= {0x11, 0x00 }; // EXIT SLEEP MODE
	// WRITE PIXEL DATA (2C, 3C)
	unsigned char PWRON_CMD_10[2]	= {0x29, 0x00 }; // DISPLAY ON


	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_01[0], PWRON_CMD_01[1]) == -1)
		printk("MIPI DSI failed to write CMD01\n");
	udelay(100);

	for (i = 0; i < ARRAY_SIZE(SHARP_PWRON_CMDS1); i++) {
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				SHARP_PWRON_CMDS1[i][0], SHARP_PWRON_CMDS1[i][1])==-1)
			printk("MIPI DSI failed to write CMDS1");
	}
	udelay(100);

	for (i = 0; i < ARRAY_SIZE(SHARP_PWRON_CMDS2); i++) {
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				SHARP_PWRON_CMDS2[i][0], SHARP_PWRON_CMDS2[i][1])==-1)
			printk("MIPI DSI failed to write CMDS2");
	}
	udelay(100);

	for (i = 0; i < ARRAY_SIZE(SHARP_PWRON_CMDS3); i++) {
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				SHARP_PWRON_CMDS3[i][0], SHARP_PWRON_CMDS3[i][1])==-1)
			printk("MIPI DSI failed to write CMDS3");
	}
	udelay(100);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_02[0], PWRON_CMD_02[1]) == -1)
		printk("MIPI DSI failed to write CMD02\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_03[0], PWRON_CMD_03[1]) == -1)
		printk("MIPI DSI failed to write CMD03\n");
	udelay(100);
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_04[0], PWRON_CMD_04[1]) == -1)
		printk("MIPI DSI failed to write CMD04\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_05[0], PWRON_CMD_05[1]) == -1)
		printk("MIPI DSI failed to write CMD05\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_06[0], PWRON_CMD_06[1]) == -1)
		printk("MIPI DSI failed to write CMD06\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)PWRON_CMD_07, ARRAY_SIZE(PWRON_CMD_07)) == -1)
		printk("MIPI DSI failed to write CMD07\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PWRON_CMD_08[0], PWRON_CMD_08[1]) == -1)
		printk("MIPI DSI failed to write CMD08\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_09[0], PWRON_CMD_09[1]) == -1)
		printk("MIPI DSI failed to write CMD09\n");
	mdelay(120);
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			PWRON_CMD_10[0], PWRON_CMD_10[1]) == -1)
		printk("MIPI DSI failed to write CMD10\n");
}

static void sharp_power_sequence_off(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_SET_DISPLAY_OFF, 0x00);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_ENTER_SLEEP_MODE, 0x00);
	mdelay(100);
}

static int init_lcd(struct mipi_dsim_device *dsim)
{
	sharp_power_sequence_on(dsim);

	return 1;
}

static int sharp_mipi_lcd_suspend(struct mipi_dsim_device *dsim)
{
	sharp_power_sequence_off(dsim);

	return 1;
}

static int sharp_mipi_lcd_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);

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
