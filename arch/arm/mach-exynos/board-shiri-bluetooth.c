/*
 * Bluetooth Broadcomm  and low power control via GPIO
 *
 *  Copyright (C) 2011 Google, Inc.
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
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <asm/mach-types.h>
#include "serial.h"
#include <linux/serial_core.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <mach/board-shiri-bluetooth.h>

#define BT_RESET_GPIO		(0x5b)

#define BT_WAKE_GPIO		(0x19)
#define BT_HOST_WAKE_GPIO	(0x8F)

#define	BT_PWR_ON		(0x1)
#define BT_PWR_OFF		(0x0)

#define WLAN_POWER		147

#define BT_MAIN_TX_GPIO	        1
#define BT_MAIN_RTS_GPIO	3

#undef DEBUG_PRINT

#ifdef DEBUG_PRINT
#define PR_INFO(a, args...) pr_info(a, ##args)
#else
#define PR_INFO(a, args...)
#endif

static struct rfkill *bt_rfkill;

struct bcm_bt_lpm {
	int wake;
	int host_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct uart_port *uport;

	struct wake_lock wake_lock;
	char wake_lock_name[100];
} bt_lpm;


static void bluetooth_power_init(void)
{
	struct regulator *regulator = NULL;
	struct regulator_dev *rdev;
	regulator = regulator_get(NULL, "vdd_bt_dual_1v8");
	if(regulator == NULL) {
		pr_err("bluetooth regulator get error\n");
		return;
	}
	rdev = regulator->rdev;
	if(rdev->use_count == 0) {
		regmap_update_bits(rdev->regmap, 0x0C, 0x04, 0x00);	/* 32kHzBT_EN OFF */
	}
	regulator_put(regulator);
	return;
}


/* cf. arch\arm\mach-exynos\include\mach\pinctrl-samsung.h */
static void bluetooth_set_pincntl(int onoff) {
	int ret;
	if(onoff == BT_PWR_ON) {
		PR_INFO("%s() ON\n", __func__);

		ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0002);/* 1byte:disable 0byte:pud */
		ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0002);/* 1byte:disable 0byte:pud */
		ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0002);/* 1byte:disable 0byte:pud */
		ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0002);/* 1byte:disable 0byte:pud */
		ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0102);/* 1byte:pull down 0byte:pud */
		ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0002);/* 1byte:disable 0byte:pud */

		ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0200);/* 1byte:UART_0_RXD 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0200);/* 1byte:UART_0_TXD 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0200);/* 1byte:UART_0_CTSn 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0200);/* 1byte:UART_0_RTSn 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0100);/* 1byte:Output 0byte:func */
		ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0F00);/* 1byte:EXT_INT42 0byte:func */

		if(gpio_get_value(WLAN_POWER) == 0) { // WLAN OFF
			ret = pin_config_set("11000000.pinctrl", "gpk1-0", 0x0002);/* 1byte:pull up 0byte:pud */
			ret = pin_config_set("11000000.pinctrl", "gpk1-1", 0x0302);/* 1byte:pull up 0byte:pud */
			ret = pin_config_set("11000000.pinctrl", "gpk1-3", 0x0302);/* 1byte:pull up 0byte:pud */
			ret = pin_config_set("11000000.pinctrl", "gpk1-4", 0x0302);/* 1byte:pull up 0byte:pud */
			ret = pin_config_set("11000000.pinctrl", "gpk1-5", 0x0302);/* 1byte:pull up 0byte:pud */
			ret = pin_config_set("11000000.pinctrl", "gpk1-6", 0x0302);/* 1byte:pull up 0byte:pud */
			ret = pin_config_set("11000000.pinctrl", "gpx2-3", 0x0002);/* 1byte:disable 0byte:pud */

			ret = pin_config_set("11000000.pinctrl", "gpk1-0", 0x0200);/* 1byte:SDIO 0byte:func */
			ret = pin_config_set("11000000.pinctrl", "gpk1-1", 0x0200);/* 1byte:SDIO 0byte:func */
			ret = pin_config_set("11000000.pinctrl", "gpk1-3", 0x0200);/* 1byte:SDIO 0byte:func */
			ret = pin_config_set("11000000.pinctrl", "gpk1-4", 0x0200);/* 1byte:SDIO 0byte:func */
			ret = pin_config_set("11000000.pinctrl", "gpk1-5", 0x0200);/* 1byte:SDIO 0byte:func */
			ret = pin_config_set("11000000.pinctrl", "gpk1-6", 0x0200);/* 1byte:SDIO 0byte:func */
			ret = pin_config_set("11000000.pinctrl", "gpx2-3", 0x0F00);/* 1byte:EXT_INT42 0byte:func */
		}
		gpio_direction_output(BT_WAKE_GPIO, 0);
	} else { // BT off
		if(gpio_get_value(WLAN_POWER) == 0) { // WLAN OFF
			PR_INFO("%s() ON  wlan = off\n", __func__);

			ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0102);/* 1byte:pull down 0byte:pud */
			ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0102);/* 1byte:pull down 0byte:pud */
			ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0102);/* 1byte:pull down 0byte:pud */
			ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0102);/* 1byte:pull down 0byte:pud */
			ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0102);/* 1byte:pull down 0byte:pud */
			ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0102);/* 1byte:pull down 0byte:pud */

			ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0100);/* 1byte:output 0byte:func */
			ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0100);/* 1byte:output 0byte:func */
			ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0100);/* 1byte:output 0byte:func */
			ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0100);/* 1byte:output 0byte:func */
			ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0100);/* 1byte:output 0byte:func */
			ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0100);/* 1byte:output 0byte:func */
		} else { // WLAN on
			PR_INFO("%s() ON  wlan = on\n", __func__);

			ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0002);/* 1byte:disable 0byte:pud */
			ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0002);/* 1byte:disable 0byte:pud */
			ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0002);/* 1byte:disable 0byte:pud */
			ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0002);/* 1byte:disable 0byte:pud */
			ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0102);/* 1byte:pull down 0byte:pud */
			ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0102);/* 1byte:pull down 0byte:pud */

			ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0000);/* 1byte:input 0byte:func */
			ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0000);/* 1byte:input 0byte:func */
			ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0000);/* 1byte:input 0byte:func */
			ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0000);/* 1byte:input 0byte:func */
			ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0100);/* 1byte:output 0byte:func */
			ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0100);/* 1byte:output 0byte:func */

		}
	}

	return;
}

static void bluetooth_power(int onoff)
{
	struct regulator *regulator = NULL;
	struct regulator_dev *rdev;
	int rc=0;
	u32 pre_use_count;
	int ret;

	regulator = regulator_get(NULL, "vdd_bt_dual_1v8");
	if(regulator == NULL) {
		pr_err("bluetooth regulator get error\n");
		return;
	}
	rdev = regulator->rdev;
	pre_use_count = rdev->use_count;
	if(onoff == BT_PWR_ON){
		rc = regulator_enable(regulator);
		if(rdev->use_count == 1) {
			PR_INFO("%s() CLK ON rdev->usecount = %d\n", __func__, rdev->use_count);
			regmap_update_bits(rdev->regmap, 0x0C, 0x04, 0x04);	/* 32kHzBT_EN ON */
		}

		bluetooth_set_pincntl(onoff);

	}else{
		rc = regulator_disable(regulator);
		if(rdev->use_count == 0) {
			PR_INFO("%s() CLK OFF rdev->usecount = %d\n", __func__, rdev->use_count);
			regmap_update_bits(rdev->regmap, 0x0C, 0x04, 0x00); /* 32kHzBT_EN OFF */
		}

		bluetooth_set_pincntl(onoff);
	}

	PR_INFO("bluetooth regulator turn on onoff=%d use_count=%d -> %d rc=%d\n",
				onoff, pre_use_count, rdev->use_count, rc);
	if (rc < 0) {
		printk("%s: bluetooth %sable fail %d\n", __func__, onoff ? "en" : "dis", rc);

		goto regs_fail;
	}

regs_fail:
	regulator_put(regulator);
	return;
}

static int bcm4330_bt_rfkill_set_power(void *data, bool blocked)
{
	int val_power = 0;

	val_power = gpio_get_value(BT_RESET_GPIO);
	if( blocked == 0 && val_power != 0) {
		return 1;	/* already power on */
	}
	if( blocked == 1 && val_power == 0) {
		return 1;	/* already power off */
	}

	PR_INFO("bcm4330_bt_rfkill_set_power() blocked=%d\n",blocked);

	// rfkill_ops callback. Turn transmitter on when blocked is false
	if (!blocked) {
		bluetooth_power(BT_PWR_ON);
		msleep(20);
		gpio_set_value(BT_RESET_GPIO, 1);
		msleep(10);
	}
	else {
		gpio_set_value(BT_WAKE_GPIO, 0);
		gpio_set_value(BT_RESET_GPIO, 0);
		msleep(20);
		bluetooth_power(BT_PWR_OFF);
	}
	PR_INFO("bcm4330_bt_rfkill_set_power() blocked=%d end\n",blocked);
	return 0;

}

static const struct rfkill_ops bcm4330_bt_rfkill_ops = {
	.set_block = bcm4330_bt_rfkill_set_power,
};

static void set_wake_locked(int wake)
{
	bt_lpm.wake = wake;

	if (!wake)
		wake_unlock(&bt_lpm.wake_lock);

	gpio_set_value(BT_WAKE_GPIO, wake);
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer) {
	unsigned long flags;
	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	set_wake_locked(0);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);

	return HRTIMER_NORESTART;
}

void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport) {
	bt_lpm.uport = uport;

	if(uport->mapbase!=0x13800000)
		return;

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);

	set_wake_locked(1);

	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
		HRTIMER_MODE_REL);
}
EXPORT_SYMBOL(bcm_bt_lpm_exit_lpm_locked);

static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm.host_wake)
		return;

	bt_lpm.host_wake = host_wake;

	if (host_wake) {
		wake_lock(&bt_lpm.wake_lock);
	} else  {
		// Take a timed wakelock, so that upper layers can take it.
		// The chipset deasserts the hostwake lock, when there is no
		// more data to send.
		wake_lock_timeout(&bt_lpm.wake_lock, HZ/2);
	}

}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;
	unsigned long flags;

	host_wake = gpio_get_value(BT_HOST_WAKE_GPIO);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	if (!bt_lpm.uport) {
		bt_lpm.host_wake = host_wake;
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	update_host_wake_locked(host_wake);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);

	return IRQ_HANDLED;
}

static int bcm_bt_lpm_init(struct platform_device *pdev)
{
	int irq;
	int ret;
	int rc;

	rc = gpio_request(BT_WAKE_GPIO, "bcm4330_wake_gpio");
	if (unlikely(rc)) {
		return rc;
	}

	rc = gpio_request(BT_HOST_WAKE_GPIO, "bcm4330_host_wake_gpio");
	if (unlikely(rc)) {
		gpio_free(BT_WAKE_GPIO);
		return rc;
	}

	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(1, 0);  /* 1 sec */
	bt_lpm.enter_lpm_timer.function = enter_lpm;

	bt_lpm.host_wake = 0;

	gpio_direction_output(BT_WAKE_GPIO, 0);
	gpio_direction_input(BT_HOST_WAKE_GPIO);
	irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	ret = request_irq(irq, host_wake_isr, IRQF_TRIGGER_HIGH,
		"bt host_wake", NULL);
	if (ret) {
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		return ret;
	}

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		return ret;
	}

	ret = gpio_request(BT_MAIN_TX_GPIO, "bcm4343_tx_gpio");
	if (unlikely(ret)) {
		pr_err("Request is denyed:bluetooth tx gpio\n");
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		return ret;
	}

	ret = gpio_request(BT_MAIN_RTS_GPIO, "bcm4343_rts_gpio");
	if (unlikely(ret)) {
		pr_err("Request is denyed:bluetooth rts gpio\n");
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		gpio_free(BT_MAIN_TX_GPIO);
		return rc;
	}

	snprintf(bt_lpm.wake_lock_name, sizeof(bt_lpm.wake_lock_name),
			"BTLowPower");
	wake_lock_init(&bt_lpm.wake_lock, WAKE_LOCK_SUSPEND,
			 bt_lpm.wake_lock_name);

	disable_irq_wake(irq);
	return 0;
}

static int bcm4330_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;
	int ret = 0;

	PR_INFO("bcm4330_bluetooth_probe\n");
	rc = gpio_request(BT_RESET_GPIO, "bcm4330_nreset_gpip");
	if (unlikely(rc)) {
		return rc;
	}

	gpio_direction_output(BT_RESET_GPIO, 0);
	bt_rfkill = rfkill_alloc("bcm4330 Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bcm4330_bt_rfkill_ops,
				NULL);

	if (unlikely(!bt_rfkill)) {
		gpio_free(BT_RESET_GPIO);
		return -ENOMEM;
	}

	bluetooth_power_init();
	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		rfkill_destroy(bt_rfkill);
		gpio_free(BT_RESET_GPIO);
		return -1;
	}

	rfkill_set_states(bt_rfkill, true, false);

	ret = bcm_bt_lpm_init(pdev);
	if (ret) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);

		gpio_free(BT_RESET_GPIO);
	}

	return ret;
}

static int bcm4330_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(BT_RESET_GPIO);
	gpio_free(BT_WAKE_GPIO);
	gpio_free(BT_HOST_WAKE_GPIO);

	gpio_free(BT_MAIN_TX_GPIO);
	gpio_free(BT_MAIN_RTS_GPIO);

	wake_lock_destroy(&bt_lpm.wake_lock);
	return 0;
}

int bcm4430_bluetooth_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret;
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);

	if(gpio_get_value(BT_RESET_GPIO) == BT_PWR_ON) { // BT on, suspend
		PR_INFO("%s() BT ON\n", __FUNCTION__);

		ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0302);/* 1byte:pull up 0byte:pud RX*/
		ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0302);/* 1byte:pull up 0byte:pud TX*/
		ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0002);/* 1byte:disable 0byte:pud CTS*/
		ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0302);/* 1byte:pull up 0byte:pud RTS*/
		ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0102);/* 1byte:pull down 0byte:pud MAIN_DUAL_WKUP*/
		ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0102);/* 1byte:pull down 0byte:pud DUAL_MAIN_WKUP*/

		ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0000);/* 1byte:input 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0100);/* 1byte:output 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0000);/* 1byte:input 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0100);/* 1byte:output 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0100);/* 1byte:output 0byte:func */
		ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0F00);/* 1byte:EXT_INT42 0byte:func */		

		gpio_direction_output(BT_MAIN_TX_GPIO, 1);
		gpio_direction_output(BT_MAIN_RTS_GPIO, 1);
		
	} else {
		PR_INFO("%s(): BT OFF\n", __FUNCTION__);
	}


	enable_irq_wake(irq);
	return 0;
}

int bcm4430_bluetooth_resume(struct platform_device *pdev)
{
	int ret;
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);

	disable_irq_wake(irq);

	if(gpio_get_value(BT_RESET_GPIO) == BT_PWR_ON) { // BT on, resume
		PR_INFO("%s(): BT ON\n", __FUNCTION__);
		ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0002);/* 1byte:disable 0byte:pud */
		ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0002);/* 1byte:disable 0byte:pud */
		ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0002);/* 1byte:disable 0byte:pud */
		ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0002);/* 1byte:disable 0byte:pud */
		ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0102);/* 1byte:pull down 0byte:pud */
		ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0002);/* 1byte:disable 0byte:pud */

		ret = pin_config_set("11400000.pinctrl", "gpa0-0", 0x0200);/* 1byte:UART_0_RXD 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpa0-1", 0x0200);/* 1byte:UART_0_TXD 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpa0-2", 0x0200);/* 1byte:UART_0_CTSn 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpa0-3", 0x0200);/* 1byte:UART_0_RTSn 0byte:func */
		ret = pin_config_set("11400000.pinctrl", "gpc0-3", 0x0100);/* 1byte:Output 0byte:func */
		ret = pin_config_set("11000000.pinctrl", "gpx2-6", 0x0F00);/* 1byte:EXT_INT42 0byte:func */
	} 
	return 0;
}

struct bluetooth_bcm4330_platform_data {
	int gpio_reset;
};

static struct bluetooth_bcm4330_platform_data bluetooth_bcm4330_platform;

static struct of_device_id bluetooth_bcm4330_of_match[] = {
	{ .compatible = "bt_host_wake", },
	{ },
};

static struct platform_driver bcm4330_bluetooth_platform_driver = {
	.probe = bcm4330_bluetooth_probe,
	.remove = bcm4330_bluetooth_remove,
	.suspend = bcm4430_bluetooth_suspend,
	.resume = bcm4430_bluetooth_resume,
	.driver = {
		.name = "bcm4330_bluetooth",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(bluetooth_bcm4330_of_match),
		   },
};

static int __init bcm4330_bluetooth_init(void)
{
	int ret;
	return platform_driver_register(&bcm4330_bluetooth_platform_driver);
}

static void __exit bcm4330_bluetooth_exit(void)
{
	platform_driver_unregister(&bcm4330_bluetooth_platform_driver);
}


module_init(bcm4330_bluetooth_init);
module_exit(bcm4330_bluetooth_exit);

MODULE_ALIAS("platform:bcm4330");
MODULE_DESCRIPTION("bcm4330_bluetooth");
MODULE_AUTHOR("Jaikumar Ganesh <jaikumar@google.com>");
MODULE_LICENSE("GPL");
