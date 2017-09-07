/*
 *  subcpu_vibrator.c
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include "../../staging/android/timed_output.h"
#include <linux/wakelock.h>
#include <linux/err.h>
#include <linux/of_gpio.h>

#include "MSensorsDrv.h"

static struct timed_output_dev tdev;

static int vibrator_get_time(struct timed_output_dev *_dev)
{
	return 0;
}

static void vibrator_enable(struct timed_output_dev *_dev, int timeout)
{
//	printk("[VIB] time = %dms\n", timeout);
	SUB_VibratorSet(timeout);
}


static int vibrator_probe(struct platform_device *pdev)
{
	int ret;

	printk(KERN_WARNING "%s: %s registering\n", __func__, pdev->name);

	/* register with timed output class */
	tdev.name = "vibrator";
	tdev.get_time = vibrator_get_time;
	tdev.enable = vibrator_enable;
	ret = timed_output_dev_register(&tdev);
	if (ret < 0) {
		pr_err("timed output register failed %d\n", ret);
	}
	return ret;
}

static const struct of_device_id vibrator_match[] = {
	{ .compatible = "casio,sub-vibrator", },
	{ },
};

static struct platform_driver vibrator_driver = {
	.probe	= vibrator_probe,
	.driver = {
		.name	= "vibrator",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(vibrator_match),
	},
};

static int __init vibrator_init(void)
{
	printk(KERN_WARNING "%s: %s\n", __func__, vibrator_driver.driver.name);
	return platform_driver_register(&vibrator_driver);
}

module_init(vibrator_init);

MODULE_DESCRIPTION("subcpu vibrator");
MODULE_LICENSE("GPL");
