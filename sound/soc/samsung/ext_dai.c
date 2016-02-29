/*
 *  sound/soc/samsung/ext_dai.c
 *
 *  Copyright (c) 2015 Samsung Electronics Co. Ltd
 *	Kwang-Hui Cho <kwanghui.cho@samsung.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under  the terms of the GNU General  Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/platform_device.h>
#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/ext-dai-device.h>

static struct snd_soc_dai_driver ext_dai[] = {
	{
		.name = "ext-modem",
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
		.name = "ext-bluetooth",
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

static __devinit int sound_ext_dai_probe(struct platform_device *pdev)
{
	struct sound_ext_dai_pdata *pdata;
	int i;

	pdata = pdev->dev.platform_data;
	if (pdata) {
		for (i = 0; i < ARRAY_SIZE(ext_dai); i++) {
			ext_dai[i].playback.channels_max =
							pdata->channels_max;
			ext_dai[i].playback.rate_max = pdata->rate_max;
			ext_dai[i].playback.rates = pdata->rates;
			ext_dai[i].playback.formats = pdata->formats;
			ext_dai[i].capture.channels_max = pdata->channels_max;
			ext_dai[i].capture.rate_max = pdata->rate_max;
			ext_dai[i].capture.rates = pdata->rates;
			ext_dai[i].capture.formats = pdata->formats;
		}
	} else {
		dev_info(&pdev->dev, "Use default value\n");
	}

	return snd_soc_register_dais(&pdev->dev, ext_dai,
						ARRAY_SIZE(ext_dai));

}

static __devexit int sound_ext_dai_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dais(&pdev->dev, ARRAY_SIZE(ext_dai));
	return 0;
}

static struct platform_driver sound_ext_dai_driver = {
	.probe	= sound_ext_dai_probe,
	.remove	= __devexit_p(sound_ext_dai_remove),
	.driver	= {
		.name = "sound-ext-dai",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(sound_ext_dai_driver);

MODULE_AUTHOR("Kwang-Hui Cho, <kwanghui.cho@samsung.com>");
MODULE_DESCRIPTION("Sound Driver for External Voice");
MODULE_ALIAS("platform:sound-voice");
MODULE_LICENSE("GPL");
