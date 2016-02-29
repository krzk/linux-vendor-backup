/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/s3c64xx-spi.h>
#include <plat/iic.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>

#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/gpio.h>
#include <mach/exynos-sound.h>

#include "board-universal3250.h"

struct platform_device i2s_audio_device = {
	.name = "i2s-audio",
	.id = -1,
};

#ifdef CONFIG_SND_SOC_DUMMY_CODEC
static struct platform_device exynos_dummy_codec = {
	.name = "dummy-codec",
	.id = -1,
};
#endif

static struct platform_device *exynos3_audio_devices[] __initdata = {
	&i2s_audio_device,
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos4_device_i2s2,
#endif
	&samsung_asoc_dma,
#ifdef CONFIG_SND_SOC_DUMMY_CODEC
	&exynos_dummy_codec,
#endif

};

#ifdef CONFIG_MACH_SLIM
#define GPIO_MIC_EN	EXYNOS3_GPE0(6)
#else
#define GPIO_MIC_EN	(-1)
#endif
struct snd_board_info board_info = {
	.mic_en_gpio = GPIO_MIC_EN,

};

static void exynos3_audio_gpio_init(void)
{
}

static void exynos3_audio_clock_setup(void)
{
	pr_info("%s\n", __func__);

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
	clk_set_rate(&clk_fout_epll, 49152000);
	clk_enable(&clk_fout_epll);
#endif
}

void __init exynos3_i2s_dummy_audio_init(void)
{
	exynos3_audio_gpio_init();

	/* audio clock setup */
	exynos3_audio_clock_setup();

	i2s_audio_device.dev.platform_data = &board_info;

	platform_add_devices(exynos3_audio_devices,
			ARRAY_SIZE(exynos3_audio_devices));
}
