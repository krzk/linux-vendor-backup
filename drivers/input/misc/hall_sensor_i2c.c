/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Joong-Mock Shin <jmock.shin@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/miscdevice.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <linux/i2c/hall_sensor_i2c.h>
#include <linux/atomic.h>
#include <plat/gpio-cfg.h>
#include <linux/trm.h>

extern struct class *sec_class;
#define HALL_I2C_BUF_SIZE	17

static const char* hall_name_str[HALL_ALL+1]={"HALL_A", "HALL_B", "HALL_C", "HALL_ALL"};
static const char* hall_irq_str[HALL_ALL]={"tizen_detent_a", "tizen_detent_b", "tizen_detent_c"};

static int hall_set_operation_mode(struct device *dev, int mode, enum hall_id id);

/* ********************************************************* */
/* customer config */
/* ********************************************************* */
#define HALL_DBG_ENABLE			// for debugging
#define HALL_DETECTION_MODE			HALL_DETECTION_MODE_INTERRUPT
#define HALL_INTERRUPT_TYPE			HALL_VAL_INTSRS_INTTYPE_BESIDE//HALL_VAL_INTSRS_INTTYPE_WITHIN
#define HALL_SENSITIVITY_TYPE			HALL_VAL_INTSRS_SRS_10BIT_0_017mT
#define HALL_PERSISTENCE_COUNT		HALL_VAL_PERSINT_COUNT(1)
#define HALL_OPERATION_HIGH_FREQUENCY	HALL_VAL_OPF_FREQ_80HZ
#define HALL_OPERATION_LOW_FREQUENCY	HALL_VAL_OPF_FREQ_10HZ
#define HALL_OPERATION_RESOLUTION		HALL_VAL_OPF_BIT_10
#define HALL_MAX_THRESHOLD			511
#define HALL_MIN_THRESHOLD			-512
#define HALL_DETECT_RANGE_HIGH		-10
#define HALL_DETECT_RANGE_LOW		-100
#define HALL_DETECT_DETENT			((HALL_DETECT_RANGE_HIGH)+(HALL_DETECT_RANGE_LOW))/2
/* ********************************************************* */

static int hall_i2c_write(struct i2c_client* client, u8 reg, u8* wdata, u8 len, enum hall_id id)
{
	struct hall_ddata *hall = i2c_get_clientdata(client);
	u8  buf[HALL_I2C_BUF_SIZE];
	int rc, i;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr + id,
			.flags = 0,
			.len = len+1,
			.buf = buf,
		},
	};

	if (client == NULL ) {
		dev_err(&client->dev, "%s: i2c client is NULL.\n", __func__);
		return -ENODEV;
	}

	buf[0] = reg;

	if (len > HALL_I2C_BUF_SIZE) {
		dev_err(&client->dev, "%s: i2c buffer size must be less than %d",
				__func__, HALL_I2C_BUF_SIZE);
		return -EIO;
	}

	for( i=0 ; i<len; i++ ) {
		buf[i+1] = wdata[i];
	}

	rc = i2c_transfer(client->adapter, msg, 1);
	if (rc< 0) {
		dev_err(&client->dev, "%s: i2c_transfer was failed (%d)", __func__, rc);
		return rc;
	}

	if (len == 1) {
		switch(reg) {
		case HALL_REG_PERSINT:
			hall->reg.map.persint = wdata[0];
			break;
		case HALL_REG_INTSRS:
			hall->reg.map.intsrs = wdata[0];
			break;
		case HALL_REG_LTHL:
			hall->reg.map.lthl = wdata[0];
			break;
		case HALL_REG_LTHH:
			hall->reg.map.lthh = wdata[0];
			break;
		case HALL_REG_HTHL:
			hall->reg.map.hthl = wdata[0];
			break;
		case HALL_REG_HTHH:
			hall->reg.map.hthh = wdata[0];
			break;
		case HALL_REG_I2CDIS:
			hall->reg.map.i2cdis = wdata[0];
			break;
		case HALL_REG_SRST:
			hall->reg.map.srst = wdata[0];
			msleep(1);
			break;
		case HALL_REG_OPF:
			hall->reg.map.opf = wdata[0];
			break;
		}
	}

	for(i=0; i<len; i++) {
		dev_dbg(&client->dev,
			"reg=0x%02X data=0x%02X", buf[0]+(u8)i, buf[i+1]);
	}

	return 0;
}

static int hall_i2c_read(struct i2c_client* client, u8 reg, u8* rdata, u8 len, enum hall_id id)
{
	int rc;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr + id,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr + id,
			.flags = I2C_M_RD,
			.len = len,
			.buf = rdata,
		},
	};

	if (client == NULL) {
		dev_err(&client->dev, "%s: client is NULL", __func__);
		return -ENODEV;
	}

	rc = i2c_transfer(client->adapter, msg, 2);
	if (rc<0) {
		dev_err(&client->dev, "%s: i2c_transfer was failed(%d)(%d)",  __func__, rc, id);
		return rc;
	}

	return 0;
}

static int hall_i2c_set_reg(struct i2c_client *client, u8 reg, u8 wdata, enum hall_id id)
{
	return hall_i2c_write(client, reg, &wdata, sizeof(wdata), id);
}

static int hall_i2c_set_all(struct i2c_client *client, u8 reg, u8 wdata)
{
	int id_cnt, err;

	for (id_cnt = HALL_A; id_cnt < HALL_ALL; id_cnt++) {
		err = hall_i2c_set_reg(client, reg, wdata, id_cnt);
		if (err){
			dev_err(&client->dev, "%s: i2c write failed (%d)(%d)", __func__, err, id_cnt);
			return err;
		}
	}

	return 0;
}

static int	hall_i2c_get_reg(struct i2c_client *client, u8 reg, u8* rdata, enum hall_id id)
{
	return hall_i2c_read(client, reg, rdata, 1, id);
}

void hall_convdata_short_to_2byte(u8 opf, short x, unsigned char *hbyte, unsigned char *lbyte)
{
	if ((opf & HALL_VAL_OPF_BIT_8) == HALL_VAL_OPF_BIT_8) {
		/* 8 bit resolution */
		if (x<-128) x=-128;
		else if (x>127) x=127;

		if (x>=0) {
			*lbyte = x & 0x7f;
		} else {
			*lbyte = ( (0x80 - (x*(-1))) & 0x7f ) | 0x80;
		}
		*hbyte = 0x00;
	} else {
		/* 10 bit resolution */
		if (x<-512) x=-512;
		else if (x>511) x=511;

		if (x>=0) {
			*lbyte = x & 0xff;
			*hbyte = (((x&0x100)>>8)&0x01) << 6;
		} else {
			*lbyte = (0x0200 - (x*(-1))) & 0xff;
			*hbyte = ((((0x0200 - (x*(-1))) & 0x100)>>8)<<6) | 0x80;
		}
	}
}

short hall_convdata_2byte_to_short(u8 opf, unsigned char hbyte, unsigned char lbyte)
{
	short x;

	if ( (opf & HALL_VAL_OPF_BIT_8) == HALL_VAL_OPF_BIT_8) {
		/* 8 bit resolution */
		x = lbyte & 0x7f;
		if (lbyte & 0x80) {
			x -= 0x80;
		}
	} else {
		/* 10 bit resolution */
		x = ( ( (hbyte & 0x40) >> 6) << 8 ) | lbyte;
		if (hbyte&0x80) {
			x -= 0x200;
		}
	}

	return x;
}

static void hall_set_debug(struct device *dev, int debug)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);

	atomic_set(&hall->atm.debug, debug);
}

static int hall_clear_interrupt(struct device *dev, enum hall_id id)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	int ret = 0;

	if (id == HALL_ALL) {
		ret = hall_i2c_set_all(hall->client, HALL_REG_PERSINT, hall->reg.map.persint | 0x01);
	} else {
		ret = hall_i2c_set_reg(hall->client, HALL_REG_PERSINT, hall->reg.map.persint | 0x01, id);
	}

	return ret;
}

static int hall_init_interrupt_threshold(struct device *dev, short raw, enum hall_id id)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	u8 lthh=0, lthl=0, hthh=0, hthl=0;
	int err = -1;

	hall_convdata_short_to_2byte(hall->reg.map.opf, HALL_MAX_THRESHOLD, &hthh, &hthl);
	hall_convdata_short_to_2byte(hall->reg.map.opf, hall->thrlow, &lthh, &lthl);

	err = hall_i2c_set_reg(hall->client, HALL_REG_HTHH, hthh, id);
	if (err)
		return err;
	err = hall_i2c_set_reg(hall->client, HALL_REG_HTHL, hthl, id);
	if (err)
		return err;
	err = hall_i2c_set_reg(hall->client, HALL_REG_LTHH, lthh, id);
	if (err)
		return err;
	err = hall_i2c_set_reg(hall->client, HALL_REG_LTHL, lthl, id);
	if (err)
		return err;

	err = hall_clear_interrupt(dev, id);
	if (err) {
		dev_err(&client->dev, "%s: failed to clear interrupt.\n", __func__);
		return err;
	}

	return err;
}

static int hall_update_interrupt_threshold(struct device *dev, short raw, enum hall_id id)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	const int id_table[HALL_ALL][HALL_ALL-1] = {{HALL_B, HALL_C},{HALL_A, HALL_C},{HALL_A, HALL_B}};
	u8 lthh=0, lthl=0, hthh=0, hthl=0;
	int err = -1, i;


	if (raw < HALL_DETECT_DETENT) {
		hall_convdata_short_to_2byte(hall->reg.map.opf, HALL_MAX_THRESHOLD, &hthh, &hthl);
		hall_convdata_short_to_2byte(hall->reg.map.opf, HALL_MIN_THRESHOLD, &lthh, &lthl);
		dev_dbg(&client->dev, "%s:[%s] threshold:[%d, %d]\n",
			__func__, hall_name_str[id],
			HALL_MAX_THRESHOLD, HALL_MIN_THRESHOLD);

		err = hall_i2c_set_reg(hall->client, HALL_REG_HTHH, hthh, id);
		if (err)
			return err;
		err = hall_i2c_set_reg(hall->client, HALL_REG_HTHL, hthl, id);
		if (err)
			return err;
		err = hall_i2c_set_reg(hall->client, HALL_REG_LTHH, lthh, id);
		if (err)
			return err;
		err = hall_i2c_set_reg(hall->client, HALL_REG_LTHL, lthl, id);
		if (err)
			return err;

		hall_convdata_short_to_2byte(hall->reg.map.opf, HALL_MAX_THRESHOLD, &hthh, &hthl);
		hall_convdata_short_to_2byte(hall->reg.map.opf, HALL_DETECT_RANGE_HIGH, &lthh, &lthl);

		for (i = 0; i < HALL_ALL-1; i++) {
			err = hall_i2c_set_reg(hall->client, HALL_REG_HTHH, hthh, id_table[id][i]);
			if (err)
				return err;
			err = hall_i2c_set_reg(hall->client, HALL_REG_HTHL, hthl, id_table[id][i]);
			if (err)
				return err;
			err = hall_i2c_set_reg(hall->client, HALL_REG_LTHH, lthh, id_table[id][i]);
			if (err)
				return err;
			err = hall_i2c_set_reg(hall->client, HALL_REG_LTHL, lthl, id_table[id][i]);
			if (err)
				return err;
			dev_dbg(&client->dev, "%s:[%s] threshold:[%d, %d]\n",
				__func__, hall_name_str[id_table[id][i]],
				HALL_MAX_THRESHOLD, HALL_DETECT_RANGE_HIGH);
		}
	} else {
		hall_convdata_short_to_2byte(hall->reg.map.opf, HALL_MAX_THRESHOLD, &hthh, &hthl);
		hall_convdata_short_to_2byte(hall->reg.map.opf, HALL_DETECT_RANGE_LOW, &lthh, &lthl);
		dev_dbg(&client->dev, "%s:[%s] threshold:[%d, %d]\n",
			__func__, hall_name_str[id], HALL_MAX_THRESHOLD, HALL_DETECT_RANGE_LOW);

		err = hall_i2c_set_all(hall->client, HALL_REG_HTHH, hthh);
		if (err)
			return err;
		err = hall_i2c_set_all(hall->client, HALL_REG_HTHL, hthl);
		if (err)
			return err;
		err = hall_i2c_set_all(hall->client, HALL_REG_LTHH, lthh);
		if (err)
			return err;
		err = hall_i2c_set_all(hall->client, HALL_REG_LTHL, lthl);
		if (err)
			return err;
	}

	err = hall_clear_interrupt(dev, HALL_ALL);
	if (err) {
		dev_err(&client->dev, "%s: failed to clear interrupt.\n", __func__);
		return err;
	}

	return err;
}

static int hall_set_detection_mode(struct device *dev, u8 mode, enum hall_id id)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	u8 data;
	int err = 0;

	if (mode & HALL_DETECTION_MODE_INTERRUPT) {
		/* config threshold */
		err = hall_init_interrupt_threshold(dev, hall->last_data, id);
		if (err) {
			dev_err(&client->dev, "%s: update failed interrupt threshold.\n", __func__);
			return err;
		}
		/* write intsrs */
		data = hall->reg.map.intsrs | HALL_DETECTION_MODE_INTERRUPT;
		err = hall_i2c_set_reg(hall->client, HALL_REG_INTSRS, data, id);
		if (err) {
			dev_err(&client->dev, "%s: failed to set HALL_REG_INTSRS.\n", __func__);
			return err;
		}
	} else {
		/* write intsrs */
		data = hall->reg.map.intsrs & (0xFF - HALL_DETECTION_MODE_INTERRUPT);
		err = hall_i2c_set_reg(hall->client, HALL_REG_INTSRS, data, id);
		if (err) {
			dev_err(&client->dev, "%s: failed to set HALL_REG_INTSRS.\n", __func__);
			return err;
		}
	}

	return err;
}

static void hall_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	int id;

	mutex_lock(&hall->mtx.enable);

	for (id = 0; id < HALL_ALL; id++) {
		if (enable) {
			if (!atomic_cmpxchg(&hall->atm.enable[id], 0, 1)) {
				hall_set_detection_mode
					(dev, hall->reg.map.intsrs & HALL_DETECTION_MODE_INTERRUPT, id);
				hall_set_operation_mode
					(&client->dev, OPERATION_MODE_MEASUREMENT, id);
				dev_info(&client->dev, "%s: enabled.\n", __func__);
			} else
				dev_info(&client->dev, "%s: Already enabled.\n", __func__);
		} else {
			if (atomic_cmpxchg(&hall->atm.enable[id], 1, 0)) {
				hall_set_operation_mode
					(&client->dev, OPERATION_MODE_POWERDOWN, id);
				dev_info(&client->dev, "%s: disabled.\n", __func__);
			} else
				dev_info(&client->dev, "%s: Already disabled.\n", __func__);
		}
		atomic_set(&hall->atm.enable[id], enable);
	}

	mutex_unlock(&hall->mtx.enable);
}

static int hall_measure(struct hall_ddata *hall, short *raw, enum hall_id id)
{
	struct i2c_client *client = hall->client;
	int err, st1_is_ok = 0;
	u8 buf[3];

	err = hall_i2c_read(client, HALL_REG_ST1, buf, sizeof(buf), id);
	if (err) {
		dev_err(&client->dev, "%s: failed to read HALL_REG_ST1", __func__);
		 return err;
	}

	if (hall->reg.map.intsrs & HALL_VAL_INTSRS_INT_ON) {
		if ( ! (buf[0] & 0x10) )
			st1_is_ok = 1;
	} else {
		if (buf[0] & 0x01)
			st1_is_ok = 1;
	}

	if (st1_is_ok) {
		*raw = hall_convdata_2byte_to_short(hall->reg.map.opf, buf[2], buf[1]);
	} else {
		dev_err(&client->dev, "%s: [%s] st1(0x%02X) is not DRDY",
			__func__, hall_name_str[id], buf[0]);
		err = -1;
	}

	return err;
}

static int hall_get_direction(struct hall_ddata *hall, enum hall_id id, int raw)
{
	struct i2c_client *client = hall->client;
	const enum hall_id pattern[HALL_ALL] = {HALL_B, HALL_C, HALL_A};
	int status = (raw < HALL_DETECT_DETENT) ? 1 : 0;

	if (status) {
		if (hall->last_detent_id == -1) {
			dev_info(&client->dev, "%s: [%s] Return_Detent", __func__, hall_name_str[id]);
			return Return_Detent;
		}
		if (hall->last_detent_id == id) {
			dev_info(&client->dev, "%s: [%s] Return_Detent", __func__, hall_name_str[id]);
			return Return_Detent;
		}
		if (pattern[id] == hall->last_detent_id) {
			dev_info(&client->dev, "%s: [%s] CounterClockwise", __func__, hall_name_str[id]);
			return CounterClockwise_Detent;
		}
		else {
			dev_info(&client->dev, "%s: [%s] Clockwise", __func__, hall_name_str[id]);
			return Clockwise_Detent;
		}
	} else {
		if (pattern[id] == hall->last_detent_id) {
			dev_info(&client->dev, "%s: [%s] CounterClockwise_Leave", __func__, hall_name_str[id]);
			return CounterClockwise_Leave;
		} else {
			dev_info(&client->dev, "%s: [%s] Clockwise_Leave", __func__, hall_name_str[id]);
			return Clockwise_Leave;
		}
	}
}

static int hall_work_handler(struct hall_ddata *hall, short* raw, enum hall_id id)
{
	struct i2c_client *client = hall->client;
	int direction;
	int err = 0;

	err = hall_measure(hall, raw, id);
	if (err) {
		dev_err(&client->dev, "%s: [%s] measure failed(%d)", __func__, hall_name_str[id], err);
		err = hall_clear_interrupt(&client->dev, HALL_ALL);
		if (err) {
			dev_err(&client->dev, "%s: failed to clear interrupt.\n", __func__);
			goto out;
		}
		goto out;
	}

	direction = hall_get_direction(hall, id, *raw);
	input_report_rel(hall->input_dev, REL_WHEEL, direction);
	input_report_rel(hall->input_dev, REL_X, id);
	input_sync(hall->input_dev);

	hall->last_id = id;
	if (*raw < HALL_DETECT_DETENT)
		hall->last_detent_id = id;
	hall->last_data = *raw;

out:
	return err;
}

static irqreturn_t hall_irq_handler(int irq, void *dev_id)
{
	struct hall_ddata *hall = (struct hall_ddata *)dev_id;
	struct i2c_client *client = hall->client;
	int id = irq-hall->irq[HALL_A];
	short raw = 0;

	if (!hall->probe_done)
		return IRQ_HANDLED;

	if (gpio_get_value(hall->igpio[id])) {
		dev_err(&client->dev, "%s: [%s] irq gpio is high.", __func__, hall_name_str[id]);
		return IRQ_HANDLED;
	}

	hall_work_handler(hall, &raw, id);

	dev_dbg(&client->dev, "%s: measure_val [%d][%s] = [%3d]", __func__, irq, hall_name_str[id], raw);

	hall_update_interrupt_threshold(&hall->client->dev, raw, id);

	return IRQ_HANDLED;
}

static int hall_request_irq(struct hall_ddata *hall)
{
	struct i2c_client *client = hall->client;
	int err, i;

	for (i = HALL_A; i < HALL_ALL; i++) {
		hall->irq[i] = gpio_to_irq(hall->igpio[i]);
		err = request_threaded_irq(hall->irq[i], NULL,
			hall_irq_handler, IRQF_TRIGGER_FALLING,
			hall_irq_str[i], hall);
		if (err) {
			dev_err(&client->dev, "%s: request irq[%s] was failed", __func__, hall_name_str[i]);
			return err;
		}
	}

	return 0;
}

static void hall_free_irq(struct hall_ddata *hall)
{
	int i;

	for (i = HALL_A; i < HALL_ALL; i++) {
		disable_irq(hall->irq[i]);
		free_irq(hall->irq[i], NULL);
	}
}

static int hall_set_operation_mode(struct device *dev, int mode, enum hall_id id)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	u8 opf = hall->reg.map.opf;
	int err = -1;

	switch(mode) {
		case OPERATION_MODE_POWERDOWN:
			if (hall->irq_enabled) {
				hall_free_irq(hall);
				hall->irq_enabled = 0;
			}
			opf &= (0xff - HALL_VAL_OPF_HSSON_ON);
			err = hall_i2c_set_reg(client, HALL_REG_OPF, opf, id);
			if (err) {
				dev_err(&client->dev, "%s: HALL_REG_OPF register set failed(%d)", __func__, err);
				return err;
			}
			dev_info(&client->dev,
				"%s:[%d] Mode chnaged to OPERATION_MODE_POWERDOWN", __func__, id);
			break;
		case OPERATION_MODE_MEASUREMENT:
			opf &= (0xff - HALL_VAL_OPF_EFRD_ON);
			opf |= HALL_VAL_OPF_HSSON_ON;
			err = hall_i2c_set_reg(client, HALL_REG_OPF, opf, id);
			if (err) {
				dev_err(&client->dev, "%s: HALL_REG_OPF register set failed(%d)", __func__, err);
				return err;
			}
			if (hall->reg.map.intsrs & HALL_DETECTION_MODE_INTERRUPT) {
				if (!hall->irq_enabled) {
					err = hall_request_irq(hall);
					if (err) {
						dev_err(&client->dev,
							"%s: hall_request_irq failed(%d)", __func__, err);
						return err;
					}
					hall->irq_enabled = 1;
				}
			}
			dev_info(&client->dev,
				"%s:[%d] Mode chnaged to OPERATION_MODE_MEASUREMENT", __func__, id);
			break;
		case OPERATION_MODE_FUSEROMACCESS:
			opf |= HALL_VAL_OPF_EFRD_ON;
			opf |= HALL_VAL_OPF_HSSON_ON;
			err = hall_i2c_set_reg(client, HALL_REG_OPF, opf, id);
			if (err) {
				dev_err(&client->dev, "%s: HALL_REG_OPF register set failed(%d)", __func__, err);
				return err;
			}
			dev_info(&client->dev,
				"%s:[%d] Mode chnaged to OPERATION_MODE_FUSEROMACCESS", __func__, id);
			break;
	}

	return err;
}

static int hall_reset_device(struct device *dev, enum hall_id id)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	int err = 0;
	u8 check_id = 0xff, data = 0x00;

	/* (1) sw reset */
	err = hall_i2c_set_reg(hall->client, HALL_REG_SRST, HALL_VAL_SRST_RESET, id);
	if (err) {
		dev_err(&client->dev, "%s: sw-reset was failed(%d)", __func__, err);
		return err;
	}

	msleep(5);
	dev_dbg(&client->dev, "%s: wait 5ms after vdd power up", __func__);

	/* (2) check id */
	err = hall_i2c_get_reg(hall->client, HALL_REG_DID, &check_id, id);
	if (err < 0) {
		dev_err(&client->dev, "%s: failed to set HALL_REG_DID.\n", __func__);
		return err;
	}

	if (check_id != HALL_VAL_DID) {
		dev_err(&client->dev,
			"%s: current device id(0x%02X) is not M1120 device id(0x%02X)",
			__func__, check_id, HALL_VAL_DID);
		return -ENXIO;
	}

	/* (3) init variables */
	/* (3-1) persint */
	data = HALL_PERSISTENCE_COUNT;
	err = hall_i2c_set_reg(hall->client, HALL_REG_PERSINT, data, id);
	if (err < 0)
		dev_err(&client->dev, "%s: failed to set HALL_REG_PERSINT.\n", __func__);

	/* (3-2) intsrs */
	data = HALL_DETECTION_MODE | HALL_SENSITIVITY_TYPE;
	if (data & HALL_DETECTION_MODE_INTERRUPT) {
		data |= HALL_INTERRUPT_TYPE;
	}
	err = hall_i2c_set_reg(hall->client, HALL_REG_INTSRS, data, id);
	if (err < 0)
		dev_err(&client->dev, "%s: failed to set HALL_REG_INTSRS.\n", __func__);

	/* (3-3) opf */
	data = HALL_OPERATION_HIGH_FREQUENCY | HALL_OPERATION_RESOLUTION;
	err = hall_i2c_set_reg(hall->client, HALL_REG_OPF, data, id);
	if (err < 0)
		dev_err(&client->dev, "%s: failed to set HALL_REG_OPF.\n", __func__);

	/* (4) write variable to register */
	err = hall_set_detection_mode(dev, HALL_DETECTION_MODE, id);
	if (err) {
		dev_err(&client->dev, "%s: failed to set HALL_DETECTION_MODE.[%d]", __func__, err);
		return err;
	}

	/* (5) set power-down mode */
	err = hall_set_operation_mode(dev, OPERATION_MODE_POWERDOWN, id);
	if (err) {
		dev_err(&client->dev, "%s: failed to set OPERATION_MODE_POWERDOWN.[%d]", __func__, err);
		return err;
	}

	return err;
}

static int hall_set_power(struct device *dev, bool on)
{
	static struct regulator *regulator_vdd = NULL;
	int ret;

	if (!regulator_vdd) {
		regulator_vdd = regulator_get(NULL, "vdd_ofm_2.8v");
		if (IS_ERR(regulator_vdd)) {
			dev_err(dev, "%s: failed to get ldo14 regulator\n", __func__);
			return PTR_ERR(regulator_vdd);
		}
	}

	if (on) {
		ret = regulator_set_voltage(regulator_vdd, 2800000, 2800000);
		if (ret) {
			dev_err(dev, "%s: unable to set voltage for avdd_vreg, %d\n", __func__, ret);
			return ret;
		}

		ret = regulator_enable(regulator_vdd);
		if (ret) {
			dev_err(dev, "%s: unable to enable regulator_pwr, %d\n", __func__, ret);
			return ret;
		}

		ret = regulator_is_enabled(regulator_vdd);
		if (ret)
			dev_info(dev, "%s: vdd_ofm_2.8v is enabled, %d\n", __func__, ret);
		else {
			dev_err(dev, "%s: vdd_ofm_2.8v is disabled, %d\n", __func__, ret);
			return ret;
		}
		msleep(10);
	} else {
		regulator_disable(regulator_vdd);

		ret = regulator_is_enabled(regulator_vdd);
		if (ret) {
			dev_err(dev, "%s: vdd_ofm_2.8v is enabled, %d\n", __func__, ret);
			return ret;
		} else
			dev_info(dev, "%s: vdd_ofm_2.8v is disabled, %d\n", __func__, ret);
	}

	return 0;
}

static int hall_init_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	int err = -1;
	int id;

	/* (1) vdd and vid power up */
	err = hall_set_power(dev, 1);
	if (err) {
		dev_err(dev, "%s: Power-on was failed (%d)", __func__, err);
		return err;
	}

	/* (2) init variables */
	atomic_set(&hall->atm.enable[HALL_A], 0);
	atomic_set(&hall->atm.enable[HALL_B], 0);
	atomic_set(&hall->atm.enable[HALL_C], 0);
	atomic_set(&hall->atm.delay, HALL_DELAY_MIN);

#ifdef HALL_DBG_ENABLE
	atomic_set(&hall->atm.debug, 1);
#else
	atomic_set(&hall->atm.debug, 0);
#endif

	hall->calibrated_data = 0;
	hall->last_data = 0;
	hall->irq_enabled = 0;
	hall->irq_first = 1;
	hall->thrhigh = HALL_DETECT_RANGE_HIGH;
	hall->thrlow = HALL_DETECT_RANGE_LOW;
	hall_set_debug(&client->dev, 0);

	for (id = 0; id < HALL_ALL; id++) {
		err = hall_reset_device(dev, id);
		if (err) {
			dev_err(dev, "%s: Hall_reset_device was failed (%d)(%d)", __func__, err, id);
			return err;
		}
	}

	dev_dbg(dev, "%s: Initializing device was success", __func__);

	return 0;
}

static int hall_input_dev_init(struct hall_ddata *hall)
{
	struct input_dev *dev;
	int err;

	dev = input_allocate_device();
	if (!dev) {
		return -ENOMEM;
	}
	dev->name = HALL_DRIVER_NAME;
	dev->id.bustype = BUS_I2C;
	hall->input_dev = dev;

	input_set_drvdata(dev, hall);

	__set_bit(EV_REL, hall->input_dev->evbit);
	__set_bit(EV_KEY, hall->input_dev->evbit);
	__set_bit(REL_X, hall->input_dev->relbit);
	__set_bit(REL_Y, hall->input_dev->relbit);
	__set_bit(BTN_LEFT, hall->input_dev->keybit);
	__set_bit(REL_WHEEL, hall->input_dev->relbit);

	input_set_capability(hall->input_dev, EV_REL, REL_X);
	input_set_capability(hall->input_dev, EV_REL, REL_Y);
	input_set_capability(hall->input_dev, EV_REL, REL_WHEEL);

	err = input_register_device(dev);
	if (err < 0) {
		dev_err(&dev->dev, "%s: failed to register input device.", __func__);
		input_free_device(dev);
		return err;
	}

	dev_info(&dev->dev, "%s: %s was initialized", __func__, HALL_DRIVER_NAME);

	return 0;
}

static void hall_input_dev_terminate(struct hall_ddata *hall)
{
	struct input_dev *dev = hall->input_dev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int hall_gpio_init(struct hall_ddata *hall)
{
	struct i2c_client *client = hall->client;
	int err;

	err = gpio_request(hall->igpio[HALL_A], "hall_irq[HALL_A]");
	if (err) {
		dev_err(&client->dev, "%s: gpio_a request was failed(%d)", __func__, err);
		goto error_1;
	}
	dev_info(&client->dev, "%s: hall->igpio[HALL_A]=[%d]. \n", __func__, hall->igpio[HALL_A]);

	err = gpio_direction_input(hall->igpio[HALL_A]);
	if (err < 0) {
		dev_err(&client->dev, "%s: gpio_a direction setting was failed(%d)", __func__, err);
		goto error_2;
	}

	err = gpio_request(hall->igpio[HALL_B], "hall_irq[HALL_B]");
	if (err) {
		dev_err(&client->dev, "%s: gpio_b request was failed(%d)", __func__, err);
		goto error_2;
	}
	dev_info(&client->dev, "%s: hall->igpio[HALL_B]=[%d]. \n", __func__, hall->igpio[HALL_B]);

	err = gpio_direction_input(hall->igpio[HALL_B]);
	if (err < 0) {
		dev_err(&client->dev, "%s: gpio_b direction setting was failed(%d)", __func__, err);
		goto error_3;
	}

	err = gpio_request(hall->igpio[HALL_C], "hall_irq[HALL_C]");
	if (err) {
		dev_err(&client->dev, "%s: gpio_c request was failed(%d)", __func__, err);
		goto error_3;
	}
	dev_info(&client->dev, "%s: hall->igpio[HALL_C]=[%d]. \n", __func__, hall->igpio[HALL_C]);

	err = gpio_direction_input(hall->igpio[HALL_C]);
	if (err < 0) {
		dev_err(&client->dev, "%s: gpio_c direction setting was failed(%d)", __func__, err);
		goto error_4;
	}

	return 0;

error_4:
	gpio_free(hall->igpio[HALL_C]);
error_3:
	gpio_free(hall->igpio[HALL_B]);
error_2:
	gpio_free(hall->igpio[HALL_A]);
error_1:
	return -1;
}

static int hall_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct hall_ddata *hall;
	int err = 0;

	dev_info(&client->dev, "%s is called[%s]. \n", __func__, id->name);

	hall = kzalloc(sizeof(struct hall_ddata), GFP_KERNEL);
	if (!hall) {
		dev_err(&client->dev, "%s: kernel memory alocation was failed", __func__);
		err = -ENOMEM;
		goto error_0;
	}

	mutex_init(&hall->mtx.enable);
	mutex_init(&hall->mtx.data);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c_check_functionality was failed", __func__);
		err = -ENODEV;
		goto error_1;
	}

	hall->client = client;
	i2c_set_clientdata(client, hall);

	/* (4) get platform data */
	hall->pdata = client->dev.platform_data;
	if (!hall->pdata) {
		dev_err(&client->dev, "%s: failed to get platform data\n", __func__);
		goto error_1;
	}

	hall->power_vi2c = hall->pdata->power_vi2c;
	hall->power_vdd = hall->pdata->power_vdd;
	hall->igpio[HALL_A] = hall->pdata->int_a_gpio;
	hall->igpio[HALL_B] = hall->pdata->int_b_gpio;
	hall->igpio[HALL_C] = hall->pdata->int_c_gpio;

	hall->last_detent_id = -1;
	hall->last_id = -1;
	hall->last_data= -1;

	err = hall_gpio_init(hall);
	if (err) {
		dev_err(&client->dev, "%s: gpio init was failed(%d)", __func__, err);
		goto error_1;
	}

	err = hall_init_device(&client->dev);
	if (err) {
		dev_err(&client->dev, "%s: hall_init_device was failed(%d)", __func__, err);
		goto error_2;
	}

	/* (8) init input device */
	err = hall_input_dev_init(hall);
	if (err) {
		dev_err(&client->dev, "%s: hall_input_dev_init was failed(%d)", __func__, err);
		goto error_3;
	}
	device_init_wakeup(&client->dev, true);
	hall_set_enable(&client->dev, true);

	hall->probe_done = true;
	dev_info(&client->dev, "%s: %s was probed.\n", __func__, HALL_DRIVER_NAME);

	return 0;

error_3:
	hall_input_dev_terminate(hall);

error_2:
error_1:
	kfree(hall);

error_0:

	return err;
}

static int hall_i2c_remove(struct i2c_client *client)
{
	struct hall_ddata *hall = i2c_get_clientdata(client);

	dev_info(&hall->client->dev, "%s\n", __func__);

	return 0;
}

static int hall_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	int err;
	u8 data = 0x00;

	if (device_may_wakeup(&client->dev)) {
		data = HALL_OPERATION_LOW_FREQUENCY |\
			HALL_OPERATION_RESOLUTION | HALL_VAL_OPF_HSSON_ON;
		err = hall_i2c_set_all(hall->client, HALL_REG_OPF, data);
		if (err < 0)
			dev_err(&client->dev, "%s: failed to set HALL_REG_OPF.\n", __func__);

		enable_irq_wake(hall->irq[HALL_A]);
		enable_irq_wake(hall->irq[HALL_B]);
		enable_irq_wake(hall->irq[HALL_C]);
	} else {
		disable_irq(hall->irq[HALL_A]);
		disable_irq(hall->irq[HALL_B]);
		disable_irq(hall->irq[HALL_C]);
	}

	return 0;
}

static int hall_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hall_ddata *hall = i2c_get_clientdata(client);
	int err;
	u8 data = 0x00;

	if (device_may_wakeup(&client->dev)) {
		data = HALL_OPERATION_HIGH_FREQUENCY |\
			HALL_OPERATION_RESOLUTION | HALL_VAL_OPF_HSSON_ON;
		err = hall_i2c_set_all(hall->client, HALL_REG_OPF, data);
		if (err < 0)
			dev_err(&client->dev, "%s: failed to set HALL_REG_OPF.\n", __func__);

		disable_irq_wake(hall->irq[HALL_A]);
		disable_irq_wake(hall->irq[HALL_B]);
		disable_irq_wake(hall->irq[HALL_C]);
	} else {
		enable_irq(hall->irq[HALL_A]);
		enable_irq(hall->irq[HALL_B]);
		enable_irq(hall->irq[HALL_C]);
	}

	return 0;
}

static const struct i2c_device_id hall_i2c_id[]={
	{I2C_HALL_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, hall_i2c_id);

#ifdef CONFIG_PM
static const struct dev_pm_ops hall_pm_ops = {
	.suspend = hall_suspend,
	.resume = hall_resume,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id hall_dt_match[] = {
	{ .compatible = "sec,hall" },
	{ }
};
#else
#define hall_dt_match NULL
#endif

static struct i2c_driver hall_i2c_driver = {
	.probe	= hall_i2c_probe,
	.remove	= hall_i2c_remove,
	.driver =	{
		.name	= I2C_HALL_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &hall_pm_ops,
#endif
		.of_match_table = of_match_ptr(hall_dt_match),
	},
	.id_table	= hall_i2c_id,
};

static int __init hall_init(void)
{
	int ret;

	ret = i2c_add_driver(&hall_i2c_driver);
	if (ret!=0)
		printk("%s: I2C device init Faild! return(%d) \n", __func__,  ret);
	else
		printk("%s: I2C device init Sucess\n", __func__);

	return ret;
}

static void __exit hall_exit(void)
{
	printk(" %s\n", __func__);
	i2c_del_driver(&hall_i2c_driver);
}

module_init(hall_init);
module_exit(hall_exit);
MODULE_AUTHOR("Sang-Min,Lee <lsmin.lee@samsung.com>");
MODULE_DESCRIPTION("i2c hall ic device driver");
MODULE_LICENSE("GPL");
