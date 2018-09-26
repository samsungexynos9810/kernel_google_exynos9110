/*
*  Bluetooth driver for Medaka board
*
*  Copyright (C) 2017 CASIO Computer Co.,Ltd.
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
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "board-medaka-rf.h"

static int dbg_level = DBG_INFO;
module_param(dbg_level, int, 0644);
MODULE_PARM_DESC(dbg_level, "set debug level");

static char bdaddress[20] = {
	'0', '0', ':', '0', '0', ':', '0', '0', ':',
	'0', '0', ':', '0', '0', ':', '0', '0',
	0x00, 0x00, 0x00
};

module_param_string(bdaddress, bdaddress, sizeof(bdaddress), S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(bdaddress, "bluetooth address");

#define dbg_err(fmt, ...) do { pr_err(fmt, ## __VA_ARGS__); } while(0)
#define dbg_info(fmt, ...) do { if (dbg_level & DBG_INFO) pr_info(fmt, ## __VA_ARGS__); } while(0)
#define dbg_trace(fmt, ...) do { if (dbg_level & DBG_TRACE) pr_info(fmt, ## __VA_ARGS__); } while(0)
#define dbg_verbose(fmt, ...) do { if (dbg_level & DBG_VERBOSE) pr_info(fmt, ## __VA_ARGS__); } while(0)

#define RFKILL_BLOCKED  1
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

static int bluetooth_set_power(void *data, bool blocked)
{
	int val_power = 0;
	val_power = gpio_get_value(BT_RESET_GPIO);
	if (blocked == 0 && val_power != 0) {
		return 1;	/* already power on */
	}
	if (blocked == 1 && val_power == 0) {
		return 1;	/* already power off */
	}

	dbg_trace("%s(): blocked=%d\n",__func__, blocked);

	// rfkill_ops callback. Turn transmitter on when blocked is false
	if (!blocked) {
		gpio_set_value(BT_RESET_GPIO, POWER_ON);
		msleep(30);
		dbg_info("%s(): Power ON\n", __func__);
	}
	else {
		gpio_set_value(BT_WAKE_GPIO, 0);
		gpio_set_value(BT_RESET_GPIO, POWER_OFF);
		dbg_info("%s(): Power OFF\n", __func__);
	}
	return 0;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static void set_wake_locked(int wake)
{
	bt_lpm.wake = wake;
	dbg_verbose("%s() wake:%d\n", __func__, wake);
	if (!wake)
		wake_unlock(&bt_lpm.wake_lock);

	gpio_set_value(BT_WAKE_GPIO, wake);
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer) {
	unsigned long flags;
	dbg_verbose("%s()\n", __func__);
	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	set_wake_locked(0);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);

	return HRTIMER_NORESTART;
}

void bt_lpm_exit_lpm_locked(struct uart_port *uport) {
	bt_lpm.uport = uport;
	dbg_verbose("%s()\n", __func__);

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);

	set_wake_locked(1);

	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
					HRTIMER_MODE_REL);
}
EXPORT_SYMBOL(bt_lpm_exit_lpm_locked);

static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm.host_wake)
		return;
	dbg_verbose("%s()\n", __func__);

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
	dbg_verbose("%s()\n", __func__);
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
	int ret = 0;

	dbg_trace("%s()\n", __func__);

	ret = gpio_request(BT_WAKE_GPIO, "bt_wake_gpio");
	if (unlikely(ret)) {
		dbg_err("Error: gpio_request BT_WAKE_GPIO\n");
		goto err;
	}

	ret = gpio_request(BT_HOST_WAKE_GPIO, "bt_host_wake_gpio");
	if (unlikely(ret)) {
		dbg_err("Error: gpio_request BT_HOST_WAKE_GPIO\n");
		gpio_free(BT_WAKE_GPIO);
		goto err;
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
		dbg_err("Error: request_irq bt host_wake\n");
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		goto err;
	}

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		dbg_err("Error: irq_set_irq_wake bt host_wake\n");
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		goto err;
	}

	ret = gpio_request(BT_HOST_TX_GPIO, "bt_tx_gpio");
	if (unlikely(ret)) {
		dbg_err("Error gpio_request BT_HOST_TX_GPIO\n");
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		goto err;
	}

	ret = gpio_request(BT_HOST_RTS_GPIO, "bt_rts_gpio");
	if (unlikely(ret)) {
		dbg_err("Error gpio_request BT_HOST_RTS_GPIO\n");
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		gpio_free(BT_HOST_TX_GPIO);
		goto err;
	}

	snprintf(bt_lpm.wake_lock_name, sizeof(bt_lpm.wake_lock_name),
			"BTLowPower");
	wake_lock_init(&bt_lpm.wake_lock, WAKE_LOCK_SUSPEND,
			 bt_lpm.wake_lock_name);

	disable_irq_wake(irq);
	return 0;

err:
	dbg_err("Failed to %s(): Error=%d\n", __func__, ret);
	return ret;
}

static int bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;
	int ret = 0;

	dbg_info("%s()\n", __func__);

	rc = gpio_request(BT_RESET_GPIO, "bt_reset_gpio");
	if (unlikely(rc)) {
		dbg_err("Failed to request BT_RESET_GPIO\n");
		return rc;
	}

	gpio_direction_output(BT_RESET_GPIO, 0);
	bt_rfkill = rfkill_alloc("cyw4343 Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bt_rfkill_ops,
				NULL);

	if (unlikely(!bt_rfkill)) {
		dbg_err("Failed to rfkill_alloc\n");
		gpio_free(BT_RESET_GPIO);
		return -ENOMEM;
	}

	rfkill_init_sw_state(bt_rfkill, RFKILL_BLOCKED);
	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		dbg_err("Failed to rfkill_register\n");
		rfkill_destroy(bt_rfkill);
		gpio_free(BT_RESET_GPIO);
		return -1;
	}

	rfkill_set_states(bt_rfkill, true, false);

	ret = bcm_bt_lpm_init(pdev);
	if (ret) {
		dbg_err("Failed to bcm_bt_lpm_init\n");
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);
		gpio_free(BT_RESET_GPIO);
	}

	return ret;
}

static int bluetooth_remove(struct platform_device *pdev)
{
	dbg_info("%s()\n", __func__);

	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(BT_RESET_GPIO);
	gpio_free(BT_WAKE_GPIO);
	gpio_free(BT_HOST_WAKE_GPIO);

	gpio_free(BT_HOST_TX_GPIO);
	gpio_free(BT_HOST_RTS_GPIO);

	wake_lock_destroy(&bt_lpm.wake_lock);

	return 0;
}

int bluetooth_suspend(struct platform_device *pdev, pm_message_t state)
{
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	dbg_trace("%s()\n", __func__);
	enable_irq_wake(irq);
	return 0;
}

int bluetooth_resume(struct platform_device *pdev)
{
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	dbg_trace("%s()\n", __func__);
	disable_irq_wake(irq);
	return 0;
}

static struct of_device_id bluetooth_medaka_of_match[] = {
	{ .compatible = "medaka-bluetooth", },
	{ },
};

static struct platform_driver bluetooth_platform_driver = {
	.probe = bluetooth_probe,
	.remove = bluetooth_remove,
	.suspend = bluetooth_suspend,
	.resume = bluetooth_resume,
	.driver = {
		.name = "medaka_bluetooth",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(bluetooth_medaka_of_match),
    },
};

static int __init bluetooth_init(void)
{
	char *pstr;

	dbg_trace("%s()\n", __func__);
	pstr = strstr(boot_command_line, "board_shiri_bluetooth.btmac=");
	if (pstr)
		memcpy(bdaddress, pstr + 28, 17);
	return platform_driver_register(&bluetooth_platform_driver);
}

static void __exit bluetooth_exit(void)
{
    dbg_trace("%s()\n", __func__);
	platform_driver_unregister(&bluetooth_platform_driver);
}

module_init(bluetooth_init);
module_exit(bluetooth_exit);

MODULE_ALIAS("platform:CYW4343");
MODULE_DESCRIPTION("CYW4343W Bluetooth Driver for Medaka board");
MODULE_LICENSE("GPL");
