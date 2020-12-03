/*
 * hsi_driver.h
 *
 * Header file for the HSI driver low level interface.
 *
 * Copyright (C) 2007-2008 Nokia Corporation. All rights reserved.
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Author: Carlos Chinea <carlos.chinea@nokia.com>
 * Author: Sebastien JAN <s-jan@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __HSI_DRIVER_H__
#define __HSI_DRIVER_H__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/notifier.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <mach/omap_hsi.h>
#include <linux/hsi_driver_if.h>

/* Channel states */
#define	HSI_CH_OPEN		0x01
#define HSI_CH_RX_POLL		0x10

/*
 * The number of channels handled by the driver in the ports, or the highest
 * port channel number (+1) used. (MAX:8 for SSI; 16 for HSI)
 * Reducing this value optimizes the driver memory footprint.
 */
#define HSI_PORT_MAX_CH		4

#define LOG_NAME		"OMAP HSI: "

/* SW strategies for FIFO mapping */
enum {
	HSI_FIFO_MAPPING_UNDEF = 0,
	HSI_FIFO_MAPPING_SSI, /* 8 FIFOs per port (SSI compatible mode) */
	HSI_FIFO_MAPPING_ALL_PORT1, /* ALL FIFOs mapped on 1st port */
};
#define HSI_FIFO_MAPPING_DEFAULT	HSI_FIFO_MAPPING_SSI

/* Device identifying constants */
enum {
	HSI_DRV_DEVICE_HSI,
	HSI_DRV_DEVICE_SSI
};

/**
 * struct hsi_data - HSI buffer descriptor
 * @addr: pointer to the buffer where to send or receive data
 * @size: size in words (32 bits) of the buffer
 * @lch: associated GDD (DMA) logical channel number, if any
 */
struct hsi_data {
	u32 *addr;
	unsigned int size;
	int lch;
};

/**
 * struct hsi_channel - HSI channel data
 * @read_data: Incoming HSI buffer descriptor
 * @write_data: Outgoing HSI buffer descriptor
 * @hsi_port: Reference to port where the channel belongs to
 * @flags: Tracks if channel has been open
 * @channel_number: HSI channel number
 * @rw_lock: Read/Write lock to serialize access to callback and hsi_device
 * @dev: Reference to the associated hsi_device channel
 * @write_done: Callback to signal TX completed.
 * @read_done: Callback to signal RX completed.
 * @port_event: Callback to signal port events (RX Error, HWBREAK, CAWAKE ...)
 */
struct hsi_channel {
	struct hsi_data read_data;
	struct hsi_data write_data;
	struct hsi_port *hsi_port;
	u8 flags;
	u8 channel_number;
	rwlock_t rw_lock;
	struct hsi_device *dev;
	void (*write_done)(struct hsi_device *dev, unsigned int size);
	void (*read_done)(struct hsi_device *dev, unsigned int size);
	void (*port_event)(struct hsi_device *dev, unsigned int event,
								void *arg);
};

/**
 * struct hsi_port - hsi port driver data
 * @hsi_channel: Array of channels in the port
 * @hsi_controller: Reference to the HSI controller
 * @port_number: port number
 * @max_ch: maximum number of channels supported on the port
 * @n_irq: HSI irq line use to handle interrupts (0 or 1)
 * @irq: IRQ number
 * @cawake_gpio: GPIO number for cawake line (-1 if none)
 * @cawake_gpio_irq: IRQ number for cawake gpio events
 * @counters_on: indicates if the HSR counters are in use or not
 * @reg_counters: stores the previous counters values when deactivated
 * @lock: Serialize access to the port registers and internal data
 * @hsi_tasklet: Bottom half for interrupts
 * @cawake_tasklet: Bottom half for cawake events
 */
struct hsi_port {
	struct hsi_channel hsi_channel[HSI_PORT_MAX_CH];
	struct hsi_dev *hsi_controller;
	u8 flags;
	u8 port_number;
	u8 max_ch;
	u8 n_irq;
	int irq;
	int cawake_gpio;
	int cawake_gpio_irq;
	int counters_on;
	unsigned long reg_counters;
	spinlock_t lock; /* access to the port registers and internal data */
	struct tasklet_struct hsi_tasklet;
	struct tasklet_struct cawake_tasklet;
};

/**
 * struct hsi_dev - hsi controller driver data
 * @hsi_port: Array of hsi ports enabled in the controller
 * @id: HSI controller platform id number
 * @max_p: Number of ports enabled in the controller
 * @hsi_clk: Reference to the HSI custom clock
 * @base: HSI registers base virtual address
 * @phy_base: HSI registers base physical address
 * @lock: Serializes access to internal data and regs
 * @cawake_clk_enable: Tracks if a cawake event has enable the clocks
 * @gdd_irq: GDD (DMA) irq number
 * @fifo_mapping_strategy: Selected strategy for fifo to ports/channels mapping
 * @gdd_usecount: Holds the number of ongoning DMA transfers
 * @last_gdd_lch: Last used GDD logical channel
 * @gdd_chan-count: Number of available DMA channels on the device (must be ^2)
 * @set_min_bus_tput: (PM) callback to set minimun bus throuput
 * @clk_notifier_register: (PM) callabck for DVFS support
 * @clk_notifier_unregister: (PM) callabck for DVFS support
 * @hsi_nb: (PM) Notification block for DVFS notification chain
 * @hsi_gdd_tasklet: Bottom half for DMA transfers
 * @dir: debugfs base directory
 * @dev: Reference to the HSI platform device
 */
struct hsi_dev {
	struct hsi_port hsi_port[HSI_MAX_PORTS];
	int id;
	u8 max_p;
	struct clk *hsi_clk;
	void __iomem *base;
	unsigned long phy_base;
	spinlock_t lock; /* Serializes access to internal data and regs */
	unsigned int cawake_clk_enable:1;
	int gdd_irq;
	unsigned int fifo_mapping_strategy;
	unsigned int gdd_usecount;
	unsigned int last_gdd_lch;
	unsigned int gdd_chan_count;
	void (*set_min_bus_tput)(struct device *dev, u8 agent_id,
						unsigned long r);
	int (*clk_notifier_register)(struct clk *clk,
					struct notifier_block *nb);
	int (*clk_notifier_unregister)(struct clk *clk,
					struct notifier_block *nb);
	struct notifier_block hsi_nb;
	struct tasklet_struct hsi_gdd_tasklet;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dir;
#endif
	struct device *dev;
};

/* HSI Bus */
extern struct bus_type hsi_bus_type;

int hsi_port_event_handler(struct hsi_port *p, unsigned int event, void *arg);
int hsi_bus_init(void);
void hsi_bus_exit(void);
/* End HSI Bus */

void hsi_reset_ch_read(struct hsi_channel *ch);
void hsi_reset_ch_write(struct hsi_channel *ch);

int hsi_driver_read_interrupt(struct hsi_channel *hsi_channel, u32 *data);
int hsi_driver_write_interrupt(struct hsi_channel *hsi_channel, u32 *data);
int hsi_driver_read_dma(struct hsi_channel *hsi_channel, u32 *data,
			unsigned int count);
int hsi_driver_write_dma(struct hsi_channel *hsi_channel, u32 *data,
			unsigned int count);

void hsi_driver_cancel_write_interrupt(struct hsi_channel *ch);
void hsi_driver_disable_read_interrupt(struct hsi_channel *ch);
void hsi_driver_cancel_read_interrupt(struct hsi_channel *ch);
void hsi_driver_cancel_write_dma(struct hsi_channel *ch);
void hsi_driver_cancel_read_dma(struct hsi_channel *ch);

int hsi_driver_device_is_hsi(struct platform_device *dev);

int hsi_mpu_init(struct hsi_port *hsi_p, const char *irq_name);
void hsi_mpu_exit(struct hsi_port *hsi_p);

int hsi_gdd_init(struct hsi_dev *hsi_ctrl, const char *irq_name);
void hsi_gdd_exit(struct hsi_dev *hsi_ctrl);

int hsi_cawake_init(struct hsi_port *port, const char *irq_name);
void hsi_cawake_exit(struct hsi_port *port);

int hsi_fifo_get_id(struct hsi_dev *hsi_ctrl, unsigned int channel,
							unsigned int port);
int hsi_fifo_get_chan(struct hsi_dev *hsi_ctrl, unsigned int fifo,
				unsigned int *channel, unsigned int *port);
int __init hsi_fifo_mapping(struct hsi_dev *hsi_ctrl, unsigned int mtype);
long hsi_hst_bufstate_f_reg(struct hsi_dev *hsi_ctrl,
				unsigned int port, unsigned int channel);
long hsi_hsr_bufstate_f_reg(struct hsi_dev *hsi_ctrl,
				unsigned int port, unsigned int channel);
long hsi_hst_buffer_reg(struct hsi_dev *hsi_ctrl,
				unsigned int port, unsigned int channel);
long hsi_hsr_buffer_reg(struct hsi_dev *hsi_ctrl,
				unsigned int port, unsigned int channel);

#ifdef CONFIG_DEBUG_FS
int hsi_debug_init(void);
void hsi_debug_exit(void);
int hsi_debug_add_ctrl(struct hsi_dev *hsi_ctrl);
void hsi_debug_remove_ctrl(struct hsi_dev *hsi_ctrl);
#else
#define	hsi_debug_add_ctrl(hsi_ctrl)	0
#define	hsi_debug_remove_ctrl(hsi_ctrl)
#define	hsi_debug_init()		0
#define	hsi_debug_exit()
#endif /* CONFIG_DEBUG_FS */

static inline unsigned int hsi_cawake(struct hsi_port *port)
{
	return gpio_get_value(port->cawake_gpio);
}

static inline struct hsi_channel *ctrl_get_ch(struct hsi_dev *hsi_ctrl,
					unsigned int port, unsigned int channel)
{
	return &hsi_ctrl->hsi_port[port - 1].hsi_channel[channel];
}

/* HSI IO access */
static inline u32 hsi_inl(void __iomem *base, u32 offset)
{
	return inl((unsigned int) base + offset);
}

static inline void hsi_outl(u32 data, void __iomem *base, u32 offset)
{
	outl(data, (unsigned int) base + offset);
}

static inline void hsi_outl_or(u32 data, void __iomem *base, u32 offset)
{
	u32 tmp = hsi_inl(base, offset);
	hsi_outl((tmp | data), base, offset);
}

static inline void hsi_outl_and(u32 data, void __iomem *base, u32 offset)
{
	u32 tmp = hsi_inl(base, offset);
	hsi_outl((tmp & data), base, offset);
}

static inline u16 hsi_inw(void __iomem *base, u32 offset)
{
	return inw((unsigned int) base + offset);
}

static inline void hsi_outw(u16 data, void __iomem *base, u32 offset)
{
	outw(data, (unsigned int) base + offset);
}

static inline void hsi_outw_or(u16 data, void __iomem *base, u32 offset)
{
	u16 tmp = hsi_inw(base, offset);
	hsi_outw((tmp | data), base, offset);
}

static inline void hsi_outw_and(u16 data, void __iomem *base, u32 offset)
{
	u16 tmp = hsi_inw(base, offset);
	hsi_outw((tmp & data), base, offset);
}

#endif /* __HSI_DRIVER_H__ */
