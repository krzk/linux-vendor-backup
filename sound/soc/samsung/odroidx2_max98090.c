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

/* Config I2S CDCLK output 19.2MHZ clock to Max98090 */
#define MAX98090_MCLK 19200000

static int odroidx2_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, 0, MAX98090_MCLK,
						SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev,
			"Unable to switch to FLL1: %d\n", ret);
		return ret;
	}

	/* Set the cpu DAI configuration in order to use CDCLK */
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	dev_dbg(codec_dai->dev, "HiFi DAI %s params: channels: %d, rate: %d\n",
		snd_pcm_stream_str(substream), params_channels(params),
		params_rate(params));

	return 0;
}

static struct snd_soc_ops odroidx2_ops = {
	.hw_params	= odroidx2_hw_params,
};

static struct snd_soc_dai_link odroidx2_dai[] = {
	{
		.name		= "MAX98090",
		.stream_name	= "MAX98090 PCM",
		.codec_dai_name	= "HiFi",
		.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM,
		.ops		= &odroidx2_ops,
	}, {
		.name		= "MAX98090 SEC",
		.stream_name	= "MAX98090 PCM SEC",
		.codec_dai_name	= "HiFi",
		.cpu_dai_name	= "samsung-i2s-sec",
		.platform_name	= "samsung-i2s-sec",
		.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM,
		.ops		= &odroidx2_ops,
	},
};

static struct snd_soc_card odroidx2 = {
	.owner		= THIS_MODULE,
	.dai_link	= odroidx2_dai,
	.num_links	= ARRAY_SIZE(odroidx2_dai),
};

static int odroidx2_audio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &odroidx2;
	int ret;

	card->dev = &pdev->dev;

	ret = snd_soc_of_parse_card_name(card, "samsung,model");
	if (ret < 0)
		return ret;

	odroidx2_dai[0].codec_of_node = of_parse_phandle(np,
						"samsung,audio-codec", 0);
	if (!odroidx2_dai[0].codec_of_node) {
		dev_err(&pdev->dev,
			"Property 'samsung,audio-codec' missing or invalid\n");
		return -EINVAL;
	}

	odroidx2_dai[0].cpu_of_node = of_parse_phandle(np,
						"samsung,i2s-controller", 0);
	if (!odroidx2_dai[0].cpu_of_node) {
		dev_err(&pdev->dev,
			"Property 'samsung,i2s-controller' missing or invalid\n");
		ret = -EINVAL;
		goto err_put_cod_n;
	}

	odroidx2_dai[0].platform_of_node = odroidx2_dai[0].cpu_of_node;

	/* Configure the secondary audio interface with the same codec dai */
	odroidx2_dai[1].codec_of_node = odroidx2_dai[0].codec_of_node;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed: %d\n", ret);
		goto err_put_cpu_n;
	}

	return 0;

err_put_cpu_n:
	of_node_put((struct device_node *)odroidx2_dai[0].cpu_of_node);
err_put_cod_n:
	of_node_put((struct device_node *)odroidx2_dai[0].codec_of_node);
	return ret;
}

static int odroidx2_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	of_node_put((struct device_node *)odroidx2_dai[0].cpu_of_node);
	of_node_put((struct device_node *)odroidx2_dai[0].codec_of_node);

	return 0;
}

static const struct of_device_id odroidx2_audio_of_match[] = {
	{ .compatible = "samsung,odroidx2-audio", },
	{ .compatible = "samsung,odroidu3-audio", },
	{ },
};
MODULE_DEVICE_TABLE(of, odroid_audio_of_match);

static struct platform_driver odroidx2_audio_driver = {
	.driver = {
		.name		= "odroidx2-audio",
		.owner		= THIS_MODULE,
		.of_match_table	= odroidx2_audio_of_match,
	},
	.probe	= odroidx2_audio_probe,
	.remove	= odroidx2_audio_remove,
};
module_platform_driver(odroidx2_audio_driver);

MODULE_AUTHOR("zhen1.chen@samsung.com");
MODULE_DESCRIPTION("ALSA SoC Odroidx2 Audio Support");
MODULE_LICENSE("GPL v2");
