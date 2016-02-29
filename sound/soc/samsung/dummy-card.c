/*
 *  dummy-card.c
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

static struct snd_soc_dai_link dummy_dai[] = {
	{
		.name = "dummy",
		.stream_name = "Dummy",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.codec_dai_name = "dummy-aif1",
		.platform_name = "snd-soc-dummy",
		.codec_name = "dummy-codec",
		.ignore_suspend = 1,
	}
};

static struct snd_soc_card dummy_snd_card = {
	.name = "DUMMY",
	.dai_link = dummy_dai,
	.num_links = ARRAY_SIZE(dummy_dai),
};

static int dummy_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &dummy_snd_card;
	int ret;

	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "%s: snd_soc_register_card() failed %d",
					__func__, ret);
	}

	return ret;
}

static int dummy_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}
static struct platform_driver dummy_machine_driver = {
	.driver = {
		.name = "dummy-snd-card",
		.owner = THIS_MODULE,
	},
	.probe = dummy_audio_probe,
	.remove = dummy_audio_remove,
};

module_platform_driver(dummy_machine_driver);

MODULE_DESCRIPTION("ALSA SoC Dummy");
MODULE_LICENSE("GPL");
