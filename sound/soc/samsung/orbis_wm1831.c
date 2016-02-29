/*
 *  orbis_wm1831.c
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
#include <linux/input.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/control.h>

#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <plat/clock.h>

#include "i2s.h"
#include "i2s-regs.h"

#include "../codecs/largo.h"
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#define MCLK_RATE       24000000
#define SYSCLK_RATE     147456000

#define LARGO_DEFAULT_MCLK2   32768

static struct input_dev *voice_input;

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
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
#endif

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
/*
 * WC1 I2S DAI operations. (AP Master)
 */
static int orbis_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, sclk, bfs, psr, rfs, ret;
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
	case 12288000:
	case 18432000:
		psr = 4;
		break;
	case 24576000:
	case 36864000:
		psr = 2;
		break;
	case 49152000:
	case 73728000:
		psr = 1;
		break;
	default:
		pr_err("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk * psr;
	pll = sclk;
	set_aud_pll_rate(pll);

#if 0
	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;
#endif

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
#else
/*
 * WC1 I2S DAI operations. (AP Slave)
 */
static int orbis_hw_params(struct snd_pcm_substream *substream,
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

	return 0;
}
#endif
static int orbis_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;

	if (codec)
		arizona_set_aif_tristate(codec, false);
	else
		pr_info("%s: codec is null\n", __func__);

	return 0;
}

static void orbis_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;

	if (codec)
		arizona_set_aif_tristate(codec, true);
	else
		pr_info("%s: codec is null\n", __func__);
}

static int dummy_delay_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
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

	pr_info("Delay for %d msec\n", delay);

	msleep_interruptible(delay);
	return 1;
}

static const char *const ucm_verb_text[] = {
		"None",
		"HiFi",
};

static const char *const ucm_dev_text[] = {
		"None",
		"Mainmic",
		"VR_Mainmic",
		"STT_Mainmic",
		"Ez2Control",
		"Ez2ControlTP",
		"Begin"
};
static int ucm_verb;
static int ucm_device;

static int ucm_verb_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	ucontrol->value.integer.value[0] = ucm_verb;
	return 0;
}

static int ucm_verb_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucm_verb = ucontrol->value.integer.value[0];
	pr_info("%s(): %s - %s\n", __func__, ucm_verb_text[ucm_verb],
			ucm_dev_text[ucm_device]);
	return 1;
}

static int ucm_device_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	ucontrol->value.integer.value[0] = ucm_device;
	return 0;
}

static int ucm_device_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucm_device = ucontrol->value.integer.value[0];
	pr_info("%s(): %s - %s\n", __func__, ucm_verb_text[ucm_verb],
			ucm_dev_text[ucm_device]);
	return 1;
}

static const struct soc_enum orbis_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, ucm_verb_text),
	SOC_ENUM_SINGLE_EXT(7, ucm_dev_text),
};

static const struct snd_kcontrol_new orbis_snd_controls[] = {
	SOC_SINGLE_EXT("Dummy Delay", SND_SOC_NOPM, 0, 1000, 0,
				dummy_delay_get, dummy_delay_put),
	SOC_ENUM_EXT("UCM Verb", orbis_snd_enum[0],
				ucm_verb_get, ucm_verb_put),
	SOC_ENUM_EXT("UCM Device", orbis_snd_enum[1],
				ucm_device_get, ucm_device_put),
};

static int i2s_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;

	err = snd_soc_add_codec_controls(codec, orbis_snd_controls,
					ARRAY_SIZE(orbis_snd_controls));
	if (err < 0)
		return err;
	return 0;
}

static struct snd_soc_ops orbis_ops = {
	.hw_params = orbis_hw_params,
	.startup = orbis_startup,
	.shutdown = orbis_shutdown,
};

#ifdef CONFIG_SND_SOC_SAMSUNG_EXT_DAI
static int orbis_aif2_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	dev_info(codec_dai->dev, "AIF2 DAI %s params ch %d, rate %d\n",
			((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? \
						"playback" : "capture"),
						params_channels(params),
						params_rate(params));

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set BT mode: %d\n", ret);
		return ret;
	}

	return 0;
}

static struct snd_soc_ops orbis_aif2_ops = {
	.hw_params = orbis_aif2_hw_params,
};
#endif

static struct snd_soc_dai_link orbis_dai[] = {
	{ /* I2S DAI interface */
		.name = "I2S Arizona",
		.stream_name = "i2s",
		.cpu_dai_name = "samsung-i2s.2",
		.codec_dai_name = "largo-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "largo-codec",
#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
#else
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBM_CFM,
#endif
		.ops = &orbis_ops,
		.init = i2s_dai_init,
	},
	{ /* Voice Control DAI */
		.name = "CPU-DSP Voice Control",
		.stream_name = "CPU-DSP Voice Control",
		.cpu_dai_name = "largo-cpu-voicectrl",
		.platform_name = "largo-codec",
		.codec_dai_name = "largo-dsp-voicectrl",
		.codec_name = "largo-codec",
	},
#ifdef CONFIG_SND_SOC_SAMSUNG_EXT_DAI
	{ /* BT */
		.name = "Bluetooth Sco",
		.stream_name = "Bluetooth Sco",
		.cpu_dai_name = "ext-bluetooth",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "largo-aif2",
		.codec_name = "largo-codec",
		.ops = &orbis_aif2_ops,
	},
#endif
};

static struct snd_soc_dapm_widget orbis_widgets[] = {
	SND_SOC_DAPM_MIC("DMIC1", NULL),
	SND_SOC_DAPM_MIC("DMIC2", NULL),
};

static struct snd_soc_dapm_route orbis_routes[] = {
	{ "DMIC1", NULL, "MICBIAS1" },
	{ "DMIC2", NULL, "MICBIAS2" },
	{ "IN1L", NULL, "DMIC1" },
	{ "IN1R", NULL, "DMIC1" },
	{ "IN2L", NULL, "DMIC2" },
	{ "IN2R", NULL, "DMIC2" },
};

static int orbis_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;

	dev_info(codec->dev, "Current PMU_DEBUG reg. [0x%X]\n",
					readl(EXYNOS_PMU_DEBUG));
	return 0;
}

static int orbis_resume_pre(struct snd_soc_card *card)
{
	return 0;
}

static int orbis_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	int ret;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		if (dapm->bias_level != SND_SOC_BIAS_STANDBY)
			break;

		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1,
						ARIZONA_FLL_SRC_MCLK2,
						LARGO_DEFAULT_MCLK2,
						SYSCLK_RATE);
		if (ret < 0)
			pr_err("%s(): Failed to start FLL: %d\n",
					__func__, ret);
		break;
	default:
		break;
	}

	return 0;
}

static int orbis_set_bias_level_post(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	int ret;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1, 0, 0, 0);
		if (ret < 0) {
			pr_err("Failed to stop FLL: %d\n", ret);
			return ret;
		}
		break;
	default:
		break;
	}

	dapm->bias_level = level;
	return 0;
}

static void orbis_ez2c_trigger(void) {
	unsigned int keycode = KEY_VOICE_WAKEUP;

	pr_info("Raise key event (%d)\n", keycode);

	input_report_key(voice_input, keycode, 1);
	input_sync(voice_input);
	usleep_range(10000, 10000 + 100);
	input_report_key(voice_input, keycode, 0);
	input_sync(voice_input);
}

static int orbis_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	int ret;

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
					ARIZONA_CLK_SRC_FLL1,
					SYSCLK_RATE,
					SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set SYSCLK : %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(aif1_dai, ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret != 0) {
		dev_err(aif1_dai->dev, "Failed to set AFI1 clock : %d\n", ret);
		return ret;
	}

	arizona_set_ez2ctrl_cb(codec, orbis_ez2c_trigger);

	return 0;
}

static struct snd_soc_card orbis_snd_card = {
	.name = "ORBIS",
	.suspend_post = orbis_suspend_post,
	.resume_pre = orbis_resume_pre,
	.dai_link = orbis_dai,
	.num_links = ARRAY_SIZE(orbis_dai),

	.late_probe = orbis_late_probe,
	.dapm_widgets = orbis_widgets,
	.num_dapm_widgets = ARRAY_SIZE(orbis_widgets),
	.dapm_routes = orbis_routes,
	.num_dapm_routes = ARRAY_SIZE(orbis_routes),

	.set_bias_level = orbis_set_bias_level,
	.set_bias_level_post = orbis_set_bias_level_post,
};

static int init_input_device(void)
{
	int ret = 0;

	voice_input = input_allocate_device();
	if (!voice_input) {
		ret = -ENOMEM;
		goto init_input_device_exit;
	}

	voice_input->name = "voice input";
	set_bit(EV_SYN, voice_input->evbit);
	set_bit(EV_KEY, voice_input->evbit);
	set_bit(KEY_VOICE_WAKEUP, voice_input->keybit);

	ret = input_register_device(voice_input);
	if (ret < 0) {
		pr_info("%s: input_register_device failed\n", __func__);
		input_free_device(voice_input);
	}

init_input_device_exit:
	return ret;
}

static int deinit_input_device(void)
{
	input_unregister_device(voice_input);
	return 0;
}

#ifdef CONFIG_SLEEP_MONITOR
int orbis_audio_sm_read_cb_func(void *dev, unsigned int *val, int check_level, int caller_type)
{
	struct platform_device *pdev = dev;
	struct snd_soc_card *card = &orbis_snd_card;
	int bias_level = 0;
	int adsp = 0;
	int ret = DEVICE_ERR_1;

	bias_level = card->rtd[0].codec_dai->codec->dapm.bias_level;

	adsp = 	largo_get_adsp_status(card->rtd[1].codec_dai->codec);

	dev_dbg(&pdev->dev, "bias [%d], adsp [%d]\n",
				bias_level, adsp);

	*val = (adsp << 16) | (bias_level & 0xff);
	switch (bias_level) {
	case SND_SOC_BIAS_OFF:
		ret = DEVICE_POWER_OFF;
		break;
	case SND_SOC_BIAS_STANDBY:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_ON:
		if (adsp == 2)
			ret = DEVICE_ON_ACTIVE2; /* ez2ctrl streaming */
		else if (adsp == 1)
			ret = DEVICE_ON_LOW_POWER; /* ez2ctrl waiting */
		else if (adsp == 0)
			ret = DEVICE_ON_ACTIVE1; /* i2s running */
	}

	return ret;
}

static struct sleep_monitor_ops sm_ops = {
	.read_cb_func = orbis_audio_sm_read_cb_func,
};
#endif

static int __devinit orbis_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &orbis_snd_card;
	int ret;

	card->dev = &pdev->dev;
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "%s: snd_soc_register_card() failed %d",
					__func__, ret);
	}

	if (init_input_device() < 0)
		dev_err(&pdev->dev, "%s: input device init failed\n", __func__);

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops((void*)pdev, &sm_ops, SLEEP_MONITOR_AUDIO);
#endif
	return ret;
}

static int __devexit orbis_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	deinit_input_device();

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_unregister_ops(SLEEP_MONITOR_AUDIO);
#endif
	return 0;
}
static struct platform_driver orbis_snd_machine_driver = {
	.driver = {
		.name = "orbis-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = orbis_audio_probe,
	.remove = __devexit_p(orbis_audio_remove),
};

module_platform_driver(orbis_snd_machine_driver);

MODULE_DESCRIPTION("ALSA SoC B2 YMU831");
MODULE_LICENSE("GPL");
