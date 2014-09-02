/*
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "../codecs/ymu831/ymu831.h"

#define EXYNOS3_XUSBXTI_SYS_PWR_REG_OFFSET     0x1280
#define EXYNOS3_XUSBXTI_SYS_PWR_CFG_MASK       (1 << 0)

enum {
	DISABLE,
	ENABLE
};

enum {
	MIC_MAIN,
	MIC_SUB,
	MIC_MAX
};

struct rinato_ymu831 {
	struct snd_soc_codec *codec;
	struct regmap *reg_pmu;

	struct clk *codec_mclk;
	struct clk *clk_32kHz;
};

static struct rinato_ymu831 rinato_ymu831_priv;

/*
 * This function maintain power-on state of XUSBXTI clock to operate
 * sound codec for BT call when sleep state.
 */
static void exynos_sys_powerdown_xusbxti_control(struct rinato_ymu831 *priv,
						 bool enable)
{
	regmap_update_bits(priv->reg_pmu, EXYNOS3_XUSBXTI_SYS_PWR_REG_OFFSET,
			   EXYNOS3_XUSBXTI_SYS_PWR_CFG_MASK,
			   enable ? ENABLE : DISABLE);
}

static int rinato_hifi_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, MC_ASOC_BCLK_MULT,
				     MC_ASOC_LRCK_X32);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set CLKDIV: %d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * rinato YMU831 DAI operations.
 */
static struct snd_soc_ops rinato_hifi_ops = {
	.hw_params	= rinato_hifi_hw_params,
};

static int rinato_card_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct rinato_ymu831 *priv = snd_soc_card_get_drvdata(card);
	int suspended;

	suspended = ymu831_get_codec_suspended(codec);
	if (!suspended)
		exynos_sys_powerdown_xusbxti_control(priv, true);

	return 0;
}

static int rinato_card_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct rinato_ymu831 *priv = snd_soc_card_get_drvdata(card);
	int suspended;

	suspended = ymu831_get_codec_suspended(codec);
	if (!suspended)
		exynos_sys_powerdown_xusbxti_control(priv, false);

	return 0;
}

static struct snd_soc_dai_link ymu831_dai[] = {
	{ /* Primary DAI i/f */
		.name		= "YMU831 AIF1",
		.stream_name	= "Pri_Dai",
		.codec_dai_name = "ymu831-da0",
		.ops		= &rinato_hifi_ops,
		.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM,
	},
};

static struct snd_soc_card ymu831_card = {
	.owner			= THIS_MODULE,

	.dai_link		= ymu831_dai,
	.num_links		= ARRAY_SIZE(ymu831_dai),

	.suspend_post		= rinato_card_suspend_post,
	.resume_pre		= rinato_card_resume_pre,
	.drvdata		= &rinato_ymu831_priv,
};

static int rinato_ymu831_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &ymu831_card;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct rinato_ymu831 *priv = snd_soc_card_get_drvdata(card);
	int ret;

	card->dev = &pdev->dev;

	ret = snd_soc_of_parse_card_name(card, "samsung,model");
	if (ret) {
		dev_err(&pdev->dev, "Card name is not provided\n");
		return -ENODEV;
	}

	dai_link->cpu_of_node = of_parse_phandle(np,
						"samsung,i2s-controller", 0);
	if (!dai_link->cpu_of_node) {
		dev_err(&pdev->dev, "i2s-controller property parse error\n");
		return -EINVAL;
	}

	dai_link->platform_of_node = dai_link->cpu_of_node;

	dai_link->codec_of_node = of_parse_phandle(np,
						   "samsung,audio-codec", 0);
	if (!dai_link->codec_of_node) {
		dev_err(&pdev->dev, "audio-codec property parse error\n");
		return -EINVAL;
	}

	priv->reg_pmu = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"samsung,pmureg-phandle");
	if (IS_ERR(priv->reg_pmu)) {
		dev_err(&pdev->dev, "failed to map PMU registers\n");
		return PTR_ERR(priv->reg_pmu);
	}

	priv->clk_32kHz = devm_clk_get(&pdev->dev, "clk_32k");
	if (IS_ERR(priv->clk_32kHz)) {
		dev_err(&pdev->dev, "failed to get codec 32k clock\n");
		return PTR_ERR(priv->clk_32kHz);
	}

	ret = clk_prepare_enable(priv->clk_32kHz);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable 32kH codec clock\n");
		return ret;
	}

	priv->codec_mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(priv->codec_mclk)) {
		dev_err(&pdev->dev, "failed to get out clock\n");
		ret = PTR_ERR(priv->codec_mclk);
		goto clk_32khz_disable;
	}

	/* mclk must be enabled for ymu831 codec control */
	ret = clk_prepare_enable(priv->codec_mclk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable 32kH codec clock\n");
		goto clk_32khz_disable;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card: %d\n", ret);
		goto clk_mclk_disable;
	}

	return 0;

clk_mclk_disable:
	clk_disable_unprepare(priv->codec_mclk);
clk_32khz_disable:
	clk_disable_unprepare(priv->clk_32kHz);
	return ret;
}

static int rinato_ymu831_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct rinato_ymu831 *priv = snd_soc_card_get_drvdata(card);

	clk_disable_unprepare(priv->clk_32kHz);
	clk_disable_unprepare(priv->codec_mclk);
	return 0;
}

static const struct of_device_id rinato_ymu831_of_match[] = {
	{ .compatible = "rinato,ymu831", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rinato_ymu831_of_match);

static struct platform_driver rinato_ymu831_driver = {
	.driver = {
		.name		= "rinato-ymu831",
		.pm		= &snd_soc_pm_ops,
		.of_match_table = rinato_ymu831_of_match,
	},
	.probe = rinato_ymu831_probe,
	.remove = rinato_ymu831_remove,
};

module_platform_driver(rinato_ymu831_driver);

MODULE_AUTHOR("Inha Song <ideal.song@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC Rinato Audio Support");
MODULE_LICENSE("GPL v2");
