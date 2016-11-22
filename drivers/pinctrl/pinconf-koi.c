/*
 * Core driver for the pin config portions of the pin control subsystem
 * For Koi/Ayu
 *
 * Copyright (C) 2016 CASIO Computer CO.,LTD.
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#define pr_fmt(fmt) "pinconfig core: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-koi.h>
#include "core.h"
#include "pinconf.h"

static int pin_config_set_for_pin(struct pinctrl_dev *pctldev, unsigned pin,
              unsigned long *configs)
{
   const struct pinconf_ops *ops = pctldev->desc->confops;
   int ret;

   if (!ops || !ops->pin_config_set) {
       dev_err(pctldev->dev, "cannot configure pin, missing "
           "config function in driver\n");
       return -EINVAL;
   }

   ret = ops->pin_config_set(pctldev, pin, configs, 1);
   if (ret) {
       dev_err(pctldev->dev,
           "unable to set pin configuration on pin %d\n", pin);
       return ret;
   }

   return 0;
}

/**
 * pin_config_set() - set the configuration of a single pin parameter
 * @dev_name: name of pin controller device for this pin
 * @name: name of the pin to set the config for
 * @config: the config in this argument will contain the desired pin state, it
 * can be used directly by drivers as a numeral, or it can be dereferenced
 * to any struct.
 */
int pin_config_set(const char *dev_name, const char *name,
          unsigned long config)
{
   struct pinctrl_dev *pctldev;
   int pin, ret;
   unsigned long config_array = { config };
   unsigned long *configs = &config_array;

   pctldev = get_pinctrl_dev_from_devname(dev_name);
   if (!pctldev) {
       ret = -EINVAL;
       return ret;
   }

   mutex_lock(&pctldev->mutex);

   pin = pin_get_from_name(pctldev, name);
   if (pin < 0) {
       ret = pin;
       goto unlock;
   }

   ret = pin_config_set_for_pin(pctldev, pin, configs);

unlock:
   mutex_unlock(&pctldev->mutex);
   return ret;
}
EXPORT_SYMBOL(pin_config_set);

int exynos_get_eint_base(void)
{
	struct pinctrl_dev *pctldev;
	u32 ret;

	pctldev = get_pinctrl_dev_from_devname("11000000.pinctrl");
	BUG_ON(!pctldev);

	mutex_lock(&pctldev->mutex);
	ret = pin_get_from_name(pctldev, "gpx0-0");
	mutex_unlock(&pctldev->mutex);

	return ret;
}
