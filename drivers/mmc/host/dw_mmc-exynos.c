/*
 * Exynos Specific Extensions for Synopsys DW Multimedia Card Interface driver
 *
 * Copyright (C) 2012, Samsung Electronics Co., Ltd.
 * Copyright (C) 2013, The Chromium OS Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>	/* for kmalloc/kfree prototype */

#include <mach/exynos-pm.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"
#include "dw_mmc-exynos.h"

/* SFR save/restore for LPA */
struct dw_mci *dw_mci_lpa_host[3] = {0, 0, 0};
unsigned int dw_mci_host_count;
unsigned int dw_mci_save_sfr[3][30];

static struct dw_mci_exynos_compatible {
	char				*compatible;
	enum dw_mci_exynos_type		ctrl_type;
} exynos_compat[] = {
	{
		.compatible	= "samsung,exynos4210-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS4210,
	}, {
		.compatible	= "samsung,exynos4412-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS4412,
	}, {
		.compatible	= "samsung,exynos5250-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS5250,
	}, {
		.compatible	= "samsung,exynos5422-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS5422,
	}, {
		.compatible	= "samsung,exynos5430-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS5430,
	},
};

static int dw_mci_exynos_priv_init(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv;
	int idx;

	priv = devm_kzalloc(host->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(host->dev, "mem alloc failed for private data\n");
		return -ENOMEM;
	}

	for (idx = 0; idx < ARRAY_SIZE(exynos_compat); idx++) {
		if (of_device_is_compatible(host->dev->of_node,
					exynos_compat[idx].compatible))
			priv->ctrl_type = exynos_compat[idx].ctrl_type;
	}

	host->priv = priv;

	return 0;
}

int dw_mci_exynos_request_status(void)
{
	int ret = DW_MMC_REQ_BUSY, i;

	for (i = 0;i < dw_mci_host_count; i++) {
		struct dw_mci *host = dw_mci_lpa_host[i];
		if (host->req_state == DW_MMC_REQ_BUSY) {
			ret = DW_MMC_REQ_BUSY;
			break;
		} else
			ret = DW_MMC_REQ_IDLE;
	}

	return ret;
}

/*
 * MSHC SFR save/restore
 */
void dw_mci_exynos_save_host_base(struct dw_mci *host)
{
	dw_mci_lpa_host[dw_mci_host_count] = host;
	dw_mci_host_count++;
}
#ifdef CONFIG_CPU_IDLE
static void exynos_sfr_save(unsigned int i)
{
	struct dw_mci *host = dw_mci_lpa_host[i];

	dw_mci_save_sfr[i][0] = mci_readl(host, CTRL);
	dw_mci_save_sfr[i][1] = mci_readl(host, PWREN);
	dw_mci_save_sfr[i][2] = mci_readl(host, CLKDIV);
	dw_mci_save_sfr[i][3] = mci_readl(host, CLKSRC);
	dw_mci_save_sfr[i][4] = mci_readl(host, CLKENA);
	dw_mci_save_sfr[i][5] = mci_readl(host, TMOUT);
	dw_mci_save_sfr[i][6] = mci_readl(host, CTYPE);
	dw_mci_save_sfr[i][7] = mci_readl(host, INTMASK);
	dw_mci_save_sfr[i][8] = mci_readl(host, FIFOTH);
	dw_mci_save_sfr[i][9] = mci_readl(host, UHS_REG);
	dw_mci_save_sfr[i][10] = mci_readl(host, BMOD);
	dw_mci_save_sfr[i][11] = mci_readl(host, PLDMND);
	dw_mci_save_sfr[i][12] = mci_readl(host, DBADDR);
	dw_mci_save_sfr[i][13] = mci_readl(host, IDINTEN);
	dw_mci_save_sfr[i][14] = mci_readl(host, CLKSEL);
	dw_mci_save_sfr[i][15] = mci_readl(host, CDTHRCTL);
}

static void exynos_sfr_restore(unsigned int i)
{
	struct dw_mci *host = dw_mci_lpa_host[i];

	int startbit_clear = false;
	unsigned int cmd_status = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(500);

	mci_writel(host, CTRL , dw_mci_save_sfr[i][0]);
	mci_writel(host, PWREN, dw_mci_save_sfr[i][1]);
	mci_writel(host, CLKDIV, dw_mci_save_sfr[i][2]);
	mci_writel(host, CLKSRC, dw_mci_save_sfr[i][3]);
	mci_writel(host, CLKENA, dw_mci_save_sfr[i][4]);
	mci_writel(host, TMOUT, dw_mci_save_sfr[i][5]);
	mci_writel(host, CTYPE, dw_mci_save_sfr[i][6]);
	mci_writel(host, INTMASK, dw_mci_save_sfr[i][7]);
	mci_writel(host, FIFOTH, dw_mci_save_sfr[i][8]);
	mci_writel(host, UHS_REG, dw_mci_save_sfr[i][9]);
	mci_writel(host, BMOD  , dw_mci_save_sfr[i][10]);
	mci_writel(host, PLDMND, dw_mci_save_sfr[i][11]);
	mci_writel(host, DBADDR, dw_mci_save_sfr[i][12]);
	mci_writel(host, IDINTEN, dw_mci_save_sfr[i][13]);
	mci_writel(host, CLKSEL, dw_mci_save_sfr[i][14]);
	mci_writel(host, CDTHRCTL, dw_mci_save_sfr[i][15]);

	mci_writel(host, CMDARG, 0);
	wmb();
	mci_writel(host, CMD, (SDMMC_CMD_START |
				SDMMC_CMD_UPD_CLK |
				SDMMC_CMD_PRV_DAT_WAIT));

	while (time_before(jiffies, timeout)) {
		cmd_status = mci_readl(host, CMD);
		if (!(cmd_status & SDMMC_CMD_START)) {
			startbit_clear = true;
			break;
		}
	}
	if (startbit_clear == false)
		dev_err(host->dev, "CMD start bit stuck %02d\n", i);
}

static int dw_mmc_exynos_notifier0(struct notifier_block *self,
					unsigned long cmd, void *v)
{
	switch (cmd) {
	case LPA_ENTER:
		exynos_sfr_save(0);
		break;
	case LPA_ENTER_FAIL:
	case LPA_EXIT:
		exynos_sfr_restore(0);
		break;
	}

	return 0;
}

static int dw_mmc_exynos_notifier1(struct notifier_block *self,
					unsigned long cmd, void *v)
{
	switch (cmd) {
	case LPA_ENTER:
		exynos_sfr_save(1);
		break;
	case LPA_ENTER_FAIL:
	case LPA_EXIT:
		exynos_sfr_restore(1);
		break;
	}

	return 0;
}

static int dw_mmc_exynos_notifier2(struct notifier_block *self,
									unsigned long cmd, void *v)
{
	switch (cmd) {
	case LPA_ENTER:
		exynos_sfr_save(2);
		break;
	case LPA_ENTER_FAIL:
	case LPA_EXIT:
		exynos_sfr_restore(2);
		break;
	}

	return 0;
}

static struct notifier_block dw_mmc_exynos_notifier_block[3] = {
	[0] = {
		.notifier_call = dw_mmc_exynos_notifier0,
		.priority = 2,
	},
	[1] = {
		.notifier_call = dw_mmc_exynos_notifier1,
		.priority = 2,
	},
	[2] = {
		.notifier_call = dw_mmc_exynos_notifier2,
		.priority = 2,
	},
};

void dw_mci_exynos_register_notifier(struct dw_mci *host)
{
	/*
	 * Should be called sequentially
	 */
	dw_mci_lpa_host[dw_mci_host_count] = host;
	exynos_pm_register_notifier(&(dw_mmc_exynos_notifier_block[dw_mci_host_count++]));
}

void dw_mci_exynos_unregister_notifier(struct dw_mci *host)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (host == dw_mci_lpa_host[i])
			exynos_pm_unregister_notifier(&(dw_mmc_exynos_notifier_block[i]));
	}
}
#endif

/*
 * Configuration of Security Management Unit
 */
void dw_mci_exynos_cfg_smu(struct dw_mci *host)
{
	volatile unsigned int reg = __raw_readl(host->regs +
					DWMCI_MPSECURITY);
	/* Extended Descriptor On */
	__raw_writel(reg | (1 << 31), host->regs + DWMCI_MPSECURITY);

	/* FMP Bypass Partition */
	__raw_writel(DW_MMC_BYPASS_SECTOR, host->regs + DWMCI_MPSBEGIN0);
	__raw_writel(DW_MMC_BYPASS_SECTOR, host->regs + DWMCI_MPSEND0);
	__raw_writel(DWMCI_MPSCTRL_SECURE_READ_BIT |
		DWMCI_MPSCTRL_SECURE_WRITE_BIT |
		DWMCI_MPSCTRL_NON_SECURE_READ_BIT |
		DWMCI_MPSCTRL_NON_SECURE_WRITE_BIT |
		DWMCI_MPSCTRL_VALID, host->regs + DWMCI_MPSCTRL0);

	/* Encryption Enabled Partition */
	__raw_writel(DW_MMC_ENCRYPTION_SECTOR, host->regs +
		DWMCI_MPSBEGIN1);
	__raw_writel(DWMCI_BLOCK_NUM, host->regs + DWMCI_MPSEND1);
	__raw_writel(DWMCI_MPSCTRL_SECURE_READ_BIT |
		DWMCI_MPSCTRL_SECURE_WRITE_BIT |
		DWMCI_MPSCTRL_NON_SECURE_READ_BIT |
		DWMCI_MPSCTRL_NON_SECURE_WRITE_BIT |
		DWMCI_MPSCTRL_ENCRYPTION |
		DWMCI_MPSCTRL_VALID, host->regs + DWMCI_MPSCTRL1);
}

static int dw_mci_exynos_setup_clock(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (priv->ctrl_type == DW_MCI_TYPE_EXYNOS5250 ||
		priv->ctrl_type == DW_MCI_TYPE_EXYNOS5422 ||
		priv->ctrl_type == DW_MCI_TYPE_EXYNOS5430)
		host->bus_hz /= (priv->ciu_div + 1);
	else if (priv->ctrl_type == DW_MCI_TYPE_EXYNOS4412)
		host->bus_hz /= EXYNOS4412_FIXED_CIU_CLK_DIV;
	else if (priv->ctrl_type == DW_MCI_TYPE_EXYNOS4210)
		host->bus_hz /= EXYNOS4210_FIXED_CIU_CLK_DIV;

	return 0;
}
static void dw_mci_exynos_register_dump(struct dw_mci *host)
{
	dev_err(host->dev, ": CLKSEL:	0x%08x\n", mci_readl(host, CLKSEL));
	dev_err(host->dev, ": DWMCI_EMMCP_BASE:	0x%08x\n",
		__raw_readl(host->regs + DWMCI_EMMCP_BASE));
	dev_err(host->dev, ": DWMCI_MPSECURITY:	0x%08x\n",
		__raw_readl(host->regs + DWMCI_MPSECURITY));
	dev_err(host->dev, ": DWMCI_MPSBEGIN0:	0x%08x\n",
		__raw_readl(host->regs + DWMCI_MPSBEGIN0));
	dev_err(host->dev, ": DWMCI_MPSEND0:	0x%08x\n",
		__raw_readl(host->regs + DWMCI_MPSEND0));
	dev_err(host->dev, ": DWMCI_MPSCTRL0:	0x%08x\n",
		__raw_readl(host->regs + DWMCI_MPSCTRL0));
	dev_err(host->dev, ": DWMCI_DDR200_RDDQS_EN:	0x%08x\n",
		__raw_readl(host->regs + DWMCI_DDR200_RDDQS_EN + 0x70));
	dev_err(host->dev, ": DWMCI_DDR200_ASYNC_FIFO_CTRL:	0x%08x\n",
		__raw_readl(host->regs + DWMCI_DDR200_ASYNC_FIFO_CTRL + 0x70));
	dev_err(host->dev, ": DWMCI_DDR200_DLINE_CTRL:	0x%08x\n",
		__raw_readl(host->regs + DWMCI_DDR200_DLINE_CTRL + 0x70));
}
static void dw_mci_exynos_prepare_command(struct dw_mci *host, u32 *cmdr)
{
	/*
	 * Exynos4412 and Exynos5250 extends the use of CMD register with the
	 * use of bit 29 (which is reserved on standard MSHC controllers) for
	 * optionally bypassing the HOLD register for command and data. The
	 * HOLD register should be bypassed in case there is no phase shift
	 * applied on CMD/DATA that is sent to the card.
	 */
	if (SDMMC_CLKSEL_GET_DRV_WD3(mci_readl(host, CLKSEL)))
		*cmdr |= SDMMC_CMD_USE_HOLD_REG;
}

/**
 * dw_mci_exynos_set_bus_hz - try to set the ciu clock; update host->bus_hz.
 *
 * @want_bus_hz: bus_hz the eMMC device wants
 */
static void dw_mci_exynos_set_bus_hz(struct dw_mci *host, u32 want_bus_hz)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 ciu_rate = clk_get_rate(host->ciu_clk);
	u32 ciu_div;
	u32 tmp_reg;
	int clkerr = 0;
	struct clk *adiv = devm_clk_get(host->dev, "dout_mmc_a");
	struct clk *bdiv = devm_clk_get(host->dev, "dout_mmc_b");

	tmp_reg = mci_readl(host, CLKSEL);
	ciu_div = SDMMC_CLKSEL_GET_DIVRATIO(tmp_reg);

	/* Try to set the upstream clock rate */
	if (ciu_rate != (want_bus_hz * ciu_div)) {
		ciu_rate = want_bus_hz * ciu_div;
		clkerr = clk_set_rate(adiv, ciu_rate);
		if (clkerr)
			dev_warn(host->dev, "Couldn't set rate to %u\n",
				 ciu_rate);

		if (priv->ctrl_type != DW_MCI_TYPE_EXYNOS5422) {
			clkerr = clk_set_rate(bdiv, ciu_rate);
			if (clkerr)
				dev_warn(host->dev, "Couldn't set rate to %u\n",
					 ciu_rate);
			ciu_rate = clk_get_rate(host->ciu_clk);
		}
	}

	host->bus_hz = ciu_rate / ciu_div;
}

static void dw_mci_exynos_set_ios(struct dw_mci *host, unsigned int tuning, struct mmc_ios *ios)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	struct dw_mci_board *pdata = host->pdata;
	u32 *clk_tbl = priv->ref_clk;
	u32 clksel, rddqs, dline;
	u32 cclkin;
	unsigned char timing = ios->timing;

	if (timing > MMC_TIMING_MMC_HS200_DDR) {
		pr_err("%s: timing(%d): not suppored\n", __func__, timing);
		return;
	}

	cclkin = clk_tbl[timing];
	rddqs = DWMCI_DDR200_RDDQS_EN_DEF;
	dline = DWMCI_DDR200_DLINE_CTRL_DEF;
	clksel = __raw_readl(host->regs + DWMCI_CLKSEL);

	if (host->bus_hz != cclkin) {
		dw_mci_exynos_set_bus_hz(host, cclkin);
		host->bus_hz = cclkin;
		host->current_speed = 0;
	}

	if (timing == MMC_TIMING_MMC_HS200_DDR) {
		clksel = ((priv->ddr200_timing & 0xfffffff8) | pdata->clk_smpl);

		if (!tuning) {
			rddqs |= (DWMCI_RDDQS_EN | DWMCI_AXI_NON_BLOCKING_WRITE);
			dline = DWMCI_FIFO_CLK_DELAY_CTRL(0x2) | DWMCI_RD_DQS_DELAY_CTRL(90);
			host->quirks &= ~DW_MCI_QUIRK_NO_DETECT_EBIT;
		}
	} else if (timing == MMC_TIMING_MMC_HS200 ||
			timing == MMC_TIMING_UHS_SDR104) {
		clksel = (clksel & 0xfff8ffff) | (priv->ciu_div << 16);
	} else if (timing == MMC_TIMING_UHS_SDR50) {
		clksel = (clksel & 0xfff8ffff) | (priv->ciu_div << 16);
	} else if (timing == MMC_TIMING_UHS_DDR50) {
		clksel = priv->ddr_timing;
	} else {
		clksel = priv->sdr_timing;
	}

	__raw_writel(clksel, host->regs + DWMCI_CLKSEL);

	if (priv->ctrl_type == DW_MCI_TYPE_EXYNOS5422 ||
		priv->ctrl_type == DW_MCI_TYPE_EXYNOS5430) {
		__raw_writel(rddqs, host->regs + DWMCI_DDR200_RDDQS_EN + 0x70);
		__raw_writel(dline, host->regs + DWMCI_DDR200_DLINE_CTRL + 0x70);
	} else {
		__raw_writel(rddqs, host->regs + DWMCI_DDR200_RDDQS_EN);
		__raw_writel(dline, host->regs + DWMCI_DDR200_DLINE_CTRL);
	}
}

#ifndef MHZ
#define MHZ (1000*1000)
#endif
static int dw_mci_exynos_parse_dt(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 timing[3];
	u32 div = 0;

	struct device_node *np = host->dev->of_node;
	int ref_clk_size;
	u32 *ref_clk;
	u32 *ciu_clkin_values = NULL;
	int idx_ref;
	int ret = 0;
	int id = 0;
	int dev_pwr = 0;

	/*
	 * This GPIOs is for eMMC device 2.8 volt supply control
	*/
	if (of_get_property(np, "gpios", NULL) != NULL) {
		dev_pwr = of_get_gpio(np, 0);
		if (gpio_request(dev_pwr, "dev-pwr")) {
			ret = -ENODEV;
			goto err_ref_clk;
		} else
			gpio_direction_output(dev_pwr, 1);
	}

	/*
	 * Reference clock values for speed mode change are extracted from DT.
	 * Now, a number of reference clock values should be defined in DT
	 * We will be able to get reference clock values by using just one function
	 * when DT support to get tables with many columns.
	*/
	if (of_property_read_u32(np, "num-ref-clks", &ref_clk_size)) {
		dev_err(host->dev, "Getting a number of referece clock failed\n");
		ret = -ENODEV;
		goto err_ref_clk;
	}

	ref_clk = devm_kzalloc(host->dev, ref_clk_size * sizeof(*ref_clk),
					GFP_KERNEL);
	if (!ref_clk) {
		dev_err(host->dev, "Mem alloc failed for reference clock table\n");
		ret = -ENOMEM;
		goto err_ref_clk;
	}

	ciu_clkin_values = devm_kzalloc(host->dev,
			ref_clk_size * sizeof(*ciu_clkin_values), GFP_KERNEL);

	if (!ciu_clkin_values) {
		dev_err(host->dev, "Mem alloc failed for temporary clock values\n");
		ret = -ENOMEM;
		goto err_ref_clk;
	}
	if (of_property_read_u32_array(np, "ciu_clkin", ciu_clkin_values, ref_clk_size)) {
		dev_err(host->dev, "Getting ciu_clkin values faild\n");
		ret = -ENOMEM;
		goto err_ref_clk;
	}

	for (idx_ref = 0; idx_ref < ref_clk_size; idx_ref++, ref_clk++, ciu_clkin_values++) {
		if (*ciu_clkin_values > MHZ)
			*(ref_clk) = (*ciu_clkin_values);
		else
			*(ref_clk) = (*ciu_clkin_values) * MHZ;
	}

	ref_clk -= ref_clk_size;
	ciu_clkin_values -= ref_clk_size;
	priv->ref_clk = ref_clk;

	of_property_read_u32(np, "samsung,dw-mshc-ciu-div", &div);
	priv->ciu_div = div;

	ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-sdr-timing", timing, 3);
	if (ret)
		return ret;

	priv->sdr_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2]);

	ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-ddr-timing", timing, 3);
	if (ret)
		goto err_ref_clk;

	priv->ddr_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2]);

	id = of_alias_get_id(host->dev->of_node, "mshc");
	switch (id) {
	/* dwmmc0 : eMMC    */
	case 0:
		ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-hs200-timing", timing, 3);
		if (ret)
			goto err_ref_clk;

		priv->hs200_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2]);

		ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-ddr200-timing", timing, 3);
		if (ret)
			goto err_ref_clk;

		priv->ddr200_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2]);
		break;
	/* dwmmc1 : SDIO    */
	case 1:
		break;
	/* dwmmc2 : SD Card */
	case 2:
		break;
	default:
		ret = -ENODEV;
	}

err_ref_clk:

	return ret;
}

static void dw_mci_set_quirk_endbit(struct dw_mci *host, s8 mid)
{
	u32 clksel, phase;
	u32 shift;

	clksel = mci_readl(host, CLKSEL);
	phase = (((clksel >> 24) & 0x7) + 1) << 1;
	shift = 360 / phase;

	if (host->verid < DW_MMC_260A && (shift * mid) % 360 >= 225)
		host->quirks |= DW_MCI_QUIRK_NO_DETECT_EBIT;
	else
		host->quirks &= ~DW_MCI_QUIRK_NO_DETECT_EBIT;
}

/* initialize the clock sample to given value */
static void dw_mci_exynos_set_sample(struct dw_mci *host, u32 sample, bool tuning)
{
	u32 clksel;

	clksel = mci_readl(host, CLKSEL);
	clksel = (clksel & ~0x7) | SDMMC_CLKSEL_CCLK_SAMPLE(sample);
	mci_writel(host, CLKSEL, clksel);
	if (!tuning)
		dw_mci_set_quirk_endbit(host, clksel);
}

/* read current clock sample offset */
static u32 dw_mci_exynos_get_sample(struct dw_mci *host)
{
	u32 clksel = mci_readl(host, CLKSEL);
	return SDMMC_CLKSEL_CCLK_SAMPLE(clksel);
}

static s8 exynos_dwmci_extra_tuning(u8 map)
{
	s8 sel = -1;

	if ((map & 0x03) == 0x03)
		sel = 0;
	else if ((map & 0x0c) == 0x0c)
		sel = 3;
	else if ((map & 0x06) == 0x06)
		sel = 2;

	return sel;
}

/*
 * After testing all (8) possible clock sample values and using one bit for
 * each value that works, return the "middle" bit position of any sequential
 * bits.
 */
static int find_median_of_bits(struct dw_mci *host, unsigned int map)
{
	unsigned int i, testbits;
	u8 divratio;

	/* replicate the map so "arithimetic shift right" shifts in
	 * the same bits "again". e.g. portable "Rotate Right" bit operation.
	 */
	testbits = map | (map << 8);

	divratio = (mci_readl(host, CLKSEL) >> 24) & 0x7;

	if (divratio == 1) {
		map = map & (map >> 4);
		goto median3;
	}

/* Middle is bit 3 */
#define SEVENBITS 0x7f

	for (i = 3; i < (8 + 3); i++, testbits >>= 1) {
		if ((testbits & SEVENBITS) == SEVENBITS)
			return SDMMC_CLKSEL_CCLK_SAMPLE(i);
	}

/* Middle is bit 2. */
#define FIVEBITS 0x1f

	for (i = 2; i < (8 + 2); i++, testbits >>= 1) {
		if ((testbits & FIVEBITS) == FIVEBITS)
			return SDMMC_CLKSEL_CCLK_SAMPLE(i);
	}

median3:
/* Middle is bit 1. */
#define THREEBITS 0x7

	for (i = 1; i < (8 + 1); i++, testbits >>= 1) {
		if ((testbits & THREEBITS) == THREEBITS)
			return SDMMC_CLKSEL_CCLK_SAMPLE(i);
	}

	if (host->pdata->extra_tuning) {
		dev_info(host->dev, "Extra Tuning\n");
		i = exynos_dwmci_extra_tuning(map);
		return SDMMC_CLKSEL_CCLK_SAMPLE(i);
	}

	return -1;
}

/*
 * Test all 8 possible "Clock in" Sample timings.
 * Create a bitmap of which CLock sample values work and find the "median"
 * value. Apply it and remember that we found the best value.
 */
static int dw_mci_exynos_execute_tuning(struct dw_mci *host, u32 opcode)
{
	struct dw_mci_slot *slot = host->cur_slot;
	struct mmc_host *mmc = slot->mmc;
	struct mmc_ios *ios = &(mmc->ios);
	const u8 *tuning_blk_pattern;	/* data pattern we expect */
	u8 *tuning_blk;			/* data read from device */
	unsigned int blksz;
	unsigned int sample_good = 0;	/* bit map of clock sample (0-7) */
	u32 test_sample, orig_sample;
	int best_sample;

	if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
		if (ios->bus_width == MMC_BUS_WIDTH_8) {
			tuning_blk_pattern = tuning_blk_pattern_8bit;
			blksz = sizeof(tuning_blk_pattern_8bit);
		} else if (ios->bus_width == MMC_BUS_WIDTH_4) {
			tuning_blk_pattern = tuning_blk_pattern_4bit;
			blksz = sizeof(tuning_blk_pattern_4bit);
		} else {
			return -EINVAL;
		}
	} else if (opcode == MMC_SEND_TUNING_BLOCK) {
		tuning_blk_pattern = tuning_blk_pattern_4bit;
		blksz = sizeof(tuning_blk_pattern_4bit);
	} else {
		dev_err(mmc_classdev(host->cur_slot->mmc),
			"Undefined command(%d) for tuning\n",
			opcode);
		return -EINVAL;
	}

	/* Short circuit: don't tune again if we already did. */
	if (host->pdata->tuned) {
		dw_mci_exynos_set_sample(host, host->pdata->clk_smpl, false);
		mci_writel(host, CDTHRCTL, host->cd_rd_thr << 16 | 1);
		return 0;
	}

	tuning_blk = kmalloc(2 * blksz, GFP_KERNEL);
	if (!tuning_blk)
		return -ENOMEM;

	orig_sample = dw_mci_exynos_get_sample(host);
	host->cd_rd_thr = 512;
	mci_writel(host, CDTHRCTL, host->cd_rd_thr << 16 | 1);

	/*
	 * eMMC 4.5 spec section 6.6.7.1 says the device is guaranteed to
	 * complete 40 iteration of CMD21 in 150ms. So this shouldn't take
	 * longer than about 30ms or so....at least assuming most values
	 * work and don't time out.
	 */
	for (test_sample = 0; test_sample < 8; test_sample++) {
		struct mmc_request mrq;
		struct mmc_command cmd;
		struct mmc_command stop;
		struct mmc_data data;
		struct scatterlist sg;

		memset(&cmd, 0, sizeof(cmd));
		cmd.opcode = opcode;
		cmd.arg = 0;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
		cmd.error = 0;
		cmd.cmd_timeout_ms = 10; /* 2x * (150ms/40 + setup overhead) */

		memset(&stop, 0, sizeof(stop));
		stop.opcode = MMC_STOP_TRANSMISSION;
		stop.arg = 0;
		stop.flags = MMC_RSP_R1B | MMC_CMD_AC;
		stop.error = 0;

		memset(&data, 0, sizeof(data));
		data.blksz = blksz;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.sg = &sg;
		data.sg_len = 1;
		data.error = 0;

		memset(tuning_blk, ~0U, blksz);
		sg_init_one(&sg, tuning_blk, blksz);

		memset(&mrq, 0, sizeof(mrq));
		mrq.cmd = &cmd;
		mrq.stop = &stop;
		mrq.data = &data;
		host->mrq = &mrq;

		dw_mci_exynos_set_sample(host, test_sample, true);
		dw_mci_set_timeout(host);

		mmc_wait_for_req(mmc, &mrq);

		if (!cmd.error && !data.error) {
			/*
			 * Verify the "tuning block" arrived (to host) intact.
			 * If yes, remember this sample value works.
			 */
			if (host->use_dma == 1) {
				sample_good |= (1 << test_sample);
			} else {
				if (!memcmp(tuning_blk_pattern, tuning_blk, blksz))
					sample_good |= (1 << test_sample);
			}
		} else {
			dev_dbg(&mmc->class_dev,
				"Tuning error: cmd.error:%d, data.error:%d\n",
				cmd.error, data.error);
		}
	}

	kfree(tuning_blk);

	/*
	 * Get at middle clock sample values.
	 */
	best_sample = find_median_of_bits(host, sample_good);

	dev_info(host->dev, "sample_good: 0x %02x best_sample: 0x %02x\n",
			sample_good, best_sample);
	if (best_sample >= 0) {
		host->pdata->clk_smpl = best_sample;
		if (host->pdata->only_once_tune)
			host->pdata->tuned = true;
		dw_mci_exynos_set_sample(host, best_sample, false);
		return 0;
	}

	/* Failed. Just restore and return error */
	mci_writel(host, CDTHRCTL, 0 << 16 | 0);
	dw_mci_exynos_set_sample(host, orig_sample, false);
	return -EIO;
}

static int dw_mci_exynos_misc_control(struct dw_mci *host, enum dw_mci_misc_control control)
{
	u8 sample = host->pdata->clk_smpl;
	switch (control) {
	case CTRL_SET_CLK_SAMPLE:
		dw_mci_exynos_set_sample(host, sample, false);
		break;
	default:
		dev_err(host->dev, "dw_mmc exynos: wrong case\n");
		return -ENODEV;
	}

	return 0;
}

/* Common capabilities of Exynos4/Exynos5 SoC */
static unsigned long exynos_dwmmc_caps[4] = {
	MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
		MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23,
	MMC_CAP_CMD23,
	MMC_CAP_CMD23 | MMC_CAP_UHS_SDR104,
	MMC_CAP_CMD23,
};

static const struct dw_mci_drv_data exynos_drv_data = {
	.caps			= exynos_dwmmc_caps,
	.init			= dw_mci_exynos_priv_init,
	.setup_clock		= dw_mci_exynos_setup_clock,
	.prepare_command	= dw_mci_exynos_prepare_command,
	.register_dump		= dw_mci_exynos_register_dump,
	.set_ios		= dw_mci_exynos_set_ios,
	.parse_dt		= dw_mci_exynos_parse_dt,
	.cfg_smu		= dw_mci_exynos_cfg_smu,
	.execute_tuning		= dw_mci_exynos_execute_tuning,
	.misc_control		= dw_mci_exynos_misc_control,
#ifdef	CONFIG_CPU_IDLE
	.register_notifier	= dw_mci_exynos_register_notifier,
	.unregister_notifier	= dw_mci_exynos_unregister_notifier,
#endif
};

static const struct of_device_id dw_mci_exynos_match[] = {
	{ .compatible = "samsung,exynos4412-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5250-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5422-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5430-dw-mshc",
			.data = &exynos_drv_data, },
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_exynos_match);

static int dw_mci_exynos_probe(struct platform_device *pdev)
{
	const struct dw_mci_drv_data *drv_data;
	const struct of_device_id *match;

	match = of_match_node(dw_mci_exynos_match, pdev->dev.of_node);
	drv_data = match->data;
	return dw_mci_pltfm_register(pdev, drv_data);
}

static struct platform_driver dw_mci_exynos_pltfm_driver = {
	.probe		= dw_mci_exynos_probe,
	.remove		= __exit_p(dw_mci_pltfm_remove),
	.driver		= {
		.name		= "dwmmc_exynos",
		.of_match_table	= dw_mci_exynos_match,
		.pm		= &dw_mci_pltfm_pmops,
	},
};

module_platform_driver(dw_mci_exynos_pltfm_driver);

MODULE_DESCRIPTION("Samsung Specific DW-MSHC Driver Extension");
MODULE_AUTHOR("Thomas Abraham <thomas.ab@samsung.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dwmmc-exynos");
