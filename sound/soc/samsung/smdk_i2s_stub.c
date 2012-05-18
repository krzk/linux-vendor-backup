/*
 * Copyright (C) 2011 Insignal Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "i2s.h"

static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}

static int smdk_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int bfs, psr, rfs, ret;
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
		rfs = (bfs == 48) ? 384 : 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		rfs = (bfs == 48) ? 768 : 512;
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
		printk(KERN_ERR "Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
						SND_SOC_DAIFMT_NB_NF |
						SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK, rfs,
						SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops smdk_ops = {
	.hw_params = smdk_hw_params,
};

static int smdk_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_dapm_sync(dapm);

	return 0;
}

static struct snd_soc_dai_link smdk_dai[] = {
	{ /* Primary DAI i/f */
		.name = "I2S",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "i2s-stub-hifi",
		.platform_name = "samsung-i2s.0",
		.codec_name = "i2s-stub",
		.init = smdk_init_paiftx,
		.ops = &smdk_ops,
	},
};

static struct snd_soc_card smdk = {
	.name = "I2S-STUB",
	.owner = THIS_MODULE,
	.dai_link = smdk_dai,
	.num_links = ARRAY_SIZE(smdk_dai),
};

static int smdk_audio_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &smdk;
	struct snd_soc_dai_link *dai_link = &smdk_dai[0];

	card->dev = &pdev->dev;

	if (np) {
		dai_link->cpu_of_node = of_parse_phandle(np,
				"samsung,i2s-controller", 0);
		if (!dai_link->cpu_of_node) {
			dev_err(&pdev->dev,
				"Property 'samsung,i2s-controller' missing\n");
			return -EINVAL;
		}

		dai_link->cpu_dai_name = NULL;
		dai_link->platform_name = NULL;
		dai_link->platform_of_node = dai_link->cpu_of_node;

		dai_link->codec_of_node = of_parse_phandle(np,
				"samsung,audio-codec", 0);
		if (!dai_link->codec_of_node) {
			dev_err(&pdev->dev,
				"Property 'samsung,audio-codec' missing\n");
			return -EINVAL;
		}
		dai_link->codec_name = NULL;
	}

	ret = snd_soc_register_card(card);

	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);

	return ret;
}

static int smdk_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id samsung_i2s_stub_of_match[] = {
	{ .compatible = "samsung,smdk-i2s-stub", },
	{},
};
MODULE_DEVICE_TABLE(of, samsung_wm8994_of_match);
#endif /* CONFIG_OF */

static struct platform_driver smdk_audio_driver = {
	.driver		= {
		.name	= "smdk-i2s-audio",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_i2s_stub_of_match),
	},
	.probe		= smdk_audio_probe,
	.remove		= smdk_audio_remove,
};

module_platform_driver(smdk_audio_driver);

MODULE_AUTHOR("Tushar Behera");
MODULE_DESCRIPTION("ALSA SoC I2S STUB");
MODULE_LICENSE("GPL");
