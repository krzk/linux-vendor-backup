/*
 * partron_ofm.c -PARTRON Optical Finger Mouse driver
 * For use with MS37C01A parts.
 *
 * Copyright 2011 PARTRON Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <asm/uaccess.h>
#include <linux/i2c/partron_ofm.h>

//#define OFM_DOME_BUTTON
//#define OFM_KEY_MODE
#define OFM_MOUSE_MODE
#define OFM_FEATURE_READ

#define I2C_READ_WORD_DATA	1
#define I2C_RETRY_CNT		5
#define I2C_WAKEUP_DELAY	2000
#define MOTION_READ_PERIOD	9

#define WAKELOCK_TIME	HZ/10

#define OFM_MOTION_ADDR	0x21
#define OFM_FEATURE_ADDR	0x31

#ifdef OFM_DOME_BUTTON
#define DCLICK_TIMER_ID	0
#define LCLICK_TIMER_ID	1
#define CLEAR_TIMER_ID	2
#define MAX_KEYPAD_CNT	7
#define KEY_PRESSED	1
#define KEY_RELEASED	0
#define DCLICK_TIME (200*NSEC_PER_MSEC)
#define LCLICK_TIME (1000*NSEC_PER_MSEC)
#define CLEAR_TIME (200*NSEC_PER_MSEC)
#define down_key	0
#define right_key		1
#define left_key		2
#define up_key		3
#define enter_key	4
#define back_key		5
#define home_key	6
#endif

#ifdef OFM_KEY_MODE
#define OFM_SENSITIVITY_MAX_LEVEL   9
#define OFM_SENSITIVITY_X_DEFAULT  4
#define OFM_SENSITIVITY_Y_DEFAULT  4

#define OFM_SENSITIVITY1  90
#define OFM_SENSITIVITY2  80
#define OFM_SENSITIVITY3  70
#define OFM_SENSITIVITY4  60
#define OFM_SENSITIVITY5  50
#define OFM_SENSITIVITY6  45
#define OFM_SENSITIVITY7  40
#define OFM_SENSITIVITY8  35
#define OFM_SENSITIVITY9  30

static int gv_ofm_Xsensitivity[OFM_SENSITIVITY_MAX_LEVEL]=
{
	OFM_SENSITIVITY1,
	OFM_SENSITIVITY2,
	OFM_SENSITIVITY3,
	OFM_SENSITIVITY4,
	OFM_SENSITIVITY5,
	OFM_SENSITIVITY6,
	OFM_SENSITIVITY7,
	OFM_SENSITIVITY8,
	OFM_SENSITIVITY9,
};

static int gv_ofm_Ysensitivity[OFM_SENSITIVITY_MAX_LEVEL]=
{
	OFM_SENSITIVITY1,
	OFM_SENSITIVITY2,
	OFM_SENSITIVITY3,
	OFM_SENSITIVITY4,
	OFM_SENSITIVITY5,
	OFM_SENSITIVITY6,
	OFM_SENSITIVITY7,
	OFM_SENSITIVITY8,
	OFM_SENSITIVITY9,
};
#endif

extern struct class *sec_class;

static void ofm_enable_irq(struct ofm *ofm);
static void ofm_disable_irq(struct ofm *ofm);
static int ofm_i2c_write(struct i2c_client *client, u_int8_t index, u_int8_t data);
static int ofm_i2c_read(struct i2c_client *client, u16 reg, u_int8_t *buff, u16 length);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ofm_early_suspend(struct early_suspend *h);
static void ofm_late_resume(struct early_suspend *h);
#endif
static int ofm_open(struct input_dev *input);
static void ofm_close(struct input_dev *input);

#ifdef OFM_DOME_BUTTON
int g_bTimerEnabled = 0;
int g_bTimerLongEnabled = 0;
int g_bTimerClearEnabled = 0;
int g_iClickCnt = 0;
#endif

#ifdef OFM_KEY_MODE
s16 sum_x=0, sum_y=0;

unsigned int ofm_keypad_keycode_map[MAX_KEYPAD_CNT] = {
	KEY_DOWN,
	KEY_RIGHT,
	KEY_LEFT,
	KEY_UP,
	KEY_ENTER,
	KEY_BACK,
	KEY_HOME,
};

unsigned char ofm_keypad_keycode_str[MAX_KEYPAD_CNT][20] = {
	"KEY_DOWN",
	"KEY_RIGHT",
	"KEY_LEFT",
	"KEY_UP",
	"KEY_ENTER",
	"KEY_BACK",
	"KEY_HOME",
};
#endif

static int ofm_init_sequece(struct ofm *ofm)
{
	int ret;
	int sequece_cnt;
	int sequece_num;
	int retry_cnt = I2C_RETRY_CNT;
	u_int8_t buf[2] = {0, };
	const char sequece[][2] = {
		{0x29, 0x50},//min feeature set
		{0x27, 0x00},//Allows X to be inverted
		{0x2a, 0x01},//Set 50 CPI for X data
		{0x2b, 0x01},//Set 50 CPI for Y data
		{0x51, 0x02},//Set Sunlight mode
		{0x28, 0x74},//Set HPF 5x5 set
		{0x4e, 0x08},//Set increase exp time
		{0x4f, 0x08},//Set decrease exp time
		{0x4a, 0x10},//Set Min exposure time
		{0x4b, 0x10},//Set exposure update every time
		{0x03, 0xfc},//Set DMIB DAC Vref setting = 1.6v
		{0x1c, 0x14},//Set frame rate 2.9k fps (350us period)
		{0x05, 0x1c},//system configuration (motion pin active high & Manual Mode)
	};

	ret = ofm_i2c_write(ofm->client, 0x16, 0x1e); //software reset
	if (ret < 0) {
		dev_err(&ofm->input_dev->dev,
			"%s: i2c write failed. add=[0x%2x],data=[0x%2x]\n",
			__func__, 0x16, 0x1e);
		return ret;
	}
	msleep(1);

	sequece_num = sizeof(sequece) / (2*sizeof(char));

	for (sequece_cnt=0; sequece_cnt < sequece_num; sequece_cnt++) {
		ret = ofm_i2c_write(ofm->client,\
			sequece[sequece_cnt][0], sequece[sequece_cnt][1]);
		if (ret < 0) {
			dev_err(&ofm->input_dev->dev,
				"%s: i2c write failed. add=[0x%2x],data=[0x%2x]\n",
				__func__, sequece[sequece_cnt][0], sequece[sequece_cnt][1]);
			break;
		}
	}
	msleep(1);
	do {
		ret = ofm_i2c_read(ofm->client, 0x21, buf, I2C_READ_WORD_DATA);
		if ((ret > 0) && (buf[0] || buf[1])) {
			dev_info(&ofm->input_dev->dev,"remove waste data (%d/%d)\n", buf[0], buf[1]);
			retry_cnt--;
			msleep(10);
		} else
			break;
	} while(retry_cnt);

	return ret;
 }

static int ofm_ctrl_power(struct ofm *ofm, bool power)
{
	struct ofm_platform_data *pdata = ofm->pdata;
	static struct regulator *regulator_pwr = NULL;
	s32 ret;

	if (!regulator_pwr) {
		regulator_pwr = regulator_get(NULL, "vdd_ofm_2.8v");
		if (IS_ERR(regulator_pwr)) {
			dev_err(&ofm->input_dev->dev, "%s: failed to get ldo14 regulator\n", __func__);
			return PTR_ERR(regulator_pwr);
		}
	}

	if (power) {
		ret = regulator_set_voltage(regulator_pwr, 2800000, 2800000);
		if (ret) {
			dev_err(&ofm->input_dev->dev, "%s: unable to set voltage for avdd_vreg, %d\n", __func__, ret);
			return ret;
		}

		ret = regulator_enable(regulator_pwr);
		if (ret) {
			dev_err(&ofm->input_dev->dev, "%s: unable to enable regulator_pwr, %d\n", __func__, ret);
			return ret;
		}

		ret = regulator_is_enabled(regulator_pwr);
		if (ret)
			pr_info("%s: regulator_pwr is enabled, %d\n", __func__, ret);
		else {
			dev_err(&ofm->input_dev->dev, "%s: regulator_pwr is disabled, %d\n", __func__, ret);
			return ret;
		}
		msleep(1); //wating for gpio get stable

		gpio_set_value(pdata->powerdown_pin, 0);
		gpio_set_value(pdata->standby_pin, 0);

		msleep(1); //wating for gpio get stable

		ret = ofm_init_sequece(ofm);
		if (ret < 0) {
			dev_err(&ofm->input_dev->dev, "%s: init sequence failed. %d\n", __func__, ret);
			return ret;
		}
		ofm->power_state = true;
		ofm_enable_irq(ofm);
	} else {
		ofm_disable_irq(ofm);
		gpio_set_value(pdata->powerdown_pin, 1);
		gpio_set_value(pdata->standby_pin, 1);
		regulator_disable(regulator_pwr);
		ofm->power_state = false;
	}

	dev_info(&ofm->input_dev->dev, "%s: 2.8v=[%s]\n", __func__,\
		regulator_is_enabled(regulator_pwr)?"ON":"OFF");

	return 0;
}

#ifdef OFM_DOME_BUTTON
static void ofm_timer_enable(struct ofm *ofm, int mode)
{
	/* int hrtimer_start(struct hrtimer *timer,  	-->timer struct.
					ktime_t time,			-->timer expire time.
					const enum hrtimer_mode mode);  -->time mode(absolute mode or relative mode
	*/
	if (mode == DCLICK_TIMER_ID) {
		//dev_info(&ofm->input_dev->dev, "\t ofm_timer_enable, timer delay %lldns\n", ktime_to_ns(ofm->dclick_time));
		hrtimer_start(&ofm->timer_click, ofm->dclick_time, HRTIMER_MODE_REL);
		g_bTimerEnabled = 1;
	} else if (mode == LCLICK_TIMER_ID) {
		//dev_info(&ofm->input_dev->dev, "\t ofm_timer_long_enable, timer delay %lldns\n", ktime_to_ns(ofm->lclick_time));
		hrtimer_start(&ofm->timer_long, ofm->lclick_time, HRTIMER_MODE_REL);
		g_bTimerLongEnabled = 1;
	} else if (mode == CLEAR_TIMER_ID)	{
		hrtimer_start(&ofm->timer_clear, ofm->clear_time, HRTIMER_MODE_REL);
		g_bTimerClearEnabled = 1;
	}
}

static void ofm_timer_disable(struct ofm *ofm, int mode)
{
	if (mode == DCLICK_TIMER_ID) {
		hrtimer_cancel(&ofm->timer_click);
		g_bTimerEnabled = 0;
	} else if (mode == LCLICK_TIMER_ID) {
		hrtimer_cancel(&ofm->timer_long);
		g_bTimerLongEnabled = 0;
	} else if (mode == CLEAR_TIMER_ID)	{
		hrtimer_cancel(&ofm->timer_clear);
		g_bTimerClearEnabled = 0;
	}
}

static void ofm_click_run(struct ofm *ofm, int code)
{
	dev_info(&ofm->input_dev->dev, "\t ofm_click_run(%d)\n", code);
	input_report_key(ofm->input_dev, ofm_keypad_keycode_map[code], KEY_PRESSED);
	input_sync(ofm->input_dev);

	input_report_key(ofm->input_dev, ofm_keypad_keycode_map[code], KEY_RELEASED);
	input_sync(ofm->input_dev);
	g_bTimerEnabled = 0;
	g_iClickCnt = 0;
}

static enum hrtimer_restart ofm_timer_click_func(struct hrtimer *timer)
{
	ofm_click_run(ofm, enter_key);
	g_iClickCnt = 0;

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart ofm_timer_long_func(struct hrtimer *timer)
{
	int down = 0;

	down = gpio_get_value(ofm->button_pin) ? 0 : 1; //  KEY_RELEASED : KEY_PRESSED

	if (down) {
		ofm_click_run(ofm, home_key);
		if (g_bTimerEnabled)
			ofm_timer_disable(0);
		g_iClickCnt = 0;
	}
	g_bTimerLongEnabled = 0;

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart ofm_timer_clear_func(struct hrtimer *timer)
{
	sum_x = 0;
	sum_y = 0;

	return HRTIMER_NORESTART;
}

static irqreturn_t ofm_left_event(int irq, void *dev_id)
{
	int down = 0;

	disable_irq_nosync(irq);
	down = gpio_get_value(ofm->button_pin) ? 0 : 1;
	if (down) {
		dev_info(&ofm->input_dev->dev, " BUTTON DOWN (%d)\n",down);
		g_iClickCnt++;
		if (g_iClickCnt == 2) {
			if (g_bTimerEnabled)
				ofm_timer_disable(DCLICK_TIMER_ID);
			if (g_bTimerLongEnabled)
				ofm_timer_disable(LCLICK_TIMER_ID);
			ofm_click_run(ofm, back_key);
			g_iClickCnt = 0;
		} else
			ofm_timer_enable(LCLICK_TIMER_ID);
	} else {
		dev_info(&ofm->input_dev->dev, " BUTTON UP   (%d)\n",down);
		if (!g_bTimerEnabled) {
			if (g_iClickCnt == 1)
				ofm_timer_enable(DCLICK_TIMER_ID);

			if (g_bTimerLongEnabled) {
				ofm_timer_disable(LCLICK_TIMER_ID);
				g_bTimerLongEnabled = 0;
			}
		}
	}
	enable_irq(irq);

	return IRQ_HANDLED;
}
#endif

#define ROTARY_BOOSTER	1

#if defined(ROTARY_BOOSTER)
#include <linux/pm_qos.h>

struct delayed_work rotary_booster_off_work;
static struct pm_qos_request pm_qos_rotary;
static int rotary_min_cpu_freq = 300000;
struct mutex rotary_dvfs_lock;
#define ROTARY_BOOSTER_DELAY	100

static void rotary_off_work_func(struct work_struct *work)
{
	mutex_lock(&rotary_dvfs_lock);

	if (pm_qos_request_active(&pm_qos_rotary))
		pm_qos_remove_request(&pm_qos_rotary);

	mutex_unlock(&rotary_dvfs_lock);
}
#endif

static void ofm_motion_handler(struct ofm *ofm)
{
	struct	input_dev *input_dev = ofm->input_dev;
	u_int8_t buf[2] = {0, };
	int ret;
	s8  x,y;

#ifdef OFM_FEATURE_READ
	int feature = 0;
#endif
#ifdef OFM_KEY_MODE
	int key_code = MAX_KEYPAD_CNT;
#endif

	/*******READ_WORD_DATA*****************************************************
	*   	LSB =X									*
	*	MSB =Y									*
	*	WORDDATA =LSB(X)+MSB(Y)							*
	*****************************************************************************/
	ret = ofm_i2c_read(ofm->client, OFM_MOTION_ADDR, buf, I2C_READ_WORD_DATA);
	if (ret < 0) {
		dev_err(&input_dev->dev, "%s: OFM_MOTION_ADDR read failed.[%d]\n", __func__, ret);
		goto out;
	}
#if defined (OFM_MOUSE_MODE)
	x = buf[0];
	y = buf[1];

	if ( x != 0 || y != 0 ) {
		ofm->x_sum += x;
		ofm->y_sum += y;
	} else
		goto out;

	wake_lock_timeout
		(&ofm->wake_lock, WAKELOCK_TIME);

	input_report_rel(input_dev, REL_X, x);
	input_report_rel(input_dev, REL_Y, y);
	input_sync(input_dev);

#if defined(ROTARY_BOOSTER)
	mutex_lock(&rotary_dvfs_lock);

	if (!pm_qos_request_active(&pm_qos_rotary)) {
		pm_qos_add_request(&pm_qos_rotary
			, PM_QOS_CPU_FREQ_MIN, rotary_min_cpu_freq);
		schedule_delayed_work(&rotary_booster_off_work
			,msecs_to_jiffies(ROTARY_BOOSTER_DELAY));
	} else {
		cancel_delayed_work_sync(&rotary_booster_off_work);
		schedule_delayed_work(&rotary_booster_off_work
			,msecs_to_jiffies(ROTARY_BOOSTER_DELAY));
	}

	mutex_unlock(&rotary_dvfs_lock);
#endif

	if (0 > x)
		ofm->motion_direction = -1;
	else
		ofm->motion_direction = 1;

#ifdef OFM_FEATURE_READ
	ret = ofm_i2c_read(ofm->client, OFM_FEATURE_ADDR, buf, I2C_READ_WORD_DATA);
	if (ret < 0) {
		dev_err(&input_dev->dev, "%s: OFM_FEATURE_ADDR read failed.[%d]\n", __func__, ret);
		feature = 0;
	} else
		feature = buf[0];

	dev_info(&input_dev->dev,
		"%s: x=%3d, x_sum=%3d y=%3d, y_sum=%3d, f=%3d, d=%d\n",\
		__func__, x, ofm->x_sum, y, ofm->y_sum, feature, ofm->motion_direction);
#else
	dev_info(&input_dev->dev,
		"%s: x=%3d, x_sum=%3d y=%3d, y_sum=%3d, d=%d\n",\
		__func__, x, ofm->x_sum, y, ofm->y_sum, ofm->motion_direction);
#endif


#elif defined (OFM_KEY_MODE)
	if (result<=0) {
		if (!g_bTimerClearEnabled)
			ofm_timer_enable(ofm, CLEAR_TIMER_ID);
		goto out;
	} else {
		if (g_bTimerClearEnabled)
			ofm_timer_disable(ofm, CLEAR_TIMER_ID);
	}

	x = buf[0];
	y = buf[1];

	sum_x += x;
	sum_y += y;

	if ((abs(sum_x) >= gv_ofm_Xsensitivity[ofm->x_level]) ||\
	(abs(sum_y) >= gv_ofm_Ysensitivity[ofm->y_level])) {
		if (abs(sum_x) >= gv_ofm_Xsensitivity[ofm->x_level]) {
			if (sum_x < 0)
				key_code = left_key;
			else
				key_code = right_key;
			sum_x = 0;
			sum_y = 0;
		}

		if (abs(sum_y) >= gv_ofm_Ysensitivity[ofm->y_level]) {
			if (sum_y < 0)
				key_code = up_key;
			else
				key_code = down_key;
			sum_x = 0;
			sum_y = 0;
		}

		input_report_key(input_dev, ofm_keypad_keycode_map[key_code], KEY_PRESSED);
		input_sync(input_dev);
		dev_info(&input_dev->dev, "\t %s KEY_PRESSED\n", (char *)&ofm_keypad_keycode_str[key_code]);

		input_report_key(input_dev, ofm_keypad_keycode_map[key_code], KEY_RELEASED);
		input_sync(input_dev);
		dev_info(&input_dev->dev, "\t %s \n", (char *)&ofm_keypad_keycode_str[key_code]);
		key_code = MAX_KEYPAD_CNT;
	}
#endif

out:
	return;
}
static void ofm_motion_handler_work(struct work_struct *work)
{
	struct ofm *ofm = container_of(work,
				struct ofm, dwork_motion.work);

	if (!gpio_get_value(ofm->pdata->motion_pin)) {
		enable_irq(ofm->motion_irq);
		return;
	}

	ofm_motion_handler(ofm);

	schedule_delayed_work(&ofm->dwork_motion, msecs_to_jiffies(MOTION_READ_PERIOD));

	return;
}

static irqreturn_t ofm_motion_interrupt(int irq, void *dev_id)
{
	struct ofm *ofm = (struct ofm *)dev_id;
	struct input_dev *input_dev = ofm->input_dev;
	int ret;

	if (ofm->always_on) {
		ret = wait_event_timeout(ofm->wait_q, ofm->wakeup_state,
					msecs_to_jiffies(I2C_WAKEUP_DELAY));
		if (!ret)
			dev_err(&input_dev->dev,
				"%s: wakeup_state timeout. ofm->wakeup_state=[%d]\n",
				__func__, ofm->wakeup_state);
	}

	disable_irq_nosync(ofm->motion_irq);

	ofm_motion_handler(ofm);

	schedule_delayed_work(&ofm->dwork_motion, msecs_to_jiffies(MOTION_READ_PERIOD));

	return IRQ_HANDLED;
}

static int ofm_i2c_read(struct i2c_client *client, u16 reg, u_int8_t *buff, u16 length)
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

static int ofm_i2c_write(struct i2c_client *client, u_int8_t index, u_int8_t data)
{
	struct ofm *ofm = i2c_get_clientdata(client);
	u_int8_t buf[2] = {index , data};
	int result;
	int retry_cnt = I2C_RETRY_CNT;

	do {
		result= i2c_master_send(client, buf, 2);
		if (result >= 0) {
			dev_info(&ofm->input_dev->dev, "%s:(0x%02x, 0x%02x), ret=%d\n", __func__, index, data, result);
			return 0;
		} else {
			retry_cnt--;
			msleep(10);
		}
	} while(retry_cnt);

	dev_err(&ofm->input_dev->dev, " ERROR i2c send!!!index(%x) data(%x) return (%x)\n",index,data,result);

	return result;
}


static void ofm_enable_irq(struct ofm *ofm)
{
	enable_irq(ofm->motion_irq);
	dev_info(&ofm->input_dev->dev, " %s: enable motion irq %d\n", __FUNCTION__, ofm->motion_irq);

#ifdef OFM_DOME_BUTTON
	enable_irq(ofm->button_pin);
	dev_info(&ofm->input_dev->dev, " %s: enable dome key irq %d\n", __FUNCTION__, ofm->button_pin);
#endif
}

static void ofm_disable_irq(struct ofm *ofm)
{
	disable_irq(ofm->motion_irq);
	dev_info(&ofm->input_dev->dev, " %s: Disable motion irq %d\n", __FUNCTION__, ofm->motion_irq);

#ifdef OFM_DOME_BUTTON
	disable_irq(ofm->button_pin);
	dev_info(&ofm->input_dev->dev, " %s: Disable dome key irq %d\n", __FUNCTION__, ofm->button_pin);
#endif
}

#ifdef OFM_KEY_MODE
static ssize_t ofm_level_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ofm *ofm = dev_get_drvdata(dev);
	int count = 0;

	dev_info(&ofm->input_dev->dev, KERN_INFO " access ofm_level_read!!!\n");
	count = sprintf(buf,"%d,%d\n", ofm->x_level, ofm->y_level);

	return count;
}

static ssize_t ofm_level_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	struct ofm *ofm = dev_get_drvdata(dev);
	int rc;
	unsigned long level;

	rc = strict_strtoul(buf, 0, &level);
	if (rc)
	{
		dev_info(&ofm->input_dev->dev, KERN_INFO " ofm_level_store : rc = %d\n", rc);
		return rc;
	}
	rc = -ENXIO;
	mutex_lock(&ofm->ops_lock);
	dev_info(&ofm->input_dev->dev, KERN_INFO " write ofm_level_store : Level = %lu\n", level);
	if ((level >= 0) && (level < OFM_SENSITIVITY_MAX_LEVEL))
	{
		ofm->x_level = level;
		ofm->y_level = level;
	}
	rc = count;
	mutex_unlock(&ofm->ops_lock);

	return rc;
}
#endif

static ssize_t ofm_sum_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ofm *ofm = dev_get_drvdata(dev);
	int count = 0;

	dev_info(&ofm->input_dev->dev, KERN_INFO " access ofm_level_read!!!\n");
	count = sprintf(buf,"x_sum=%d,y_sum=%d\n", ofm->x_sum, ofm->y_sum);

	return count;
}

static ssize_t ofm_sum_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	struct ofm *ofm = dev_get_drvdata(dev);

	ofm->x_sum = 0;
	ofm->y_sum = 0;

	return count;
}

static ssize_t ofm_action_onoff(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	struct ofm *ofm = dev_get_drvdata(dev);
	int rc;
	unsigned long on_off;

	rc = strict_strtoul(buf, 0, &on_off);
	if (rc) {
		dev_info(&ofm->input_dev->dev, KERN_INFO " ofm_action_store : rc = %d\n", rc);
		return rc;
	}
	dev_info(&ofm->input_dev->dev, KERN_INFO " write ofm_action_store : on/off = %lu\n", on_off);

	rc = -ENXIO;
#ifdef OFM_KEY_MODE
	mutex_lock(&ofm->ops_lock);
#endif

	if (on_off)
		ofm_ctrl_power(ofm, true);
	else
		ofm_ctrl_power(ofm, false);

	rc = count;
#ifdef OFM_KEY_MODE
	mutex_unlock(&ofm->ops_lock);
#endif

	return rc;
}

#ifdef OFM_KEY_MODE
static struct device_attribute dev_attr_set_level = __ATTR(setlevel, 0664 , NULL, ofm_level_store);
static struct device_attribute dev_attr_get_level = __ATTR(getlevel, 0444, ofm_level_read, NULL);
#endif
static struct device_attribute dev_attr_ofm_onoff = __ATTR(setonoff, 0664 , NULL, ofm_action_onoff);
static struct device_attribute dev_attr_ofm_show_sum = __ATTR(show_sum, 0664 , ofm_sum_show, NULL);
static struct device_attribute dev_attr_ofm_clear_sum = __ATTR(clear_sum, 0664 , NULL, ofm_sum_store);

static struct attribute *ofm_sysfs_attrs[] = {
#ifdef OFM_KEY_MODE
	&dev_attr_set_level.attr,
	&dev_attr_get_level.attr,
#endif
	&dev_attr_ofm_onoff.attr,
	&dev_attr_ofm_show_sum.attr,
	&dev_attr_ofm_clear_sum.attr,
	NULL
};

static struct attribute_group ofm_attribute_group = {
	.attrs = ofm_sysfs_attrs,
};

#ifdef CONFIG_OF
static struct ofm_platform_data *ofm_parse_dt(struct device *dev)
{
	struct ofm_platform_data *pdata;
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

	if(!of_find_property(np, "ofm-gpio-stb", NULL)) {
		pdata->standby_pin = -1;
		dev_err(dev, "failed to get ofm-gpio-stb property\n");
	}
	pdata->standby_pin = of_get_named_gpio_flags(
				np, "ofm-gpio-stb", 0, NULL);

	if(!of_find_property(np, "ofm-gpio-pd", NULL)) {
		pdata->powerdown_pin = -1;
		dev_err(dev, "failed to get ofm-gpio-pd property\n");
	}
	pdata->powerdown_pin = of_get_named_gpio_flags(
				np, "ofm-gpio-pd", 0, NULL);

	if(!of_find_property(np, "ofm-gpio-mtn", NULL)) {
		pdata->standby_pin = -1;
		dev_err(dev, "failed to get ofm-gpio-mtn property\n");
	}
	pdata->motion_pin = of_get_named_gpio_flags(
				np, "ofm-gpio-mtn", 0, NULL);

	return pdata;
}
#endif

static int ofm_gpio_init(struct i2c_client *client)
{
	struct ofm *ofm = i2c_get_clientdata(client);
	struct ofm_platform_data *pdata = ofm->pdata;
	int ret = 0;

	if (gpio_is_valid(pdata->standby_pin)) {
		ret = gpio_request(pdata->standby_pin, "OFM_standby");
		if (ret)
			dev_err(&client->dev, "unable to set GPIO OFM_standby, %d\n", ret);

		gpio_direction_output(pdata->standby_pin, 0);
		dev_info(&ofm->input_dev->dev, "%s: pdata->standby_pin= %d, [%d]\n",
			__func__, pdata->standby_pin, gpio_get_value(pdata->standby_pin));
	}

	if (gpio_is_valid(pdata->powerdown_pin)) {
		ret = gpio_request(pdata->powerdown_pin, "OFM_powerdown");
		if (ret)
			dev_err(&client->dev, "unable to set GPIO OFM_powerdown, %d\n", ret);

		gpio_direction_output(pdata->powerdown_pin, 1);
		dev_info(&ofm->input_dev->dev, "%s: pdata->powerdown_pin= %d, [%d]\n",
			__func__, pdata->powerdown_pin, gpio_get_value(pdata->powerdown_pin));
	}

	return ret;
}

static ssize_t show_ofm_alway_on(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct ofm *ofm = dev_get_drvdata(dev);

	dev_info(&ofm->input_dev->dev, "%s: ofm->always_on=[%d]\n",
			__func__, ofm->always_on);

	return snprintf(buf, PAGE_SIZE, "%d\n", ofm->always_on);
}

static ssize_t store_ofm_alway_on(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct ofm *ofm = dev_get_drvdata(dev);
	int mode = -1;

	sscanf(buf, "%d", &mode);

	dev_info(&ofm->input_dev->dev, "%s: alway_on=%d\n", __func__, mode);

	if (mode) {
		ofm->wakeup_state = true;
		ofm->always_on = true;
	} else
		ofm->always_on = false;

	return count;
}

static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR | S_IWGRP,\
			show_ofm_alway_on, store_ofm_alway_on);

static struct attribute *sec_ofm_attributes[] = {
	&dev_attr_mode.attr,
	NULL,
};

static struct attribute_group sec_ofm_attr_group = {
	.attrs = sec_ofm_attributes,
};


static int ofm_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ofm *ofm;
	int result;
#ifdef OFM_KEY_MODE
	int key, code;
#endif

	ofm = kzalloc(sizeof(struct ofm), GFP_KERNEL);
	if (!ofm)
	{
		dev_err(&client->dev, " failed to allocate driver data\n");
		result = -ENOMEM;
		goto err_kzalloc;
	}

#ifdef CONFIG_OF
		ofm->pdata = ofm_parse_dt(&client->dev);
#else
		ofm->pdata = client->dev.platform_data;
#endif
	ofm->input_dev = input_allocate_device();
	if (!ofm->input_dev) {
		dev_err(&client->dev, " Failed to allocate input device.\n");
		result = -ENOMEM;
		goto err_input_allocate;
	}

	ofm->client = client;
	ofm->input_dev->name = OFM_NAME;
	ofm->input_dev->id.bustype = BUS_I2C;
	ofm->input_dev->dev.parent = &client->dev;
	ofm->input_dev->phys = "ofm";
	ofm->input_dev->id.vendor = 0x0001;//0xDEAD;
	ofm->input_dev->id.product = 0x0001;//0xBEEF;
	ofm->input_dev->id.version = 0x0100;//0x01;
	ofm->input_dev->open = ofm_open;
	ofm->input_dev->close = ofm_close;
	ofm->always_on = ofm->pdata->always_on;
	i2c_set_clientdata(client, ofm);

	wake_lock_init(&ofm->wake_lock,
		WAKE_LOCK_SUSPEND, "ofm_wake_lock");

	init_waitqueue_head(&ofm->wait_q);

	input_set_drvdata(ofm->input_dev, ofm);
#if defined (OFM_MOUSE_MODE)
	__set_bit(EV_REL, ofm->input_dev->evbit);
	__set_bit(EV_KEY, ofm->input_dev->evbit);
	__set_bit(REL_X, ofm->input_dev->relbit);
	__set_bit(REL_Y, ofm->input_dev->relbit);
	__set_bit(BTN_LEFT, ofm->input_dev->keybit);

	input_set_capability(ofm->input_dev, EV_REL, REL_X);
	input_set_capability(ofm->input_dev, EV_REL, REL_Y);
#elif defined (OFM_KEY_MODE)
	set_bit(EV_KEY, ofm->input_dev->evbit);

	for(key = 0; key < MAX_KEYPAD_CNT; key++) {
		code = ofm_keypad_keycode_map[key];
		if (code<=0)
			continue;
		set_bit(code&KEY_MAX, ofm->input_dev->keybit);
	}

	ofm->x_level = OFM_SENSITIVITY_X_DEFAULT;
	ofm->y_level = OFM_SENSITIVITY_Y_DEFAULT;
	mutex_init(&ofm->ops_lock);
 #endif

#ifdef OFM_DOME_BUTTON
	__set_bit(EV_KEY, ofm->input_dev->evbit);
	__set_bit(BTN_LEFT, ofm->input_dev->keybit);
	__set_bit(KEY_ENTER, ofm->input_dev->keybit);
	__set_bit(KEY_BACK, ofm->input_dev->keybit);
	__set_bit(KEY_HOME, ofm->input_dev->keybit);

	/*set dome key pressing start time check timer*/
	hrtimer_init(&ofm->timer_click, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ofm->dclick_time = ns_to_ktime(DCLICK_TIME);
	ofm->timer_click.function =ofm_timer_click_func;

	/*set dome key long press check timer*/
	hrtimer_init(&ofm->timer_long, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ofm->lclick_time = ns_to_ktime(LCLICK_TIME);
	ofm->timer_long.function =ofm_timer_long_func;

	/*set summing data clear check timer*/
	hrtimer_init(&ofm->timer_clear, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ofm->clear_time = ns_to_ktime(CLEAR_TIME);
	ofm->timer_clear.function =ofm_timer_clear_func;

	//set dome key interrupt
	ofm->button_pin = &button_pin;
	s3c_gpio_cfgpin(ofm->button_pin, ofm->ofm_left_setting);
	s3c_gpio_setpull(ofm->button_pin, S3C_GPIO_PULL_NONE);
	result = request_irq (ofm->button_pin,ofm_left_event,\
			IRQF_DISABLED|IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,\
		ofm->button_pin->name,ofm);
	if (result) {
		dev_err(&ofm->input_dev->dev, KERN_ERR "\t %s : Key:Request IRQ  %d  failed\n",\
			__FUNCTION__, ofm->button_pin);
		goto err_input_reg;
	}
	dev_info(&ofm->input_dev->dev, KERN_INFO "\t %s : Key:Request IRQ  %d  Success\n",\
			__FUNCTION__, ofm->button_pin);
#endif

	result = input_register_device(ofm->input_dev);
	if (result) {
		dev_err(&client->dev, "%s:Unable to register %s input device\n",\
			 __FUNCTION__, ofm->input_dev->name);
		goto err_input_reg;
	}

	result = sysfs_create_group(&ofm->input_dev->dev.kobj,&ofm_attribute_group);
	if (result) {
		dev_err(&ofm->input_dev->dev, "%s: Creating sysfs attribute group failed", __func__);
		goto err_input_reg;
	}

	result = ofm_gpio_init(ofm->client);
	if (result) {
		dev_err(&ofm->input_dev->dev, "%s: OFM gpio init failed", __func__);
		goto err_input_reg;
	}

	result = ofm_ctrl_power(ofm, true);
	if (result) {
		dev_err(&ofm->input_dev->dev, "%s: OFM power control failed", __func__);
		goto err_input_reg;
	}

	result = request_threaded_irq(client->irq,
			NULL, ofm_motion_interrupt,
			IRQF_TRIGGER_HIGH  | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			OFM_NAME, ofm);
	if (result < 0) {
		dev_err(&ofm->input_dev->dev, "%s: Failed to register interrupt [%d][%d]\n",
			__func__, client->irq, result);
		goto err_input_reg;
	}

	ofm->motion_irq = client->irq;

	INIT_DELAYED_WORK(&ofm->dwork_motion, ofm_motion_handler_work);
#if defined(ROTARY_BOOSTER)
	mutex_init(&rotary_dvfs_lock);
	INIT_DELAYED_WORK(&rotary_booster_off_work, rotary_off_work_func);
#endif

	ofm->sec_dev = device_create(sec_class, NULL, 0, ofm, "sec_rotary");
	if (IS_ERR(ofm->sec_dev)) {
		dev_err(&ofm->input_dev->dev, "Failed to create device for the sysfs\n");
		goto err_input_reg;
	}

	result = sysfs_create_group(&ofm->sec_dev->kobj,
		&sec_ofm_attr_group);
	if (result) {
		dev_err(&ofm->input_dev->dev, "Failed to create sysfs group\n");
		goto err_create_group;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ofm->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	ofm->early_suspend.suspend = ofm_early_suspend;
	ofm->early_suspend.resume = ofm_late_resume;
	register_early_suspend(&ofm->early_suspend);
#endif
	ofm->wakeup_state = true;
	dev_info(&ofm->input_dev->dev, KERN_INFO "%s : probe done.\n", __func__);

	return 0;

err_create_group:
	device_destroy(sec_class,ofm->sec_dev->devt);
err_input_reg:
	wake_lock_destroy(&ofm->wake_lock);
	dev_set_drvdata(&client->dev, NULL);
#ifdef OFM_DOME_BUTTON
	free_irq(client, &ofm->button_pin);
#endif
	input_unregister_device(ofm->input_dev);
	input_free_device(ofm->input_dev);
err_input_allocate:
	kfree(ofm);
err_kzalloc:
	return result;
}

static void ofm_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct ofm *ofm = dev_get_drvdata(dev);
	struct	input_dev *input_dev = ofm->input_dev;

	dev_info(&input_dev->dev, "%s\n", __func__);

	if (!ofm->power_state)
		dev_err(&input_dev->dev, "%s: already power off.\n", __func__);
	else
		ofm_ctrl_power(ofm, false);

	return;
}

static int ofm_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct ofm *ofm = dev_get_drvdata(dev);
	struct	input_dev *input_dev = ofm->input_dev;

	dev_info(&input_dev->dev, "%s\n", __func__);

	if (ofm->power_state)
		dev_err(&input_dev->dev, "%s: already power on.\n", __func__);
	else {
		ofm->x_sum = 0;
		ofm->y_sum = 0;
		ofm_ctrl_power(ofm, true);
	}

	return 0;
}

static int ofm_i2c_remove(struct i2c_client *client)
{
	struct ofm *ofm = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ofm->early_suspend);
#endif
	device_destroy(sec_class, ofm->sec_dev->devt);
	wake_lock_destroy(&ofm->wake_lock);
	input_unregister_device(ofm->input_dev);
	kfree(ofm);
	return 0;
}

static int ofm_suspend(struct device *dev)
{
	struct ofm *ofm = dev_get_drvdata(dev);

	dev_info(&ofm->input_dev->dev, "%s\n", __func__);

	if (ofm->always_on && ofm->power_state) {
		ofm->wakeup_state = false;
		enable_irq_wake(ofm->motion_irq);
	} else if (ofm->power_state)
		ofm_ctrl_power(ofm, false);

	return 0;
}

static int ofm_resume(struct device *dev)
{
	struct ofm *ofm = dev_get_drvdata(dev);

	dev_info(&ofm->input_dev->dev, "%s\n", __func__);

	if (ofm->always_on && ofm->power_state) {
		disable_irq_wake(ofm->motion_irq);
		ofm->wakeup_state = true;
		wake_up(&ofm->wait_q);
	} else if (!ofm->power_state) {
		ofm->x_sum = 0;
		ofm->y_sum = 0;
		ofm_ctrl_power(ofm, true);
	}

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ofm_early_suspend(struct early_suspend *h)
{
	ofm_suspend(ofm->client, PMSG_SUSPEND);
}

static void ofm_late_resume(struct early_suspend *h)
{
	ofm_resume(ofm->client);
}
#endif

static const struct i2c_device_id ofm_i2c_id[]={
	{OFM_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ofm_i2c_id);

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops ofm_pm_ops = {
	.suspend = ofm_suspend,
	.resume = ofm_resume,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id ofm_dt_match[] = {
	{ .compatible = "partron,OFM" },
	{ }
};
#else
#define ofm_dt_match NULL
#endif

static struct i2c_driver ofm_i2c_driver = {
	.probe	= ofm_i2c_probe,
	.remove	= ofm_i2c_remove,
	.driver =	{
		.name	= OFM_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm = &ofm_pm_ops,
#endif
		.of_match_table = of_match_ptr(ofm_dt_match),
	},
	.id_table	= ofm_i2c_id,
};

static int __init ofm_init(void)
{
	int ret;

	ret = i2c_add_driver(&ofm_i2c_driver);
	if (ret!=0)
		printk("%s: I2C device init Faild! return(%d) \n", __func__,  ret);
	else
		printk("%s: I2C device init Sucess\n", __func__);

	return ret;
}

static void __exit ofm_exit(void)
{
	printk(" %s\n", __func__);
	i2c_del_driver(&ofm_i2c_driver);
}

module_init(ofm_init);
module_exit(ofm_exit);
MODULE_DESCRIPTION("OFM Device Driver");
MODULE_AUTHOR("Partron Sensor Lab");
MODULE_LICENSE("GPL");
