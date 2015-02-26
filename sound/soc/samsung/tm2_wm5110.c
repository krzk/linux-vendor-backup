/*
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * Authors: Inha Song <ideal.song@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "i2s.h"
#include "lpass.h"
#include "../codecs/wm5110.h"
#include "../codecs/max98504a.h"

struct tm2_machine_priv {
	struct snd_soc_codec *codec;
	struct clk *codec_mclk1;
	struct clk *codec_mclk2;
	int mic_bias;
};

static struct tm2_machine_priv tm2_machine_priv;

static int tm2_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct tm2_machine_priv *priv =
				snd_soc_card_get_drvdata(rtd->card);
	unsigned int sysclk_rate;
	unsigned int mclk_rate =
			(unsigned int)clk_get_rate(priv->codec_mclk1);
	int ret;

	dev_dbg(codec->dev, "params_rate: %d\n", params_rate(params));

	/*
	 * SYSCLK Frequency is dependent on the Sample Rate. According to
	 * the sample rate, valid SYSCLK frequency is defined in manual.
	 * The manual recommand to select the highest possible SYSCLK
	 * frequency.
	 */
	switch (params_rate(params)) {
	case 4000:
	case 8000:
	case 12000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 96000:
	case 192000:
		/* highest possible SYSCLK frequency: 147.456MHz */
		sysclk_rate = 147456000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176400:
		/* highest possible SYSCLK frequency: 135.4752 MHz */
		sysclk_rate = 135475200;
		break;
	default:
		dev_err(codec->dev, "Not supported sample rate: %d\n",
			params_rate(params));
		return -EINVAL;
	}

	ret = snd_soc_codec_set_pll(codec, WM5110_FLL1,
				    ARIZONA_FLL_SRC_MCLK1,
				    mclk_rate,
				    sysclk_rate);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to start FLL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec, WM5110_FLL1_REFCLK,
				    ARIZONA_FLL_SRC_MCLK1,
				    mclk_rate,
				    sysclk_rate);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set FLL1 Source: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set SYSCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       sysclk_rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set SYSCLK Source: %d\n", ret);
		return ret;
	}

	return 0;
}

static int tm2_aif1_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tm2_machine_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	int ret;

	ret = clk_prepare_enable(priv->codec_mclk1);
	if (ret) {
		dev_err(rtd->card->dev, "Failed to enable mclk: %d\n", ret);
		return ret;
	}

	return 0;
}

static void tm2_aif1_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tm2_machine_priv *priv = snd_soc_card_get_drvdata(rtd->card);

	clk_disable_unprepare(priv->codec_mclk1);
}

static struct snd_soc_ops tm2_aif1_ops = {
	.startup = tm2_aif1_startup,
	.shutdown = tm2_aif1_shutdown,
	.hw_params = tm2_aif1_hw_params,
};

static int tm2_spk_power(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		max98504_set_speaker_status(1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		max98504_set_speaker_status(0);
		break;
	}

	return 0;
}

static int tm2_mic_bias(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct tm2_machine_priv *priv = snd_soc_card_get_drvdata(card);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		gpio_set_value(priv->mic_bias,  1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		gpio_set_value(priv->mic_bias,  0);
		break;
	}

	return 0;
}

static int tm2_late_probe(struct snd_soc_card *card)
{
	struct tm2_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	ret = devm_gpio_request_one(card->dev, priv->mic_bias,
				    GPIOF_OUT_INIT_LOW, "MICBIAS_EN_AP");
	if (ret) {
		dev_err(card->dev,
			"Failed to request mic_bias_gpio: %d\n", ret);
		return ret;
	}

	/* 32khz must be enabled for jack detect */
	if (!IS_ERR(priv->codec_mclk2))
		clk_prepare_enable(priv->codec_mclk2);
	gpio_direction_output(priv->mic_bias, 0);

	return 0;
}

static const struct snd_kcontrol_new card_controls[] = {
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
};

const struct snd_soc_dapm_widget machine_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("SPK", tm2_spk_power),
	SND_SOC_DAPM_MIC("Main Mic", tm2_mic_bias),
};

static struct snd_soc_dai_link machine_dai[] = {
	{
		.name		= "WM5110 AIF1",
		.stream_name	= "Pri_Dai",
		.codec_dai_name = "wm5110-aif1",
		.codec_name	= "wm5110-codec",
		.ops		= &tm2_aif1_ops,
		.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM,
	},
};

static struct snd_soc_card tm2_card = {
	.owner			= THIS_MODULE,

	.dai_link		= machine_dai,
	.num_links		= ARRAY_SIZE(machine_dai),
	.controls		= card_controls,
	.num_controls		= ARRAY_SIZE(card_controls),
	.dapm_widgets		= machine_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(machine_dapm_widgets),

	.late_probe		= tm2_late_probe,

	.drvdata		= &tm2_machine_priv,
};

static int tm2_wm5110_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &tm2_card;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct tm2_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret, i;

	if (!np) {
		dev_err(&pdev->dev, "of node is missing.\n");
		return -ENODEV;
	}

	card->dev = &pdev->dev;

	ret = snd_soc_of_parse_card_name(card, "samsung,model");
	if (ret) {
		dev_err(&pdev->dev,
			"Card name is not provided\n");
		return ret;
	}

	ret = snd_soc_of_parse_audio_routing(card, "samsung,audio-routing");
	if (ret) {
		dev_err(&pdev->dev, "Audio routing is not provided\n");
		return ret;
	}

	for (i = 0; i < card->num_links; i++) {
		dai_link[i].cpu_dai_name = NULL;
		dai_link[i].cpu_name = NULL;
		dai_link[i].cpu_of_node = of_parse_phandle(np,
							"samsung,i2s-controller", 0);
		if (!dai_link[i].cpu_of_node) {
			dev_err(&pdev->dev, "i2s-controller property parse error\n");
			return -EINVAL;
		}

		dai_link[i].platform_name = NULL;
		dai_link[i].platform_of_node = dai_link[i].cpu_of_node;
	}

	priv->codec_mclk1 = devm_clk_get(&pdev->dev, "mclk1");
	if (IS_ERR(priv->codec_mclk1)) {
		dev_err(&pdev->dev, "Failed to get out clock\n");
		return PTR_ERR(priv->codec_mclk1);
	}

	/* mclk2 is optional */
	priv->codec_mclk2 = devm_clk_get(&pdev->dev, "mclk2");
	if (IS_ERR(priv->codec_mclk2))
		dev_err(&pdev->dev, "Failed to get mclk2 clock\n");

	priv->mic_bias = of_get_named_gpio(np, "mic_bias_gpio", 0);
	if (!gpio_is_valid(priv->mic_bias)) {
		dev_err(&pdev->dev, "Failed to get mic_bias_gpio\n");
		return -EINVAL;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id tm2_wm5110_of_match[] = {
	{ .compatible = "samsung,tm2-audio", },
	{ },
};
MODULE_DEVICE_TABLE(of, tm2_wm5110_of_match);

static struct platform_driver tm2_wm5110_driver = {
	.driver = {
		.name = "tm2-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tm2_wm5110_of_match,
	},
	.probe = tm2_wm5110_probe,
};

module_platform_driver(tm2_wm5110_driver);

MODULE_AUTHOR("Inha Song <ideal.song@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC TM2 Audio Support");
MODULE_LICENSE("GPL v2");
