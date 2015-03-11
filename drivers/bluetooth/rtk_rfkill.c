/*
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/ioport.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>
#include <linux/acpi.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wakelock.h>


#include <asm/mach-types.h>
#include <mach/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <plat/gpio-cfg.h>

static struct rfkill *bt_rfk;
static const char bt_name[] = "bluetooth";
unsigned int bt_wake_host_irq = 0;
struct wake_lock bt2host_wakelock;
static struct device_node *node = NULL;
const char *realtek_bt_rt8723_dt = "realtek,rtl8723";

extern int rtw_plat_set_gpio_val(char *pin_name, int val);
//static  unsigned long bt_reset;

#if 0
/** Global state flags */
static unsigned long flags;
/* state variable names and bit positions */
#define FLAG_RESET      	0x00
#define BT_ACTIVE		0x02
#define BT_SUSPEND		0x04
#endif

static inline u32 ms_to_systime(u32 ms)
{
#ifdef PLATFORM_LINUX
	        return ms * HZ / 1000;
#endif
#ifdef PLATFORM_FREEBSD
		return ms /1000;
#endif
#ifdef PLATFORM_WINDOWS
		return ms * 10000 ;
#endif
		return ms*HZ/1000;
}


#if 0
static void getIoResource(struct platform_device  *pdev)
{
	struct resource *res;

	printk("rfkill get gpio\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,"bt_reset");
	if (!res) {
		printk("couldn't find bt_reset gpio\n");
	}

	bt_reset = res->start;

	printk("bt_reset = %ld\n", bt_reset);
}
#endif

static int bluetooth_set_power(void *data, bool blocked)
{
	printk("%s: block=%d\n",__func__, blocked);

	if (!blocked) {
	//	gpio_direction_output(bt_reset, 1);
		rtw_plat_set_gpio_val("realtek,bt_dis", 1);
	} else {
	//	gpio_direction_output(bt_reset, 0);
		rtw_plat_set_gpio_val("realtek,bt_dis", 0);
	}

	return 0;
}

static struct rfkill_ops rfkill_bluetooth_ops = {
	.set_block = bluetooth_set_power,
};


static irqreturn_t bt2host_wakeup_irq(int irq, void *dev_id)
{
	struct wake_lock *bt2host_wakelock = dev_id;
	printk("bt wake up irq \n");
#ifdef CONFIG_HAS_WAKELOCK
	//wake_lock_timeout(bt2host_wakelock,ms_to_systime(HZ/2));
	wake_lock_timeout(bt2host_wakelock,5*HZ);
#endif
	return IRQ_HANDLED;
}

#if 0
static void rfkill_gpio_init(void)
{
	if(gpio_request(bt_reset,"bt_reset")){
		printk("request bt reset fail\n");
	}
}

static void rfkill_gpio_deinit(void)
{
	gpio_free(bt_reset);
}
#endif
#if 1
static unsigned int rtw_plat_set_gpio_irq(char *pin_name, int open)
{
      int gpio_num = 0;
       unsigned int oob_irq = 0;

        printk("%s start!\n", __func__);
        if (node == NULL)
              return -1; 

	/* gpio_num */
        gpio_num = of_get_named_gpio(node, pin_name, 0); 
        if (gpio_num < 0) {
	     printk("Looking up %s property in node %s failed %d\n",pin_name, node->full_name, gpio_num);
	     return -1; 
	  }   
	if (gpio_is_valid(gpio_num)) 
	{
	     if (open)
	     {
	     	 gpio_request(gpio_num, pin_name);
               	 s3c_gpio_cfgpin(gpio_num, S3C_GPIO_SFN(0xF));//Input set-up
                 oob_irq = gpio_to_irq(gpio_num);
                 printk("%s bt_wake_host_irq:%d!\n", __func__, oob_irq);
             } else {
         		gpio_free(gpio_num);
             }   
  	 } else {
               printk("rtk_bt gpio_num pin(%u) is invalid.\n", gpio_num);
               return -1; 
     	}   

      return oob_irq;
}
#endif

static int rfkill_bluetooth_probe(struct platform_device *pdev)
{

	int rc = 0;
	bool default_state = true;

	printk(KERN_INFO "##################3-->%s\n", __func__);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&bt2host_wakelock, WAKE_LOCK_SUSPEND, "bt2host_wakeup");
#endif
        node = of_find_compatible_node(NULL, NULL, realtek_bt_rt8723_dt);
       if (!node) {
		printk("% of_find_compatible_node failed.\n",realtek_bt_rt8723_dt);
		return -1;
	}

	bt_wake_host_irq = rtw_plat_set_gpio_irq("realtek,bt_wake_int", 1);
	if(bt_wake_host_irq == -1)
	{
		printk(KERN_ERR "bt wake up host irq fail\n");
		return -1;
	}

	rc = request_irq(bt_wake_host_irq, bt2host_wakeup_irq, IRQF_TRIGGER_FALLING,"bt2host_wakeup", &bt2host_wakelock);
	if (rc) {
		printk(KERN_ERR "bt2host wakeup irq request failed!\n");
		wake_lock_destroy(&bt2host_wakelock);
		return -1;
	}

	//clear_bit(BT_SUSPEND, &flags);
	//set_bit(BT_ACTIVE, &flags);

	//	getIoResource(pdev);

	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
	   &rfkill_bluetooth_ops, NULL);
	if (!bt_rfk) {
	 rc = -ENOMEM;
	 goto err_rfkill_alloc;
	}
#if 0
        rfkill_gpio_init();
	/* userspace cannot take exclusive control */
#endif
	rfkill_init_sw_state(bt_rfk,false);
	rc = rfkill_register(bt_rfk);
	if (rc)
		goto err_rfkill_reg;

	rfkill_set_sw_state(bt_rfk,true);
#if 0
	bluetooth_set_power(NULL, default_state);
#endif
	rtw_plat_set_gpio_val("realtek,bt_dis", default_state);

	printk(KERN_INFO "<--%s\n", __func__);
	return 0;

err_rfkill_reg:
	rfkill_destroy(bt_rfk);
err_rfkill_alloc:
	return rc;
}

static int rfkill_bluetooth_remove(struct platform_device *dev)
{
	printk(KERN_INFO "-->%s\n", __func__);
	//if(disable_irq_wake(bt_wake_host_irq))
	//	printk("Couldn't disable hostwake IRQ wakeup mode\n");
	free_irq(bt_wake_host_irq, NULL);

	//rfkill_gpio_deinit();
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);
	printk(KERN_INFO "<--%s\n", __func__);
	return 0;
}

static int bluetooth_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("bluetooth suspend -->enter \n");
//	if(test_bit(BT_ACTIVE, &flags)) {
		if(enable_irq_wake(bt_wake_host_irq))
			printk("Couldn't enable hostwake IRQ wakeup mode\n");
						
//		clear_bit(BT_ACTIVE, &flags);
//		set_bit(BT_SUSPEND, &flags);
//	}
	printk("bluetooth suspend <--leave\n");
	return 0;
}
static int bluetooth_resume(struct platform_device *pdev)
{
	printk("bluetooth resume -->enter \n");
//	if (test_bit(BT_SUSPEND, &flags)) {
		if(disable_irq_wake(bt_wake_host_irq))
			printk("Couldn't disable hostwake IRQ wakeup mode\n");
//		clear_bit(BT_SUSPEND, &flags);
//		set_bit(BT_ACTIVE, &flags);
//	}
	printk("bluetooth resume <---leave\n");
	return 0;
}

static struct platform_driver rfkill_bluetooth_driver = {
	.probe  = rfkill_bluetooth_probe,
	.suspend  = bluetooth_suspend,
	.resume  = bluetooth_resume,
	.remove = rfkill_bluetooth_remove,
	.driver = {
		.name = "rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init rfkill_bluetooth_init(void)
{
	printk(KERN_INFO "-->%s\n", __func__);
	return platform_driver_register(&rfkill_bluetooth_driver);
}

static void __exit rfkill_bluetooth_exit(void)
{
	printk(KERN_INFO "-->%s\n", __func__);
	platform_driver_unregister(&rfkill_bluetooth_driver);
}

late_initcall(rfkill_bluetooth_init);
