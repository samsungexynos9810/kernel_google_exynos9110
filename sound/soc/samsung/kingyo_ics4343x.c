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
#include <linux/delay.h>

#include "i2s.h"
#include "i2s-regs.h"

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
	#define gprintk(fmt, x... ) printk( "%s: " fmt, __FUNCTION__ , ## x)
#else
	#define gprintk(x...) do { } while (0)
#endif

/*
 * TODO: we should bind "Samsung mixer component" to control its registers
 * however we must do a lot of things to do by using kcontrol,
 * e.g. define Widget, route, etc.
 * Kingyo must just initialize mixer only for recording so we had better
 * simply write registers with static configuration data.
 * Namely undefine the following pre-processor value.
 */
#ifdef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
/*
 * define this if you use dapm and calling snd_soc_dapm_sync is neccessary.
 */
#define CALL_DAPM_SYNC

#else
/*
 * If We must set divider of mixer clock for each when hw_setting
 */
#define SET_MIXER_CLK_AT_STREAM

/*
 * We should use clock api rather than this iomapping method,
 * so we must change it if it would work correctly.
 */
#define MIXER_CLK_IOREGMAP

#define AUDMIXER_SYS_CLK_FREQ_48KHZ	(24576000U + 100)

#endif

/*
 * define this, if we use dts node to define cpu/codec dai name
 */
//#define DAI_DEFINE_BY_DTS

/*
 * We must move I2S IO power-supply from this card driver to cpu_dai driver(i2s)
 * because i2s power must supply at first before startup of cpu_dai driver.
 */
//#define I2S_POWER_SUPPLY

/*
 * The following paramters are set in bootloader. These paramters are no longer
 * used here, they are accessed by audio_fw.c in platform HAL layser throgh
 * this machine driver layer.
 */

int MicGain = 8;
int IDVol = 138;
module_param(MicGain, int, S_IRUGO|S_IWUSR);
module_param(IDVol, int, S_IRUGO|S_IWUSR);

#ifndef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
#ifdef MIXER_CLK_IOREGMAP
static void __iomem *dispaud_vclk;
#define PA_CMU_DISPAUD				0x148D0000
#define CLK_CON_DIV_CLK_DISPAUD_MIXER 0x410
#endif
#include "../codecs/exynos-audmixer-regs.h"
static void __iomem *audmixer_reg;
#define MIXER_AUD					0x14880000
#endif

#ifdef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
static const struct snd_soc_component_driver smdk7270_cmpnt = {
	.name = "smdk7270-audio",
};

static struct snd_soc_dai_driver smdk7270_ext_dai[] = {
};
#endif

#ifdef I2S_POWER_SUPPLY
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
#endif

#ifndef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
static int set_mixer_clock(struct snd_soc_card *card)
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
	struct clk *div_mixer = devm_clk_get(card->dev, "audmixer_dout");
	if (IS_ERR(div_mixer)) {
		gprintk("audmixer_dout clk not found\n");
		return -ENOMEM;
	}
	if ( clk_set_rate(div_mixer, AUDMIXER_SYS_CLK_FREQ_48KHZ) ) {
		gprintk("clk_set audmixer_dout return error\n");
	}
	devm_clk_put(card->dev,div_mixer);
#endif
	return 0;
}

static unsigned int mixer_config_regs[] = {
	AUDMIXER_REG_10_DMIX1, 0x00,
	AUDMIXER_REG_11_DMIX2, 0x00,		// 0x80
	AUDMIXER_REG_0F_DIG_EN, 0x08,
	AUDMIXER_REG_0D_RMIX_CTL, 0x00,		// 0x80
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
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int rfs=256, bfs=64;
	int ret = 0;
	gprintk("fs=%u\n", params_rate(params));

	/* Do we need call here again as another module overwrite this register */
	#ifdef SET_MIXER_CLK_AT_STREAM
	set_mixer_clock(rtd->card);
	#endif

	/* We need to call sysclock for i2s here */
	snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK, 0, MOD_OPCLK_PCLK);
	snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK, rfs, SND_SOC_CLOCK_OUT);
	snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_RCLK, rfs);

	/* The following setting mixer register might be removed after
	 * mixer driver funcions would be supported */
	#ifdef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
	{
		struct snd_soc_card *card = rtd->card;
		struct snd_soc_dai *amixer_dai = rtd->codec_dais[0];
		#if 1
		struct snd_soc_dai *dai = amixer_dai;
		int ratio = bfs;
			if (dai->driver && dai->driver->ops->set_bclk_ratio) {
				ret = dai->driver->ops->set_bclk_ratio(dai, ratio);
				if ( ret != 0) {
					gprintk("result error %d\n",ret);
				}
			} else {
				if (!dai->driver)
					gprintk("parameter driver error\n");
				else
					gprintk("parameter ratio error\n");
				return -EINVAL;
			}
		#else
		ret = snd_soc_dai_set_bclk_ratio(amixer_dai, bfs);
		#endif
		if (ret < 0) {
			dev_err(card->dev, "aif1: Failed to configure mixer\n");
			return ret;
		}
	}
	#else
		ret = config_audmixer();
	#endif

	gprintk("exit\n");
	return ret;
}


struct kingyo_priv {
	struct delayed_work mute_work;
};

#define MUTE_DELAY_TIME 250

static void mute_delaywork(struct work_struct *work)
{
	writel(0x80, audmixer_reg + AUDMIXER_REG_0D_RMIX_CTL);
	writel(0x80, audmixer_reg + AUDMIXER_REG_11_DMIX2);
}

static int kingyo_trigger(
	struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct kingyo_priv *priv = snd_soc_card_get_drvdata(rtd->card);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		queue_delayed_work(system_power_efficient_wq, &priv->mute_work,
			msecs_to_jiffies(MUTE_DELAY_TIME));
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		cancel_delayed_work_sync(&priv->mute_work);
		break;
	}

	return ret;
}

static struct snd_soc_ops kingyo_ops = {
#ifdef I2S_POWER_SUPPLY
	.startup = kingyo_aif_startup,
	.shutdown = kingyo_aif_shutdown,
#endif
	.hw_params = kingyo_hw_params,
	.trigger = kingyo_trigger,
};

#ifdef CALL_DAPM_SYNC
static int kingyo_ics434_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(rtd->codec);

	/* scan and power dapm paths */
	snd_soc_dapm_sync(dapm);

	return 0;
}
#endif

#ifdef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
static struct snd_soc_dai_link_component codecs_ap0[] = {{
		.name = "14880000.s1403x",
		.dai_name = "AP0",
	}, {
		.name = "ics43432",
		.dai_name = "ics43432-hifi",
	},
};
#endif

/*
 * FE DAI links is just only for Capture by MiC
 * dai_fmt: configure initial format to i2s driver
 */
static struct snd_soc_dai_link kingyo_dai[] = {
	/* Recording mic */
	{
		.name = "ICS43432 Direct",
		.stream_name = "Capture",
#ifdef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
		.codecs = codecs_ap0,
		.num_codecs = ARRAY_SIZE(codecs_ap0),
#else
		.codec_name = "ics43432",
		.codec_dai_name = "ics43432-hifi",
#endif
#ifdef CALL_DAPM_SYNC
		.init = kingyo_ics434_init,
#endif
#ifndef CPU_DAI_BY_DTS
		.cpu_dai_name = "148a0000.i2s",
		.platform_name = "148a0000.i2s",
#endif
		.ops = &kingyo_ops,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_NB_NF,
	},
};

static int kingyo_probe(struct snd_soc_card *card) {

	gprintk("\n");

#ifdef I2S_POWER_SUPPLY
	{
		int ret;
		ret = devm_regulator_bulk_get(card->dev, 1, core_supplies);
		if (ret != 0) {
			dev_err(card->dev, "Failed to request core supplies: %d\n",ret);
			return -ENOMEM;
		}
	}
#endif

#ifndef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
	/* set CLK_DISPAUD_MIXER DIV_RATIO */
	set_mixer_clock(card);

	/* iomapping audio mixer */
	map_audiomixer();
#endif

	return 0;
}

static int kingyo_remove(struct snd_soc_card *card) {
	gprintk("\n");
	return 0;
}

#ifdef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
static int audmixer_init(struct snd_soc_component *cmp)
{
	gprintk("\n");
	return 0;
}

static struct snd_soc_aux_dev audmixer_aux_dev[] = {
	{
		.name = "S1403X",
		.init = audmixer_init,
	},
};

static struct snd_soc_codec_conf audmixer_codec_conf[] = {
	{
		.name_prefix = "AudioMixer",
	},
};
#endif

static struct snd_soc_card kingyo = {
	.name = "KINGYO-ICS4343X",
	.probe = kingyo_probe,
	.remove = kingyo_remove,
	.dai_link = kingyo_dai,
	.num_links = ARRAY_SIZE(kingyo_dai),
#ifdef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
	.aux_dev = audmixer_aux_dev,
	.num_aux_devs = ARRAY_SIZE(audmixer_aux_dev),
	.codec_conf = audmixer_codec_conf,
	.num_configs = ARRAY_SIZE(audmixer_codec_conf),
#endif
};

static int kingyo_ics4343X_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &kingyo;
	struct kingyo_priv *priv;
	int ret;

	gprintk("\n");
	if (!np) {
		dev_err(&pdev->dev, "Failed to get device node\n");
		return -EINVAL;
	}

	card->dev = &pdev->dev;

	#ifdef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
	{
		int n;
		struct device_node *auxdev_np;
		gprintk("register_component\n");
		ret = snd_soc_register_component(&pdev->dev, &smdk7270_cmpnt,
				smdk7270_ext_dai,	0);
		if (ret) {
			dev_err(&pdev->dev, "Failed to register component: %d\n", ret);
			return ret;
		}

		for (n = 0; n < ARRAY_SIZE(audmixer_aux_dev); n++) {
			auxdev_np = of_parse_phandle(np, "samsung,auxdev", n);
			if (!auxdev_np) {
				dev_err(&pdev->dev,
					"Property 'samsung,auxdev' missing\n");
				return -EINVAL;
			}
			gprintk("auxdev[%d] of_node\n",n);
			#ifdef AUX_DEV_DEFINE
			audmixer_aux_dev[n].codec_of_node = auxdev_np;
			#endif
			audmixer_codec_conf[n].of_node = auxdev_np;
		}
	}
	#endif

#ifdef DAI_DEFINE_BY_DTS
	{
		struct device_node *cpu_np, *codec_np;

		gprintk("cpu and codecs of_node\n");
		for (n = 0; n < ARRAY_SIZE(kingyo_dai); n++) {
			/* Skip parsing DT for fully formed dai links */
			if (kingyo_dai[n].platform_name && kingyo_dai[n].codec_name) {
				dev_dbg(card->dev,
				"Skipping dt for populated dai link %s\n",
				kingyo_dai[n].name);
				continue;
			}

			codec_np = of_parse_phandle(np, "samsung,audio-codec", n);
			if (!codec_np) {
				dev_err(&pdev->dev,	"Property 'samsung,audio-codec' missing\n");
				break;
			}
			#ifdef CONFIG_SND_SAMSUNG_MIXER_COMPONENT
			kingyo_dai[n].codecs[1].of_node = codec_np;
			#endif


			cpu_np = of_parse_phandle(np, "samsung,audio-cpu", n);
			if (!cpu_np) {
				dev_err(&pdev->dev,	"Property 'samsung,audio-cpu' missing\n");
				break;
			}

			if (!kingyo_dai[n].cpu_dai_name)
				kingyo_dai[n].cpu_of_node = cpu_np;
			if (!kingyo_dai[n].platform_name)
				kingyo_dai[n].platform_of_node = cpu_np;
			if (!kingyo_dai[n].codec_dai_name)
				kingyo_dai[n].codec_of_node = cpu_np;

		}
	}
#endif

	gprintk("register card\n");
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	INIT_DELAYED_WORK(&priv->mute_work, mute_delaywork);
	platform_set_drvdata(pdev, priv);
	snd_soc_card_set_drvdata(card, priv);
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

MODULE_DESCRIPTION("ALSA SoC Driver for Kingyo Board");
MODULE_LICENSE("GPL");
