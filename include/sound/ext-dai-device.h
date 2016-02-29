/*
 * Copyright (c) 2015 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 */

#ifndef __EXT_DAI_DEVICE_H
#define __EXT_DAI_DEVICE_H

#include <linux/platform_device.h>
#include <linux/types.h>

struct sound_ext_dai_pdata {
	unsigned int channels_max;
	unsigned int rate_max;
	unsigned int rates;
	u64 formats;
};

#endif /* __EXT_DAI_DEVICE_H */
