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
#include <linux/io.h>

#include <sound/soc.h>
#include <linux/regulator/consumer.h>

#include "i2s.h"
#include "i2s-regs.h"

#if 0
	#define gprintk(fmt, x... ) printk( "%s: " fmt, __FUNCTION__ , ## x)
#else
	#define gprintk(x...) do { } while (0)
#endif

#define AUDMIXER
#define IOREGMAP

/*
 * The following paramters are set in bootloader. These paramters are no longer
 * used here, they are accessed by audio_fw.c in platform HAL layser throgh
 * this machine driver layer.
 */

int MicGain = 1;
int IDVol = 16;
module_param(MicGain, int, S_IRUGO|S_IWUSR);
module_param(IDVol, int, S_IRUGO|S_IWUSR);

/*
 * We should use clock api instead if they would be supported
 */
#ifdef IOREGMAP
static void __iomem *dispaud_vclk;

// #define CMU_DISPAUD_REG(x)     (disaud_vclk + (x))

#define PA_CMU_DISPAUD				0x148D0000
#define CLK_CON_DIV_CLK_DISPAUD_MIXER 0x410

#endif

#ifdef AUDMIXER
struct kingyo_priv {
	struct snd_soc_codec *codec;
	int aifrate;
};

static const struct snd_soc_component_driver smdk7270_cmpnt = {
	.name = "smdk7270-audio",
};

static struct snd_soc_dai_driver smdk7270_ext_dai[] = {
};

#endif

static struct regulator_bulk_data core_supplies[] = {
	{
	.supply = "vdd_18",
	}
};

static int kingyo_aif_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	int ret;

	gprintk("\n");
	ret = regulator_bulk_enable(ARRAY_SIZE(core_supplies), core_supplies);

	if (ret != 0) {
		dev_err(card->dev, "Failed to enable supplies: %d\n", ret);
		return ret;
	}


	return 0;
}

static void kingyo_aif_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	int ret;

	gprintk("\n");
	ret = regulator_bulk_disable(ARRAY_SIZE(core_supplies), core_supplies);
	if (ret != 0) {
		dev_err(card->dev, "Failed to disable supplies: %d\n", ret);
	}

}

static int set_mixer_div(void)
{
#ifdef IOREGMAP
	dispaud_vclk = ioremap(PA_CMU_DISPAUD, SZ_4K);
	if (dispaud_vclk == NULL) {
		gprintk("CMU_DISPAUD ioremap failed\n");
		return -ENOMEM;
	}
	writel(0x1, dispaud_vclk + CLK_CON_DIV_CLK_DISPAUD_MIXER);
#else
	div_mixer = devm_clk_get(dev, "div_audmixer");
	if (IS_ERR(div_mixer)) {
		gprintk("clk_get div_mixer return error\n");
		return -ENOMEM;
	}
	if ( clk_set_rate(div_mixer, 3) ) {
		gprintk("clk_set div_mixer return error\n");
	}
	devm_clk_put(dev,div_mixer);
#endif
	return 0;
}

/*
 * kingyo hw params. (AP I2S Master with mic)
 */
static int kingyo_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{

	gprintk("\n");

	#if 0
	{
		struct snd_soc_pcm_runtime *rtd = substream->private_data;
		struct struct platform_device *card = rtd->card->dev;
		unsigned int fs, aud_pll_rate;
		int ret;
		clk *aud_pll, * div_mixer;

		fs = params_rate(params);
		/* rate = 64(rfs) * 48K/44.1(fs) * N is about equal to  98.304MHz */
		if ( fs % 8000 == 0 ) {
			aud_pll_rate = 98304019; /* N = 32 */
		} else {
			aud_pll_rate = 90316800; /* N = 32 */
		}
		gprintk("fs: %u, pll: %u\n", fs, aud_pll_rate);
		aud_pll = devm_clk_get(dev, "fout_aud_pll");
		if (IS_ERR(aud_pll)) {
			gprintk("clk_get aud_pll return error\n");
		} else {
			clk_set_rate(aud_pll, aud_pll_rate);
			devm_clk_put(dev,aud_pll);
		}
	}
	#endif

	/* Do we need call here again as another module overwrite this register */
	set_mixer_div();

	/* Do we need to call sysclock here */
	{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int rfs=256, bfs=64;
	snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK, 0, MOD_OPCLK_PCLK);
	snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK, rfs, SND_SOC_CLOCK_OUT);
	snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_RCLK, rfs);
	}
	gprintk("exit\n");
	return 0;
}


static struct snd_soc_ops kingyo_ops = {
	.startup = kingyo_aif_startup,
	.shutdown = kingyo_aif_shutdown,
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
		.cpu_dai_name = "148a0000.i2s",
		.platform_name = "148a0000.i2s",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_NB_NF,
	},
};

static int kingyo_probe(struct snd_soc_card *card) {
	int ret;

	gprintk("\n");

	ret = devm_regulator_bulk_get(card->dev, 1, core_supplies);
	if (ret != 0) {
		dev_err(card->dev, "Failed to request core supplies: %d\n",ret);
		return -ENOMEM;
	}

	/* set CLK_DISPAUD_MIXER DIV_RATIO */
	set_mixer_div();

	return 0;
}

static int kingyo_remove(struct snd_soc_card *card) {
	gprintk("\n");
	return 0;
}


static struct snd_soc_card kingyo = {
	.name = "KINGYO-ICS4343X",
	.probe = kingyo_probe,
	.remove = kingyo_remove,
	.dai_link = kingyo_dai,
	.num_links = ARRAY_SIZE(kingyo_dai),
};

static int kingyo_ics4343X_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card = &kingyo;

	gprintk("start \n");
	/* register card */
	card->dev = &pdev->dev;

	#ifdef AUDMIXER
	{
		struct kingyo_priv *priv;
		priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
		if (!priv)
			return -ENOMEM;
		gprintk("register_component\n");
		ret = snd_soc_register_component(&pdev->dev, &smdk7270_cmpnt,
				smdk7270_ext_dai,	0);
		if (ret) {
			dev_err(&pdev->dev, "Failed to register component: %d\n", ret);
			return ret;
		}
		snd_soc_card_set_drvdata(card, priv);
	}
	#endif

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		return ret;
	}
	gprintk("ics4343X-dai: register card %s -> %s\n", card->dai_link->codec_dai_name, card->dai_link->cpu_dai_name);

	/* audio clock prepare is done in lpass driver */

	return ret;

}
static int kingyo_ics4343X_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	gprintk("remove\n");
	snd_soc_unregister_card(card);
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
