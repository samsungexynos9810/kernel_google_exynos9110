/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 * 
 * MediaTek Inc. (C) 2010. All rights reserved.
 * 
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#ifndef __MTK_BT_HWCTL_H
#define __MTK_BT_HWCTL_H

#include <linux/types.h>
#include <linux/irq.h>

//wenhui will delete
struct gpio_set {
	unsigned int gpio;
	int requested;
	char *label;
};

struct bt_hwctl {
    bool powerup;
    dev_t dev_t;
    struct class *cls;
    struct device *dev;
    struct cdev cdev;
    struct mutex sem;
};

#define BTHWCTL_NAME                 "bthwctl"
#define BTHWCTL_DEV_NAME             "/dev/bthwctl"
#define BTHWCTL_IOC_MAGIC            0xf6
#define BTHWCTL_IOCTL_SET_POWER      _IOWR(BTHWCTL_IOC_MAGIC, 0, uint32_t)
#define BTHWCTL_IOCTL_SET_EINT       _IOWR(BTHWCTL_IOC_MAGIC, 1, uint32_t)


#define BT_HWCTL_DEBUG  1

#define BT_HWCTL_INFO(f, s...)   printk(KERN_INFO "BTHWCTL: " f, ## s)
#define BT_HWCTL_ALERT(f, s...)  printk(KERN_ALERT "BTHWCTL: " f, ## s)
#define BT_HWCTL_ERR(f, s...)    printk(KERN_ERR "BTHWCTL: " f, ## s)
#if BT_HWCTL_DEBUG
#define BT_HWCTL_DBG(f, s...)    printk(KERN_INFO "BTHWCTL: " f, ## s)
#else
#define BT_HWCTL_DBG(f, s...)    ((void)0)
#endif

/****************************************************************************
 *                           C O N S T A N T S                              *
*****************************************************************************/
#define MODULE_TAG         "[BT-PLAT] "

/****************************************************************************
 *                           F U N C T I O N  S                              *
*****************************************************************************/
void mt_bt_gpio_init(void);
void mt_bt_gpio_release(void);
int  mt_bt_power_on(void);
void mt_bt_power_off(void);

irqreturn_t mt_bt_eirq_handler(int, void*);
void mt_bt_enable_irq(void);
void mt_bt_disable_irq(void);



#endif
