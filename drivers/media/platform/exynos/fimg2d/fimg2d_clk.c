/* linux/drivers/media/video/exynos/fimg2d/fimg2d_clk.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/fimg2d.h>
#include "fimg2d.h"
#include "fimg2d_clk.h"

void fimg2d_clk_on(struct fimg2d_control *ctrl)
{
	clk_enable(ctrl->clock);
	fimg2d_debug("%s : clock enable\n", __func__);
}

void fimg2d_clk_off(struct fimg2d_control *ctrl)
{
	clk_disable(ctrl->clock);
	fimg2d_debug("%s : clock disable\n", __func__);
}

int fimg2d_clk_setup(struct fimg2d_control *ctrl)
{
	struct fimg2d_platdata *pdata;
	struct clk *parent, *sclk;
	int ret = 0;

	sclk = parent = NULL;
#ifdef CONFIG_OF
	pdata = ctrl->pdata;
#else
	pdata = to_fimg2d_plat(ctrl->dev);
#endif


	if (ip_is_g2d_5g() || ip_is_g2d_5a()) {
		fimg2d_info("aclk_acp(%lu) pclk_acp(%lu)\n",
				clk_get_rate(clk_get(NULL, "aclk_acp")),
				clk_get_rate(clk_get(NULL, "pclk_acp")));
	} else if (ip_is_g2d_5ar2()) {
		of_property_read_string_index(ctrl->dev->of_node,
				"clock-names", 0, &(pdata->gate_clkname));

		ctrl->clock = clk_get(ctrl->dev, pdata->gate_clkname);
		if (IS_ERR(ctrl->clock)) {
			dev_err(ctrl->dev, "failed to get clk_get():%s\n",
					pdata->gate_clkname);
			ret = -ENOENT;
			goto err_clk1;
		}
	} else {
		sclk = clk_get(ctrl->dev, pdata->clkname);
		if (IS_ERR(sclk)) {
			fimg2d_err("failed to get fimg2d clk\n");
			ret = -ENOENT;
			goto err_clk1;
		}
		fimg2d_info("fimg2d clk name: %s clkrate: %ld\n",
				pdata->clkname, clk_get_rate(sclk));
	}
	/* clock for gating */
	ctrl->clock = clk_get(ctrl->dev, pdata->gate_clkname);
	if (IS_ERR(ctrl->clock)) {
		fimg2d_err("failed to get gate clk\n");
		ret = -ENOENT;
		goto err_clk2;
	}

	if (ip_is_g2d_5ar2()) {
		ret = clk_prepare(ctrl->clock);
		if (ret < 0) {
			dev_err(ctrl->dev, "failed to prepare gate clk\n");
			ret = -ENOENT;
			goto err_clk2;
		}
	}

	fimg2d_info("gate clk: %s\n", pdata->gate_clkname);

	return ret;

err_clk2:
	if (sclk)
		clk_put(sclk);
	if (ctrl->clock)
		clk_put(ctrl->clock);

err_clk1:
	return ret;
}

void fimg2d_clk_release(struct fimg2d_control *ctrl)
{
	clk_put(ctrl->clock);
	if (ip_is_g2d_4p()) {
		struct fimg2d_platdata *pdata;
#ifdef CONFIG_OF
		pdata = ctrl->pdata;
#else
		pdata = to_fimg2d_plat(ctrl->dev);
#endif
		clk_put(clk_get(ctrl->dev, pdata->clkname));
		clk_put(clk_get(ctrl->dev, pdata->parent_clkname));
	}
}

int fimg2d_clk_set_gate(struct fimg2d_control *ctrl)
{
	/* CPLL:666MHz */
	struct clk *aclk_g2d_333, *aclk_g2d_333_nogate;
	struct fimg2d_platdata *pdata;
	int ret = 0;

#ifdef CONFIG_OF
	pdata = ctrl->pdata;
#else
	pdata = to_fimg2d_plat(ctrl->dev);
#endif

	aclk_g2d_333 = clk_get(NULL, "aclk_g2d_333");
	if (IS_ERR(aclk_g2d_333)) {
		pr_err("failed to get %s clock\n", "aclk_g2d_333");
		ret = PTR_ERR(aclk_g2d_333);
		goto err_clk1;
	}

	aclk_g2d_333_nogate = clk_get(NULL, "aclk_g2d_333_nogate");
	if (IS_ERR(aclk_g2d_333_nogate)) {
		pr_err("failed to get %s clock\n", "aclk_g2d_333_nogate");
		ret = PTR_ERR(aclk_g2d_333_nogate);
		goto err_clk2;
	}

	if (clk_set_parent(aclk_g2d_333_nogate, aclk_g2d_333))
		pr_err("Unable to set parent %s of clock %s\n",
			aclk_g2d_333->name, aclk_g2d_333_nogate->name);

	/* clock for gating */
	ctrl->clock = clk_get(ctrl->dev, pdata->gate_clkname);
	if (IS_ERR(ctrl->clock)) {
		fimg2d_err("failed to get gate clk\n");
		ret = -ENOENT;
		goto err_clk3;
	}
	fimg2d_debug("gate clk: %s\n", pdata->gate_clkname);

err_clk3:
	if (aclk_g2d_333_nogate)
		clk_put(aclk_g2d_333_nogate);
err_clk2:
	if (aclk_g2d_333)
		clk_put(aclk_g2d_333);
err_clk1:

	return ret;
}
