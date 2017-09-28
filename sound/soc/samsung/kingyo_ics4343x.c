/*
 *  kingyo_4343X.c
 *
 * ALSA Machine Audio Layer - Glue Samsung SOC and ics43432 codec
 *
 *  Copyright (c) 2017. CASIO COMMPUTER CO., LTD.
 *
 *  Author: keisak <keisak@casio.co.jp>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/clk.h>

#include <sound/soc.h>
#include <linux/regulator/consumer.h>

#if 1
	#define gprintk(fmt, x... ) printk( "%s: " fmt, __FUNCTION__ , ## x)
#else
	#define gprintk(x...) do { } while (0)
#endif

/*
 * The following paramters are set in bootloader. These paramters are no longer
 * used here, they are accessed by audio_fw.c in platform HAL layser throgh
 * this machine driver layer.
 */

int MicGain = 1;
int IDVol = 16;
module_param(MicGain, int, S_IRUGO|S_IWUSR);
module_param(IDVol, int, S_IRUGO|S_IWUSR);


static struct regulator_bulk_data core_supplies[] = {
	{
	.supply = "vdd_18",
	}
};

static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *aud_pll;
	int ret = 0;

	aud_pll = clk_get(NULL, "aud_pll");
	if (IS_ERR(aud_pll)) {
		pr_err("%s: failed to get aud_pll\n", __func__);
		return PTR_ERR(aud_pll);
	}

	if (rate == clk_get_rate(aud_pll))
		goto out;

	if (clk_set_rate(aud_pll, rate)!=0) {
		pr_err("%s: failed set aud_pll rate:", __func__);
		ret = -EIO;
	}

	gprintk("write:%ld , read:%ld\n", rate, clk_get_rate(aud_pll));
out:
	clk_put(aud_pll);

	return ret;
}


/*
 * kingyo hw params. (AP I2S Master with mic)
 */
static int kingyo_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	int ret;
	unsigned int fs, aud_pll_rate;

	gprintk("\n");

	fs = params_rate(params);

	/* rate = 64(rfs) * 48K/44.1(fs) * N is about equal to  98.304MHz */
	if ( fs % 8000 == 0 ) {
		aud_pll_rate = 98304000; /* N = 32 */
	} else {
		aud_pll_rate = 90316800; /* N = 32 */
	}

	gprintk("fs: %u, pll: %u\n", fs, aud_pll_rate);
	ret = set_aud_pll_rate(aud_pll_rate);
	if (ret) {
		pr_err("%s: set_aud_pll_rate return error %d\n", __func__, ret);
		return ret;
	}

	return 0;
}


static struct snd_soc_ops kingyo_ops = {
	.hw_params = kingyo_hw_params,
};

static int kingyo_ics434_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(rtd->codec);

	/* scan and power dapm paths */
	snd_soc_dapm_sync(dapm);

	return 0;
}

/*
 * dai_fmt: configure initial format to i2s driver
 */
static struct snd_soc_dai_link kingyo_dai[] = {
	{
		.name = "ICS43432 HiFi",
		.stream_name = "Capture",
		.codec_dai_name = "ics43432-hifi",
		.codec_name = "ics43432",
		.init = kingyo_ics434_init,
		.ops = &kingyo_ops,
		.cpu_dai_name = "148A0000.i2s0",
		.platform_name = "148A0000.i2s0",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_NB_NF,
	},
};

static int kingyo_probe(struct snd_soc_card *card) {
	int ret;

	gprintk("\n");

	ret = devm_regulator_bulk_get(card->dev, 1, core_supplies);
	if (ret != 0) {
		dev_err(card->dev, "Failed to request core supplies: %d\n",ret);
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(core_supplies), core_supplies);
	if (ret != 0) {
		dev_err(card->dev, "Failed to enable core supplies: %d\n",ret);
	}


	return 0;
}

static int kingyo_remove(struct snd_soc_card *card) {
	gprintk("\n");
	return 0;
}

static int kingyo_suspend_pre(struct snd_soc_card *card) {
	gprintk("\n");
	return 0;
}

static int kingyo_suspend_post(struct snd_soc_card *card) {
	int ret;
	gprintk("\n");
	ret = regulator_bulk_disable(ARRAY_SIZE(core_supplies), core_supplies);
	if (ret != 0) {
		dev_err(card->dev, "Failed to disable supplies: %d\n", ret);
		return ret;
	}
	return 0;
}
static int kingyo_resume_pre(struct snd_soc_card *card) {
	int ret;
	gprintk("\n");

	ret = regulator_bulk_enable(ARRAY_SIZE(core_supplies), core_supplies);

	if (ret != 0) {
		dev_err(card->dev, "Failed to enable supplies: %d\n", ret);
		return ret;
	}

	return 0;
}
static int kingyo_resume_post(struct snd_soc_card *card) {
	gprintk("\n");
	return 0;
}


static struct snd_soc_card kingyo = {
	.name = "KINGYO-ICS4343X",
	.probe = kingyo_probe,
	.remove = kingyo_remove,
	/* the pre and post PM functions are used to do any PM work before and
	 * after the codec and DAIs do any PM work. */
	.suspend_pre = kingyo_suspend_pre,
	.suspend_post = kingyo_suspend_post,
	.resume_pre = kingyo_resume_pre,
	.resume_post = kingyo_resume_post,
	.dai_link = kingyo_dai,
	.num_links = ARRAY_SIZE(kingyo_dai),
};

static int kingyo_ics4343X_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card = &kingyo;
	struct clk *aud_pll;

	gprintk("start \n");
	/* register card */
	card->dev = &pdev->dev;

	aud_pll = clk_get(NULL, "aud_pll");
	if (IS_ERR(aud_pll)) {
		ret = -EINVAL;
		pr_err("%s: failed to get aud_pll\n", __func__);
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		goto err_out;
	}

	clk_prepare_enable(aud_pll);

	gprintk("ics4343X-dai: register card %s -> %s\n", card->dai_link->codec_dai_name, card->dai_link->cpu_dai_name);
err_out:
	clk_put(aud_pll);
	return ret;

}
static int kingyo_ics4343X_remove(struct platform_device *pdev)
{
	gprintk("remove\n");
	snd_soc_unregister_card(&kingyo);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id kingyo_ics4343X_of_match[] = {
	{ .compatible = "samsung,kingyo-4343X", },
	{},
};
MODULE_DEVICE_TABLE(of, kingyo_ics4343X_of_match);
#endif /* CONFIG_OF */

static struct platform_driver kingyo_ics4343X_driver = {
	.driver = {
		.name = "KINGYO-ICS4343X",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(kingyo_ics4343X_of_match),
	},
	.probe = kingyo_ics4343X_probe,
	.remove = kingyo_ics4343X_remove,
};
module_platform_driver(kingyo_ics4343X_driver);

MODULE_AUTHOR("keisak <keisak@casio.co.jp>");
MODULE_DESCRIPTION("ALSA SoC Driver for Kingyo Board");
MODULE_LICENSE("GPL");
