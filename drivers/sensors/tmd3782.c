/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include "sensors_core.h"

#define VENDOR_NAME			"AMS"
#define MODEL_NAME			"TMD3782"
#define LIGHT_MODULE_NAME		"light_sensor"
#define PROXIMITY_MODULE_NAME		"proximity_sensor"
#define PROX_OFFSET_FILE_PATH		"/efs/prox_cal"

#define I2C_M_WR			0 /* for i2c Write */
#define I2c_M_RD			1 /* for i2c Read */

#define EVENT_TYPE_LIGHT_LUX		REL_MISC
#define EVENT_TYPE_LIGHT_CCT		REL_WHEEL
#define EVENT_TYPE_PROX			ABS_DISTANCE

#define CHIP_ID				0x69

/* TMD3782 register offsets */
#define CNTRL				0x00
#define ALS_TIME			0X01
#define PRX_TIME			0x02
#define WAIT_TIME			0x03
#define ALS_MINTHRESHLO			0X04
#define ALS_MINTHRESHHI			0X05
#define ALS_MAXTHRESHLO			0X06
#define ALS_MAXTHRESHHI			0X07
#define PRX_MINTHRESHLO			0X08
#define PRX_MINTHRESHHI			0X09
#define PRX_MAXTHRESHLO			0X0A
#define PRX_MAXTHRESHHI			0X0B
#define INTERRUPT			0x0C
#define PRX_CFG				0x0D
#define PRX_COUNT			0x0E
#define GAIN				0x0F
#define REVID				0x11
#define CHIPID				0x12
#define STATUS				0x13
#define CLR_CHAN0LO			0x14
#define CLR_CHAN0HI			0x15
#define RED_CHAN1LO			0x16
#define RED_CHAN1HI			0x17
#define GRN_CHAN1LO			0x18
#define GRN_CHAN1HI			0x19
#define BLU_CHAN1LO			0x1A
#define BLU_CHAN1HI			0x1B
#define PRX_LO				0x1C
#define PRX_HI				0x1D
#define PRX_OFFSET			0x1E
#define TEST_STATUS			0x1F

/*TMD3782 cmd reg masks*/
#define CMD_REG				0X80
#define CMD_BYTE_RW			0x00
#define CMD_WORD_BLK_RW			0x20
#define CMD_SPL_FN			0x60
#define CMD_PROX_INTCLR			0X05
#define CMD_ALS_INTCLR			0X06
#define CMD_PROXALS_INTCLR		0X07
#define CMD_TST_REG			0X08
#define CMD_USER_REG			0X09

/* TMD3782 cntrl reg masks */
#define CNTL_REG_CLEAR			0x00
#define CNTL_PROX_INT_ENBL		0X20
#define CNTL_ALS_INT_ENBL		0X10
#define CNTL_WAIT_TMR_ENBL		0X08
#define CNTL_PROX_DET_ENBL		0X04
#define CNTL_ADC_ENBL			0x02
#define CNTL_PWRON			0x01
#define CNTL_ALSPON_ENBL		0x03
#define CNTL_INTALSPON_ENBL		0x13
#define CNTL_PROXPON_ENBL		0x0F
#define CNTL_INTPROXPON_ENBL		0x2F

/* TMD3782 status reg masks */
#define STA_ADCVALID			0x01
#define STA_PRXVALID			0x02
#define STA_ADC_PRX_VALID		0x03
#define STA_ADCINTR			0x10
#define STA_PRXINTR			0x20

#define Atime_ms			504 /* 50.4ms */
#define DGF				475
#define R_Coef1				535
#define G_Coef1				1000
#define B_Coef1				795
#define CT_Coef1			2855
#define CT_Offset1			1973
#define INTEGRATION_CYCLE		240

#define PROX_AVG_COUNT			40
#define MAX_LUX				39768

#define PROX_MAX			1023
#define PROX_MIN			0
#define PROX_OFFSET_ARRAY_LENGTH	10

#ifdef CONFIG_KLIMT
#define ALS_TIME_SET				0xEB
#define WAIT_TIME_SET				0xFF
#define INTER_FILTER_SET			0x23
#define PRX_CFG_SET				0x00
#define PRX_PULSE_COUNT_SET		0x08
#define ALS_GAIN_SET				0x21
#define PROX_THD_HIGH_DEFAULT		750
#define PROX_THD_LOW_DEFAULT		580
#else
#define ALS_TIME_SET			0xED
#define WAIT_TIME_SET			0xFF
#define INTER_FILTER_SET		0x33
#define PRX_CFG_SET			0x00
#define PRX_PULSE_COUNT_SET		0x0C
#define ALS_GAIN_SET			0x28
#define PROX_THD_HIGH_DEFAULT		750
#define PROX_THD_LOW_DEFAULT		580
#endif

#define LIGHT_LOG_TIME			15 /* 15 sec */

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

enum {
	STATE_CLOSE = 0,
	STATE_FAR = 1,
};

#define TMD3782_LIGHT_DEFAULT_DELAY	200000000LL

struct tmd3782_p {
	struct i2c_client *i2c_client;
	struct mutex power_lock;
	struct input_dev *light_input;
	struct input_dev *prox_input;
	struct device *light_dev;
	struct device *prox_dev;
	struct delayed_work light_work;
	struct delayed_work prox_work;
	struct delayed_work prox_avg_work;
	struct wake_lock prox_wake_lock;
	atomic_t light_poll_delay;
	u8 power_state;
	u16 clrdata;
	u16 reddata;
	u16 grndata;
	u16 bludata;
	s32 irdata;
	int time_count;
	int prox_irq;
	int prox_irq_gpio;
	int prox_avg[3];
	int prox_avg_enable;
	u16 prox_thd_high;
	u16 prox_thd_low;
	u16 prox_offset;
	int prox_state;
	int prox_cal_result;
};

static int tmd3782_i2c_read(struct tmd3782_p *data,
		unsigned char reg_addr,  unsigned char *buf)
{
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = data->i2c_client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(data->i2c_client->adapter, msg, 2);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - error %d\n", __func__, ret);

		return ret;
	}

	return 0;
}

static int tmd3782_i2c_read_word(struct tmd3782_p *data,
		unsigned char reg_addr, u16 *buf)
{
	int ret;
	struct i2c_msg msg[2];
	unsigned char r_buf[2];

	msg[0].addr = data->i2c_client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = r_buf;

	ret = i2c_transfer(data->i2c_client->adapter, msg, 2);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - i2c read error %d\n", __func__, ret);

		return ret;
	}

	*buf = (u16)r_buf[1];
	*buf = (*buf << 8) | (u16)r_buf[0];

	return 0;
}

static int tmd3782_i2c_write(struct tmd3782_p *data,
		unsigned char reg_addr, unsigned char buf)
{
	int ret = 0;
	struct i2c_msg msg;
	unsigned char w_buf[2];

	w_buf[0] = reg_addr;
	w_buf[1] = buf;

	/* send slave address & command */
	msg.addr = data->i2c_client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 2;
	msg.buf = (char *)w_buf;

	ret = i2c_transfer(data->i2c_client->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - error %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int tmd3782_i2c_write_cmd(struct tmd3782_p *data, unsigned char cmd)
{
	int ret = 0;
	struct i2c_msg msg;

	/* send slave address & command */
	msg.addr = data->i2c_client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 1;
	msg.buf = (char *)&cmd;

	ret = i2c_transfer(data->i2c_client->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - error %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int tmd3782_chip_on(struct tmd3782_p *data)
{
	int i = 0, ret = 0;
	u8 prox_thd_buf[4];

	ret = tmd3782_i2c_write(data, CMD_REG | CNTRL, CNTL_PWRON);
	if (ret < 0)
		goto exit_i2c_fail;

	ret = tmd3782_i2c_write(data, CMD_REG | ALS_TIME, ALS_TIME_SET);
	if (ret < 0)
		goto exit_i2c_fail;

	ret = tmd3782_i2c_write(data, CMD_REG | WAIT_TIME, WAIT_TIME_SET);
	if (ret < 0)
		goto exit_i2c_fail;

	ret = tmd3782_i2c_write(data, CMD_REG | INTERRUPT, INTER_FILTER_SET);
	if (ret < 0)
		goto exit_i2c_fail;

	ret = tmd3782_i2c_write(data, CMD_REG |PRX_CFG, PRX_CFG_SET);
	if (ret < 0)
		goto exit_i2c_fail;

	ret = tmd3782_i2c_write(data, CMD_REG | PRX_COUNT,
			PRX_PULSE_COUNT_SET);
	if (ret < 0)
		goto exit_i2c_fail;

	ret = tmd3782_i2c_write(data, CMD_REG | GAIN, ALS_GAIN_SET);
	if (ret < 0)
		goto exit_i2c_fail;

	/* Setting for proximity interrupt */
	prox_thd_buf[0] = 0;
	prox_thd_buf[1] = 0;
	prox_thd_buf[2] = data->prox_thd_high & 0xff;
	prox_thd_buf[3] = (data->prox_thd_high >> 8) & 0xff;

	for (i = 0; i < 4; i++) {
		ret = tmd3782_i2c_write(data, CMD_REG | (PRX_MINTHRESHLO + i),
				prox_thd_buf[i]);
		if (ret < 0)
			goto exit_i2c_fail;
	}

	ret = tmd3782_i2c_write(data, CMD_REG | CNTRL, CNTL_INTPROXPON_ENBL);
	if (ret < 0)
		goto exit_i2c_fail;

	msleep(60); /*more than 58 ms*/
	goto exit;

exit_i2c_fail:
	pr_err("[SENSOR]: %s - fail! %d\n", __func__, ret);
exit:
	return ret;
}

static int tmd3782_chip_off(struct tmd3782_p *data)
{
	int ret;

	ret = tmd3782_i2c_write(data, CMD_REG | CNTRL, CNTL_REG_CLEAR);
	if (ret < 0)
		pr_err("[SENSOR]: %s - fail! %d\n", __func__, ret);

	return ret;
}


static int tmd3782_light_get_cct(struct tmd3782_p *data)
{
	s32 bp1 = (s32)data->bludata - data->irdata;
	s32 rp1 = (s32)data->reddata - data->irdata;
	int cct = 0;

	if(rp1 != 0)
		cct = (int)(CT_Coef1 * bp1 / rp1 + CT_Offset1);

	return cct;
}

static int tmd3782_light_get_lux(struct tmd3782_p *data)
{
	s32 rp1, gp1, bp1, cp1;
	s32 lux = 0;
	u16 gain_reg = 0x0;
	int gain = 1;
	int ret = 0;

	ret = tmd3782_i2c_read_word(data, CMD_REG | GAIN, &gain_reg);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - fail! %d\n", __func__, ret);
		return 0;
	}

	tmd3782_i2c_read_word(data, CMD_REG | CLR_CHAN0LO, &data->clrdata);
	tmd3782_i2c_read_word(data, CMD_REG | RED_CHAN1LO, &data->reddata);
	tmd3782_i2c_read_word(data, CMD_REG | GRN_CHAN1LO, &data->grndata);
	tmd3782_i2c_read_word(data, CMD_REG | BLU_CHAN1LO, &data->bludata);

	switch (gain_reg & 0x03) {
		case 0x00:
			gain = 1;
			break;
		case 0x01:
			gain = 4;
			break;
		case 0x02:
			gain = 16;
			break;
#if 0
		case 0x03:
			gain = 64;
			break;
#endif
		default:
			break;
	}

	if (data->clrdata >= 18500) {
		lux = MAX_LUX;
		return lux;
	}

	if (gain ==4 && data->clrdata < 100)
		tmd3782_i2c_write(data, CMD_REG | GAIN, 0x22);
	else if(gain == 16 && data->clrdata > 15000)
		tmd3782_i2c_write(data, CMD_REG | GAIN, 0x21);

	/* calculate lux */
	data->irdata = ((s32)data->reddata + (s32)data->grndata\
			+ (s32)data->bludata - (s32)data->clrdata) / 2;

	/* remove ir from counts */
	rp1 = (s32)data->reddata - data->irdata;
	gp1 = (s32)data->grndata - data->irdata;
	bp1 = (s32)data->bludata - data->irdata;
	cp1 = (s32)data->clrdata - data->irdata;

	lux = (rp1 * R_Coef1 + gp1 * G_Coef1 + bp1 * B_Coef1) / 1000;

	if(lux < 0) {
		lux = 0;
	} else {
		/* divide by CPL, CPL = (Atime_ms * ALS_GAIN / DGF) */
		lux = lux * DGF;
		lux *= 10; /* Atime_ms */
		lux /= Atime_ms;
		lux /= gain;
	}

	return (int)lux;
}

static void tmd3782_light_light_enable(struct tmd3782_p *data)
{
	schedule_delayed_work(&data->light_work,
		nsecs_to_jiffies(atomic_read(&data->light_poll_delay)));
}

static void tmd3782_light_light_disable(struct tmd3782_p *data)
{
	cancel_delayed_work_sync(&data->light_work);
}

static void tmd3782_light_work_func(struct work_struct *work)
{
	struct tmd3782_p *data = container_of((struct delayed_work *)work,
			struct tmd3782_p, light_work);
	unsigned long delay =
			nsecs_to_jiffies(atomic_read(&data->light_poll_delay));
	int lux = tmd3782_light_get_lux(data);
	int cct = tmd3782_light_get_cct(data);

	input_report_rel(data->light_input, EVENT_TYPE_LIGHT_LUX, lux + 1);
	input_report_rel(data->light_input, EVENT_TYPE_LIGHT_CCT, cct);
	input_sync(data->light_input);

	if (((int64_t)atomic_read(&data->light_poll_delay)\
		* (int64_t)data->time_count)
		>= ((int64_t)LIGHT_LOG_TIME * NSEC_PER_SEC)) {
		pr_info("[SENSOR]: %s - r = %u g = %u b = %u c = %u,"
			" lux = %d, cct = %d\n", __func__,
			data->reddata, data->grndata,
			data->bludata, data->clrdata, lux, cct);
		data->time_count = 0;
	} else {
		data->time_count++;
	}

	schedule_delayed_work(&data->light_work, delay);
}

/* sysfs */
static ssize_t tmd3782_light_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			atomic_read(&data->light_poll_delay));
}

static ssize_t tmd3782_light_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);
	int64_t new_delay;
	int ret;

	ret = kstrtoll(buf, 10, &new_delay);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	atomic_set(&data->light_poll_delay, new_delay);
	pr_info("[SENSOR]: %s - poll_delay = %lld\n", __func__, new_delay);

	return size;
}

static ssize_t tmd3782_light_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct tmd3782_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	mutex_lock(&data->power_lock);
	pr_info("[SENSOR]: %s - new_value = %u\n", __func__, enable);
	if (enable && !(data->power_state & LIGHT_ENABLED)) {
		if (data->power_state == 0)
			tmd3782_chip_on(data);
		data->power_state |= LIGHT_ENABLED;
		tmd3782_light_light_enable(data);
	} else if (!enable && (data->power_state & LIGHT_ENABLED)) {
		tmd3782_light_light_disable(data);
		data->power_state &= ~LIGHT_ENABLED;
		if (data->power_state == 0)
			tmd3782_chip_off(data);
	}
	mutex_unlock(&data->power_lock);
	return size;
}

static ssize_t tmd3782_light_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			(data->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		tmd3782_light_delay_show, tmd3782_light_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		tmd3782_light_enable_show, tmd3782_light_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL,
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

/* light sysfs */
static ssize_t tmd3782_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t tmd3782_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t tmd3782_light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u\n",
			data->reddata, data->grndata,
			data->bludata, data->clrdata);
}

static ssize_t tmd3782_light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u\n",
			data->reddata, data->grndata,
			data->bludata, data->clrdata);
}

static DEVICE_ATTR(name, S_IRUGO, tmd3782_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, tmd3782_vendor_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, tmd3782_light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, tmd3782_light_data_show, NULL);

static struct device_attribute *light_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

static int tmd3782_light_input_init(struct tmd3782_p *data)
{
	int ret = 0;
	struct input_dev *dev;

	/* allocate lightsensor input_device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = LIGHT_MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, EVENT_TYPE_LIGHT_LUX);
	input_set_capability(dev, EV_REL, EVENT_TYPE_LIGHT_CCT);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0) {
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &light_attribute_group);
	if (ret < 0) {
		sensors_remove_symlink(&dev->dev.kobj, dev->name);
		input_unregister_device(dev);
		return ret;
	}

	data->light_input = dev;
	return 0;
}

static int tmd3782_prox_led_onoff(struct tmd3782_p *data, bool onoff)
{
	struct regulator *leda;

	leda = devm_regulator_get(&data->i2c_client->dev, "leda_2.8v");
	if (IS_ERR(leda)) {
		pr_err("[SENSOR]: %s - cannot get leda_2.8v\n", __func__);
		return -ENOMEM;
	}

	if (onoff)
		regulator_enable(leda);
	else
		regulator_disable(leda);

	devm_regulator_put(leda);

	return 0;
}

static void tmd3782_prox_work_func(struct work_struct *work)
{
	struct tmd3782_p *data = container_of((struct delayed_work *)work,
			struct tmd3782_p, prox_work);
	u8 prox_thd_buf[4];
	u16 adc, thd_high, thd_low;
	int i, state = STATE_FAR;

	tmd3782_i2c_read_word(data, CMD_REG | PRX_LO, &adc);
	tmd3782_i2c_read_word(data, CMD_REG | PRX_MAXTHRESHLO, &thd_high);
	tmd3782_i2c_read_word(data, CMD_REG | PRX_MINTHRESHLO, &thd_low);
	pr_info("[SENSOR]: %s - hi = %u, low = %u, adc = %u\n",
		__func__, data->prox_thd_high, data->prox_thd_low, adc);

	if ((thd_high == data->prox_thd_high)
			&& (adc >= data->prox_thd_high)) {
		state = STATE_CLOSE;
		prox_thd_buf[0] = (data->prox_thd_low) & 0xFF;
		prox_thd_buf[1] = (data->prox_thd_low >> 8) & 0xFF;
		prox_thd_buf[2] = (0xFFFF) & 0xFF;
		prox_thd_buf[3] = (0xFFFF >> 8) & 0xFF;
	} else if ((thd_high == 0xFFFF) && (adc <= data->prox_thd_low)) {
		state = STATE_FAR;
		prox_thd_buf[0] = (0x0000) & 0xFF;
		prox_thd_buf[1] = (0x0000 >> 8) & 0xFF;
		prox_thd_buf[2] = (data->prox_thd_high) & 0xFF;
		prox_thd_buf[3] = (data->prox_thd_high >> 8) & 0xFF;
	} else {
		pr_err("[SENSOR]: %s - Error Case! [adc= %u, th_high = %u,"
			"th_min = %u]\n", __func__, adc, thd_high, thd_low);
		goto exit;
	}

	for (i = 0; i < 4; i++)
		tmd3782_i2c_write(data, CMD_REG | (PRX_MINTHRESHLO + i),
			prox_thd_buf[i]);

	input_report_abs(data->prox_input, EVENT_TYPE_PROX, state);
	input_sync(data->prox_input);
	pr_info("[SENSOR]: %s - value = %d\n", __func__, state);

	data->prox_state = state;

exit:
	/* reset Interrupt pin */
	/* to active Interrupt, TMD2771x Interuupt pin shoud be reset. */
	tmd3782_i2c_write_cmd(data, CMD_REG | CMD_SPL_FN | CMD_PROXALS_INTCLR);
}

static irqreturn_t tmd3782_prox_irq_thread(int irq, void *pdata)
{
	struct tmd3782_p *data = pdata;

	pr_info("[SENSOR]: ################ %s ################\n", __func__);
	wake_lock_timeout(&data->prox_wake_lock, 3 * HZ);
	schedule_delayed_work(&data->prox_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

static void tmd3782_prox_avg_work_func(struct work_struct *work)
{
	struct tmd3782_p *data = container_of((struct delayed_work *)work,
			struct tmd3782_p, prox_avg_work);
	u16 adc = 0;
	int min = 0xffff, max = 0, avg = 0;
	int i = 0;

	for (i = 0; i < PROX_AVG_COUNT; i++) {
		tmd3782_i2c_read_word(data, CMD_REG | PRX_LO, &adc);
		if (adc > PROX_MAX)
			adc = PROX_MAX;

		avg += adc;
		if (adc < min)
			min = adc;
		if (adc > max)
			max = adc;
		msleep(40);
	}

	avg /= i;
	data->prox_avg[0] = min;
	data->prox_avg[1] = avg;
	data->prox_avg[2] = max;

	schedule_delayed_work(&data->prox_avg_work, msecs_to_jiffies(2000));
}

static int tmd3782_prox_open_offset(struct tmd3782_p *data)
{
	int ret = 0;
	struct file *offset_filp = NULL;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	offset_filp = filp_open(PROX_OFFSET_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(offset_filp)) {
		ret = PTR_ERR(offset_filp);
		if (ret != -ENOENT)
			pr_err("[SENSOR]: %s - Can't open offset file\n",
				__func__);
		set_fs(old_fs);
		return ret;
	}

	ret = offset_filp->f_op->read(offset_filp,
		(char *)&data->prox_offset, sizeof(u16), &offset_filp->f_pos);
	if (ret != sizeof(u16)) {
		pr_err("[SENSOR]: %s - Can't read the offset data from file\n",
			__func__);
		ret = -EIO;
	}

	pr_info("[SENSOR]: %s - offset = %u\n", __func__, data->prox_offset);
	filp_close(offset_filp, current->files);
	set_fs(old_fs);
	return ret;
}

static int tmd3782_prox_adc(struct tmd3782_p *data)
{
	u16 adc;
	int i = 0;
	int avg = 0;
	int min = 0xffff;
	int max = 0;
	int total = 0;

	for (i = 0; i < PROX_OFFSET_ARRAY_LENGTH; i++) {
		usleep_range(11000, 11000);
		tmd3782_i2c_read_word(data, CMD_REG | PRX_LO, &adc);
		if (adc < min)
			min = adc;
		else if (adc > max)
			max = adc;

		total += adc;
	}

	total -= (min + max);
	avg = (int)(total / (PROX_OFFSET_ARRAY_LENGTH - 2));

	return avg;
}

static void tmd3782_prox_thd_set(struct tmd3782_p *data)
{
	int i;
	u8 prox_thd_buf[4] = {0,};

	/* Setting for proximity interrupt */
	if (data->prox_state == STATE_CLOSE) {
		prox_thd_buf[0] = (data->prox_thd_low) & 0xFF;
		prox_thd_buf[1] = (data->prox_thd_low >> 8) & 0xFF;
		prox_thd_buf[2] = (0xFFFF) & 0xFF;
		prox_thd_buf[3] = (0xFFFF >> 8) & 0xFF;
	} else if (data->prox_state == STATE_FAR) {
		prox_thd_buf[0] = (0x0000) & 0xFF;
		prox_thd_buf[1] = (0x0000 >> 8) & 0xFF;
		prox_thd_buf[2] = (data->prox_thd_high) & 0xff;
		prox_thd_buf[3] = (data->prox_thd_high >> 8) & 0xff;
	}

	for (i = 0; i < 4; i++)
		tmd3782_i2c_write(data, CMD_REG | (PRX_MINTHRESHLO + i),
				prox_thd_buf[i]);
}

static int tmd3782_prox_store_offset(struct tmd3782_p *data, bool do_calib)
{
	struct file *offset_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;
	u16 max_ct = PROX_THD_LOW_DEFAULT / 2;
	u16 abnormal_ct = tmd3782_prox_adc(data);
	u16 offset = 0;

	if(do_calib) {
		pr_info("[SENSOR]: %s - calibration start\n", __func__);
		if(abnormal_ct > 500) {
			pr_err("[SENSOR]: %s - crosstalk is lager than 500\n",
				__func__);
		}

		if(abnormal_ct > max_ct) {
			offset = abnormal_ct - max_ct;
			if ((PROX_THD_HIGH_DEFAULT + offset) > 900)
				pr_err("[SENSOR]: %s - Threshold is lager"
						"than 900\n", __func__);
		}

		data->prox_offset = offset;
		data->prox_thd_high = PROX_THD_HIGH_DEFAULT + offset;
		data->prox_thd_low = PROX_THD_LOW_DEFAULT + offset;

		tmd3782_prox_thd_set(data);
		data->prox_cal_result = 1;
	} else {
		pr_info("[SENSOR]: %s - clear\n", __func__);
		data->prox_thd_high = PROX_THD_HIGH_DEFAULT;
		data->prox_thd_low = PROX_THD_LOW_DEFAULT;
		tmd3782_prox_thd_set(data);
		data->prox_offset = 0;
		data->prox_cal_result = 2;
	}
	pr_info("[SENSOR]: %s - max_ct : %d, abnormal_ct : %d, offset : %d\n",
			__func__, max_ct, abnormal_ct, data->prox_offset);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	offset_filp = filp_open(PROX_OFFSET_FILE_PATH,
				O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(offset_filp)) {
		pr_err("[SENSOR]: %s - Can't open offset file\n", __func__);
		set_fs(old_fs);
		ret = PTR_ERR(offset_filp);
		return ret;
	}

	ret = offset_filp->f_op->write(offset_filp,
		(char *)&data->prox_offset, sizeof(u16), &offset_filp->f_pos);
	if (ret != sizeof(u16)) {
		pr_err("[SENSOR]: %s - Can't write the offset to file\n",
			__func__);
		ret = -EIO;
	}

	filp_close(offset_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static ssize_t tmd3782_prox_cal_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	bool do_calib;
	int ret;
	struct tmd3782_p *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) { /* calibrate cancelation value */
		do_calib = true;
	} else if (sysfs_streq(buf, "0")) { /* reset cancelation value */
		do_calib = false;
	} else {
		pr_err("[SENSOR]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ret = tmd3782_prox_store_offset(data, do_calib);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - fail!\n", __func__);
		data->prox_cal_result = -1;
		return ret;
	}

	return size;
}

static ssize_t tmd3782_prox_cal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);

	tmd3782_prox_open_offset(data);

	return sprintf(buf, "%d,%d,%d\n",
		data->prox_offset, data->prox_thd_high, data->prox_thd_low);
}

static ssize_t tmd3782_prox_offset_pass_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->prox_cal_result);
}

static ssize_t tmd3782_prox_avg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct tmd3782_p *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d,%d,%d\n", data->prox_avg[0],
			data->prox_avg[1], data->prox_avg[2]);
}

static ssize_t tmd3782_prox_avg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);
	int new_value = 0;

	if (sysfs_streq(buf, "1")) {
		new_value = 1;
	} else if (sysfs_streq(buf, "0")) {
		new_value = 0;
	} else {
		pr_err("[SENSOR]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (data->prox_avg_enable == new_value) {
		pr_err("[SENSOR]: %s - same state\n", __func__);
	} else if (new_value == 1) {
		pr_info("[SENSOR]: %s - start to get avg\n", __func__);
		schedule_delayed_work(&data->prox_avg_work,
				msecs_to_jiffies(2000));
		data->prox_avg_enable = 1;
	} else {
		pr_info("[SENSOR]: %s - stop to get avg\n", __func__);
		cancel_delayed_work_sync(&data->prox_avg_work);
		data->prox_avg_enable = 0;
	}

	return size;
}

static ssize_t tmd3782_prox_thd_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);
	u16 thresh_hi = 0,thresh_low = 0;

	tmd3782_i2c_read_word(data, CMD_REG | PRX_MINTHRESHLO, &thresh_low);
	tmd3782_i2c_read_word(data, CMD_REG | PRX_MAXTHRESHLO, &thresh_hi);

	pr_info("[SENSOR]: %s - thresh_hi = %d, thresh_low = %d\n",
		__func__, thresh_hi, thresh_low);

	return sprintf(buf, "%u,%u\n", thresh_hi,thresh_low);
}

static ssize_t tmd3782_prox_thd_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);
	int thresh_value;
	int ret = 0;

	ret = kstrtoint(buf, 10, &thresh_value);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - kstrtoint failed.", __func__);
		return -1;
	}

	pr_info("[SENSOR]: %s - thresh_value = %d\n", __func__, thresh_value);
	data->prox_thd_high = (u16)thresh_value;
	tmd3782_prox_thd_set(data);
	return size;
}

static ssize_t tmd3782_prox_thd_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);
	u16 thresh_hi = 0,thresh_low = 0;

	tmd3782_i2c_read_word(data, CMD_REG | PRX_MINTHRESHLO, &thresh_low);
	tmd3782_i2c_read_word(data, CMD_REG | PRX_MAXTHRESHLO, &thresh_hi);

	pr_info("[SENSOR]: %s - thresh_hi = %d, thresh_low = %d\n",
		__func__, thresh_hi, thresh_low);

	return sprintf(buf, "%u,%u\n", thresh_hi,thresh_low);
}

static ssize_t tmd3782_prox_thd_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);
	int thresh_value;
	int ret = 0;

	ret = kstrtoint(buf, 10, &thresh_value);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - kstrtoint failed.", __func__);
		return -1;
	}

	pr_info("[SENSOR]: %s - thresh_value = %d\n", __func__, thresh_value);
	data->prox_thd_low = (u16)thresh_value;
	tmd3782_prox_thd_set(data);
	return size;
}

static ssize_t tmd3782_prox_adc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u16 adc = 0;
	struct tmd3782_p *data = dev_get_drvdata(dev);

	tmd3782_i2c_read_word(data, CMD_REG | PRX_LO, &adc);
	if (adc > PROX_MAX)
		adc = PROX_MAX;

	return sprintf(buf, "%d\n", adc);
}

static struct device_attribute dev_attr_prox_raw_data =
	__ATTR(raw_data, S_IRUGO, tmd3782_prox_adc_show, NULL);
static DEVICE_ATTR(state, S_IRUGO, tmd3782_prox_adc_show, NULL);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3782_prox_avg_show, tmd3782_prox_avg_store);
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3782_prox_cal_show, tmd3782_prox_cal_store);
static DEVICE_ATTR(prox_offset_pass, S_IRUGO,
	tmd3782_prox_offset_pass_show, NULL);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3782_prox_thd_high_show, tmd3782_prox_thd_high_store);
static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3782_prox_thd_low_show, tmd3782_prox_thd_low_store);

static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_prox_raw_data,
	&dev_attr_state,
	&dev_attr_prox_avg,
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	NULL,
};

static ssize_t tmd3782_prox_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(data->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}

static ssize_t tmd3782_prox_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret = 0;
	struct tmd3782_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	mutex_lock(&data->power_lock);
	pr_info("[SENSOR]: %s - new_value = %u\n", __func__, enable);
	if (enable && !(data->power_state & PROXIMITY_ENABLED)) {
		tmd3782_prox_open_offset(data);
		data->prox_thd_high = PROX_THD_HIGH_DEFAULT + data->prox_offset;
		data->prox_thd_low = PROX_THD_LOW_DEFAULT + data->prox_offset;
		pr_info("[SENSOR]: %s - th_hi = %u, th_low = %u\n", __func__,
				data->prox_thd_high, data->prox_thd_low);
		data->power_state |= PROXIMITY_ENABLED;

		/* interrupt clearing */
		tmd3782_i2c_write_cmd(data,
				CMD_REG | CMD_SPL_FN | CMD_PROXALS_INTCLR);
		tmd3782_prox_led_onoff(data, true);
		tmd3782_chip_on(data);

		input_report_abs(data->prox_input, EVENT_TYPE_PROX, 1);
		input_sync(data->prox_input);

		enable_irq(data->prox_irq);
		enable_irq_wake(data->prox_irq);
	} else if (!enable && (data->power_state & PROXIMITY_ENABLED)) {
		disable_irq_wake(data->prox_irq);
		disable_irq(data->prox_irq);

		data->power_state &= ~PROXIMITY_ENABLED;
		if (data->power_state == 0)
			tmd3782_chip_off(data);
		tmd3782_prox_led_onoff(data, false);
	}
	mutex_unlock(&data->power_lock);
	return size;
}

static struct device_attribute dev_attr_prox_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3782_prox_enable_show, tmd3782_prox_enable_store);

static struct attribute *prox_sysfs_attrs[] = {
	&dev_attr_prox_enable.attr,
	NULL,
};

static struct attribute_group prox_attribute_group = {
	.attrs = prox_sysfs_attrs,
};

static int tmd3782_prox_input_init(struct tmd3782_p *data)
{
	int ret = 0;
	struct input_dev *dev;

	/* allocate proximity sensor input_device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = PROXIMITY_MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_PROX);
	input_set_abs_params(dev, EVENT_TYPE_PROX, 0, 1, 0, 0);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0) {
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &prox_attribute_group);
	if (ret < 0) {
		sensors_remove_symlink(&dev->dev.kobj, dev->name);
		input_unregister_device(dev);
		return ret;
	}

	data->prox_input = dev;
	return 0;
}

static int tmd3782_prox_setup_pin(struct tmd3782_p *data)
{
	int ret;

	ret = gpio_request(data->prox_irq_gpio, "tmd3782_prox_nirq");
	if (ret < 0) {
		pr_err("[SENSOR]: %s - gpio %d request failed (%d)\n",
			__func__, data->prox_irq_gpio, ret);
		return ret;
	}

	ret = gpio_direction_input(data->prox_irq_gpio);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - failed to set gpio %d as input (%d)\n",
			__func__, data->prox_irq_gpio, ret);
		gpio_free(data->prox_irq_gpio);
		return ret;
	}

	return 0;
}

static int tmd3782_prox_setup_irq(struct tmd3782_p *data)
{
	int ret;

	data->prox_irq = gpio_to_irq(data->prox_irq_gpio);
	ret = request_threaded_irq(data->prox_irq, NULL,
			tmd3782_prox_irq_thread, IRQF_TRIGGER_FALLING,
			"tmd3782_prox_irq", data);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - failed to set request_threaded_irq %d"
			" as returning (%d)\n", __func__, data->prox_irq, ret);
		return ret;
	}

	disable_irq(data->prox_irq);
	return ret;
}

static int tmd3782_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct tmd3782_p *data = NULL;

	pr_info("[SENSOR]: %s - Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SENSOR]: %s - i2c_check_functionality error\n",
			__func__);
		goto exit;
	}

	data = kzalloc(sizeof(struct tmd3782_p), GFP_KERNEL);
	if (data == NULL) {
		pr_err("[SENSOR]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	data->prox_irq_gpio = client->irq;
	ret = tmd3782_prox_setup_pin(data);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - could not setup pin\n", __func__);
		goto exit_setup_pin;
	}

	data->i2c_client = client;
	i2c_set_clientdata(client, data);

	/* ID Check */
	ret = i2c_smbus_read_byte_data(client, CMD_REG | CHIPID);
	if (ret != CHIP_ID) {
		pr_err("[SENSOR]: %s - chipid error [%d]\n", __func__, ret);
		goto exit_chip_id;
	}

	/* input device init */
	ret = tmd3782_light_input_init(data);
	if (ret < 0)
		goto exit_light_input_init;

	ret = tmd3782_prox_input_init(data);
	if (ret < 0)
		goto exit_prox_input_init;

	atomic_set(&data->light_poll_delay, TMD3782_LIGHT_DEFAULT_DELAY);
	data->time_count = 0;

	INIT_DELAYED_WORK(&data->light_work, tmd3782_light_work_func);
	INIT_DELAYED_WORK(&data->prox_work, tmd3782_prox_work_func);
	INIT_DELAYED_WORK(&data->prox_avg_work, tmd3782_prox_avg_work_func);

	/* set sysfs for light sensor */
	ret = sensors_register(data->light_dev, data, light_sensor_attrs,
			LIGHT_MODULE_NAME);
	if (ret < 0)
		goto exit_light_sensors_register;

	ret = sensors_register(data->prox_dev, data, prox_sensor_attrs,
			PROXIMITY_MODULE_NAME);
	if (ret < 0)
		goto exit_prox_sensors_register;

	mutex_init(&data->power_lock);
	wake_lock_init(&data->prox_wake_lock,
			WAKE_LOCK_SUSPEND, "prox_wake_lock");

	/* initailize interrupt reporting */
	ret = tmd3782_prox_setup_irq(data);
	if (ret < 0)
		goto exit_request_threaded_irq;

	pr_info("[SENSOR]: %s - Probe done!\n", __func__);

	return 0;

exit_request_threaded_irq:
	wake_lock_destroy(&data->prox_wake_lock);
	mutex_destroy(&data->power_lock);
	sensors_unregister(data->prox_dev, prox_sensor_attrs);
exit_prox_sensors_register:
	sensors_unregister(data->light_dev, light_sensor_attrs);
exit_light_sensors_register:
	sensors_remove_symlink(&data->prox_input->dev.kobj,
			data->prox_input->name);
	sysfs_remove_group(&data->prox_input->dev.kobj,
			&prox_attribute_group);
	input_unregister_device(data->prox_input);
exit_prox_input_init:
	sensors_remove_symlink(&data->light_input->dev.kobj,
			data->light_input->name);
	sysfs_remove_group(&data->light_input->dev.kobj,
			&light_attribute_group);
	input_unregister_device(data->light_input);
exit_light_input_init:
exit_chip_id:
	gpio_free(data->prox_irq_gpio);
exit_setup_pin:
	kfree(data);
exit_kzalloc:
exit:
	pr_err("[SENSOR]: %s - Probe fail!\n", __func__);
	return ret;
}

static void tmd3782_shutdown(struct i2c_client *client)
{
	struct tmd3782_p *data = i2c_get_clientdata(client);

	pr_info("[SENSOR]: %s\n", __func__);
	if (data->power_state) {
		if (data->power_state & LIGHT_ENABLED)
			tmd3782_light_light_disable(data);

		if (data->power_state & PROXIMITY_ENABLED) {
			disable_irq_wake(data->prox_irq);
			disable_irq(data->prox_irq);
			cancel_delayed_work_sync(&data->prox_work);
			if (data->prox_avg_enable == 1)
				cancel_delayed_work_sync(&data->prox_avg_work);
			tmd3782_prox_led_onoff(data, false);
		}

		tmd3782_chip_off(data);
		data->power_state = 0;
	}

	wake_lock_destroy(&data->prox_wake_lock);
}

static int __devexit tmd3782_remove(struct i2c_client *client)
{
	struct tmd3782_p *data = i2c_get_clientdata(client);

	/* device off */
	if (data->power_state) {
		if (data->power_state & LIGHT_ENABLED)
			tmd3782_light_light_disable(data);

		if (data->power_state & PROXIMITY_ENABLED) {
			disable_irq_wake(data->prox_irq);
			disable_irq(data->prox_irq);
			cancel_delayed_work_sync(&data->prox_work);
			if (data->prox_avg_enable == 1)
				cancel_delayed_work_sync(&data->prox_avg_work);
			tmd3782_prox_led_onoff(data, false);
		}

		tmd3782_chip_off(data);
		data->power_state = 0;
	}

	free_irq(data->prox_irq, data);
	gpio_free(data->prox_irq_gpio);

	/* sysfs destroy */
	sensors_unregister(data->light_dev, light_sensor_attrs);
	sensors_unregister(data->prox_dev, prox_sensor_attrs);

	/* symbolic link destroy */
	sensors_remove_symlink(&data->light_input->dev.kobj,
			data->light_input->name);
	sensors_remove_symlink(&data->prox_input->dev.kobj,
			data->prox_input->name);

	/* input sysfs destroy */
	sysfs_remove_group(&data->light_input->dev.kobj,
			&light_attribute_group);
	sysfs_remove_group(&data->prox_input->dev.kobj,
			&prox_attribute_group);

	/* input device destroy */
	input_unregister_device(data->light_input);
	input_unregister_device(data->prox_input);

	wake_lock_destroy(&data->prox_wake_lock);
	mutex_destroy(&data->power_lock);
	kfree(data);

	return 0;
}

static int tmd3782_suspend(struct device *dev)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED) {
		pr_info("[SENSOR]: %s\n", __func__);
		tmd3782_light_light_disable(data);

		if (!(data->power_state & PROXIMITY_ENABLED))
			tmd3782_chip_off(data);
	}

	return 0;
}

static int tmd3782_resume(struct device *dev)
{
	struct tmd3782_p *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED) {
		pr_info("[SENSOR]: %s\n", __func__);
		if (!(data->power_state & PROXIMITY_ENABLED))
			tmd3782_chip_on(data);

		tmd3782_light_light_enable(data);
	}

	return 0;
}

static const struct i2c_device_id tmd3782_device_id[] = {
	{ "tmd3782-i2c", 0 },
	{ }
};

static const struct dev_pm_ops tmd3782_pm_ops = {
	.suspend = tmd3782_suspend,
	.resume = tmd3782_resume
};

static struct i2c_driver tmd3782_i2c_driver = {
	.driver = {
		.name = MODEL_NAME,
		.owner = THIS_MODULE,
		.pm = &tmd3782_pm_ops
	},
	.probe = tmd3782_probe,
	.shutdown = tmd3782_shutdown,
	.remove = __devexit_p(tmd3782_remove),
	.id_table = tmd3782_device_id,
};

static int __init tmd3782_init(void)
{
	return i2c_add_driver(&tmd3782_i2c_driver);
}

static void __exit tmd3782_exit(void)
{
	i2c_del_driver(&tmd3782_i2c_driver);
}

module_init(tmd3782_init);
module_exit(tmd3782_exit);

MODULE_DESCRIPTION("Light & Proximity Sensor device driver for tmd3782");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
