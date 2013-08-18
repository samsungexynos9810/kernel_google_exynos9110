/* sound/soc/samsung/lpass.c
 *
 * Low Power Audio SubSystem driver for Samsung Exynos
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>

#include <plat/clock.h>
#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/regs-clock-exynos5430.h>

#include "lpass.h"


/* Target clock rate */
#define TARGET_ACLKENM_RATE	(133000000)
#define TARGET_PCLKEN_DBG_RATE	(66000000)

/* Default interrupt mask */
#define INTR_CA5_MASK_VAL	(LPASS_INTR_SFR)
#define INTR_CPU_MASK_VAL	(LPASS_INTR_DMA | LPASS_INTR_I2S | \
				 LPASS_INTR_PCM | LPASS_INTR_SB | \
				 LPASS_INTR_UART | LPASS_INTR_SFR)

#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

struct lpass_info {
	spinlock_t		lock;
	bool			valid;
	struct platform_device	*pdev;
	void __iomem		*regs;
	void __iomem		*mem;
	struct clk		*clk_dmac;
	struct clk		*clk_sramc;
	struct clk		*clk_intr;
	struct clk		*clk_timer;
	struct clk		*clk_sysreg;
	struct clk		*clk_pmu;
	struct clk		*clk_gpio;
	struct clk		*clk_dbg;
} lpass;

struct aud_reg {
	void __iomem		*reg;
	u32			val;
	struct list_head	node;
};

static LIST_HEAD(reg_list);

extern int exynos_set_parent(const char *child, const char *parent);

void __iomem *lpass_get_regs(void)
{
	return lpass.regs;
}

void __iomem *lpass_get_mem(void)
{
	return lpass.mem;
}

void lpass_reset(int ip, int op)
{
	u32 reg, val, bit;

	spin_lock(&lpass.lock);
	reg = LPASS_CORE_SW_RESET;
	switch (ip) {
	case LPASS_IP_DMA:
		bit = LPASS_SW_RESET_DMA;
		break;
	case LPASS_IP_MEM:
		bit = LPASS_SW_RESET_MEM;
		break;
	case LPASS_IP_TIMER:
		bit = LPASS_SW_RESET_TIMER;
		break;
	case LPASS_IP_I2S:
		bit = LPASS_SW_RESET_I2S;
		break;
	case LPASS_IP_PCM:
		bit = LPASS_SW_RESET_PCM;
		break;
	case LPASS_IP_UART:
		bit = LPASS_SW_RESET_UART;
		break;
	case LPASS_IP_SLIMBUS:
		bit = LPASS_SW_RESET_SB;
		break;
	case LPASS_IP_CA5:
		reg = LPASS_CA5_SW_RESET;
		bit = LPASS_SW_RESET_CA5;
		break;
	default:
		spin_unlock(&lpass.lock);
		pr_err("%s: wrong ip type %d!\n", __func__, ip);
		return;
	}

	val = readl(lpass.regs + reg);
	switch (op) {
	case LPASS_OP_RESET:
		val &= ~bit;
		break;
	case LPASS_OP_NORMAL:
		val |= bit;
		break;
	default:
		spin_unlock(&lpass.lock);
		pr_err("%s: wrong op type %d!\n", __func__, op);
		return;
	}

	writel(val, lpass.regs + reg);
	spin_unlock(&lpass.lock);
}

void lpass_reset_toggle(int ip)
{
	pr_debug("%s: %d\n", __func__, ip);
	lpass_reset(ip, LPASS_OP_NORMAL);
	udelay(100);
	lpass_reset(ip, LPASS_OP_RESET);
	udelay(100);
	lpass_reset(ip, LPASS_OP_NORMAL);
}

static void lpass_reg_save(void)
{
	struct aud_reg *ar;

	pr_debug("Registers of LPASS are saved\n");

	list_for_each_entry(ar, &reg_list, node)
		ar->val = readl(ar->reg);

	return;
}

static void lpass_reg_restore(void)
{
	struct aud_reg *ar;

	pr_debug("Registers of LPASS are restore\n");

	list_for_each_entry(ar, &reg_list, node)
		writel(ar->val, ar->reg);

	return;
}

static void lpass_enable(void)
{
	if (!lpass.valid) {
		pr_debug("%s: LPASS is not available", __func__);
		return;
	}

	lpass_reg_restore();

	/* CLK_MUX_SEL_AUD0 */
	exynos_set_parent("mout_aud_pll_user", "mout_aud_pll");
	exynos_set_parent("mout_aud_dpll_user", "fin_pll");
	exynos_set_parent("mout_aud_pll_sub", "mout_aud_pll_user");

	clk_prepare_enable(lpass.clk_dmac);
	clk_prepare_enable(lpass.clk_sramc);
	clk_prepare_enable(lpass.clk_intr);
	clk_prepare_enable(lpass.clk_timer);
	clk_prepare_enable(lpass.clk_sysreg);
	clk_prepare_enable(lpass.clk_pmu);
	clk_prepare_enable(lpass.clk_gpio);
	clk_prepare_enable(lpass.clk_dbg);

	lpass_reset(LPASS_IP_CA5, LPASS_OP_RESET);
	lpass_reset_toggle(LPASS_IP_MEM);
	lpass_reset_toggle(LPASS_IP_I2S);
	lpass_reset_toggle(LPASS_IP_DMA);
}

static void lpass_disable(void)
{
	if (!lpass.valid) {
		pr_debug("%s: LPASS is not available", __func__);
		return;
	}

	clk_disable_unprepare(lpass.clk_dmac);
	clk_disable_unprepare(lpass.clk_sramc);
	clk_disable_unprepare(lpass.clk_intr);
	clk_disable_unprepare(lpass.clk_timer);
	clk_disable_unprepare(lpass.clk_sysreg);
	clk_disable_unprepare(lpass.clk_pmu);
	clk_disable_unprepare(lpass.clk_gpio);
	clk_disable_unprepare(lpass.clk_dbg);

	/* CLK_MUX_SEL_AUD0 @ FIN_PLL */
	exynos_set_parent("mout_aud_pll_user", "fin_pll");
	exynos_set_parent("mout_aud_dpll_user", "fin_pll");

	lpass_reg_save();
}

static int clk_set_heirachy(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	/* PMU_DEBUG @ XXTI 24MHz */
	writel(0x01000, EXYNOS_PMU_DEBUG);

	lpass.clk_dmac = clk_get(dev, "dmac");
	if (IS_ERR(lpass.clk_dmac)) {
		dev_err(dev, "dmac clk not found\n");
		goto err0;
	}

	lpass.clk_sramc = clk_get(dev, "sramc");
	if (IS_ERR(lpass.clk_sramc)) {
		dev_err(dev, "sramc clk not found\n");
		goto err1;
	}

	lpass.clk_intr = clk_get(dev, "intr");
	if (IS_ERR(lpass.clk_intr)) {
		dev_err(dev, "intr clk not found\n");
		goto err2;
	}

	lpass.clk_timer = clk_get(dev, "timer");
	if (IS_ERR(lpass.clk_timer)) {
		dev_err(dev, "timer clk not found\n");
		goto err3;
	}

	lpass.clk_sysreg = clk_get(dev, "sysreg");
	if (IS_ERR(lpass.clk_sysreg)) {
		dev_err(dev, "sysreg clk not found\n");
		goto err4;
	}

	lpass.clk_pmu = clk_get(dev, "pmu");
	if (IS_ERR(lpass.clk_pmu)) {
		dev_err(dev, "pmu clk not found\n");
		goto err5;
	}

	lpass.clk_gpio = clk_get(dev, "gpio");
	if (IS_ERR(lpass.clk_gpio)) {
		dev_err(dev, "gpio clk not found\n");
		goto err6;
	}

	lpass.clk_dbg = clk_get(dev, "dbg");
	if (IS_ERR(lpass.clk_dbg)) {
		dev_err(dev, "dbg clk not found\n");
		goto err7;
	}

	/* CLK_MUX_SEL_AUD0 @ FIN_PLL */
	exynos_set_parent("mout_aud_pll_user", "fin_pll");
	exynos_set_parent("mout_aud_dpll_user", "fin_pll");

	/* AUD_PLL @ 393.216MHz */
	writel(49282, EXYNOS5430_VA_CMU_TOP + 0x0114);	/* AUD_PLL_CON1 (K) */
	writel((1 << 31) | (459 << 16) | (7 << 8) | (2),
		EXYNOS5430_VA_CMU_TOP + 0x0110);	/* AUD_PLL_CON0 */

	/* CMU_AUD */
	writel(0x00520, EXYNOS5430_VA_CMU_AUD + 0x0600);/* CLK_DIV_AUD0 */
	writel(0xF2BF7, EXYNOS5430_VA_CMU_AUD + 0x0604);/* CLK_DIV_AUD1 */

	/* CLK_MUX_SEL_AUD0 */
	exynos_set_parent("mout_aud_pll_user", "mout_aud_pll");
	exynos_set_parent("mout_aud_dpll_user", "fin_pll");
	exynos_set_parent("mout_aud_pll_sub", "mout_aud_pll_user");

	/* CLK_MUX_SEL_AUD1 */
	exynos_set_parent("mout_sclk_i2s_a", "mout_aud_pll_sub");
	exynos_set_parent("mout_sclk_pcm_a", "mout_aud_pll_sub");

	return 0;

err7:
	clk_put(lpass.clk_gpio);
err6:
	clk_put(lpass.clk_pmu);
err5:
	clk_put(lpass.clk_sysreg);
err4:
	clk_put(lpass.clk_timer);
err3:
	clk_put(lpass.clk_intr);
err2:
	clk_put(lpass.clk_sramc);
err1:
	clk_put(lpass.clk_dmac);
err0:
	return -1;
}

static void lpass_add_suspend_reg(void __iomem *reg)
{
	struct device *dev = &lpass.pdev->dev;
	struct aud_reg *ar;

	ar = devm_kzalloc(dev, sizeof(struct aud_reg), GFP_KERNEL);
	if (!ar)
		return;

	ar->reg = reg;
	list_add(&ar->node, &reg_list);
}

static void lpass_init_reg_list(void)
{
	lpass_add_suspend_reg(EXYNOS5430_SRC_SEL_AUD0);
	lpass_add_suspend_reg(EXYNOS5430_SRC_SEL_AUD1);
	lpass_add_suspend_reg(EXYNOS5430_SRC_ENABLE_AUD0);
	lpass_add_suspend_reg(EXYNOS5430_SRC_ENABLE_AUD1);
	lpass_add_suspend_reg(EXYNOS5430_DIV_AUD0);
	lpass_add_suspend_reg(EXYNOS5430_DIV_AUD1);
	lpass_add_suspend_reg(EXYNOS5430_DIV_AUD2);
	lpass_add_suspend_reg(EXYNOS5430_DIV_AUD3);
	lpass_add_suspend_reg(EXYNOS5430_DIV_AUD4);
	lpass_add_suspend_reg(EXYNOS5430_ENABLE_ACLK_AUD);
	lpass_add_suspend_reg(EXYNOS5430_ENABLE_PCLK_AUD);
	lpass_add_suspend_reg(EXYNOS5430_ENABLE_SCLK_AUD0);
	lpass_add_suspend_reg(EXYNOS5430_ENABLE_SCLK_AUD1);
	lpass_add_suspend_reg(EXYNOS5430_ENABLE_IP_AUD0);
	lpass_add_suspend_reg(EXYNOS5430_ENABLE_IP_AUD1);

	lpass_add_suspend_reg(lpass.regs + LPASS_INTR_CA5_MASK);
	lpass_add_suspend_reg(lpass.regs + LPASS_INTR_CPU_MASK);
}

#ifdef CONFIG_PM
static int lpass_suspend(struct platform_device *pdev, pm_message_t state)
{
	pr_debug("%s entered\n", __func__);

	lpass_disable();

	return 0;
}

static int lpass_resume(struct platform_device *pdev)
{
	pr_debug("%s entered\n", __func__);

	lpass_enable();

	return 0;
}
#else
#define lpass_suspend NULL
#define lpass_resume  NULL
#endif

static char banner[] =
	KERN_INFO "Samsung Low Power Audio Subsystem driver, "\
		  "(c)2013 Samsung Electronics\n";

static int lpass_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	/* struct device_node *np = pdev->dev.of_node; */
	int ret = 0;

	printk(banner);

	lpass.pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Unable to get LPASS SFRs\n");
		return -ENXIO;
	}

	lpass.regs = devm_request_and_ioremap(&pdev->dev, res);
	if (!lpass.regs) {
		dev_err(dev, "SFR ioremap failed\n");
		return -ENOMEM;
	}
	pr_debug("%s: regs_base = %08X (%08X bytes)\n",
		__func__, res->start, resource_size(res));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get LPASS SRAM\n");
		return -ENXIO;
	}

	lpass.mem = devm_request_and_ioremap(&pdev->dev, res);
	if (!lpass.mem) {
		dev_err(&pdev->dev, "SRAM ioremap failed\n");
		return -ENOMEM;
	}
	pr_debug("%s: sram_base = %08X (%08X bytes)\n",
		__func__, res->start, resource_size(res));

	/* Set clock hierarchy for audio subsystem */
	ret = clk_set_heirachy(pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to set clock hierachy\n");
		return -ENXIO;
	}

	spin_lock_init(&lpass.lock);
	lpass_init_reg_list();

	/* unmask irq source */
	writel(INTR_CA5_MASK_VAL, lpass.regs + LPASS_INTR_CA5_MASK);
	writel(INTR_CPU_MASK_VAL, lpass.regs + LPASS_INTR_CPU_MASK);

	lpass_reg_save();
	lpass.valid = true;

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(&pdev->dev);
	lpass_reset(LPASS_IP_CA5, LPASS_OP_RESET);
	lpass_reset_toggle(LPASS_IP_MEM);
	lpass_reset_toggle(LPASS_IP_I2S);
	lpass_reset_toggle(LPASS_IP_DMA);
#else
	lpass_enable();
#endif

	return 0;
}

static int lpass_remove(struct platform_device *pdev)
{
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#else
	lpass_disable();
#endif

	clk_put(lpass.clk_dmac);
	clk_put(lpass.clk_sramc);
	clk_put(lpass.clk_intr);
	clk_put(lpass.clk_timer);
	clk_put(lpass.clk_sysreg);
	clk_put(lpass.clk_pmu);
	clk_put(lpass.clk_gpio);
	clk_put(lpass.clk_dbg);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int lpass_runtime_suspend(struct device *dev)
{
	pr_info("%s entered\n", __func__);

	lpass_disable();

	return 0;
}

static int lpass_runtime_resume(struct device *dev)
{
	pr_info("%s entered\n", __func__);

	lpass_enable();

	return 0;
}
#endif

static struct platform_device_id lpass_driver_ids[] = {
	{
		.name	= "samsung-lpass",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, lpass_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id exynos_lpass_match[] = {
	{
		.compatible = "samsung,exynos5430-lpass",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_lpass_match);
#endif

static const struct dev_pm_ops lpass_pmops = {
	SET_RUNTIME_PM_OPS(
		lpass_runtime_suspend,
		lpass_runtime_resume,
		NULL
	)
};

static struct platform_driver lpass_driver = {
	.probe		= lpass_probe,
	.remove		= lpass_remove,
	.suspend	= lpass_suspend,
	.resume		= lpass_resume,
	.id_table	= lpass_driver_ids,
	.driver		= {
		.name	= "samsung-lpass",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_lpass_match),
		.pm	= &lpass_pmops,
	},
};

module_platform_driver(lpass_driver);

/* Module information */
MODULE_AUTHOR("Yeongman Seo, <yman.seo@samsung.com>");
MODULE_DESCRIPTION("Samsung Low Power Audio Subsystem Interface");
MODULE_ALIAS("platform:samsung-lpass");
MODULE_LICENSE("GPL");
