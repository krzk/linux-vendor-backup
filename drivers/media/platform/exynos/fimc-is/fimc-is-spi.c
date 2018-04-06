/*
 * driver for FIMC-IS SPI
 *
 * Copyright (c) 2011, Samsung Electronics. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include "fimc-is-core.h"
#include "fimc-is-regs.h"


int fimc_is_spi_reset_by_core(struct spi_device *spi, void *buf, u32 rx_addr, size_t size)
{
	unsigned char req_rst[1] = { 0x99 };
	int ret;

	struct spi_transfer t_c;
	struct spi_transfer t_r;

	struct spi_message m;

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	t_c.tx_buf = req_rst;
	t_c.len = 1;
	t_c.cs_change = 0;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);

	ret = spi_sync(spi, &m);
	if (ret) {
		err("spi sync error - can't get device information");
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(fimc_is_spi_reset_by_core);

int fimc_is_spi_read_by_core(struct spi_device *spi, void *buf, u32 rx_addr, size_t size)
{
	unsigned char req_data[4] = { 0x03,  };
	int ret;

	struct spi_transfer t_c;
	struct spi_transfer t_r;

	struct spi_message m;

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	req_data[1] = (rx_addr & 0xFF0000) >> 16;
	req_data[2] = (rx_addr & 0xFF00) >> 8;
	req_data[3] = (rx_addr & 0xFF);

	t_c.tx_buf = req_data;
	t_c.len = 4;
	t_c.cs_change = 1;
	t_c.bits_per_word = 32;

	t_r.rx_buf = buf;
	t_r.len = size;
	t_r.cs_change = 0;
	t_r.bits_per_word = 32;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);
	spi_message_add_tail(&t_r, &m);

	spi->max_speed_hz = 48000000;
	ret = spi_sync(spi, &m);
	if (ret) {
		err("spi sync error - can't read data");
		return -EIO;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(fimc_is_spi_read_by_core);

int fimc_is_spi_read_module_id(struct spi_device *spi, void *buf, u16 rx_addr, size_t size)
{
	unsigned char req_data[4] = { 0x90,  };
	int ret;

	struct spi_transfer t_c;
	struct spi_transfer t_r;

	struct spi_message m;

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	req_data[1] = (rx_addr & 0xFF00) >> 8;
	req_data[2] = (rx_addr & 0xFF);

	t_c.tx_buf = req_data;
	t_c.len = 4;
	t_c.cs_change = 1;
	t_c.bits_per_word = 32;

	t_r.rx_buf = buf;
	t_r.len = size;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);
	spi_message_add_tail(&t_r, &m);

	spi->max_speed_hz = 48000000;
	ret = spi_sync(spi, &m);
	if (ret) {
		err("spi sync error - can't read data");
		return -EIO;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(fimc_is_spi_read_module_id);
