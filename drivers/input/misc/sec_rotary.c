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
#include <linux/regulator/consumer.h>
#include <linux/miscdevice.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <linux/i2c/sec_rotary.h>
#include <linux/atomic.h>
#include <plat/gpio-cfg.h>
#if defined(CONFIG_SYSTEM_LOAD_ANALYZER)
#include <linux/load_analyzer.h>
#endif
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif
#include <linux/trm.h>
#ifdef CONFIG_DISPLAY_EARLY_DPMS
#include <drm/exynos_drm.h>
#endif

#define rotary_MOUSE_MODE

#define I2C_READ_WORD_DATA	1
#define I2C_RETRY_CNT		5
#define I2C_WAKEUP_DELAY	2000
#define MOTION_READ_PERIOD	9

#define WAKELOCK_TIME	100

#define ROTARY_MOTION_ADDR	0x21
#define ROTARY_FEATURE_ADDR	0x31
#define ROTARY_DEVICEID_ADDR	0x90
#define ROTARY_SYSTEM_STATE_ADDR	0x91

#define ROTARY_X_SCALING_VALUE	1
#define ROTARY_MIN_FEATURE_VALUE	0x50
#define MAX_PATTERN	2
#define MAX_HALL	3
#define HALL_INIT	0xff
#define HALL_DETECT_DELAY	10
#define MIN_OFM_DIFF	0

#define VD5376_VALUE		0
#define VD5377_VALUE		77

enum OFM_ID {
	OFM_VD5376 = 0,
	OFM_VD5377,
	OFM_UNKNOWN,
	OFM_MAX,
};

enum OFM_state {
	OFM_Boot,
	OFM_Software_Standby,
	OFM_AutoRunning,
	OFM_Sleep_1,
	OFM_Sleep_2,
	OFM_Sleep_3,
	OFM_ManualRunning,
	OFM_PowerOff,
	OFM_Unkown,
	OFM_MaxState,
};

extern struct class *sec_class;

static int rotary_get_system_state(struct rotary_ddata *rotary, enum OFM_state *state);
static void rotary_enable_irq(struct rotary_ddata *rotary);
static void rotary_disable_irq(struct rotary_ddata *rotary);
static int rotary_i2c_write(struct i2c_client *client, u_int8_t index, u_int8_t data);
static int rotary_i2c_read(struct i2c_client *client, u16 reg, u_int8_t *buff, u16 length);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void rotary_early_suspend(struct early_suspend *h);
static void rotary_late_resume(struct early_suspend *h);
#endif
static int rotary_open(struct input_dev *input);
static void rotary_close(struct input_dev *input);

#ifdef CONFIG_SLEEP_MONITOR
static int rotary_get_sleep_monitor_cb(void* priv, unsigned int *raw_val, int check_level, int caller_type);

static struct sleep_monitor_ops rotary_sleep_monitor_ops = {
	 .read_cb_func = rotary_get_sleep_monitor_cb,
};

static int rotary_get_sleep_monitor_cb(void* priv, unsigned int *raw_val, int check_level, int caller_type)
{
	struct rotary_ddata *rotary = priv;
	enum OFM_state hard_state;
	int state = DEVICE_UNKNOWN;
	int ret;

	if (check_level == SLEEP_MONITOR_CHECK_SOFT) {
		if (rotary->power_state)
			state = DEVICE_ON_ACTIVE1;
		else
			state = DEVICE_POWER_OFF;

		*raw_val = state;

		dev_dbg(&rotary->mouse_dev->dev,
			"%s:power_state[%d]state[%d]\n",
			__func__, rotary->power_state, state);
	} else if (check_level == SLEEP_MONITOR_CHECK_HARD) {
		ret = rotary_get_system_state(rotary, &hard_state);
		if (ret < 0) {
			dev_err(&rotary->mouse_dev->dev, "%s: system stata read failed.[%d]\n", __func__, ret);
			state = DEVICE_UNKNOWN;
			goto out;
		}

		*raw_val = hard_state;

		if (hard_state == OFM_Unkown)
			state = DEVICE_UNKNOWN;
		else if (hard_state == OFM_PowerOff)
			state = DEVICE_POWER_OFF;
		else if (hard_state == OFM_Sleep_1)
			state = DEVICE_ON_LOW_POWER;
		else if (hard_state == OFM_Sleep_2)
			state = DEVICE_ON_LOW_POWER;
		else if (hard_state == OFM_Sleep_3)
			state = DEVICE_ON_LOW_POWER;
		else
			state = DEVICE_ON_ACTIVE1;
		dev_dbg(&rotary->mouse_dev->dev,
			"%s:hard_state[%d]state[%d]\n",
			__func__, hard_state, state);
	} else {
		state = DEVICE_UNKNOWN;
		*raw_val = OFM_Unkown;
		dev_err(&rotary->mouse_dev->dev,
			"%s:Unknown check_level. [%d]\n",
			__func__, check_level);
	}

out:
	return state;
}
#endif

static int rotary_init_sequece(struct rotary_ddata *rotary)
{
	int ret;
	int sequece_cnt;
	int sequece_num;
	int retry_cnt = I2C_RETRY_CNT;
	u_int8_t buf[2] = {0, };
	const char sequece[][2] = {
		{0x29, ROTARY_MIN_FEATURE_VALUE},//min feeature set
		{0x27, 0x00},//Allows X to be inverted
		{0x2a, ROTARY_X_SCALING_VALUE},//Set 50 CPI for X data
		{0x2b, 0x01},//Set 50 CPI for Y data
		{0x51, 0x02},//Set Sunlight mode
		{0x28, 0x74},//Set HPF 5x5 set
		{0x4e, 0x08},//Set increase exp time
		{0x4f, 0x08},//Set decrease exp time
		{0x4a, 0x10},//Set Min exposure time
		{0x4b, 0x10},//Set exposure update every time
		{0x03, 0xfc},//Set DMIB DAC Vref setting = 1.6v
		{0x1c, 0x14},//Set frame rate 2.9k fps (350us period)
		{0x82, 0x10},//Set power mode control (sleep1)
		{0x05, 0x1d},//system configuration (motion pin active high & Automatic Mode)
	};

	ret = rotary_i2c_write(rotary->client, 0x16, 0x1e); //software reset
	if (ret < 0) {
		dev_err(&rotary->mouse_dev->dev,
			"%s: i2c write failed. add=[0x%2x],data=[0x%2x]\n",
			__func__, 0x16, 0x1e);
		return ret;
	}
	msleep(1);

	sequece_num = sizeof(sequece) / (2*sizeof(char));

	for (sequece_cnt=0; sequece_cnt < sequece_num; sequece_cnt++) {
		ret = rotary_i2c_write(rotary->client,\
			sequece[sequece_cnt][0], sequece[sequece_cnt][1]);
		if (ret < 0) {
			dev_err(&rotary->mouse_dev->dev,
				"%s: i2c write failed. add=[0x%2x],data=[0x%2x]\n",
				__func__, sequece[sequece_cnt][0], sequece[sequece_cnt][1]);
			break;
		}
	}
	msleep(1);

	do {
		ret = rotary_i2c_read(rotary->client, 0x21, buf, I2C_READ_WORD_DATA);
		if ((ret > 0) && (buf[0] || buf[1])) {
			dev_info(&rotary->mouse_dev->dev,"remove waste data (%d/%d)\n", buf[0], buf[1]);
			retry_cnt--;
			msleep(10);
		} else
			break;
	} while(retry_cnt);

	return ret;
 }

static int rotary_ctrl_power(struct rotary_ddata *rotary, bool power)
{
	struct rotary_platform_data *pdata = rotary->pdata;
	static struct regulator *regulator_pwr = NULL;
	s32 ret;

	if (!regulator_pwr) {
		regulator_pwr = regulator_get(NULL, "vdd_ofm_2.8v");
		if (IS_ERR(regulator_pwr)) {
			dev_err(&rotary->mouse_dev->dev, "%s: failed to get ldo14 regulator\n", __func__);
			return PTR_ERR(regulator_pwr);
		}
	}

	if (power) {
		gpio_set_value(pdata->powerdown_pin, 1);
		gpio_set_value(pdata->standby_pin, 1);

		ret = regulator_set_voltage(regulator_pwr, 2800000, 2800000);
		if (ret) {
			dev_err(&rotary->mouse_dev->dev, "%s: unable to set voltage for avdd_vreg, %d\n", __func__, ret);
			return ret;
		}

		ret = regulator_enable(regulator_pwr);
		if (ret) {
			dev_err(&rotary->mouse_dev->dev, "%s: unable to enable regulator_pwr, %d\n", __func__, ret);
			return ret;
		}

		ret = regulator_is_enabled(regulator_pwr);
		if (ret)
			pr_info("%s: regulator_pwr is enabled, %d\n", __func__, ret);
		else {
			dev_err(&rotary->mouse_dev->dev, "%s: regulator_pwr is disabled, %d\n", __func__, ret);
			return ret;
		}
		msleep(1); //wating for gpio get stable

		gpio_set_value(pdata->powerdown_pin, 0);
		gpio_set_value(pdata->standby_pin, 0);

		msleep(1); //wating for gpio get stable

		ret = rotary_init_sequece(rotary);
		if (ret < 0) {
			dev_err(&rotary->mouse_dev->dev, "%s: init sequence failed. %d\n", __func__, ret);
			return ret;
		}
		rotary->power_state = true;
		rotary_enable_irq(rotary);
	} else {
		rotary_disable_irq(rotary);
		regulator_disable(regulator_pwr);
		rotary->power_state = false;
	}

	dev_info(&rotary->mouse_dev->dev, "%s: 2.8v=[%s]\n", __func__,\
		regulator_is_enabled(regulator_pwr)?"ON":"OFF");

	return 0;
}

static void rotary_motion_handler(struct rotary_ddata *rotary)
{
	struct input_dev *input_dev = rotary->mouse_dev;
	u_int8_t buf[2] = {0, };
	int ret, feature = 0;
	s8  x,y;

	wake_lock_timeout
		(&rotary->wake_lock, WAKELOCK_TIME);

	ret = rotary_i2c_read(rotary->client, ROTARY_MOTION_ADDR, buf, I2C_READ_WORD_DATA);
	if (ret < 0) {
		dev_err(&input_dev->dev, "%s: OFM_MOTION_ADDR read failed.[%d]\n", __func__, ret);
		goto out;
	}

	x = buf[0];
	y = buf[1];

	ret = rotary_i2c_read(rotary->client, ROTARY_FEATURE_ADDR, buf, I2C_READ_WORD_DATA);
	if (ret < 0)
		dev_err(&input_dev->dev, "%s: OFM_FEATURE_ADDR read failed.[%d]\n", __func__, ret);
	else
		feature = buf[0];

	if ( x != 0 || y != 0 ) {
		rotary->x_sum += x;
		rotary->y_sum += y;
	} else
		goto out;

	input_report_rel(input_dev, REL_X, x);
	input_report_rel(input_dev, REL_Y, y);
	input_report_rel(input_dev, REL_Z, feature);
	input_sync(input_dev);

#if defined (ROTARY_BOOSTER)
	rotary_booster_turn_on();
#endif

	dev_info(&input_dev->dev,\
		"%s: x=%3d, x_sum=%4d y=%3d, y_sum=%4d, f=%3d\n",\
		__func__, x, rotary->x_sum, y, rotary->y_sum, feature);

out:
	return;
}

static void rotary_motion_handler_work(struct work_struct *work)
{
	struct rotary_ddata *rotary = container_of(work,
				struct rotary_ddata, motion_dwork.work);

	if (!gpio_get_value(rotary->pdata->motion_pin)) {
		enable_irq(rotary->motion_irq);
		return;
	}

	rotary_motion_handler(rotary);

	schedule_delayed_work(&rotary->motion_dwork,
			msecs_to_jiffies(MOTION_READ_PERIOD));

	return;
}

static irqreturn_t rotary_motion_interrupt(int irq, void *dev_id)
{
	struct rotary_ddata *rotary = (struct rotary_ddata *)dev_id;
	struct input_dev *mouse_dev = rotary->mouse_dev;
	int ret;

	if (rotary->always_on) {
		ret = wait_event_timeout(rotary->wait_q, rotary->wakeup_state,
					msecs_to_jiffies(I2C_WAKEUP_DELAY));
		if (!ret)
			dev_err(&mouse_dev->dev,
				"%s: wakeup_state timeout. rotary->wakeup_state=[%d]\n",
				__func__, rotary->wakeup_state);
	}

	disable_irq_nosync(rotary->motion_irq);

	rotary_motion_handler(rotary);

	schedule_delayed_work(&rotary->motion_dwork,
			msecs_to_jiffies(MOTION_READ_PERIOD));

	return IRQ_HANDLED;
}

enum hall_patten {
	Patten_A = 0,
	Patten_B = 1,
	Patten_C = 2,
};

const static int pattern[MAX_HALL][MAX_PATTERN] = {{Patten_C, Patten_B},
					          {Patten_A, Patten_C},
					          {Patten_B, Patten_A}};

static int hall_sensor_get_status(struct rotary_ddata *rotary)
{
	struct input_dev *input_dev = rotary->hall_dev;
	int value;

	rotary->a_status = gpio_get_value(rotary->pdata->hall_a_pin);
	rotary->b_status = gpio_get_value(rotary->pdata->hall_b_pin);

	value = (rotary->a_status << 1) | rotary->b_status;

	dev_dbg(&input_dev->dev, "%s: a=[%u], b=[%u], value=[%u]\n",
		__func__, rotary->a_status, rotary->b_status, value);

	return value;
}

static int hall_sensor_get_est_value(int prev, int direction)
{
	if (prev > Patten_C)
		return -2;

	return pattern[prev][direction];
}

static int hall_sensor_check_valid(struct rotary_ddata *rotary, int status)
{
	struct input_dev *input_dev = rotary->hall_dev;
	static int motion_direction = 0;
	int est_value;

	if (status < Patten_A || status > Patten_C) {
		dev_err(&input_dev->dev,
			"%s: invalid status value.[%d]\n", __func__, status);
		return -1;
	}

	if (status == rotary->last_status) {
		dev_dbg(&input_dev->dev,
			"%s: status value is same as last_status.[%d]\n", __func__, status);
		return -1;
	}

	if (HALL_INIT == rotary->last_status) {
		dev_dbg(&input_dev->dev,  "%s: hall init status.\n", __func__);
		return 0;
	}

	if (!rotary->power_state) {
		dev_dbg(&input_dev->dev,  "%s: OFM power off.[%d]\n", __func__, status);
		return 0;
	}

	if (MIN_OFM_DIFF > abs(rotary->x_sum - rotary->last_x_sum)) {
		dev_err(&input_dev->dev,
			"%s: x_sum value is too short. [%d][%d]\n",
				__func__, status,
				rotary->x_sum - rotary->last_x_sum);
		return -1;
	}

	if ((rotary->x_sum - rotary->last_x_sum) > 0)
		motion_direction = 1;
	else if ((rotary->x_sum - rotary->last_x_sum) < 0)
		motion_direction = 0;

	est_value = hall_sensor_get_est_value (rotary->last_status, motion_direction);

	if (est_value < 0) {
		dev_err(&input_dev->dev,
			"%s: estimated value get failed. e=[%d], s=[%d]\n",
			__func__, est_value, rotary->last_status);
		return -1;
	}

	if (est_value != status) {
		dev_err(&input_dev->dev, "%s: status=[%d], est_value=[%d], d=[%d]\n",
			__func__, status, est_value, motion_direction);
		return -1;
	}

	return 0;
}

static irqreturn_t hall_status_detect(int irq, void *dev_id)
{
	struct rotary_ddata *rotary = dev_id;
	struct input_dev *input_dev = rotary->hall_dev;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	struct display_early_dpms_nb_event event;
#endif
	int status;

	status = hall_sensor_get_status(rotary);

	if (hall_sensor_check_valid(rotary, status)) {
		dev_dbg(&input_dev->dev, "%s: read invalid data [%d][%d]\n",
				__func__, rotary->last_status, status);
		goto out;
	}

	wake_lock_timeout(&rotary->wake_lock, WAKELOCK_TIME);

	input_report_rel(input_dev, REL_WHEEL, status+1);
	input_sync(input_dev);

#ifdef CONFIG_DISPLAY_EARLY_DPMS
	if (!rotary->hall_resume_state) {
		event.id = DISPLAY_EARLY_DPMS_ID_PRIMARY;
		event.data = (void *)true;
		display_early_dpms_nb_send_event(DISPLAY_EARLY_DPMS_MODE_SET,
					(void *)&event);
	}
#endif
	rotary->last_status = status;
	rotary->hall_sum++;

	dev_info(&input_dev->dev, "%s: detent. status=[%d], sum=[%d], ofm=[%d]\n",
			__func__, status, rotary->hall_sum, rotary->x_sum - rotary->last_x_sum);

	rotary->last_x_sum = rotary->x_sum;
out:

	return IRQ_HANDLED;
}

static int rotary_i2c_read(struct i2c_client *client, u16 reg, u_int8_t *buff, u16 length)
{
	int result;
	int retry_cnt = I2C_RETRY_CNT;

	do {
		result= i2c_smbus_read_word_data(client, reg);
		if (result < 0) {
			retry_cnt--;
			msleep(10);
		} else {
			*buff = (u16)result;
			return length;
		}
	} while(retry_cnt);

	dev_err(&client->dev, "%s: I2C read error\n", __func__);

	return result;
}

static int rotary_i2c_write(struct i2c_client *client, u_int8_t index, u_int8_t data)
{
	u_int8_t buf[2] = {index , data};
	int retry_cnt = I2C_RETRY_CNT;
	int result;

	do {
		result= i2c_master_send(client, buf, 2);
		if (result < 0) {
			retry_cnt--;
			msleep(10);
		} else
			return 0;
	} while(retry_cnt);

	dev_err(&client->dev,
		"%s: I2C write error. index(0x%x) data(0x%x) return (0x%x)\n",
		__func__, index, data, result);

	return result;
}


static void rotary_enable_irq(struct rotary_ddata *rotary)
{
	enable_irq(rotary->motion_irq);

	dev_info(&rotary->mouse_dev->dev, " %s: Enable motion irq %d\n",
			__func__, rotary->motion_irq);
}

static void rotary_disable_irq(struct rotary_ddata *rotary)
{
	disable_irq(rotary->motion_irq);

	dev_info(&rotary->mouse_dev->dev, " %s: Disable motion irq %d\n",
			__func__, rotary->motion_irq);
}

static ssize_t rotary_min_feature(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);

	dev_info(&rotary->mouse_dev->dev, KERN_INFO
		"%s: scaling=[%d]\n", __func__, ROTARY_MIN_FEATURE_VALUE);
	return sprintf(buf,"%d\n", ROTARY_MIN_FEATURE_VALUE);
}


static ssize_t rotary_scaling_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);
	const int scaling = ROTARY_X_SCALING_VALUE * 50;

	dev_info(&rotary->mouse_dev->dev, KERN_INFO "%s: scaling=[%d]\n", __func__, scaling);
	return sprintf(buf,"%d\n", scaling);
}

static ssize_t rotary_device_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);
	const char* id_str[OFM_MAX] = {"VD5376", "VD5377", "UNKNOWN_DEVICE"};
	int ret, count = 0;
	u_int8_t id = 0;

	ret = rotary_i2c_read(rotary->client, ROTARY_DEVICEID_ADDR, &id, I2C_READ_WORD_DATA);
	if (ret < 0) {
		dev_err(&rotary->mouse_dev->dev,
			"%s: ROTARY_DEVICEID_ADDR read failed.[%d]\n", __func__, ret);
		goto out;
	}

	if (id == VD5376_VALUE)
		id = OFM_VD5376;
	else if (id == VD5377_VALUE)
		id = OFM_VD5377;
	else
		id = OFM_UNKNOWN;

	dev_info(&rotary->mouse_dev->dev, "%s: OFM_ID=[%d][%s]\n", __func__, id, id_str[id]);
	count = sprintf(buf,"%s\n", id_str[id]);

out:
	return count;
}

static ssize_t rotary_sum_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);
	int count = 0;

	dev_info(&rotary->mouse_dev->dev, KERN_INFO " access rotary_level_read!!!\n");
	count = sprintf(buf,"x_sum=%d,y_sum=%d\n", rotary->x_sum, rotary->y_sum);

	return count;
}

static ssize_t rotary_sum_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);

	rotary->x_sum = 0;
	rotary->y_sum = 0;

	return count;
}

static ssize_t show_rotary_alway_on(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);

	dev_info(&rotary->mouse_dev->dev, "%s: rotary->always_on=[%d]\n",
			__func__, rotary->always_on);

	return snprintf(buf, PAGE_SIZE, "%d\n", rotary->always_on);
}

static ssize_t store_rotary_alway_on(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);
	int mode = -1;

	sscanf(buf, "%d", &mode);

	dev_info(&rotary->mouse_dev->dev, "%s: alway_on=%d\n", __func__, mode);

	if (mode) {
		rotary->wakeup_state = true;
		rotary->always_on = true;
	} else
		rotary->always_on = false;

	return count;
}

static int rotary_get_system_state(struct rotary_ddata *rotary, enum OFM_state *state)
{
	int ret;
	u_int8_t buf = OFM_Unkown;

	if (!rotary->power_state) {
		dev_dbg(&rotary->mouse_dev->dev,
			"%s: device power off.[%d]\n", __func__, rotary->power_state);
		buf = OFM_PowerOff;
		goto out;
	}

	ret = rotary_i2c_read(rotary->client, ROTARY_SYSTEM_STATE_ADDR, &buf, I2C_READ_WORD_DATA);
	if (ret < 0) {
		dev_err(&rotary->mouse_dev->dev,
			"%s: ROTARY_SYSTEM_STATE_ADDR read failed.[%d]\n", __func__, ret);
		return ret;
	}

	if (buf > OFM_Unkown) {
		dev_err(&rotary->mouse_dev->dev,
			"%s: Unknow state readed.[%d]\n", __func__, buf);
		buf = OFM_Unkown;
	}
out:
	*state = buf;

	return 0;
}

static ssize_t rotary_system_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);
	const char* state_str[OFM_MaxState] = {"Boot", "Software Standby",\
						"AutoRunning", "Sleep_1", "Sleep_2",\
						"Sleep_3", "ManualRunning", "PowerOff",\
						"Unkown"};
	enum OFM_state state;
	int ret, count = 0;

	ret = rotary_get_system_state(rotary, &state);
	if (ret < 0) {
		dev_err(&rotary->mouse_dev->dev, "%s: system stata read failed.[%d]\n", __func__, ret);
		goto out;
	}

	dev_info(&rotary->mouse_dev->dev, "%s: system stata=[%d][%s]\n", __func__, state, state_str[state]);
	count = sprintf(buf,"%s\n", state_str[state]);
out:
	return count;
}


static ssize_t rotary_hall_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);
	int ret, count = 0;

	ret = hall_sensor_get_status(rotary);

	dev_info(&rotary->hall_dev->dev, "%s: hall_sensor_get_status=[%d]\n", __func__, ret);

	count = sprintf(buf,"%d\n", ret);

	return count;
}

static struct device_attribute dev_attr_rotary_show_hall_status = __ATTR(hall_status, 0664, rotary_hall_status_show, NULL);
static struct device_attribute dev_attr_rotary_show_system_state = __ATTR(system_state, 0664, rotary_system_state_show, NULL);
static struct device_attribute dev_attr_rotary_show_min_feature = __ATTR(min_feature, 0664, rotary_min_feature, NULL);
static struct device_attribute dev_attr_rotary_show_scaling = __ATTR(scaling, 0664, rotary_scaling_show, NULL);
static struct device_attribute dev_attr_rotary_show_id = __ATTR(device_id, 0664, rotary_device_id_show, NULL);
static struct device_attribute dev_attr_rotary_show_sum = __ATTR(show_sum, 0664, rotary_sum_show, NULL);
static struct device_attribute dev_attr_rotary_clear_sum = __ATTR(clear_sum, 0664, NULL, rotary_sum_store);
static struct device_attribute dev_attr_rotary_mode = __ATTR(mode, 0664, show_rotary_alway_on, store_rotary_alway_on);

static struct attribute *sec_rotary_attributes[] = {
	&dev_attr_rotary_show_hall_status.attr,
	&dev_attr_rotary_show_system_state.attr,
	&dev_attr_rotary_show_min_feature.attr,
	&dev_attr_rotary_show_scaling.attr,
	&dev_attr_rotary_show_id.attr,
	&dev_attr_rotary_show_sum.attr,
	&dev_attr_rotary_clear_sum.attr,
	&dev_attr_rotary_mode.attr,
	NULL,
};

static struct attribute_group sec_rotary_attr_group = {
	.attrs = sec_rotary_attributes,
};

#ifdef CONFIG_OF
static struct rotary_platform_data *rotary_parse_dt(struct device *dev)
{
	struct rotary_platform_data *pdata;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "%s: dev->of_node is null.\n", __func__);
		return NULL;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "%s: failed to allocate platform data\n", __func__);
		return NULL;
	}

	if(!of_find_property(np, "rotary-gpio-stb", NULL)) {
		pdata->standby_pin = -1;
		dev_err(dev, "failed to get rotary-gpio-stb property\n");
	}
	pdata->standby_pin = of_get_named_gpio_flags(
				np, "rotary-gpio-stb", 0, NULL);

	if(!of_find_property(np, "rotary-gpio-pd", NULL)) {
		pdata->powerdown_pin = -1;
		dev_err(dev, "failed to get rotary-gpio-pd property\n");
	}
	pdata->powerdown_pin = of_get_named_gpio_flags(
				np, "rotary-gpio-pd", 0, NULL);

	if(!of_find_property(np, "rotary-gpio-mtn", NULL)) {
		pdata->standby_pin = -1;
		dev_err(dev, "failed to get rotary-gpio-mtn property\n");
	}

	pdata->motion_pin = of_get_named_gpio_flags(
				np, "rotary-gpio-mtn", 0, NULL);
	pdata->hall_a_pin = of_get_named_gpio_flags(
				np, "rotary-gpio-hall-a", 0, NULL);
	pdata->hall_b_pin = of_get_named_gpio_flags(
				np, "rotary-gpio-hall-b", 0, NULL);

	return pdata;
}
#endif

static int rotary_gpio_init(struct i2c_client *client)
{
	struct rotary_ddata *rotary = i2c_get_clientdata(client);
	struct rotary_platform_data *pdata = rotary->pdata;
	int ret = 0;

	if (gpio_is_valid(pdata->standby_pin)) {
		ret = gpio_request(pdata->standby_pin, "OFM_standby");
		if (ret)
			dev_err(&client->dev, "unable to set GPIO rotary_standby, %d\n", ret);

		gpio_direction_output(pdata->standby_pin, 0);
		dev_info(&rotary->mouse_dev->dev, "%s: pdata->standby_pin= %d, [%d]\n",
			__func__, pdata->standby_pin, gpio_get_value(pdata->standby_pin));
	}

	if (gpio_is_valid(pdata->powerdown_pin)) {
		ret = gpio_request(pdata->powerdown_pin, "OFM_powerdown");
		if (ret)
			dev_err(&client->dev, "unable to set GPIO rotary_powerdown, %d\n", ret);

		gpio_direction_output(pdata->powerdown_pin, 1);
		dev_info(&rotary->mouse_dev->dev, "%s: pdata->powerdown_pin= %d, [%d]\n",
			__func__, pdata->powerdown_pin, gpio_get_value(pdata->powerdown_pin));

		ret = s3c_gpio_setpull(pdata->powerdown_pin, S5P_GPIO_PD_UPDOWN_DISABLE);
		if (ret) {
			dev_err(&rotary->mouse_dev->dev,
				"%s: unable to set pull down [%d]\n",
				__func__, pdata->powerdown_pin);
		}
	}

	return ret;
}

static void rotary_mouse_remove(struct rotary_ddata *rotary)
{
	struct	rotary_platform_data *pdata = rotary->pdata;

	gpio_free(pdata->standby_pin);
	gpio_free(pdata->powerdown_pin);
	input_unregister_device(rotary->mouse_dev);
	dev_set_drvdata(&rotary->client->dev, NULL);
	input_free_device(rotary->mouse_dev);
}

static int rotary_mouse_init(struct rotary_ddata *rotary)
{
	struct	rotary_platform_data *pdata = rotary->pdata;
	int result;

	rotary->mouse_dev = input_allocate_device();
	if (!rotary->mouse_dev) {
		dev_err(&rotary->client->dev, "%s: Failed to allocate input device.\n", __func__);
		goto err_allocate_device;
	}

	rotary->mouse_dev->name = ROTARY_NAME;
	rotary->mouse_dev->id.bustype = BUS_I2C;
	rotary->mouse_dev->dev.parent = &rotary->client->dev;
	rotary->mouse_dev->phys = "MOUSE";
	rotary->mouse_dev->id.vendor = 0x0001;
	rotary->mouse_dev->id.product = 0x0001;
	rotary->mouse_dev->id.version = 0x0100;
	rotary->mouse_dev->open = rotary_open;
	rotary->mouse_dev->close = rotary_close;
	rotary->always_on = pdata->always_on;

	input_set_drvdata(rotary->mouse_dev, rotary);

	__set_bit(EV_REL, rotary->mouse_dev->evbit);
	__set_bit(EV_KEY, rotary->mouse_dev->evbit);
	__set_bit(REL_X, rotary->mouse_dev->relbit);
	__set_bit(REL_Y, rotary->mouse_dev->relbit);
	__set_bit(REL_Z, rotary->mouse_dev->relbit);
	__set_bit(BTN_LEFT, rotary->mouse_dev->keybit);

	input_set_capability(rotary->mouse_dev, EV_REL, REL_X);
	input_set_capability(rotary->mouse_dev, EV_REL, REL_Y);
	input_set_capability(rotary->mouse_dev, EV_REL, REL_Z);

	INIT_DELAYED_WORK(&rotary->motion_dwork, rotary_motion_handler_work);

	result = rotary_gpio_init(rotary->client);
	if (result) {
		dev_err(&rotary->mouse_dev->dev, "%s: rotary gpio init failed", __func__);
		goto err_gpio_request;
	}

	result = rotary_ctrl_power(rotary, true);
	if (result) {
		dev_err(&rotary->mouse_dev->dev, "%s: rotary power control failed", __func__);
		goto err_ctrl_power;
	}

	result = request_threaded_irq(rotary->client->irq,
			NULL, rotary_motion_interrupt,
			IRQF_TRIGGER_HIGH  | IRQF_ONESHOT,
			ROTARY_NAME, rotary);
	if (result < 0) {
		dev_err(&rotary->mouse_dev->dev, "%s: Failed to register interrupt [%d][%d]\n",
			__func__, rotary->client->irq, result);
		goto err_threaded_irq;
	}

	result = input_register_device(rotary->mouse_dev);
	if (result) {
		dev_err(&rotary->mouse_dev->dev, "%s:Unable to register %s input device\n",\
			 __FUNCTION__, rotary->mouse_dev->name);
		goto err_register_device;
	}

	return 0;

err_register_device:
	free_irq(rotary->client->irq, rotary);
err_threaded_irq:
	rotary_ctrl_power(rotary, false);
err_ctrl_power:
	gpio_free(pdata->standby_pin);
	gpio_free(pdata->powerdown_pin);
err_gpio_request:
	dev_set_drvdata(&rotary->client->dev, NULL);
	input_free_device(rotary->mouse_dev);
err_allocate_device:
	return -1;
}

static void rotary_hall_remove(struct rotary_ddata *rotary)
{
	struct rotary_platform_data *pdata = rotary->pdata;

	gpio_free(pdata->hall_b_pin);
	gpio_free(pdata->hall_a_pin);
	input_unregister_device(rotary->hall_dev);
	input_free_device(rotary->hall_dev);
}

static int rotary_hall_init(struct rotary_ddata *rotary)
{
	struct rotary_platform_data *pdata = rotary->pdata;
	int result;

	rotary->hall_dev = input_allocate_device();
	if (!rotary->hall_dev) {
		dev_err(&rotary->hall_dev->dev, "Failed to allocate input device.\n");
		goto err_allocate_device;
	}

	rotary->hall_dev->name = HALL_NAME;
	rotary->hall_dev->id.bustype = BUS_VIRTUAL;
	rotary->hall_dev->dev.parent = &rotary->client->dev;
	rotary->hall_dev->phys = "HALL";
	rotary->hall_dev->id.vendor = 0x0001;
	rotary->hall_dev->id.product = 0x0001;
	rotary->hall_dev->id.version = 0x0100;
	rotary->last_status = HALL_INIT;
	input_set_drvdata(rotary->hall_dev, rotary);

	__set_bit(EV_REL, rotary->hall_dev->evbit);
	__set_bit(EV_KEY, rotary->hall_dev->evbit);
	__set_bit(REL_X, rotary->hall_dev->relbit);
	__set_bit(REL_Y, rotary->hall_dev->relbit);
	__set_bit(BTN_LEFT, rotary->hall_dev->keybit);
	__set_bit(REL_WHEEL, rotary->hall_dev->relbit);

	input_set_capability(rotary->hall_dev, EV_REL, REL_X);
	input_set_capability(rotary->hall_dev, EV_REL, REL_Y);
	input_set_capability(rotary->hall_dev, EV_REL, REL_WHEEL);

	result = input_register_device(rotary->hall_dev);
	if (result) {
		dev_err(&rotary->hall_dev->dev, "%s:Unable to register %s input device\n",\
			 __func__, rotary->hall_dev->name);
		goto err_register_device;
	}

	result = gpio_request(pdata->hall_a_pin, "hall_sensor_a");
	if (result) {
		dev_err(&rotary->hall_dev->dev, "%s:"\
			" unable to request hall_sensor_a [%d]\n",\
			__func__, pdata->hall_a_pin);
		goto err_a_pin_request;
	}

	result = gpio_direction_input(pdata->hall_a_pin);
	if (result) {
		dev_err(&rotary->hall_dev->dev,
			"%s: unable to request input pin [%d]\n",
			__func__, pdata->hall_a_pin);
		goto err_a_pin_direction;
	}

	result = s3c_gpio_setpull(pdata->hall_a_pin, S3C_GPIO_PULL_DOWN);
	if (result) {
		dev_err(&rotary->hall_dev->dev,
			"%s: unable to set pull down [%d]\n",
			__func__, pdata->hall_a_pin);
		goto err_a_pin_direction;
	}

	rotary->hall_a_irq = gpio_to_irq(pdata->hall_a_pin);

	result = gpio_request(pdata->hall_b_pin, "hall_sensor_b");
	if (result) {
		dev_err(&rotary->hall_dev->dev, "%s:"\
			" unable to request hall_sensor_a [%d]\n",\
			__func__, pdata->hall_b_pin);
		goto err_a_pin_direction;
	}

	result = gpio_direction_input(pdata->hall_b_pin);
	if (result) {
		dev_err(&rotary->hall_dev->dev,
			"%s: unable to request input pin [%d]\n",
			__func__, pdata->hall_b_pin);
		goto err_b_pin_direction;
	}

	result = s3c_gpio_setpull(pdata->hall_b_pin, S3C_GPIO_PULL_DOWN);
	if (result) {
		dev_err(&rotary->hall_dev->dev,
			"%s: unable to set pull down [%d]\n",
			__func__, pdata->hall_b_pin);
		goto err_b_pin_direction;
	}

	rotary->hall_b_irq = gpio_to_irq(pdata->hall_b_pin);

	result = request_threaded_irq(rotary->hall_a_irq , NULL, hall_status_detect,
		IRQF_DISABLED |IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"hall_a_status", rotary);
	if (result < 0) {
		dev_err(&rotary->hall_dev->dev, "failed to request hall irq[%d] gpio %d\n",
		rotary->hall_a_irq , pdata->hall_a_pin);
		goto err_threaded_irq;
	}

	result = request_threaded_irq(rotary->hall_b_irq , NULL, hall_status_detect,
		IRQF_DISABLED |IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"hall_b_status", rotary);
	if (result < 0) {
		dev_err(&rotary->hall_dev->dev, "failed to request hall irq[%d] gpio %d\n",
		rotary->hall_b_irq , pdata->hall_b_pin);
		goto err_threaded_irq;
	}

	rotary->hall_resume_state = true;

	return 0;

err_threaded_irq:
	wake_lock_destroy(&rotary->wake_lock);
err_b_pin_direction:
	gpio_free(pdata->hall_b_pin);
err_a_pin_direction:
	gpio_free(pdata->hall_a_pin);
err_a_pin_request:
	input_unregister_device(rotary->hall_dev);
err_register_device:
	input_free_device(rotary->hall_dev);
err_allocate_device:
	return -1;
}

static int rotary_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct	rotary_ddata *rotary;
	int result;

	rotary = kzalloc(sizeof(struct rotary_ddata), GFP_KERNEL);
	if (!rotary)
	{
		dev_err(&client->dev, "%s: failed to allocate driver data\n", __func__);
		result = -ENOMEM;
		goto err_kzalloc;
	}

	rotary->client = client;
	rotary->motion_irq = rotary->client->irq;

#ifdef CONFIG_OF
	rotary->pdata = rotary_parse_dt(&client->dev);
#else
	rotary->pdata = client->dev.platform_data;
#endif

	i2c_set_clientdata(rotary->client, rotary);
	init_waitqueue_head(&rotary->wait_q);

	wake_lock_init(&rotary->wake_lock,
		WAKE_LOCK_SUSPEND, "rotary_wake_lock");

	result = rotary_mouse_init(rotary);
	if (result < 0) {
		dev_err(&client->dev, "%s: Failed to init mouse_device.\n", __func__);
		goto err_mouse_init;
	}

	result = rotary_hall_init(rotary);
	if (result < 0) {
		dev_err(&client->dev, "%s: Failed to init hall_device.\n", __func__);
		goto err_hall_init;
	}

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops(rotary, &rotary_sleep_monitor_ops,
			SLEEP_MONITOR_ROTARY);
#endif

	rotary->sec_dev = device_create(sec_class, NULL, 0, rotary, ROTARY_NAME);
	if (IS_ERR(rotary->sec_dev)) {
		dev_err(&rotary->mouse_dev->dev, "Failed to create device for the sysfs\n");
		goto err_create_device;
	}

	result = sysfs_create_group(&rotary->sec_dev->kobj, &sec_rotary_attr_group);
	if (result) {
		dev_err(&rotary->mouse_dev->dev, "Failed to create sysfs group\n");
		goto err_create_group;
	}

	device_init_wakeup(&rotary->hall_dev->dev, true);
	rotary->wakeup_state = true;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	device_set_early_complete(&rotary->hall_dev->dev, EARLY_COMP_SLAVE);
#endif
	dev_info(&rotary->hall_dev->dev, "%s done successfully\n", __func__);

	return 0;

err_create_group:
	device_destroy(sec_class,rotary->sec_dev->devt);
err_create_device:
#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_unregister_ops(SLEEP_MONITOR_ROTARY);
#endif
	rotary_hall_remove(rotary);
err_hall_init:
	rotary_mouse_remove(rotary);
err_mouse_init:
	wake_lock_destroy(&rotary->wake_lock);
	kfree(rotary);
err_kzalloc:
	return result;
}

static void rotary_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct rotary_ddata *rotary = dev_get_drvdata(dev);
	struct input_dev *input_dev = rotary->mouse_dev;

	dev_info(&input_dev->dev, "%s\n", __func__);

	if (!rotary->power_state)
		dev_err(&input_dev->dev, "%s: already power off.\n", __func__);
	else
		rotary_ctrl_power(rotary, false);

	return;
}

static int rotary_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct rotary_ddata *rotary = dev_get_drvdata(dev);
	struct	input_dev *input_dev = rotary->mouse_dev;

	dev_info(&input_dev->dev, "%s\n", __func__);

	if (rotary->power_state)
		dev_err(&input_dev->dev, "%s: already power on.\n", __func__);
	else {
		rotary->x_sum = 0;
		rotary->y_sum = 0;
		rotary->last_x_sum = 0;
		rotary_ctrl_power(rotary, true);
	}

	return 0;
}

static int rotary_i2c_remove(struct i2c_client *client)
{
	struct rotary_ddata *rotary = i2c_get_clientdata(client);

	device_destroy(sec_class,rotary->sec_dev->devt);
#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_unregister_ops(SLEEP_MONITOR_ROTARY);
#endif
	rotary_hall_remove(rotary);
	rotary_mouse_remove(rotary);
	wake_lock_destroy(&rotary->wake_lock);
	kfree(rotary);

	return 0;
}

static int rotary_suspend(struct device *dev)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);

	dev_info(&rotary->mouse_dev->dev, "%s\n", __func__);

	if (rotary->always_on && rotary->power_state) {
		rotary->wakeup_state = false;
		enable_irq_wake(rotary->motion_irq);
	}

	if (device_may_wakeup(&rotary->hall_dev->dev)) {
		rotary->hall_resume_state = false;
		enable_irq_wake(rotary->hall_a_irq);
		enable_irq_wake(rotary->hall_b_irq);
	} else {
		disable_irq(rotary->hall_a_irq);
		disable_irq(rotary->hall_b_irq);
	}

	return 0;
}

static int rotary_resume(struct device *dev)
{
	struct rotary_ddata *rotary = dev_get_drvdata(dev);

	dev_info(&rotary->mouse_dev->dev, "%s\n", __func__);

	if (rotary->always_on && rotary->power_state) {
		disable_irq_wake(rotary->motion_irq);
		rotary->wakeup_state = true;
		wake_up(&rotary->wait_q);
	}

	if (device_may_wakeup(&rotary->hall_dev->dev)) {
		rotary->hall_resume_state = true;
		disable_irq_wake(rotary->hall_a_irq);
		disable_irq_wake(rotary->hall_b_irq);
	} else {
		rotary->last_status = HALL_INIT;
		rotary->hall_sum = 0;
		enable_irq(rotary->hall_a_irq);
		enable_irq(rotary->hall_b_irq);
	}

	return 0;
}

static const struct i2c_device_id rotary_i2c_id[]={
	{ROTARY_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, rotary_i2c_id);

#ifdef CONFIG_PM
static const struct dev_pm_ops rotary_pm_ops = {
	.suspend = rotary_suspend,
	.resume = rotary_resume,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id rotary_dt_match[] = {
	{ .compatible = "sec,rotary" },
	{ }
};
#else
#define rotary_dt_match NULL
#endif

static struct i2c_driver rotary_i2c_driver = {
	.probe	= rotary_i2c_probe,
	.remove	= rotary_i2c_remove,
	.driver =	{
		.name	= ROTARY_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &rotary_pm_ops,
#endif
		.of_match_table = of_match_ptr(rotary_dt_match),
	},
	.id_table	= rotary_i2c_id,
};

static int __init rotary_init(void)
{
	int ret;

	ret = i2c_add_driver(&rotary_i2c_driver);
	if (ret!=0)
		printk("%s: I2C device init Faild! return(%d) \n", __func__,  ret);
	else
		printk("%s: I2C device init Sucess\n", __func__);

	return ret;
}

static void __exit rotary_exit(void)
{
	printk(" %s\n", __func__);
	i2c_del_driver(&rotary_i2c_driver);
}

module_init(rotary_init);
module_exit(rotary_exit);
MODULE_AUTHOR("Joong-Mock Shin <jmock.shin@samsung.com>");
MODULE_DESCRIPTION("rotary device driver");
MODULE_LICENSE("GPL");
