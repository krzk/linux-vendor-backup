/*
 * bcm5935x.h
 * BCM5935X WPC Header
 *
 * Copyright (C) 2015 Samsung Electronics, Inc.
 *
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

#ifndef __BCM5935X_H
#define __BCM5935X_H __FILE__

#include <linux/battery/sec_charging_common.h>
#include <linux/battery/bcm5935x_reg.h>

enum wireless_protocol_type {
	WPC_PROTOCOL = 0,
	PMA_PROTOCOL,
};

enum vout_config_type {
	VOUT_UNKNOWN = 0,
	VOUT_4500mV,
	VOUT_4700mV,
	VOUT_4800mV,
};

enum fod_status {
	FOD_UNKNOWN = 0,
	FOD_DONE,
};

enum sec_wpc_detection_type {
	SEC_WPC_DETECT_MUIC_INT = 0,
	SEC_WPC_DETECT_WPC_INT,
};

struct bcm5935x_platform_data {
	int irq_gpio;
	enum sec_wpc_detection_type detect_type;
};

struct bcm5935x_dev {
	struct device *dev;
	struct i2c_client		*i2c_low; /* i2c addr: 0x20; under reg100 */
	struct i2c_client		*i2c_high; /* i2c addr: 0x24; over reg100 */
	struct bcm5935x_platform_data *pdata;

	struct mutex i2c_lock;
	struct mutex irq_lock;

	struct delayed_work isr_work;
	struct delayed_work init_work;
	/* register programming */
	int reg_addr;
	u8 reg_data[2];

	int irq;
	bool is_wpc_ready;
};

struct bcm5935x_wpc_info {
	struct device *dev;
	struct i2c_client *i2c; /* i2c addr: 0x20; under reg100 */
	struct mutex wpc_mutex;
	struct power_supply		psy_wpc;

	int status;
	int online;
	int voltage_now;
	enum vout_config_type vout_status;

	int fod_reg[8];
	int fod_cnt;
	enum fod_status fod_status;
	struct wake_lock fod_wake_lock;
	struct workqueue_struct *fod_wqueue;
	struct delayed_work fod_work;
	struct delayed_work full_work;

	enum wireless_protocol_type protocol;
	enum sec_wpc_detection_type detect_type;

	int ioctrl;
	int reg;
};

extern int bcm5935x_read_reg(struct i2c_client *i2c, int reg);

extern int bcm5935x_write_reg(struct i2c_client *i2c, int reg, int value);

extern irqreturn_t bcm5935x_irq_thread(int irq, void *data);

extern void bcm5935x_isr_work(struct work_struct *work);

extern void bcm5935x_wpc_send_EPT_packet(struct bcm5935x_wpc_info *wpc);

#endif /* __BCM5935X_H */
