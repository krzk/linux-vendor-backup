/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * Authors: Hyunhee Kim <hyunhee.kim@samsung.com>
 *          Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include "../codecs/ymu831/ymu831.h"

#define EXYNOS3_XUSBXTI_SYS_PWR_REG_OFFSET	0x1280
#define EXYNOS3_XUSBXTI_SYS_PWR_CFG_MASK	(1 << 0)

enum {
	DISABLE,
	ENABLE
};

#define SND_SOC_DAPM_SPKMODE(wname, wevent) \
{       .id = snd_soc_dapm_spk, .name = wname, .kcontrol_news = NULL, \
	.num_kcontrols = 0, .reg = SND_SOC_NOPM, .event = wevent, \
	.event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD}

enum {
	MIC_MAIN,
	MIC_SUB,
	MIC_MAX
};

struct rinato_ymu831 {
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct regmap *reg_pmu;

	struct clk *codec_mclk;
	struct clk *clk_32kHz;
	int gpio_mic_bias[MIC_MAX];
	/* int gpios[GPIO_NUM]; */

	u32 mic_avail[MIC_MAX];
};

/*
 * This function maintain power-on state of XUSBXTI clock to operate
 * sound codec for BT call when sleep state.
 */
static void exynos_sys_powerdown_xusbxti_control(struct rinato_ymu831 *machine,
						 bool enable)
{
	regmap_update_bits(machine->reg_pmu, EXYNOS3_XUSBXTI_SYS_PWR_REG_OFFSET,
			   EXYNOS3_XUSBXTI_SYS_PWR_CFG_MASK,
			   enable ? ENABLE : DISABLE);
}

static int rinato_hifi_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* Set the codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set the cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, MC_ASOC_BCLK_MULT,
				MC_ASOC_LRCK_X32);

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * rinato YMU831 DAI operations.
 */
static struct snd_soc_ops rinato_hifi_ops = {
	.hw_params = rinato_hifi_hw_params,
};

static int machine_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static struct snd_soc_dai_link machine_dai[] = {
	{ /* Primary DAI i/f */
		.name		= "YMU831 AIF1",
		.stream_name	= "Pri_Dai",
		.codec_dai_name = "ymu831-da0",
		.init		= machine_init_paiftx,
		.ops		= &rinato_hifi_ops,
	},
};

static int machine_card_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct rinato_ymu831 *machine = snd_soc_card_get_drvdata(card);
	int suspended;

	suspended = ymu831_get_codec_suspended(codec);
	if (!suspended)
		exynos_sys_powerdown_xusbxti_control(machine, true);

	return 0;
}

static int machine_card_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct rinato_ymu831 *machine = snd_soc_card_get_drvdata(card);
	int suspended;

	suspended = ymu831_get_codec_suspended(codec);
	if (!suspended)
		exynos_sys_powerdown_xusbxti_control(machine, false);

	return 0;
}

static int rinato_ymu831_parse_dt(struct rinato_ymu831 *machine,
				   struct device *dev)
{
	struct device_node *node = dev->of_node;

	of_property_read_u32_array(node, "samsung,mic-availability",
				   machine->mic_avail,
				   ARRAY_SIZE(machine->mic_avail));
	return 0;
}

static int rinato_ymu831_probe(struct platform_device *pdev)
{
	struct snd_soc_dai_link *dai_link = &machine_dai[0];
	struct rinato_ymu831 *machine;
	struct snd_soc_card *card;
	struct clk *parent, *out_mux;
	int ret;

	if (!pdev->dev.of_node)
		return -ENODEV;

	card = devm_kzalloc(&pdev->dev, sizeof(*card), GFP_KERNEL);
	if (!card)
		return -ENOMEM;

	card->dai_link = machine_dai;
	card->num_links = ARRAY_SIZE(machine_dai);
	card->suspend_post = machine_card_suspend_post;
	card->resume_pre = machine_card_resume_pre;
	card->dev = &pdev->dev;

	machine = devm_kzalloc(&pdev->dev, sizeof(*machine), GFP_KERNEL);
	if (!machine)
		return -ENOMEM;

	ret = rinato_ymu831_parse_dt(machine, &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s, parse dt error\n", __func__);
		return ret;
	}

	dai_link->cpu_dai_name = NULL;
	dai_link->cpu_name = NULL;
	dai_link->cpu_of_node = of_parse_phandle(pdev->dev.of_node,
						"samsung,i2s-controller", 0);
	if (!dai_link->cpu_of_node) {
		dev_err(&pdev->dev, "i2s-controller property parse error\n");
		return -EINVAL;
	}
	dai_link->platform_name = NULL;
	dai_link->platform_of_node = dai_link->cpu_of_node;

	dai_link->codec_name = NULL;
	dai_link->codec_of_node = of_parse_phandle(pdev->dev.of_node,
						"samsung,audio-codec", 0);
	if (!dai_link->codec_of_node) {
		dev_err(&pdev->dev, "audio-codec property parse error\n");
		return -EINVAL;
	}

	machine->reg_pmu = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"samsung,pmureg-phandle");
	if (IS_ERR(machine->reg_pmu)) {
		dev_err(&pdev->dev, "failed to map PMU registers\n");
		return PTR_ERR(machine->reg_pmu);
	}

	machine->clk_32kHz = clk_get(&pdev->dev, "clk_32k");
	if (IS_ERR(machine->clk_32kHz)) {
		dev_err(&pdev->dev, "failed to get codec 32k clock\n");
		return PTR_ERR(machine->clk_32kHz);
	}
	ret = clk_prepare_enable(machine->clk_32kHz);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable 32kH codec clock\n");
		return ret;
	}

	machine->codec_mclk = clk_get(&pdev->dev, "out");
	if (IS_ERR(machine->codec_mclk)) {
		dev_err(&pdev->dev, "failed to get out clock\n");
		return PTR_ERR(machine->codec_mclk);
	}

	out_mux = clk_get(&pdev->dev, "out-mux");
	if (IS_ERR(out_mux)) {
		dev_err(&pdev->dev, "failed to get out mux clock\n");
		return PTR_ERR(out_mux);
	}

	parent = clk_get(&pdev->dev, "parent");
	if (IS_ERR(parent)) {
		dev_err(&pdev->dev, "failed to get parent clock\n");
		return PTR_ERR(parent);
	}

	clk_set_parent(out_mux, parent);
	clk_prepare_enable(machine->codec_mclk);

	ret = snd_soc_of_parse_card_name(card, "samsung,model");
	if (ret)
		return -ENODEV;

	dev_info(&pdev->dev, "Card name is %s\n", card->name);

	snd_soc_card_set_drvdata(card, machine);

	return snd_soc_register_card(card);
}

static int rinato_ymu831_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct rinato_ymu831 *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);
	clk_put(machine->codec_mclk);

	clk_disable_unprepare(machine->clk_32kHz);
	clk_put(machine->clk_32kHz);
	return 0;
}

static const struct of_device_id rinato_ymu831_of_match[] = {
	{ .compatible = "rinato,ymu831", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rinato_ymu831_of_match);

static struct platform_driver rinato_ymu831_driver = {
	.driver = {
		.name = "rinato-ymu831",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = rinato_ymu831_of_match,
	},
	.probe = rinato_ymu831_probe,
	.remove = rinato_ymu831_remove,
};

module_platform_driver(rinato_ymu831_driver);

MODULE_DESCRIPTION("RINATO YMU831 machine ASoC driver");
MODULE_LICENSE("GPL");
