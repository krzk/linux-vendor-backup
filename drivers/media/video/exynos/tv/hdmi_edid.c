/*
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/module.h>

#include "hdmi.h"

#define EDID_SEGMENT_ADDR	(0x60 >> 1)
#define EDID_ADDR		(0xA0 >> 1)
#define EDID_SEGMENT_IGNORE	(2)
#define EDID_BLOCK_SIZE		128
#define EDID_SEGMENT(x)		((x) >> 1)
#define EDID_OFFSET(x)		(((x) & 1) * EDID_BLOCK_SIZE)
#define EDID_EXTENSION_FLAG	0x7E
#define EDID_NATIVE_FORMAT	0x83
#define EDID_BASIC_AUDIO	(1 << 6)

static struct i2c_client *edid_client;

static struct edid_preset {
	u32 preset;
	u16 xres;
	u16 yres;
	u16 refresh;
	char *name;
	bool supported;
} edid_presets[] = {
	{ V4L2_DV_480P59_94,  720,  480,  59, "480p@59.94" },
	{ V4L2_DV_576P50,     720,  576,  50, "576p@50" },
	{ V4L2_DV_720P24,     1280, 720,  24, "720p@24" },
	{ V4L2_DV_720P25,     1280, 720,  25, "720p@25" },
	{ V4L2_DV_720P30,     1280, 720,  30, "720p@30" },
	{ V4L2_DV_720P50,     1280, 720,  50, "720p@50" },
	{ V4L2_DV_720P59_94,  1280, 720,  59, "720p@59.94" },
	{ V4L2_DV_720P60,     1280, 720,  60, "720p@60" },
	{ V4L2_DV_1080P24,    1920, 1080, 24, "1080p@24" },
	{ V4L2_DV_1080P25,    1920, 1080, 25, "1080p@25" },
	{ V4L2_DV_1080P30,    1920, 1080, 30, "1080p@30" },
};

static u32 preferred_preset = HDMI_DEFAULT_PRESET;
static u32 edid_misc;
static int max_audio_channels;
static int audio_bit_rates;
static int audio_sample_rates;
static u32 source_phy_addr = 0;

#ifdef CONFIG_VIDEO_MHL_SII8246
int hdmi_forced_resolution = -1;
#endif

static int edid_i2c_read(struct hdmi_device *hdev, u8 segment, u8 offset,
						   u8 *buf, size_t len)
{
	struct device *dev = hdev->dev;
	/*struct platform_device *pdev = to_platform_device(dev);*/
	/*struct s5p_hdmi_platdata *pdata = pdev->dev.platform_data;*/
	struct i2c_client *i2c = edid_client;
	int cnt = 0;
	int ret;
	struct i2c_msg msg[] = {
		{
			.addr = EDID_SEGMENT_ADDR,
			.flags = segment ? 0 : I2C_M_IGNORE_NAK,
			.len = 1,
			.buf = &segment
		},
		{
			.addr = EDID_ADDR,
			.flags = 0,
			.len = 1,
			.buf = &offset
		},
		{
			.addr = EDID_ADDR,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf
		}
	};

	if (!i2c)
		return -ENODEV;

	do {
		/*
		 * If the HS-I2C is used for DDC(EDID),
		 * shouldn't write the segment pointer.
		 */
		/*
		 *if (is_ip_ver_5r) {
		 *	ret = i2c_transfer(i2c->adapter, &msg[1],
		 *				EDID_SEGMENT_IGNORE);
		 *	if (ret == EDID_SEGMENT_IGNORE)
		 *		break;
		 *} else
		 */
			ret = i2c_transfer(i2c->adapter, msg, ARRAY_SIZE(msg));
			if (ret == ARRAY_SIZE(msg))
				break;

		dev_dbg(dev, "%s: can't read data, retry %d\n", __func__, cnt);
		msleep(25);
		cnt++;
	} while (cnt < 5);

	if (cnt == 5) {
		dev_err(dev, "%s: can't read data, timeout\n", __func__);
		return -ETIME;
	}

	return 0;
}

static int
edid_read_block(struct hdmi_device *hdev, int block, u8 *buf, size_t len)
{
	struct device *dev = hdev->dev;
	int ret, i;
	u8 segment = EDID_SEGMENT(block);
	u8 offset = EDID_OFFSET(block);
	u8 sum = 0;

	if (len < EDID_BLOCK_SIZE)
		return -EINVAL;

	ret = edid_i2c_read(hdev, segment, offset, buf, EDID_BLOCK_SIZE);
	if (ret)
		return ret;

	for (i = 0; i < EDID_BLOCK_SIZE; i++)
		sum += buf[i];

	if (sum) {
		dev_err(dev, "%s: checksum error block=%d sum=%d\n", __func__,
								  block, sum);
		return -EPROTO;
	}

	return 0;
}

static int edid_read(struct hdmi_device *hdev, u8 **data)
{
	u8 block0[EDID_BLOCK_SIZE];
	u8 *edid;
	int block = 0;
	int block_cnt, ret;

	ret = edid_read_block(hdev, 0, block0, sizeof(block0));
	if (ret)
		return ret;

	block_cnt = block0[EDID_EXTENSION_FLAG] + 1;

	edid = kmalloc(block_cnt * EDID_BLOCK_SIZE, GFP_KERNEL);
	if (!edid)
		return -ENOMEM;

	memcpy(edid, block0, sizeof(block0));
	while (++block < block_cnt) {
		ret = edid_read_block(hdev, block,
				edid + block * EDID_BLOCK_SIZE,
					       EDID_BLOCK_SIZE);
		if (ret) {
			kfree(edid);
			return ret;
		}
	}

	*data = edid;
	return block_cnt;
}

static struct edid_preset *edid_find_preset(struct fb_videomode *mode)
{
	struct edid_preset *preset = edid_presets;
	int i;

	if (mode->vmode & FB_VMODE_INTERLACED)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(edid_presets); i++, preset++) {
		if (mode->refresh == preset->refresh &&
			mode->xres	== preset->xres &&
		    mode->yres    == preset->yres) {
			return preset;
		}
	}

	return NULL;
}

static void edid_use_default_preset(void)
{
	int i;

	preferred_preset = HDMI_DEFAULT_PRESET;
	for (i = 0; i < ARRAY_SIZE(edid_presets); i++)
		edid_presets[i].supported =
			(edid_presets[i].preset == preferred_preset);
	max_audio_channels = 2;
}

int edid_update(struct hdmi_device *hdev)
{
	struct fb_monspecs specs;
	struct edid_preset *preset;
#ifdef CONFIG_VIDEO_MHL_SII8246
	u32 pre = V4L2_DV_INVALID;
#endif
	bool first = true;
	u8 *edid = NULL;
	int channels_max = 0, support_bit_rates = 0, support_sample_rates = 0;
	int basic_audio = 0;
	int block_cnt = 0;
	int ret = 0;
	int i;

	edid_misc = 0;

	block_cnt = edid_read(hdev, &edid);
	if (block_cnt < 0)
		goto out;
	else if (block_cnt > 1)
		basic_audio = edid[EDID_NATIVE_FORMAT] & EDID_BASIC_AUDIO;

	print_hex_dump_bytes("EDID: ", DUMP_PREFIX_OFFSET, edid,
						block_cnt * EDID_BLOCK_SIZE);

	fb_edid_to_monspecs(edid, &specs);
	for (i = 1; i < block_cnt; i++) {
		ret = fb_edid_add_monspecs(edid + i * EDID_BLOCK_SIZE, &specs);
		if (ret < 0)
			goto out;
	}

	preferred_preset = V4L2_DV_INVALID;
	for (i = 0; i < ARRAY_SIZE(edid_presets); i++)
		edid_presets[i].supported = false;

	/* find 2D preset */
	for (i = 0; i < specs.modedb_len; i++) {
		preset = edid_find_preset(&specs.modedb[i]);
		if (preset) {
			if (preset->supported == false) {
				pr_info("EDID: found %s", preset->name);
				preset->supported = true;
			}
#ifdef CONFIG_VIDEO_MHL_SII8246
			if (preset->supported) {
				first = false;
				if (pre < (preset->preset)) {
					preferred_preset = preset->preset;
					pre = preset->preset;
					pr_info("EDID: set %s", preset->name);
				}
			}
#else
			if (first) {
				preferred_preset = preset->preset;
				first = false;
			}
#endif
		}
	}


	edid_misc = specs.misc;
	pr_info("EDID: misc flags %08x", edid_misc);

	for (i = 0; i < specs.audiodb_len; i++) {
		if (specs.audiodb[i].format != FB_AUDIO_LPCM)
			continue;
		if (specs.audiodb[i].channel_count > channels_max) {
			channels_max = specs.audiodb[i].channel_count;
			support_sample_rates = specs.audiodb[i].sample_rates;
			support_bit_rates = specs.audiodb[i].bit_rates;
		}
	}

	if (edid_misc & FB_MISC_HDMI) {
		if (channels_max) {
			max_audio_channels = channels_max;
			audio_sample_rates = support_sample_rates;
			audio_bit_rates = support_bit_rates;
		} else {
			if (basic_audio) {
			max_audio_channels = 2;
				audio_sample_rates = FB_AUDIO_48KHZ; /*default audio info*/
				audio_bit_rates = FB_AUDIO_16BIT;
	} else {
		max_audio_channels = 0;
				audio_sample_rates = 0;
				audio_bit_rates = 0;
			}		
		}
	} else {
		max_audio_channels = 0;
		audio_sample_rates = 0;
		audio_bit_rates = 0;
	}
	pr_info("EDID: Audio channels %d", max_audio_channels);

	fb_destroy_modedb(specs.modedb);
	fb_destroy_audiodb(specs.audiodb);
	fb_destroy_videodb(specs.videodb);
	fb_destroy_vsdb(specs.vsdb);
out:
	/* No supported preset found, use default */
	if (first)
		edid_use_default_preset();

	kfree(edid);
	return block_cnt;
}

u32 edid_enum_presets(struct hdmi_device *hdev, int index)
{
	int i, j = 0;

	for (i = 0; i < ARRAY_SIZE(edid_presets); i++) {
		if (edid_presets[i].supported) {
			if (j++ == index)
				return edid_presets[i].preset;
		}
	}

	return V4L2_DV_INVALID;
}

u32 edid_preferred_preset(struct hdmi_device *hdev)
{
#ifdef CONFIG_VIDEO_MHL_SII8246
	if (hdmi_forced_resolution >= 0)
		return hdmi_forced_resolution;
	else
#endif
	return preferred_preset;
}

bool edid_supports_hdmi(struct hdmi_device *hdev)
{
	return edid_misc & FB_MISC_HDMI;
}

u32 edid_audio_informs(struct hdmi_device *hdev)
{
	u32 value = 0, ch_info = 0;

	if (max_audio_channels > 0)
		ch_info |= (1 << (max_audio_channels - 1));
	if (max_audio_channels > 6)
		ch_info |= (1 << 5);
	value = ((audio_sample_rates << 19) | (audio_bit_rates << 16) |
			ch_info);
	return value;
}


int edid_source_phy_addr(struct hdmi_device *hdev)
{
	return source_phy_addr;
}

static int __devinit edid_probe(struct i2c_client *client,
				const struct i2c_device_id *dev_id)
{
	edid_client = client;
	edid_use_default_preset();
	dev_info(&client->adapter->dev, "probed exynos edid\n");
	return 0;
}

static int edid_remove(struct i2c_client *client)
{
	edid_client = NULL;
	dev_info(&client->adapter->dev, "removed exynos edid\n");
	return 0;
}

static struct i2c_device_id edid_idtable[] = {
	{"exynos_edid", 2},
};
MODULE_DEVICE_TABLE(i2c, edid_idtable);

static struct i2c_driver edid_driver = {
	.driver = {
		.name = "exynos_edid",
		.owner = THIS_MODULE,
	},
	.id_table	= edid_idtable,
	.probe		= edid_probe,
	.remove		= __devexit_p(edid_remove),
};

static int __init edid_init(void)
{
	return i2c_add_driver(&edid_driver);
}

static void __exit edid_exit(void)
{
	i2c_del_driver(&edid_driver);
}
module_init(edid_init);
module_exit(edid_exit);
