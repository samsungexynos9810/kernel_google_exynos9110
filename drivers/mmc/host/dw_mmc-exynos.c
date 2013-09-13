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
#include <linux/slab.h>	/* for kmalloc/kfree prototype */

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"

#define NUM_PINS(x)			(x + 2)

#define SDMMC_CLKSEL			0x09C
#define SDMMC_CLKSEL_CCLK_SAMPLE(x)	(((x) & 7) << 0)
#define SDMMC_CLKSEL_CCLK_DRIVE(x)	(((x) & 7) << 16)
#define SDMMC_CLKSEL_CCLK_DIVIDER(x)	(((x) & 7) << 24)
#define SDMMC_CLKSEL_GET_DRV_WD3(x)	(((x) >> 16) & 0x7)
#define SDMMC_CLKSEL_GET_DIVRATIO(x)	((((x) >> 24) & 0x7) + 1)
#define SDMMC_CLKSEL_TIMING(x, y, z)	(SDMMC_CLKSEL_CCLK_SAMPLE(x) |	\
					SDMMC_CLKSEL_CCLK_DRIVE(y) |	\
					SDMMC_CLKSEL_CCLK_DIVIDER(z))

#define SDMMC_CMD_USE_HOLD_REG		BIT(29)

/* Block number in eMMC */
#define DWMCI_BLOCK_NUM			0xFFFFFFFF

#define DWMCI_EMMCP_BASE		0x1000
#define DWMCI_MPSECURITY		(DWMCI_EMMCP_BASE + 0x0010)
#define DWMCI_MPSBEGIN0			(DWMCI_EMMCP_BASE + 0x0200)
#define DWMCI_MPSEND0			(DWMCI_EMMCP_BASE + 0x0204)
#define DWMCI_MPSCTRL0			(DWMCI_EMMCP_BASE + 0x020C)

/* SMU control bits */
#define DWMCI_MPSCTRL_SECURE_READ_BIT		BIT(7)
#define DWMCI_MPSCTRL_SECURE_WRITE_BIT		BIT(6)
#define DWMCI_MPSCTRL_NON_SECURE_READ_BIT	BIT(5)
#define DWMCI_MPSCTRL_NON_SECURE_WRITE_BIT	BIT(4)
#define DWMCI_MPSCTRL_USE_FUSE_KEY		BIT(3)
#define DWMCI_MPSCTRL_ECB_MODE			BIT(2)
#define DWMCI_MPSCTRL_ENCRYPTION		BIT(1)
#define DWMCI_MPSCTRL_VALID			BIT(0)

#define EXYNOS4210_FIXED_CIU_CLK_DIV	2
#define EXYNOS4412_FIXED_CIU_CLK_DIV	4

/* Variations in Exynos specific dw-mshc controller */
enum dw_mci_exynos_type {
	DW_MCI_TYPE_EXYNOS4210,
	DW_MCI_TYPE_EXYNOS4412,
	DW_MCI_TYPE_EXYNOS5250,
	DW_MCI_TYPE_EXYNOS5430,
};

/* Exynos implementation specific driver private data */
struct dw_mci_exynos_priv_data {
	enum dw_mci_exynos_type		ctrl_type;
	u8				ciu_div;
	u32				sdr_timing;
	u32				ddr_timing;
};

/*
 * Tunning patterns are from emmc4.5 spec section 6.6.7.1
 * Figure 27 (for 8-bit) and Figure 28 (for 4bit).
 */
static const u8 tuning_blk_pattern_4bit[] = {
	0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
	0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
	0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
	0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
	0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
	0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
	0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
	0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

static const u8 tuning_blk_pattern_8bit[] = {
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
	0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
	0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
	0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
	0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
	0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
	0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
	0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
	0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
	0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
	0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
	0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
	0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

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

/*
 * By-pass Security Management Unit
 */
void dw_mci_exynos_cfg_smu(struct dw_mci *host)
{
	/* Set the start and end region to configure */
	__raw_writel(0, host->regs + DWMCI_MPSBEGIN0);
	__raw_writel(DWMCI_BLOCK_NUM, host->regs + DWMCI_MPSEND0);
	__raw_writel(DWMCI_MPSCTRL_SECURE_READ_BIT |
		DWMCI_MPSCTRL_SECURE_WRITE_BIT |
		DWMCI_MPSCTRL_NON_SECURE_READ_BIT |
		DWMCI_MPSCTRL_NON_SECURE_WRITE_BIT |
		DWMCI_MPSCTRL_VALID, host->regs + DWMCI_MPSCTRL0);
}

static int dw_mci_exynos_setup_clock(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (priv->ctrl_type == DW_MCI_TYPE_EXYNOS5250 ||
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
	u32 ciu_div = priv->ciu_div + 1;	/* aka DIVRATIO */
	int clkerr = 0;
	struct clk *adiv = clk_get(host->dev, "dout_mmc0_a");
	struct clk *bdiv = clk_get(host->dev, "dout_mmc0_b");

	/* Try to set the upstream clock rate */
	if (ciu_rate != (want_bus_hz * ciu_div)) {
		ciu_rate = want_bus_hz * ciu_div;
		clkerr = clk_set_rate(adiv, ciu_rate);
		if (clkerr)
			dev_warn(host->dev, "Couldn't set rate to %u\n",
				 ciu_rate);
		clkerr = clk_set_rate(bdiv, ciu_rate);
		if (clkerr)
			dev_warn(host->dev, "Couldn't set rate to %u\n",
				 ciu_rate);
		ciu_rate = clk_get_rate(host->ciu_clk);
	}

	host->bus_hz = ciu_rate / ciu_div;
}

static void dw_mci_exynos_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (ios->timing == MMC_TIMING_UHS_DDR50) {
		mci_writel(host, CLKSEL, priv->ddr_timing);
		/* Exynos wants 2x clock input only for DDR timing */
		dw_mci_exynos_set_bus_hz(host, 2 * 50 * 1000 * 1000);
	} else if (ios->timing == MMC_TIMING_MMC_HS200) {
		/* We'll keep the timing from earlier calls to set_ios */
		dw_mci_exynos_set_bus_hz(host, 200 * 1000 * 1000);
	} else {
		mci_writel(host, CLKSEL, priv->sdr_timing);
	}
}

static int dw_mci_exynos_parse_dt(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	struct device_node *np = host->dev->of_node;
	u32 timing[2];
	u32 div = 0;
	int ret;

	of_property_read_u32(np, "samsung,dw-mshc-ciu-div", &div);
	priv->ciu_div = div;

	ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-sdr-timing", timing, 2);
	if (ret)
		return ret;

	priv->sdr_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], div);

	ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-ddr-timing", timing, 2);
	if (ret)
		return ret;

	priv->ddr_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], div);
	return 0;
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

/*
 * After testing all (8) possible clock sample values and using one bit for
 * each value that works, return the "middle" bit position of any sequential
 * 5 bits.
 */
static int find_median_of_5bits(unsigned int map)
{
	unsigned int i, testbits;

	/* replicate the map so "arithimetic shift right" shifts in
	 * the same bits "again". e.g. portable "Rotate Right" bit operation.
	 */
	testbits = map | (map << 8);

/* Middle is bit 2. */
#define FIVEBITS 0x1f

	for (i = 2; i < (8 + 2); i++, testbits >>= 1) {
		if ((testbits & FIVEBITS) == FIVEBITS)
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

	tuning_blk = kmalloc(blksz, GFP_KERNEL);
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
			if (!memcmp(tuning_blk_pattern, tuning_blk, blksz))
				sample_good |= (1 << test_sample);
		} else {
			dev_dbg(&mmc->class_dev,
				"Tuning error: cmd.error:%d, data.error:%d\n",
				cmd.error, data.error);
		}
	}

	kfree(tuning_blk);

	/*
	 * See if we got at least 5 consectutive clock sample values
	 * that worked.
	 */
	best_sample = find_median_of_5bits(sample_good);
	if (best_sample >= 0) {
		host->pdata->clk_smpl = best_sample;
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
	MMC_CAP_CMD23,
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
};

static const struct of_device_id dw_mci_exynos_match[] = {
	{ .compatible = "samsung,exynos4412-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5250-dw-mshc",
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
