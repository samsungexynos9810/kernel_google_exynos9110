#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/scatterlist.h>
#include <linux/completion.h>
#include <linux/dma-direction.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>

#include <asm/cacheflush.h>

#include "exynos-iommu.h"

static int smmutst_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int smmutst_release(struct inode *inode, struct file *file)
{
	return 0;
}


struct smmutst_data {
	size_t size;
	size_t max_page_size; /* one of 1m, 64k and 4k */
	int cpu;
	long long latency;
};

struct kernel_smmutst_data {
	struct smmutst_data data;
	struct device *dev;
	struct sg_table sgt;
	struct completion done;
	int result;
};

#define SMMUTST_IOC_MAP		_IOWR('S', 0, struct smmutst_data)

const static unsigned int page_orders[3] = {20, 16, 12};
#define page_size(n) (1 << page_orders[(n)])

static int smmutst_thread(void *p)
{
	struct kernel_smmutst_data *data = p;
	dma_addr_t dva;
	ktime_t time1, time2;

	data->result = 0;

	time1 = ktime_get();

	dva = iovmm_map(data->dev, data->sgt.sgl, 0, data->data.size, DMA_TO_DEVICE, 0);

	time2 = ktime_get();

	if (IS_ERR_VALUE(dva)) {
		data->result = (int)dva;
		pr_err("smmutst: failed to iovmm_map:%d\n", data->result);
		goto err;
	}

	data->data.latency = ktime_to_ns(ktime_sub(time2, time1));

	iovmm_unmap(data->dev, dva);

err:
	complete(&data->done);
	return 0;
}

static long smmutst_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct kernel_smmutst_data data;
	struct task_struct *thread;
	struct scatterlist *sg;
	struct page *page;
	unsigned int nents[3] = {0, 0, 0};
	size_t size;
	int i;
	int ret = 0;

	if (cmd != SMMUTST_IOC_MAP) {
		pr_err("smmutst: unknown cmd %#x\n", cmd);
		return -EINVAL;
	}

	if (copy_from_user(&data.data, (void __user *)arg, sizeof(data.data))) {
		pr_err("smmutst: failed to copy data from user\n");
		return -EFAULT;
	}

	data.dev = ((struct miscdevice *)filp->private_data)->this_device;
	size = PAGE_ALIGN(data.data.size);
	for (i = 0; i < ARRAY_SIZE(page_orders); i++) {
		if (data.data.max_page_size >= page_size(i)) {
			nents[i] = size >> page_orders[i];
			size -= (nents[i] << page_orders[i]);
		}
	}

	if (size != 0) {
		pr_err("smmutst: check size %#zx/%#zx, pagesize %#zx\n", size, data.data.size, data.data.max_page_size);
		return -EINVAL;
	}

	ret = sg_alloc_table(&data.sgt, nents[0] + nents[1] + nents[2], GFP_KERNEL);
	if (ret) {
		pr_err("smmutst: failed to allocate sgtable\n");
		return ret;
	}

	page = virt_to_page(PAGE_OFFSET);

	sg = data.sgt.sgl;
	for (i = 0; i < ARRAY_SIZE(page_orders); i++) {
		unsigned int n;
		phys_addr_t phys;

		for (n = 0; n < nents[i]; n++) {
			phys = page_to_phys(page);
			phys += 1 << page_orders[i];
			page = phys_to_page(phys);

			sg_set_page(sg, page, nents[i] << page_orders[i], 0);
			sg = sg_next(sg);

			phys = page_to_phys(page);
			phys += 1 << page_orders[i];
			page = phys_to_page(phys);
		}
	}

	init_completion(&data.done);
	data.result = 0;

	thread = kthread_create_on_cpu(smmutst_thread, &data,
					data.data.cpu, "smmutst_thread");
	if (IS_ERR(thread)) {
		pr_err("smmutst: failed to create kernel thread on CPU %d\n",
			data.data.cpu);
		ret = PTR_ERR(thread);
		goto err_thread;
	}

	kthread_unpark(thread);

	wait_for_completion_interruptible(&data.done);
	if (data.result != 0) {
		pr_err("smmutst: failed to evaluate size %#zx on CPU %d\n",
				data.data.size, data.data.cpu);
		ret = data.result;
	} else {
		if (copy_to_user((void __user *)arg, &data.data, sizeof(data.data))) {
			pr_err("smmutst: failed to copy data from user\n");
			ret = -EFAULT;
		}
	}


	if (copy_to_user((void __user *)arg, &data.data, sizeof(data.data))) {
		pr_err("smmutst: failed to copy data from user\n");
		ret = -EFAULT;
	}

err_thread:
	sg_free_table(&data.sgt);
	return ret;
}

static const struct file_operations smmutst_fops = {
	.owner          = THIS_MODULE,
	.open           = smmutst_open,
	.release        = smmutst_release,
	.unlocked_ioctl = smmutst_ioctl,
};

static struct miscdevice smmutst = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "smmutst",
	.fops = &smmutst_fops,
};

static struct exynos_iommu_owner smmutst_owner;
static int __init smmutst_init(void)
{
	int ret;

	ret = misc_register(&smmutst);
	if (ret) {
		pr_err("smmutst: failed to register smmutst(%d)\n", ret);
		return ret;
	}

	INIT_LIST_HEAD(&smmutst_owner.mmu_list);
	INIT_LIST_HEAD(&smmutst_owner.client);
	smmutst_owner.dev = smmutst.this_device;
	spin_lock_init(&smmutst_owner.lock);

	smmutst.this_device->archdata.iommu = &smmutst_owner;

	ret = exynos_create_iovmm(smmutst.this_device, 1, 1);
	if (ret) {
		pr_err("%s: failed to create IOVMM\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

module_init(smmutst_init);
