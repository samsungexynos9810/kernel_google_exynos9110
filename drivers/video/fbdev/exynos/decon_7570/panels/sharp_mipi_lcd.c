/*
	sharp_mipi_lcd.c
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>

#include "../dsim.h"
#include "decon_lcd.h"
#include "sharp_lcd_ctrl.h"
#include "../../../../../misc/casio/MSensorsDrv.h"

#define ID 0
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 160

static struct backlight_device *bd;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    sharp_lcd_early_suspend;
#endif

static int sharp_lcd_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

#ifdef CONFIG_BACKLIGHT_SUBCPU
static int bl_force_off;
void sharp_lcd_notify_seglcd(int seglcd_on)
{
	bl_force_off = 0;
}

static int always_segment_mode;
void sharp_lcd_notify_always_segment(int mode)
{
	always_segment_mode = mode;
}

static int ambient_in_always_segment_mode;
void sharp_lcd_notify_ambient(void)
{
	if (!always_segment_mode)
		return;

	ambient_in_always_segment_mode = 1;
	backlight_update_status(bd);
	SUB_LCDForceOnOffSet(1);
	ambient_in_always_segment_mode = 0;
}
#endif

static int sharp_lcd_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

#ifdef CONFIG_BACKLIGHT_SUBCPU
	if (brightness == 0) {
		if (!(bd->props.state & BL_CORE_FBBLANK))
			brightness = 1;
	}

	if (bl_force_off || ambient_in_always_segment_mode)
		brightness = 0;

	if (brightness > 0)
		SUB_LCDBrightnessSet((brightness >> 4) + 1);
	else
		SUB_LCDBrightnessSet(0);
#endif

	return 0;
}

static const struct backlight_ops sharp_lcd_backlight_ops = {
	.get_brightness = sharp_lcd_get_brightness,
	.update_status = sharp_lcd_set_brightness,
};

static int sharp_mipi_lcd_probe(struct dsim_device *dsim)
{
	printk(KERN_INFO "***** sharp_mipi_lcd_probe \n");
	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &sharp_lcd_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;
	bd->props.state = BL_CORE_FBBLANK;

	return 0;
}

static int sharp_mipi_lcd_displayon(struct dsim_device *dsim)
{
	printk(KERN_INFO "***** sharp_mipi_lcd_displayon \n");
	sharp_lcd_init(&dsim->lcd_info);
	return 0;
}

static int sharp_mipi_lcd_suspend(struct dsim_device *dsim)
{
	printk(KERN_INFO "***** sharp_mipi_lcd_suspend \n");
	sharp_lcd_disable();
	return 0;
}

static int sharp_mipi_lcd_resume(struct dsim_device *dsim)
{
	return 0;
}

static int sharp_mipi_lcd_enteridle(struct dsim_device *dsim)
{
	pr_debug(KERN_INFO "***** sharp_mipi_lcd_enteridle \n");
	sharp_lcd_idle_mode(1);
	return 0;
}

static int sharp_mipi_lcd_exitidle(struct dsim_device *dsim)
{
	pr_debug(KERN_INFO "***** sharp_mipi_lcd_exitidle \n");
	sharp_lcd_idle_mode(0);
	return 0;
}

struct mipi_dsim_lcd_driver sharp_mipi_lcd_driver = {
	.probe			= sharp_mipi_lcd_probe,
	.displayon		= sharp_mipi_lcd_displayon,
	.suspend		= sharp_mipi_lcd_suspend,
	.resume			= sharp_mipi_lcd_resume,
	.enteridle		= sharp_mipi_lcd_enteridle,
	.exitidle		= sharp_mipi_lcd_exitidle,
};
