/*
 * MIPI-DSI based s6e3ha2 AMOLED 5.7 inch panel driver.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * Donghwa Lee <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include "samsung-dynamic_aid.h"

#define LDI_MTP_REG 0xc8
#define LDI_GAMMODE1 0xca

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		100
#define DEFAULT_BRIGHTNESS	80

static const u8 MDNIE_6500K[] = {
	0xec, 0x98, 0x24, 0x10, 0x14, 0xb3, 0x01, 0x0e, 0x01, 0x00,
	0x66, 0xfa, 0x2d, 0x03, 0x96, 0x02, 0xff, 0x00, 0x00, 0x07,
	0xff, 0x07, 0xff, 0x14, 0x00, 0x0a, 0x00, 0x32, 0x01, 0xf4,
	0x0b, 0x8a, 0x6e, 0x99, 0x1b, 0x17, 0x14, 0x1e, 0x02, 0x5f,
	0x02, 0xc8, 0x03, 0x33, 0x02, 0x22, 0x10, 0x10, 0x07, 0x07,
	0x20, 0x2d, 0x01, 0x40, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x02, 0x1b, 0x02, 0x1b, 0x02, 0x1b, 0x02, 0x1b,
	0x09, 0xa6, 0x09, 0xa6, 0x09, 0xa6, 0x09, 0xa6, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0xFF, 0x40, 0x67, 0xa9, 0x0c, 0x0c, 0x0c, 0x0c, 0x00,
	0xaa, 0xab, 0x00, 0xaa, 0xab, 0x00, 0xaa, 0xab, 0x00, 0xaa,
	0xab, 0xd5, 0x2c, 0x2a, 0xff, 0xf5, 0x63, 0xfe, 0x4a, 0xff,
	0xff, 0xf9, 0xf8, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff,
	0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00,
	0xff, 0xff, 0x00, 0xf7, 0x00, 0xed, 0x00,
};

static const u8 MDNIE_BYPASS[] = {
	0xec, 0x98, 0x24, 0x10, 0x14, 0xb3, 0x01, 0x0e, 0x01, 0x00,
	0x66, 0xfa, 0x2d, 0x03, 0x96, 0x00, 0xff, 0x00, 0x00, 0x07,
	0xff, 0x07, 0xff, 0x14, 0x00, 0x0a, 0x00, 0x32, 0x01, 0xf4,
	0x0b, 0x8a, 0x6e, 0x99, 0x1b, 0x17, 0x14, 0x1e, 0x02, 0x5f,
	0x02, 0xc8, 0x03, 0x33, 0x02, 0x22, 0x10, 0x10, 0x07, 0x07,
	0x20, 0x2d, 0x01, 0x00, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x02, 0x1b, 0x02, 0x1b, 0x02, 0x1b, 0x02, 0x1b,
	0x09, 0xa6, 0x09, 0xa6, 0x09, 0xa6, 0x09, 0xa6, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0xFF, 0x00, 0x67, 0xa9, 0x0c, 0x0c, 0x0c, 0x0c, 0x00,
	0xaa, 0xab, 0x00, 0xaa, 0xab, 0x00, 0xaa, 0xab, 0x00, 0xaa,
	0xab, 0xd5, 0x2c, 0x2a, 0xff, 0xf5, 0x63, 0xfe, 0x4a, 0xff,
	0xff, 0xf9, 0xf8, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff,
	0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
};

#define S6E3HA2_VREG_OUT 6400 /* mV */

static const int s6e3ha2_nits[] = {
	2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 20,
	21, 22, 24, 25, 27, 29, 30, 32, 34, 37, 39, 41, 44, 47, 50, 53,
	56, 60, 64, 68, 72, 77, 82, 87, 93, 98, 105, 111, 119, 126, 134,
	143, 152, 162, 172, 183, 195, 207, 220, 234, 249, 265, 282, 300,
	316, 333, 360, 360
};
#define S6E3HA2_NITS_COUNT ARRAY_SIZE(s6e3ha2_nits)

static const int s6e3ha2_brightness_base[S6E3HA2_NITS_COUNT] = {
	113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
	113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
	113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
	113, 122, 128, 136, 145, 155, 164, 175, 187, 196, 208, 223, 234,
	253, 253, 253, 253, 253, 253, 253, 268, 283, 300, 319, 337, 360,
	360
};

static const int s6e3ha2_gradation[S6E3HA2_NITS_COUNT][DAID_GCP_COUNT] = {
	{0, 4, 27, 35, 33, 27, 22, 17, 9, 0},
	{0, 4, 29, 29, 25, 17, 17, 12, 8, 0},
	{0, 23, 26, 24, 21, 19, 16, 12, 7, 0},
	{0, 21, 22, 20, 17, 15, 13, 9, 5, 0},
	{0, 21, 21, 19, 16, 14, 12, 10, 6, 0},
	{0, 21, 20, 18, 14, 13, 11, 9, 5, 0},
	{0, 19, 19, 17, 13, 12, 10, 8, 4, 0},
	{0, 18, 19, 16, 13, 12, 9, 7, 4, 0},
	{0, 20, 18, 15, 12, 10, 9, 7, 4, 0},
	{0, 16, 17, 15, 11, 10, 8, 7, 4, 0},
	{0, 17, 16, 14, 11, 10, 8, 6, 4, 0},
	{0, 17, 16, 14, 11, 10, 7, 6, 4, 0},
	{0, 17, 15, 13, 10, 9, 6, 5, 4, 0},
	{0, 15, 15, 12, 10, 9, 7, 6, 4, 0},
	{0, 16, 14, 12, 9, 8, 6, 5, 4, 0},
	{0, 13, 14, 11, 9, 8, 6, 5, 4, 0},
	{0, 14, 13, 10, 8, 7, 6, 5, 4, 0},
	{0, 11, 13, 10, 8, 7, 5, 5, 4, 0},
	{0, 10, 13, 10, 7, 7, 5, 5, 4, 0},
	{0, 11, 12, 9, 7, 6, 5, 5, 4, 0},
	{0, 12, 11, 8, 6, 6, 5, 4, 4, 0},
	{0, 11, 11, 8, 6, 6, 5, 5, 4, 0},
	{0, 12, 10, 8, 6, 5, 4, 4, 4, 0},
	{0, 9, 10, 8, 6, 5, 4, 4, 4, 0},
	{0, 8, 10, 8, 5, 5, 4, 5, 3, 0},
	{0, 10, 9, 7, 5, 5, 4, 4, 4, 0},
	{0, 7, 9, 7, 5, 4, 4, 4, 4, 0},
	{0, 8, 8, 6, 4, 4, 3, 4, 4, 0},
	{0, 8, 8, 6, 5, 4, 4, 4, 3, 0},
	{0, 10, 7, 6, 4, 4, 3, 4, 4, 0},
	{0, 8, 7, 6, 4, 4, 3, 4, 3, 0},
	{0, 5, 7, 6, 4, 3, 3, 4, 3, 0},
	{0, 8, 6, 5, 3, 4, 3, 4, 3, 0},
	{0, 6, 6, 5, 3, 3, 3, 4, 3, 0},
	{0, 8, 5, 5, 3, 3, 3, 4, 3, 0},
	{0, 6, 5, 5, 3, 3, 3, 4, 3, 0},
	{0, 4, 5, 4, 2, 3, 2, 4, 3, 0},
	{0, 6, 4, 4, 2, 3, 2, 3, 3, 0},
	{0, 4, 4, 4, 2, 3, 2, 3, 3, 0},
	{0, 2, 4, 3, 2, 3, 2, 3, 3, 0},
	{0, 2, 4, 3, 3, 2, 2, 3, 2, 0},
	{0, 1, 4, 3, 2, 2, 2, 2, 1, 0},
	{0, 1, 4, 3, 2, 2, 3, 3, 2, 0},
	{0, 5, 3, 3, 2, 2, 3, 4, 2, 0},
	{0, 4, 3, 3, 2, 2, 2, 3, 1, 0},
	{0, 2, 3, 3, 1, 1, 2, 2, 0, 0},
	{0, 4, 3, 3, 2, 2, 3, 4, 0, 0},
	{0, 6, 3, 3, 2, 2, 3, 3, -2, 0},
	{0, 5, 3, 3, 1, 2, 2, 3, -1, 0},
	{0, 0, 3, 2, 1, 2, 3, 2, 0, 0},
	{0, 2, 3, 2, 2, 2, 3, 3, 0, 0},
	{0, 0, 2, 2, 2, 2, 3, 3, 0, 0},
	{0, 3, 2, 1, 2, 1, 2, 2, 0, 0},
	{0, 2, 1, 1, 1, 1, 2, 3, 1, 0},
	{0, 1, 1, 1, 1, 1, 2, 2, 0, 0},
	{0, 0, 1, 0, 1, 1, 1, 1, -1, 0},
	{0, 0, 1, 0, 1, 1, 1, 1, -1, 0},
	{0, 2, 1, 0, 1, 0, 1, 1, -1, 0},
	{0, 2, 0, 0, 0, 0, 1, 1, 0, 0},
	{0, 3, 0, 1, 0, 0, 1, 1, 1, 0},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 0},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 0},
	{0, 0, 0, 0, -1, 0, 1, 0, 1, 0},
	{0, 0, 0, -1, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static const daid_rgb s6e3ha2_color_ofs[S6E3HA2_NITS_COUNT][DAID_GCP_COUNT] = {
	{{0, 0, 0}, {0, 0, 0}, {21, -2, 1}, {2, 2, -8}, {-7, -1, -5}, {-22, 0, -14}, {-14, -1, -6}, {-5, 0, -3}, {-1, 1, 0}, {-3, 4, 0}},
	{{0, 0, 0}, {0, 0, 0}, {21, -2, 1}, {-3, 1, -8}, {-4, -1, -3}, {-26, -1, -15}, {-10, 1, -4}, {-4, 1, -1}, {-2, -1, -1}, {-3, 3, -1}},
	{{0, 0, 0}, {0, 0, 0}, {10, -2, -6}, {-7, 2, -10}, {-10, 4, -4}, {-18, -1, -8}, {-9, 1, -5}, {-3, 1, -1}, {-2, -1, -1}, {0, 4, 1}},
	{{0, 0, 0}, {0, 0, 0}, {10, 0, -6}, {-8, 2, -8}, {-12, 1, -6}, {-13, 1, -3}, {-7, 1, -4}, {-3, 1, -1}, {-1, 1, 0}, {0, 3, 1}},
	{{0, 0, 0}, {0, 0, 0}, {10, 2, -7}, {-7, 2, -8}, {-12, 2, -6}, {-11, 2, -2}, {-6, 1, -4}, {-3, 1, -1}, {-1, -1, -1}, {-2, 1, -2}},
	{{0, 0, 0}, {0, 0, 0}, {4, 4, -9}, {-6, 2, -8}, {-11, 2, -6}, {-9, 2, -3}, {-6, 1, -4}, {-3, 1, -1}, {0, 0, 0}, {-1, 1, -1}},
	{{0, 0, 0}, {0, 0, 0}, {4, 5, -11}, {-7, 1, -9}, {-12, 2, -7}, {-7, 2, -3}, {-5, 2, -3}, {-2, 1, 0}, {-2, 1, 0}, {0, 0, -1}},
	{{0, 0, 0}, {0, 0, 0}, {0, 1, -14}, {-7, 4, -9}, {-10, 1, -7}, {-7, 3, -4}, {-5, 2, -2}, {-2, 1, 0}, {-1, 1, 0}, {0, 0, -1}},
	{{0, 0, 0}, {0, 0, 0}, {0, 2, -14}, {-7, 3, -9}, {-9, 1, -7}, {-7, 3, -4}, {-5, 2, -2}, {-2, 1, 0}, {0, 1, 1}, {0, 0, -1}},
	{{0, 0, 0}, {0, 0, 0}, {1, 5, -14}, {-7, 2, -9}, {-9, 1, -7}, {-7, 2, -4}, {-3, 2, -2}, {-2, 1, 0}, {0, 1, 1}, {0, 0, -1}},
	{{0, 0, 0}, {0, 0, 0}, {2, 5, -13}, {-6, 3, -9}, {-7, 1, -6}, {-7, 2, -4}, {-3, 2, -2}, {-1, 1, 0}, {0, 1, 1}, {0, 0, -1}},
	{{0, 0, 0}, {0, 0, 0}, {1, 6, -14}, {-7, 2, -11}, {-7, 1, -6}, {-7, 2, -4}, {-2, 2, -2}, {-1, 1, 0}, {-1, 0, 0}, {0, 0, -1}},
	{{0, 0, 0}, {0, 0, 0}, {5, 8, -13}, {-6, 5, -11}, {-5, 1, -4}, {-7, 1, -4}, {-2, 2, -2}, {-1, 1, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {0, 3, -14}, {-6, 5, -11}, {-5, 1, -4}, {-6, 1, -4}, {-2, 2, -2}, {-1, 1, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {6, 10, -13}, {-8, 2, -11}, {-5, 1, -4}, {-6, 1, -4}, {-2, 2, -2}, {-1, 1, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {3, 6, -13}, {-7, 3, -11}, {-5, 1, -4}, {-6, 1, -3}, {-2, 2, -2}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {8, 9, -13}, {-7, 3, -11}, {-5, 1, -5}, {-6, 1, -3}, {-2, 2, -2}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {6, 5, -16}, {-7, 4, -10}, {-6, 0, -6}, {-6, 1, -3}, {-2, 2, -1}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {1, 3, -17}, {-7, 3, -8}, {-5, 1, -5}, {-5, 1, -3}, {-2, 2, 0}, {-1, 0, -2}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {5, 7, -15}, {-5, 3, -8}, {-5, 2, -5}, {-5, 1, -2}, {-2, 2, 0}, {-1, 0, -2}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {6, 8, -15}, {-3, 5, -10}, {-5, 2, -4}, {-3, 1, -2}, {-2, 2, 0}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {5, 7, -15}, {-5, 4, -10}, {-5, 2, -4}, {-3, 1, -2}, {-2, 2, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {11, 12, -15}, {-5, 2, -10}, {-5, 1, -4}, {-3, 1, -2}, {-2, 2, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {8, 10, -16}, {-5, 2, -9}, {-5, 1, -5}, {-3, 0, -2}, {-2, 2, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {7, 9, -16}, {-7, -1, -9}, {-2, 1, -4}, {-3, 2, -1}, {-2, 1, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {11, 12, -14}, {-2, 2, -9}, {-3, 0, -5}, {-3, 2, 0}, {-2, 1, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {7, 8, -16}, {-4, 1, -9}, {-3, 0, -5}, {-2, 2, 1}, {-1, 1, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {7, 9, -16}, {1, 5, -7}, {-3, 0, -5}, {-2, 2, 1}, {-1, 1, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {8, 9, -15}, {0, 2, -8}, {-3, 0, -5}, {-2, 2, 1}, {-1, 1, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {15, 16, -12}, {-1, 2, -9}, {-3, 0, -5}, {-2, 2, 1}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {13, 14, -12}, {0, 2, -8}, {-5, -2, -7}, {-2, 2, 1}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {10, 12, -14}, {1, 0, -9}, {-4, -2, -7}, {-2, 2, 1}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {12, 14, -12}, {3, 3, -8}, {-2, 1, -5}, {-2, 0, 1}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {10, 12, -13}, {3, 2, -8}, {-2, 1, -4}, {-2, 0, 0}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {19, 18, -11}, {3, 1, -8}, {-2, 1, -4}, {-2, 0, 0}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {14, 15, -13}, {2, 0, -9}, {-1, 0, -4}, {-2, 0, 0}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {7, 8, -11}, {7, 3, -8}, {0, 2, -2}, {-2, 0, 0}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {13, 12, -11}, {7, 3, -8}, {0, 1, -2}, {-2, 0, 0}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {11, 11, -13}, {6, 1, -9}, {0, 1, -2}, {-2, 0, 0}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {5, 3, -12}, {11, 4, -8}, {1, 2, -1}, {-2, 0, 0}, {-1, 0, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {6, 6, -15}, {6, -1, -10}, {1, 2, -1}, {-2, 0, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {5, 5, -12}, {6, 0, -10}, {1, 2, -1}, {-2, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {4, 3, -10}, {3, -4, -12}, {1, 2, -1}, {-2, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {13, 10, -7}, {4, -2, -10}, {1, 1, -1}, {-1, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {17, 14, -3}, {-1, -3, -10}, {0, 0, -1}, {-1, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {16, 12, -4}, {0, -3, -9}, {0, 0, -1}, {-1, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {15, 11, -4}, {4, 0, -6}, {0, 0, -1}, {-1, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {11, 8, -4}, {-1, -4, -9}, {0, 0, -1}, {-1, 1, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {10, 6, -5}, {0, -3, -7}, {0, 0, -1}, {-2, 1, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {10, 4, -5}, {2, -1, -5}, {0, 0, 0}, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {15, 10, -1}, {-3, -4, -7}, {0, 0, 0}, {-1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {15, 9, -1}, {-1, -2, -5}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {15, 9, 0}, {0, 0, -5}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {13, 9, 0}, {0, -1, -4}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {13, 8, -1}, {0, -2, -5}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {11, 1, -2}, {4, 3, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {8, -3, -5}, {5, 3, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {7, -4, -5}, {5, 3, -1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
	{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
};

unsigned char VINT_TABLE[] = {
	0x18, 0x19, 0x1A, 0x1B, 0x1C,
	0x1D, 0x1E, 0x1F, 0x20, 0x21
};

enum s6e3ha2_model { MODEL_1440, MODEL_1600 };

struct s6e3ha2 {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *bl_dev;
	enum s6e3ha2_model model;

	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpio;
	struct gpio_desc *panel_en_gpio;
	struct videomode vm;
	u32 width_mm;
	u32 height_mm;
	u32 vr_mode;
	bool hmt_mode;

	/* This field is tested by functions directly accessing DSI bus before
	 * transfer, transfer is skipped if it is set. In case of transfer
	 * failure or unexpected response the field is set to error value.
	 * Such construct allows to eliminate many checks in higher level
	 * functions.
	 */
	int error;

	/*
	 * This mutex lock is used to ensure to prevent from being raced
	 * between Panel device access by VR sysfs and KMS power management
	 * operation because while enabling or disabing Panel device power
	 * it's possible to access Panel device by VR sysfs interface.
	 */
	struct mutex lock;

	u8 gammodes[S6E3HA2_NITS_COUNT][DAID_PARAM_COUNT];
};

static inline struct s6e3ha2 *panel_to_s6e3ha2(struct drm_panel *panel)
{
	return container_of(panel, struct s6e3ha2, panel);
}

static int s6e3ha2_clear_error(struct s6e3ha2 *ctx)
{
	int ret = ctx->error;

	ctx->error = 0;
	return ret;
}

static void s6e3ha2_dcs_write(struct s6e3ha2 *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing dcs seq: %*ph\n", ret,
							(int)len, data);
		ctx->error = ret;
	}
}

#define s6e3ha2_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	s6e3ha2_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define s6e3ha2_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	s6e3ha2_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define NSEQ(seq...) sizeof((char[]){ seq }), seq

static void s6e3ha2_write_nseq(struct s6e3ha2 *ctx, const u8 *nseq)
{
	int count;

	while ((count = *nseq++)) {
		s6e3ha2_dcs_write(ctx, nseq, count);
		nseq += count;
	}
}

static void s6e3ha2_dcs_read(struct s6e3ha2 *ctx, u8 cmd, void *buf, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dsi_set_maximum_return_packet_size(dsi, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error setting maximum return packet size to %lu\n",
			len);
		ctx->error = ret;
		return;
	}

	ret = mipi_dsi_dcs_read(dsi, cmd, buf, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq(%#x)\n", ret, cmd);
		ctx->error = ret;
	}
}

static void s6e3ha2_test_key_on_f0(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf0, 0x5a, 0x5a);
}

static void s6e3ha2_test_key_off_f0(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf0, 0xa5, 0xa5);
}

static void s6e3ha2_test_key_on_fc(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfc, 0x5a, 0x5a);
}

static void s6e3ha2_test_key_off_fc(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfc, 0xa5, 0xa5);
}

static void s6e3ha2_single_dsi_set1(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf2, 0x67);
}

static void s6e3ha2_single_dsi_set2(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf9, 0x09);
}

static void s6e3ha2_freq_calibration(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfd, 0x1c);
	if (ctx->model != MODEL_1440)
		s6e3ha2_dcs_write_seq_static(ctx, 0xf2, 0x67, 0x40, 0xc5);
	s6e3ha2_dcs_write_seq_static(ctx, 0xfe, 0x20, 0x39);
	s6e3ha2_dcs_write_seq_static(ctx, 0xfe, 0xa0);
	s6e3ha2_dcs_write_seq_static(ctx, 0xfe, 0x20);
	if (ctx->model == MODEL_1440)
		s6e3ha2_dcs_write_seq_static(ctx, 0xce, 0x03, 0x3b, 0x12, 0x62,
			0x40, 0x80, 0xc0, 0x28, 0x28, 0x28, 0x28, 0x39, 0xc5);
	else
		s6e3ha2_dcs_write_seq_static(ctx, 0xce, 0x03, 0x3b, 0x14, 0x6d,
			0x40, 0x80, 0xc0, 0x28, 0x28, 0x28, 0x28, 0x39, 0xc5);
}

static void s6e3ha2_aor_control(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb2, 0x03, 0x10);
}

static void s6e3ha2_caps_elvss_set(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb6, 0x9c, 0x0a);
}

static void s6e3ha2_acl_off(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0x55, 0x00);
}

static void s6e3ha2_acl_off_opr(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb5, 0x40);
}

static void s6e3ha2_test_global(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb0, 0x07);
}

static void s6e3ha2_test(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb8, 0x19);
}

static void s6e3ha2_touch_hsync_on1(struct s6e3ha2 *ctx) {
	s6e3ha2_dcs_write_seq_static(ctx,
			0xbd, 0x33, 0x11, 0x02, 0x16, 0x02, 0x16);
}

static void s6e3ha2_pentile_control(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xc0, 0x00, 0x00, 0xd8, 0xd8);
}

static void s6e3ha2_poc_global(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb0, 0x20);
}

static void s6e3ha2_poc_setting(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfe, 0x08);
}

static void s6e3ha2_pcd_set_off(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xcc, 0x40, 0x51);
}

static void s6e3ha2_err_fg_set(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xed, 0x44);
}

static void s6e3ha2_hbm_off(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0x53, 0x00);
}

static void s6e3ha2_te_start_setting(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb9, 0x10, 0x09, 0xff, 0x00, 0x09);
}

#define BITS(v, h, l) ((int)(GENMASK((h), (l)) & (v)) >> (l))

static void s6e3ha2_calc_gammodes(struct s6e3ha2 *ctx)
{
	struct daid_cfg cfg = {
		.vreg_out = S6E3HA2_VREG_OUT,
		.nits = s6e3ha2_nits,
		.nits_count = S6E3HA2_NITS_COUNT,
		.nit_gct = 360,
		.gradation = s6e3ha2_gradation,
		.color_offset = s6e3ha2_color_ofs,
		.brightness_base = s6e3ha2_brightness_base,
	};
	u8 mtp[44];

	s6e3ha2_dcs_read(ctx, LDI_MTP_REG, mtp, ARRAY_SIZE(mtp));
	if (ctx->error < 0)
		return;

	dev_dbg(ctx->dev, "MTP: %*ph\n", (int)ARRAY_SIZE(mtp), mtp);
	dev_info(ctx->dev, "Manufacture date: %d-%02d-%02d %02d:%02d\n",
		BITS(mtp[40], 7, 4) + 2011, BITS(mtp[40], 3, 0),
		BITS(mtp[41], 4, 0), BITS(mtp[42], 4, 0), BITS(mtp[43], 5, 0));

	daid_calc_gammodes(ctx->gammodes, &cfg, mtp);
}

static void s6e3ha2_gamma_update(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf7, 0x03);
}

static void s6e3ha2_gamma_update_l(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf7, 0x00);
}

static void s6e3ha2_vr_enable(struct s6e3ha2 *ctx, int enable)
{
	/* TEST KEY ENABLE. */
	s6e3ha2_test_key_on_f0(ctx);
	s6e3ha2_test_key_on_fc(ctx);

	if (enable)
		s6e3ha2_dcs_write(ctx, MDNIE_6500K, ARRAY_SIZE(MDNIE_6500K));
	else
		s6e3ha2_dcs_write(ctx, MDNIE_BYPASS, ARRAY_SIZE(MDNIE_BYPASS));

	/* TEST KEY DISABLE. */
	s6e3ha2_test_key_off_f0(ctx);
	s6e3ha2_test_key_off_fc(ctx);

	/* TEST KEY ENABLE. */
	s6e3ha2_test_key_on_f0(ctx);
	s6e3ha2_test_key_on_fc(ctx);

	if (enable)
		s6e3ha2_dcs_write_seq_static(ctx, 0xeb, 0x01, 0x00, 0x3c);
	else
		s6e3ha2_dcs_write_seq_static(ctx, 0xeb, 0x01, 0x00, 0x00);

	/* TEST KEY DISABLE. */
	s6e3ha2_test_key_off_f0(ctx);
	s6e3ha2_test_key_off_fc(ctx);
}


static int s6e3ha2_get_brightness(struct backlight_device *bl_dev)
{
	return bl_dev->props.brightness;
}

static void s6e3ha2_set_vint(struct s6e3ha2 *ctx)
{
	int vind = (ARRAY_SIZE(VINT_TABLE) - 1)
		 * ctx->bl_dev->props.brightness / MAX_BRIGHTNESS;

	s6e3ha2_dcs_write_seq(ctx, 0xf4, 0x8b, VINT_TABLE[vind]);
}

static unsigned int s6e3ha2_get_brightness_index(unsigned int brightness)
{
	return (brightness * (S6E3HA2_NITS_COUNT - 1)) / MAX_BRIGHTNESS;
}

static void s6e3ha2_update_gamma(struct s6e3ha2 *ctx, unsigned int brightness)
{
	unsigned int index = s6e3ha2_get_brightness_index(brightness);
	char data[DAID_PARAM_COUNT + 1];

	data[0] = LDI_GAMMODE1;
	memcpy(data + 1, ctx->gammodes[index], DAID_PARAM_COUNT);
	s6e3ha2_dcs_write(ctx, data, ARRAY_SIZE(data));
	s6e3ha2_gamma_update(ctx);

	if (!ctx->error)
		ctx->bl_dev->props.brightness = brightness;
}

static void s6e3ha2_set_hmt_gamma(struct s6e3ha2 *ctx)
{
	/* TODO */
	s6e3ha2_dcs_write_seq_static(ctx,
			0xca, 0x00, 0xf3, 0x00, 0xe6, 0x00, 0xf4, 0x86, 0x85,
			0x85, 0x85, 0x85, 0x86, 0x88, 0x87, 0x88, 0x86, 0x84,
			0x86, 0x83, 0x82, 0x85, 0x88, 0x86, 0x87, 0x8f, 0x93,
			0x85, 0xa6, 0x90, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00);
}

static void s6e3ha2_set_hmt_aid_parameter_ctl(struct s6e3ha2 *ctx)
{
	/* TODO */
	s6e3ha2_dcs_write_seq_static(ctx, 0xb2, 0x3, 0x13, 0x4);
}

static void s6e3ha2_set_hmt_elvss(struct s6e3ha2 *ctx)
{
	/* TODO */
	s6e3ha2_dcs_write_seq_static(ctx, 0xb6, 0x9c, 0xa);
}

static void s6e3ha2_set_hmt_vint(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf4, 0x8b, 0x21);
}

static void s6e3ha2_set_hmt_brightness(struct s6e3ha2 *ctx)
{
	s6e3ha2_test_key_on_f0(ctx);
	s6e3ha2_test_key_on_fc(ctx);

	s6e3ha2_set_hmt_gamma(ctx);
	s6e3ha2_set_hmt_aid_parameter_ctl(ctx);
	s6e3ha2_set_hmt_elvss(ctx);
	if (ctx->model == MODEL_1440)
		s6e3ha2_set_hmt_vint(ctx);
	s6e3ha2_gamma_update(ctx);
	s6e3ha2_gamma_update_l(ctx);

	s6e3ha2_test_key_off_fc(ctx);
	s6e3ha2_test_key_off_f0(ctx);
	/* TODO */
}

static void s6e3ha2_set_brightness(struct s6e3ha2 *ctx)
{
	if (ctx->hmt_mode) {
		s6e3ha2_set_hmt_brightness(ctx);
		return;
	}

	s6e3ha2_test_key_on_f0(ctx);
	s6e3ha2_update_gamma(ctx, ctx->bl_dev->props.brightness);
	s6e3ha2_aor_control(ctx);
	s6e3ha2_set_vint(ctx);
	s6e3ha2_test_key_off_f0(ctx);
}

static int s6e3ha2_bl_update_status(struct backlight_device *bl_dev)
{
	struct s6e3ha2 *ctx = (struct s6e3ha2 *)bl_get_data(bl_dev);
	unsigned int brightness = bl_dev->props.brightness;
	int ret;

	mutex_lock(&ctx->lock);

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bl_dev->props.max_brightness) {
		dev_err(ctx->dev, "Invalid brightness: %u\n", brightness);
		ret = -EINVAL;
		goto end;
	}

	if (bl_dev->props.power > FB_BLANK_NORMAL) {
		dev_err(ctx->dev,
			"panel must be at least in fb blank normal state\n");
		ret = -EPERM;
		goto end;
	}

	s6e3ha2_set_brightness(ctx);
	ret = s6e3ha2_clear_error(ctx);

end:
	mutex_unlock(&ctx->lock);

	return ret;
}

static const struct backlight_ops s6e3ha2_bl_ops = {
	.get_brightness = s6e3ha2_get_brightness,
	.update_status = s6e3ha2_bl_update_status,
};

static void s6e3ha2_panel_init(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_EXIT_SLEEP_MODE);

	usleep_range(5000, 6000);

	s6e3ha2_test_key_on_f0(ctx);
	s6e3ha2_single_dsi_set1(ctx);
	s6e3ha2_single_dsi_set2(ctx);

	s6e3ha2_test_key_on_fc(ctx);
	s6e3ha2_freq_calibration(ctx);
	s6e3ha2_test_key_off_fc(ctx);
	s6e3ha2_calc_gammodes(ctx);
	s6e3ha2_test_key_off_f0(ctx);
}

static int s6e3ha2_power_off(struct s6e3ha2 *ctx)
{
	return regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
}

static int s6e3ha2_disable(struct drm_panel *panel)
{
	struct s6e3ha2 *ctx = panel_to_s6e3ha2(panel);

	mutex_lock(&ctx->lock);

	s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	if (ctx->error != 0)
		goto err;

	 s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	if (ctx->error != 0)
		goto err;

	msleep(40);
	ctx->bl_dev->props.power = FB_BLANK_NORMAL;

	return 0;
err:
	mutex_unlock(&ctx->lock);
	return ctx->error;
}

static int s6e3ha2_unprepare(struct drm_panel *panel)
{
	struct s6e3ha2 *ctx = panel_to_s6e3ha2(panel);
	int ret;

	/*
	 * This function is called by mipi dsi driver
	 * after calling disable callback and mutex is locked by
	 * disable callback. So make sure to check if locked.
	 */
	if (!mutex_is_locked(&ctx->lock)) {
		WARN_ON(1);
		return -EPERM;
	}

	s6e3ha2_power_off(ctx);
	ret = s6e3ha2_clear_error(ctx);
	if (!ret)
		ctx->bl_dev->props.power = FB_BLANK_POWERDOWN;

	mutex_unlock(&ctx->lock);

	return ret;
}

static int s6e3ha2_power_on(struct s6e3ha2 *ctx)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	msleep(25);

	gpiod_set_value(ctx->panel_en_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->panel_en_gpio, 1);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5000, 6000);

	return 0;
}
static int s6e3ha2_prepare(struct drm_panel *panel)
{
	struct s6e3ha2 *ctx = panel_to_s6e3ha2(panel);
	int ret;

	/* This mutex will be unlocked at enable callback. */
	mutex_lock(&ctx->lock);

	ret = s6e3ha2_power_on(ctx);
	if (ret < 0) {
		mutex_unlock(&ctx->lock);
		return ret;
	}

	ctx->bl_dev->props.power = FB_BLANK_NORMAL;

	s6e3ha2_panel_init(ctx);
	if (ctx->error < 0)
		return s6e3ha2_unprepare(panel);

	return ret;
}

static void s6e3ha2_hmt_set(struct s6e3ha2 *ctx, bool enable);

static int s6e3ha2_enable(struct drm_panel *panel)
{
	struct s6e3ha2 *ctx = panel_to_s6e3ha2(panel);
	u8 id[3];

	/*
	 * This function is called by mipi dsi driver
	 * after calling prepare callback and mutex is locked by
	 * prepare callback. So make sure to check if locked.
	 */
	if (!mutex_is_locked(&ctx->lock)) {
		WARN_ON(1);
		return -EPERM;
	}

	msleep(120);

	s6e3ha2_dcs_read(ctx, MIPI_DCS_GET_DISPLAY_ID, id, ARRAY_SIZE(id));
	if (ctx->error >= 0)
		dev_info(ctx->dev, "Id: %*ph\n", (int)ARRAY_SIZE(id), id);

	/* common setting */
	s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_SET_TEAR_ON, MIPI_DSI_DCS_TEAR_MODE_VBLANK);

	s6e3ha2_test_key_on_f0(ctx);
	s6e3ha2_test_key_on_fc(ctx);
	s6e3ha2_touch_hsync_on1(ctx);
	s6e3ha2_pentile_control(ctx);
	s6e3ha2_poc_global(ctx);
	s6e3ha2_poc_setting(ctx);
	s6e3ha2_test_key_off_fc(ctx);

	/* pcd setting off for TB */
	s6e3ha2_pcd_set_off(ctx);
	s6e3ha2_err_fg_set(ctx);
	s6e3ha2_te_start_setting(ctx);


	/* brightness setting */
	s6e3ha2_set_brightness(ctx);
	s6e3ha2_aor_control(ctx);
	s6e3ha2_caps_elvss_set(ctx);
	s6e3ha2_gamma_update(ctx);
	s6e3ha2_acl_off(ctx);
	s6e3ha2_acl_off_opr(ctx);
	s6e3ha2_hbm_off(ctx);

	/* elvss temp compensation */
	s6e3ha2_test_global(ctx);
	s6e3ha2_test(ctx);
	s6e3ha2_test_key_off_f0(ctx);

	s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_ON);
	if (ctx->error != 0)
		return ctx->error;

	ctx->bl_dev->props.power = FB_BLANK_UNBLANK;

	if (ctx->vr_mode)
		s6e3ha2_vr_enable(ctx, 1);

	if (ctx->hmt_mode)
		s6e3ha2_hmt_set(ctx, 1);

	mutex_unlock(&ctx->lock);

	return 0;
}

static int s6e3ha2_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct s6e3ha2 *ctx = panel_to_s6e3ha2(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_create(connector->dev);
	if (!mode) {
		DRM_ERROR("failed to create a new display mode\n");
		return 0;
	}

	drm_display_mode_from_videomode(&ctx->vm, mode);
	mode->vrefresh = 60;
	mode->width_mm = ctx->width_mm;
	mode->height_mm = ctx->height_mm;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs s6e3ha2_drm_funcs = {
	.disable = s6e3ha2_disable,
	.unprepare = s6e3ha2_unprepare,
	.prepare = s6e3ha2_prepare,
	.enable = s6e3ha2_enable,
	.get_modes = s6e3ha2_get_modes,
};

static int s6e3ha2_parse_dt(struct s6e3ha2 *ctx)
{
	struct device *dev = ctx->dev;
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_get_videomode(np, &ctx->vm, 0);
	if (ret < 0)
		return ret;

	switch (ctx->vm.hactive) {
	case 1440:
		ctx->model = MODEL_1440;
		break;
	case 1600:
		ctx->model = MODEL_1600;
		break;
	default:
		dev_err(dev, "Unsupported panel resolution: %dx%d\n",
			ctx->vm.hactive, ctx->vm.vactive);
		return -EINVAL;
	}

	of_property_read_u32(np, "panel-width-mm", &ctx->width_mm);
	of_property_read_u32(np, "panel-height-mm", &ctx->height_mm);

	return 0;
}

static ssize_t s6e3ha2_vr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s6e3ha2 *ctx = dev_get_drvdata(dev);

	sprintf(buf, "%s\n", ctx->vr_mode ? "on" : "off");

	return strlen(buf);
}

static ssize_t s6e3ha2_vr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct s6e3ha2 *ctx = dev_get_drvdata(dev);
	int value;
	int rc;

	mutex_lock(&ctx->lock);

	if (ctx->bl_dev->props.power != FB_BLANK_UNBLANK) {
		dev_err(ctx->dev,
			"panel must be in fb blank unblank state\n");
		goto out;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0) {
		mutex_unlock(&ctx->lock);
		return rc;
	}

	if (value != 0 && value != 1) {
		dev_err(dev, "invalid vr mode.\n");
		goto out;
	}

	if (ctx->vr_mode == value)
		goto out;

	s6e3ha2_vr_enable(ctx, value);

	ctx->vr_mode = value;

out:
	mutex_unlock(&ctx->lock);

	return size;
}

static DEVICE_ATTR(vr, 0664, s6e3ha2_vr_show, s6e3ha2_vr_store);

static void s6e3ha2_hmt_on(struct s6e3ha2 *ctx)
{
	static const u8 ha2_hmt_on[] = {
		NSEQ(0xF2, 0x67, 0x41, 0xC5, 0x0A, 0x06),
		NSEQ(0xB2, 0x03, 0x10, 0x00, 0x0A, 0x0A, 0x00),
		NSEQ(0xF3, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10),
		NSEQ(0x2B, 0x00, 0x00, 0x09, 0xFF),
		NSEQ(0xCB,
		     0x18, 0x11, 0x01, 0x00, 0x00, 0x24, 0x00, 0x00, 0xe1, 0x0B,
		     0x01, 0x00, 0x00, 0x00, 0x02, 0x09, 0x00, 0x15, 0x98, 0x15,
		     0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
		     0x00, 0x12, 0x8c, 0x00, 0x00, 0xca, 0x0a, 0x0a, 0x0a, 0xca,
		     0x8a, 0xca, 0x0a, 0x0a, 0x0a, 0xc0, 0xc1, 0xc4, 0xc5, 0x42,
		     0xc3, 0xca, 0xca, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
		     0x0a, 0x0c),
		0
	};
	static const u8 hf2_hmt_on[] = {
		NSEQ(0xB0, 0x03),
		NSEQ(0xF2, 0x0A, 0x06),
		NSEQ(0xB0, 0x05),
		NSEQ(0xB2, 0x00),
		NSEQ(0xB0, 0x06),
		NSEQ(0xF3, 0x10),
		NSEQ(0x2B, 0x00, 0x00, 0x09, 0xFF),
		NSEQ(0xB0, 0x09),
		NSEQ(0xCB, 0x0B),
		NSEQ(0xB0, 0x0F),
		NSEQ(0xCB, 0x09),
		NSEQ(0xB0, 0x1D),
		NSEQ(0xCB, 0x10),
		NSEQ(0xB0, 0x33),
		NSEQ(0xCB, 0xCC),
		0
	};
	static const u8 *hmt_on[] = { ha2_hmt_on, hf2_hmt_on };

	s6e3ha2_write_nseq(ctx, hmt_on[ctx->model]);
}

static void s6e3ha2_hmt_off(struct s6e3ha2 *ctx)
{
	static const u8 ha2_hmt_off[] = {
		NSEQ(0xF2, 0x67, 0x41, 0xC5, 0x06, 0x0A),
		NSEQ(0xB2, 0x03, 0x10, 0x00, 0x0A, 0x0A, 0x40),
		NSEQ(0xCB,
		     0x18, 0x11, 0x01, 0x00, 0x00, 0x24, 0x00, 0x00, 0xe1, 0x09,
		     0x01, 0x00, 0x00, 0x00, 0x02, 0x05, 0x00, 0x15, 0x98, 0x15,
		     0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		     0x00, 0x12, 0x8c, 0x00, 0x00, 0xca, 0x0a, 0x0a, 0x0a, 0xca,
		     0x8a, 0xca, 0x0a, 0x0a, 0x0a, 0xc0, 0xc1, 0xc4, 0xc5, 0x42,
		     0xc3, 0xca, 0xca, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
		     0x0a, 0x0b),
		0
	};
	static const u8 hf2_hmt_off[] = {
		NSEQ(0xB0, 0x03),
		NSEQ(0xF2, 0x06, 0x0A),
		NSEQ(0xB0, 0x05),
		NSEQ(0xB2, 0x40),
		NSEQ(0xB0, 0x09),
		NSEQ(0xCB, 0x09),
		NSEQ(0xB0, 0x0F),
		NSEQ(0xCB, 0x05),
		NSEQ(0xB0, 0x1D),
		NSEQ(0xCB, 0x00),
		NSEQ(0xB0, 0x33),
		NSEQ(0xCB, 0xCB),
		0
	};
	static const u8 *hmt_off[] = { ha2_hmt_off, hf2_hmt_off };

	s6e3ha2_write_nseq(ctx, hmt_off[ctx->model]);
}

static void s6e3ha2_hmt_set(struct s6e3ha2 *ctx, bool enable)
{
	if (ctx->bl_dev->props.power != FB_BLANK_UNBLANK)
		return;

	ctx->hmt_mode = enable;

	s6e3ha2_test_key_on_f0(ctx);
	s6e3ha2_test_key_on_fc(ctx);

	if (ctx->model != MODEL_1440) {
		s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_SET_TEAR_OFF);
		usleep_range(17000, 20000);
	}

	if (enable)
		s6e3ha2_hmt_on(ctx);
	else
		s6e3ha2_hmt_off(ctx);

	if (ctx->model != MODEL_1440)
		s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_SET_TEAR_ON,
					     MIPI_DSI_DCS_TEAR_MODE_VBLANK);

	s6e3ha2_test_key_off_fc(ctx);
	s6e3ha2_test_key_off_f0(ctx);

	s6e3ha2_set_brightness(ctx);
}

static ssize_t s6e3ha2_hmt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s6e3ha2 *ctx = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", ctx->hmt_mode);

	return strlen(buf);
}

static ssize_t s6e3ha2_hmt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct s6e3ha2 *ctx = dev_get_drvdata(dev);
	int ret, value;

	ret = kstrtoint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (value < 0 || value > 1)
		return -ERANGE;

	mutex_lock(&ctx->lock);

	if (ctx->hmt_mode != value)
		s6e3ha2_hmt_set(ctx, value);

	mutex_unlock(&ctx->lock);

	return size;
}

static DEVICE_ATTR(hmt, 0664, s6e3ha2_hmt_show, s6e3ha2_hmt_store);

static int s6e3ha2_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct s6e3ha2 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(struct s6e3ha2), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	mutex_init(&ctx->lock);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = s6e3ha2_parse_dt(ctx);
	if (ret < 0)
		return ret;

	ctx->supplies[0].supply = "vdd3";
	ctx->supplies[1].supply = "vci";

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "failed to get regulators: %d\n", ret);
		return ret;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset");
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	ret = gpiod_direction_output(ctx->reset_gpio, 1);
	if (ret < 0) {
		dev_err(dev, "cannot configure reset-gpios %d\n", ret);
		return ret;
	}

	ctx->panel_en_gpio = devm_gpiod_get(dev, "panel-en");
	if (IS_ERR(ctx->panel_en_gpio)) {
		dev_err(dev, "cannot get panel-en-gpios %ld\n",
			PTR_ERR(ctx->panel_en_gpio));
		return PTR_ERR(ctx->panel_en_gpio);
	}
	ret = gpiod_direction_output(ctx->panel_en_gpio, 1);
	if (ret < 0) {
		dev_err(dev, "cannot configure panel-en-gpios %d\n", ret);
		return ret;
	}

	ctx->bl_dev = backlight_device_register("s6e3ha2", dev, ctx,
						&s6e3ha2_bl_ops, NULL);
	if (IS_ERR(ctx->bl_dev)) {
		dev_err(dev, "failed to register backlight device\n");
		return PTR_ERR(ctx->bl_dev);
	}

	ret = device_create_file(dev, &dev_attr_vr);
	if (ret) {
		dev_err(dev, "failed to create vr sysfs file.\n");
		goto unregister_backlight;
	}

	ret = device_create_file(dev, &dev_attr_hmt);
	if (ret) {
		dev_err(dev, "failed to create hmt sysfs file.\n");
		goto remove_vr;
	}

	ctx->bl_dev->props.max_brightness = MAX_BRIGHTNESS;
	ctx->bl_dev->props.brightness = DEFAULT_BRIGHTNESS;
	ctx->bl_dev->props.power = FB_BLANK_POWERDOWN;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &s6e3ha2_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		goto remove_hmt;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		goto remove_panel;

	return ret;

remove_panel:
	drm_panel_remove(&ctx->panel);

remove_hmt:
	device_remove_file(dev, &dev_attr_hmt);

remove_vr:
	device_remove_file(dev, &dev_attr_vr);

unregister_backlight:
	backlight_device_unregister(ctx->bl_dev);

	return ret;
}

static int s6e3ha2_remove(struct mipi_dsi_device *dsi)
{
	struct s6e3ha2 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	device_remove_file(ctx->dev, &dev_attr_vr);
	backlight_device_unregister(ctx->bl_dev);

	return 0;
}

static struct of_device_id s6e3ha2_of_match[] = {
	{ .compatible = "samsung,s6e3ha2" },
	{ }
};
MODULE_DEVICE_TABLE(of, s6e3ha2_of_match);

static struct mipi_dsi_driver s6e3ha2_driver = {
	.probe = s6e3ha2_probe,
	.remove = s6e3ha2_remove,
	.driver = {
		.name = "panel_s6e3ha2",
		.owner = THIS_MODULE,
		.of_match_table = s6e3ha2_of_match,
	},
};
module_mipi_dsi_driver(s6e3ha2_driver);

MODULE_AUTHOR("Donghwa Lee <dh09.lee@samsung.com>");
MODULE_AUTHOR("Hyungwon Hwang <human.hwang@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based s6e3ha2 AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
