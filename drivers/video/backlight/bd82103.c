/*
 * Driver for bd82103
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/delay.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "../../misc/MSensorsDrv.h"

#define DRIVER_NAME	"bd82103"

#define BRIGHTNESS_MAX	0xff
#define BRIGHTNESS_INIT 0x80

#define T_ACC		1    //intial waiting time [ms]
#define T_HI		50   //pulse length of hi  [ns]
#define T_LOW		300  //pulse length of low [ns]
#define T_LAT		1    //waiting time after setting [ms]

struct bd82103_chip_data {
	struct device *dev;
	struct backlight_device *bd;

	int gpio;
	int bl_intensity;
	unsigned char flg_theater_mode;
};
static struct bd82103_chip_data g_pchip;

static int initialize_gpio(int gpio)
{
	int err;

	if(gpio_is_valid(gpio)){
		err = gpio_request(gpio, "bd82103_ctrl");
		if (err)
			printk("%s : reset gpio request failed", __func__);
#ifdef CONFIG_BACKLIGHT_SUBCPU
		err = gpio_direction_input(gpio);
		if (err)
			printk("%s : set_direction for reset gpio failed", __func__);
#else
		err = gpio_direction_output(gpio, 1);
		if (err)
			printk("%s : set_direction for reset gpio failed", __func__);
#endif
	}
	return 0;
}

static int get_pulse_num(int intensity)
{
	int pulse_num;

	if (intensity == 0){
		pulse_num = 0;
	} else {
#ifdef CONFIG_LCD_MIPI_JDI
		// current must be less than 10mA for safety of device
		pulse_num = 16 - (intensity * 12 - 1) / BRIGHTNESS_MAX;
#else
		pulse_num = 16 - (intensity * 16 - 1) / BRIGHTNESS_MAX;
#endif
	}
	return pulse_num;
}

static int ctrl_brightness(int pre_intensity, int intensity, int gpio)
{
	unsigned char pulse_num;
#ifndef CONFIG_BACKLIGHT_SUBCPU
	int i;
#endif

	pulse_num = get_pulse_num(intensity);

#ifdef CONFIG_BACKLIGHT_SUBCPU
	return SUB_LCDBrightnessSet(pulse_num);
#else
	if (intensity == 0) {
		gpio_set_value_cansleep(gpio, 0);
	} else {
		if (pre_intensity == 0) {
			gpio_set_value_cansleep(gpio, 1);
			msleep(T_ACC);
		}
		for (i = 0; i < pulse_num;  i++) {
			gpio_set_value_cansleep(gpio, 0);
			ndelay(T_LOW);
			gpio_set_value_cansleep(gpio, 1);
			ndelay(T_HI);
		}
	}
	msleep(T_LAT);

	return 0;
#endif
}

void on_off_light_for_pnlcd_demo(uint8_t sw)
{
	struct bd82103_chip_data *pchip = &g_pchip;
	int intensity = pchip->bd->props.brightness;

	if (sw) {
		ctrl_brightness(pchip->bl_intensity, intensity, pchip->gpio);
		pchip->bl_intensity = intensity;
	} else {
		ctrl_brightness(pchip->bl_intensity, 0, pchip->gpio);
		pchip->bl_intensity = 0;
	}
}

static int bd82103_update_status(struct backlight_device *bd)
{
	struct bd82103_chip_data *pchip = &g_pchip;
	int intensity = bd->props.brightness;

	/* detect theater mode */
	if (bd->props.power == FB_BLANK_UNBLANK) {
		if (intensity == 0 && pchip->flg_theater_mode != 1) {
			pchip->flg_theater_mode = 1;
			SUB_IsTheaterMode(pchip->flg_theater_mode);
		} else if (intensity != 0 && pchip->flg_theater_mode == 1) {
			pchip->flg_theater_mode = 0;
			SUB_IsTheaterMode(pchip->flg_theater_mode);
		}
	}
#ifndef CONFIG_FB_AMBIENT_SUPPORT
	/* for ambient debugging */
	if (bd->props.power != FB_BLANK_UNBLANK ||
	    bd->props.state & BL_CORE_FBBLANK ||
	    bd->props.state & BL_CORE_SUSPENDED ||
	    bd->props.power == FB_BLANK_POWERDOWN)
		intensity = 0;
#endif

#ifndef CONFIG_BACKLIGHT_SUBCPU
	// return from black window
	if (pchip->bl_intensity==0 && intensity != 0){
		gpio_set_value_cansleep(pchip->gpio, 1);
		msleep(T_ACC);
	}
#endif
	ctrl_brightness(pchip->bl_intensity, intensity, pchip->gpio);
	pchip->bl_intensity = intensity;

	return 0;
}

static int bd82103_get_brightness(struct backlight_device *bd)
{
	struct bd82103_chip_data *pchip = &g_pchip;

	return pchip->bl_intensity;
}

static const struct backlight_ops bd82103_bd_ops = {
	.get_brightness = bd82103_get_brightness,
	.update_status  = bd82103_update_status,
};

static int bd82103_probe(struct platform_device *pdev)
{
	struct bd82103_chip_data *pchip = &g_pchip;
	struct backlight_properties props;

	pchip->dev = &pdev->dev;
	pchip->gpio = of_get_gpio(pdev->dev.of_node, 0);

	// initialize
	props.brightness = BRIGHTNESS_INIT;
	props.max_brightness = BRIGHTNESS_MAX;
	props.power = FB_BLANK_UNBLANK;
	props.type = BACKLIGHT_RAW;
	pchip->bd = backlight_device_register(DRIVER_NAME, pchip->dev, pchip, &bd82103_bd_ops, &props);
	if (IS_ERR(pchip->bd))
		return PTR_ERR(pchip->bd);

	initialize_gpio(pchip->gpio);
	if (ctrl_brightness(0, BRIGHTNESS_INIT, pchip->gpio)) {
		gpio_free(pchip->gpio);
		backlight_device_unregister(pchip->bd);
		return -EPROBE_DEFER;
	}
	pchip->bl_intensity = BRIGHTNESS_INIT;
	pchip->flg_theater_mode = 0xFF;

	return 0;
}

static int bd82103_remove(struct platform_device *pdev)
{
	struct bd82103_chip_data *pchip = &g_pchip;

	pchip->bd->props.brightness = 0;
	backlight_update_status(pchip->bd);
	gpio_free(pchip->gpio);

	backlight_device_unregister(pchip->bd);

	return 0;
}

static const struct of_device_id bd82103_of_match[] = {
	{ .compatible = "rohm,bd82103", },
	{},
};
MODULE_DEVICE_TABLE(of, bd82103_of_match);

static struct platform_driver bd82103_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(bd82103_of_match)
	},
	.probe		= bd82103_probe,
	.remove		= bd82103_remove,
};
module_platform_driver(bd82103_driver);

MODULE_DESCRIPTION("Backlight driver for bd82103");
MODULE_AUTHOR("Itsuki Yamashita");
MODULE_LICENSE("GPL v2");
