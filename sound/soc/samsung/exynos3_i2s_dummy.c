/*
 *  exynos3_i2s_dummy.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/firmware.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/control.h>

#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <mach/exynos-sound.h>
#include <plat/clock.h>

#include "i2s.h"
#include "i2s-regs.h"


//static struct clk *xtal_24mhz_ap;
static struct snd_board_info *board_info;
static unsigned int stable_msec;

static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		pr_err("%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
	pr_debug("%s: EPLL rate = %ld\n",
		__func__, clk_get_rate(fout_epll));
out:
	clk_put(fout_epll);

	return 0;
}

static int set_aud_sclk(unsigned long rate)
{
	struct clk *mout_epll;
	struct clk *sclk_epll_muxed;
	struct clk *sclk_audio;
	struct clk *sclk_i2s;
	int ret = 0;

	mout_epll = clk_get(NULL, "mout_epll");
	if (IS_ERR(mout_epll)) {
		pr_err("%s: failed to get mout_epll\n", __func__);
		ret = -EINVAL;
		goto out0;
	}

	sclk_epll_muxed = clk_get(NULL, "sclk_epll_muxed");
	if (IS_ERR(sclk_epll_muxed)) {
		pr_err("%s: failed to get sclk_epll_muxed\n", __func__);
		ret = -EINVAL;
		goto out1;
	}
	clk_set_parent(sclk_epll_muxed, mout_epll);

	sclk_audio = clk_get(NULL, "sclk_audio");
	if (IS_ERR(sclk_audio)) {
		pr_err("%s: failed to get sclk_audio\n", __func__);
		ret = -EINVAL;
		goto out2;
	}
	clk_set_parent(sclk_audio, sclk_epll_muxed);

	sclk_i2s = clk_get(NULL, "sclk_i2s");
	if (IS_ERR(sclk_i2s)) {
		pr_err("%s: failed to get sclk_i2s\n", __func__);
		ret = -EINVAL;
		goto out3;
	}

	clk_set_rate(sclk_i2s, rate);
	pr_debug("%s: SCLK_I2S rate = %ld\n",
		__func__, clk_get_rate(sclk_i2s));

	clk_put(sclk_i2s);
out3:
	clk_put(sclk_audio);
out2:
	clk_put(sclk_epll_muxed);
out1:
	clk_put(mout_epll);
out0:
	return ret;
}

/*
 * ESPRESSO I2S DAI operations. (AP Master)
 */
static int exynos3_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, sclk, bfs, div, rfs, ret;
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
	case 32000:
	/* TODO
	   case 44100: */
	case 48000:
	case 96000:
	case 192000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 6144000:
	case 8192000:
		div = 12;
		break;
	/* TODO
	   case 11289600: */
	case 16934400:
	case 12288000:
	case 18432000:
		div = 4;
		break;
	case 24576000:
	case 36864000:
		div = 2;
		break;
	case 49152000:
	case 73728000:
		div = 1;
		break;
	default:
		pr_err("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk;
	pll = sclk * div;
	set_aud_pll_rate(pll);

	/* Set SCLK */
	ret = set_aud_sclk(sclk);
	if (ret < 0)
		return ret;

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}

static int dummy_delay_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = stable_msec;
	return 0;
}
static int dummy_delay_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int delay = ucontrol->value.integer.value[0];

	if (delay > 1000)
		delay = 1000;
	else if (delay < 0)
		delay = 0;

	stable_msec = delay;
	pr_info("Sound delay for %d msec\n", stable_msec);

	return 1;
}

static const struct snd_kcontrol_new exynos3_snd_controls[] = {
	SOC_SINGLE_EXT("Stable Delay", SND_SOC_NOPM, 0, 1000, 0,
				dummy_delay_get, dummy_delay_put),
};
static int snd_micbias_ctrl(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	if (board_info->mic_en_gpio > 0) {
		switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			dev_info(codec->dev, "micbias enable\n");
			gpio_set_value(board_info->mic_en_gpio, 1);
			break;
		case SND_SOC_DAPM_POST_PMD:
			dev_info(codec->dev, "micbias disable\n");
			gpio_set_value(board_info->mic_en_gpio, 0);
			break;
		}
	} else {
		dev_info(codec->dev, "skip micbias control\n");
	}

	if (event == SND_SOC_DAPM_PRE_PMU) {
		if (stable_msec > 0) {
			dev_info(codec->dev, "micbias stable %u msec\n",  stable_msec);
			msleep_interruptible(stable_msec);
		}
	}
	return 0;
}

const struct snd_soc_dapm_widget machine_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Main Mic", snd_micbias_ctrl),
};

const struct snd_soc_dapm_route machine_dapm_routes[] = {
	{ "Capture", NULL, "Main Mic"},
};

static int i2s_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;

	err = snd_soc_add_codec_controls(codec, exynos3_snd_controls,
					ARRAY_SIZE(exynos3_snd_controls));
	if (err < 0)
		return err;
	err = snd_soc_dapm_new_controls(&codec->dapm, machine_dapm_widgets,
					ARRAY_SIZE(machine_dapm_widgets));
	if (err < 0)
		dev_err(codec->dev, "Failed to add DAPM widgets: %d\n", err);

	err = snd_soc_dapm_add_routes(&codec->dapm, machine_dapm_routes,
					ARRAY_SIZE(machine_dapm_routes));
	if (err < 0)
		dev_err(codec->dev, "Failed to add DAPM routes: %d\n", err);

	return 0;
}

static struct snd_soc_ops exynos3_ops = {
	.hw_params = exynos3_hw_params,
};

static struct snd_soc_dai_link exynos3_dai[] = {
	{
		.name = "I2S PRI",
		.stream_name = "i2s",
		.cpu_dai_name = "samsung-i2s.2",
		.codec_dai_name = "dummy-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "dummy-codec",
		.ops = &exynos3_ops,
		.init = i2s_dai_init,
	}
};

static int exynos3_suspend_post(struct snd_soc_card *card)
{
	return 0;
}

static int exynos3_resume_pre(struct snd_soc_card *card)
{
	return 0;
}

static struct snd_soc_card exynos3_snd_card = {
	.name = "i2s",
	.suspend_post = exynos3_suspend_post,
	.resume_pre = exynos3_resume_pre,
	.dai_link = exynos3_dai,
	.num_links = ARRAY_SIZE(exynos3_dai),
};

static int __devinit exynos3_audio_probe(struct platform_device *pdev)
{
	struct snd_board_info *pdata = dev_get_platdata(&pdev->dev);
	struct snd_soc_card *card = &exynos3_snd_card;
	int ret;

	card->dev = &pdev->dev;

	if (!pdata) {
		dev_err(&pdev->dev, "Can not get board info\n");
		return -ENODEV;
	}

	board_info = pdata;
	if (board_info->mic_en_gpio > 0) {
		ret = gpio_request(board_info->mic_en_gpio, "MIC_EN");
		if (ret) {
			pr_info("%s: MIC_EN GPIO set error\n", __func__);
		} else {
			/* Initial Low */
			gpio_direction_output(board_info->mic_en_gpio, 0);
		}
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "%s: snd_soc_register_card() failed %d",
					__func__, ret);
	}

	return ret;
}

static int __devexit exynos3_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	if (board_info->mic_en_gpio > 0)
		gpio_free(board_info->mic_en_gpio);

	snd_soc_unregister_card(card);

	return 0;
}
static struct platform_driver exynos3_snd_machine_driver = {
	.driver = {
		.name = "i2s-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = exynos3_audio_probe,
	.remove = __devexit_p(exynos3_audio_remove),
};

module_platform_driver(exynos3_snd_machine_driver);

MODULE_DESCRIPTION("ALSA SoC Exynos3");
MODULE_LICENSE("GPL");
