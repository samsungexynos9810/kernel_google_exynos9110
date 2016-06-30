/*
 * exynos-usb-switch.c - USB switch driver for Exynos
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 * Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_data/exynos-usb-switch.h>
#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/usb/hcd.h>
#include <linux/regulator/consumer.h>

#include "exynos-usb-switch.h"

#define DRIVER_DESC "Exynos USB Switch Driver"
#define SWITCH_WAIT_TIME	500
#define WAIT_TIMES		10

static const char switch_name[] = "exynos_usb_switch";
static struct exynos_usb_switch *our_switch;

const char *exynos_usbswitch_mode_string(unsigned long mode)
{
	if (!mode)
		return "IDLE";
	else if (test_bit(USB_HOST_ATTACHED, &mode))
		return "USB_HOST_ATTACHED";
	else if (test_bit(USB_DEVICE_ATTACHED, &mode))
		return "USB_DEVICE_ATTACHED";
	else
		/* something wrong */
		return "undefined";
}

static int is_device_detect(struct exynos_usb_switch *usb_switch)
{
	if (!gpio_is_valid(usb_switch->gpio_device_detect))
		return 0;
	return gpio_get_value(usb_switch->gpio_device_detect);
}

struct usb_gadget *
__attribute__((weak)) s3c_udc_get_gadget(struct device *dev)
{
	return NULL;
}

static int exynos_change_usb_mode(struct exynos_usb_switch *usb_switch,
				enum usb_cable_status mode)
{
	struct usb_gadget *gadget;
	unsigned long cur_mode = usb_switch->connect;
	int ret = 0;

	if (test_bit(USB_DEVICE_ATTACHED, &cur_mode) ||
	    test_bit(USB_HOST_ATTACHED, &cur_mode)) {
		if (mode == USB_DEVICE_ATTACHED ||
			mode == USB_HOST_ATTACHED) {
			printk(KERN_DEBUG "Skip request %d, current %lu\n",
				mode, cur_mode);
			return -EPERM;
		}
	}

	if (!test_bit(USB_DEVICE_ATTACHED, &cur_mode) &&
			mode == USB_DEVICE_DETACHED) {
		printk(KERN_DEBUG "Skip request %d, current %lu\n",
			mode, cur_mode);
		return -EPERM;
	} else if (!test_bit(USB_HOST_ATTACHED, &cur_mode) &&
			mode == USB_HOST_DETACHED) {
		printk(KERN_DEBUG "Skip request %d, current %lu\n",
			mode, cur_mode);
		return -EPERM;
	}

	/*
	 * FIXME: Currently we get gadget every time usb mode is going
	 * to change. This prevents from problems related to UDC module
	 * removing and wrong probe order. S3C UDC driver should provide
	 * corresponding helper function. If S3C UDC is not available weak
	 * analog is used instead.
	 *
	 * Correct solution would be to implement binding similar to OTG's
	 * set_host() and set_peripheral().
	 */
	gadget = s3c_udc_get_gadget(usb_switch->s3c_udc_dev);

	switch (mode) {
	case USB_DEVICE_DETACHED:
		if (test_bit(USB_HOST_ATTACHED, &cur_mode)) {
			printk(KERN_ERR "Abnormal request %d, current %lu\n",
				mode, cur_mode);
			return -EPERM;
		}
		if (gadget && gadget->ops)
			usb_gadget_vbus_disconnect(gadget);
		clear_bit(USB_DEVICE_ATTACHED, &usb_switch->connect);
		break;
	case USB_DEVICE_ATTACHED:
		if (gadget && gadget->ops)
			usb_gadget_vbus_connect(gadget);
		set_bit(USB_DEVICE_ATTACHED, &usb_switch->connect);
		break;
	default:
		printk(KERN_ERR "Does not changed\n");
	}
	printk(KERN_ERR "usb cable = %d\n", mode);

	return ret;
}

static void exynos_usb_switch_worker(struct work_struct *work)
{
	struct exynos_usb_switch *usb_switch =
		container_of(work, struct exynos_usb_switch, switch_work);

	mutex_lock(&usb_switch->mutex);
	/* If already device detached or host_detected, */
	if (!is_device_detect(usb_switch))
		goto done;

	/* Check Device, VBUS PIN high active */
	exynos_change_usb_mode(usb_switch, USB_DEVICE_ATTACHED);
done:
	mutex_unlock(&usb_switch->mutex);
}

static irqreturn_t exynos_device_detect_thread(int irq, void *data)
{
	struct exynos_usb_switch *usb_switch = data;

	mutex_lock(&usb_switch->mutex);

	/* Debounce connect delay */
	msleep(20);

	if (is_device_detect(usb_switch)) {
		wake_lock(&usb_switch->wake_lock);
		queue_work(usb_switch->workqueue, &usb_switch->switch_work);
	} else {
		/* VBUS PIN low */
		exynos_change_usb_mode(usb_switch, USB_DEVICE_DETACHED);
		wake_unlock(&usb_switch->wake_lock);
	}

	mutex_unlock(&usb_switch->mutex);

	return IRQ_HANDLED;
}

static int exynos_usb_status_init(struct exynos_usb_switch *usb_switch)
{
	mutex_lock(&usb_switch->mutex);

	if (is_device_detect(usb_switch)) {
		queue_work(usb_switch->workqueue, &usb_switch->switch_work);
	}

	mutex_unlock(&usb_switch->mutex);

	return 0;
}

#ifdef CONFIG_PM
static int exynos_usbswitch_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_usb_switch *usb_switch = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);
	mutex_lock(&usb_switch->mutex);
	if (test_bit(USB_DEVICE_ATTACHED, &usb_switch->connect))
		exynos_change_usb_mode(usb_switch, USB_DEVICE_DETACHED);

	usb_switch->connect = 0;
	mutex_unlock(&usb_switch->mutex);

	return 0;
}

static int exynos_usbswitch_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_usb_switch *usb_switch = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);
	exynos_usb_status_init(usb_switch);

	return 0;
}
#else
#define exynos_usbswitch_suspend	NULL
#define exynos_usbswitch_resume		NULL
#endif

/* SysFS interface */

static ssize_t
exynos_usbswitch_show_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_usb_switch *usb_switch = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			exynos_usbswitch_mode_string(usb_switch->connect));
}

static DEVICE_ATTR(mode, S_IRUSR | S_IRGRP,
	exynos_usbswitch_show_mode, NULL);

static struct attribute *exynos_usbswitch_attributes[] = {
	&dev_attr_mode.attr,
	NULL
};

static const struct attribute_group exynos_usbswitch_attr_group = {
	.attrs = exynos_usbswitch_attributes,
};

static int exynos_usbswitch_parse_dt(struct exynos_usb_switch *usb_switch,
				     struct device *dev)
{
	struct device_node *node;
	struct platform_device *pdev;
	int ret;

	/* Device detection */
	usb_switch->gpio_device_detect = of_get_named_gpio(dev->of_node,
					"samsung,bsess-gpio", 0);
	if (!gpio_is_valid(usb_switch->gpio_device_detect)) {
		dev_info(dev, "device detect gpio is not available\n");
	} else {
		ret = devm_gpio_request(dev, usb_switch->gpio_device_detect,
						"usbswitch_b_sess_gpio");
		if (ret)
			dev_err(dev, "failed to request host detect gpio");
		else
			usb_switch->device_detect_irq =
				gpio_to_irq(usb_switch->gpio_device_detect);
	}

	/* UDC */
	node = of_parse_phandle(dev->of_node, "udc", 0);
	if (!node) {
		dev_info(dev, "udc device is not available\n");
	} else {
		pdev = of_find_device_by_node(node);
		if (!pdev) {
			dev_err(dev, "failed to find udc device\n");
		} else {
			dev_info(dev, "%s: find udc device\n", __func__);
			usb_switch->s3c_udc_dev = &pdev->dev;
		}
		of_node_put(node);
	}

	return 0;
}

static int exynos_usbswitch_probe(struct platform_device *pdev)
{
	struct s5p_usbswitch_platdata *pdata = dev_get_platdata(&pdev->dev);
	struct device *dev = &pdev->dev;
	struct exynos_usb_switch *usb_switch;
	const struct exynos_usbswitch_drvdata *drvdata;
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	usb_switch = devm_kzalloc(dev, sizeof(struct exynos_usb_switch),
					GFP_KERNEL);
	if (!usb_switch)
		return -ENOMEM;

	our_switch = usb_switch;
	mutex_init(&usb_switch->mutex);
	wake_lock_init(&usb_switch->wake_lock, WAKE_LOCK_SUSPEND,
			"usb_switch_present");
	usb_switch->workqueue = create_singlethread_workqueue("usb_switch");
	INIT_WORK(&usb_switch->switch_work, exynos_usb_switch_worker);

	if (dev->of_node) {
		ret = exynos_usbswitch_parse_dt(usb_switch, dev);
		if (ret < 0) {
			dev_err(dev, "Failed to parse dt\n");
			goto fail;
		}
	} else if (pdata) {
		usb_switch->gpio_device_detect = pdata->gpio_device_detect;
		usb_switch->s3c_udc_dev = pdata->s3c_udc_dev;
		usb_switch->device_detect_irq = platform_get_irq(pdev, 1);
	} else {
		dev_err(dev, "Platform data is not available\n");
		ret = -ENODEV;
		goto fail;
	}

	drvdata = exynos_usb_switch_get_driver_data(pdev);
	usb_switch->drvdata = drvdata;

	/* USB Device detect IRQ */
	if (usb_switch->device_detect_irq > 0 && usb_switch->s3c_udc_dev) {
		ret = devm_request_threaded_irq(dev,
				usb_switch->device_detect_irq,
				NULL, exynos_device_detect_thread,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, "DEVICE_DETECT", usb_switch);
		if (ret) {
			dev_err(dev, "Failed to request device irq %d\n",
					usb_switch->device_detect_irq);
			goto fail;
		}
	} else if (usb_switch->s3c_udc_dev) {
		if (usb_switch->drvdata->use_change_mode)
			exynos_change_usb_mode(usb_switch, USB_DEVICE_ATTACHED);
	} else {
		dev_info(dev, "Disable device detect IRQ\n");
	}

	if (usb_switch->drvdata->use_change_mode)
		exynos_usb_status_init(usb_switch);

	ret = sysfs_create_group(&dev->kobj, &exynos_usbswitch_attr_group);
	if (ret)
		dev_warn(dev, "failed to create dwc3 otg attributes\n");

	platform_set_drvdata(pdev, usb_switch);

	return 0;

fail:
	wake_unlock(&usb_switch->wake_lock);
	cancel_work_sync(&usb_switch->switch_work);
	destroy_workqueue(usb_switch->workqueue);
	mutex_destroy(&usb_switch->mutex);
	return ret;
}

static int exynos_usbswitch_remove(struct platform_device *pdev)
{
	struct exynos_usb_switch *usb_switch = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, 0);

	sysfs_remove_group(&pdev->dev.kobj, &exynos_usbswitch_attr_group);
	wake_unlock(&usb_switch->wake_lock);
	cancel_work_sync(&usb_switch->switch_work);
	destroy_workqueue(usb_switch->workqueue);
	mutex_destroy(&usb_switch->mutex);

	return 0;
}

static const struct dev_pm_ops exynos_usbswitch_pm_ops = {
	.suspend	= exynos_usbswitch_suspend,
	.resume		= exynos_usbswitch_resume,
};

struct exynos_usbswitch_drvdata exynos_usb_switch_drvdata = {
	.driver_unregister = 0,
	.use_change_mode = 1,
};

static const struct of_device_id of_exynos_usbswitch_match[] = {
	{
		.compatible =	"samsung,exynos-usb-switch",
		.data =	&exynos_usb_switch_drvdata,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_usbswitch_match);

static struct platform_driver exynos_usbswitch_driver = {
	.probe		= exynos_usbswitch_probe,
	.remove		= exynos_usbswitch_remove,
	.driver		= {
		.name	= "exynos-usb-switch",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(of_exynos_usbswitch_match),
		.pm	= &exynos_usbswitch_pm_ops,
	},
};

module_platform_driver(exynos_usbswitch_driver);

MODULE_DESCRIPTION("Exynos USB switch driver");
MODULE_AUTHOR("<yulgon.kim@samsung.com>");
MODULE_LICENSE("GPL");
