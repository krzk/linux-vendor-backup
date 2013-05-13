/*
 * Samsung HDMI interface driver
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * Tomasz Stanislawski, <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundiation. either version 2 of the License,
 * or (at your option) any later version
 */

#define pr_fmt(fmt) "s5p-tv (hdmi_drv): " fmt

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_HDMI_DEBUG
#define DEBUG
#endif

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/bug.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include <media/s5p_hdmi.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>

#include "regs-hdmi.h"

MODULE_AUTHOR("Tomasz Stanislawski, <t.stanislaws@samsung.com>");
MODULE_DESCRIPTION("Samsung HDMI");
MODULE_LICENSE("GPL");

/* default preset configured on probe */
#define HDMI_DEFAULT_PRESET V4L2_DV_480P59_94

enum hdmi_type {
	HDMI_TYPE13,
	HDMI_TYPE14,
};

struct hdmi_pulse {
	u32 beg;
	u32 end;
};

struct hdmi_tg_regs {
	u8 cmd;
	u8 h_fsz_l;
	u8 h_fsz_h;
	u8 hact_st_l;
	u8 hact_st_h;
	u8 hact_sz_l;
	u8 hact_sz_h;
	u8 v_fsz_l;
	u8 v_fsz_h;
	u8 vsync_l;
	u8 vsync_h;
	u8 vsync2_l;
	u8 vsync2_h;
	u8 vact_st_l;
	u8 vact_st_h;
	u8 vact_sz_l;
	u8 vact_sz_h;
	u8 field_chg_l;
	u8 field_chg_h;
	u8 vact_st2_l;
	u8 vact_st2_h;
	u8 vact_st3_l;
	u8 vact_st3_h;
	u8 vact_st4_l;
	u8 vact_st4_h;
	u8 vsync_top_hdmi_l;
	u8 vsync_top_hdmi_h;
	u8 vsync_bot_hdmi_l;
	u8 vsync_bot_hdmi_h;
	u8 field_top_hdmi_l;
	u8 field_top_hdmi_h;
	u8 field_bot_hdmi_l;
	u8 field_bot_hdmi_h;
	u8 tg_3d;
};

struct hdmi_v13_core_regs {
	u8 h_blank[2];
	u8 v_blank[3];
	u8 h_v_line[3];
	u8 vsync_pol[1];
	u8 int_pro_mode[1];
	u8 v_blank_f[3];
	u8 h_sync_gen[3];
	u8 v_sync_gen1[3];
	u8 v_sync_gen2[3];
	u8 v_sync_gen3[3];
};

struct hdmi_v14_core_regs {
	u8 h_blank[2];
	u8 v2_blank[2];
	u8 v1_blank[2];
	u8 v_line[2];
	u8 h_line[2];
	u8 hsync_pol[1];
	u8 vsync_pol[1];
	u8 int_pro_mode[1];
	u8 v_blank_f0[2];
	u8 v_blank_f1[2];
	u8 h_sync_start[2];
	u8 h_sync_end[2];
	u8 v_sync_line_bef_2[2];
	u8 v_sync_line_bef_1[2];
	u8 v_sync_line_aft_2[2];
	u8 v_sync_line_aft_1[2];
	u8 v_sync_line_aft_pxl_2[2];
	u8 v_sync_line_aft_pxl_1[2];
	u8 v_blank_f2[2]; /* for 3D mode */
	u8 v_blank_f3[2]; /* for 3D mode */
	u8 v_blank_f4[2]; /* for 3D mode */
	u8 v_blank_f5[2]; /* for 3D mode */
	u8 v_sync_line_aft_3[2];
	u8 v_sync_line_aft_4[2];
	u8 v_sync_line_aft_5[2];
	u8 v_sync_line_aft_6[2];
	u8 v_sync_line_aft_pxl_3[2];
	u8 v_sync_line_aft_pxl_4[2];
	u8 v_sync_line_aft_pxl_5[2];
	u8 v_sync_line_aft_pxl_6[2];
	u8 vact_space_1[2];
	u8 vact_space_2[2];
	u8 vact_space_3[2];
	u8 vact_space_4[2];
	u8 vact_space_5[2];
	u8 vact_space_6[2];
};

struct hdmi_v13_conf {
	struct hdmi_v13_core_regs core;
	struct hdmi_tg_regs tg;
};

struct hdmi_v14_conf {
	struct hdmi_v14_core_regs core;
	struct hdmi_tg_regs tg;
};

struct hdmi_preset_conf {
	u32 preset;
	union {
		const struct hdmi_v13_conf *v13_conf;
		const struct hdmi_v14_conf *v14_conf;
	} conf;
};

static const struct hdmi_v13_conf hdmi_v13_conf_480p = {
	.core = {
		.h_blank = {0x8a, 0x00},
		.v_blank = {0x0d, 0x6a, 0x01},
		.h_v_line = {0x0d, 0xa2, 0x35},
		.vsync_pol = {0x01},
		.int_pro_mode = {0x00},
		.v_blank_f = {0x00, 0x00, 0x00},
		.h_sync_gen = {0x0e, 0x30, 0x11},
		.v_sync_gen1 = {0x0f, 0x90, 0x00},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x5a, 0x03, /* h_fsz */
		0x8a, 0x00, 0xd0, 0x02, /* hact */
		0x0d, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0xe0, 0x01, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_conf hdmi_v13_conf_720p60 = {
	.core = {
		.h_blank = {0x72, 0x01},
		.v_blank = {0xee, 0xf2, 0x00},
		.h_v_line = {0xee, 0x22, 0x67},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f = {0x00, 0x00, 0x00}, /* don't care */
		.h_sync_gen = {0x6c, 0x50, 0x02},
		.v_sync_gen1 = {0x0a, 0x50, 0x00},
		.v_sync_gen2 = {0x01, 0x10, 0x00},
		.v_sync_gen3 = {0x01, 0x10, 0x00},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x72, 0x06, /* h_fsz */
		0x71, 0x01, 0x01, 0x05, /* hact */
		0xee, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x1e, 0x00, 0xd0, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_conf hdmi_v13_conf_1080i50 = {
	.core = {
		.h_blank = {0xd0, 0x02},
		.v_blank = {0x32, 0xB2, 0x00},
		.h_v_line = {0x65, 0x04, 0xa5},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x01},
		.v_blank_f = {0x49, 0x2A, 0x23},
		.h_sync_gen = {0x0E, 0xEA, 0x08},
		.v_sync_gen1 = {0x07, 0x20, 0x00},
		.v_sync_gen2 = {0x39, 0x42, 0x23},
		.v_sync_gen3 = {0x38, 0x87, 0x73},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x50, 0x0A, /* h_fsz */
		0xCF, 0x02, 0x81, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x16, 0x00, 0x1c, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_conf hdmi_v13_conf_1080p50 = {
	.core = {
		.h_blank = {0xd0, 0x02},
		.v_blank = {0x65, 0x6c, 0x01},
		.h_v_line = {0x65, 0x04, 0xa5},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f = {0x00, 0x00, 0x00}, /* don't care */
		.h_sync_gen = {0x0e, 0xea, 0x08},
		.v_sync_gen1 = {0x09, 0x40, 0x00},
		.v_sync_gen2 = {0x01, 0x10, 0x00},
		.v_sync_gen3 = {0x01, 0x10, 0x00},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x50, 0x0A, /* h_fsz */
		0xCF, 0x02, 0x81, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_conf hdmi_v13_conf_1080i60 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v_blank = {0x32, 0xB2, 0x00},
		.h_v_line = {0x65, 0x84, 0x89},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x01},
		.v_blank_f = {0x49, 0x2A, 0x23},
		.h_sync_gen = {0x56, 0x08, 0x02},
		.v_sync_gen1 = {0x07, 0x20, 0x00},
		.v_sync_gen2 = {0x39, 0x42, 0x23},
		.v_sync_gen3 = {0xa4, 0x44, 0x4a},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x17, 0x01, 0x81, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x16, 0x00, 0x1c, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_conf hdmi_v13_conf_1080p60 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v_blank = {0x65, 0x6c, 0x01},
		.h_v_line = {0x65, 0x84, 0x89},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f = {0x00, 0x00, 0x00}, /* don't care */
		.h_sync_gen = {0x56, 0x08, 0x02},
		.v_sync_gen1 = {0x09, 0x40, 0x00},
		.v_sync_gen2 = {0x01, 0x10, 0x00},
		.v_sync_gen3 = {0x01, 0x10, 0x00},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x17, 0x01, 0x81, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static struct hdmi_preset_conf hdmi_v13_timings[] = {
	{ V4L2_DV_480P59_94, .conf.v13_conf = &hdmi_v13_conf_480p },
	{ V4L2_DV_720P60,   .conf.v13_conf = &hdmi_v13_conf_720p60 },
	{ V4L2_DV_1080P50,  .conf.v13_conf = &hdmi_v13_conf_1080p50 },
	{ V4L2_DV_1080I50,  .conf.v13_conf = &hdmi_v13_conf_1080i50 },
	{ V4L2_DV_1080I60,  .conf.v13_conf = &hdmi_v13_conf_1080i60 },
	{ V4L2_DV_1080P60,  .conf.v13_conf = &hdmi_v13_conf_1080p60 },
};

static const struct hdmi_v14_conf hdmi_conf_480p60 = {
	.core = {
		.h_blank = {0x8a, 0x00},
		.v2_blank = {0x0d, 0x02},
		.v1_blank = {0x2d, 0x00},
		.v_line = {0x0d, 0x02},
		.h_line = {0x5a, 0x03},
		.hsync_pol = {0x01},
		.vsync_pol = {0x01},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x0e, 0x00},
		.h_sync_end = {0x4c, 0x00},
		.v_sync_line_bef_2 = {0x0f, 0x00},
		.v_sync_line_bef_1 = {0x09, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x5a, 0x03, /* h_fsz */
		0x8a, 0x00, 0xd0, 0x02, /* hact */
		0x0d, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0xe0, 0x01, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_v14_conf hdmi_conf_720p50 = {
	.core = {
		.h_blank = {0xbc, 0x02},
		.v2_blank = {0xee, 0x02},
		.v1_blank = {0x1e, 0x00},
		.v_line = {0xee, 0x02},
		.h_line = {0xbc, 0x07},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0xb6, 0x01},
		.h_sync_end = {0xde, 0x01},
		.v_sync_line_bef_2 = {0x0a, 0x00},
		.v_sync_line_bef_1 = {0x05, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0xbc, 0x07, /* h_fsz */
		0xbc, 0x02, 0x00, 0x05, /* hact */
		0xee, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x1e, 0x00, 0xd0, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_v14_conf hdmi_conf_720p60 = {
	.core = {
		.h_blank = {0x72, 0x01},
		.v2_blank = {0xee, 0x02},
		.v1_blank = {0x1e, 0x00},
		.v_line = {0xee, 0x02},
		.h_line = {0x72, 0x06},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x6c, 0x00},
		.h_sync_end = {0x94, 0x00},
		.v_sync_line_bef_2 = {0x0a, 0x00},
		.v_sync_line_bef_1 = {0x05, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x72, 0x06, /* h_fsz */
		0x72, 0x01, 0x00, 0x05, /* hact */
		0xee, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x1e, 0x00, 0xd0, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_v14_conf hdmi_conf_1080i50 = {
	.core = {
		.h_blank = {0xd0, 0x02},
		.v2_blank = {0x32, 0x02},
		.v1_blank = {0x16, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x50, 0x0a},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x01},
		.v_blank_f0 = {0x49, 0x02},
		.v_blank_f1 = {0x65, 0x04},
		.h_sync_start = {0x0e, 0x02},
		.h_sync_end = {0x3a, 0x02},
		.v_sync_line_bef_2 = {0x07, 0x00},
		.v_sync_line_bef_1 = {0x02, 0x00},
		.v_sync_line_aft_2 = {0x39, 0x02},
		.v_sync_line_aft_1 = {0x34, 0x02},
		.v_sync_line_aft_pxl_2 = {0x38, 0x07},
		.v_sync_line_aft_pxl_1 = {0x38, 0x07},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x50, 0x0a, /* h_fsz */
		0xd0, 0x02, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x16, 0x00, 0x1c, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_v14_conf hdmi_conf_1080i60 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v2_blank = {0x32, 0x02},
		.v1_blank = {0x16, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x98, 0x08},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x01},
		.v_blank_f0 = {0x49, 0x02},
		.v_blank_f1 = {0x65, 0x04},
		.h_sync_start = {0x56, 0x00},
		.h_sync_end = {0x82, 0x00},
		.v_sync_line_bef_2 = {0x07, 0x00},
		.v_sync_line_bef_1 = {0x02, 0x00},
		.v_sync_line_aft_2 = {0x39, 0x02},
		.v_sync_line_aft_1 = {0x34, 0x02},
		.v_sync_line_aft_pxl_2 = {0xa4, 0x04},
		.v_sync_line_aft_pxl_1 = {0xa4, 0x04},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x18, 0x01, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x16, 0x00, 0x1c, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_v14_conf hdmi_conf_1080p30 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v2_blank = {0x65, 0x04},
		.v1_blank = {0x2d, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x98, 0x08},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x56, 0x00},
		.h_sync_end = {0x82, 0x00},
		.v_sync_line_bef_2 = {0x09, 0x00},
		.v_sync_line_bef_1 = {0x04, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x18, 0x01, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_v14_conf hdmi_conf_1080p50 = {
	.core = {
		.h_blank = {0xd0, 0x02},
		.v2_blank = {0x65, 0x04},
		.v1_blank = {0x2d, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x50, 0x0a},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x0e, 0x02},
		.h_sync_end = {0x3a, 0x02},
		.v_sync_line_bef_2 = {0x09, 0x00},
		.v_sync_line_bef_1 = {0x04, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x50, 0x0a, /* h_fsz */
		0xd0, 0x02, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_v14_conf hdmi_conf_1080p60 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v2_blank = {0x65, 0x04},
		.v1_blank = {0x2d, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x98, 0x08},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x56, 0x00},
		.h_sync_end = {0x82, 0x00},
		.v_sync_line_bef_2 = {0x09, 0x00},
		.v_sync_line_bef_1 = {0x04, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x18, 0x01, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};


static struct hdmi_preset_conf hdmi_v14_timings[] = {
	{ V4L2_DV_480P59_94, .conf.v14_conf = &hdmi_conf_480p60 },
	{ V4L2_DV_720P50,   .conf.v14_conf = &hdmi_conf_720p50 },
	{ V4L2_DV_720P59_94,   .conf.v14_conf = &hdmi_conf_720p60 },
	{ V4L2_DV_1080P60,  .conf.v14_conf = &hdmi_conf_1080p60 },
	{ V4L2_DV_1080P50,  .conf.v14_conf = &hdmi_conf_1080p50 },
	{ V4L2_DV_1080I50,  .conf.v14_conf = &hdmi_conf_1080i50 },
	{ V4L2_DV_1080I50,  .conf.v14_conf = &hdmi_conf_1080i60 },
	{ V4L2_DV_1080P30,  .conf.v14_conf = &hdmi_conf_1080p30 },
};

struct hdmi_resources {
	struct clk *hdmi;
	struct clk *sclk_hdmi;
	struct clk *sclk_pixel;
	struct clk *sclk_hdmiphy;
	struct clk *hdmiphy;
	struct regulator_bulk_data *regul_bulk;
	int regul_count;
};

struct hdmi_device {
	/** base address of HDMI registers */
	void __iomem *regs;
	/** HDMI interrupt */
	unsigned int irq;
	/** pointer to device parent */
	struct device *dev;
	/** subdev generated by HDMI device */
	struct v4l2_subdev sd;
	/** V4L2 device structure */
	struct v4l2_device v4l2_dev;
	/** subdev of HDMIPHY interface */
	struct v4l2_subdev *phy_sd;
	/** subdev of MHL interface */
	struct v4l2_subdev *mhl_sd;
	/** configuration of current graphic mode */
	const struct hdmi_preset_conf *cur_conf;
	/** flag indicating that timings are dirty */
	int cur_conf_dirty;
	/** current preset */
	u32 cur_preset;
	/** hpd gpio number */
	int hpd_gpio;
	/** hdmi version */
	enum hdmi_type type;
	/** other resources */
	struct hdmi_resources res;
};

static struct platform_device_id hdmi_driver_types[] = {
	{
		.name		= "s5pv210-hdmi",
		.driver_data	= HDMI_TYPE13,
	}, {
		.name		= "exynos4-hdmi",
		.driver_data	= HDMI_TYPE13,
	}, {
		/* end node */
	}
};

#ifdef CONFIG_OF
static struct of_device_id hdmi_dt_match[] = {
	{
		.compatible = "samsung,s5pv210-hdmi",
		.data	= (void	*)HDMI_TYPE13,
	}, {
		.compatible = "samsung,exynos4-hdmi",
		.data   = (void *)HDMI_TYPE13,
	}, {
		.compatible = "samsung,exynos5-hdmi",
		.data	= (void	*)HDMI_TYPE14,
	}, {
		/* end node */
	},
};
#endif

static const struct v4l2_subdev_ops hdmi_sd_ops;

static struct hdmi_device *sd_to_hdmi_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct hdmi_device, sd);
}

static inline
void hdmi_write(struct hdmi_device *hdev, u32 reg_id, u32 value)
{
	writel(value, hdev->regs + reg_id);
}

static inline
void hdmi_write_mask(struct hdmi_device *hdev, u32 reg_id, u32 value, u32 mask)
{
	u32 old = readl(hdev->regs + reg_id);
	value = (value & mask) | (old & ~mask);
	writel(value, hdev->regs + reg_id);
}

static inline
void hdmi_writeb(struct hdmi_device *hdev, u32 reg_id, u8 value)
{
	writeb(value, hdev->regs + reg_id);
}

static inline
void hdmi_writebn(struct hdmi_device *hdev, u32 reg_id, int n, u32 value)
{
	switch (n) {
	default:
		writeb(value >> 24, hdev->regs + reg_id + 12);
	case 3:
		writeb(value >> 16, hdev->regs + reg_id + 8);
	case 2:
		writeb(value >>  8, hdev->regs + reg_id + 4);
	case 1:
		writeb(value >>  0, hdev->regs + reg_id + 0);
	}
}

static inline u32 hdmi_read(struct hdmi_device *hdev, u32 reg_id)
{
	return readl(hdev->regs + reg_id);
}

static irqreturn_t hdmi_irq_handler(int irq, void *dev_data)
{
	struct hdmi_device *hdev = dev_data;
	u32 hpd_status;

	hpd_status = gpio_get_value(hdev->hpd_gpio);
	if (hpd_status == 0)
		pr_info("unplugged\n");
	else
		pr_info("plugged\n");

	return IRQ_HANDLED;
}

static void hdmi_reg_init(struct hdmi_device *hdev)
{
	/* enable HPD interrupts */
	hdmi_write_mask(hdev, HDMI_INTC_CON, ~0, HDMI_INTC_EN_GLOBAL |
		HDMI_INTC_EN_HPD_PLUG | HDMI_INTC_EN_HPD_UNPLUG);
	/* choose DVI mode */
	hdmi_write_mask(hdev, HDMI_MODE_SEL,
		HDMI_MODE_DVI_EN, HDMI_MODE_MASK);
	hdmi_write_mask(hdev, HDMI_CON_2, ~0,
		HDMI_DVI_PERAMBLE_EN | HDMI_DVI_BAND_EN);
	/* disable bluescreen */
	hdmi_write_mask(hdev, HDMI_CON_0, 0, HDMI_BLUE_SCR_EN);
	/* choose bluescreen (fecal) color */
	hdmi_writeb(hdev, HDMI_V13_BLUE_SCREEN_0, 0x12);
	hdmi_writeb(hdev, HDMI_V13_BLUE_SCREEN_1, 0x34);
	hdmi_writeb(hdev, HDMI_V13_BLUE_SCREEN_2, 0x56);
}

static void hdmi_v13_timing_apply(struct hdmi_device *hdev,
	const struct hdmi_preset_conf *conf)
{
	const struct hdmi_v13_core_regs *core = &conf->conf.v13_conf->core;
	const struct hdmi_tg_regs *tg = &conf->conf.v13_conf->tg;

	/* setting core registers */
	hdmi_writeb(hdev, HDMI_H_BLANK_0, core->h_blank[0]);
	hdmi_writeb(hdev, HDMI_H_BLANK_1, core->h_blank[1]);
	hdmi_writeb(hdev, HDMI_V13_V_BLANK_0, core->v_blank[0]);
	hdmi_writeb(hdev, HDMI_V13_V_BLANK_1, core->v_blank[1]);
	hdmi_writeb(hdev, HDMI_V13_V_BLANK_2, core->v_blank[2]);
	hdmi_writeb(hdev, HDMI_V13_H_V_LINE_0, core->h_v_line[0]);
	hdmi_writeb(hdev, HDMI_V13_H_V_LINE_1, core->h_v_line[1]);
	hdmi_writeb(hdev, HDMI_V13_H_V_LINE_2, core->h_v_line[2]);
	hdmi_writeb(hdev, HDMI_VSYNC_POL, core->vsync_pol[0]);
	hdmi_writeb(hdev, HDMI_INT_PRO_MODE, core->int_pro_mode[0]);
	hdmi_writeb(hdev, HDMI_V13_V_BLANK_F_0, core->v_blank_f[0]);
	hdmi_writeb(hdev, HDMI_V13_V_BLANK_F_1, core->v_blank_f[1]);
	hdmi_writeb(hdev, HDMI_V13_V_BLANK_F_2, core->v_blank_f[2]);
	hdmi_writeb(hdev, HDMI_V13_H_SYNC_GEN_0, core->h_sync_gen[0]);
	hdmi_writeb(hdev, HDMI_V13_H_SYNC_GEN_1, core->h_sync_gen[1]);
	hdmi_writeb(hdev, HDMI_V13_H_SYNC_GEN_2, core->h_sync_gen[2]);
	hdmi_writeb(hdev, HDMI_V13_V_SYNC_GEN_1_0, core->v_sync_gen1[0]);
	hdmi_writeb(hdev, HDMI_V13_V_SYNC_GEN_1_1, core->v_sync_gen1[1]);
	hdmi_writeb(hdev, HDMI_V13_V_SYNC_GEN_1_2, core->v_sync_gen1[2]);
	hdmi_writeb(hdev, HDMI_V13_V_SYNC_GEN_2_0, core->v_sync_gen2[0]);
	hdmi_writeb(hdev, HDMI_V13_V_SYNC_GEN_2_1, core->v_sync_gen2[1]);
	hdmi_writeb(hdev, HDMI_V13_V_SYNC_GEN_2_2, core->v_sync_gen2[2]);
	hdmi_writeb(hdev, HDMI_V13_V_SYNC_GEN_3_0, core->v_sync_gen3[0]);
	hdmi_writeb(hdev, HDMI_V13_V_SYNC_GEN_3_1, core->v_sync_gen3[1]);
	hdmi_writeb(hdev, HDMI_V13_V_SYNC_GEN_3_2, core->v_sync_gen3[2]);
	/* Timing generator registers */
	hdmi_writeb(hdev, HDMI_TG_H_FSZ_L, tg->h_fsz_l);
	hdmi_writeb(hdev, HDMI_TG_H_FSZ_H, tg->h_fsz_h);
	hdmi_writeb(hdev, HDMI_TG_HACT_ST_L, tg->hact_st_l);
	hdmi_writeb(hdev, HDMI_TG_HACT_ST_H, tg->hact_st_h);
	hdmi_writeb(hdev, HDMI_TG_HACT_SZ_L, tg->hact_sz_l);
	hdmi_writeb(hdev, HDMI_TG_HACT_SZ_H, tg->hact_sz_h);
	hdmi_writeb(hdev, HDMI_TG_V_FSZ_L, tg->v_fsz_l);
	hdmi_writeb(hdev, HDMI_TG_V_FSZ_H, tg->v_fsz_h);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_L, tg->vsync_l);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_H, tg->vsync_h);
	hdmi_writeb(hdev, HDMI_TG_VSYNC2_L, tg->vsync2_l);
	hdmi_writeb(hdev, HDMI_TG_VSYNC2_H, tg->vsync2_h);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST_L, tg->vact_st_l);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST_H, tg->vact_st_h);
	hdmi_writeb(hdev, HDMI_TG_VACT_SZ_L, tg->vact_sz_l);
	hdmi_writeb(hdev, HDMI_TG_VACT_SZ_H, tg->vact_sz_h);
	hdmi_writeb(hdev, HDMI_TG_FIELD_CHG_L, tg->field_chg_l);
	hdmi_writeb(hdev, HDMI_TG_FIELD_CHG_H, tg->field_chg_h);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST2_L, tg->vact_st2_l);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST2_H, tg->vact_st2_h);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_TOP_HDMI_L, tg->vsync_top_hdmi_l);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_TOP_HDMI_H, tg->vsync_top_hdmi_h);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_BOT_HDMI_L, tg->vsync_bot_hdmi_l);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_BOT_HDMI_H, tg->vsync_bot_hdmi_h);
	hdmi_writeb(hdev, HDMI_TG_FIELD_TOP_HDMI_L, tg->field_top_hdmi_l);
	hdmi_writeb(hdev, HDMI_TG_FIELD_TOP_HDMI_H, tg->field_top_hdmi_h);
	hdmi_writeb(hdev, HDMI_TG_FIELD_BOT_HDMI_L, tg->field_bot_hdmi_l);
	hdmi_writeb(hdev, HDMI_TG_FIELD_BOT_HDMI_H, tg->field_bot_hdmi_h);

	clk_disable(hdev->res.sclk_hdmi);
	clk_set_parent(hdev->res.sclk_hdmi, hdev->res.sclk_hdmiphy);
	clk_enable(hdev->res.sclk_hdmi);

	if (core->int_pro_mode[0])
		hdmi_write_mask(hdev, HDMI_TG_CMD, ~0, HDMI_TG_FIELD_EN);
	else
		hdmi_write_mask(hdev, HDMI_TG_CMD, 0, HDMI_TG_FIELD_EN);

#if 0
	/* enable HDMI and timing generator */
	hdmi_write_mask(hdev, HDMI_CON_0, ~0, HDMI_EN);
	if (core->int_pro_mode[0])
		hdmi_write_mask(hdev, HDMI_TG_CMD, ~0, HDMI_TG_EN |
				HDMI_FIELD_EN);
	else
		hdmi_write_mask(hdev, HDMI_TG_CMD, ~0, HDMI_TG_EN);
#endif
}

static void hdmi_v14_timing_apply(struct hdmi_device *hdev,
	const struct hdmi_preset_conf *conf)
{
	const struct hdmi_v14_core_regs *core = &conf->conf.v14_conf->core;
	const struct hdmi_tg_regs *tg = &conf->conf.v14_conf->tg;

	/* setting core registers */
	hdmi_writeb(hdev, HDMI_H_BLANK_0, core->h_blank[0]);
	hdmi_writeb(hdev, HDMI_H_BLANK_1, core->h_blank[1]);
	hdmi_writeb(hdev, HDMI_V2_BLANK_0, core->v2_blank[0]);
	hdmi_writeb(hdev, HDMI_V2_BLANK_1, core->v2_blank[1]);
	hdmi_writeb(hdev, HDMI_V1_BLANK_0, core->v1_blank[0]);
	hdmi_writeb(hdev, HDMI_V1_BLANK_1, core->v1_blank[1]);
	hdmi_writeb(hdev, HDMI_V_LINE_0, core->v_line[0]);
	hdmi_writeb(hdev, HDMI_V_LINE_1, core->v_line[1]);
	hdmi_writeb(hdev, HDMI_H_LINE_0, core->h_line[0]);
	hdmi_writeb(hdev, HDMI_H_LINE_1, core->h_line[1]);
	hdmi_writeb(hdev, HDMI_HSYNC_POL, core->hsync_pol[0]);
	hdmi_writeb(hdev, HDMI_VSYNC_POL, core->vsync_pol[0]);
	hdmi_writeb(hdev, HDMI_INT_PRO_MODE, core->int_pro_mode[0]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F0_0, core->v_blank_f0[0]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F0_1, core->v_blank_f0[1]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F1_0, core->v_blank_f1[0]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F1_1, core->v_blank_f1[1]);
	hdmi_writeb(hdev, HDMI_H_SYNC_START_0, core->h_sync_start[0]);
	hdmi_writeb(hdev, HDMI_H_SYNC_START_1, core->h_sync_start[1]);
	hdmi_writeb(hdev, HDMI_H_SYNC_END_0, core->h_sync_end[0]);
	hdmi_writeb(hdev, HDMI_H_SYNC_END_1, core->h_sync_end[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_BEF_2_0,
			core->v_sync_line_bef_2[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_BEF_2_1,
			core->v_sync_line_bef_2[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_BEF_1_0,
			core->v_sync_line_bef_1[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_BEF_1_1,
			core->v_sync_line_bef_1[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_2_0,
			core->v_sync_line_aft_2[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_2_1,
			core->v_sync_line_aft_2[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_1_0,
			core->v_sync_line_aft_1[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_1_1,
			core->v_sync_line_aft_1[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_2_0,
			core->v_sync_line_aft_pxl_2[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_2_1,
			core->v_sync_line_aft_pxl_2[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_1_0,
			core->v_sync_line_aft_pxl_1[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_1_1,
			core->v_sync_line_aft_pxl_1[1]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F2_0, core->v_blank_f2[0]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F2_1, core->v_blank_f2[1]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F3_0, core->v_blank_f3[0]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F3_1, core->v_blank_f3[1]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F4_0, core->v_blank_f4[0]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F4_1, core->v_blank_f4[1]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F5_0, core->v_blank_f5[0]);
	hdmi_writeb(hdev, HDMI_V_BLANK_F5_1, core->v_blank_f5[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_3_0,
			core->v_sync_line_aft_3[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_3_1,
			core->v_sync_line_aft_3[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_4_0,
			core->v_sync_line_aft_4[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_4_1,
			core->v_sync_line_aft_4[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_5_0,
			core->v_sync_line_aft_5[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_5_1,
			core->v_sync_line_aft_5[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_6_0,
			core->v_sync_line_aft_6[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_6_1,
			core->v_sync_line_aft_6[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_3_0,
			core->v_sync_line_aft_pxl_3[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_3_1,
			core->v_sync_line_aft_pxl_3[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_4_0,
			core->v_sync_line_aft_pxl_4[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_4_1,
			core->v_sync_line_aft_pxl_4[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_5_0,
			core->v_sync_line_aft_pxl_5[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_5_1,
			core->v_sync_line_aft_pxl_5[1]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_6_0,
			core->v_sync_line_aft_pxl_6[0]);
	hdmi_writeb(hdev, HDMI_V_SYNC_LINE_AFT_PXL_6_1,
			core->v_sync_line_aft_pxl_6[1]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_1_0, core->vact_space_1[0]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_1_1, core->vact_space_1[1]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_2_0, core->vact_space_2[0]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_2_1, core->vact_space_2[1]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_3_0, core->vact_space_3[0]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_3_1, core->vact_space_3[1]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_4_0, core->vact_space_4[0]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_4_1, core->vact_space_4[1]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_5_0, core->vact_space_5[0]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_5_1, core->vact_space_5[1]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_6_0, core->vact_space_6[0]);
	hdmi_writeb(hdev, HDMI_VACT_SPACE_6_1, core->vact_space_6[1]);

	/* Timing generator registers */
	hdmi_writeb(hdev, HDMI_TG_H_FSZ_L, tg->h_fsz_l);
	hdmi_writeb(hdev, HDMI_TG_H_FSZ_H, tg->h_fsz_h);
	hdmi_writeb(hdev, HDMI_TG_HACT_ST_L, tg->hact_st_l);
	hdmi_writeb(hdev, HDMI_TG_HACT_ST_H, tg->hact_st_h);
	hdmi_writeb(hdev, HDMI_TG_HACT_SZ_L, tg->hact_sz_l);
	hdmi_writeb(hdev, HDMI_TG_HACT_SZ_H, tg->hact_sz_h);
	hdmi_writeb(hdev, HDMI_TG_V_FSZ_L, tg->v_fsz_l);
	hdmi_writeb(hdev, HDMI_TG_V_FSZ_H, tg->v_fsz_h);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_L, tg->vsync_l);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_H, tg->vsync_h);
	hdmi_writeb(hdev, HDMI_TG_VSYNC2_L, tg->vsync2_l);
	hdmi_writeb(hdev, HDMI_TG_VSYNC2_H, tg->vsync2_h);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST_L, tg->vact_st_l);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST_H, tg->vact_st_h);
	hdmi_writeb(hdev, HDMI_TG_VACT_SZ_L, tg->vact_sz_l);
	hdmi_writeb(hdev, HDMI_TG_VACT_SZ_H, tg->vact_sz_h);
	hdmi_writeb(hdev, HDMI_TG_FIELD_CHG_L, tg->field_chg_l);
	hdmi_writeb(hdev, HDMI_TG_FIELD_CHG_H, tg->field_chg_h);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST2_L, tg->vact_st2_l);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST2_H, tg->vact_st2_h);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST3_L, tg->vact_st3_l);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST3_H, tg->vact_st3_h);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST4_L, tg->vact_st4_l);
	hdmi_writeb(hdev, HDMI_TG_VACT_ST4_H, tg->vact_st4_h);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_TOP_HDMI_L, tg->vsync_top_hdmi_l);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_TOP_HDMI_H, tg->vsync_top_hdmi_h);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_BOT_HDMI_L, tg->vsync_bot_hdmi_l);
	hdmi_writeb(hdev, HDMI_TG_VSYNC_BOT_HDMI_H, tg->vsync_bot_hdmi_h);
	hdmi_writeb(hdev, HDMI_TG_FIELD_TOP_HDMI_L, tg->field_top_hdmi_l);
	hdmi_writeb(hdev, HDMI_TG_FIELD_TOP_HDMI_H, tg->field_top_hdmi_h);
	hdmi_writeb(hdev, HDMI_TG_FIELD_BOT_HDMI_L, tg->field_bot_hdmi_l);
	hdmi_writeb(hdev, HDMI_TG_FIELD_BOT_HDMI_H, tg->field_bot_hdmi_h);
	hdmi_writeb(hdev, HDMI_TG_3D, tg->tg_3d);

	clk_disable(hdev->res.sclk_hdmi);
	clk_set_parent(hdev->res.sclk_hdmi, hdev->res.sclk_hdmiphy);
	clk_enable(hdev->res.sclk_hdmi);

	if (core->int_pro_mode[0])
		hdmi_write_mask(hdev, HDMI_TG_CMD, ~0, HDMI_TG_FIELD_EN);
	else
		hdmi_write_mask(hdev, HDMI_TG_CMD, 0, HDMI_TG_FIELD_EN);

#if 0
	/* enable HDMI and timing generator */
	hdmi_write_mask(hdev, HDMI_CON_0, ~0, HDMI_EN);
	if (core->int_pro_mode[0])
		hdmi_write_mask(hdev, HDMI_TG_CMD, ~0, HDMI_TG_EN |
				HDMI_FIELD_EN);
	else
		hdmi_write_mask(hdev, HDMI_TG_CMD, ~0, HDMI_TG_EN);
#endif
}

static void hdmi_timing_apply(struct hdmi_device *mdev,
	const struct hdmi_preset_conf *conf)
{
	if (mdev->type == HDMI_TYPE13)
		hdmi_v13_timing_apply(mdev, conf);
	else if (mdev->type == HDMI_TYPE14)
		hdmi_v14_timing_apply(mdev, conf);
}

static int hdmi_conf_apply(struct hdmi_device *hdmi_dev)
{
	struct device *dev = hdmi_dev->dev;
	const struct hdmi_preset_conf *conf = hdmi_dev->cur_conf;
	struct v4l2_dv_preset preset;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	/* skip if conf is already synchronized with HW */
	if (!hdmi_dev->cur_conf_dirty)
		return 0;

	/* reset hdmiphy */
	hdmi_write_mask(hdmi_dev, HDMI_PHY_RSTOUT, ~0, HDMI_PHY_SW_RSTOUT);
	mdelay(10);
	hdmi_write_mask(hdmi_dev, HDMI_PHY_RSTOUT,  0, HDMI_PHY_SW_RSTOUT);
	mdelay(10);

	/* configure presets */
	preset.preset = hdmi_dev->cur_preset;
	ret = v4l2_subdev_call(hdmi_dev->phy_sd, video, s_dv_preset, &preset);
	if (ret) {
		dev_err(dev, "failed to set preset (%u)\n", preset.preset);
		return ret;
	}

	/* resetting HDMI core */
	hdmi_write_mask(hdmi_dev, HDMI_CORE_RSTOUT,  0, HDMI_CORE_SW_RSTOUT);
	mdelay(10);
	hdmi_write_mask(hdmi_dev, HDMI_CORE_RSTOUT, ~0, HDMI_CORE_SW_RSTOUT);
	mdelay(10);

	hdmi_reg_init(hdmi_dev);

	/* setting core registers */
	hdmi_timing_apply(hdmi_dev, conf);

	hdmi_dev->cur_conf_dirty = 0;

	return 0;
}

static void hdmi_dumpregs(struct hdmi_device *hdev, char *prefix)
{
#define DUMPREG(reg_id) \
	dev_dbg(hdev->dev, "%s:" #reg_id " = %08x\n", prefix, \
		readl(hdev->regs + reg_id))

	dev_dbg(hdev->dev, "%s: ---- CONTROL REGISTERS ----\n", prefix);
	DUMPREG(HDMI_INTC_FLAG);
	DUMPREG(HDMI_INTC_CON);
	DUMPREG(HDMI_HPD_STATUS);
	DUMPREG(HDMI_PHY_RSTOUT);
	DUMPREG(HDMI_PHY_VPLL);
	DUMPREG(HDMI_PHY_CMU);
	DUMPREG(HDMI_CORE_RSTOUT);

	dev_dbg(hdev->dev, "%s: ---- CORE REGISTERS ----\n", prefix);
	DUMPREG(HDMI_CON_0);
	DUMPREG(HDMI_CON_1);
	DUMPREG(HDMI_CON_2);
	DUMPREG(HDMI_SYS_STATUS);
	DUMPREG(HDMI_V13_PHY_STATUS);
	DUMPREG(HDMI_STATUS_EN);
	DUMPREG(HDMI_HPD);
	DUMPREG(HDMI_MODE_SEL);
	DUMPREG(HDMI_V13_HPD_GEN);
	DUMPREG(HDMI_DC_CONTROL);
	DUMPREG(HDMI_VIDEO_PATTERN_GEN);

	dev_dbg(hdev->dev, "%s: ---- CORE SYNC REGISTERS ----\n", prefix);
	DUMPREG(HDMI_H_BLANK_0);
	DUMPREG(HDMI_H_BLANK_1);
	DUMPREG(HDMI_V13_V_BLANK_0);
	DUMPREG(HDMI_V13_V_BLANK_1);
	DUMPREG(HDMI_V13_V_BLANK_2);
	DUMPREG(HDMI_V13_H_V_LINE_0);
	DUMPREG(HDMI_V13_H_V_LINE_1);
	DUMPREG(HDMI_V13_H_V_LINE_2);
	DUMPREG(HDMI_VSYNC_POL);
	DUMPREG(HDMI_INT_PRO_MODE);
	DUMPREG(HDMI_V13_V_BLANK_F_0);
	DUMPREG(HDMI_V13_V_BLANK_F_1);
	DUMPREG(HDMI_V13_V_BLANK_F_2);
	DUMPREG(HDMI_V13_H_SYNC_GEN_0);
	DUMPREG(HDMI_V13_H_SYNC_GEN_1);
	DUMPREG(HDMI_V13_H_SYNC_GEN_2);
	DUMPREG(HDMI_V13_V_SYNC_GEN_1_0);
	DUMPREG(HDMI_V13_V_SYNC_GEN_1_1);
	DUMPREG(HDMI_V13_V_SYNC_GEN_1_2);
	DUMPREG(HDMI_V13_V_SYNC_GEN_2_0);
	DUMPREG(HDMI_V13_V_SYNC_GEN_2_1);
	DUMPREG(HDMI_V13_V_SYNC_GEN_2_2);
	DUMPREG(HDMI_V13_V_SYNC_GEN_3_0);
	DUMPREG(HDMI_V13_V_SYNC_GEN_3_1);
	DUMPREG(HDMI_V13_V_SYNC_GEN_3_2);

	dev_dbg(hdev->dev, "%s: ---- TG REGISTERS ----\n", prefix);
	DUMPREG(HDMI_TG_CMD);
	DUMPREG(HDMI_TG_H_FSZ_L);
	DUMPREG(HDMI_TG_H_FSZ_H);
	DUMPREG(HDMI_TG_HACT_ST_L);
	DUMPREG(HDMI_TG_HACT_ST_H);
	DUMPREG(HDMI_TG_HACT_SZ_L);
	DUMPREG(HDMI_TG_HACT_SZ_H);
	DUMPREG(HDMI_TG_V_FSZ_L);
	DUMPREG(HDMI_TG_V_FSZ_H);
	DUMPREG(HDMI_TG_VSYNC_L);
	DUMPREG(HDMI_TG_VSYNC_H);
	DUMPREG(HDMI_TG_VSYNC2_L);
	DUMPREG(HDMI_TG_VSYNC2_H);
	DUMPREG(HDMI_TG_VACT_ST_L);
	DUMPREG(HDMI_TG_VACT_ST_H);
	DUMPREG(HDMI_TG_VACT_SZ_L);
	DUMPREG(HDMI_TG_VACT_SZ_H);
	DUMPREG(HDMI_TG_FIELD_CHG_L);
	DUMPREG(HDMI_TG_FIELD_CHG_H);
	DUMPREG(HDMI_TG_VACT_ST2_L);
	DUMPREG(HDMI_TG_VACT_ST2_H);
	DUMPREG(HDMI_TG_VSYNC_TOP_HDMI_L);
	DUMPREG(HDMI_TG_VSYNC_TOP_HDMI_H);
	DUMPREG(HDMI_TG_VSYNC_BOT_HDMI_L);
	DUMPREG(HDMI_TG_VSYNC_BOT_HDMI_H);
	DUMPREG(HDMI_TG_FIELD_TOP_HDMI_L);
	DUMPREG(HDMI_TG_FIELD_TOP_HDMI_H);
	DUMPREG(HDMI_TG_FIELD_BOT_HDMI_L);
	DUMPREG(HDMI_TG_FIELD_BOT_HDMI_H);
#undef DUMPREG
}

static const struct hdmi_preset_conf *hdmi_preset2timings(
		struct hdmi_device *mdev, u32 preset)
{
	struct device *dev = mdev->dev;
	struct hdmi_preset_conf *confs;
	int i, count;

	switch (mdev->type) {
	case HDMI_TYPE13:
		confs = hdmi_v13_timings;
		count = ARRAY_SIZE(hdmi_v13_timings);
		break;
	case HDMI_TYPE14:
		confs = hdmi_v14_timings;
		count = ARRAY_SIZE(hdmi_v14_timings);
		break;
	default:
		dev_err(dev, "invalid hdmi type(%d).\n", mdev->type);
		return NULL;
	}
	for (i = 0; i < count; ++i)
		if (confs[i].preset == preset)
			return  &confs[i];
	return NULL;
}

static int hdmi_streamon(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;
	struct hdmi_resources *res = &hdev->res;
	int ret, tries;

	dev_dbg(dev, "%s\n", __func__);

	ret = hdmi_conf_apply(hdev);
	if (ret)
		return ret;

	ret = v4l2_subdev_call(hdev->phy_sd, video, s_stream, 1);
	if (ret)
		return ret;

	/* waiting for HDMIPHY's PLL to get to steady state */
	for (tries = 100; tries; --tries) {
		u32 val = hdmi_read(hdev, HDMI_PHY_STATUS_0);
		if (val & HDMI_PHY_STATUS_READY)
			break;
		mdelay(1);
	}
	/* steady state not achieved */
	if (tries == 0) {
		dev_err(dev, "hdmiphy's pll could not reach steady state.\n");
		v4l2_subdev_call(hdev->phy_sd, video, s_stream, 0);
		hdmi_dumpregs(hdev, "hdmiphy - s_stream");
		return -EIO;
	}

	/* starting MHL */
	ret = v4l2_subdev_call(hdev->mhl_sd, video, s_stream, 1);
	if (hdev->mhl_sd && ret) {
		v4l2_subdev_call(hdev->phy_sd, video, s_stream, 0);
		hdmi_dumpregs(hdev, "mhl - s_stream");
		return -EIO;
	}

	/* hdmiphy clock is used for HDMI in streaming mode */
	clk_disable(res->sclk_hdmi);
	clk_set_parent(res->sclk_hdmi, res->sclk_hdmiphy);
	clk_enable(res->sclk_hdmi);

	/* enable HDMI and timing generator */
	hdmi_write_mask(hdev, HDMI_CON_0, ~0, HDMI_EN);
	hdmi_write_mask(hdev, HDMI_TG_CMD, ~0, HDMI_TG_EN);
	hdmi_dumpregs(hdev, "streamon");
	return 0;
}

static int hdmi_streamoff(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;
	struct hdmi_resources *res = &hdev->res;

	dev_dbg(dev, "%s\n", __func__);

	hdmi_write_mask(hdev, HDMI_CON_0, 0, HDMI_EN);
	hdmi_write_mask(hdev, HDMI_TG_CMD, 0, HDMI_TG_EN);

	/* pixel(vpll) clock is used for HDMI in config mode */
	clk_disable(res->sclk_hdmi);
	clk_set_parent(res->sclk_hdmi, res->sclk_pixel);
	clk_enable(res->sclk_hdmi);

	v4l2_subdev_call(hdev->mhl_sd, video, s_stream, 0);
	v4l2_subdev_call(hdev->phy_sd, video, s_stream, 0);

	hdmi_dumpregs(hdev, "streamoff");
	return 0;
}

static int hdmi_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;

	dev_dbg(dev, "%s(%d)\n", __func__, enable);
	if (enable)
		return hdmi_streamon(hdev);
	return hdmi_streamoff(hdev);
}

static void hdmi_resource_poweron(struct hdmi_resources *res)
{
	/* turn HDMI power on */
	regulator_bulk_enable(res->regul_count, res->regul_bulk);
	/* power-on hdmi physical interface */
	clk_enable(res->hdmiphy);
	/* use VPP as parent clock; HDMIPHY is not working yet */
	clk_set_parent(res->sclk_hdmi, res->sclk_pixel);
	/* turn clocks on */
	clk_enable(res->sclk_hdmi);
}

static void hdmi_resource_poweroff(struct hdmi_resources *res)
{
	/* turn clocks off */
	clk_disable(res->sclk_hdmi);
	/* power-off hdmiphy */
	clk_disable(res->hdmiphy);
	/* turn HDMI power off */
	regulator_bulk_disable(res->regul_count, res->regul_bulk);
}

static int hdmi_s_power(struct v4l2_subdev *sd, int on)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	int ret;

	if (on)
		ret = pm_runtime_get_sync(hdev->dev);
	else
		ret = pm_runtime_put_sync(hdev->dev);
	/* only values < 0 indicate errors */
	return IS_ERR_VALUE(ret) ? ret : 0;
}

static int hdmi_s_dv_preset(struct v4l2_subdev *sd,
	struct v4l2_dv_preset *preset)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;
	const struct hdmi_preset_conf *conf;

	conf = hdmi_preset2timings(sd_to_hdmi_dev(sd), preset->preset);
	if (conf == NULL) {
		dev_err(dev, "preset (%u) not supported\n", preset->preset);
		return -EINVAL;
	}
	hdev->cur_conf = conf;
	hdev->cur_conf_dirty = 1;
	hdev->cur_preset = preset->preset;
	return 0;
}

static int hdmi_g_dv_preset(struct v4l2_subdev *sd,
	struct v4l2_dv_preset *preset)
{
	memset(preset, 0, sizeof(*preset));
	preset->preset = sd_to_hdmi_dev(sd)->cur_preset;
	return 0;
}

static int hdmi_g_mbus_fmt(struct v4l2_subdev *sd,
	  struct v4l2_mbus_framefmt *fmt)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	const struct hdmi_tg_regs *tg;
	const struct hdmi_v13_core_regs *v13_core;
	const struct hdmi_v14_core_regs *v14_core;
	int is_interlaced;

	dev_dbg(hdev->dev, "%s\n", __func__);
	if (!hdev->cur_conf)
		return -EINVAL;
	memset(fmt, 0, sizeof(*fmt));

	if (hdev->type == HDMI_TYPE13) {
		tg = &hdev->cur_conf->conf.v13_conf->tg;
		v13_core = &hdev->cur_conf->conf.v13_conf->core;
		is_interlaced = v13_core->int_pro_mode[0];
	} else if (hdev->type == HDMI_TYPE14) {
		tg = &hdev->cur_conf->conf.v14_conf->tg;
		v14_core = &hdev->cur_conf->conf.v14_conf->core;
		is_interlaced = v14_core->int_pro_mode[0];
	} else {
		return -EINVAL;
	}
	fmt->width = (tg->hact_sz_h << 8) | tg->hact_sz_l;
	fmt->height = (tg->vact_sz_h << 8) | tg->vact_sz_l;
	fmt->code = V4L2_MBUS_FMT_FIXED; /* means RGB888 */
	fmt->colorspace = V4L2_COLORSPACE_SRGB;

	if (is_interlaced) {
		fmt->field = V4L2_FIELD_INTERLACED;
		fmt->height *= 2;
	} else {
		fmt->field = V4L2_FIELD_NONE;
	}
	return 0;
}

static int hdmi_enum_dv_presets(struct v4l2_subdev *sd,
	struct v4l2_dv_enum_preset *preset)
{
	struct hdmi_preset_conf *confs;
	struct hdmi_device *mdev = sd_to_hdmi_dev(sd);
	int count;

	switch (mdev->type) {
	case HDMI_TYPE13:
		confs = hdmi_v13_timings;
		count = ARRAY_SIZE(hdmi_v13_timings);
		break;
	case HDMI_TYPE14:
		confs = hdmi_v14_timings;
		count = ARRAY_SIZE(hdmi_v14_timings);
		break;
	default:
		dev_err(mdev->dev, "invalid hdmi type(%d).\n", mdev->type);
		return -EINVAL;
	}

	if (preset->index >= count)
		return -EINVAL;
	return v4l_fill_dv_preset_info(confs[preset->index].preset, preset);
}

#ifdef CONFIG_OF
/* Heavily based[1] on v4l2_i2c_new_subdev_board()
 *
 * [1] Copy-pasted, that is
 */
struct v4l2_subdev *hdmi_of_get_i2c_subdev(struct v4l2_device *v4l2_dev,
	struct device_node *np, const char *propname)
{
	struct v4l2_subdev *sd = NULL;
	struct i2c_client *client;
	struct device_node *cnp;

	BUG_ON(!v4l2_dev);

	cnp = of_parse_phandle(np, propname, 0);
	if (!cnp) {
		dev_err(v4l2_dev->dev, "Can't find subdev %s\n", propname);
		goto err;
	}

	client = of_find_i2c_device_by_node(cnp);
	if (!client) {
		dev_err(v4l2_dev->dev, "subdev %s doesn't reference correct node\n",
			propname);
		goto err;
	}

	if (client == NULL || client->driver == NULL)
		goto err;

	/* Lock the module so we can safely get the v4l2_subdev pointer */
	if (!try_module_get(client->driver->driver.owner))
		goto err;
	sd = i2c_get_clientdata(client);

	/* Register with the v4l2_device which increases the module's
	   use count as well. */
	if (v4l2_device_register_subdev(v4l2_dev, sd)) {
		dev_err(v4l2_dev->dev,
			"%s: failed to register subdev\n", __func__);
		sd = NULL;
	}
	/* Decrease the module use count to match the first try_module_get. */
	module_put(client->driver->driver.owner);
err:
	of_node_put(cnp);

	return sd;
}
#endif

static const struct v4l2_subdev_core_ops hdmi_sd_core_ops = {
	.s_power = hdmi_s_power,
};

static const struct v4l2_subdev_video_ops hdmi_sd_video_ops = {
	.s_dv_preset = hdmi_s_dv_preset,
	.g_dv_preset = hdmi_g_dv_preset,
	.enum_dv_presets = hdmi_enum_dv_presets,
	.g_mbus_fmt = hdmi_g_mbus_fmt,
	.s_stream = hdmi_s_stream,
};

static const struct v4l2_subdev_ops hdmi_sd_ops = {
	.core = &hdmi_sd_core_ops,
	.video = &hdmi_sd_video_ops,
};

static int hdmi_runtime_suspend(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);

	dev_dbg(dev, "%s\n", __func__);
	v4l2_subdev_call(hdev->mhl_sd, core, s_power, 0);
	hdmi_resource_poweroff(&hdev->res);
	/* flag that device context is lost */
	hdev->cur_conf_dirty = 1;
	return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	int ret = 0;

	dev_dbg(dev, "%s\n", __func__);

	hdmi_resource_poweron(&hdev->res);

	/* starting MHL */
	ret = v4l2_subdev_call(hdev->mhl_sd, core, s_power, 1);
	if (hdev->mhl_sd && ret)
		goto fail;

	dev_dbg(dev, "poweron succeed\n");

	return 0;

fail:
	hdmi_resource_poweroff(&hdev->res);
	dev_err(dev, "poweron failed\n");

	return ret;
}

static const struct dev_pm_ops hdmi_pm_ops = {
	.runtime_suspend = hdmi_runtime_suspend,
	.runtime_resume	 = hdmi_runtime_resume,
};

static void hdmi_resources_cleanup(struct hdmi_device *hdev)
{
	struct hdmi_resources *res = &hdev->res;

	dev_dbg(hdev->dev, "HDMI resource cleanup\n");
	/* put clocks, power */
	if (res->regul_count)
		regulator_bulk_free(res->regul_count, res->regul_bulk);
	/* kfree is NULL-safe */
	kfree(res->regul_bulk);
	if (!IS_ERR_OR_NULL(res->hdmiphy))
		clk_put(res->hdmiphy);
	if (!IS_ERR_OR_NULL(res->sclk_hdmiphy))
		clk_put(res->sclk_hdmiphy);
	if (!IS_ERR_OR_NULL(res->sclk_pixel))
		clk_put(res->sclk_pixel);
	if (!IS_ERR_OR_NULL(res->sclk_hdmi))
		clk_put(res->sclk_hdmi);
	if (!IS_ERR_OR_NULL(res->hdmi))
		clk_put(res->hdmi);
	memset(res, 0, sizeof(*res));
}

static int hdmi_resources_init(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;
	struct hdmi_resources *res = &hdev->res;
	static char *supply[] = {
		"hdmi-en",
		"vdd",
		"vdd_osc",
		"vdd_pll",
	};
	int i, ret;

	dev_dbg(dev, "HDMI resource init\n");

	memset(res, 0, sizeof(*res));
	/* get clocks, power */

	res->hdmi = clk_get(dev, "hdmi");
	if (IS_ERR(res->hdmi)) {
		dev_err(dev, "failed to get clock 'hdmi'\n");
		goto fail;
	}
	res->sclk_hdmi = clk_get(dev, "sclk_hdmi");
	if (IS_ERR(res->sclk_hdmi)) {
		dev_err(dev, "failed to get clock 'sclk_hdmi'\n");
		goto fail;
	}
	res->sclk_pixel = clk_get(dev, "sclk_pixel");
	if (IS_ERR(res->sclk_pixel)) {
		dev_err(dev, "failed to get clock 'sclk_pixel'\n");
		goto fail;
	}
	res->sclk_hdmiphy = clk_get(dev, "sclk_hdmiphy");
	if (IS_ERR(res->sclk_hdmiphy)) {
		dev_err(dev, "failed to get clock 'sclk_hdmiphy'\n");
		goto fail;
	}
	res->hdmiphy = clk_get(dev, "hdmiphy");
	if (IS_ERR(res->hdmiphy)) {
		dev_err(dev, "failed to get clock 'hdmiphy'\n");
		goto fail;
	}
	res->regul_bulk = kcalloc(ARRAY_SIZE(supply),
				  sizeof(res->regul_bulk[0]), GFP_KERNEL);
	if (!res->regul_bulk) {
		dev_err(dev, "failed to get memory for regulators\n");
		goto fail;
	}
	for (i = 0; i < ARRAY_SIZE(supply); ++i) {
		res->regul_bulk[i].supply = supply[i];
		res->regul_bulk[i].consumer = NULL;
	}

	ret = regulator_bulk_get(dev, ARRAY_SIZE(supply), res->regul_bulk);
	if (ret) {
		dev_err(dev, "failed to get regulators\n");
		goto fail;
	}
	res->regul_count = ARRAY_SIZE(supply);

	return 0;
fail:
	dev_err(dev, "HDMI resource init - failed\n");
	hdmi_resources_cleanup(hdev);
	return -ENODEV;
}

static struct v4l2_subdev *hdmi_get_subdev(
	struct hdmi_device *hdmi_dev,
	struct i2c_board_info *bdinfo,
	int bus,
	const char *propname)
{
	struct v4l2_subdev *sd = NULL;
	struct i2c_adapter *adapter;
	struct device *dev = hdmi_dev->dev;

#ifdef CONFIG_OF
	if (dev->of_node)
		return hdmi_of_get_i2c_subdev(&hdmi_dev->v4l2_dev,
					   dev->of_node, propname);
#endif

	if (!bdinfo) {
		dev_err(dev, "%s info is missing in platform data\n",
			propname);
		return ERR_PTR(-ENXIO);
	}

	adapter = i2c_get_adapter(bus);
	if (adapter == NULL) {
		dev_err(dev, "%s adapter request failed, name\n",
			propname);
		return ERR_PTR(-ENXIO);
	}

	sd = v4l2_i2c_new_subdev_board(&hdmi_dev->v4l2_dev,
				       adapter, bdinfo, NULL);

	/* on failure or not adapter is no longer useful */
	i2c_put_adapter(adapter);

	if (sd == NULL) {
		dev_err(dev, "missing subdev for %s\n", propname);
		return ERR_PTR(-ENODEV);
	}

	return sd;
}

static int hdmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct v4l2_subdev *sd;
	struct hdmi_device *hdmi_dev = NULL;
	struct s5p_hdmi_platform_data *pdata = dev->platform_data;
	int ret;

	dev_dbg(dev, "probe start\n");

	if (!pdata && !dev->of_node) {
		dev_err(dev, "platform data is missing\n");
		ret = -ENODEV;
		goto fail;
	}

	hdmi_dev = devm_kzalloc(&pdev->dev, sizeof(*hdmi_dev), GFP_KERNEL);
	if (!hdmi_dev) {
		dev_err(dev, "out of memory\n");
		ret = -ENOMEM;
		goto fail;
	}

	hdmi_dev->dev = dev;

	ret = hdmi_resources_init(hdmi_dev);
	if (ret)
		goto fail;

	/* mapping HDMI registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "get memory resource failed.\n");
		ret = -ENXIO;
		goto fail_init;
	}

	hdmi_dev->regs = devm_ioremap(&pdev->dev, res->start,
				      resource_size(res));
	if (hdmi_dev->regs == NULL) {
		dev_err(dev, "register mapping failed.\n");
		ret = -ENXIO;
		goto fail_init;
	}

	if (pdev->dev.of_node) {
		if (!of_find_property(pdev->dev.of_node, "hpd-gpio", NULL)) {
			dev_err(dev, "no hpd gpio property found\n");
			goto fail_init;
		}

		hdmi_dev->hpd_gpio = of_get_named_gpio_flags(pdev->dev.of_node,
				"hpd-gpio", 0, NULL);
	} else {
		hdmi_dev->hpd_gpio = pdata->hpd_gpio;
	}

	ret = devm_gpio_request(&pdev->dev, hdmi_dev->hpd_gpio, "HPD");
	if (ret) {
		dev_err(dev, "failed to request HPD gpio\n");
		return ret;
	}

	res->start = gpio_to_irq(hdmi_dev->hpd_gpio);
	if (res->start < 0) {
		dev_err(dev, "failed to get GPIO irq\n");
		return ret;
	}

	ret = devm_request_irq(&pdev->dev, res->start, hdmi_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT, "hdmi", hdmi_dev);
	if (ret) {
		dev_err(dev, "request interrupt failed.\n");
		goto fail_init;
	}
	hdmi_dev->irq = res->start;

	/* setting v4l2 name to prevent WARN_ON in v4l2_device_register */
	strlcpy(hdmi_dev->v4l2_dev.name, dev_name(dev),
		sizeof(hdmi_dev->v4l2_dev.name));
	/* passing NULL owner prevents driver from erasing drvdata */
	ret = v4l2_device_register(NULL, &hdmi_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "could not register v4l2 device.\n");
		goto fail_init;
	}

	hdmi_dev->phy_sd = hdmi_get_subdev(hdmi_dev,
					   pdata ? pdata->hdmiphy_info : NULL,
					   pdata ? pdata->hdmiphy_bus : -1,
					   "phy");
	if (IS_ERR_OR_NULL(hdmi_dev->phy_sd)) {
		ret = PTR_ERR(hdmi_dev->phy_sd);
		goto fail_vdev;
	}

	if (pdata && pdata->mhl_info) {
		hdmi_dev->mhl_sd = hdmi_get_subdev(hdmi_dev,
				pdata ? pdata->mhl_info : NULL ,
				pdata ? pdata->mhl_bus : -1,
				"mhl");
		if (IS_ERR_OR_NULL(hdmi_dev->mhl_sd)) {
			ret = PTR_ERR(hdmi_dev->mhl_sd);
			goto fail_vdev;
		}
	}

	clk_enable(hdmi_dev->res.hdmi);

	pm_runtime_enable(dev);

	sd = &hdmi_dev->sd;
	v4l2_subdev_init(sd, &hdmi_sd_ops);
	sd->owner = THIS_MODULE;

	if (dev->of_node) {
		const struct of_device_id *match;
		match = of_match_node(of_match_ptr(hdmi_dt_match),
					pdev->dev.of_node);
		if (match == NULL)
			return -ENODEV;
		hdmi_dev->type = (enum hdmi_type)match->data;
	} else {
		hdmi_dev->type = (enum hdmi_type)platform_get_device_id
					(pdev)->driver_data;
	}

	strlcpy(sd->name, "s5p-hdmi", sizeof(sd->name));
	hdmi_dev->cur_preset = HDMI_DEFAULT_PRESET;
	/* FIXME: missing fail preset is not supported */
	hdmi_dev->cur_conf = hdmi_preset2timings(hdmi_dev,
			hdmi_dev->cur_preset);
	hdmi_dev->cur_conf_dirty = 1;

	/* storing subdev for call that have only access to struct device */
	dev_set_drvdata(dev, sd);

	dev_info(dev, "probe successful\n");

	return 0;

fail_vdev:
	v4l2_device_unregister(&hdmi_dev->v4l2_dev);

fail_init:
	hdmi_resources_cleanup(hdmi_dev);

fail:
	dev_err(dev, "probe failed\n");
	return ret;
}

static int hdmi_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hdmi_device *hdmi_dev = sd_to_hdmi_dev(sd);

	pm_runtime_disable(dev);
	clk_disable(hdmi_dev->res.hdmi);
	v4l2_device_unregister(&hdmi_dev->v4l2_dev);
	disable_irq(hdmi_dev->irq);
	hdmi_resources_cleanup(hdmi_dev);
	dev_info(dev, "remove successful\n");

	return 0;
}


static struct platform_driver hdmi_driver __refdata = {
	.probe = hdmi_probe,
	.remove = hdmi_remove,
	.id_table = hdmi_driver_types,
	.driver = {
		.name = "s5p-hdmi",
		.owner = THIS_MODULE,
		.pm = &hdmi_pm_ops,
		.of_match_table = of_match_ptr(hdmi_dt_match),
	}
};

module_platform_driver(hdmi_driver);
