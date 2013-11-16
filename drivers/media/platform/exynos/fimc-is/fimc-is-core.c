/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <mach/regs-clock.h>
#include <linux/pm_qos.h>
#include <linux/bug.h>
#include <linux/v4l2-mediabus.h>
#include <linux/exynos_iovmm.h>
#include <mach/devfreq.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-clk-gate.h"

#include "sensor/fimc-is-device-6b2.h"
#include "sensor/fimc-is-device-8b1.h"
#include "sensor/fimc-is-device-imx135.h"
#include "sensor/fimc-is-device-3l2.h"
#include "sensor/fimc-is-device-2p2.h"

#ifdef USE_OWN_FAULT_HANDLER
#include <plat/sysmmu.h>
#endif

#if defined(CONFIG_ARM_EXYNOS5260_BUS_DEVFREQ)
#define CONFIG_FIMC_IS_BUS_DEVFREQ
#endif
#if defined(CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ)
#define CONFIG_FIMC_IS_BUS_DEVFREQ
#endif
#if defined(CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ)
#define CONFIG_FIMC_IS_BUS_DEVFREQ
#endif

struct fimc_is_from_info *sysfs_finfo = NULL;
struct fimc_is_from_info *sysfs_pinfo = NULL;
struct device *fimc_is_dev = NULL;

extern struct pm_qos_request exynos_isp_qos_dev;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;
extern struct pm_qos_request exynos_isp_qos_disp;

extern int fimc_is_3a0_video_probe(void *data);
extern int fimc_is_3a1_video_probe(void *data);
extern int fimc_is_isp_video_probe(void *data);
extern int fimc_is_scc_video_probe(void *data);
extern int fimc_is_scp_video_probe(void *data);
extern int fimc_is_vdc_video_probe(void *data);
extern int fimc_is_vdo_video_probe(void *data);
extern int fimc_is_3a0c_video_probe(void *data);
extern int fimc_is_3a1c_video_probe(void *data);

/* sysfs global variable for debug */
struct fimc_is_sysfs_debug sysfs_debug;

static int fimc_is_ischain_allocmem(struct fimc_is_core *this)
{
	int ret = 0;
	void *fw_cookie;

	dbg_core("Allocating memory for FIMC-IS firmware.\n");

	fw_cookie = vb2_ion_private_alloc(this->mem.alloc_ctx,
				FIMC_IS_A5_MEM_SIZE +
#ifdef ENABLE_ODC
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
#endif
#ifdef ENABLE_VDIS
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
#endif
#ifdef ENABLE_TDNR
				SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF +
#endif
#if defined(CONFIG_ARCH_EXYNOS4)
				0);
#else
				0, 1, 0);
#endif

	if (IS_ERR(fw_cookie)) {
		err("Allocating bitprocessor buffer failed");
		fw_cookie = NULL;
		ret = -ENOMEM;
		goto exit;
	}

	ret = vb2_ion_dma_address(fw_cookie, &this->minfo.dvaddr);
	if ((ret < 0) || (this->minfo.dvaddr  & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.");
		vb2_ion_private_free(fw_cookie);
		this->minfo.dvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto exit;
	}
	dbg_core("Device vaddr = %08x , size = %08x\n",
		this->minfo.dvaddr, FIMC_IS_A5_MEM_SIZE);

	this->minfo.kvaddr = (u32)vb2_ion_private_vaddr(fw_cookie);
	if (IS_ERR((void *)this->minfo.kvaddr)) {
		err("Bitprocessor memory remap failed");
		vb2_ion_private_free(fw_cookie);
		this->minfo.kvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto exit;
	}

exit:
	dbg_core("Virtual address for FW: %08lx\n",
		(long unsigned int)this->minfo.kvaddr);
	this->minfo.fw_cookie = fw_cookie;

	return ret;
}

static int fimc_is_ishcain_initmem(struct fimc_is_core *this)
{
	int ret = 0;
	u32 offset;

	dbg_core("fimc_is_init_mem - ION\n");

	ret = fimc_is_ischain_allocmem(this);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		ret = -ENOMEM;
		goto exit;
	}

	offset = FW_SHARED_OFFSET;
	this->minfo.dvaddr_fshared = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_fshared = this->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE;
	this->minfo.dvaddr_region = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_region = this->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE;
#ifdef ENABLE_ODC
	this->minfo.dvaddr_odc = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_odc = this->minfo.kvaddr + offset;
	offset += (SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
#else
	this->minfo.dvaddr_odc = 0;
	this->minfo.kvaddr_odc = 0;
#endif

#ifdef ENABLE_VDIS
	this->minfo.dvaddr_dis = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_dis = this->minfo.kvaddr + offset;
	offset += (SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
#else
	this->minfo.dvaddr_dis = 0;
	this->minfo.kvaddr_dis = 0;
#endif

#ifdef ENABLE_TDNR
	this->minfo.dvaddr_3dnr = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_3dnr = this->minfo.kvaddr + offset;
	offset += (SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF);
#else
	this->minfo.dvaddr_3dnr = 0;
	this->minfo.kvaddr_3dnr = 0;
#endif

	dbg_core("fimc_is_init_mem done\n");

exit:
	return ret;
}

#if defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ)
static int exynos5_fimc_mif_notifier(struct notifier_block *notifier,
	unsigned long event, void *v)
{
	struct devfreq_info *info = (struct devfreq_info *)v;
	int cfg;
	int timeout = 1000000;

	switch (event) {
	case MIF_DEVFREQ_PRECHANGE:
		if (info->new == 800000) {
			spin_lock(&int_div_lock);
			cfg = __raw_readl(EXYNOS5_CLKDIV_TOP1);
			cfg &= ~(0x7 << 20);
			cfg |= (0x1 << 20);
			__raw_writel(cfg, EXYNOS5_CLKDIV_TOP1);
			do {
				timeout--;
			} while(timeout &&
				(__raw_readl(EXYNOS5_CLKDIV_STAT_TOP1) &
				(0x1 << 20)));
			if (!timeout)
				pr_err("%s: timeout for clock diving #1\n",
					__func__);
			spin_unlock(&int_div_lock);
		}
		break;
	case MIF_DEVFREQ_POSTCHANGE:
               if (info->new == 400000) {
                        if ((__raw_readl(EXYNOS5_BPLL_CON0) & 0x7) == 2) {
				udelay(500);
				spin_lock(&int_div_lock);
                                cfg = __raw_readl(EXYNOS5_CLKDIV_TOP1);
                                cfg &= ~(0x7 << 20);
                                cfg |= (0x0 << 20);
                                __raw_writel(cfg, EXYNOS5_CLKDIV_TOP1);
                                do {
                                        timeout--;
                                } while(timeout &&
                                        (__raw_readl(EXYNOS5_CLKDIV_STAT_TOP1) &
                                        (0x1 << 20)));
				if (!timeout)
					pr_err("%s: timeout for clock "
						"diving #1\n",__func__);
				spin_unlock(&int_div_lock);
                        } else {
				pr_err("%s: CRITCAL error\n",__func__);
	                }
                }
		break;
	default:
		pr_err("%s: No scenario for %d to %d\n", __func__, info->old, info->new);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_mif_nb = {
	.notifier_call = exynos5_fimc_mif_notifier,
};
#endif

static int fimc_is_suspend(struct device *dev)
{
	pr_debug("FIMC_IS Suspend\n");
	return 0;
}

static int fimc_is_resume(struct device *dev)
{
	pr_debug("FIMC_IS Resume\n");
	return 0;
}

int fimc_is_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(pdev);

	pr_info("FIMC_IS runtime suspend in\n");

#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_detach_iommu(core->mem.alloc_ctx);
#endif

#if defined(CONFIG_FIMC_IS_BUS_DEVFREQ)
	 exynos5_update_media_layers(TYPE_FIMC_LITE, false);
#endif

#if defined(CONFIG_MACH_SMDK5410) || defined(CONFIG_MACH_SMDK5420)
	/* Sensor power off */
        /* TODO : need close_sensor */
	if (core->pdata->cfg_gpio) {
		core->pdata->cfg_gpio(core->pdev, 0, false);
		core->pdata->cfg_gpio(core->pdev, 2, false);
	} else {
		err("failed to sensor_power_on\n");
		ret = -EINVAL;
		goto p_err;
	}
#endif

	if (core->pdata->clk_off) {
		core->pdata->clk_off(core->pdev);
	} else {
		err("clk_off is fail\n");
		ret = -EINVAL;
		goto p_err;
	}
	pr_info("FIMC_IS runtime suspend out\n");

p_err:
	pm_relax(dev);
	return ret;
}

int fimc_is_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(pdev);
#if defined(CONFIG_PM_DEVFREQ)
	int int_qos, mif_qos, cam_qos, disp_qos;
#endif

	pm_stay_awake(dev);
	pr_info("FIMC_IS runtime resume in\n");

	/* HACK: DVFS lock sequence is change.
	 * DVFS level should be locked after power on.
	 */
#if defined(CONFIG_PM_DEVFREQ)
	int_qos = 667000;
	mif_qos = 800000;
	cam_qos = 666000;
	disp_qos = 333000;
	pm_qos_add_request(&exynos_isp_qos_dev, PM_QOS_DEVICE_THROUGHPUT, int_qos);
	pm_qos_add_request(&exynos_isp_qos_mem, PM_QOS_BUS_THROUGHPUT, mif_qos);
	pm_qos_add_request(&exynos_isp_qos_cam, PM_QOS_CAM_THROUGHPUT, cam_qos);
	pm_qos_add_request(&exynos_isp_qos_disp, PM_QOS_DISPLAY_THROUGHPUT, disp_qos);

	pr_info("[RSC] %s: DVFS LOCK(int(%d), mif(%d), cam(%d), disp(%d))\n",
		__func__, int_qos, mif_qos, cam_qos, disp_qos);
#endif

	/* Low clock setting */
	if (core->pdata->clk_cfg) {
		core->pdata->clk_cfg(core->pdev);
	} else {
		err("clk_cfg is fail\n");
		ret = -EINVAL;
		goto p_err;
	}

#if defined(CONFIG_PM_DEVFREQ)
	pm_qos_update_request(&exynos_isp_qos_dev, int_qos);
	pm_qos_update_request(&exynos_isp_qos_mem, mif_qos);
	pm_qos_update_request(&exynos_isp_qos_cam, cam_qos);
	pm_qos_update_request(&exynos_isp_qos_disp, disp_qos);
#endif
	/* Clock on */
	if (core->pdata->clk_on) {
		core->pdata->clk_on(core->pdev);
	} else {
		err("clk_on is fail\n");
		ret = -EINVAL;
		goto p_err;
	}

#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_attach_iommu(core->mem.alloc_ctx);
#endif

#if defined(CONFIG_FIMC_IS_BUS_DEVFREQ)
	exynos5_update_media_layers(TYPE_FIMC_LITE, true);
#endif

	pr_info("FIMC-IS runtime resume out\n");

	return 0;

p_err:
	pm_relax(dev);
	return ret;
}

#ifdef USE_OWN_FAULT_HANDLER
#define SECT_ORDER 20
#define LPAGE_ORDER 16
#define SPAGE_ORDER 12

#define lv1ent_page(sent) ((*(sent) & 3) == 1)

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & 0xFF000) >> SPAGE_ORDER)
#define lv2table_base(sent) (*(sent) & 0xFFFFFC00)

static unsigned long *section_entry(unsigned long *pgtable, unsigned long iova)
{
	return pgtable + lv1ent_offset(iova);
}

static unsigned long *page_entry(unsigned long *sent, unsigned long iova)
{
	return (unsigned long *)__va(lv2table_base(sent)) + lv2ent_offset(iova);
}

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNKNOWN FAULT"
};

static int fimc_is_fault_handler(struct device *dev, const char *mmuname,
					enum exynos_sysmmu_inttype itype,
					unsigned long pgtable_base,
					unsigned long fault_addr)
{
	u32 i;
	unsigned long *ent;
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_framemgr *framemgr;

	if ((itype >= SYSMMU_FAULTS_NUM) || (itype < SYSMMU_PAGEFAULT))
		itype = SYSMMU_FAULT_UNKNOWN;

	pr_err("%s occured at 0x%lx by '%s'(Page table base: 0x%lx)\n",
		sysmmu_fault_name[itype], fault_addr, mmuname, pgtable_base);

	ent = section_entry(__va(pgtable_base), fault_addr);
	pr_err("\tLv1 entry: 0x%lx\n", *ent);

	if (lv1ent_page(ent)) {
		ent = page_entry(ent, fault_addr);
		pr_err("\t Lv2 entry: 0x%lx\n", *ent);
	}

	pr_err("Generating Kernel OOPS... because it is unrecoverable.\n");

	core = dev_get_drvdata(dev);
	if (!core)
		pr_err("core is NULL\n");

	fimc_is_hw_logdump(&core->interface);
	fimc_is_hw_memdump(&core->interface,
		core->minfo.kvaddr + 0x010F8000 /* TTB_BASE ~ 16KB */,
		core->minfo.kvaddr + 0x010F8000 + 0x4000);
	fimc_is_hw_memdump(&core->interface,
		core->minfo.kvaddr + 0x010FC000 /* GUARD2_BASE ~ 16KB */,
		core->minfo.kvaddr + 0x010FC000 + 0x4000);

	sensor = &core->sensor[0];
	if (test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state)) {
		framemgr = &sensor->vctx->q_dst.framemgr;
		for (i = 0; i < FRAMEMGR_MAX_REQUEST; ++i) {
			pr_err("LITE0 BUF[%d][0] = %d, 0x%08X\n", i,
				framemgr->frame[i].memory, framemgr->frame[i].dvaddr_buffer[0]);
		}
	}

	sensor = &core->sensor[1];
	if (test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state)) {
		framemgr = &sensor->vctx->q_dst.framemgr;
		for (i = 0; i < FRAMEMGR_MAX_REQUEST; ++i) {
			pr_err("LITE1 BUF[%d][0] = %d, 0x%08X\n", i,
				framemgr->frame[i].memory, framemgr->frame[i].dvaddr_buffer[0]);
		}
	}

	BUG();

	return 0;
}
#endif

static ssize_t show_clk_gate_mode(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.clk_gate_mode);
}

static ssize_t store_clk_gate_mode(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef HAS_FW_CLOCK_GATE
	unsigned int value;
	int ret;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		goto out;

	if (value > 0)
		sysfs_debug.clk_gate_mode = 1;
	else
		sysfs_debug.clk_gate_mode = 0;
out:
#endif
	return count;
}

static ssize_t show_en_clk_gate(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.en_clk_gate);
}

static ssize_t store_en_clk_gate(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef ENABLE_CLOCK_GATE
	unsigned int value;
	int ret;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		goto out;

	if (value > 0) {
		sysfs_debug.en_clk_gate = 1;
		sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE;
	} else {
		sysfs_debug.en_clk_gate = 0;
		sysfs_debug.clk_gate_mode = 0;
	}
out:
#endif
	return count;
}

static ssize_t show_en_dvfs(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.en_dvfs);
}

static ssize_t store_en_dvfs(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef ENABLE_DVFS
	struct fimc_is_core *core =
		(struct fimc_is_core *)platform_get_drvdata(to_platform_device(dev));
	struct fimc_is_resourcemgr *resourcemgr;
	unsigned int value;
	int i, ret;

	BUG_ON(!core);

	resourcemgr = &core->resourcemgr;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		goto out;

	if (value > 0) {
		/* It can not re-define static scenario */
		sysfs_debug.en_dvfs = value;
	} else {
		sysfs_debug.en_dvfs = 0;
		/* update dvfs lever to max */
		mutex_lock(&resourcemgr->dvfs_ctrl.lock);
		sysfs_debug.en_dvfs = value;
		for (i = 0; i < FIMC_IS_MAX_NODES; i++) {
			if (test_bit(FIMC_IS_ISCHAIN_OPEN, &((core->ischain[i]).state)))
				fimc_is_set_dvfs(&(core->ischain[i]), FIMC_IS_SN_MAX);
		}
		fimc_is_dvfs_init(resourcemgr);
		resourcemgr->dvfs_ctrl.static_ctrl->cur_scenario_id = FIMC_IS_SN_MAX;
		mutex_unlock(&resourcemgr->dvfs_ctrl.lock);
	}
out:
#endif
	return count;
}

static DEVICE_ATTR(en_clk_gate, 0644, show_en_clk_gate, store_en_clk_gate);
static DEVICE_ATTR(clk_gate_mode, 0644, show_clk_gate_mode, store_clk_gate_mode);
static DEVICE_ATTR(en_dvfs, 0644, show_en_dvfs, store_en_dvfs);

static struct attribute *fimc_is_debug_entries[] = {
	&dev_attr_en_clk_gate.attr,
	&dev_attr_clk_gate_mode.attr,
	&dev_attr_en_dvfs.attr,
	NULL,
};
static struct attribute_group fimc_is_debug_attr_group = {
	.name	= "debug",
	.attrs	= fimc_is_debug_entries,
};

static int fimc_is_probe(struct platform_device *pdev)
{
	struct exynos_platform_fimc_is *pdata;
	struct resource *mem_res;
	struct resource *regs_res;
	struct fimc_is_core *core;
	int ret = -ENODEV;

	minfo("%s\n", __func__);

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		pdata = fimc_is_parse_dt(&pdev->dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	core = kzalloc(sizeof(struct fimc_is_core), GFP_KERNEL);
	if (!core) {
		err("core is NULL");
		return -ENOMEM;
	}

	fimc_is_dev = &pdev->dev;
	ret = dev_set_drvdata(fimc_is_dev, core);
	if (ret) {
		err("dev_set_drvdata is fail(%d)", ret);
		return ret;
	}

	core->pdev = pdev;
	core->pdata = pdata;
	core->id = pdev->id;
	core->debug_cnt = 0;
	device_init_wakeup(&pdev->dev, true);

	/* for mideaserver force down */
	atomic_set(&core->rsccount, 0);
	clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		goto p_err1;
	}

	regs_res = request_mem_region(mem_res->start, resource_size(mem_res),
					pdev->name);
	if (!regs_res) {
		dev_err(&pdev->dev, "Failed to request io memory region\n");
		goto p_err1;
	}

	core->regs_res = regs_res;
	core->regs =  ioremap_nocache(mem_res->start, resource_size(mem_res));
	if (!core->regs) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		goto p_err2;
	}

	core->irq = platform_get_irq(pdev, 0);
	if (core->irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto p_err3;
	}

	fimc_is_mem_probe(&core->mem,
		core->pdev);

	fimc_is_interface_probe(&core->interface,
		(u32)core->regs,
		core->irq,
		core);

	fimc_is_resource_probe(&core->resourcemgr, core);

	/* group initialization */
	fimc_is_groupmgr_probe(&core->groupmgr);

#ifndef SENSOR_S5K6B2_DRIVING
	ret = sensor_6b2_probe(NULL, NULL);
	if (ret) {
		err("sensor_6b2_probe is fail(%d)", ret);
		goto p_err3;
	}
#endif

#ifndef SENSOR_S5K8B1_DRIVING
	ret = sensor_8b1_probe(NULL, NULL);
	if (ret) {
		err("sensor_8b1_probe is fail(%d)", ret);
		goto p_err3;
	}
#endif

#ifndef SENSOR_IMX135_DRIVING
	ret = sensor_imx135_probe(NULL, NULL);
	if (ret) {
		err("sensor_imx135_probe is fail(%d)", ret);
		goto p_err3;
	}
#endif

#ifndef SENSOR_3L2_DRIVING
	ret = sensor_3l2_probe(NULL, NULL);
	if (ret) {
		err("sensor_3l2_probe is fail(%d)", ret);
		goto p_err3;
	}
#endif

#ifndef SENSOR_2P2_DRIVING
	ret = sensor_2p2_probe(NULL, NULL);
	if (ret) {
		err("sensor_2p2_probe is fail(%d)", ret);
		goto p_err3;
	}
#endif

	/* device entity - ischain0 */
	fimc_is_ischain_probe(&core->ischain[0],
		&core->interface,
		&core->resourcemgr,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		0,
		(u32)core->regs);

	/* device entity - ischain1 */
	fimc_is_ischain_probe(&core->ischain[1],
		&core->interface,
		&core->resourcemgr,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		1,
		(u32)core->regs);

	/* device entity - ischain2 */
	fimc_is_ischain_probe(&core->ischain[2],
		&core->interface,
		&core->resourcemgr,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		2,
		(u32)core->regs);

	ret = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register fimc-is v4l2 device\n");
		goto p_err3;
	}

	/* video entity - 3a0 */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a0))
		fimc_is_3a0_video_probe(core);

	/* video entity - 3a0 capture */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a0))
		fimc_is_3a0c_video_probe(core);

	/* video entity - 3a1 */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a1))
		fimc_is_3a1_video_probe(core);

	/* video entity - 3a1 capture */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a1))
		fimc_is_3a1c_video_probe(core);

	/* video entity - isp */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, isp))
		fimc_is_isp_video_probe(core);

	/*front video entity - scalerC */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, scc))
		fimc_is_scc_video_probe(core);

	/* back video entity - scalerP*/
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, scp))
		fimc_is_scp_video_probe(core);

	if (GET_FIMC_IS_NUM_OF_SUBIP(core, dis)) {
		/* vdis video entity - vdis capture*/
		fimc_is_vdc_video_probe(core);
		/* vdis video entity - vdis output*/
		fimc_is_vdo_video_probe(core);
	}

	platform_set_drvdata(pdev, core);

	fimc_is_ishcain_initmem(core);

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

#ifdef USE_OWN_FAULT_HANDLER
	exynos_sysmmu_set_fault_handler(fimc_is_dev, fimc_is_fault_handler);
#endif

	dbg("%s : fimc_is_front_%d probe success\n", __func__, pdev->id);

	/* set sysfs for debuging */
	sysfs_debug.en_clk_gate = 0;
	sysfs_debug.en_dvfs = 1;
#ifdef ENABLE_CLOCK_GATE
	sysfs_debug.en_clk_gate = 1;
#ifdef HAS_FW_CLOCK_GATE
	sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE;
#endif
#endif
	ret = sysfs_create_group(&core->pdev->dev.kobj, &fimc_is_debug_attr_group);

#ifdef ENABLE_DVFS
	{
		struct fimc_is_resourcemgr *resourcemgr;
		resourcemgr = &core->resourcemgr;
		/* dvfs controller init */
		ret = fimc_is_dvfs_init(resourcemgr);
		if (ret)
			err("%s: fimc_is_dvfs_init failed!\n", __func__);
	}
#endif
#ifdef ENABLE_CLOCK_GATE
	/* clock gate init */
	ret = fimc_is_clk_gate_init(core);
	if (ret)
		err("%s: fimc_is_clk_gate_init failed!\n", __func__);
#endif
	return 0;

p_err3:
	iounmap(core->regs);
p_err2:
	release_mem_region(regs_res->start, resource_size(regs_res));
p_err1:
	kfree(core);
	return ret;
}

static int fimc_is_remove(struct platform_device *pdev)
{
	dbg("%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops fimc_is_pm_ops = {
	.suspend		= fimc_is_suspend,
	.resume			= fimc_is_resume,
	.runtime_suspend	= fimc_is_runtime_suspend,
	.runtime_resume		= fimc_is_runtime_resume,
};

static int fimc_is_spi_probe(struct spi_device *spi)
{
	int ret = 0;
	struct fimc_is_core *core;

	BUG_ON(!fimc_is_dev);

	dbg_core("%s\n", __func__);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	/* spi->bits_per_word = 16; */
	if (spi_setup(spi)) {
		pr_err("failed to setup spi for fimc_is_spi\n");
		ret = -EINVAL;
		goto exit;
	}

	if (!strncmp(spi->modalias, "fimc_is_spi0", 12))
		core->spi0 = spi;

	if (!strncmp(spi->modalias, "fimc_is_spi1", 12))
		core->spi1 = spi;

exit:
	return ret;
}

static int fimc_is_spi_remove(struct spi_device *spi)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is",
	},
	{
		.compatible = "samsung,fimc_is_spi0",
	},
	{
		.compatible = "samsung,fimc_is_spi1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_match);

static struct spi_driver fimc_is_spi0_driver = {
	.driver = {
		.name = "fimc_is_spi0",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = exynos_fimc_is_match,
	},
	.probe 	= fimc_is_spi_probe,
	.remove = fimc_is_spi_remove,
};

module_spi_driver(fimc_is_spi0_driver);

static struct spi_driver fimc_is_spi1_driver = {
	.driver = {
		.name = "fimc_is_spi1",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = exynos_fimc_is_match,
	},
	.probe 	= fimc_is_spi_probe,
	.remove = fimc_is_spi_remove,
};

module_spi_driver(fimc_is_spi1_driver);

static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove		= fimc_is_remove,
	.driver = {
		.name	= FIMC_IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
		.of_match_table = exynos_fimc_is_match,
	}
};

module_platform_driver(fimc_is_driver);
#else
static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove	= __devexit_p(fimc_is_remove),
	.driver = {
		.name	= FIMC_IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
	}
};

static int __init fimc_is_init(void)
{
	int ret = platform_driver_register(&fimc_is_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);
	return ret;
}

static void __exit fimc_is_exit(void)
{
	platform_driver_unregister(&fimc_is_driver);
}
module_init(fimc_is_init);
module_exit(fimc_is_exit);
#endif

MODULE_AUTHOR("Jiyoung Shin<idon.shin@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS2 driver");
MODULE_LICENSE("GPL");
