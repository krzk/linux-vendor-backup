/* linux/drivers/video/exynos/exynos_mipi_dsi_common.c
 *
 * Samsung SoC MIPI-DSI common driver.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *
 * InKi Dae, <inki.dae@samsung.com>
 * Donghwa Lee, <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <video/mipi_display.h>
#include <video/exynos_mipi_dsim.h>

#include <mach/map.h>

#include "exynos_mipi_dsi_regs.h"
#include "exynos_mipi_dsi_lowlevel.h"
#include "exynos_mipi_dsi_common.h"

#define MIPI_FIFO_TIMEOUT	msecs_to_jiffies(60)
#define MIPI_RX_FIFO_READ_DONE  0x30800002
#define MIPI_RX_MAX_FIFO        256	/* 4byte x 64depth */
#define MHZ			(1000 * 1000)
#define FIN_HZ			(24 * MHZ)

#define DFIN_PLL_MIN_HZ		(6 * MHZ)
#define DFIN_PLL_MAX_HZ		(12 * MHZ)

#define DFVCO_MIN_HZ		(500 * MHZ)
#define DFVCO_MAX_HZ		(1000 * MHZ)

#define TRY_FIFO_CLEAR		(100)

#define MAKE_TX_HEADER(di, d0, d1)	(((d1) << 16) | ((d0) << 8) | \
					(((di) & 0x3f) << 0))
#define MAKE_RX_HEADER(di, d0)		(((d0) << 8) | ((di) << 0))

/* define DSI lane types. */
enum {
	DSIM_LANE_CLOCK = (1 << 0),
	DSIM_LANE_DATA0 = (1 << 1),
	DSIM_LANE_DATA1 = (1 << 2),
	DSIM_LANE_DATA2 = (1 << 3),
	DSIM_LANE_DATA3 = (1 << 4)
};

static unsigned int dpll_table[15] = {
	100, 120, 170, 220, 270,
	320, 390, 450, 510, 560,
	640, 690, 770, 870, 950
};

static void wait_for_data_lane_stop_state(struct mipi_dsim_device *dsim)
{
	struct mipi_dsim_config *cfg = dsim->dsim_config;
	unsigned int lane_mask = 0;
	unsigned int repeat = 500;
	unsigned int pos;

	for (pos = 0; pos < cfg->e_no_data_lane + 1; pos++)
		lane_mask |= 1 << pos;

	/* Wait until all data lanes would be stop state. */
	do {
		if ((readl(dsim->reg_base + EXYNOS_DSIM_STATUS) & lane_mask)
								== lane_mask)
			break;

		udelay(2);
	} while (repeat--);

	if (!repeat)
		dev_warn(dsim->dev, "data lane state is not stop.\n");
}

void wait_for_fifo_empty_state(struct mipi_dsim_device *dsim)
{
	unsigned long fifo_empty;
	unsigned int repeat = 500;

	fifo_empty = SFR_PAYLOAD_EMPTY | SFR_HEADER_EMPTY;

	do {
		if ((readl(dsim->reg_base + EXYNOS_DSIM_FIFOCTRL) & fifo_empty)
								== fifo_empty)
			break;

		udelay(2);
	} while (repeat--);

	if (!repeat)
		dev_warn(dsim->dev, "Waiting fifo empty has been timed out.\n");
}

static void atomic_wait_for_rx_done(struct mipi_dsim_device *dsim)
{
	unsigned int repeat = 500;

	do {
		if ((readl(dsim->reg_base + EXYNOS_DSIM_INTSRC) &
					INTMSK_RX_DONE) == INTMSK_RX_DONE)
			break;

		udelay(2);
	} while (repeat--);

	if (!repeat)
		dev_warn(dsim->dev, "RX done has been timed out.\n");
}

static void wait_for_ulps_exit_state(struct mipi_dsim_device *dsim)
{
	struct mipi_dsim_config *cfg = dsim->dsim_config;
	unsigned int mask = 0, pos, repeat = 500;

	for (pos = 0; pos < cfg->e_no_data_lane + 1; pos++)
		mask |= 1 << pos;

	mask = DSIM_ULPS_DAT_SHIFT(mask) | DSIM_ULPS_CLK;

	do {
		if (!(readl(dsim->reg_base + EXYNOS_DSIM_STATUS) & mask))
			break;

		udelay(2);
	} while (repeat--);

	if (!repeat)
		dev_warn(dsim->dev, "Data lane or clock is in ULPS.\n");
}

void exynos_mipi_dsi_set_ulps_enable(struct mipi_dsim_device *dsim,
					unsigned int enable)
{
	unsigned int reg = readl(dsim->reg_base + EXYNOS_DSIM_ESCMODE);

	if (enable) {
		dsim->state = DSIM_STATE_ULPS;

		/* request to enter for clock & data lanes */
		reg |= DSIM_TX_ULPS_CLK | DSIM_TX_ULPS_DATA;
	} else {
		/* request to exit for clock & data lanes */
		reg |= DSIM_TX_ULPS_CLK_EXIT | DSIM_TX_ULPS_DATA_EXIT;

		writel(reg, dsim->reg_base + EXYNOS_DSIM_ESCMODE);

		wait_for_ulps_exit_state(dsim);

		/* clear for all clocks & data lanes manually */
		reg &= ~DSIM_TX_ULPS_MASK;

		dsim->state = DSIM_STATE_LP;
	}

	writel(reg, dsim->reg_base + EXYNOS_DSIM_ESCMODE);
}

/*
 * write long packet to mipi dsi slave
 * @dsim: mipi dsim device structure.
 * @data0: packet data to send.
 * @data1: size of packet data
 */
static void exynos_mipi_dsi_long_data_wr(struct mipi_dsim_device *dsim,
		const unsigned char *data0, unsigned int data_size)
{
	unsigned int data_cnt = 0, payload = 0;

	/* in case that data count is more then 4 */
	for (data_cnt = 0; data_cnt < data_size; data_cnt += 4) {
		/*
		 * after sending 4bytes per one time,
		 * send remainder data less then 4.
		 */
		if ((data_size - data_cnt) < 4) {
			if ((data_size - data_cnt) == 3) {
				payload = data0[data_cnt] |
				    data0[data_cnt + 1] << 8 |
					data0[data_cnt + 2] << 16;
			dev_dbg(dsim->dev, "count = 3 payload = %x, %x %x %x\n",
				payload, data0[data_cnt],
				data0[data_cnt + 1],
				data0[data_cnt + 2]);
			} else if ((data_size - data_cnt) == 2) {
				payload = data0[data_cnt] |
					data0[data_cnt + 1] << 8;
			dev_dbg(dsim->dev,
				"count = 2 payload = %x, %x %x\n", payload,
				data0[data_cnt],
				data0[data_cnt + 1]);
			} else if ((data_size - data_cnt) == 1) {
				payload = data0[data_cnt];
			}

			exynos_mipi_dsi_wr_tx_data(dsim, payload);
		/* send 4bytes per one time. */
		} else {
			payload = data0[data_cnt] |
				data0[data_cnt + 1] << 8 |
				data0[data_cnt + 2] << 16 |
				data0[data_cnt + 3] << 24;

			dev_dbg(dsim->dev,
				"count = 4 payload = %x, %x %x %x %x\n",
				payload, *(u8 *)(data0 + data_cnt),
				data0[data_cnt + 1],
				data0[data_cnt + 2],
				data0[data_cnt + 3]);

			exynos_mipi_dsi_wr_tx_data(dsim, payload);
		}
	}
}

int dsim_wr_data_transfer(struct mipi_dsim_device *dsim,
			struct exynos_dsi_command *cmd)
{
	unsigned int data_id;
	const unsigned char *data0;
	unsigned int data_size;
	unsigned int tx_header = 0;
	unsigned long flags;
	int ret = 0;

	data_id = cmd->type & (~DSIM_WRITE_CMD);
	data0 = cmd->tx_data;
	data_size = cmd->len;

	/*
	 * Make sure that data lanes become stop
	 * before transferring command data.
	 */
	wait_for_data_lane_stop_state(dsim);

	spin_lock_irqsave(&dsim->atomic_lock, flags);

	switch (data_id) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
		tx_header = MAKE_TX_HEADER(data_id, data0[0], data0[1]);
		writel_relaxed(tx_header, dsim->reg_base + EXYNOS_DSIM_PKTHDR);
		break;
	/* general command */
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		tx_header = MAKE_TX_HEADER(data_id, data0[0], data0[1]);
		writel_relaxed(tx_header, dsim->reg_base + EXYNOS_DSIM_PKTHDR);
		break;
	/* packet types for video data */
	case MIPI_DSI_V_SYNC_START:
	case MIPI_DSI_V_SYNC_END:
	case MIPI_DSI_H_SYNC_START:
	case MIPI_DSI_H_SYNC_END:
	case MIPI_DSI_END_OF_TRANSMISSION:
		break;
	/* long packet type and null packet */
	case MIPI_DSI_NULL_PACKET:
	case MIPI_DSI_BLANKING_PACKET:
		break;
	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
		/* if data count is less then 4, then send 3bytes data.  */
		if (data_size < 4) {
			unsigned int payload = 0;

			payload = data0[0] |
				data0[1] << 8 |
				data0[2] << 16;

			writel_relaxed(payload,
				dsim->reg_base + EXYNOS_DSIM_PAYLOAD);

			dev_dbg(dsim->dev, "count = %d payload = %x,%x %x %x\n",
				data_size, payload, data0[0],
				data0[1], data0[2]);

		/* in case that data count is more then 4 */
		} else
			exynos_mipi_dsi_long_data_wr(dsim, data0, data_size);

		tx_header = MAKE_TX_HEADER(data_id, data_size & 0xff,
						(data_size & 0xff00) >> 8);

		/* put data into header fifo */
		writel_relaxed(tx_header, dsim->reg_base + EXYNOS_DSIM_PKTHDR);
		break;
	/* packet typo for video data */
	case MIPI_DSI_PACKED_PIXEL_STREAM_16:
	case MIPI_DSI_PACKED_PIXEL_STREAM_18:
	case MIPI_DSI_PIXEL_STREAM_3BYTE_18:
	case MIPI_DSI_PACKED_PIXEL_STREAM_24:
		break;
	default:
		dev_warn(dsim->dev,
			"data id %x is not supported current DSI spec.\n",
			data_id);

		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&dsim->atomic_lock, flags);

	return ret;
}

static unsigned int exynos_mipi_dsi_long_data_rd(struct mipi_dsim_device *dsim,
		unsigned int req_size, unsigned int rx_data, u8 *rx_buf)
{
	unsigned int rcv_pkt, i, j;
	u16 rxsize;

	/* for long packet */
	rxsize = (u16)((rx_data & 0x00ffff00) >> 8);
	dev_dbg(dsim->dev, "mipi dsi rx size : %d\n", rxsize);
	if (rxsize != req_size) {
		dev_err(dsim->dev,
			"received size mismatch received: %d, requested: %d\n",
			rxsize, req_size);
		return 0;
	}

	for (i = 0; i < (rxsize >> 2); i++) {
		rcv_pkt = exynos_mipi_dsi_rd_rx_fifo(dsim);
		dev_dbg(dsim->dev, "received pkt : %08x\n", rcv_pkt);
		for (j = 0; j < 4; j++) {
			rx_buf[(i * 4) + j] =
					(u8)(rcv_pkt >> (j * 8)) & 0xff;
			dev_dbg(dsim->dev, "received value : %02x\n",
					(rcv_pkt >> (j * 8)) & 0xff);
		}
	}
	if (rxsize % 4) {
		rcv_pkt = exynos_mipi_dsi_rd_rx_fifo(dsim);
		dev_dbg(dsim->dev, "received pkt : %08x\n", rcv_pkt);
		for (j = 0; j < (rxsize % 4); j++) {
			rx_buf[(i * 4) + j] =
					(u8)(rcv_pkt >> (j * 8)) & 0xff;
			dev_dbg(dsim->dev, "received value : %02x\n",
					(rcv_pkt >> (j * 8)) & 0xff);
		}
	}

	return rxsize;
}

static inline unsigned int exynos_mipi_dsi_response_size(unsigned int req_size)
{
	switch (req_size) {
	case 1:
		return MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE;
	case 2:
		return MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE;
	default:
		return MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE;
	}
}

static inline unsigned int exynos_mipi_dsi_dcs_response_size(unsigned int req_size)
{
	switch (req_size) {
	case 1:
		return MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE;
	case 2:
		return MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE;
	default:
		return MIPI_DSI_RX_DCS_LONG_READ_RESPONSE;
	}
}

int dsim_rd_data_transfer(struct mipi_dsim_device *dsim,
			struct exynos_dsi_command *cmd)
{
	unsigned int data_id;
	unsigned int data0;
	unsigned int req_size;
	unsigned char *rx_buf;
	unsigned int rx_data, rcv_pkt;
	unsigned int rx_header = 0;
	unsigned int fifo_depth_len;
	unsigned long flags;
	u8 response = 0;
	u16 rxsize;

	data_id = cmd->type & (~DSIM_READ_CMD);
	data0 = cmd->rx_data;
	req_size = cmd->len;

	rx_buf = cmd->rx_payload;

	/*
	 * Make sure that data lanes become stop
	 * before transferring command data.
	 */
	wait_for_data_lane_stop_state(dsim);

	spin_lock_irqsave(&dsim->atomic_lock, flags);

	switch (data_id) {
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
		response = exynos_mipi_dsi_response_size(req_size);
		break;
	case MIPI_DSI_DCS_READ:
		response = exynos_mipi_dsi_dcs_response_size(req_size);
		break;
	default:
		spin_unlock_irqrestore(&dsim->atomic_lock, flags);
		dev_warn(dsim->dev,
			"data id %x is not supported current DSI spec.\n",
			data_id);

		return -EINVAL;
	}

	rx_header = MAKE_RX_HEADER(MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
					req_size);
	writel_relaxed(rx_header, dsim->reg_base + EXYNOS_DSIM_PKTHDR);

	rx_header = MAKE_RX_HEADER(data_id, data0);
	writel_relaxed(rx_header, dsim->reg_base + EXYNOS_DSIM_PKTHDR);

	atomic_wait_for_rx_done(dsim);

	rx_data = readl(dsim->reg_base + EXYNOS_DSIM_RXFIFO);
	if ((u8)(rx_data & 0xff) != response) {
		dev_err(dsim->dev,
			"mipi dsi wrong response rx_data : %x, response:%x\n",
			rx_data, response);
		goto clear_rx_fifo;
	}

	if (req_size <= 2) {
		unsigned int i;

		/* for short packet */
		for (i = 0; i < req_size; i++)
			rx_buf[i] = (rx_data >> (8 + (i * 8))) & 0xff;
		rxsize = req_size;
	} else {
		/* for long packet */
		rxsize = exynos_mipi_dsi_long_data_rd(dsim, req_size, rx_data,
							rx_buf);
		if (rxsize != req_size)
			goto clear_rx_fifo;
	}

	rcv_pkt = readl(dsim->reg_base + EXYNOS_DSIM_RXFIFO);
	if (rcv_pkt != MIPI_RX_FIFO_READ_DONE) {
		dev_err(dsim->dev,
			"Can't found RX FIFO READ DONE FLAG : %x\n", rcv_pkt);
		goto clear_rx_fifo;
	}

	spin_unlock_irqrestore(&dsim->atomic_lock, flags);

	return rxsize;

clear_rx_fifo:
	fifo_depth_len = MIPI_RX_MAX_FIFO >> 2;

	do {
		rcv_pkt = readl(dsim->reg_base + EXYNOS_DSIM_RXFIFO);
		if (rcv_pkt == MIPI_RX_FIFO_READ_DONE)
			break;

		dev_dbg(dsim->dev,
				"mipi dsi clear rx fifo : %08x\n", rcv_pkt);
	} while (--fifo_depth_len);

	spin_unlock_irqrestore(&dsim->atomic_lock, flags);

	return 0;
}

int dsim_data_transfer(struct mipi_dsim_device *dsim)
{
	struct exynos_dsi_command *cmd;
	unsigned long bus_lock_flags, cmd_lock_flags;
	int ret = 0;

	spin_lock_irqsave(&dsim->cmd_lock, cmd_lock_flags);
	if (list_empty(&dsim->cmd_list))
		goto out;

	/*
	 * Do not tranfer command to lcd panel if in trigger.
	 * This command will be transferred by mipi dsi interrupt handler
	 * when frame done interrupt occurs.
	 */
	if (atomic_read(&dsim->in_trigger))
		goto out;

again:
	spin_lock_irqsave(&dsim->bus_lock, bus_lock_flags);

	cmd = list_first_entry(&dsim->cmd_list, struct exynos_dsi_command,
				list);
	list_del_init(&cmd->list);

	if (cmd->type & DSIM_WRITE_CMD)
		ret = dsim_wr_data_transfer(dsim, cmd);
	else if (cmd->type & DSIM_READ_CMD)
		ret = dsim_rd_data_transfer(dsim, cmd);
	else if (cmd->type & DSIM_BEGIN_CMD_SET) {
		/*
		 * Exit from ULPS mode if there was a cmd transfer request while
		 * in ULPS mode.
		 */
		if (dsim->state == DSIM_STATE_ULPS) {
			exynos_mipi_dsi_pll_on(dsim, 1);
			exynos_mipi_dsi_set_ulps_enable(dsim, 0);
		}
		/* Set HS mode to transfer cmd data set and keep it till end */
		exynos_mipi_dsi_set_hs_enable(dsim, 1);

		atomic_inc(&dsim->bus_cmd_req_cnt);
		ret = 0;
	} else if (cmd->type & DSIM_END_CMD_SET) {
		/*
		 * Make sure that SFR FIFO becomes empty before transferring
		 * img data
		 */
		wait_for_fifo_empty_state(dsim);

		atomic_dec(&dsim->bus_cmd_req_cnt);
		ret = 0;
	} else
		ret = -EINVAL;

	kfree(cmd);

	spin_unlock_irqrestore(&dsim->bus_lock, bus_lock_flags);

	if (!list_empty(&dsim->cmd_list))
		goto again;

out:
	/* Keep HS mode during transferring img data or cmd data set */
	if ((!atomic_read(&dsim->in_trigger)) &&
					!atomic_read(&dsim->bus_cmd_req_cnt))
		exynos_mipi_dsi_set_hs_enable(dsim, 0);

	spin_unlock_irqrestore(&dsim->cmd_lock, cmd_lock_flags);

	return ret;
}

int exynos_mipi_dsi_atomic_wr_data(struct mipi_dsim_device *dsim,
						unsigned int data_id,
						const unsigned char *data0,
						unsigned int data_size)
{
	unsigned int tx_header = 0;
	unsigned long flags;
	int ret = 0;

	/*
	 * Make sure that data lanes become stop
	 * before transferring command data.
	 */
	wait_for_data_lane_stop_state(dsim);

	spin_lock_irqsave(&dsim->atomic_lock, flags);

	switch (data_id) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
		tx_header = MAKE_TX_HEADER(data_id, data0[0], data0[1]);
		writel_relaxed(tx_header, dsim->reg_base + EXYNOS_DSIM_PKTHDR);
		break;
	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
		/* if data count is less then 4, then send 3bytes data.  */
		if (data_size < 4) {
			unsigned int payload = 0;

			payload = data0[0] | data0[1] << 8 | data0[2] << 16;

			writel_relaxed(payload,
					dsim->reg_base + EXYNOS_DSIM_PAYLOAD);

			dev_dbg(dsim->dev, "count = %d payload = %x,%x %x %x\n",
				data_size, payload, data0[0],
				data0[1], data0[2]);

		/* in case that data count is more then 4 */
		} else
			exynos_mipi_dsi_long_data_wr(dsim, data0, data_size);

		tx_header = MAKE_TX_HEADER(data_id, data_size & 0xff,
						(data_size & 0xff00) >> 8);

		/* put data into header fifo */
		writel_relaxed(tx_header, dsim->reg_base + EXYNOS_DSIM_PKTHDR);
		break;
	default:
		dev_warn(dsim->dev,
			"data id %x is not supported current DSI spec.\n",
			data_id);
		ret = -EINVAL;
	}

	spin_unlock_irqrestore(&dsim->atomic_lock, flags);

	return ret;
}

int exynos_mipi_dsi_begin_data_set(struct mipi_dsim_device *dsim)
{
	struct exynos_dsi_command *cmd;
	unsigned long flags;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->type = DSIM_BEGIN_CMD_SET;

	spin_lock_irqsave(&dsim->cmd_lock, flags);
	list_add_tail(&cmd->list, &dsim->cmd_list);
	spin_unlock_irqrestore(&dsim->cmd_lock, flags);

	return dsim_data_transfer(dsim);
}

int exynos_mipi_dsi_end_data_set(struct mipi_dsim_device *dsim)
{
	struct exynos_dsi_command *cmd;
	unsigned long flags;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->type = DSIM_END_CMD_SET;

	spin_lock_irqsave(&dsim->cmd_lock, flags);
	list_add_tail(&cmd->list, &dsim->cmd_list);
	spin_unlock_irqrestore(&dsim->cmd_lock, flags);

	return dsim_data_transfer(dsim);
}

int exynos_mipi_dsi_wr_data(struct mipi_dsim_device *dsim, unsigned int data_id,
						const unsigned char *data0,
						unsigned int data_size)
{
	struct exynos_dsi_command *cmd;
	unsigned long flags;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->type = data_id | DSIM_WRITE_CMD;
	cmd->tx_data = data0;
	cmd->len = data_size;

	spin_lock_irqsave(&dsim->cmd_lock, flags);
	list_add_tail(&cmd->list, &dsim->cmd_list);
	spin_unlock_irqrestore(&dsim->cmd_lock, flags);

	return dsim_data_transfer(dsim);
}

int exynos_mipi_dsi_rd_data(struct mipi_dsim_device *dsim, unsigned int data_id,
				unsigned int data0, unsigned int req_size,
				u8 *rx_buf)
{
	struct exynos_dsi_command *cmd;
	unsigned long flags;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->type = data_id | DSIM_READ_CMD;
	cmd->rx_data = data0;
	cmd->len = req_size;
	cmd->rx_payload = rx_buf;

	spin_lock_irqsave(&dsim->cmd_lock, flags);
	list_add_tail(&cmd->list, &dsim->cmd_list);
	spin_unlock_irqrestore(&dsim->cmd_lock, flags);

	return dsim_data_transfer(dsim);
}

int exynos_mipi_dsi_pll_on(struct mipi_dsim_device *dsim,
				unsigned int enable)
{
	int sw_timeout;

	if (enable) {
		sw_timeout = 1000;

		exynos_mipi_dsi_enable_pll(dsim, 1);
		while (1) {
			sw_timeout--;
			if (exynos_mipi_dsi_is_pll_stable(dsim))
				return 0;
			if (sw_timeout == 0)
				return -EINVAL;
		}
	} else
		exynos_mipi_dsi_enable_pll(dsim, 0);

	return 0;
}

static unsigned long exynos_mipi_dsi_change_pll(struct mipi_dsim_device *dsim,
	unsigned int pre_divider, unsigned int main_divider,
	unsigned int scaler)
{
	unsigned long dfin_pll, dfvco, dpll_out;
	unsigned int i, freq_band = 0xf;

	dfin_pll = (FIN_HZ / pre_divider);

	/******************************************************
	 *	Serial Clock(=ByteClk X 8)	FreqBand[3:0] *
	 ******************************************************
	 *	~ 99.99 MHz			0000
	 *	100 ~ 119.99 MHz		0001
	 *	120 ~ 159.99 MHz		0010
	 *	160 ~ 199.99 MHz		0011
	 *	200 ~ 239.99 MHz		0100
	 *	140 ~ 319.99 MHz		0101
	 *	320 ~ 389.99 MHz		0110
	 *	390 ~ 449.99 MHz		0111
	 *	450 ~ 509.99 MHz		1000
	 *	510 ~ 559.99 MHz		1001
	 *	560 ~ 639.99 MHz		1010
	 *	640 ~ 689.99 MHz		1011
	 *	690 ~ 769.99 MHz		1100
	 *	770 ~ 869.99 MHz		1101
	 *	870 ~ 949.99 MHz		1110
	 *	950 ~ 1000 MHz			1111
	 ******************************************************/
	if (dfin_pll < DFIN_PLL_MIN_HZ || dfin_pll > DFIN_PLL_MAX_HZ) {
		dev_warn(dsim->dev, "fin_pll range should be 6MHz ~ 12MHz\n");
		exynos_mipi_dsi_enable_afc(dsim, 0, 0);
	} else {
		if (dfin_pll < 7 * MHZ)
			exynos_mipi_dsi_enable_afc(dsim, 1, 0x1);
		else if (dfin_pll < 8 * MHZ)
			exynos_mipi_dsi_enable_afc(dsim, 1, 0x0);
		else if (dfin_pll < 9 * MHZ)
			exynos_mipi_dsi_enable_afc(dsim, 1, 0x3);
		else if (dfin_pll < 10 * MHZ)
			exynos_mipi_dsi_enable_afc(dsim, 1, 0x2);
		else if (dfin_pll < 11 * MHZ)
			exynos_mipi_dsi_enable_afc(dsim, 1, 0x5);
		else
			exynos_mipi_dsi_enable_afc(dsim, 1, 0x4);
	}

	dfvco = dfin_pll * main_divider;
	dev_dbg(dsim->dev, "dfvco = %lu, dfin_pll = %lu, main_divider = %d\n",
				dfvco, dfin_pll, main_divider);
	if (dfvco < DFVCO_MIN_HZ || dfvco > DFVCO_MAX_HZ)
		dev_warn(dsim->dev, "fvco range should be 500MHz ~ 1000MHz\n");

	dpll_out = dfvco / (1 << scaler);
	dev_dbg(dsim->dev, "dpll_out = %lu, dfvco = %lu, scaler = %d\n",
		dpll_out, dfvco, scaler);

	for (i = 0; i < ARRAY_SIZE(dpll_table); i++) {
		if (dpll_out < dpll_table[i] * MHZ) {
			freq_band = i;
			break;
		}
	}

	dev_dbg(dsim->dev, "freq_band = %d\n", freq_band);

	exynos_mipi_dsi_pll_freq(dsim, pre_divider, main_divider, scaler);

	exynos_mipi_dsi_hs_zero_ctrl(dsim, 0);
	exynos_mipi_dsi_prep_ctrl(dsim, 0);

	/* Freq Band */
	exynos_mipi_dsi_pll_freq_band(dsim, freq_band);

	/* Stable time */
	exynos_mipi_dsi_pll_stable_time(dsim,
		dsim->dsim_config->pll_stable_time);

	/* Enable PLL */
	dev_dbg(dsim->dev, "FOUT of mipi dphy pll is %luMHz\n",
		(dpll_out / MHZ));

	return dpll_out;
}

static int exynos_mipi_dsi_set_clock(struct mipi_dsim_device *dsim,
	unsigned int byte_clk_sel, unsigned int enable)
{
	unsigned int esc_div;
	unsigned long esc_clk_error_rate;
	unsigned long hs_clk = 0, byte_clk = 0, escape_clk = 0;

	if (enable) {
		dsim->e_clk_src = byte_clk_sel;

		/* Escape mode clock and byte clock source */
		exynos_mipi_dsi_set_byte_clock_src(dsim, byte_clk_sel);

		/* DPHY, DSIM Link : D-PHY clock out */
		if (byte_clk_sel == DSIM_PLL_OUT_DIV8) {
			hs_clk = exynos_mipi_dsi_change_pll(dsim,
				dsim->dsim_config->p, dsim->dsim_config->m,
				dsim->dsim_config->s);
			if (hs_clk == 0) {
				dev_err(dsim->dev,
					"failed to get hs clock.\n");
				return -EINVAL;
			}

			byte_clk = hs_clk / 8;
			exynos_mipi_dsi_enable_pll_bypass(dsim, 0);
			exynos_mipi_dsi_pll_on(dsim, 1);
		/* DPHY : D-PHY clock out, DSIM link : external clock out */
		} else if (byte_clk_sel == DSIM_EXT_CLK_DIV8) {
			dev_warn(dsim->dev, "this project is not support\n");
			dev_warn(dsim->dev,
				"external clock source for MIPI DSIM.\n");
		} else if (byte_clk_sel == DSIM_EXT_CLK_BYPASS) {
			dev_warn(dsim->dev, "this project is not support\n");
			dev_warn(dsim->dev,
				"external clock source for MIPI DSIM\n");
		}

		/* escape clock divider */
		esc_div = byte_clk / (dsim->dsim_config->esc_clk);
		dev_dbg(dsim->dev,
			"esc_div = %d, byte_clk = %lu, esc_clk = %lu\n",
			esc_div, byte_clk, dsim->dsim_config->esc_clk);
		if ((byte_clk / esc_div) >= (20 * MHZ) ||
				(byte_clk / esc_div) >
					dsim->dsim_config->esc_clk)
			esc_div += 1;

		escape_clk = byte_clk / esc_div;
		dev_dbg(dsim->dev,
			"escape_clk = %lu, byte_clk = %lu, esc_div = %d\n",
			escape_clk, byte_clk, esc_div);

		/* enable escape clock. */
		exynos_mipi_dsi_enable_byte_clock(dsim, 1);

		/* enable byte clk and escape clock */
		exynos_mipi_dsi_set_esc_clk_prs(dsim, 1, esc_div);
		/* escape clock on lane */
		exynos_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 1);

		dev_dbg(dsim->dev, "byte clock is %luMHz\n",
			(byte_clk / MHZ));
		dev_dbg(dsim->dev, "escape clock that user's need is %lu\n",
			(dsim->dsim_config->esc_clk / MHZ));
		dev_dbg(dsim->dev, "escape clock divider is %x\n", esc_div);
		dev_dbg(dsim->dev, "escape clock is %luMHz\n",
			((byte_clk / esc_div) / MHZ));

		if ((byte_clk / esc_div) > escape_clk) {
			esc_clk_error_rate = escape_clk /
				(byte_clk / esc_div);
			dev_warn(dsim->dev, "error rate is %lu over.\n",
				(esc_clk_error_rate / 100));
		} else if ((byte_clk / esc_div) < (escape_clk)) {
			esc_clk_error_rate = (byte_clk / esc_div) /
				escape_clk;
			dev_warn(dsim->dev, "error rate is %lu under.\n",
				(esc_clk_error_rate / 100));
		}
	} else {
		exynos_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 0);
		exynos_mipi_dsi_set_esc_clk_prs(dsim, 0, 0);

		/* disable escape clock. */
		exynos_mipi_dsi_enable_byte_clock(dsim, 0);

		if (byte_clk_sel == DSIM_PLL_OUT_DIV8)
			exynos_mipi_dsi_pll_on(dsim, 0);
	}

	return 0;
}

int exynos_mipi_dsi_init_dsim(struct mipi_dsim_device *dsim)
{
	switch (dsim->dsim_config->e_no_data_lane) {
	case DSIM_DATA_LANE_1:
		dsim->data_lane = DSIM_LANE_DATA0;
		break;
	case DSIM_DATA_LANE_2:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1;
		break;
	case DSIM_DATA_LANE_3:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2;
		break;
	case DSIM_DATA_LANE_4:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2 | DSIM_LANE_DATA3;
		break;
	default:
		dev_info(dsim->dev, "data lane is invalid.\n");
		return -EINVAL;
	};

	exynos_mipi_dsi_sw_reset(dsim);
	exynos_mipi_dsi_func_reset(dsim);

	exynos_mipi_dsi_dp_dn_swap(dsim, dsim->ddata->e_version, 0);

	return 0;
}

void exynos_mipi_dsi_init_interrupt(struct mipi_dsim_device *dsim)
{
	unsigned int src = 0;

	src = (INTSRC_RX_DATA_DONE | INTSRC_FRAME_DONE);
	exynos_mipi_dsi_set_interrupt(dsim, src, 1);

	src = 0;
	src = ~(INTMSK_RX_DONE | INTMSK_FRAME_DONE);
	exynos_mipi_dsi_set_interrupt_mask(dsim, src, 1);
}

int exynos_mipi_dsi_enable_frame_done_int(struct mipi_dsim_device *dsim,
	unsigned int enable)
{
	/* enable only frame done interrupt */
	exynos_mipi_dsi_set_interrupt_mask(dsim, INTMSK_FRAME_DONE, enable);

	return 0;
}

void exynos_mipi_dsi_standby(struct mipi_dsim_device *dsim, unsigned int enable)
{
	unsigned int reg;

	reg = readl(dsim->reg_base + EXYNOS_DSIM_MDRESOL);

	reg &= ~DSIM_MAIN_STAND_BY;

	if (enable)
		reg |= DSIM_MAIN_STAND_BY;

	writel_relaxed(reg, dsim->reg_base + EXYNOS_DSIM_MDRESOL);
}

int exynos_mipi_dsi_set_display_mode(struct mipi_dsim_device *dsim,
	struct mipi_dsim_config *dsim_config)
{
	struct mipi_dsim_platform_data *dsim_pd;
	struct fb_videomode *timing;

	dsim_pd = (struct mipi_dsim_platform_data *)dsim->pd;
	timing = (struct fb_videomode *)dsim_pd->lcd_panel_info;

	/* in case of VIDEO MODE (RGB INTERFACE), it sets polarities. */
	if (dsim_config->e_interface == (u32) DSIM_VIDEO) {
		if (dsim_config->auto_vertical_cnt == 0) {
			exynos_mipi_dsi_set_main_disp_vporch(dsim,
				dsim_config->cmd_allow,
				timing->upper_margin,
				timing->lower_margin);
			exynos_mipi_dsi_set_main_disp_hporch(dsim,
				timing->left_margin,
				timing->right_margin);
			exynos_mipi_dsi_set_main_disp_sync_area(dsim,
				timing->vsync_len,
				timing->hsync_len);
		}
	}

	exynos_mipi_dsi_set_main_disp_resol(dsim, timing->xres,
			timing->yres);

	exynos_mipi_dsi_display_config(dsim, dsim_config);

	dev_dbg(dsim->dev, "lcd panel ==> width = %d, height = %d\n",
			timing->xres, timing->yres);

	return 0;
}

int exynos_mipi_dsi_init_link(struct mipi_dsim_device *dsim)
{
	unsigned int time_out = 100;

	exynos_mipi_dsi_init_fifo_pointer(dsim, 0x1f);

	/* dsi configuration */
	exynos_mipi_dsi_init_config(dsim);
	exynos_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 1);
	exynos_mipi_dsi_enable_lane(dsim, dsim->data_lane, 1);

	/* set clock configuration */
	exynos_mipi_dsi_set_clock(dsim, dsim->dsim_config->e_byte_clk, 1);

	/* check clock and data lane state are stop state */
	while (!(exynos_mipi_dsi_is_lane_state(dsim))) {
		time_out--;
		if (time_out == 0) {
			dev_err(dsim->dev,
				"DSI Master is not stop state.\n");
			dev_err(dsim->dev,
				"Check initialization process\n");

			return -EINVAL;
		}
	}
	if (time_out != 0) {
		dev_dbg(dsim->dev,
			"DSI Master driver has been completed.\n");
		dev_dbg(dsim->dev, "DSI Master state is stop state\n");
	}

	/* BTA sequence counters */
	exynos_mipi_dsi_set_stop_state_counter(dsim,
		dsim->dsim_config->stop_holding_cnt);
	exynos_mipi_dsi_set_bta_timeout(dsim,
		dsim->dsim_config->bta_timeout);
	exynos_mipi_dsi_set_lpdr_timeout(dsim,
		dsim->dsim_config->rx_timeout);

	return 0;
}

int exynos_mipi_dsi_set_hs_enable(struct mipi_dsim_device *dsim,
					unsigned int enable)
{
	if (dsim->e_clk_src == DSIM_EXT_CLK_BYPASS) {
		dev_warn(dsim->dev, "clock source is external bypass.\n");
		return 0;
	}

	if (dsim->secure_mode) {
		pr_info("%s[%d]bypass:secure_mode\n", __func__, enable);
		return 0;
	}

	if (enable) {
		if (dsim->state == DSIM_STATE_HS)
			return 0;
		 /* set LCDC and CPU transfer mode to HS. */
		exynos_mipi_dsi_set_lcdc_transfer_mode(dsim, 0);
		exynos_mipi_dsi_set_cpu_transfer_mode(dsim, 0);
		exynos_mipi_dsi_enable_hs_clock(dsim, 1);

		dsim->state = DSIM_STATE_HS;
	} else {
		if (dsim->state == DSIM_STATE_LP)
			return 0;

		dsim->state = DSIM_STATE_LP;
		exynos_mipi_dsi_set_lcdc_transfer_mode(dsim, 1);
		exynos_mipi_dsi_set_cpu_transfer_mode(dsim, 1);
		exynos_mipi_dsi_enable_hs_clock(dsim, 0);
	}

	return 0;
}

int exynos_mipi_dsi_get_frame_done_status(struct mipi_dsim_device *dsim)
{
	return _exynos_mipi_dsi_get_frame_done_status(dsim);
}

int exynos_mipi_dsi_clear_frame_done(struct mipi_dsim_device *dsim)
{
	_exynos_mipi_dsi_clear_frame_done(dsim);

	return 0;
}

int exynos_mipi_dsi_fifo_clear(struct mipi_dsim_device *dsim,
				unsigned int val)
{
	int try = TRY_FIFO_CLEAR;

	exynos_mipi_dsi_sw_reset_release(dsim);
	exynos_mipi_dsi_func_reset(dsim);

	do {
		if (exynos_mipi_dsi_get_sw_reset_release(dsim)) {
			exynos_mipi_dsi_init_interrupt(dsim);
			dev_dbg(dsim->dev, "reset release done.\n");
			return 0;
		}
	} while (--try);

	dev_err(dsim->dev, "failed to clear dsim fifo.\n");
	return -EAGAIN;
}

irqreturn_t exynos_mipi_dsi_interrupt_handler(int irq, void *dev_id)
{
	unsigned int intsrc = 0;
	unsigned int intmsk = 0;
	struct mipi_dsim_device *dsim = NULL;

	dsim = dev_id;
	if (!dsim) {
		pr_err("%s:error: wrong parameter\n",
							__func__);
		return IRQ_HANDLED;
	}

	intsrc = exynos_mipi_dsi_read_interrupt(dsim);
	intmsk = exynos_mipi_dsi_read_interrupt_mask(dsim);

	intmsk = ~(intmsk) & intsrc;

	if (intsrc & INTMSK_RX_DONE)
		dev_dbg(dsim->dev, "MIPI INTMSK_RX_DONE\n");
	if (intsrc & INTMSK_FRAME_DONE) {
		unsigned long flags;
		/*
		 * FRAME DONE interrupt means that DSIM received last line
		 * from FIMD so wait for until the last line is transferred
		 * to LCD Panel.
		 */
		wait_for_data_lane_stop_state(dsim);

		spin_lock_irqsave(&dsim->bus_lock, flags);
		atomic_set(&dsim->in_trigger, 0);
		atomic_set(&dsim->bus_img_req_cnt, 0);
		spin_unlock_irqrestore(&dsim->bus_lock, flags);

		/* Transfer pended commands to lcd panel */
		dsim_data_transfer(dsim);

		dev_dbg(dsim->dev, "MIPI INTMSK_FRAME_DONE\n");
	}

	exynos_mipi_dsi_clear_interrupt(dsim, intmsk);

	return IRQ_HANDLED;
}

MODULE_AUTHOR("InKi Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samusung SoC MIPI-DSI common driver");
MODULE_LICENSE("GPL");
