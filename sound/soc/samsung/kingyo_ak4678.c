/*
 *  kingyo_ak4678.c
 *
 *  Copyright (c) 2012, Insignal Co., Ltd.
 *
 *  Author: Claude <claude@insginal.co.kr>
 *          Nermy  <nermy@insignal.co.kr>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>

#include <sound/control.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <linux/regulator/consumer.h>
#include <linux/regulator/fixed.h>

#include "ak4678mic.h"
#include "i2s-regs.h"
#include "i2s.h"

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#define gprintk(fmt, x...) printk("%s: " fmt, __FUNCTION__, ##x)
#else
#define gprintk(x...) do { } while (0)
#endif

/* for sysfs to rd/wr register */
#define SYSFS_MIXER

/* for power management of device e.g. codec and mic amp */
#define AUDIOIF_POWER_SUPPLY

/* for configuration to codec */
#define AK4678_PLL_MASTER 3
#define AK4678_PLL_24MHZ (7 << 0)

/* control ak4678 codec */
/* load parameter from boot */
int MicGain = 13;
int IDVol = 169;
module_param(MicGain, int, S_IRUGO | S_IWUSR);
module_param(IDVol, int, S_IRUGO | S_IWUSR);
#define AK4678_07_MIC_AMP_GAIN 0x07
#define AK4678_11_LIN_VOLUME 0x11

/* control audio mixer */
static void __iomem *dispaud_vclk;
#define PA_CMU_DISPAUD 0x148D0000
#define CLK_CON_DIV_CLK_DISPAUD_MIXER 0x410
#include "../codecs/exynos-audmixer-regs.h"
static void __iomem *audmixer_reg;
#define MIXER_AUD 0x14880000

static int set_mixer_clock(struct snd_soc_card *card)
{
	if (dispaud_vclk == NULL) {
		dispaud_vclk = ioremap(PA_CMU_DISPAUD, SZ_4K);
		if (dispaud_vclk == NULL) {
			gprintk("CMU_DISPAUD ioremap failed\n");
			return -ENOMEM;
		}
	}
	writel(0x3, dispaud_vclk + CLK_CON_DIV_CLK_DISPAUD_MIXER);
	return 0;
}

static unsigned int mixer_config_regs[] = {
	AUDMIXER_REG_10_DMIX1, 0x00,
	AUDMIXER_REG_0F_DIG_EN, 0x00,
	AUDMIXER_REG_11_DMIX2, 0x00,
	AUDMIXER_REG_16_DOUTMX1, 0x01,
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
	for (i = 0; i < sizeof(ARRAY_SIZE(mixer_config_regs)); i += 2) {
		writel(mixer_config_regs[i + 1], audmixer_reg + mixer_config_regs[i]);
	}
	return 0;
}

/*
 * kingyo hw params. (AP I2S Master with mic)
 */

static int kingyo_hw_params(
	struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int rfs = 256, bfs = 64;
	int ret;

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

	/* Do not set CPU DAI configuration */

	/* configuration audio mixer without itself own driver */
	ret = config_audmixer();

	ret = snd_soc_dai_set_fmt(codec_dai,
		SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set codec mode fmt: %d\n", ret);
		return ret;
	}
	ret = snd_soc_dai_set_pll(codec_dai, AK4678_PMPLL | AK4678_BCKO, 0, 0, 0);
	if (ret < 0) {
		dev_err(
			card->dev, "Failed to set PLL master mode codec fmt: %d\n", ret);
		return ret;
	}

	/* TODO: we should use widget rather than sysclk api */
	gprintk("update MicGain=%d, IDVol=%d\n", MicGain, IDVol);
	snd_soc_dai_set_sysclk(codec_dai, AK4678_07_MIC_AMP_GAIN, MicGain, 0);
	snd_soc_dai_set_sysclk(codec_dai, AK4678_11_LIN_VOLUME, IDVol, 0);

	return ret;
}

#ifdef AUDIOIF_POWER_SUPPLY

static int kingyo_aif_startup(struct snd_pcm_substream *substream)
{
	gprintk("\n");
	return 0;
}

static void kingyo_aif_shutdown(struct snd_pcm_substream *substream)
{
	gprintk("\n");
}
#endif

static struct snd_soc_ops kingyo_ops = {
#ifdef AUDIOIF_POWER_SUPPLY
	.startup = kingyo_aif_startup,
	.shutdown = kingyo_aif_shutdown,
#endif
	.hw_params = kingyo_hw_params,
};

static int kingyo_ak4678_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
#if 0
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	snd_soc_dapm_sync(dapm);
#endif
	return 0;
}

static struct snd_soc_dai_link kingyo_dai[] = {
	{
		.name = "Kingyo_AK4678_Mic",
		.stream_name = "Capture",
		.codec_name = "ak4678mic.3-0012",
		.codec_dai_name = "ak4678-mic",
		.init = kingyo_ak4678_init_paiftx,
		.cpu_dai_name = "148a0000.i2s",
		.platform_name = "148a0000.i2s",
		.ops = &kingyo_ops,
	},
};

static int kingyo_probe(struct snd_soc_card *card)
{
	gprintk("\n");
	/* set CLK_DISPAUD_MIXER DIV_RATIO */
	set_mixer_clock(card);

	/* iomapping audio mixer */
	map_audiomixer();
	return 0;
}
static int kingyo_remove(struct snd_soc_card *card)
{
	gprintk("\n");
	return 0;
}

static struct snd_soc_card kingyo_card = {
	.name = "KINGYO-AK4678",
	.probe = kingyo_probe,
	.remove = kingyo_remove,
	.dai_link = kingyo_dai,
	.num_links = ARRAY_SIZE(kingyo_dai),
};

#ifdef SYSFS_MIXER

static u32 basepaddr = 0;
static u32 offaddr;
static void __iomem *basevaddr = NULL;

/*
 * store format :
 * 		set baseaddres : "a <paddress>"
 * 		write data :"w <offset>> <data>" => @(paddres + offset) = data
 * 		read data :	"r <offset>>" => @(paddres + offset) ! "echo mixer"
 */
static ssize_t mixer_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	char s[24];
	char *string = s;
	char *argv[3];
	int argc;
	u32 data1, data2;

	strncpy(s, buf, sizeof(s));
	for (argc = 0; argc < 3; argc++) {
		argv[argc] = strsep(&string, " ");
		if (argv[argc] == NULL)
			break;
	}

	if (argc < 2 || kstrtouint(argv[1], 16, &data1) ||
		(argc > 2 && kstrtouint(argv[2], 16, &data2)))
		return count;

	switch (*argv[0]) {
	case 'a':
		dev_dbg(dev, "iomap 0x%x\n", data1);
		if (basepaddr != data1) {
			if (basevaddr != NULL)
				iounmap(basevaddr);
			basepaddr = data1;
			basevaddr = ioremap(basepaddr, SZ_1K);
			gprintk("iomap Done\n");
		}
		break;
	case 'w':
		offaddr = data1 & ~3;
		gprintk("w @0x%x 0x%x\n", basepaddr + data1, data2);
		writel(data2, basevaddr + offaddr);
		break;
	case 'r':
		offaddr = data1 & ~3;
		gprintk("r @0x%x\n", data1);
	}
	return count;
}

/* example(in console): cat mixer "@12 34"*/
static ssize_t mixer_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	u32 data1;
	unsigned int count;
	if (basevaddr != NULL) {
		data1 = readl(basevaddr + offaddr);
		count = sprintf(buf, "@0x%x 0x%x\n", basepaddr + offaddr, data1);
	} else {
		count = sprintf(buf, "use a and r");
	}
	return count;
}

static DEVICE_ATTR(mixer, S_IRUGO | S_IWUSR | S_IWGRP, mixer_show, mixer_store);

static struct attribute *kaudio_attr_list[] = {
	&dev_attr_mixer.attr,
	NULL,
};

static struct attribute_group kaudio_attr_grp = {
	.attrs = kaudio_attr_list,
};

#endif

static int kingyo_ak4678_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &kingyo_card;

	gprintk("\n");
	if (!np) {
		dev_err(&pdev->dev, "Failed to get device node\n");
		return -EINVAL;
	}
	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		return ret;
	}

	gprintk("ak4678-dai: register card %s -> %s\n",
		card->dai_link->codec_dai_name, card->dai_link->cpu_dai_name);

#ifdef SYSFS_MIXER
	ret = sysfs_create_group(&(pdev->dev.kobj), &kaudio_attr_grp);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create sysfs group, errno:%d\n", ret);
	}
#endif

	return ret;
}
static int kingyo_ak4678_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	gprintk("\n");

#ifdef SYSFS_MIXER
	sysfs_remove_group(&(pdev->dev.kobj), &kaudio_attr_grp);
#endif
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id akm_ak4678_of_match[] = {
	{
		.compatible = "samsung,kingyo-ak4678",
	},
	{},
};
MODULE_DEVICE_TABLE(of, akm_ak4678_of_match);

static struct platform_driver kingyo_ak4678_driver = {
	.driver =
		{
			.name = "KINGYO-AK4678",
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(akm_ak4678_of_match),
		},
	.probe = kingyo_ak4678_probe,
	.remove = kingyo_ak4678_remove,
};
module_platform_driver(kingyo_ak4678_driver);

MODULE_DESCRIPTION("AK4678 ALSA SoC Driver for Koi Board");
MODULE_LICENSE("GPL");
