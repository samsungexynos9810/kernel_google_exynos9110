#include <asm/mach-types.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <plat/gpio-cfg.h>

struct device_node *node = NULL;
const char *realtek_rt8723_dt = "realtek,rtl8723";
unsigned int wowlan_irq = 0;

int rtw_plat_set_gpio_val(char *pin_name, int val)
{
	int gpio_num = 0;

    	printk("%s start!\n", __func__);
	if (node == NULL)
		return -1;

	/* gpio_num */
	gpio_num = of_get_named_gpio(node, pin_name, 0);
	if (gpio_num < 0) {
        	printk("Looking up %s property in node %s failed %d\n",
				pin_name, node->full_name, gpio_num);
		return -1;
	}
	if (gpio_is_valid(gpio_num)) {
		gpio_request(gpio_num, pin_name);
        	s3c_gpio_cfgpin(gpio_num, S3C_GPIO_SFN(0x1));//Output set-up
		if (val) {
			s3c_gpio_setpull(gpio_num, S3C_GPIO_PULL_UP);//UP
			gpio_direction_output(gpio_num, 1);
		} else {
			s3c_gpio_setpull(gpio_num, S3C_GPIO_PULL_DOWN);//UP
			gpio_direction_output(gpio_num, 0);
		}
		s5p_gpio_set_drvstr(gpio_num, S5P_GPIO_DRVSTR_LV4);
		//gpio_set_value_cansleep(gpio_num, 1);
		gpio_free(gpio_num);
	} else {
        	printk("rtk_wmt gpio_num pin(%u) is invalid.\n", gpio_num);
		return -1;
	}

	return gpio_num;
}
EXPORT_SYMBOL_GPL(rtw_plat_set_gpio_val);

unsigned int rtw_plat_set_gpio_irq(char *pin_name, int open)
{
	int gpio_num = 0;
	unsigned int oob_irq = 0;

    	printk("%s start!\n", __func__);
	if (node == NULL)
		return -1;

	/* gpio_num */
	gpio_num = of_get_named_gpio(node, pin_name, 0);
	if (gpio_num < 0) {
        	printk("Looking up %s property in node %s failed %d\n",
				pin_name, node->full_name, gpio_num);
		return -1;
	}
	if (gpio_is_valid(gpio_num)) {
		if (open) {
			gpio_request(gpio_num, pin_name);
			s3c_gpio_cfgpin(gpio_num, S3C_GPIO_SFN(0xF));//Input set-up
			oob_irq = gpio_to_irq(gpio_num);
			printk("%s wowlan_irq:%d!\n", __func__, oob_irq);
		} else {
			gpio_free(gpio_num);
		}
	} else {
        	printk("rtk_wmt gpio_num pin(%u) is invalid.\n", gpio_num);
		return -1;
	}

	return oob_irq;
}

unsigned int rtw_plat_oob_irq_get(void)
{
	return wowlan_irq;
}
EXPORT_SYMBOL_GPL(rtw_plat_oob_irq_get);

static int rtw_plat_parse_dt(void)
{
	node = of_find_compatible_node(NULL, NULL, realtek_rt8723_dt);
	if (!node) {
        	printk("%s of_find_compatible_node failed.\n", realtek_rt8723_dt);
		return -1;
	}

	rtw_plat_set_gpio_val("realtek,pwr_en", 1);
	rtw_plat_set_gpio_val("realtek,chip_en", 1);
	rtw_plat_set_gpio_val("realtek,wl_dis", 1);
	rtw_plat_set_gpio_val("realtek,bt_dis", 0);
	
	wowlan_irq = rtw_plat_set_gpio_irq("realtek,wl_wake_int", 1);

	return 0;
}

static int rtw_init_gpio(void)
{
	printk("%s enter \n", __func__);

	if (rtw_plat_parse_dt() < 0) {
        	printk("rtw_plat_parse_dt() failed. \n");
	}

	printk("%s exit \n", __func__);
	return 0;
}

static int __init wlan_bt_device_init(void)
{
	return rtw_init_gpio();
}
device_initcall(wlan_bt_device_init);
//MODULE_DESCRIPTION("Realtek wlan/bt init driver");
//MODULE_LICENSE("GPL");

