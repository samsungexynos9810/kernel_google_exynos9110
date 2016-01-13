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
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include "../../misc/MSensorsDrv.h"

#define DRIVER_NAME	"bd82103"

#define BRIGHTNESS_MAX	0xff
#define BRIGHTNESS_INIT 0x80 /* this value should equal to uboot setting */

#define T_ACC		10   /* intial waiting time [ms] */
#define T_LAT		10   /* waiting time after setting [ms] */

struct bd82103_chip_data {
	struct device *dev;
	struct backlight_device *bd;
#ifndef CONFIG_BACKLIGHT_SUBCPU
	spinlock_t slock;
#endif

	int gpio;
	int bl_intensity;
	unsigned char flg_backlight_0;
};

static int initialize_gpio(struct bd82103_chip_data *pchip)
{
	int err;

	if(gpio_is_valid(pchip->gpio)){
		err = gpio_request(pchip->gpio, "bd82103_ctrl");
		if (err)
			dev_err(pchip->dev, "gpio request failed");
#ifdef CONFIG_BACKLIGHT_SUBCPU
		err = gpio_direction_input(pchip->gpio);
#else
		err = gpio_direction_output(pchip->gpio, 1);
#endif
		if (err)
			dev_err(pchip->dev, "set direction of gpio failed");
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

static void ctrl_brightness(struct bd82103_chip_data *pchip, int intensity)
{
	unsigned char pulse_num;
#ifndef CONFIG_BACKLIGHT_SUBCPU
	unsigned long flags;
	int i;
#endif

	pulse_num = get_pulse_num(intensity);
	if (get_pulse_num(pchip->bl_intensity) == pulse_num)
		return;

#ifdef CONFIG_BACKLIGHT_SUBCPU
	if (intensity > 0)
		SUB_LCDBrightnessSet((intensity >> 4) + 1);
	else
		SUB_LCDBrightnessSet(0);
#else
	if (intensity == 0) {
		gpio_set_value_cansleep(pchip->gpio, 0);
	} else {
		if (pchip->bl_intensity == 0) {
			gpio_set_value_cansleep(pchip->gpio, 1);
			msleep(T_ACC);
		}
		spin_lock_irqsave(&pchip->slock, flags);
		for (i = 0; i < pulse_num;  i++) {
			gpio_set_value(pchip->gpio, 0);
			gpio_set_value(pchip->gpio, 1);
		}
		spin_unlock_irqrestore(&pchip->slock, flags);
	}
	msleep(T_LAT);
#endif
	pchip->bl_intensity = intensity;
}

static int bd82103_update_status(struct backlight_device *bd)
{
	struct bd82103_chip_data *pchip = bl_get_data(bd);
	int intensity = bd->props.brightness;

	/* detect backlight is 0 for segmented display*/
	if (bd->props.power == FB_BLANK_UNBLANK) {
		if (intensity == 0 && pchip->flg_backlight_0 != 1) {
			pchip->flg_backlight_0 = 1;
			Msensors_set_backlight_zero_flag(1);
		} else if (intensity != 0 && pchip->flg_backlight_0 != 0) {
			pchip->flg_backlight_0 = 0;
			Msensors_set_backlight_zero_flag(0);
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

	ctrl_brightness(pchip, intensity);

	return 0;
}

static int bd82103_get_brightness(struct backlight_device *bd)
{
	struct bd82103_chip_data *pchip = bl_get_data(bd);

	return pchip->bl_intensity;
}

static const struct backlight_ops bd82103_bd_ops = {
	.get_brightness = bd82103_get_brightness,
	.update_status  = bd82103_update_status,
};

static int bd82103_probe(struct platform_device *pdev)
{
	struct bd82103_chip_data *pchip = NULL;
	struct backlight_properties props;

	pchip = devm_kzalloc(&pdev->dev, sizeof(struct bd82103_chip_data), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	pchip->dev = &pdev->dev;
	pchip->gpio = of_get_gpio(pdev->dev.of_node, 0);
#ifndef CONFIG_BACKLIGHT_SUBCPU
	spin_lock_init(&pchip->slock);
#endif

	// initialize
	props.brightness = BRIGHTNESS_INIT;
	props.max_brightness = BRIGHTNESS_MAX;
	props.power = FB_BLANK_UNBLANK;
	props.type = BACKLIGHT_RAW;

	pchip->bd = backlight_device_register(DRIVER_NAME, pchip->dev, pchip, &bd82103_bd_ops, &props);
	if (IS_ERR(pchip->bd))
		return PTR_ERR(pchip->bd);
	initialize_gpio(pchip);
	pchip->bl_intensity = 0;
#ifndef CONFIG_BACKLIGHT_SUBCPU
	ctrl_brightness(pchip, BRIGHTNESS_INIT);
#endif
	pchip->flg_backlight_0 = 0xFF;

	return 0;
}

static int bd82103_remove(struct platform_device *pdev)
{
	struct bd82103_chip_data *pchip = platform_get_drvdata(pdev);

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
