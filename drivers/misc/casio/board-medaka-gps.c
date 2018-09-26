/*
 *  Copyright (C) 2017 CASIO Computer CO., LTD.
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
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <asm/uaccess.h>

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

#define BUF0_SIZE	2048
#define BUF1_SIZE	32

struct gps_buffer {
	int ridx0;
	int widx0;
	wait_queue_head_t wait0;
	spinlock_t slock0;
	__u8 buf0[BUF0_SIZE];

	int ridx1;
	int widx1;
	wait_queue_head_t wait1;
	spinlock_t slock1;
	__u8 buf1[BUF1_SIZE];
};

static struct gps_buffer *gb;

/* called from assistservice to write to buf0 */
static ssize_t gps0_write(struct file *file, const char *buf,
				size_t count, loff_t *offset)
{
	int num, widx, tmp, ret, copied = 0;
	unsigned long flags;

	while (count) {
		num = BUF0_SIZE - (gb->widx0 - gb->ridx0);
		if (num) {
			if (num > count)
				num = count;
			widx = gb->widx0 & (BUF0_SIZE - 1);
			tmp = BUF0_SIZE - widx;
			if (tmp >= num) {
				if (copy_from_user(&gb->buf0[widx], buf, num))
					return -EFAULT;
			} else {
				if (copy_from_user(&gb->buf0[widx], buf, tmp))
					return -EFAULT;
				if (copy_from_user(&gb->buf0[0], buf + tmp, num - tmp))
					return -EFAULT;
			}
			buf += num;
			spin_lock_irqsave(&gb->slock0, flags);
			gb->widx0 += num;
			spin_unlock_irqrestore(&gb->slock0, flags);
			wake_up_interruptible(&gb->wait0);
			count -= num;
			copied += num;
		} else {
			ret = wait_event_interruptible(gb->wait0, BUF0_SIZE - (gb->widx0 - gb->ridx0));
			if (ret < 0)
				return ret;
		}
	}
	return copied;
}

/* called from gps_hal to read from buf0 */
static ssize_t gps1_read(struct file *file, char *buf, size_t count,
				loff_t *offset)
{
	int num, ridx, tmp, ret;
	unsigned long flags;

	if (gb->widx0 > 32768 && gb->ridx0 > 32768) {
		spin_lock_irqsave(&gb->slock0, flags);
		gb->widx0 -= 32768;
		gb->ridx0 -= 32768;
		spin_unlock_irqrestore(&gb->slock0, flags);
	}
	num = gb->widx0 - gb->ridx0;
	if (num == 0) {	/* empty */
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(gb->wait0, gb->widx0 - gb->ridx0);
		if (ret < 0)
			return ret;
		num = gb->widx0 - gb->ridx0;
	}
	if (num > count)
		num = count;
	ridx = gb->ridx0 & (BUF0_SIZE - 1);
	tmp = BUF0_SIZE - ridx;
	if (tmp >= num) {
		if (copy_to_user(buf, &gb->buf0[ridx], num))
			return -EFAULT;
	} else {
		if (copy_to_user(buf, &gb->buf0[ridx], tmp))
			return -EFAULT;
		if (copy_to_user(buf + tmp, &gb->buf0[0], num - tmp))
			return -EFAULT;
	}
	spin_lock_irqsave(&gb->slock0, flags);
	gb->ridx0 += num;
	spin_unlock_irqrestore(&gb->slock0, flags);
	wake_up_interruptible(&gb->wait0);
	return num;
}

/* called from gps_hal to write to buf1 */
static ssize_t gps1_write(struct file *file, const char *buf,
				size_t count, loff_t *offset)
{
	int num, widx, tmp, ret;
	unsigned long flags;

	num = BUF1_SIZE - (gb->widx1 - gb->ridx1);
	if (num == 0) {	/* no room */
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(gb->wait1, BUF1_SIZE - (gb->widx1 - gb->ridx1));
		if (ret < 0)
			return ret;
		num = BUF1_SIZE - (gb->widx1 - gb->ridx1);
	}
	if (num > count)
		num = count;
	widx = gb->widx1 & (BUF1_SIZE - 1);
	tmp = BUF1_SIZE - widx;
	if (tmp >= num) {
		if (copy_from_user(&gb->buf1[widx], buf, num))
			return -EFAULT;
	} else {
		if (copy_from_user(&gb->buf1[widx], buf, tmp))
			return -EFAULT;
		if (copy_from_user(&gb->buf1[0], buf + tmp, num - tmp))
			return -EFAULT;
	}
	spin_lock_irqsave(&gb->slock1, flags);
	gb->widx1 += num;
	spin_unlock_irqrestore(&gb->slock1, flags);
	wake_up_interruptible(&gb->wait1);
	return num;
}

/* called from assistservice to read from buf1 */
static ssize_t gps0_read(struct file *file, char *buf, size_t count,
				loff_t *offset)
{
	int num, ridx, tmp, ret, copied = 0;
	unsigned long flags;

	if (gb->widx1 > 32768 && gb->ridx1 > 32768) {
		spin_lock_irqsave(&gb->slock1, flags);
		gb->widx1 -= 32768;
		gb->ridx1 -= 32768;
		spin_unlock_irqrestore(&gb->slock1, flags);
	}
	while (count) {
		num = gb->widx1 - gb->ridx1;
		if (num) {
			if (num > count)
				num = count;
			ridx = gb->ridx1 & (BUF1_SIZE - 1);
			tmp = BUF1_SIZE - ridx;
			if (tmp >= num) {
				if (copy_to_user(buf, &gb->buf1[ridx], num))
					return -EFAULT;
			} else {
				if (copy_to_user(buf, &gb->buf1[ridx], tmp))
					return -EFAULT;
				if (copy_to_user(buf + tmp, &gb->buf1[0], num - tmp))
					return -EFAULT;
			}
			buf += num;
			spin_lock_irqsave(&gb->slock1, flags);
			gb->ridx1 += num;
			spin_unlock_irqrestore(&gb->slock1, flags);
			wake_up_interruptible(&gb->wait1);
			count -= num;
			copied += num;
		} else {
			ret = wait_event_interruptible(gb->wait1, gb->widx1 - gb->ridx1);
			if (ret < 0)
				return ret;
		}
	}
	return copied;
}

static unsigned int gps1_poll(struct file *file, poll_table *wait)
{
	int num;
	unsigned int mask = 0;

	poll_wait(file, &gb->wait0, wait);
	poll_wait(file, &gb->wait1, wait);

	num = gb->widx0 - gb->ridx0;
	if (num)
		mask |= POLLIN | POLLRDNORM;
	num = BUF1_SIZE - (gb->widx1 - gb->ridx1);
	if (num)
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}

static int gps0_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int gps1_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int gps0_close(struct inode *inode, struct file *file)
{
	return 0;
}

static int gps1_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations gps1_fops = {
	.owner = THIS_MODULE,
	.open = gps1_open,
	.release = gps1_close,
	.read = gps1_read,
	.write = gps1_write,
	.poll = gps1_poll,
};

static const struct file_operations gps0_fops = {
	.owner = THIS_MODULE,
	.open = gps0_open,
	.release = gps0_close,
	.read = gps0_read,
	.write = gps0_write,
};

static struct class *gps_class;
static struct cdev gps_cdev0, gps_cdev1;

static int medaka_gps_probe(struct platform_device *pdev)
{
	int ret, irq;
	struct gps_info *info;
	dev_t devno;
	struct device *device;

	pr_info("%s\n", __func__);

	ret = alloc_chrdev_region(&devno, 0, 2, "medaka_gps");
	if (ret < 0) {
		pr_err("%s: alloc_chrdev_region failed %d\n", __func__, ret);
		return ret;
	}
	gps_class = class_create(THIS_MODULE, "medaka_gps");
	if (IS_ERR(gps_class)) {
		ret = PTR_ERR(gps_class);
		pr_err("%s: class_create failed %d\n", __func__, ret);
		return ret;
	}

	/* gps0 */
	cdev_init(&gps_cdev0, &gps0_fops);
	gps_cdev0.owner = THIS_MODULE;
	ret = cdev_add(&gps_cdev0, MKDEV(MAJOR(devno),0), 1);
	if (ret < 0) {
		pr_err("%s: cdev_add failed %d\n", __func__, ret);
		return ret;
	}
	device = device_create(gps_class, NULL, MKDEV(MAJOR(devno),0), NULL, "gps0");
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("%s: device_create failed %d\n", __func__, ret);
		return ret;
	}
	/* gps1 */
	cdev_init(&gps_cdev1, &gps1_fops);
	gps_cdev1.owner = THIS_MODULE;
	ret = cdev_add(&gps_cdev1, MKDEV(MAJOR(devno),1), 1);
	if (ret < 0) {
		pr_err("%s: cdev_add failed %d\n", __func__, ret);
		return ret;
	}
	device = device_create(gps_class, NULL, MKDEV(MAJOR(devno),1), NULL, "gps1");
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("%s: device_create failed %d\n", __func__, ret);
		return ret;
	}
	gb = devm_kzalloc(&pdev->dev, sizeof(struct gps_buffer), GFP_KERNEL);
	if (!gb)
		return -ENOMEM;

	init_waitqueue_head(&gb->wait0);
	init_waitqueue_head(&gb->wait1);
	spin_lock_init(&gb->slock0);
	spin_lock_init(&gb->slock1);

	/* host_wake */
	info = devm_kzalloc(&pdev->dev, sizeof(struct gps_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->nr_gpio = of_get_named_gpio(pdev->dev.of_node,	"sub-main-int2", 0);
	pr_info("gps nr_gpio = %d\n", info->nr_gpio);
	ret = gpio_request(info->nr_gpio, "sub_main_int2");
	if (unlikely(ret)) {
		return ret;
	}

	wake_lock_init(&info->wakelock, WAKE_LOCK_SUSPEND, "medaka_gps");

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

static struct of_device_id medaka_gps_of_match[] = {
	{ .compatible = "medaka-gps", },
	{ },
};

static struct platform_driver medaka_gps_platform_driver = {
	.probe = medaka_gps_probe,
	.driver = {
		.name = "medaka-gps",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(medaka_gps_of_match),
	},
};

module_platform_driver(medaka_gps_platform_driver);

MODULE_ALIAS("platform:medaka-gps");
MODULE_DESCRIPTION("medaka_gps");
MODULE_AUTHOR("casio");
MODULE_LICENSE("GPL");
