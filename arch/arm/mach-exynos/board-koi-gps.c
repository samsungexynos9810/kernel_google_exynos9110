/*
 *  Copyright (C) 2016 CASIO Computer CO., LTD.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <asm/mach-types.h>
#include <linux/serial_core.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-koi.h>

struct gps_info {
	int nr_gpio;
	struct wake_lock wakelock;
	int last_host_wake;
};

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	struct gps_info *info = dev;
	int host_wake;

	host_wake = gpio_get_value(info->nr_gpio);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	if (host_wake != info->last_host_wake) {
		info->last_host_wake = host_wake;

		if (host_wake) {
			wake_lock(&info->wakelock);
		} else  {
			// Take a timed wakelock, so that upper layers can take it.
			// The chipset deasserts the hostwake lock, when there is no
			// more data to send.
			wake_lock_timeout(&info->wakelock, HZ/5);
		}
	}

	return IRQ_HANDLED;
}

static int koi_gps_probe(struct platform_device *pdev)
{
	int ret, irq;
	struct gps_info *info;

	pr_info("%s\n", __func__);
	info = devm_kzalloc(&pdev->dev, sizeof(struct gps_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->nr_gpio = of_get_named_gpio(pdev->dev.of_node,	"sub-main-int2", 0);
	pr_info("gps nr_gpio = %d\n", info->nr_gpio);
	ret = gpio_request(info->nr_gpio, "sub_main_int2");
	if (unlikely(ret)) {
		return ret;
	}

	wake_lock_init(&info->wakelock, WAKE_LOCK_SUSPEND, "koi_gps");

	gpio_direction_input(info->nr_gpio);
	irq = gpio_to_irq(info->nr_gpio);
	ret = request_irq(irq, host_wake_isr, IRQF_TRIGGER_HIGH,
		"gps_host_wake", info);
	if (ret) {
		gpio_free(info->nr_gpio);
		return ret;
	}

	ret = enable_irq_wake(irq);
	if (ret) {
		gpio_free(info->nr_gpio);
	}

	return ret;
}

static struct of_device_id koi_gps_of_match[] = {
	{ .compatible = "koi-gps", },
	{ },
};

static struct platform_driver koi_gps_platform_driver = {
	.probe = koi_gps_probe,
	.driver = {
		.name = "koi-gps",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(koi_gps_of_match),
	},
};

module_platform_driver(koi_gps_platform_driver);

int get_product_id(void);	/* defined in arch/arm/mach-exynos/exynos.c */

static int __init koi_gps_optimize_uart(void)
{
	/* optimize pull setting */
	/* UART1_TX */
	pin_config_set("11400000.pinctrl", "gpa0-5", 0x0002);// pud		:NoPull
	pin_config_set("11400000.pinctrl", "gpa0-5", 0x0005);// pudpdn	:NoPull
	/* UART1_RTS */
	pin_config_set("11400000.pinctrl", "gpa0-7", 0x0002);// pud:	:NoPull
	pin_config_set("11400000.pinctrl", "gpa0-7", 0x0005);// pudpdn	:NoPull
	if (get_product_id()) {	/* ayu */
		/* UART1_RX */
		pin_config_set("11400000.pinctrl", "gpa0-4", 0x0002);// pud		:NoPull
		pin_config_set("11400000.pinctrl", "gpa0-4", 0x0005);// pudpdn	:NoPull
		/* UART1_CTS */
		pin_config_set("11400000.pinctrl", "gpa0-6", 0x0002);// pud		:NoPull
		pin_config_set("11400000.pinctrl", "gpa0-6", 0x0005);// pudpdn	:NoPull
	}
	return 0;
}

late_initcall(koi_gps_optimize_uart);

MODULE_ALIAS("platform:koi-gps");
MODULE_DESCRIPTION("koi_gps");
MODULE_AUTHOR("casio");
MODULE_LICENSE("GPL");
