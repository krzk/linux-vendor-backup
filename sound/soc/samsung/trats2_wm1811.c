/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * Authors: KwangHui Cho <kwanghui.cho@samsung.com>
 *          Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>
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
#include <sound/jack.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "s3c-i2s-v2.h"
#include "../codecs/wm8994.h"

#define XTAL_24MHZ_AP			24000000
#define CODEC_CLK32K			32768
#define CODEC_DEFAULT_SYNC_CLK		11289600

#define WM1811_JACKDET_MODE_NONE	0x0000
#define WM1811_JACKDET_MODE_JACK	0x0100
#define WM1811_JACKDET_MODE_MIC		0x0080
#define WM1811_JACKDET_MODE_AUDIO	0x0180

#define WM1811_JACKDET_BTN0		0x04
#define WM1811_JACKDET_BTN1		0x10
#define WM1811_JACKDET_BTN2		0x08

#define SND_SOC_DAPM_SPKMODE(wname, wevent) \
{       .id = snd_soc_dapm_spk, .name = wname, .kcontrol_news = NULL, \
	.num_kcontrols = 0, .reg = SND_SOC_NOPM, .event = wevent, \
	.event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD}


static char *mic_names[] = {
	"Main Mic",
	"Sub Mic",
};

static const struct wm8958_micd_rate machine_det_rates[] = {
	{ CODEC_CLK32K,			true,  0, 0 },
	{ CODEC_CLK32K,			false, 0, 0 },
	{ CODEC_DEFAULT_SYNC_CLK,	true,  0xa, 0xb },
	{ CODEC_DEFAULT_SYNC_CLK,	false, 0xa, 0xb },
};

static const struct wm8958_micd_rate machine_jackdet_rates[] = {
	{ CODEC_CLK32K,			true,  0, 0 },
	{ CODEC_CLK32K,			false, 0, 0 },
	{ CODEC_DEFAULT_SYNC_CLK,	true, 12, 12 },
	{ CODEC_DEFAULT_SYNC_CLK,	false, 7, 7 },
};

static int aif2_mode;
const char *aif2_mode_text[] = {
	"Slave", "Master"
};
enum {
	MODE_CODEC_SLAVE,
	MODE_CODEC_MASTER,
};

/* TODO: convert to DT properties */
enum {
	MIC_MAIN,
	MIC_SUB,
	MIC_MAX
};

enum trats2_wm1811_gpios {
	GPIO_VPS_SOUND,
	GPIO_MIC_BIAS,
	GPIO_MIC_SUB_BIAS,
	GPIO_NUM
};

/* To support PBA function test */
static struct class *audio_class;
static struct device *jack_dev;
static struct device *caps_dev;

struct trats2_wm1811 {
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct clk *codec_mclk;

	int gpio_mic_bias[MIC_MAX];
	int gpio_vps_en;
	/* int gpios[GPIO_NUM]; */

	u32 mic_avail[MIC_MAX];
	bool ignore_earjack;
	bool cp_wb_support;

	/* Exynos4 EPLL clock */
	struct clk *clk_pll;
};

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static int get_aif2_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = aif2_mode;
	return 0;
}

static int set_aif2_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	if (aif2_mode == ucontrol->value.integer.value[0])
		return 0;

	aif2_mode = ucontrol->value.integer.value[0];

	WARN_ON((aif2_mode != MODE_CODEC_SLAVE) &&
		(aif2_mode != MODE_CODEC_MASTER));

	pr_info("set AIF2 mode: %s\n", aif2_mode_text[aif2_mode]);
	return 0;
}

static int exynos_snd_micbias(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct trats2_wm1811 *machine = snd_soc_card_get_drvdata(codec->card);
	unsigned int mic;

	dev_info(codec->dev, "Mic Bias: %s event is %02X", w->name, event);

	if (!strncmp(w->name, mic_names[MIC_MAIN], strlen(mic_names[MIC_MAIN])))
		mic = MIC_MAIN;
	else if (!strncmp(w->name, mic_names[MIC_SUB],
			  strlen(mic_names[MIC_SUB])))
		mic = MIC_SUB;
	else {
		pr_err("Sound: Unknown dapm widget %s\n", w->name);
		return 0;
	}

	if (!machine->mic_avail[mic]) {
		dev_err(codec->dev, "%s is not available on this board",
			mic_names[mic]);
		return 0;
	}

	if (gpio_is_valid(machine->gpio_mic_bias[mic])) {
		switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			pr_info("Sound: %s bias enable", mic_names[mic]);
			gpio_set_value(machine->gpio_mic_bias[mic], 1);
			/* add delay to remove main mic pop up noise */
			msleep(150);
			break;
		case SND_SOC_DAPM_POST_PMD:
			pr_info("Sound: %s bias disable", mic_names[mic]);
			gpio_set_value(machine->gpio_mic_bias[mic], 0);
			break;
		}
	} else {
		/* if EXYNOS gpio is not assigned to external ldo control
		   that means MICBIAS1 (A9 pin) is used for mic bias supply */
		switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			pr_info("Sound: %s bias enable", mic_names[mic]);
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
				WM8994_MICB1_ENA_MASK, WM8994_MICB1_ENA);
			break;
		case SND_SOC_DAPM_POST_PMD:
			pr_info("Sound: %s bias disable", mic_names[mic]);
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
				WM8994_MICB1_ENA_MASK, 0);
			break;
		}
	}

	return 0;
}

static int exynos_snd_spkmode(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_info(codec->dev, "set speaker L+R mix");
		ret = snd_soc_update_bits(codec, WM8994_SPKOUT_MIXERS,
				  WM8994_SPKMIXR_TO_SPKOUTL_MASK,
				  WM8994_SPKMIXR_TO_SPKOUTL);
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_info(codec->dev, "unset speaker L+R mix");
		ret = snd_soc_update_bits(codec, WM8994_SPKOUT_MIXERS,
				  WM8994_SPKMIXR_TO_SPKOUTL_MASK,
				  0);
		break;
	}

	return ret;
}

static int exynos_snd_vps_switch(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct trats2_wm1811 *machine = snd_soc_card_get_drvdata(codec->card);

	if (!gpio_is_valid(machine->gpio_vps_en))
		return 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		pr_info("vps switch enable");
		gpio_set_value(machine->gpio_vps_en, 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		pr_info("vps switch disable");
		gpio_set_value(machine->gpio_vps_en, 0);
		break;
	}

	return 0;
}

static void codec_micd_set_rate(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	int best, i, sysclk, val;
	bool idle;
	const struct wm8958_micd_rate *rates = NULL;
	int num_rates = 0;

	idle = !wm8994->jack_mic;

	sysclk = snd_soc_read(codec, WM8994_CLOCKING_1);
	if (sysclk & WM8994_SYSCLK_SRC)
		sysclk = wm8994->aifclk[1];
	else
		sysclk = wm8994->aifclk[0];

	if (wm8994->jackdet) {
		rates = machine_jackdet_rates;
		num_rates = ARRAY_SIZE(machine_jackdet_rates);
	} else {
		rates = machine_det_rates;
		num_rates = ARRAY_SIZE(machine_det_rates);
	}

	best = 0;
	for (i = 0; i < num_rates; i++) {
		if (rates[i].idle != idle)
			continue;
		if (abs(rates[i].sysclk - sysclk) <
		    abs(rates[best].sysclk - sysclk))
			best = i;
		else if (rates[best].idle != idle)
			best = i;
	}

	val = rates[best].start << WM8958_MICD_BIAS_STARTTIME_SHIFT
		| rates[best].rate << WM8958_MICD_RATE_SHIFT;

	snd_soc_update_bits(codec, WM8958_MIC_DETECT_1,
			    WM8958_MICD_BIAS_STARTTIME_MASK |
			    WM8958_MICD_RATE_MASK, val);
}

static void codec_micdet(u16 status, void *data)
{
	struct trats2_wm1811 *machine = data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(machine->codec);
	int report;
	static int check_report;

	dev_info(machine->codec->dev, "jack status 0x%x", status);
	/*
	 * LCD is still off even though earjack event has occurred.
	 * So platform daemon need time to deal this event
	 */
	pm_wakeup_event(machine->codec->dev, 500);
	/* Either nothing present or just starting detection */
	if (!(status & WM8958_MICD_STS)) {
		if (!wm8994->jackdet) {
			/* If nothing present then clear our statuses */
			dev_dbg(machine->codec->dev, "Detected open circuit\n");
			wm8994->jack_mic = false;
			wm8994->mic_detecting = true;

			codec_micd_set_rate(machine->codec);

			snd_soc_jack_report(wm8994->micdet[0].jack, 0,
					    wm8994->btn_mask |
					     SND_JACK_HEADSET);
		}
		return;
	}
	/*
	 * If the measurement is showing a high impedence we've got
	 * a microphone.
	 */
	if (wm8994->mic_detecting && (status & 0x400)) {
		dev_info(machine->codec->dev, "Detected microphone\n");

		wm8994->mic_detecting = false;
		wm8994->jack_mic = true;

		codec_micd_set_rate(machine->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack,
				SND_JACK_HEADSET, SND_JACK_HEADSET);
	}

	if (wm8994->mic_detecting && status & 0x4) {
		dev_info(machine->codec->dev, "Detected headphone\n");
		wm8994->mic_detecting = false;

		codec_micd_set_rate(machine->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack,
				SND_JACK_HEADPHONE, SND_JACK_HEADSET);

		/* If we have jackdet that will detect removal */
		if (wm8994->jackdet) {
			mutex_lock(&wm8994->accdet_lock);

			snd_soc_update_bits(machine->codec,
					WM8958_MIC_DETECT_1,
					WM8958_MICD_ENA, 0);

			if (wm8994->active_refcount) {
				snd_soc_update_bits(machine->codec,
					WM8994_ANTIPOP_2,
					WM1811_JACKDET_MODE_MASK,
					WM1811_JACKDET_MODE_AUDIO);
			}

			mutex_unlock(&wm8994->accdet_lock);
#if 0
			if (wm8994->pdata->jd_ext_cap) {
				mutex_lock(&wm1811->codec->mutex);
				snd_soc_dapm_disable_pin(&wm1811->codec->dapm,
							"MICBIAS2");
				snd_soc_dapm_sync(&wm1811->codec->dapm);
				mutex_unlock(&wm1811->codec->mutex);
			}
#endif

		}
	}

	/* Report short circuit as a button */
	if (wm8994->jack_mic) {
		report = 0;
		if (status & WM1811_JACKDET_BTN0)
			report |= SND_JACK_BTN_0;

		if (status & WM1811_JACKDET_BTN1)
			report |= SND_JACK_BTN_1;

		if (status & WM1811_JACKDET_BTN2)
			report |= SND_JACK_BTN_2;

		/* TODO : Check this on wm1811a */
		if (check_report != report) {
			dev_info(machine->codec->dev,
				"Report Button: %08x (%08X)", report, status);
			snd_soc_jack_report(wm8994->micdet[0].jack, report,
					    wm8994->btn_mask);
			check_report = report;
		} else
			dev_info(machine->codec->dev, "Skip button report");
	}
}

#ifndef CONFIG_SND_SAMSUNG_I2S_MASTER
static int machine_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	int ret;

	pr_info("params_rate: %d\n", params_rate(params));

	if (params_rate(params) == 8000 || params_rate(params) == 11025)
		pll_out = params_rate(params) * 512;
	else
		pll_out = params_rate(params) * 256;

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

	/* Switch FLL */
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1, WM8994_FLL_SRC_MCLK1,
				  XTAL_24MHZ_AP, pll_out);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to start FLL1: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
				     pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev,
			"Unable to switch to FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(cpu_dai->dev,
			"Unable to set i2s opclk: 0x%x\n", ret);
		return ret;
	}

	codec_micd_set_rate(codec_dai->codec);

	dev_info(codec_dai->dev,
		"AIF1 DAI %s params ch %d, rate %d as i2s slave\n",
		((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
						"playback" : "capture"),
						params_channels(params),
						params_rate(params));
	return 0;
}
#else /* CONFIG_SND_SAMSUNG_I2S_MASTER */

static int machine_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int bfs, psr, rfs, ret;
	unsigned long rclk, epll_freq;

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
		dev_info(codec_dai->dev, "Not yet supported!\n");
		return -EINVAL;
	}

	epll_freq = rclk * psr;

	if (epll_freq != clk_get_rate(machine->clk_pll)) {
		ret = clk_set_rate(machine->clk_pll, epll_freq);
		if (ret < 0)
			return ret;
		pr_info("%s: fout_epll: %ld (%ld) Hz\n", __func__,
			clk_get_rate(machine->clk_pll), epll_freq);
	}

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

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_MCLK1,
					rclk, SND_SOC_CLOCK_IN);
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
#endif /* CONFIG_SND_SAMSUNG_I2S_MASTER */

/*
 * WM1811 DAI operations.
 */
static struct snd_soc_ops machine_aif1_ops = {
	.hw_params = machine_aif1_hw_params,
};

static int machine_aif2_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;
	int prate;
	int bclk;
	unsigned int fmt;
	int pll_src;
	unsigned int pll_freq_in;

	prate = params_rate(params);
	switch (params_rate(params)) {
	case 8000:
	case 16000:
	       break;
	default:
		dev_warn(codec_dai->dev, "Unsupported LRCLK %d, falling back to 8kHz\n",
			(int)params_rate(params));
		prate = 8000;
	}

	fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF;
	if (aif2_mode == MODE_CODEC_SLAVE)
		fmt |= SND_SOC_DAIFMT_CBS_CFS;
	else /* MODE_CODEC_MASTER */
		fmt |= SND_SOC_DAIFMT_CBM_CFM;

	/* Set the codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);

	if (ret < 0)
		return ret;

	switch (prate) {
	case 8000:
		bclk = 256000;
		break;
	case 16000:
		bclk = 512000;
		break;
	default:
		return -EINVAL;
	}

	if (aif2_mode == MODE_CODEC_SLAVE) {
		pll_src = WM8994_FLL_SRC_BCLK;
		pll_freq_in = bclk;
	} else {
		pll_src = WM8994_FLL_SRC_MCLK1;
		pll_freq_in = XTAL_24MHZ_AP;
	}
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2,
				pll_src, pll_freq_in,
				prate * 256);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to configure FLL2: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL2,
					prate * 256, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to switch to FLL2: %d\n", ret);

	dev_info(codec_dai->dev,
		"AIF2 DAI %s params ch %d, rate %d as Clock %s\n",
		((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
						"playback" : "capture"),
						params_channels(params),
						params_rate(params),
						aif2_mode_text[aif2_mode]);
	return 0;
}

static struct snd_soc_ops machine_aif2_ops = {
	.hw_params = machine_aif2_hw_params,
};

static int machine_aif3_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	dev_info(codec_dai->dev, "AIF3 DAI %s params ch %d, rate %d\n",
		((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
						"playback" : "capture"),
						params_channels(params),
						params_rate(params));
	return 0;
}

static struct snd_soc_ops machine_aif3_ops = {
	.hw_params = machine_aif3_hw_params,
};

static const struct snd_kcontrol_new card_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("LINE"),
	SOC_DAPM_PIN_SWITCH("HDMI"),

	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

static const struct snd_kcontrol_new codec_controls[] = {
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),
};

const struct snd_soc_dapm_widget machine_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPKMODE("SPK", exynos_snd_spkmode),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_LINE("LINE", exynos_snd_vps_switch),
	SND_SOC_DAPM_LINE("HDMI", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", exynos_snd_micbias),
	SND_SOC_DAPM_MIC("Sub Mic", exynos_snd_micbias),

};

const struct snd_soc_dapm_route machine_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "RCV", NULL, "HPOUT2N" },
	{ "RCV", NULL, "HPOUT2P" },

	{ "LINE", NULL, "LINEOUT2N" },
	{ "LINE", NULL, "LINEOUT2P" },

	{ "HDMI", NULL, "LINEOUT1N" }, /* Not connected */
	{ "HDMI", NULL, "LINEOUT1P" }, /* Not connected */
};

const struct snd_soc_dapm_route wm1811_input_dapm_routes[] = {
	{ "IN1LP", NULL, "Main Mic" },
	{ "IN1LN", NULL, "Main Mic" },

	{ "IN1RP", NULL, "Sub Mic" },
	{ "IN1RN", NULL, "Sub Mic" },

	{ "IN2LP:VXRN", NULL, "MICBIAS2" },
	{ "MICBIAS2", NULL, "Headset Mic" },
};

const struct snd_soc_dapm_route redwood_input_dapm_routes[] = {
	{ "IN1LP", NULL, "Main Mic" },
	{ "IN1LN", NULL, "Main Mic" },

	{ "IN1RP", NULL, "MICBIAS2" },
	{ "IN1RN", NULL, "MICBIAS2" },

	{ "IN2LP:VXRN", NULL, "MICBIAS2" },
	{ "MICBIAS2", NULL, "Headset Mic" },
};

const struct snd_soc_dapm_route redwood45_input_dapm_routes[] = {
	{ "IN2LP:VXRN", NULL, "Main Mic" },
	{ "IN2LN", NULL, "Main Mic" },

	/* if add route of MICBIAS1 to "Sub Mic",
	   it could be automatically controlled by dapm sequence
	   but we have alternative boards of n035 and redwood45
	   so MICBIAS1 should be controlled another SND_SOC_DAPM_MIC
	   "Sub Mic" control function */
	{ "IN1RP", NULL, "Sub Mic" },
	{ "IN1RN", NULL, "Sub Mic" },

	{ "IN1RP", NULL, "MICBIAS2" },
	{ "MICBIAS2", NULL, "Headset Mic" },
};

static ssize_t earjack_select_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("Sound: %s operate nothing", __func__);

	return 0;
}

static ssize_t earjack_select_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	int report = 0;

	wm8994->mic_detecting = false;
	wm8994->jack_mic = true;

	codec_micd_set_rate(codec);

	if ((!size) || (buf[0] == '0')) {
		snd_soc_jack_report(wm8994->micdet[0].jack,
				    0, SND_JACK_HEADSET);
		dev_info(codec->dev, "Forced remove earjack\n");
	} else {
		if (buf[0] == '1') {
			dev_info(codec->dev, "Forced detect microphone\n");
			report = SND_JACK_HEADSET;
		} else {
			dev_info(codec->dev, "Forced detect headphone\n");
			report = SND_JACK_HEADPHONE;
		}
		snd_soc_jack_report(wm8994->micdet[0].jack,
				    report, SND_JACK_HEADSET);
	}

	return size;
}

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);

static ssize_t audio_caps_cp_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct trats2_wm1811 *machine = dev_get_drvdata(dev);

	if (machine->cp_wb_support == true)
		return sprintf(buf, "wb\n") + 1;
	else
		return sprintf(buf, "nb\n") + 1;
}
static DEVICE_ATTR(cp_caps, S_IRUGO, audio_caps_cp_show, NULL);

static ssize_t audio_caps_mic_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct trats2_wm1811 *machine = dev_get_drvdata(dev);
	int i, cnt = 0;

	if (!machine)
		return 0;

	for (i = 0; i < MIC_MAX; i++) {
		if (machine->mic_avail[i])
			cnt++;
	}

	return sprintf(buf, "%d\n", cnt);
}
static DEVICE_ATTR(mic_count, S_IRUGO, audio_caps_mic_count_show, NULL);

static int machine_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = rtd->card;
	struct trats2_wm1811 *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *aif1_dai = rtd->codec_dai;
	struct wm8994 *wm8994 = dev_get_drvdata(codec->dev->parent);
	int ret;

	ret = clk_prepare_enable(machine->codec_mclk);
	if (ret < 0)
		return ret;

	ret = snd_soc_add_card_controls(card, card_controls,
					ARRAY_SIZE(card_controls));
	if (ret != 0)
		dev_err(card->dev, "Failed to add card ctrls: %d\n", ret);

	ret = snd_soc_add_codec_controls(codec, codec_controls,
					ARRAY_SIZE(codec_controls));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add codec ctrls: %d\n", ret);

	ret = snd_soc_dapm_new_controls(&codec->dapm, machine_dapm_widgets,
					   ARRAY_SIZE(machine_dapm_widgets));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add DAPM widgets: %d\n", ret);

	ret = snd_soc_dapm_add_routes(&codec->dapm, machine_dapm_routes,
					   ARRAY_SIZE(machine_dapm_routes));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add DAPM routes: %d\n", ret);

	if (!strcmp(card->name, "redwood45")) {
		ret = snd_soc_dapm_add_routes(&codec->dapm,
				redwood45_input_dapm_routes,
				ARRAY_SIZE(redwood45_input_dapm_routes));
	} else if (!strcmp(card->name, "redwood")) {
		ret = snd_soc_dapm_add_routes(&codec->dapm,
					redwood_input_dapm_routes,
					ARRAY_SIZE(redwood_input_dapm_routes));
	} else {
		ret = snd_soc_dapm_add_routes(&codec->dapm,
					wm1811_input_dapm_routes,
					ARRAY_SIZE(wm1811_input_dapm_routes));
	}

	if (ret != 0)
		dev_err(codec->dev,
			"Failed to add DAPM input routes: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(aif1_dai, WM8994_SYSCLK_MCLK2,
				     CODEC_CLK32K, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to boot clocking\n");

	/* Force AIF1CLK on as it will be master for jack detection */
	ret = snd_soc_dapm_force_enable_pin(&codec->dapm, "AIF1CLK");
	if (ret < 0)
		dev_err(codec->dev, "Failed to enable AIF1CLK: %d\n", ret);

	/* check sub mic support */
	if (machine && !machine->mic_avail[MIC_SUB])
		snd_soc_dapm_nc_pin(&codec->dapm, "Sub Mic");
	else
		snd_soc_dapm_ignore_suspend(&codec->dapm, "Sub Mic");

	/* check earjack support */
	if (machine->ignore_earjack)
		snd_soc_dapm_nc_pin(&codec->dapm, "Headset Mic");
	else
		snd_soc_dapm_ignore_suspend(&codec->dapm, "Headset Mic");

	/* disable codec ldo for mic mias when using ext mic bias */
	if (!machine->gpio_mic_bias[MIC_MAIN] &&
	    !machine->gpio_mic_bias[MIC_SUB])
		snd_soc_dapm_nc_pin(&codec->dapm, "MICBIAS1");

	/* disable fm radio analog input */
	snd_soc_dapm_nc_pin(&codec->dapm, "IN2RN");
	snd_soc_dapm_nc_pin(&codec->dapm, "IN2RP:VXRP");

	snd_soc_dapm_ignore_suspend(&codec->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "HP");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1DACDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2DACDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3DACDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1ADCDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2ADCDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3ADCDAT");

	machine->codec = codec;

	codec_micd_set_rate(codec);

	ret = snd_soc_jack_new(codec, "Headset Jack",
				SND_JACK_HEADSET | SND_JACK_BTN_0 |
				SND_JACK_BTN_1 | SND_JACK_BTN_2,
				&machine->jack);
	if (ret < 0)
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);

	ret = snd_jack_set_key(machine->jack.jack, SND_JACK_BTN_0, KEY_MEDIA);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_MEDIA: %d\n", ret);

	ret = snd_jack_set_key(machine->jack.jack, SND_JACK_BTN_1,
							KEY_VOLUMEDOWN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEUP: %d\n", ret);

	ret = snd_jack_set_key(machine->jack.jack, SND_JACK_BTN_2,
							KEY_VOLUMEUP);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEDOWN: %d\n", ret);

	/* TODO : check this on fast boot environment */
	machine->jack.status = 0;

	/* check earjack support */
	if (machine->ignore_earjack) {
		dev_info(codec->dev,
			"Earjack event doesn't support for this board");
		ret = snd_soc_update_bits(codec, WM8958_MIC_DETECT_1,
				WM8958_MICD_ENA, 0);
		if (ret < 0)
			dev_err(codec->dev,
				"Failed to disable mic detection: %d\n", ret);
	} else {
#if 0
		ret = wm8958_mic_detect(codec,
				&machine->jack, codec_micdet, machine);
		if (ret < 0)
			dev_err(codec->dev,
				"Failed start detection: %d\n", ret);
#endif
		/* To wakeup for earjack event in suspend mode */
		enable_irq_wake(wm8994->irq);
		device_init_wakeup(machine->codec->dev, true);
	}

	/* To support PBA function test */
	audio_class = class_create(THIS_MODULE, "audio");

	if (IS_ERR(audio_class))
		pr_err("Failed to create class\n");

	jack_dev = device_create(audio_class, NULL, 0, codec, "earjack");

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_select_jack.attr.name);

	caps_dev = device_create(audio_class, NULL, 0, NULL, "caps");
	dev_set_drvdata(caps_dev, machine);

	if (device_create_file(caps_dev, &dev_attr_cp_caps) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_cp_caps.attr.name);

	if (device_create_file(caps_dev, &dev_attr_mic_count) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_mic_count.attr.name);

	return snd_soc_dapm_sync(&codec->dapm);
}

static struct snd_soc_dai_driver voice_dai[] = {
	{
		.name = "voice-modem",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "voice-bluetooth",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static const struct snd_soc_component_driver voice_component = {
	.name		= "trats2-voice",
};

static struct snd_soc_dai_link machine_dai[] = {
	{
		/* Primary DAI i/f */
		.name = "WM1811 AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-i2s.0",
		.codec_name = "wm8994-codec",
		.init = machine_init_paiftx,
		.ops = &machine_aif1_ops,
	}, {
		.name = "WM1811 Voice",
		.stream_name = "Voice Tx/Rx",
		.cpu_dai_name = "voice-modem",
		.codec_dai_name = "wm8994-aif2",
		.codec_name = "wm8994-codec",
		.ops = &machine_aif2_ops,
		.ignore_suspend = 1,
	}, {
		.name = "WM1811 BT",
		.stream_name = "BT Tx/Rx",
		.cpu_dai_name = "voice-bluetooth",
		.codec_dai_name = "wm8994-aif3",
		.codec_name = "wm8994-codec",
		.ops = &machine_aif3_ops,
		.ignore_suspend = 1,
	}, {
		/* Sec_Fifo DAI i/f */
		.name = "Sec_FIFO TX",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s-sec",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-i2s-sec",
		.codec_name = "wm8994-codec",
		.ops = &machine_aif1_ops,
	},
};

static int machine_card_suspend_post(struct snd_soc_card *card)
{
	struct trats2_wm1811 *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *aif2_dai = card->rtd[1].codec_dai;
	int ret;


	if (!codec->active) {
		/* change sysclk from pll out of mclk1 to mclk2
		   to disable mclk1 from AP */
		dev_info(codec->dev, "use mclk2 and disable fll");

		ret = snd_soc_dai_set_sysclk(aif2_dai,
				WM8994_SYSCLK_MCLK2,
				CODEC_CLK32K,
				SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(codec->dev,
			"Unable to switch aif2 sysclk to MCLK2: %d\n", ret);

		ret = snd_soc_dai_set_pll(aif2_dai, WM8994_FLL2, 0, 0, 0);
		if (ret < 0)
			dev_err(codec->dev,
				"Unable to stop FLL2: %d\n", ret);

		ret = snd_soc_dai_set_sysclk(aif1_dai,
				WM8994_SYSCLK_MCLK2,
				CODEC_CLK32K,
				SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(codec->dev,
			"Unable to switch aif1 sysclk to MCLK2: %d\n", ret);

		ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1, 0, 0, 0);
		if (ret < 0)
			dev_err(codec->dev, "Unsable to stop FLL1: 0x%x", ret);

		/* Jack det rate and mic det rate should be changed
		to match the above changed sysclk */
		codec_micd_set_rate(codec);

		clk_disable_unprepare(machine->codec_mclk);
	}

	snd_soc_dapm_sync(&codec->dapm);
	return 0;
}

static int machine_card_resume_pre(struct snd_soc_card *card)
{
	struct trats2_wm1811 *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	int reg = 0;
	int ret;

	if (!codec->active)
		clk_prepare_enable(machine->codec_mclk);

	/* change sysclk from mclk2 to pll out of mclk1 */
	ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1,
			WM8994_FLL_SRC_MCLK1,
			XTAL_24MHZ_AP,
			CODEC_DEFAULT_SYNC_CLK);
	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to start FLL1: 0x%x\n", ret);

	ret = snd_soc_dai_set_sysclk(aif1_dai, WM8994_SYSCLK_FLL1,
			CODEC_DEFAULT_SYNC_CLK, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(aif1_dai->dev,
			"Unable to switch sysclk to FLL1: 0x%x\n", ret);

	codec_micd_set_rate(codec);

	/* workaround for jack detection
	 * sometimes WM8994_GPIO_1 type changed wrong function type
	 * so if type mismatched, update to IRQ type
	 */
	reg = snd_soc_read(codec, WM8994_GPIO_1);

	if ((reg & WM8994_GPN_FN_MASK) != WM8994_GP_FN_IRQ) {
		dev_err(codec->dev, "%s: GPIO1 type 0x%x\n", __func__, reg);
		snd_soc_write(codec, WM8994_GPIO_1, WM8994_GP_FN_IRQ);
	}

	return 0;
}

static int trats2_wm1811_parse_dt(struct trats2_wm1811 *machine,
				   struct device *dev)
{
	struct device_node *node = dev->of_node;
	int gpio, ret, i;

	of_property_read_u32_array(node, "samsung,mic-availability",
				   machine->mic_avail,
				   ARRAY_SIZE(machine->mic_avail));
	machine->ignore_earjack = of_property_read_bool(node,
					"samsung,ignore-earjack");
	machine->cp_wb_support = of_property_read_bool(node, "samsung,cp-wb");
	machine->gpio_vps_en = -EINVAL;

	for (i = MIC_MAIN; i < MIC_MAX; i++) {
		machine->gpio_mic_bias[i] = -EINVAL;

		if (!machine->mic_avail[i])
			break;

		gpio = of_get_named_gpio(node, "samsung,mic-en-gpios", i);

		if (!gpio_is_valid(gpio)) {
			dev_info(dev, "%s uses CODEC MICBIAS1\n", mic_names[i]);
			continue;
		}
		ret = devm_gpio_request_one(dev, gpio, GPIOF_OUT_INIT_LOW,
							mic_names[i]);
		if (ret) {
			dev_err(dev, "%s gpio request failed\n", mic_names[i]);
			continue;
		}
		dev_info(dev, "%s enable GPIO initialized\n", mic_names[i]);
		machine->gpio_mic_bias[i] = gpio;
	}

	gpio = of_get_named_gpio(node, "samsung,vps-en-gpios", 0);
	if (!gpio_is_valid(gpio))
		return 0;
	ret = devm_gpio_request_one(dev, gpio, GPIOF_OUT_INIT_LOW, "wm1811");
	if (!ret)
		machine->gpio_vps_en = gpio;

	pr_info("gpio: %d, ret: %d\n", ret, gpio);
	return ret;
}

static int trats2_wm1811_probe(struct platform_device *pdev)
{
	struct snd_soc_dai_link *dai_link = &machine_dai[0];
	struct trats2_wm1811 *machine;
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

	ret = trats2_wm1811_parse_dt(machine, &pdev->dev);
	if (ret < 0)
		return ret;

	dai_link->cpu_dai_name = NULL;
	dai_link->cpu_of_node = of_parse_phandle(pdev->dev.of_node,
						"samsung,i2s-controller", 0);
	if (dai_link->cpu_of_node == NULL) {
		dev_err(&pdev->dev, "i2s-controller property parse error\n");
		return -EINVAL;
	}
	dai_link->platform_name = NULL;
	dai_link->platform_of_node = dai_link->cpu_of_node;

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

	machine->clk_pll = clk_get(&pdev->dev, "pll");
	if (IS_ERR(machine->clk_pll)) {
		dev_err(&pdev->dev, "failed to get pll clock\n");
		return PTR_ERR(machine->clk_pll);
	}

	ret = snd_soc_of_parse_card_name(card, "samsung,model");
	if (ret)
		return -ENODEV;

	dev_info(&pdev->dev, "Card name is %s\n", card->name);
	/* FIXME: Clean up on error path */

	snd_soc_card_set_drvdata(card, machine);

	/* register voice DAI */
	ret = snd_soc_register_component(&pdev->dev, &voice_component,
					voice_dai, ARRAY_SIZE(voice_dai));
	if (ret)
		return ret;

	return snd_soc_register_card(card);
}

static int trats2_wm1811_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct trats2_wm1811 *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);
	clk_put(machine->codec_mclk);
	clk_put(machine->clk_pll);
	return 0;
}

static const struct of_device_id trats2_wm1811_of_match[] = {
	{
		.compatible = "samsung,trats2-wm1811",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, trats2_wm1811_of_match);

static struct platform_driver trats2_wm1811_driver = {
	.driver = {
		.name = "trats2-wm1811",
		.of_match_table = trats2_wm1811_of_match,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = trats2_wm1811_probe,
	.remove = trats2_wm1811_remove,
};

module_platform_driver(trats2_wm1811_driver);

MODULE_AUTHOR("KwangHui Cho <kwanghui.cho@samsung.com>");
MODULE_AUTHOR("Sylwester Nawrocki <s.nawrocki@samsung.com>");
MODULE_DESCRIPTION("trats2 machine ASoC driver");
MODULE_LICENSE("GPL");
