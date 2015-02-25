/*
 * Driver for Silicon Image SiL8620 Mobile HD Transmitter
 *
 * Copyright (C) 2015, Samsung Electronics Co., Ltd.
 * Andrzej Hajda <a.hajda@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <drm/drm_edid.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <linux/mhl.h>
#include "sii8620.h"

#define VAL_RX_HDMI_CTRL2_DEFVAL	VAL_RX_HDMI_CTRL2_IDLE_CNT(3)

enum sii8620_mode {
	CM_DISCONNECTED,
	CM_DISCOVERY,
	CM_MHL1,
	CM_MHL3
};

enum sii8620_status_indices {
	MHL_ST_CONNECTED_RDY,
	MHL_ST_LINK_MODE,
	MHL_ST_VERSION,
	MHL_XST_CBUS_MODE,
	MHL_XST_LINK_STATUS,
	MHL_XST_AV_LINK_MODE_CONTROL,
	MHL_XST_SINK_STATUS,
	MHL_ST_LENGTH
};

enum sii8620_sink_type {
	SINK_NONE,
	SINK_HDMI,
	SINK_DVI
};

const char *sii8620_sink_type_str[] = {
	[SINK_NONE] = "NONE",
	[SINK_HDMI] = "HDMI",
	[SINK_DVI] = "DVI"
};

struct sii8620 {
	struct device *dev;
	struct clk *clk_xtal;
	struct gpio_desc *gpio_reset;
	struct gpio_desc *gpio_int;
	struct regulator_bulk_data supplies[2];
	struct mutex lock; /* context lock */
	int i2c_error;
	enum sii8620_mode mode;
	enum sii8620_sink_type sink_type;
	u8 cbus_status;
	u8 stat[MHL_ST_LENGTH];
	u8 devcap[MHL_DEVCAP_SIZE];
	u8 avif[19];
	struct edid *edid;
	unsigned int gen2_write_burst:1;
	unsigned int mt_ready:1;
	unsigned int mt_done:1;
	int irq;
	struct list_head mt_queue;
};

struct sii8620_msc_msg;

typedef void (*sii8620_msc_msg_cb)(struct sii8620 *ctx,
				   struct sii8620_msc_msg *msg);

struct sii8620_msc_msg {
	struct list_head node;
	u8 cmd;
	u8 reg[2];
	u8 ret;
	sii8620_msc_msg_cb send;
	sii8620_msc_msg_cb recv;
};

static const u8 sii8620_i2c_page[] = {
	0x39, /* Main System */
	0x3d, /* TDM and HSIC */
	0x49, /* TMDS Receiver, MHL EDID */
	0x4d, /* eMSC, HDCP, HSIC */
	0x5d, /* MHL Spec */
	0x64, /* MHL CBUS */
	0x59, /* Hardware TPI (Transmitter Programming Interface) */
	0x61, /* eCBUS-S, eCBUS-D */
};

static int sii8620_clear_error(struct sii8620 *ctx)
{
	int ret = ctx->i2c_error;

	ctx->i2c_error = 0;
	return ret;
}

static void sii8620_read_buf(struct sii8620 *ctx, u16 addr, u8 *buf, int len)
{
	struct device *dev = ctx->dev;
	struct i2c_client *client = to_i2c_client(dev);
	u8 data = addr;
	struct i2c_msg msg[] = {
		{
			.addr = sii8620_i2c_page[addr >> 8],
			.flags = client->flags,
			.len = 1,
			.buf = &data
		},
		{
			.addr = sii8620_i2c_page[addr >> 8],
			.flags = client->flags | I2C_M_RD,
			.len = len,
			.buf = buf
		},
	};
	int ret;

	if (ctx->i2c_error)
		return;

	ret = i2c_transfer(client->adapter, msg, 2);
	pr_debug("MHLR %02x:%02x %*ph, %d\n", msg[0].addr, addr & 0xff, len,
		 buf, ret);

	if (ret != 2) {
		dev_err(dev, "I2C read of [%#06x] failed with code %d.\n",
			addr, ret);
		ctx->i2c_error = ret < 0 ? ret : -EIO;
	}
}

static u8 sii8620_readb(struct sii8620 *ctx, u16 addr)
{
	u8 ret;

	sii8620_read_buf(ctx, addr, &ret, 1);
	return ret;
}

static void sii8620_write_buf(struct sii8620 *ctx, u16 addr, const u8 *buf,
			      int len)
{
	struct device *dev = ctx->dev;
	struct i2c_client *client = to_i2c_client(dev);
	u8 data[2];
	struct i2c_msg msg = {
		.addr = sii8620_i2c_page[addr >> 8],
		.flags = client->flags,
		.len = len + 1,
	};
	int ret;

	if (ctx->i2c_error)
		return;

	if (len > 1) {
		msg.buf = kmalloc(len + 1, GFP_KERNEL);
		if (!msg.buf) {
			ctx->i2c_error = -ENOMEM;
			return;
		}
		memcpy(msg.buf + 1, buf, len);
	} else {
		msg.buf = data;
		msg.buf[1] = *buf;
	}

	msg.buf[0] = addr;

	ret = i2c_transfer(client->adapter, &msg, 1);
	pr_debug("MHLW %02x:%02x %*ph, %d\n", msg.addr, addr & 0xff, len, buf,
		 ret);

	if (ret != 1) {
		dev_err(dev, "I2C write [%#06x]=%*ph failed with code %d.\n",
			addr, len, buf, ret);
		ctx->i2c_error = ret ?: -EIO;
	}

	if (len > 1)
		kfree(msg.buf);
}

#define sii8620_write(ctx, addr, arr...) \
({\
	u8 d[] = { arr }; \
	sii8620_write_buf(ctx, addr, d, ARRAY_SIZE(d)); \
})

static void _sii8620_write_seq(struct sii8620 *ctx, u16 *seq, int len)
{
	int i;

	for (i = 0; i < len; i += 2)
		sii8620_write(ctx, seq[i], seq[i + 1]);
}

#define sii8620_write_seq(ctx, seq...) \
({\
	u16 d[] = { seq }; \
	_sii8620_write_seq(ctx, d, ARRAY_SIZE(d)); \
})

static void sii8620_setbits(struct sii8620 *ctx, u16 addr, u8 mask, u8 val)
{
	val &= mask;
	val |= sii8620_readb(ctx, addr) & ~mask;
	sii8620_write(ctx, addr, val);
}

enum edid_cae_data_block_type_tag {
	EDID_CAE_DBT_AUDIO = 1,
	EDID_CAE_DBT_VIDEO = 2,
	EDID_CAE_DBT_VENDOR = 3,
	EDID_CAE_DBT_SPEAKER = 4,
};

static void sii8620_parse_edid_cea(struct sii8620 *ctx, u8 *cea)
{
	int i;

	for (i = 4; i < cea[2]; i += (cea[i] & 0x1f) + 1) {
		switch (cea[i] >> 5) {
		case EDID_CAE_DBT_VENDOR:
			if (!memcmp(&cea[i + 1], "\x03\x0c\x00", 3))
				ctx->sink_type = SINK_HDMI;
		}
	}
}

static void sii8620_parse_edid(struct sii8620 *ctx)
{
	struct edid *edid = ctx->edid;
	int extn;
	u8 *ext;

	if (!edid)
		return;

	ctx->sink_type = SINK_DVI;

	for (extn = 0; extn < edid->extensions; ++extn) {
		ext = (void *)&edid[extn + 1];
		switch (ext[0]) {
		case CEA_EXT:
			sii8620_parse_edid_cea(ctx, ext);
		}
	}
}

static void sii8620_msc_work(struct sii8620 *ctx)
{
	struct sii8620_msc_msg *msg;

	if (list_empty(&ctx->mt_queue))
		return;

	if (ctx->mt_done) {
		msg = list_first_entry(&ctx->mt_queue, struct sii8620_msc_msg,
				       node);

		if (msg->recv)
			msg->recv(ctx, msg);
		list_del(&msg->node);
		kfree(msg);
		ctx->mt_done = 0;
	}

	if (!ctx->mt_ready || list_empty(&ctx->mt_queue))
		return;

	msg = list_first_entry(&ctx->mt_queue, struct sii8620_msc_msg, node);

	if (msg->send)
		msg->send(ctx, msg);
	ctx->mt_ready = 0;
}

static void sii8620_mt_write_stat_send(struct sii8620 *ctx,
				       struct sii8620_msc_msg *msg)
{
	sii8620_write_seq(ctx,
		REG_MSC_CMD_OR_OFFSET, msg->reg[0],
		REG_MSC_1ST_TRANSMIT_DATA, msg->reg[1],
		REG_MSC_COMMAND_START, BIT_MSC_COMMAND_START_WRITE_STAT
	);
}

static void sii8620_mt_write_stat(struct sii8620 *ctx, u8 reg, u8 val)
{
	struct sii8620_msc_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (!msg) {
		ctx->i2c_error = -ENOMEM;
		return;
	}

	msg->reg[0] = reg;
	msg->reg[1] = val;
	msg->send = sii8620_mt_write_stat_send;
	list_add_tail(&msg->node, &ctx->mt_queue);
}

static inline void sii8620_mt_set_int(struct sii8620 *ctx, u8 irq, u8 mask)
{
	sii8620_mt_write_stat(ctx, irq, mask);
}

static void sii8620_mt_read_devcap_send(struct sii8620 *ctx,
					struct sii8620_msc_msg *msg)
{
	sii8620_write_seq(ctx,
		REG_INTR9_MASK, BIT_INTR9_DEVCAP_DONE,
		REG_EDID_CTRL, VAL_EDID_CTRL_EDID_PRIME_VALID_DISABLE
			| VAL_EDID_CTRL_DEVCAP_SELECT_DEVCAP
			| VAL_EDID_CTRL_EDID_FIFO_ADDR_AUTO_ENABLE
			| VAL_EDID_CTRL_EDID_MODE_EN_ENABLE,
		REG_TPI_CBUS_START, BIT_TPI_CBUS_START_GET_DEVCAP_START
	);
}

static void sii8620_fetch_edid(struct sii8620 *ctx);
static void sii8620_set_upstream_edid(struct sii8620 *ctx);
static void sii8620_enable_hpd(struct sii8620 *ctx);

static void sii8620_mt_read_devcap_recv(struct sii8620 *ctx,
					struct sii8620_msc_msg *msg)
{
	sii8620_write_seq(ctx,
		REG_INTR9_MASK, BIT_INTR9_DEVCAP_DONE | BIT_INTR9_EDID_DONE
			| BIT_INTR9_EDID_ERROR,
		REG_EDID_CTRL, VAL_EDID_CTRL_EDID_PRIME_VALID_DISABLE
			| VAL_EDID_CTRL_DEVCAP_SELECT_DEVCAP
			| VAL_EDID_CTRL_EDID_FIFO_ADDR_AUTO_ENABLE
			| VAL_EDID_CTRL_EDID_MODE_EN_ENABLE,
		REG_EDID_FIFO_ADDR, 0
	);

	sii8620_read_buf(ctx, REG_EDID_FIFO_RD_DATA, ctx->devcap, MHL_DEVCAP_SIZE);

	sii8620_fetch_edid(ctx);
	sii8620_parse_edid(ctx);
	dev_info(ctx->dev, "detected sink type: %s\n",
		sii8620_sink_type_str[ctx->sink_type]);
	sii8620_set_upstream_edid(ctx);
	sii8620_enable_hpd(ctx);
}

static void sii8620_mt_read_devcap(struct sii8620 *ctx)
{
	struct sii8620_msc_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (!msg) {
		ctx->i2c_error = -ENOMEM;
		return;
	}

	msg->send = sii8620_mt_read_devcap_send;
	msg->recv = sii8620_mt_read_devcap_recv;
	list_add_tail(&msg->node, &ctx->mt_queue);
}

static void sii8620_fetch_edid(struct sii8620 *ctx)
{
	u8 lm_ddc;
	u8 ddc_cmd;
	u8 int3;
	u8 cbus;
	int i;
	u8 *edid;
	int edid_len = EDID_LENGTH;
	int fetched;

	sii8620_readb(ctx, REG_CBUS_STATUS);
	lm_ddc = sii8620_readb(ctx, REG_LM_DDC);
	ddc_cmd = sii8620_readb(ctx, REG_DDC_CMD);

	sii8620_write_seq(ctx,
		REG_INTR9_MASK, 0,
		REG_EDID_CTRL, VAL_EDID_CTRL_EDID_FIFO_ADDR_AUTO_ENABLE,
		REG_HDCP2X_POLL_CS, 0x71,
		REG_HDCP2X_CTRL_0, BIT_HDCP2X_CTRL_0_HDCP2X_HDCPTX,
		REG_LM_DDC, lm_ddc | VAL_LM_DDC_SW_TPI_EN_DISABLED,
	);

	for (i = 0; i < 256; ++i) {
		u8 ddc_stat = sii8620_readb(ctx, REG_DDC_STATUS);

		if (!(ddc_stat & BIT_DDC_STATUS_DDC_I2C_IN_PROG))
			break;
		sii8620_write(ctx, REG_DDC_STATUS,
			       BIT_DDC_STATUS_DDC_FIFO_EMPTY);
	}

	sii8620_write(ctx, REG_DDC_ADDR, 0x50 << 1);

#define FETCH_SIZE 16
	edid = kmalloc(EDID_LENGTH, GFP_KERNEL);
	if (!edid) {
		ctx->i2c_error = -ENOMEM;
		return;
	}

	for (fetched = 0; fetched < edid_len; fetched += FETCH_SIZE) {
		sii8620_readb(ctx, REG_DDC_STATUS);
		sii8620_write_seq(ctx,
			REG_DDC_CMD, ddc_cmd | VAL_DDC_CMD_DDC_CMD_ABORT,
			REG_DDC_CMD, ddc_cmd | VAL_DDC_CMD_DDC_CMD_CLEAR_FIFO,
			REG_DDC_STATUS, BIT_DDC_STATUS_DDC_FIFO_EMPTY
		);
		sii8620_write_seq(ctx,
			REG_DDC_SEGM, fetched >> 8,
			REG_DDC_OFFSET, fetched & 0xff,
			REG_DDC_DIN_CNT1, FETCH_SIZE,
			REG_DDC_DIN_CNT2, 0,
			REG_DDC_CMD, ddc_cmd | VAL_DDC_CMD_ENH_DDC_READ_NO_ACK
		);

		do {
			int3 = sii8620_readb(ctx, REG_INTR3);
			cbus = sii8620_readb(ctx, REG_CBUS_STATUS);

			if (int3 & BIT_DDC_CMD_DONE)
				break;

			if (!(cbus & BIT_CBUS_STATUS_CBUS_CONNECTED))
				return;
		} while (1);

		sii8620_readb(ctx, REG_DDC_STATUS);
		while (sii8620_readb(ctx, REG_DDC_DOUT_CNT) < FETCH_SIZE)
			usleep_range(10, 20);

		sii8620_read_buf(ctx, REG_DDC_DATA, edid + fetched, FETCH_SIZE);
		if (fetched + FETCH_SIZE == EDID_LENGTH) {
			u8 ext = ((struct edid *)edid)->extensions;

			if (ext) {
				edid_len += ext * EDID_LENGTH;
				edid = krealloc(edid, edid_len, GFP_KERNEL);
			}
		}

		if (fetched + FETCH_SIZE == edid_len)

		sii8620_write(ctx, REG_INTR3, int3);
	}

	sii8620_write(ctx, REG_LM_DDC, lm_ddc);

	if (ctx->edid)
		kfree(ctx->edid);

	ctx->edid = (struct edid *)edid;
}

static inline unsigned int sii8620_edid_size(struct edid *edid) {
	return (edid->extensions + 1) * EDID_LENGTH;
}

static void sii8620_set_upstream_edid(struct sii8620 *ctx)
{
	sii8620_setbits(ctx, REG_DPD, BIT_DPD_PDNRX12 | BIT_DPD_PDIDCK_N
			| BIT_DPD_PD_MHL_CLK_N, 0xff);

	sii8620_write_seq(ctx,
		REG_RX_HDMI_CTRL3, 0x00,
		REG_PKT_FILTER_0, 0xFF,
		REG_PKT_FILTER_1, 0xFF,
		REG_ALICE0_BW_I2C, 0x06
	);

	sii8620_setbits(ctx, REG_RX_HDMI_CLR_BUFFER,
			BIT_RX_HDMI_CLR_BUFFER_VSI_CLR_EN, 0xff);

	sii8620_write_seq(ctx,
		REG_EDID_CTRL, VAL_EDID_CTRL_EDID_PRIME_VALID_DISABLE
			| VAL_EDID_CTRL_DEVCAP_SELECT_EDID
			| VAL_EDID_CTRL_EDID_FIFO_ADDR_AUTO_ENABLE
			| VAL_EDID_CTRL_EDID_MODE_EN_ENABLE,
		REG_EDID_FIFO_ADDR, 0,
	);

	sii8620_write_buf(ctx, REG_EDID_FIFO_WR_DATA, (u8 *)ctx->edid,
			  sii8620_edid_size(ctx->edid));

	sii8620_write_seq(ctx,
		REG_EDID_CTRL, VAL_EDID_CTRL_EDID_PRIME_VALID_ENABLE
			| VAL_EDID_CTRL_DEVCAP_SELECT_EDID
			| VAL_EDID_CTRL_EDID_FIFO_ADDR_AUTO_ENABLE
			| VAL_EDID_CTRL_EDID_MODE_EN_ENABLE,
		REG_INTR5_MASK, BIT_INTR_SCDT_CHANGE,
		REG_INTR9_MASK, 0
	);
}

static void sii8620_xtal_set_rate(struct sii8620 *ctx)
{
	static const struct {
		unsigned int rate;
		u8 div;
		u8 tp1;
	} rates[] = {
		{ 19200, 0x04, 0x53 },
		{ 20000, 0x04, 0x62 },
		{ 24000, 0x05, 0x75 },
		{ 30000, 0x06, 0x92 },
		{ 38400, 0x0c, 0xbc },
	};
	unsigned long rate = clk_get_rate(ctx->clk_xtal) / 1000;
	int i;

	for (i = 0; i < ARRAY_SIZE(rates) - 1; ++i)
		if (rate <= rates[i].rate)
			break;

	if (rate != rates[i].rate)
		dev_err(ctx->dev, "xtal clock rate(%lukHz) not supported, setting MHL for %ukHz.\n",
			rate, rates[i].rate);

	sii8620_write(ctx, REG_DIV_CTL_MAIN, rates[i].div);
	sii8620_write(ctx, REG_HDCP2X_TP1, rates[i].tp1);
}

static int sii8620_hw_on(struct sii8620 *ctx)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret)
		return ret;

	gpiod_set_value(ctx->gpio_reset, 1);

	return 0;
}

static void sii8620_hw_reset(struct sii8620 *ctx)
{
	usleep_range(10000, 20000);
	gpiod_set_value(ctx->gpio_reset, 1);
	usleep_range(5000, 20000);
	gpiod_set_value(ctx->gpio_reset, 0);
	usleep_range(10000, 20000);
	gpiod_set_value(ctx->gpio_reset, 1);
	msleep(30);
}

static void sii8620_cbus_reset(struct sii8620 *ctx)
{
	sii8620_write_seq(ctx,
		REG_PWD_SRST, BIT_PWD_SRST_CBUS_RST
			| BIT_PWD_SRST_CBUS_RST_SW_EN,
		REG_PWD_SRST, BIT_PWD_SRST_CBUS_RST_SW_EN
	);

	/* TODO: android do not use it
	sii8620_setbits(ctx, REG_COC_MISC_CTL0, BIT_COC_MISC_CTL0_FSM_MON, ~0);
	sii8620_setbits(ctx, REG_GPIO_CTRL1,
		BIT_CTRL1_GPIO_I_7 | BIT_CTRL1_GPIO_OEN_7 |
		BIT_CTRL1_GPIO_I_6 | BIT_CTRL1_GPIO_OEN_6,
		0);
	*/
}

static void sii8620_set_auto_zone(struct sii8620 *ctx)
{
	if (ctx->mode != CM_MHL1) {
		sii8620_write_seq(ctx,
			REG_TX_ZONE_CTL1, 0x0,
			REG_MHL_PLL_CTL0, VAL_MHL_PLL_CTL0_HDMI_CLK_RATIO_1X
				| BIT_MHL_PLL_CTL0_CRYSTAL_CLK_SEL
				| BIT_MHL_PLL_CTL0_ZONE_MASK_OE
		);
	} else {
		sii8620_write_seq(ctx,
			REG_TX_ZONE_CTL1, VAL_TX_ZONE_CTL1_TX_ZONE_CTRL_MODE,
			REG_MHL_PLL_CTL0, VAL_MHL_PLL_CTL0_HDMI_CLK_RATIO_1X
				| BIT_MHL_PLL_CTL0_ZONE_MASK_OE
		);
	}
}

static void sii8620_stop_video(struct sii8620 *ctx)
{
	u8 uninitialized_var(val);

	/* TODO: add MHL3 and DVI support  */
	sii8620_write_seq(ctx,
		REG_TPI_INTR_EN, 0,
		REG_HDCP2X_INTR0_MASK, 0,
		REG_TPI_COPP_DATA2, 0,
		REG_TPI_INTR_ST0, ~0,
	);

	switch (ctx->sink_type) {
	case SINK_NONE:
		return;
	case SINK_DVI:
		val = VAL_TPI_SC_REG_TMDS_OE_POWER_DOWN
			| VAL_TPI_SC_TPI_AV_MUTE_MUTED;
		break;
	case SINK_HDMI:
		val = VAL_TPI_SC_REG_TMDS_OE_POWER_DOWN
			| VAL_TPI_SC_TPI_AV_MUTE_MUTED
			| VAL_TPI_SC_TPI_OUTPUT_MODE_0_HDMI;
		break;
	}

	sii8620_write(ctx, REG_TPI_SC, val);
}

static void sii8620_start_hdmi(struct sii8620 *ctx)
{
	sii8620_write_seq(ctx,
		REG_RX_HDMI_CTRL2, VAL_RX_HDMI_CTRL2_DEFVAL
			| BIT_RX_HDMI_CTRL2_USE_AV_MUTE,
		REG_VID_OVRRD, BIT_VID_OVRRD_PP_AUTO_DISABLE
			| BIT_VID_OVRRD_M1080P_OVRRD,
		REG_VID_MODE, VAL_VID_MODE_M1080P_DISABLE,
		REG_MHL_TOP_CTL, 0x1,
		REG_MHLTX_CTL6, 0xa0,
		REG_TPI_INPUT, VAL_TPI_FORMAT(RGB, FULL),
		REG_TPI_OUTPUT, VAL_TPI_FORMAT(RGB, FULL),
	);

	sii8620_mt_write_stat(ctx, MHL_STATUS_REG_LINK_MODE,
			MHL_STATUS_CLK_MODE_NORMAL | MHL_STATUS_PATH_ENABLED);

	sii8620_set_auto_zone(ctx);

	// TODO: add HDCP
	//sii8620_stop_video(ctx);

	sii8620_write(ctx, REG_TPI_SC, VAL_TPI_SC_TPI_OUTPUT_MODE_0_HDMI);

	sii8620_write_buf(ctx, REG_TPI_AVI_CHSUM, ctx->avif, ARRAY_SIZE(ctx->avif));

	sii8620_write(ctx, REG_PKT_FILTER_0, 0xa1, 0x2);
}

static void sii8620_start_dvi(struct sii8620 *ctx)
{
	// TODO
}

static void sii8620_start_video(struct sii8620 *ctx)
{
	if (ctx->mode < CM_MHL3)
		sii8620_stop_video(ctx);

	switch (ctx->sink_type) {
	case SINK_HDMI:
		sii8620_start_hdmi(ctx);
		break;
	case SINK_DVI:
		sii8620_start_dvi(ctx);
		break;
	default:
		return;
	}
}

static void sii8620_disable_hpd(struct sii8620 *ctx)
{
	sii8620_setbits(ctx, REG_EDID_CTRL, BIT_EDID_CTRL_EDID_PRIME_VALID, 0);
	sii8620_write_seq(ctx,
		REG_HPD_CTRL, BIT_HPD_CTRL_HPD_OUT_OVR_EN,
		REG_INTR8_MASK, 0
	);
}

static void sii8620_enable_hpd(struct sii8620 *ctx)
{
	sii8620_setbits(ctx, REG_TMDS_CSTAT_P3,
			BIT_TMDS_CSTAT_P3_SCDT_CLR_AVI_DIS
			| BIT_TMDS_CSTAT_P3_CLR_AVI, ~0);
	sii8620_write_seq(ctx,
		REG_HPD_CTRL, BIT_HPD_CTRL_HPD_OUT_OVR_EN
			| VAL_HPD_CTRL_HPD_HIGH,
	);
}

static void sii8620_enable_gen2_write_burst(struct sii8620 *ctx)
{
	if (ctx->gen2_write_burst)
		return;

	sii8620_write_seq(ctx,
		REG_MDT_RCV_TIMEOUT, 100,
//		REG_MDT_XMIT_CONTROL, BIT_MDT_XMIT_CONTROL_MDT_XMIT_EN
//			| BIT_MDT_XMIT_CONTROL_MDT_XMIT_FIXED_BURST_LEN,
		REG_MDT_RCV_CONTROL, BIT_MDT_RCV_CONTROL_MDT_RCV_EN
	);
	ctx->gen2_write_burst = 1;
}

static void sii8620_disable_gen2_write_burst(struct sii8620 *ctx)
{
	if (!ctx->gen2_write_burst)
		return;

	sii8620_write_seq(ctx,
		REG_MDT_XMIT_CONTROL, 0,
		REG_MDT_RCV_CONTROL, 0
	);
	ctx->gen2_write_burst = 0;
}

static void sii8620_start_gen2_write_burst(struct sii8620 *ctx)
{
	sii8620_write_seq(ctx,
		REG_MDT_INT_1_MASK, BIT_MDT_RCV_TIMEOUT
			| BIT_MDT_RCV_SM_ABORT_PKT_RCVD | BIT_MDT_RCV_SM_ERROR
			| BIT_MDT_XMIT_TIMEOUT | BIT_MDT_XMIT_SM_ABORT_PKT_RCVD
			| BIT_MDT_XMIT_SM_ERROR,
		REG_MDT_INT_0_MASK, BIT_MDT_XFIFO_EMPTY
			| BIT_MDT_IDLE_AFTER_HAWB_DISABLE
			| BIT_MDT_RFIFO_DATA_RDY
	);
	sii8620_enable_gen2_write_burst(ctx);
}

static void sii8620_mhl_discover(struct sii8620 *ctx)
{
	sii8620_write_seq(ctx,
		REG_DISC_CTRL9, BIT_DISC_CTRL9_WAKE_DRVFLT
			| BIT_DISC_CTRL9_DISC_PULSE_PROCEED,
		REG_DISC_CTRL4, VAL_DISC_CTRL4(VAL_PUP_5K, VAL_PUP_20K),
		REG_CBUS_DISC_INTR0_MASK, BIT_MHL3_EST_INT
			| BIT_MHL_EST_INT
			| BIT_NOT_MHL_EST_INT
			| BIT_CBUS_MHL3_DISCON_INT
			| BIT_CBUS_MHL12_DISCON_INT
			| BIT_RGND_READY_INT,
		REG_MHL_PLL_CTL0, VAL_MHL_PLL_CTL0_HDMI_CLK_RATIO_1X
			| BIT_MHL_PLL_CTL0_CRYSTAL_CLK_SEL
			| BIT_MHL_PLL_CTL0_ZONE_MASK_OE,
		REG_MHL_DP_CTL0, BIT_MHL_DP_CTL0_DP_OE
			| BIT_MHL_DP_CTL0_TX_OE_OVR,
		REG_M3_CTRL, VAL_M3_CTRL_MHL3_VALUE,
		REG_MHL_DP_CTL1, 0xA2,
		REG_MHL_DP_CTL2, 0x03,
		REG_MHL_DP_CTL3, 0x35,
		REG_MHL_DP_CTL5, 0x02,
		REG_MHL_DP_CTL6, 0x02,
		REG_MHL_DP_CTL7, 0x03,
		REG_COC_CTLC, 0xFF,
		REG_DPD, BIT_DPD_PWRON_PLL | BIT_DPD_PDNTX12
			| BIT_DPD_OSC_EN | BIT_DPD_PWRON_HSIC,
		REG_COC_INTR_MASK, BIT_COC_PLL_LOCK_STATUS_CHANGE
			| BIT_COC_CALIBRATION_DONE,
		REG_CBUS_INT_1_MASK, BIT_CBUS_MSC_ABORT_RCVD
			| BIT_CBUS_CMD_ABORT,
		REG_CBUS_INT_0_MASK, BIT_CBUS_MSC_MT_DONE
			| BIT_CBUS_HPD_CHG
			| BIT_CBUS_MSC_MR_WRITE_STAT
			| BIT_CBUS_MSC_MR_MSC_MSG
			| BIT_CBUS_MSC_MR_WRITE_BURST
			| BIT_CBUS_MSC_MR_SET_INT
			| BIT_CBUS_MSC_MT_DONE_NACK
	);
}

static void sii8620_peer_specific_init(struct sii8620 *ctx)
{
	if (ctx->mode == CM_MHL3)
		sii8620_write_seq(ctx,
			REG_SYS_CTRL1, BIT_SYS_CTRL1_BLOCK_DDC_BY_HPD,
			REG_EMSCINTRMASK1,
				BIT_EMSCINTR1_EMSC_TRAINING_COMMA_ERR
		);
	else
		sii8620_write_seq(ctx,
			REG_HDCP2X_INTR0_MASK, 0x00,
			REG_EMSCINTRMASK1, 0x00,
			REG_HDCP2X_INTR0, 0xFF,
			REG_INTR1, 0xFF,
			REG_SYS_CTRL1, BIT_SYS_CTRL1_BLOCK_DDC_BY_HPD
				| BIT_SYS_CTRL1_TX_CONTROL_HDMI
		);
}

#define SII8620_MHL_VERSION			0x32
#define SII8620_SCRATCHPAD_SIZE			16
#define SII8620_INT_STAT_SIZE			0x33

static void sii8620_set_dev_cap(struct sii8620 *ctx)
{
	static const u8 dev_cap[MHL_DEVCAP_SIZE] = {
		[MHL_DEVCAP_MHL_VERSION] = 0x32,
		[MHL_DEVCAP_DEV_CAT] = MHL_DEV_CAT_SOURCE | MHL_DEV_CAT_POWER,
		[MHL_DEVCAP_ADOPTER_ID_H] = 0x01,
		[MHL_DEVCAP_ADOPTER_ID_L] = 0x41,
		[MHL_DEVCAP_VID_LINK_MODE] = MHL_DEV_VID_LINK_SUPP_RGB444
			| MHL_DEV_VID_LINK_SUPP_PPIXEL
			| MHL_DEV_VID_LINK_SUPP_16BPP,
		[MHL_DEVCAP_AUD_LINK_MODE] = MHL_DEV_AUD_LINK_2CH,
		[MHL_DEVCAP_VIDEO_TYPE] = MHL_VT_GRAPHICS,
		[MHL_DEVCAP_LOG_DEV_MAP] = MHL_DEV_LD_GUI,
		[MHL_DEVCAP_BANDWIDTH] = 0x0f,
		[MHL_DEVCAP_FEATURE_FLAG] = MHL_FEATURE_RCP_SUPPORT
			| MHL_FEATURE_RAP_SUPPORT | MHL_FEATURE_SP_SUPPORT,
		[MHL_DEVCAP_SCRATCHPAD_SIZE] = SII8620_SCRATCHPAD_SIZE,
		[MHL_DEVCAP_INT_STAT_SIZE] = SII8620_INT_STAT_SIZE,
	};
	static const u8 xdev_cap[MHL_XDEVCAP_SIZE] = {
		[MHL_XDEVCAP_ECBUS_SPEEDS] = MHL_XDC_ECBUS_S_075
			| MHL_XDC_ECBUS_S_8BIT,
		[MHL_XDEVCAP_TMDS_SPEEDS] = MHL_XDC_TMDS_150
			| MHL_XDC_TMDS_300 | MHL_XDC_TMDS_600,
		[MHL_XDEVCAP_ECBUS_ROLES] = MHL_XDC_DEV_HOST,
		[MHL_XDEVCAP_LOG_DEV_MAPX] = MHL_XDC_LD_PHONE,
	};

	sii8620_write_buf(ctx, REG_MHL_DEVCAP_0, dev_cap, ARRAY_SIZE(dev_cap));
	sii8620_write_buf(ctx, REG_MHL_EXTDEVCAP_0, xdev_cap, ARRAY_SIZE(xdev_cap));
}

static void sii8620_mhl_init(struct sii8620 *ctx)
{
	sii8620_write_seq(ctx,
		REG_DISC_CTRL4, VAL_DISC_CTRL4(VAL_PUP_OFF, VAL_PUP_20K),
		REG_CBUS_MSC_COMPATIBILITY_CONTROL,
			BIT_CBUS_MSC_COMPATIBILITY_CONTROL_XDEVCAP_EN,
	);

	sii8620_peer_specific_init(ctx);

	sii8620_disable_hpd(ctx);

	sii8620_write_seq(ctx,
		REG_EDID_CTRL, VAL_EDID_CTRL_EDID_FIFO_ADDR_AUTO_ENABLE,
		REG_DISC_CTRL9, BIT_DISC_CTRL9_WAKE_DRVFLT
			| BIT_DISC_CTRL9_WAKE_PULSE_BYPASS,
		REG_TMDS0_CCTRL1, 0x90,
		REG_TMDS_CLK_EN, 0x01,
		REG_TMDS_CH_EN, 0x11,
		REG_BGR_BIAS, 0x87,
		REG_ALICE0_ZONE_CTRL, 0xE8,
		REG_ALICE0_MODE_CTRL, 0x04,
	);
	sii8620_setbits(ctx, REG_LM_DDC, BIT_LM_DDC_SW_TPI_EN,
			VAL_LM_DDC_SW_TPI_EN_ENABLED);
	sii8620_write_seq(ctx,
		REG_TPI_HW_OPT3, 0x76,
		REG_TMDS_CCTRL, BIT_TMDS_CCTRL_TMDS_OE,
		REG_TPI_DTD_B2, 79,
	);
	sii8620_set_dev_cap(ctx);
	sii8620_write_seq(ctx,
		REG_MDT_XMIT_TIMEOUT, 100,
		REG_MDT_XMIT_CONTROL, 0x03,
		REG_MDT_XFIFO_STAT, 0x00,
		REG_MDT_RCV_TIMEOUT, 100,
		REG_CBUS_LINK_CONTROL_8, 0x1D,
	);

	sii8620_start_gen2_write_burst(ctx);
	sii8620_write_seq(ctx,
		REG_BIST_CTRL, 0x00,
		REG_COC_CTL1, 0x10,
		REG_COC_CTL2, 0x18,
		REG_COC_CTLF, 0x07,
		REG_COC_CTL11, 0xF8,
		REG_COC_CTL17, 0x61,
		REG_COC_CTL18, 0x46,
		REG_COC_CTL19, 0x15,
		REG_COC_CTL1A, 0x01,
		REG_MHL_COC_CTL3, BIT_MHL_COC_CTL3_COC_AECHO_EN,
		REG_MHL_COC_CTL4, 0x2D,
		REG_MHL_COC_CTL5, 0xF9,
		REG_MSC_HEARTBEAT_CONTROL, 0x27,
	);
	sii8620_disable_gen2_write_burst(ctx);

	sii8620_mt_write_stat(ctx, MHL_STATUS_REG_VERSION_STAT,
			       SII8620_MHL_VERSION);
	sii8620_mt_write_stat(ctx, MHL_STATUS_REG_CONNECTED_RDY,
			       MHL_STATUS_DCAP_RDY | MHL_STATUS_XDEVCAPP_SUPP
			       | MHL_STATUS_POW_STAT);
	sii8620_mt_set_int(ctx, MHL_RCHANGE_INT, MHL_INT_DCAP_CHG);

	ctx->mt_ready = 1;
}

static void sii8620_set_mode(struct sii8620 *ctx, enum sii8620_mode mode)
{
	if (ctx->mode == mode)
		return;

	ctx->mode = mode;

	if (mode != CM_MHL1) {
		sii8620_set_auto_zone(ctx);
		return;
	}

	sii8620_write_seq(ctx,
		REG_CBUS_MSC_COMPATIBILITY_CONTROL, 0x02,
		REG_M3_CTRL, VAL_M3_CTRL_MHL1_2_VALUE,
		REG_DPD, BIT_DPD_PWRON_PLL | BIT_DPD_PDNTX12 | BIT_DPD_OSC_EN,
		REG_COC_INTR_MASK, 0
	);

	sii8620_set_auto_zone(ctx);

	sii8620_write_seq(ctx,
		REG_MHL_DP_CTL0, 0xBC,
		REG_MHL_DP_CTL1, 0xBB,
		REG_MHL_DP_CTL3, 0x48,
		REG_MHL_DP_CTL5, 0x39,
		REG_MHL_DP_CTL2, 0x2A,
		REG_MHL_DP_CTL6, 0x2A,
		REG_MHL_DP_CTL7, 0x08
	);
}

static void sii8620_disconnect(struct sii8620 *ctx)
{
	sii8620_disable_gen2_write_burst(ctx);
	sii8620_stop_video(ctx);
	msleep(50);
	sii8620_cbus_reset(ctx);
	sii8620_set_mode(ctx, CM_DISCONNECTED);
	sii8620_write_seq(ctx,
		REG_COC_CTL0, 0x40,
		REG_CBUS3_CNVT, 0x84,
		REG_COC_CTL14, 0x00,
		REG_COC_CTL0, 0x40,
		REG_HRXCTRL3, 0x07,
		REG_MHL_PLL_CTL0, VAL_MHL_PLL_CTL0_HDMI_CLK_RATIO_1X
			| BIT_MHL_PLL_CTL0_CRYSTAL_CLK_SEL
			| BIT_MHL_PLL_CTL0_ZONE_MASK_OE,
		REG_MHL_DP_CTL0, BIT_MHL_DP_CTL0_DP_OE
			| BIT_MHL_DP_CTL0_TX_OE_OVR,
		REG_MHL_DP_CTL1, 0xBB,
		REG_MHL_DP_CTL3, 0x48,
		REG_MHL_DP_CTL5, 0x3F,
		REG_MHL_DP_CTL2, 0x2F,
		REG_MHL_DP_CTL6, 0x2A,
		REG_MHL_DP_CTL7, 0x03
	);
	sii8620_disable_hpd(ctx);
	sii8620_write_seq(ctx,
		REG_M3_CTRL, VAL_M3_CTRL_MHL3_VALUE,
		REG_MHL_COC_CTL1, 0x07,
		REG_DISC_CTRL4, VAL_DISC_CTRL4(VAL_PUP_OFF, VAL_PUP_20K),
		REG_DISC_CTRL8, 0x00,
		REG_DISC_CTRL9, BIT_DISC_CTRL9_WAKE_DRVFLT
			| BIT_DISC_CTRL9_WAKE_PULSE_BYPASS,
		REG_INT_CTRL, 0x00,
		REG_MSC_HEARTBEAT_CONTROL, 0x27,
		REG_DISC_CTRL1, 0x25,
		REG_CBUS_DISC_INTR0, (u8)~BIT_RGND_READY_INT,
		REG_CBUS_DISC_INTR0_MASK, BIT_RGND_READY_INT,
		REG_MDT_INT_1, 0xff,
		REG_MDT_INT_1_MASK, 0x00,
		REG_MDT_INT_0, 0xff,
		REG_MDT_INT_0_MASK, 0x00,
		REG_COC_INTR, 0xff,
		REG_COC_INTR_MASK, 0x00,
		REG_TRXINTH, 0xff,
		REG_TRXINTMH, 0x00,
		REG_CBUS_INT_0, 0xff,
		REG_CBUS_INT_0_MASK, 0x00,
		REG_CBUS_INT_1, 0xff,
		REG_CBUS_INT_1_MASK, 0x00,
		REG_EMSCINTR, 0xff,
		REG_EMSCINTRMASK, 0x00,
		REG_EMSCINTR1, 0xff,
		REG_EMSCINTRMASK1, 0x00,
		REG_INTR8, 0xff,
		REG_INTR8_MASK, 0x00,
		REG_TPI_INTR_ST0, 0xff,
		REG_TPI_INTR_EN, 0x00,
		REG_HDCP2X_INTR0, 0xff,
		REG_HDCP2X_INTR0_MASK, 0x00,
		REG_INTR9, 0xff,
		REG_INTR9_MASK, 0x00,
		REG_INTR3, 0xff,
		REG_INTR3_MASK, 0x00,
		REG_INTR5, 0xff,
		REG_INTR5_MASK, 0x00,
		REG_INTR2, 0xff,
		REG_INTR2_MASK, 0x00,
	);
	memset(ctx->stat, 0, sizeof(ctx->stat));
	memset(ctx->devcap, 0, sizeof(ctx->devcap));
	ctx->cbus_status = 0;
	ctx->sink_type = SINK_NONE;
	kfree(ctx->edid);
	ctx->edid = NULL;
}

static void sii8620_mhl_disconnected(struct sii8620 *ctx)
{
	sii8620_write_seq(ctx,
		REG_DISC_CTRL4, VAL_DISC_CTRL4(VAL_PUP_OFF, VAL_PUP_20K),
		REG_CBUS_MSC_COMPATIBILITY_CONTROL,
			BIT_CBUS_MSC_COMPATIBILITY_CONTROL_XDEVCAP_EN
	);
	sii8620_disconnect(ctx);
}

static void sii8620_irq_disc(struct sii8620 *ctx)
{
	u8 stat = sii8620_readb(ctx, REG_CBUS_DISC_INTR0);

	if (stat & VAL_CBUS_MHL_DISCON)
		sii8620_mhl_disconnected(ctx);

	if (stat & BIT_RGND_READY_INT) {
		u8 stat2 = sii8620_readb(ctx, REG_DISC_STAT2);

		if ((stat2 & MSK_DISC_STAT2_RGND) == VAL_RGND_1K)
			sii8620_mhl_discover(ctx);
		else {
			sii8620_write_seq(ctx,
				REG_DISC_CTRL9, BIT_DISC_CTRL9_WAKE_DRVFLT
					| BIT_DISC_CTRL9_NOMHL_EST
					| BIT_DISC_CTRL9_WAKE_PULSE_BYPASS,
				REG_CBUS_DISC_INTR0_MASK, BIT_RGND_READY_INT
					| BIT_CBUS_MHL3_DISCON_INT
					| BIT_CBUS_MHL12_DISCON_INT
					| BIT_NOT_MHL_EST_INT
			);
		}
	}
	if (stat & BIT_MHL_EST_INT)
		sii8620_mhl_init(ctx);

	sii8620_write(ctx, REG_CBUS_DISC_INTR0, stat);
}

static void sii8620_irq_g2wb(struct sii8620 *ctx)
{
	u8 stat = sii8620_readb(ctx, REG_MDT_INT_0);

	if (stat & BIT_MDT_IDLE_AFTER_HAWB_DISABLE)
		dev_dbg(ctx->dev, "HAWB idle\n");

	sii8620_write(ctx, REG_MDT_INT_0, stat);
}

static void sii8620_status_changed_dcap(struct sii8620 *ctx)
{
	if (ctx->stat[MHL_ST_CONNECTED_RDY] & MHL_STATUS_DCAP_RDY) {
		/* TODO: add MHL3 support */
		sii8620_set_mode(ctx, CM_MHL1);
		sii8620_peer_specific_init(ctx);
		sii8620_write(ctx, REG_INTR9_MASK, BIT_INTR9_DEVCAP_DONE
			       | BIT_INTR9_EDID_DONE | BIT_INTR9_EDID_ERROR);
	} else {
		// TODO
	}
}

static void sii8620_status_changed_path(struct sii8620 *ctx)
{
	if (ctx->stat[MHL_ST_LINK_MODE] & MHL_STATUS_PATH_ENABLED) {
		sii8620_mt_write_stat(ctx, MHL_STATUS_REG_LINK_MODE,
				      MHL_STATUS_CLK_MODE_NORMAL
				      | MHL_STATUS_PATH_ENABLED);
		// TODO: read devcap should be after receiving SET_INT
		sii8620_mt_read_devcap(ctx);
	} else {
		sii8620_mt_write_stat(ctx, MHL_STATUS_REG_LINK_MODE,
				      MHL_STATUS_CLK_MODE_NORMAL);
	}
}

static void sii8620_msc_mr_write_stat(struct sii8620 *ctx)
{
	u8 st[MHL_ST_LENGTH];
	int i;

	sii8620_read_buf(ctx, REG_MHL_STAT_0, st, 3);
	sii8620_read_buf(ctx, REG_MHL_EXTSTAT_0, st + 3, 4);

	for (i = 0; i < ARRAY_SIZE(st); ++i) {
		st[i] ^= ctx->stat[i];
		ctx->stat[i] ^= st[i];
	}

	if (st[MHL_ST_CONNECTED_RDY] & MHL_STATUS_DCAP_RDY)
		sii8620_status_changed_dcap(ctx);

	if (st[MHL_ST_LINK_MODE] & MHL_STATUS_PATH_ENABLED)
		sii8620_status_changed_path(ctx);
}

static void sii8620_msc_mr_set_int(struct sii8620 *ctx)
{
	u8 int_0[2];

	sii8620_read_buf(ctx, REG_MHL_INT_0, int_0, ARRAY_SIZE(int_0));
	sii8620_write_buf(ctx, REG_MHL_INT_0, int_0, ARRAY_SIZE(int_0));
	// TODO: add handling of MHL interrupts
}

static void sii8620_msc_mt_done(struct sii8620 *ctx)
{
	struct device *dev = ctx->dev;
	struct sii8620_msc_msg *msg;

	if (list_empty(&ctx->mt_queue)) {
		dev_err(dev, "unexpected MSC MT response\n");
		return;
	}

	msg = list_first_entry(&ctx->mt_queue, struct sii8620_msc_msg, node);
	msg->ret = sii8620_readb(ctx, REG_MSC_MT_RCVD_DATA0);
	ctx->mt_done = 1;
	ctx->mt_ready = 1;
}

static void sii8620_irq_msc(struct sii8620 *ctx)
{
	u8 stat = sii8620_readb(ctx, REG_CBUS_INT_0);

	if (stat & ~BIT_CBUS_HPD_CHG)
		sii8620_write(ctx, REG_CBUS_INT_0, stat & ~BIT_CBUS_HPD_CHG);

	if (stat & BIT_CBUS_HPD_CHG) {
		u8 cbus_stat = sii8620_readb(ctx, REG_CBUS_STATUS);

		if ((cbus_stat ^ ctx->cbus_status) & BIT_CBUS_STATUS_CBUS_HPD) {
			sii8620_write(ctx, REG_CBUS_INT_0, BIT_CBUS_HPD_CHG);
		} else {
			stat ^= BIT_CBUS_STATUS_CBUS_HPD;
			cbus_stat ^= BIT_CBUS_STATUS_CBUS_HPD;
		}
		ctx->cbus_status = cbus_stat;
	}

	if (stat & BIT_CBUS_MSC_MR_WRITE_STAT)
		sii8620_msc_mr_write_stat(ctx);

	if (stat & BIT_CBUS_MSC_MR_SET_INT)
		sii8620_msc_mr_set_int(ctx);

	if (stat & BIT_CBUS_MSC_MT_DONE)
		sii8620_msc_mt_done(ctx);
}

static void sii8620_irq_coc(struct sii8620 *ctx)
{
	u8 stat = sii8620_readb(ctx, REG_COC_INTR);

	sii8620_write(ctx, REG_COC_INTR, stat);
}

static void sii8620_irq_merr(struct sii8620 *ctx)
{
	u8 stat = sii8620_readb(ctx, REG_CBUS_INT_1);

	sii8620_write(ctx, REG_CBUS_INT_1, stat);
}

static void sii8620_irq_edid(struct sii8620 *ctx)
{
	u8 stat = sii8620_readb(ctx, REG_INTR9);

	if (stat & BIT_INTR9_DEVCAP_DONE) {
		ctx->mt_done = 1;
		ctx->mt_ready = 1;
	}

	sii8620_write(ctx, REG_INTR9, stat);
}

static void sii8620_scdt_high(struct sii8620 *ctx)
{
	sii8620_write_seq(ctx,
		REG_INTR8_MASK, BIT_CEA_NEW_AVI | BIT_CEA_NEW_VSI,
		REG_TPI_SC, VAL_TPI_SC_TPI_OUTPUT_MODE_0_HDMI,
	);
}

static void sii8620_scdt_low(struct sii8620 *ctx)
{
	sii8620_write(ctx, REG_TMDS_CSTAT_P3,
		       BIT_TMDS_CSTAT_P3_SCDT_CLR_AVI_DIS
		       | BIT_TMDS_CSTAT_P3_CLR_AVI);

	sii8620_stop_video(ctx);

	sii8620_write(ctx, REG_INTR8_MASK, 0);
}

static void sii8620_irq_scdt(struct sii8620 *ctx)
{
	u8 stat = sii8620_readb(ctx, REG_INTR5);

	if (stat & BIT_INTR_SCDT_CHANGE) {
		u8 cstat = sii8620_readb(ctx, REG_TMDS_CSTAT_P3);

		if (cstat & BIT_TMDS_CSTAT_P3_SCDT)
			sii8620_scdt_high(ctx);
		else
			sii8620_scdt_low(ctx);
	}

	sii8620_write(ctx, REG_INTR5, stat);
}

static void sii8620_new_vsi(struct sii8620 *ctx)
{
	u8 vsif[11];

	sii8620_write(ctx, REG_RX_HDMI_CTRL2,
		       VAL_RX_HDMI_CTRL2_DEFVAL
		       | VAL_RX_HDMI_CTRL2_VSI_MON_SEL_VSI);
	sii8620_read_buf(ctx, REG_RX_HDMI_MON_PKT_HEADER1, vsif, ARRAY_SIZE(vsif));
}

static void sii8620_new_avi(struct sii8620 *ctx)
{
	sii8620_write(ctx, REG_RX_HDMI_CTRL2, VAL_RX_HDMI_CTRL2_DEFVAL);
	sii8620_read_buf(ctx, REG_RX_HDMI_MON_PKT_HEADER1, ctx->avif,
			 ARRAY_SIZE(ctx->avif));
}

static void sii8620_irq_infr(struct sii8620 *ctx)
{
	u8 stat = sii8620_readb(ctx, REG_INTR8)
		& (BIT_CEA_NEW_VSI | BIT_CEA_NEW_AVI);

	sii8620_write(ctx, REG_INTR8, stat);

	if (stat & BIT_CEA_NEW_VSI)
		sii8620_new_vsi(ctx);

	if (stat & BIT_CEA_NEW_AVI)
		sii8620_new_avi(ctx);

	if (stat & (BIT_CEA_NEW_VSI | BIT_CEA_NEW_AVI))
		sii8620_start_video(ctx);
}

/* endian agnostic, non-volatile version of test_bit */
static bool sii8620_test_bit(unsigned int nr, const u8 *addr)
{
	return 1 & (addr[nr / BITS_PER_BYTE] >> (nr % BITS_PER_BYTE));
}

static irqreturn_t sii8620_irq_thread(int irq, void *data)
{
	static const struct {
		int bit;
		void (*handler)(struct sii8620 *ctx);
	} irq_vec[] = {
		{ BIT_FAST_INTR_STAT_DISC, sii8620_irq_disc },
		{ BIT_FAST_INTR_STAT_G2WB, sii8620_irq_g2wb },
		{ BIT_FAST_INTR_STAT_COC, sii8620_irq_coc },
		{ BIT_FAST_INTR_STAT_MSC, sii8620_irq_msc },
		{ BIT_FAST_INTR_STAT_MERR, sii8620_irq_merr },
		{ BIT_FAST_INTR_STAT_EDID, sii8620_irq_edid },
		{ BIT_FAST_INTR_STAT_SCDT, sii8620_irq_scdt },
		{ BIT_FAST_INTR_STAT_INFR, sii8620_irq_infr },
	};
	struct sii8620 *ctx = data;
	u8 stats[LEN_FAST_INTR_STAT];
	int i;

	mutex_lock(&ctx->lock);

	sii8620_read_buf(ctx, REG_FAST_INTR_STAT, stats, ARRAY_SIZE(stats));
	for (i = 0; i < ARRAY_SIZE(irq_vec); ++i)
		if (sii8620_test_bit(irq_vec[i].bit, stats))
			irq_vec[i].handler(ctx);

	sii8620_msc_work(ctx);

	mutex_unlock(&ctx->lock);

	return IRQ_HANDLED;
}

static void sii8620_cable_in(struct sii8620 *ctx)
{
	struct device *dev = ctx->dev;
	u8 ver[5];
	int ret;

	sii8620_hw_on(ctx);
	clk_prepare_enable(ctx->clk_xtal);
	sii8620_hw_reset(ctx);

	sii8620_read_buf(ctx, REG_VND_IDL, ver, ARRAY_SIZE(ver));
	ret = sii8620_clear_error(ctx);
	if (ret) {
		dev_err(dev, "Error accessing I2C bus, %d.\n", ret);
		return;
	}

	dev_info(dev, "ChipID %02x%02x:%02x%02x rev %02x.\n", ver[1], ver[0],
		 ver[3], ver[2], ver[4]);

	sii8620_write(ctx, REG_DPD,
			BIT_DPD_PWRON_PLL | BIT_DPD_PDNTX12 | BIT_DPD_OSC_EN);

	sii8620_xtal_set_rate(ctx);
	sii8620_disconnect(ctx);

	sii8620_write_seq(ctx,
		REG_MHL_CBUS_CTL0, VAL_MHL_CBUS_CTL0_CBUS_DRV_SEL_STRONG
			| VAL_MHL_CBUS_CTL0_CBUS_RGND_VBIAS_734,
		REG_MHL_CBUS_CTL1, VAL_MHL_CBUS_CTL1_1115_OHM,
		REG_DPD, BIT_DPD_PWRON_PLL | BIT_DPD_PDNTX12 | BIT_DPD_OSC_EN,
	);

	enable_irq(ctx->irq);
}

static int sii8620_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sii8620 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	mutex_init(&ctx->lock);
	INIT_LIST_HEAD(&ctx->mt_queue);

	ctx->clk_xtal = devm_clk_get(dev, "xtal");
	if (IS_ERR(ctx->clk_xtal)) {
		dev_err(dev, "failed to get xtal clock from DT\n");
		return PTR_ERR(ctx->clk_xtal);
	}

	ctx->gpio_int = devm_gpiod_get(dev, "int", GPIOD_ASIS);
	if (IS_ERR(ctx->gpio_int)) {
		dev_err(dev, "failed to get int gpio from DT\n");
		return PTR_ERR(ctx->gpio_int);
	}

	ctx->irq = gpiod_to_irq(ctx->gpio_int);
	if (ctx->irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return ctx->irq;
	}
	irq_set_status_flags(ctx->irq, IRQ_NOAUTOEN);
	ret = devm_request_threaded_irq(dev, ctx->irq, NULL, sii8620_irq_thread,
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "sii8620", ctx);

	ctx->gpio_reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->gpio_reset)) {
		dev_err(dev, "failed to get reset gpio from DT\n");
		return PTR_ERR(ctx->gpio_reset);
	}

	ctx->supplies[0].supply = "cvcc10";
	ctx->supplies[1].supply = "iovcc18";
	ret = devm_regulator_bulk_get(dev, 2, ctx->supplies);
	if (ret)
		return ret;

	i2c_set_clientdata(client, ctx);

	sii8620_cable_in(ctx);

	return 0;
}

static int sii8620_remove(struct i2c_client *client)
{
	//struct sii8620 *ctx = i2c_get_clientdata(client);

	return 0;
}

static const struct of_device_id sii8620_dt_match[] = {
	{ .compatible = "sil,sii8620" },
	{ },
};
MODULE_DEVICE_TABLE(of, sii8620_dt_match);

static const struct i2c_device_id sii8620_id[] = {
	{ "SII8620", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, sii8620_id);
static struct i2c_driver sii8620_driver = {
	.driver = {
		.name	= "sii8620",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sii8620_dt_match),
	},
	.probe		= sii8620_probe,
	.remove		= sii8620_remove,
	.id_table = sii8620_id,
};

module_i2c_driver(sii8620_driver);
MODULE_LICENSE("GPL v2");
