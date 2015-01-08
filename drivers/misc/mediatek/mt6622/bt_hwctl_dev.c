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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <mach/io.h>
#include <asm/mach-types.h>


//for bluez compatible
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

// add for platform_device
#include <linux/mt5931_6622.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "bt_hwctl_dev.h"
/**************************************************************************
 *                    S T A T I C    V A R I A B L E S                    *
**************************************************************************/
wait_queue_head_t eint_wait;
int eint_gen;
int eint_mask;

static unsigned char irq_requested = 0;//wenhui add 
static int irq_num;
// to avoid irq enable and disable not match
static unsigned int irq_mask;

//hw ctl specified things
static struct bt_hwctl *bh = NULL;

static struct mtk_mt6622_platform_data mt6622_pdata;// platform data from board file

extern void omap_mux_set_gpio(u16 val, int gpio);


/****************************************************************************
 *                       I R Q   F U N C T I O N S                          *
*****************************************************************************/
static int mt_bt_request_irq(void)
{
    int iRet;
    irq_mask = 0;
    
    iRet = request_irq(irq_num, mt_bt_eirq_handler, 
        IRQF_TRIGGER_HIGH, "BT_INT_B", NULL);
    if (iRet){
        printk(KERN_ALERT MODULE_TAG "request_irq IRQ%d fails, errno %d\n", irq_num, iRet);
    }
    else{
        printk(KERN_INFO MODULE_TAG "request_irq IRQ%d success\n", irq_num);
        mt_bt_disable_irq();
        /* enable irq when driver init complete, at hci_uart_open */
    }
    irq_requested = 1; //wenhui add
    return iRet;
}

static void mt_bt_free_irq(void)
{
    if(irq_requested) {
	free_irq(irq_num, NULL);
	irq_mask = 0;
	irq_requested = 0;
    }
}

void mt_bt_enable_irq(void)
{
    printk(KERN_INFO MODULE_TAG "enable_irq, mask %d\n", irq_mask);
    if (irq_mask){
        irq_mask = 0;
        enable_irq(irq_num);
    }
}

void mt_bt_disable_irq(void)
{
    printk(KERN_INFO MODULE_TAG "disable_irq_nosync, mask %d\n", irq_mask);
    if (!irq_mask){
        irq_mask = 1;
        disable_irq_nosync(irq_num);
    }
}

// eirq handler
irqreturn_t mt_bt_eirq_handler(int i, void *arg)
{
    struct hci_dev *hdev = NULL;

    BT_HWCTL_ALERT("mt_bt_eirq_handler\n");
    mt_bt_disable_irq();

#ifdef CONFIG_BT_HCIUART
    /* BlueZ stack, hci_uart driver */
    hdev = hci_dev_get(0);
    if(hdev == NULL){
        /* Avoid the early interrupt before hci0 registered */
        BT_HWCTL_ALERT("hdev is NULL\n");
    }else{
        BT_HWCTL_ALERT("EINT arrives! Notify host awake\n");
        BT_HWCTL_ALERT("Send host awake command\n");
        hci_send_cmd(hdev, 0xFCC1, 0, NULL);
        /* Enable irq after receiving host awake command's event */
    }
#else
    /* Maybe handle the interrupt in user space? */
    eint_gen = 1;
    wake_up_interruptible(&eint_wait);
    /* Send host awake command in user space, enable irq then */
#endif

    return IRQ_HANDLED;
}

/*****************************************************************************
 *                       BT DEV OPS FUNCTIONS                                *
*****************************************************************************/
static int bt_hwctl_open(struct inode *inode, struct file *file)
{
    BT_HWCTL_DBG("bt_hwctl_open\n");
    eint_gen = 0;
    eint_mask = 0;
    return 0;
}

static long bt_hwctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
     
    BT_HWCTL_DBG("bt_hwctl_ioctl\n");
    
    if(!bh) {
        BT_HWCTL_ERR("bt_hwctl struct not initialized\n");
        return -EFAULT;
    }
    
    switch(cmd)
    {
        case BTHWCTL_IOCTL_SET_POWER:
        {
            unsigned long pwr = 0;
            if (copy_from_user(&pwr, (void*)arg, sizeof(unsigned long)))
                return -EFAULT;
                
            BT_HWCTL_DBG("BTHWCTL_IOCTL_SET_POWER: %d\n", (int)pwr);
            
            mutex_lock(&bh->sem);
            if (pwr){
                ret = mt_bt_power_on();
            }
            else{
                mt_bt_power_off();
            }
            mutex_unlock(&bh->sem);
            
            break;
        }
        case BTHWCTL_IOCTL_SET_EINT:
        {
            unsigned long eint = 0;
            if (copy_from_user(&eint, (void*)arg, sizeof(unsigned long)))
                return -EFAULT;
                
            BT_HWCTL_DBG("BTHWCTL_IOCTL_SET_EINT: %d\n", (int)eint);
            
            mutex_lock(&bh->sem);
            if (eint){
                /* Enable irq from user space */ 
                BT_HWCTL_DBG("Set BT EINT enable\n");
                mt_bt_enable_irq();
            }
            else{
                /* Disable irq from user space, maybe time to close driver */
                BT_HWCTL_DBG("Set BT EINT disable\n");
                mt_bt_disable_irq();
                eint_mask = 1;
                wake_up_interruptible(&eint_wait);
            }
            mutex_unlock(&bh->sem);
            break;
        }    
        default:
            BT_HWCTL_ALERT("BTHWCTL_IOCTL not support\n");
            return -EPERM;
    }
    return ret;
}

static int bt_hwctl_release(struct inode *inode, struct file *file)
{
    BT_HWCTL_DBG("bt_hwctl_release\n");
    eint_gen = 0;
    eint_mask = 0;
    return 0;
}

static unsigned int bt_hwctl_poll(struct file *file, poll_table *wait)
{
    uint32_t mask = 0;
    
    BT_HWCTL_DBG("bt_hwctl_poll eint_gen %d, eint_mask %d ++\n", eint_gen, eint_mask);
    //poll_wait(file, &eint_wait, wait);
    wait_event_interruptible(eint_wait, (eint_gen == 1 || eint_mask == 1));
    BT_HWCTL_DBG("bt_hwctl_poll eint_gen %d, eint_mask %d --\n", eint_gen, eint_mask);
    
    if(eint_gen == 1){
        mask = POLLIN|POLLRDNORM;
        eint_gen = 0;
    }
    else if (eint_mask == 1){
        mask = POLLERR;
        eint_mask = 0;
    }
    
    return mask;
}

/**************************************************************************
 *                K E R N E L   I N T E R F A C E S                       *
***************************************************************************/
static struct file_operations bt_hwctl_fops = {
    .owner      = THIS_MODULE,
//    .ioctl      = bt_hwctl_ioctl,
    .unlocked_ioctl = bt_hwctl_ioctl,
    .open       = bt_hwctl_open,
    .release    = bt_hwctl_release,
    .poll       = bt_hwctl_poll,
};

/**************************************************************************
 *                              POWER ON/OFF                              *
***************************************************************************/
int mt_bt_power_on(void)
{
    int error;
    printk(KERN_INFO MODULE_TAG "mt_bt_power_on ++\n");
    
/************************************************************
 *  Make sure BT_PWR_EN is default gpio output low when
 *  system boot up, otherwise MT6622 gets actived unexpectedly 
 ************************************************************/
    // EINT
    /* set to EINT mode */
    gpio_direction_input(mt6622_pdata.bt_eint);//important
    irq_num = gpio_to_irq(mt6622_pdata.bt_eint);

    printk("GPIO_BT_PWR_EN_PIN before config(out_h: %d) \n", gpio_get_value(mt6622_pdata.pmu));
    // PWR_EN & RESET pull high
    omap_mux_set_gpio(3, mt6622_pdata.pmu);
    gpio_direction_output(mt6622_pdata.pmu, 1);
    //if PWR_EN & RESET not share a single pin, please set rst pin to high here
    printk("GPIO_BT_PWR_EN_PIN after config(out_h: %d) \n", gpio_get_value(mt6622_pdata.pmu));
    msleep(1000);

    error = mt_bt_request_irq();
    if (error){
        /* Clear GPIO configurations */
        gpio_direction_output(mt6622_pdata.pmu, 0);
        //if PWR_EN & RESET not share a single pin, please set rst pin to low here
	    gpio_direction_output(mt6622_pdata.bt_eint, 0);
        return error;
    }
    
    printk(KERN_INFO MODULE_TAG "mt_bt_power_on --\n");
    return 0;
}

void mt_bt_power_off(void)
{
    printk(KERN_INFO MODULE_TAG "mt_bt_power_off ++\n");
    // PWR_EN and RESET
    gpio_direction_output(mt6622_pdata.pmu, 0);
       //if PWR_EN & RESET not share a single pin, please set rst pin to low here
    gpio_direction_output(mt6622_pdata.bt_eint, 0);
    
    mt_bt_free_irq();
    
    printk(KERN_INFO MODULE_TAG "mt_bt_power_off --\n");
}

/**************************************************************************
 *                           GPIO  FUNCTIONS                            *
***************************************************************************/
void mt6622_gpio_init(struct platform_device *pdev)
{
	struct mtk_mt6622_platform_data *p = pdev->dev.platform_data;

	mt6622_pdata.pmu = p->pmu;
    if (gpio_request(mt6622_pdata.pmu, "BT_PWR_EN")){
            BT_HWCTL_ALERT("GPIO%d for BT_PWR_EN is busy!\n", mt6622_pdata.pmu);
    }
    BT_HWCTL_INFO("GPIO%d requested for BT_PWR_EN!\n", mt6622_pdata.pmu);

// for cob may have individual pmu & rst pin
#ifndef MT6622_PMU_RST_SHARE
	mt6622_pdata.rst = p->rst;
    if (gpio_request(mt6622_pdata.rst, "BT_RESET")){
            BT_HWCTL_ALERT("GPIO%d for BT_RESET is busy!\n", mt6622_pdata.rst);
    }
    BT_HWCTL_INFO("GPIO%d requested for BT_RESET!\n", mt6622_pdata.rst);
#endif
	mt6622_pdata.bt_eint = p->bt_eint;
    if (gpio_request(mt6622_pdata.bt_eint, "BT_EINT")){
            BT_HWCTL_ALERT("GPIO%d for BT_RESET is busy!\n", mt6622_pdata.bt_eint);
    }
    BT_HWCTL_INFO("GPIO%d requested for BT_EINT!\n", mt6622_pdata.bt_eint);
}

static int mt6622_plt_probe(struct platform_device *pdev)
{
	mt6622_gpio_init(pdev);
	return 0;
}

static int mt6622_plt_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mt6622_platform_driver = {
	.probe = mt6622_plt_probe,
	.remove = mt6622_plt_remove,
	.driver = {
		.name = "mtk_mt6622",
		.owner = THIS_MODULE,
	},
};

/**************************************************************************
 *                         MODULE INIT/EXIT                            *
***************************************************************************/
static int __init bt_hwctl_init(void)
{
    int ret = -1, err = -1;

    if (!(bh = kzalloc(sizeof(struct bt_hwctl), GFP_KERNEL)))
    {
        BT_HWCTL_ERR("bt_hwctl_init allocate dev struct failed\n");
        err = -ENOMEM;
        goto ERR_EXIT;
    }
    
    ret = alloc_chrdev_region(&bh->dev_t, 0, 1, BTHWCTL_NAME);
    if (ret) {
        BT_HWCTL_ERR("alloc chrdev region failed\n");
        goto ERR_EXIT;
    }
    
    BT_HWCTL_INFO("alloc %s: %d:%d\n", BTHWCTL_NAME, MAJOR(bh->dev_t), MINOR(bh->dev_t));
    
    cdev_init(&bh->cdev, &bt_hwctl_fops);
    
    bh->cdev.owner = THIS_MODULE;
    bh->cdev.ops = &bt_hwctl_fops;
    
    err = cdev_add(&bh->cdev, bh->dev_t, 1);
    if (err) {
        BT_HWCTL_ERR("add chrdev failed\n");
        goto ERR_EXIT;
    }
    
    bh->cls = class_create(THIS_MODULE, BTHWCTL_NAME);
    if (IS_ERR(bh->cls)) {
        err = PTR_ERR(bh->cls);
        BT_HWCTL_ERR("class_create failed, errno:%d\n", err);
        goto ERR_EXIT;
    }
    
    bh->dev = device_create(bh->cls, NULL, bh->dev_t, NULL, BTHWCTL_NAME);
    mutex_init(&bh->sem);
    
    init_waitqueue_head(&eint_wait);

    platform_driver_register(&mt6622_platform_driver);//wenhui
    
    BT_HWCTL_INFO("Bluetooth hardware control driver initialized\n");
    
    return 0;
    
ERR_EXIT:
    if (err == 0)
        cdev_del(&bh->cdev);
    if (ret == 0)
        unregister_chrdev_region(bh->dev_t, 1);
        
    if (bh){
        kfree(bh);
        bh = NULL;
    }     
    return -1;
}

/*****************************************************************************/
static void __exit bt_hwctl_exit(void)
{    
    if (bh){
        cdev_del(&bh->cdev);
        
        unregister_chrdev_region(bh->dev_t, 1);
        device_destroy(bh->cls, bh->dev_t);
        
        class_destroy(bh->cls);
        mutex_destroy(&bh->sem);
        
        kfree(bh);
        bh = NULL;
    }
    
    /* release gpio used by BT */
    mt_bt_gpio_release();
}

module_init(bt_hwctl_init);
module_exit(bt_hwctl_exit);
MODULE_AUTHOR("Tingting Lei <tingting.lei@mediatek.com>");
MODULE_DESCRIPTION("Bluetooth hardware control driver");
MODULE_LICENSE("GPL");
