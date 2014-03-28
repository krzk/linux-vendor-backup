/*
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/of.h>
#include <linux/module.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include "i2s.h"

static int odroidx2_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int bfs, rfs, ret;
	unsigned long rclk;

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

	ret = snd_soc_dai_set_sysclk(codec_dai, 0, rclk, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops odroidx2_ops = {
	.hw_params = odroidx2_hw_params,
};

static struct snd_soc_dai_link odroidx2_dai[] = {
	{
		.name = "MAX98090",
		.stream_name = "MAX98090 PCM",
		.codec_dai_name = "HiFi",
		.ops = &odroidx2_ops,
	},
};

static struct snd_soc_card odroidx2 = {
	.name = "odroidx2",
	.owner = THIS_MODULE,
	.dai_link = odroidx2_dai,
	.num_links = ARRAY_SIZE(odroidx2_dai),
};

static int odroidx2_audio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &odroidx2;

	if (!np)
		return -ENODEV;

	card->dev = &pdev->dev;

	odroidx2_dai[0].codec_name = NULL;
	odroidx2_dai[0].codec_of_node = of_parse_phandle(np,
						"samsung,audio-codec", 0);
	if (!odroidx2_dai[0].codec_of_node) {
		dev_err(&pdev->dev,
			"Property 'samsung,audio-codec' missing or invalid\n");
		return -EINVAL;
	}

	odroidx2_dai[0].cpu_name = NULL;
	odroidx2_dai[0].cpu_of_node = of_parse_phandle(np,
						"samsung,i2s-controller", 0);
	if (!odroidx2_dai[0].cpu_of_node) {
		dev_err(&pdev->dev,
			"Property 'samsung,i2s-controller' missing or invalid\n");
		return -EINVAL;
	}

	odroidx2_dai[0].platform_of_node = odroidx2_dai[0].cpu_of_node;

	return snd_soc_register_card(card);
}

static int odroidx2_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id odroidx2_audio_of_match[] = {
	{ .compatible = "samsung,odroidx2-audio", },
	{ },
};
MODULE_DEVICE_TABLE(of, odroid_audio_of_match);

static struct platform_driver odroidx2_audio_driver = {
	.driver = {
		.name = "odroidx2-audio",
		.owner = THIS_MODULE,
		.of_match_table = odroidx2_audio_of_match,
	},
	.probe = odroidx2_audio_probe,
	.remove = odroidx2_audio_remove,
};

module_platform_driver(odroidx2_audio_driver);

MODULE_AUTHOR("zhen1.chen@samsung.com");
MODULE_DESCRIPTION("ALSA SoC Odroidx2 Audio Support");
MODULE_LICENSE("GPL v2");
