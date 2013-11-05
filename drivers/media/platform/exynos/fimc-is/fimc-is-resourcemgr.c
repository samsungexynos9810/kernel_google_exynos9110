#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "fimc-is-resourcemgr.h"
#include "fimc-is-core.h"

struct pm_qos_request exynos5_isp_qos_dev;
struct pm_qos_request exynos5_isp_qos_mem;
struct pm_qos_request exynos5_isp_qos_cam;
struct pm_qos_request exynos5_isp_qos_disp;

int fimc_is_resource_probe(struct fimc_is_resourcemgr *resourcemgr,
	void *private_data)
{
	int ret = 0;

	BUG_ON(!resourcemgr);
	BUG_ON(!private_data);

	spin_lock_init(&resourcemgr->slock_clock);
	/* init spin_lock for clock gating */
	mutex_init(&resourcemgr->clock.lock);

	resourcemgr->private_data = private_data;

	atomic_set(&resourcemgr->rsccount, 0);
	atomic_set(&resourcemgr->rsccount_sensor, 0);
	atomic_set(&resourcemgr->rsccount_ischain, 0);

#ifdef ENABLE_DVFS
	/* dvfs controller init */
	ret = fimc_is_dvfs_init(resourcemgr);
	if (ret)
		err("%s: fimc_is_dvfs_init failed!\n", __func__);
#endif

	minfo("%s\n", __func__);
	return ret;
}

int fimc_is_resource_get(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
	struct fimc_is_core *core;

	BUG_ON(!resourcemgr);

	core = (struct fimc_is_core *)resourcemgr->private_data;

	minfo("[RSC] %s: rsccount = %d\n", __func__, atomic_read(&core->rsccount));

	if (!atomic_read(&core->rsccount)) {
		core->debug_cnt = 0;

		memset(&resourcemgr->clock.msk_cnt[0], 0, sizeof(int[GROUP_ID_MAX]));
		resourcemgr->clock.msk_state = 0;
		resourcemgr->clock.state_3a0 = 0;
		resourcemgr->clock.dvfs_level = DVFS_L0;
		resourcemgr->clock.dvfs_skipcnt = 0;
		resourcemgr->clock.dvfs_state = 0;

		/* 1. interface open */
		fimc_is_interface_open(&core->interface);

#if defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ)
		exynos5_mif_bpll_register_notifier(&exynos_mif_nb);
#endif
	}

	atomic_inc(&core->rsccount);

	if (atomic_read(&core->rsccount) > 5) {
		err("[RSC] %s: Invalid rsccount(%d)\n", __func__,
			atomic_read(&core->rsccount));
		ret = -EMFILE;
	}

	return ret;
}

int fimc_is_resource_put(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
	struct fimc_is_core *core;

	BUG_ON(!resourcemgr);

	core = (struct fimc_is_core *)resourcemgr->private_data;

	if ((atomic_read(&core->rsccount) == 0) ||
		(atomic_read(&core->rsccount) > 5)) {
		err("[RSC] %s: Invalid rsccount(%d)\n", __func__,
			atomic_read(&core->rsccount));
		ret = -EMFILE;

		goto exit;
	}

	atomic_dec(&core->rsccount);

	 pr_info("[RSC] %s: rsccount = %d\n",
               __func__, atomic_read(&core->rsccount));

	if (!atomic_read(&core->rsccount)) {
		/* HACK: This will be moved to runtime suspend */
#if defined(CONFIG_PM_DEVFREQ)
		/* 4. bus release */
		pr_info("[RSC] %s: DVFS UNLOCK\n", __func__);
		pm_qos_remove_request(&exynos5_isp_qos_dev);
		pm_qos_remove_request(&exynos5_isp_qos_mem);
		pm_qos_remove_request(&exynos5_isp_qos_cam);
		pm_qos_remove_request(&exynos5_isp_qos_disp);
#endif

		if (test_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state)) {
			/* 1. Stop a5 and other devices operation */
			ret = fimc_is_itf_power_down(&core->interface);
			if (ret)
				err("power down is failed, retry forcelly");

			/* 2. Power down */
			ret = fimc_is_ischain_power(&core->ischain[0], 0);
			if (ret)
				err("fimc_is_ischain_power is failed");
		}

		/* 3. Deinit variables */
		ret = fimc_is_interface_close(&core->interface);
		if (ret)
			err("fimc_is_interface_close is failed");

#if defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ)
		exynos5_mif_bpll_unregister_notifier(&exynos_mif_nb);
#endif

#ifndef RESERVED_MEM
		/* 5. Dealloc memroy */
		fimc_is_ishcain_deinitmem(&core->ischain[0]);
#endif
	}

exit:
	return ret;
}

static int fimc_is_set_group_state(struct fimc_is_resourcemgr *resourcemgr,
	int group_id, bool on)
{
	int ret = 0;

	if (on) {
		if (!test_and_set_bit(group_id, &resourcemgr->clock.msk_state)) {
			goto exit;
		} else {
			resourcemgr->clock.msk_cnt[group_id]++;
		}
	} else {
		if (resourcemgr->clock.msk_cnt[group_id]) {
			resourcemgr->clock.msk_cnt[group_id]--;
			goto exit;
		} else {
			clear_bit(group_id, &resourcemgr->clock.msk_state);
		}
	}
exit:
	return ret;
}

#define ISP_CLK_ON 1
#define ISP_CLK_OFF 0
#define ISP_CLK_ISP_ON 1
#define ISP_CLK_ISP_OFF 0
#define ISP_DIS_ON 1
#define ISP_DIS_OFF 2
static int isp_clock_onoff(int onoff, int isp, int dis)
{
	int timeout = 1000;
	int isp0_on_mask = ((0x1 << GATE_IP_ISP) | (0x1 << GATE_IP_DRC) | \
		(0x1 << GATE_IP_SCC) | (0x1 << GATE_IP_SCP));
#ifndef ENABLE_FULL_BYPASS
	int isp1_on_mask = ((0x1 << GATE_IP_ODC) | (0x1 << GATE_IP_DIS) | \
		(0x1 << GATE_IP_DNR));
#endif
	int isp0_off_mask = ((0x1 << GATE_IP_DRC) | (0x1 << GATE_IP_SCC) | \
		(0x1 << GATE_IP_SCP));
#ifndef ENABLE_FULL_BYPASS
	int isp1_off_mask = ((0x1 << GATE_IP_ODC) | (0x1 << GATE_IP_DIS) | \
		(0x1 << GATE_IP_DNR));
#endif
	int cfg;

	if (onoff == ISP_CLK_ON) {
		timeout = 1000;
		do
		{
			cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
			cfg |= (0x1 << GATE_IP_ISP);
			cfg |= (0x1 << GATE_IP_DRC);
			cfg |= (0x1 << GATE_IP_SCC);
			cfg |= (0x1 << GATE_IP_SCP);
			writel(cfg, EXYNOS5_CLKGATE_IP_ISP0);
			timeout--;
			if (!timeout)
				break;
		} while ((readl(EXYNOS5_CLKGATE_IP_ISP0) & isp0_on_mask) != \
			isp0_on_mask);

		if (!timeout) {
			pr_err("%s can never turn on the clocks #1\n", __func__);
			return 0;
		}

#ifndef ENABLE_FULL_BYPASS
		timeout = 1000;
		do
		{
			cfg = readl(EXYNOS5_CLKGATE_IP_ISP1);
			cfg |= (0x1 << GATE_IP_ODC);
			cfg |= (0x1 << GATE_IP_DIS);
			cfg |= (0x1 << GATE_IP_DNR);
			writel(cfg, EXYNOS5_CLKGATE_IP_ISP1);
			timeout--;
			if (!timeout)
				break;
		} while ((readl(EXYNOS5_CLKGATE_IP_ISP1) & isp1_on_mask) != \
			isp1_on_mask);

		if (!timeout) {
			pr_err("%s can never turn on the clocks #2\n", __func__);
			return 0;
		}
#endif
	} else {
		if (isp == ISP_CLK_ISP_ON) {
			timeout = 1000;
			do
			{
				cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
				cfg |= (0x1 << GATE_IP_ISP);
				writel(cfg, EXYNOS5_CLKGATE_IP_ISP0);
				timeout--;
				if (!timeout)
					break;
			} while (!(readl(EXYNOS5_CLKGATE_IP_ISP0) & \
				(0x1 << GATE_IP_ISP)));
		} else {
			timeout = 1000;
			do
			{
				cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
				cfg &= ~(0x1 << GATE_IP_ISP);
				writel(cfg, EXYNOS5_CLKGATE_IP_ISP0);
				timeout--;
				if (!timeout)
					break;
			} while (readl(EXYNOS5_CLKGATE_IP_ISP0) & \
				(0x1 << GATE_IP_ISP));
		}
		if (!timeout) {
			pr_err("%s can never turn on the clocks #3\n", __func__);
			return 0;
		}

		timeout = 1000;
		do
		{
			cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
			cfg &= ~(0x1<<GATE_IP_DRC);
			cfg &= ~(0x1<<GATE_IP_SCC);
			cfg &= ~(0x1<<GATE_IP_SCP);
			writel(cfg, EXYNOS5_CLKGATE_IP_ISP0);
			timeout--;
			if (!timeout)
				break;
		} while (readl(EXYNOS5_CLKGATE_IP_ISP0) & isp0_off_mask);

		if (!timeout) {
			pr_err("%s can never turn on the clocks #4\n", __func__);
			return 0;
		}

#ifndef ENABLE_FULL_BYPASS
		timeout = 1000;
		if (dis == ISP_DIS_OFF) {
			do
			{
				cfg = readl(EXYNOS5_CLKGATE_IP_ISP1);

				cfg &= ~(0x1<<GATE_IP_ODC);
				cfg &= ~(0x1<<GATE_IP_DIS);
				cfg &= ~(0x1<<GATE_IP_DNR);
				writel(cfg, EXYNOS5_CLKGATE_IP_ISP1);
				timeout--;
				if (!timeout)
					break;
			} while (readl(EXYNOS5_CLKGATE_IP_ISP1) & \
				isp1_off_mask);
		}
		if (!timeout) {
			pr_err("%s can never turn on the clocks #5\n", __func__);
			return 0;
		}
#endif
	}
	return 1;
}

int fimc_is_clock_set(struct fimc_is_resourcemgr *resourcemgr,
	int group_id,
	bool on)
{
	int ret = 0;
	int state;
	u32 cfg;
	int refcount;
	int i;
	struct fimc_is_core *core;

	BUG_ON(!resourcemgr);

	core = (struct fimc_is_core *)resourcemgr->private_data;

	spin_lock(&resourcemgr->slock_clock);

	if (!test_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state)) {
		if (group_id != GROUP_ID_MAX)
			err("power down state, accessing register is not allowd");
		goto exit;
	}

	refcount = atomic_read(&core->video_isp.refcount);
	if (refcount < 0) {
		err("invalid ischain refcount");
		goto exit;
	}

	/* HACK */
	for (i = 0; i < refcount; i++)
		if (IS_ISCHAIN_OTF(&core->ischain[i]))
			goto exit;

	if (refcount >= 3)
		group_id = GROUP_ID_MAX;

	if ((!on) && (group_id != GROUP_ID_MAX)) {
		for (i=0; i<FIMC_IS_MAX_NODES; i++) {
			unsigned long *ta_state, *isp_state, *dis_state;

			ta_state = &core->ischain[i].group_3aa.state;
			isp_state = &core->ischain[i].group_isp.state;
			dis_state = &core->ischain[i].group_dis.state;

			/*
			 *  if a group state is OPEN without READY, core
			 *  means the group is under init.
			 *  DO NOT turn the clock off.
			 */
			if (test_bit(FIMC_IS_GROUP_OPEN, ta_state) && \
				!test_bit(FIMC_IS_GROUP_READY, ta_state)) {
				warn("don't turn the clock off #1 !\n");
				goto exit;
			}
			if (test_bit(FIMC_IS_GROUP_OPEN, isp_state) && \
				!test_bit(FIMC_IS_GROUP_READY, isp_state)) {
				warn("don't turn the clock off #2 !\n");
				goto exit;
			}
			if (test_bit(FIMC_IS_GROUP_OPEN, dis_state) && \
				!test_bit(FIMC_IS_GROUP_READY, dis_state)) {
				warn("don't turn the clock off #3 !\n");
				goto exit;
			}
		}
	}

	if (group_id == GROUP_ID_MAX) {
		/* ALL ON */
		isp_clock_onoff(ISP_CLK_ON, ISP_CLK_ISP_ON, ISP_DIS_OFF);
		/* for debugging */
		writel(0x5A5A5A5A, core->ischain[0].interface->regs + ISSR53);
		goto exit;
	}

	fimc_is_set_group_state(resourcemgr, group_id, on);
	state = resourcemgr->clock.msk_state;

	if (!test_bit(FIMC_IS_SUBDEV_START, &core->ischain[0].dis.state)) {
		if (on || (state & (1<<GROUP_ID_ISP))) {
			/* ON */
			resourcemgr->clock.state_3a0 = false;
			isp_clock_onoff(ISP_CLK_ON, ISP_CLK_ISP_ON, ISP_DIS_OFF);

			writel(0x5A5A5A5A, core->ischain[0].interface->regs + ISSR53);
		} else if (!on) {
			/* OFF */
			if (state & (1<<GROUP_ID_3A0))
				writel(0x5A5A5F5F, core->ischain[0]. interface->regs + ISSR53);
			else
				writel(0x5F5F5F5F, core->ischain[0]. interface->regs + ISSR53);

			cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
			if (state & (1<<GROUP_ID_3A0)) {
				/* ON */
				isp_clock_onoff(ISP_CLK_OFF, ISP_CLK_ISP_ON, ISP_DIS_OFF);
			} else {
				if (resourcemgr->clock.state_3a0 == false)
				/* OFF */
					isp_clock_onoff(ISP_CLK_OFF, ISP_CLK_ISP_OFF, ISP_DIS_OFF);
				else
					isp_clock_onoff(ISP_CLK_OFF,ISP_CLK_ISP_ON, ISP_DIS_OFF);
			}
		} else {
			pr_err("%s should not be here.!!\n", __func__);
			isp_clock_onoff(ISP_CLK_ON, ISP_CLK_ISP_ON, \
				ISP_DIS_OFF);
		}
	} else {
		if (state & (1<<GROUP_ID_ISP)) {
			resourcemgr->clock.state_3a0 = false;

			isp_clock_onoff(ISP_CLK_ON, ISP_CLK_ISP_ON, ISP_DIS_ON);

			writel(0x5A5A5A5A, core->ischain[0].interface->regs + ISSR53);
		} else {
			if (state & (1<<GROUP_ID_3A0))
				writel(0x5A5A5F5F, core->ischain[0].interface->regs + ISSR53);
			else
				writel(0x5F5F5F5F, core->ischain[0].interface->regs + ISSR53);

			cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
			if (state & (1<<GROUP_ID_3A0)) {
				isp_clock_onoff(ISP_CLK_OFF, ISP_CLK_ISP_ON, ISP_DIS_ON);
			} else {
				isp_clock_onoff(ISP_CLK_OFF, ISP_CLK_ISP_OFF, ISP_DIS_ON);
			}
		}
	}

exit:
	spin_unlock(&resourcemgr->slock_clock);
	return ret;
}

