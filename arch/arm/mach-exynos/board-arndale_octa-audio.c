/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/i2c.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include "board-arndale_octa.h"

static struct i2c_board_info i2c_devs1[] __initdata = {
#if defined(CONFIG_SND_SOC_AK4953)
        {
	            I2C_BOARD_INFO("ak4953a", 0x13),
		        },
#endif
};

static struct platform_device arndale_audio = {
        .name   = "SOC-AUDIO-ARNDALE",
	    .id = -1,
};


#ifdef CONFIG_SND_SAMSUNG_PCM
static struct platform_device exynos_smdk_pcm = {
	.name = "samsung-smdk-pcm",
	.id = -1,
};
#endif

static struct platform_device *smdk5420_audio_devices[] __initdata = {
	&s3c_device_i2c1,
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos5_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
	&exynos5_device_pcm0,
	&exynos_smdk_pcm,
#endif
#ifdef CONFIG_SND_SAMSUNG_SPDIF
	&exynos5_device_spdif,
#endif
#ifdef CONFIG_SND_SAMSUNG_ALP
	&exynos5_device_srp,
#endif
	&samsung_asoc_dma,
	&samsung_asoc_idma,
	&arndale_audio,
};

void __init exynos5_arndale_octa_audio_init(void)
{
	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	platform_add_devices(smdk5420_audio_devices,
			ARRAY_SIZE(smdk5420_audio_devices));
}
