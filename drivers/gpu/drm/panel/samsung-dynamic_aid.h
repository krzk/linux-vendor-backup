#ifndef __DRM_PANEL_SAMSUNG_DYNAMIC_AID_H__
#define __DRM_PANEL_SAMSUNG_DYNAMIC_AID_H__
/*
 * Dynamic AMOLED Impulse Driving (DAID) helper functions.
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * Andrzej Hajda <a.hajda@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>

/* GCP - gamma control points */
#define DAID_GCP_COUNT 10
#define DAID_VOUT_COUNT 256
#define DAID_PARAM_COUNT (3 * (DAID_GCP_COUNT + 1) + 2)

typedef int daid_rgb[3];

struct daid_cfg {
	int vreg_out;
	const int *nits;
	int nits_count;
	int nit_gct;
	const int *brightness_base;
	const int (*gradation)[DAID_GCP_COUNT];
	const daid_rgb (*color_offset)[DAID_GCP_COUNT];
};

int daid_calc_gammodes(u8 (*gamma)[DAID_PARAM_COUNT],
	struct daid_cfg *cfg, u8 *mtp);

#endif
