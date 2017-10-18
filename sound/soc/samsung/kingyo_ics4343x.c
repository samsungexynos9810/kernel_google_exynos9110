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

#if 1
	#define gprintk(fmt, x... ) printk( "%s: " fmt, __FUNCTION__ , ## x)
#else
	#define gprintk(x...) do { } while (0)
#endif

/*
 * TODO: Do we need register Audio component to control it.
 * If we do not use its function, we should remove this.
 */
#define MIXER_COMPONENT
/*
 * We must set divider of mixer clock
 */
#define MIXER_SET_CLKDIV
/*
 * We should use clock api rather than this iomapping method,
 * so we must change it if they would support the clock
 */
#define MIXER_CLK_IOREGMAP
/*
 * We must configire mixer register here until audio mixer driver
 * will be supported.
 */
#define MIXER_CONTORL_IOREG

/*
 * The following paramters are set in bootloader. These paramters are no longer
 * used here, they are accessed by audio_fw.c in platform HAL layser throgh
 * this machine driver layer.
 */
int MicGain = 1;
int IDVol = 16;
module_param(MicGain, int, S_IRUGO|S_IWUSR);
module_param(IDVol, int, S_IRUGO|S_IWUSR);


#ifdef MIXER_CLK_IOREGMAP
static void __iomem *dispaud_vclk;
// #define CMU_DISPAUD_REG(x)     (disaud_vclk + (x))
#define PA_CMU_DISPAUD				0x148D0000
#define CLK_CON_DIV_CLK_DISPAUD_MIXER 0x410
#endif
#ifdef MIXER_CONTORL_IOREG
#include "../codecs/exynos-audmixer-regs.h"
static void __iomem *audmixer_reg;
#define MIXER_AUD					0x14880000
#endif


#ifdef MIXER_COMPONENT
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

#ifdef MIXER_SET_CLKDIV
static int set_mixer_div(void)
{
#ifdef MIXER_CLK_IOREGMAP
	if (dispaud_vclk == NULL) {
		dispaud_vclk = ioremap(PA_CMU_DISPAUD, SZ_4K);
		if (dispaud_vclk == NULL) {
			gprintk("CMU_DISPAUD ioremap failed\n");
			return -ENOMEM;
		}
	}
	writel(0x3, dispaud_vclk + CLK_CON_DIV_CLK_DISPAUD_MIXER);
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
#endif

#ifdef MIXER_CONTORL_IOREG
static unsigned int mixer_config_regs[] = {
	AUDMIXER_REG_10_DMIX1, 0x00,
	AUDMIXER_REG_11_DMIX2, 0x80,
	AUDMIXER_REG_0F_DIG_EN, 0x08,
	AUDMIXER_REG_0D_RMIX_CTL, 0x80,
};
static int map_audiomixer(void)
{
	audmixer_reg = ioremap(MIXER_AUD, SZ_1K);
	if (audmixer_reg == NULL) {
		gprintk("MIXER_AUD ioremap failed\n");
		return -ENOMEM;
	}
	return 0;
}
static int config_audmixer(void)
{
	int i;
	for (i=0; i<sizeof(ARRAY_SIZE(mixer_config_regs)); i+=2) {
		writel(mixer_config_regs[i+1],
				audmixer_reg + mixer_config_regs[i]);
	}
	return 0;
}
#endif
/*
 * kingyo hw params. (AP I2S Master with mic)
 */
static int kingyo_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{

	gprintk("\n");

	/* Do we need call here again as another module overwrite this register */
	#ifdef MIXER_SET_CLKDIV
	set_mixer_div();
	#endif

	/* Do we need to call sysclock for i2s here */
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

	/* The following setting mixer register might be removed after
	 * mixer driver funcions would be supported */
	#ifdef MIXER_CONTORL_IOREG
	config_audmixer();
	#endif

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

	#ifdef MIXER_SET_CLKDIV
	/* set CLK_DISPAUD_MIXER DIV_RATIO */
	set_mixer_div();
	#endif

	#ifdef MIXER_CONTORL_IOREG
	/* iomapping audio mixer */
	map_audiomixer();
	#endif

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

	#ifdef MIXER_COMPONENT
	{
		gprintk("register_component\n");
		ret = snd_soc_register_component(&pdev->dev, &smdk7270_cmpnt,
				smdk7270_ext_dai,	0);
		if (ret) {
			dev_err(&pdev->dev, "Failed to register component: %d\n", ret);
			return ret;
		}
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
