/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/mfd/arizona/pdata.h>
#include <linux/mfd/arizona/registers.h>
#include <linux/input.h>

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
#include <mach/spi-clocks.h>

#include "board-universal3250.h"

extern unsigned int system_rev;

#define GPIO_CODEC_RESET EXYNOS3_GPE0(3)

struct platform_device orbis_audio_device = {
	.name = "orbis-audio",
	.id = -1,
};

struct platform_device dummy_codec = {
	.name = "dummy-codec",
	.id = -1,
};

struct platform_device dummy_snd_card = {
	.name = "dummy-snd-card",
	.id = -1,
};

#ifdef CONFIG_SND_SOC_SAMSUNG_EXT_DAI
struct platform_device ext_dai_device = {
	.name = "sound-ext-dai",
	.id = -1,
	.dev = {
		.platform_data = NULL,
	},
};
#endif

#ifdef CONFIG_S3C64XX_DEV_SPI1
static void wm1831_init_done(void)
{
	platform_device_register(&orbis_audio_device);
};

static struct s3c64xx_spi_csinfo spi1_csi[] = {
        [0] = {
		.line           = EXYNOS3_GPB(5),
		.set_level      = gpio_set_value,
		.fb_delay       = 0x2,
        },
};

static struct arizona_pdata arizona_platform_data = {
	.reset = GPIO_CODEC_RESET,
	.irq_base = IRQ_BOARD_CODEC_START,
	.irq_flags = IRQF_TRIGGER_LOW,
	.dmic_ref = {
		1, 0, 0, 0
	},
	.micbias = {
		[0] = { 1800, 0, 1, 0, 1},
	},
	.gpio_base = S3C_GPIO_END + 1,
	.init_done = wm1831_init_done,
};

static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias               = "wm1831",
		.platform_data          = &arizona_platform_data,
		.max_speed_hz           = 20*1000*1000,
		.bus_num                = 1,
		.chip_select            = 0,
		.mode                   = SPI_MODE_0,
		.irq			= IRQ_EINT(12),
		.controller_data        = &spi1_csi[0],
	}
};
#endif

static struct platform_device *orbis_audio_devices[] __initdata = {
#ifdef CONFIG_S3C64XX_DEV_SPI1
	&s3c64xx_device_spi1,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos4_device_i2s2,
#endif
	&samsung_asoc_dma,
#ifdef CONFIG_SND_SOC_SAMSUNG_EXT_DAI
	&ext_dai_device,
#endif
};


static struct platform_device *orbis_dummy_audio_devices[] __initdata = {
	&dummy_codec,
	&dummy_snd_card,
};

static void exynos3_orbis_audio_gpio_init(void)
{
	int err;

	pr_info("%s\n", __func__);
	err = gpio_request(GPIO_CODEC_RESET, "GPIO_CODEC_RESET");
	if (err) {
		pr_err("GPIO_CODEC_RESET set error\n");
		return;
	}
	gpio_direction_output(GPIO_CODEC_RESET, 1);
	gpio_set_value(GPIO_CODEC_RESET, 0);
	gpio_free(GPIO_CODEC_RESET);

	s3c_gpio_cfgpin(EXYNOS3_GPX1(4), S3C_GPIO_SFN(0xF));
}

static void exynos3_orbis_audio_clock_setup(void)
{
	pr_info("%s\n", __func__);

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
	clk_set_rate(&clk_fout_epll, 49152000);
	clk_enable(&clk_fout_epll);
#endif
}

void __init exynos3_orbis_audio_init(void)
{
	int ret;

#ifdef CONFIG_MACH_WC1
	if (system_rev == 0x00) {
		pr_info("%s: Revision 0x%x doesn't support audio\n",
						__func__, system_rev);

		platform_add_devices(orbis_dummy_audio_devices,
				ARRAY_SIZE(orbis_dummy_audio_devices));
		return;
	}
#endif
	if (system_rev >= 0x06) {
		spi1_csi[0].line = EXYNOS3_GPX2(5);
	} else {
		spi1_csi[0].line = EXYNOS3_GPB(5);
	}

	/* Codec LDO GPIO setup */
	exynos3_orbis_audio_gpio_init();

	/* audio clock setup */
	exynos3_orbis_audio_clock_setup();

	/* SPI1 setup */
#ifdef CONFIG_S3C64XX_DEV_SPI1
	exynos_spi_clock_setup(&s3c64xx_device_spi1.dev, 1);
	ret = exynos_spi_cfg_cs(spi1_csi[0].line, 1);
	if (!ret) {
		s3c64xx_spi1_set_platdata(&s3c64xx_spi1_pdata,
				EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi1_csi));
		spi_register_board_info(spi1_board_info,
				ARRAY_SIZE(spi1_board_info));
	} else {
		pr_err(KERN_ERR "Error requesting gpio for SPI-CH1 CS\n");
	}
#endif

	platform_add_devices(orbis_audio_devices,
			ARRAY_SIZE(orbis_audio_devices));
}
