/*
 * WiFi driver for Medaka board
 *
 * Copyright (C) 2013 LGE, Inc
 * Copyright (C) 2017 CASIO Computer Co.,Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "board-medaka-rf.h"
#include <linux/kernel.h>
#include <linux/if.h>
#include <linux/random.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/fs.h>
#include <asm/io.h>

#define WLAN_STATIC_SCAN_BUF0           5
#define WLAN_STATIC_SCAN_BUF1           6
#define PREALLOC_WLAN_SEC_NUM           4
#define PREALLOC_WLAN_BUF_NUM           160
#define PREALLOC_WLAN_SECTION_HEADER    24

#define WLAN_SECTION_SIZE_0     (PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1     (PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2     (PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3     (PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE                 336
#define DHD_SKB_1PAGE_BUFSIZE   ((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE   ((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE   ((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM        17

static int dbg_level = DBG_INFO;
module_param(dbg_level, int, 0644);
MODULE_PARM_DESC(dbg_level, "set debug level");

#define dbg_err(fmt, ...) do { pr_err(fmt, ## __VA_ARGS__); } while(0)
#define dbg_info(fmt, ...) do { if (dbg_level & DBG_INFO) pr_info(fmt, ## __VA_ARGS__); } while(0)
#define dbg_trace(fmt, ...) do { if (dbg_level & DBG_TRACE) pr_info(fmt, ## __VA_ARGS__); } while(0)
#define dbg_verbose(fmt, ...) do { if (dbg_level & DBG_VERBOSE) pr_info(fmt, ## __VA_ARGS__); } while(0)

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

static void *wlan_static_scan_buf0 = NULL;
static void *wlan_static_scan_buf1 = NULL;

static void *bcm_wifi_mem_prealloc(int section, unsigned long size)
{
	dbg_trace("%s section:%d size %lu\n", __func__, section, size);

	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;
	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;
	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int bcm_init_wlan_mem(void)
{
	int i;

	for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
		wlan_static_skb[i] = NULL;
	}

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wlan_mem_array[i].mem_ptr =
			kmalloc(wlan_mem_array[i].size, GFP_KERNEL);
		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}

	wlan_static_scan_buf0 = kmalloc(65536, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;

	wlan_static_scan_buf1 = kmalloc(65536, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_static_scan_buf;

	pr_info("wifi: %s: WIFI MEM Allocated\n", __func__);
	return 0;

err_static_scan_buf:
	pr_err("%s: failed to allocate scan_buf0\n", __func__);
	kfree(wlan_static_scan_buf0);
	wlan_static_scan_buf0 = NULL;

err_mem_alloc:
	pr_err("%s: failed to allocate mem_alloc\n", __func__);
	for (; i >= 0 ; i--) {
		kfree(wlan_mem_array[i].mem_ptr);
		wlan_mem_array[i].mem_ptr = NULL;
	}

	i = WLAN_SKB_BUF_NUM;
err_skb_alloc:
	pr_err("%s: failed to allocate skb_alloc\n", __func__);
	for (; i >= 0 ; i--) {
		dev_kfree_skb(wlan_static_skb[i]);
		wlan_static_skb[i] = NULL;
	}

	return -ENOMEM;
}

extern void (*wifi_status_cb) (struct platform_device *dev, int state);
extern struct platform_device *wifi_status_cb_devid;
int wcf_status_register(void (*cb)(struct platform_device *dev, int state),  void *dev)
{
	pr_info("%s\n", __func__);

	if (wifi_status_cb)
		return -EBUSY;

	wifi_status_cb = cb;

	return 0;
}

static int __init bcm_wifi_init_gpio_mem(struct platform_device *pdev)
{
	int rc = 0;

	/* WLAN_POWER */
	rc = gpio_request_one(WLAN_RESET_GPIO, GPIOF_OUT_INIT_LOW, "WL_REG_ON");
	if (rc < 0) {
		pr_err("%s: Failed to request gpio %d for WL_REG_ON\n",
                                __func__, WLAN_RESET_GPIO);
                goto err_gpio_wifi_power;
	}

	/* HOST_WAKEUP */
	rc = gpio_request_one(WLAN_HOST_WAKE_GPIO, GPIOF_IN, "wlan_wake");
	if (rc < 0) {
			pr_err("%s: Failed to request gpio %d for wlan_wake\n",
							__func__, WLAN_HOST_WAKE_GPIO);
			goto err_gpio_tlmm_wlan_wake;
	}

	if (pdev) {
		struct resource *resource = pdev->resource;
		if (resource) {
			resource->start = gpio_to_irq(WLAN_HOST_WAKE_GPIO);
			resource->end = gpio_to_irq(WLAN_HOST_WAKE_GPIO);
		}
	}

	if (bcm_init_wlan_mem() < 0)
		goto err_alloc_wifi_mem_array;

	pr_info("%s: wifi gpio and mem initialized\n", __func__);
	return 0;

err_alloc_wifi_mem_array:
	gpio_free(WLAN_HOST_WAKE_GPIO);
err_gpio_tlmm_wlan_wake:
	gpio_free(WLAN_RESET_GPIO);
err_gpio_wifi_power:
	return rc;
}

int bcm_wifi_set_power(int enable)
{
	int ret = 0;

	if (enable) {
		ret = gpio_direction_output(WLAN_RESET_GPIO, POWER_ON);
		if (ret) {
			pr_err("%s: WL_REG_ON  failed to pull up (%d)\n",
					__func__, ret);
			goto err;
		}
		/* WLAN chip to reset */
		msleep(200); /* wait at least 150ms before transmitting */

		pr_info("%s: wifi power successed to pull up\n", __func__);
	} else { // OFF
		ret = gpio_direction_output(WLAN_RESET_GPIO, POWER_OFF);
		if (ret) {
			pr_err("%s:  WL_REG_ON  failed to pull down (%d)\n",
					__func__, ret);
			goto err;
		}

		/* WLAN chip down */
		msleep(100);
		pr_info("%s: wifi power successed to pull down\n", __func__);
	}

err:
	return ret;
}

static int bcm_wifi_reset(int on)
{
	// Nothing to do
	return 0;
}

static int bcm_wifi_carddetect(int val)
{
	pr_info("%s\n",__func__);
	if (wifi_status_cb) {
		pr_info("%s: wifi_status_cb\n",__func__);
		wifi_status_cb(wifi_status_cb_devid, val);
	}
	else
		pr_warn("%s: There is no callback for notify\n", __func__);
	return 0;
}

#define ETHER_ADDR_LEN    6
#define FILE_WIFI_MACADDR "/persist/wifi/.macaddr"

static inline int xdigit (char c)
{
	unsigned d;

	d = (unsigned)(c-'0');
	if (d < 10)
		return (int)d;
	d = (unsigned)(c-'a');
	if (d < 6)
		return (int)(10+d);
	d = (unsigned)(c-'A');
	if (d < 6)
		return (int)(10+d);
	return -1;
}

struct ether_addr {
	unsigned char ether_addr_octet[ETHER_ADDR_LEN];
} __attribute__((__packed__));

struct ether_addr *
ether_aton_r (const char *asc, struct ether_addr * addr)
{
	int i, val0, val1;

	for (i = 0; i < ETHER_ADDR_LEN; ++i) {
		val0 = xdigit(*asc);
		asc++;
		if (val0 < 0)
			return NULL;

		val1 = xdigit(*asc);
		asc++;
		if (val1 < 0)
			return NULL;

		addr->ether_addr_octet[i] = (unsigned char)((val0 << 4) + val1);

		if (i < ETHER_ADDR_LEN - 1) {
			if (*asc != ':')
				return NULL;
			asc++;
		}
	}

	if (*asc != '\0')
		return NULL;

	return addr;
}

struct ether_addr * ether_aton (const char *asc)
{
	static struct ether_addr addr;
	return ether_aton_r(asc, &addr);
}

static int bcm_wifi_get_mac_addr(unsigned char *buf)
{
	int ret = 0;

	mm_segment_t oldfs;
	struct kstat stat;
	struct file* fp;
	int readlen = 0;
	char macread[128] = {0,};
	uint rand_mac;
	static unsigned char mymac[ETHER_ADDR_LEN] = {0,};
	const unsigned char nullmac[ETHER_ADDR_LEN] = {0,};
	const unsigned char bcastmac[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

	if (buf == NULL)
		return -EAGAIN;

	memset(buf, 0x00, ETHER_ADDR_LEN);

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_stat(FILE_WIFI_MACADDR, &stat);
	if (ret) {
		set_fs(oldfs);
		pr_err("%s: Failed to get information from file %s (%d)\n",
				__FUNCTION__, FILE_WIFI_MACADDR, ret);
		goto random_mac;
	}
	set_fs(oldfs);

	fp = filp_open(FILE_WIFI_MACADDR, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: Failed to read error %s\n",
				__FUNCTION__, FILE_WIFI_MACADDR);
		goto random_mac;
	}

#ifdef WIFI_MAC_FORMAT_ASCII
	readlen = kernel_read(fp, fp->f_pos, macread, 17); // 17 = 12 + 5
#else
	readlen = kernel_read(fp, fp->f_pos, macread, 6);
#endif
	if (readlen > 0) {
		unsigned char* macbin;
#ifdef WIFI_MAC_FORMAT_ASCII
		struct ether_addr* convmac = ether_aton( macread );

		if (convmac == NULL) {
			pr_err("%s: Invalid Mac Address Format %s\n",
					__FUNCTION__, macread );
			goto random_mac;
		}

		macbin = convmac->ether_addr_octet;
#else
		macbin = (unsigned char*)macread;
#endif
		pr_info("%s: READ MAC ADDRESS %02X:%02X:%02X:%02X:%02X:%02X\n",
				__FUNCTION__,
				macbin[0], macbin[1], macbin[2],
				macbin[3], macbin[4], macbin[5]);

		if (memcmp(macbin, nullmac, ETHER_ADDR_LEN) == 0 ||
				memcmp(macbin, bcastmac, ETHER_ADDR_LEN) == 0) {
			filp_close(fp, NULL);
			goto random_mac;
		}
		memcpy(buf, macbin, ETHER_ADDR_LEN);
	} else {
		filp_close(fp, NULL);
		goto random_mac;
	}

	filp_close(fp, NULL);
	return ret;

random_mac:

	pr_debug("%s: %p\n", __func__, buf);

	if (memcmp( mymac, nullmac, ETHER_ADDR_LEN) != 0) {
		/* Mac displayed from UI is never updated..
		   So, mac obtained on initial time is used */
		memcpy(buf, mymac, ETHER_ADDR_LEN);
		return 0;
	}

	prandom_seed((uint)jiffies);
	rand_mac = prandom_u32();
	buf[0] = 0x00;
	buf[1] = 0x90;
	buf[2] = 0x4c;
	buf[3] = (unsigned char)rand_mac;
	buf[4] = (unsigned char)(rand_mac >> 8);
	buf[5] = (unsigned char)(rand_mac >> 16);

	memcpy(mymac, buf, 6);

	pr_info("[%s] Exiting. MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
			__FUNCTION__,
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5] );

	return 0;
}

static struct wifi_platform_data bcm_wifi_control = {
	.mem_prealloc	= bcm_wifi_mem_prealloc,
	.set_power	= bcm_wifi_set_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_carddetect,
	.get_mac_addr   = bcm_wifi_get_mac_addr,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name = "bcmdhd_wlan_irq",
		.start = 0,  //assigned later
		.end   = 0,  //assigned later
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE, // for HW_OOB
	},
};

static struct platform_device bcm_wifi_device = {
	.name           = "bcmdhd_wlan",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(wifi_resource),
	.resource       = wifi_resource,
	.dev            = {
		.platform_data = &bcm_wifi_control,
	},
};

int __init init_bcm_wifi(void)
{
	bcm_wifi_init_gpio_mem(&bcm_wifi_device);
	platform_device_register(&bcm_wifi_device);

	return 0;
}

device_initcall(init_bcm_wifi);

MODULE_DESCRIPTION("CYW4343W WiFi Driver for Medaka board");
MODULE_LICENSE("GPL");
