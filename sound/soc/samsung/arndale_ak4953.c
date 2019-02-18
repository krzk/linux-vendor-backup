/*
 *  arndale_rt5631.c
 *
 *  Copyright (c) 2012, Insignal Co., Ltd.
 *
 *  Author: Claude <claude@insginal.co.kr>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "i2s.h"
#include "i2s-regs.h"

#if 1
	#define gprintk(fmt, x... ) printk( "%s: " fmt, __FUNCTION__ , ## x)
#else
	#define gprintk(x...) do { } while (0)
#endif


static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}

static int arndale_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int bfs, psr, rfs, ret;
	unsigned long rclk;
	int get_format, get_rate;

	get_format = params_format(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}
	get_rate = params_rate(params);
	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;
#if 0 
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
	    return ret;
#else
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK, 0, MOD_OPCLK_PCLK);
	if (ret < 0)
	    return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, rclk, SND_SOC_CLOCK_OUT);
	if (ret < 0)
	    return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK, rfs, SND_SOC_CLOCK_OUT);
	if (ret < 0)
	    return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
	    return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, 0, rclk, SND_SOC_CLOCK_IN);
	if (ret < 0)
	    return ret;

#endif 
	return 0;
}

static struct snd_soc_ops arndale_ops = {
	.hw_params = arndale_hw_params,
};

static int ak4953a_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	
	gprintk("===============================================================1\n");

	snd_soc_dapm_sync(dapm);
	return 0;
}

static struct snd_soc_dai_link arndale_dai[] = {
	{ /* Primary DAI i/f */
		.name = "AK4953A AIF1",
		.stream_name = "Playback",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "ak4953a-AIF1",
		.platform_name = "samsung-audio",
		.codec_name = "ak4953a.1-0013", // ghcstop fix
		.init = ak4953a_init,
		.ops = &arndale_ops,
	},
	{ /* Sec_Fifo DAI i/f */
		.name = "AK4953A AIF2",
		.stream_name = "Capture",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "ak4953a-AIF1",
		.platform_name = "samsung-audio",
		.codec_name = "ak4953a.1-0013", // ghcstop fix
		.init = ak4953a_init,
		.ops = &arndale_ops,
	},
};

static struct snd_soc_card arndale_card = {
	.name = "ARNDALE-AK4953A",
	.owner = THIS_MODULE,
	.dai_link = arndale_dai,
	.num_links = ARRAY_SIZE(arndale_dai),
};

static struct platform_device *arndale_snd_device;

#if 0 
static int __init arndale_audio_init(void)
{
	int ret;

	arndale_snd_device = platform_device_alloc("soc-audio", -1);
	if (!arndale_snd_device)
		return -ENOMEM;

	platform_set_drvdata(arndale_snd_device, &arndale_card);

	ret = platform_device_add(arndale_snd_device);
	if (ret)
		platform_device_put(arndale_snd_device);

	return ret;
}
module_init(arndale_audio_init);

static void __exit arndale_audio_exit(void)
{
	platform_device_unregister(arndale_snd_device);
}
module_exit(arndale_audio_exit);
#else
static __devinit int arndale_ak4953_probe(struct platform_device *pdev)
{
    int ret;
    struct snd_soc_card *card = &arndale_card;
    
	/* register card */
	card->dev = &pdev->dev;
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		return ret;
	}

	printk("ak4953-dai: register card %s -> %s\n", card->dai_link->codec_dai_name, card->dai_link->cpu_dai_name);
	gprintk("========================> ak4953 regsitered\n");

    return ret;
}

static int __devexit arndale_ak4953_remove(struct platform_device *pdev)
{
    snd_soc_unregister_card(&arndale_card);
    return 0;
}


static struct platform_driver arndale_ak4953_driver = {
    .driver = {
        .name = "SOC-AUDIO-ARNDALE",
        .owner = THIS_MODULE,
    },
    .probe = arndale_ak4953_probe,
    .remove = __devexit_p(arndale_ak4953_remove),
};
module_platform_driver(arndale_ak4953_driver);

#endif


MODULE_AUTHOR("ghcstop <ghcstop@insignal.co.kr>");
MODULE_DESCRIPTION("ALSA SoC Driver for Arndale Board");
MODULE_LICENSE("GPL");
