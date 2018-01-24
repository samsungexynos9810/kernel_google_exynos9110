/*
	auo_h120bln017_mipi_lcd.c
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>

#include "../dsim.h"
#include "auo_h120bln017_lcd_ctrl.h"
#include "decon_lcd.h"
#include "auo_h120bln017_param.h"
#include "../../../../../misc/casio/MSensorsDrv.h"

//#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 72

struct backlight_device *bd;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    auo_h120bln017_early_suspend;
#endif

static int auo_h120bln017_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int update_brightness(int brightness)
{
	auo_h120bln017_lcd_brightness_set(brightness);
	return 0;
}

static int auo_h120bln017_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

#ifdef CONFIG_BACKLIGHT_SUBCPU
	if (brightness > 0)
		SUB_LCDBrightnessSet((brightness >> 4) + 1);
	else
		SUB_LCDBrightnessSet(0);
#endif

	update_brightness(brightness);

	return 0;
}

static const struct backlight_ops auo_h120bln017_backlight_ops = {
	.get_brightness = auo_h120bln017_get_brightness,
	.update_status = auo_h120bln017_set_brightness,
};

static int auo_h120bln017_probe(struct dsim_device *dsim)
{
	printk(KERN_INFO "***** auo_h120bln017_probe \n");
	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &auo_h120bln017_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 0;
}

static int auo_h120bln017_displayon(struct dsim_device *dsim)
{
	printk(KERN_INFO "***** auo_h120bln017_displayon \n");
	auo_h120bln017_lcd_init(&dsim->lcd_info);
	return 0;
}

static int auo_h120bln017_suspend(struct dsim_device *dsim)
{
	printk(KERN_INFO "***** auo_h120bln017_suspend \n");
	auo_h120bln017_lcd_disable();
	return 0;
}

static int auo_h120bln017_resume(struct dsim_device *dsim)
{
	printk(KERN_INFO "***** auo_h120bln017_resume\n");
	//lcd_init(&dsim->lcd_info);
	return 0;
}

struct mipi_dsim_lcd_driver auo_h120bln017_mipi_lcd_driver = {
	.probe		= auo_h120bln017_probe,
	.displayon	= auo_h120bln017_displayon,
	.suspend	= auo_h120bln017_suspend,
	.resume		= auo_h120bln017_resume,
};
