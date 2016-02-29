/*
 * rtc-sec.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/rtc.h>
#include <linux/mfd/samsung/s2mps11.h>
#include <linux/mfd/samsung/s2mpa01.h>
#include <linux/mfd/samsung/s2mpu01.h>
#include <linux/mfd/samsung/s2mpu01a.h>
#include <linux/mfd/samsung/s2mps14.h>
#include <linux/mfd/samsung/irq.h>
#include <linux/regmap.h>
#include <linux/time_history.h>
#include <linux/device.h>

#define CONFIG_RTC_SETTIME_HISTORY

#define RTC_UPDATE_INIT
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

struct s2m_rtc_info {
	struct device *dev;
	struct sec_pmic_dev *iodev;
	struct rtc_device *rtc_dev;
	int irq;
	int rtc_24hr_mode;
	bool wtsr_smpl;
#ifdef CONFIG_DEBUG_FS
	struct dentry			*reg_rtc_debugfs_dir;
#endif
};

#ifdef CONFIG_RTC_SETTIME_HISTORY
#define S2M_SETTIME_HISORY_MAX 30

int s2m_settime_idx = 0;

struct s2m_settime_hisory_t {
	struct timespec ts;
	struct rtc_time tm_set;
	struct rtc_time tm_get;
};
struct s2m_settime_hisory_t s2m_settime_history[S2M_SETTIME_HISORY_MAX];
#endif

static inline int s2m_rtc_calculate_wday(u8 shifted)
{
	int counter = -1;

	while (shifted) {
		shifted >>= 1;
		counter++;
	}
	return counter;
}

static void s2m_data_to_tm(u8 *data, struct rtc_time *tm,
				int rtc_24hr_mode)
{
	tm->tm_sec = data[RTC_SEC] & 0x7f;
	tm->tm_min = data[RTC_MIN] & 0x7f;

	if (rtc_24hr_mode) {
		tm->tm_hour = data[RTC_HOUR] & 0x1f;
	} else {
		tm->tm_hour = data[RTC_HOUR] & 0x0f;
		if (data[RTC_HOUR] & HOUR_PM_MASK)
			tm->tm_hour += 12;
	}

	tm->tm_wday = s2m_rtc_calculate_wday(data[RTC_WEEKDAY] & 0x7f);
	tm->tm_mday = data[RTC_DATE] & 0x1f;
	tm->tm_mon = (data[RTC_MONTH] & 0x0f) - 1;
	tm->tm_year = (data[RTC_YEAR1] & 0x7f)+100;
	tm->tm_yday = 0;
	tm->tm_isdst = 0;
}

static void s2m_tm_to_data(struct rtc_time *tm, u8 *data)
{
	data[RTC_SEC] = tm->tm_sec;
	data[RTC_MIN] = tm->tm_min;

	if (tm->tm_hour >= 12)
		data[RTC_HOUR] = tm->tm_hour | HOUR_PM_MASK;
	else
		data[RTC_HOUR] = tm->tm_hour & ~HOUR_PM_MASK;

	data[RTC_WEEKDAY] = 1 << tm->tm_wday;
	data[RTC_DATE] = tm->tm_mday;
	data[RTC_MONTH] = tm->tm_mon + 1;
	data[RTC_YEAR1] = tm->tm_year > 100 ? (tm->tm_year - 100) : 0 ;
}

static inline int s2m_rtc_set_time_reg(struct s2m_rtc_info *info,
					   int alarm_enable)
{
	int ret;
	unsigned int data;

	ret = sec_rtc_read(info->iodev, S2M_RTC_UPDATE, &data);

	if (ret < 0)
		return ret;
#ifdef RTC_UPDATE_INIT
	if (data) {
		dev_err(info->dev, "%s: read value is wrong! data=0x%02x\n",
			__func__, data);
		data = 0x00;
	}
#endif

	data |= RTC_WUDR_MASK;

	if (alarm_enable)
		data |= RTC_RUDR_MASK;
	else
		data &= ~RTC_RUDR_MASK;

	ret = sec_rtc_write(info->iodev, S2M_RTC_UPDATE, (char)data);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
				__func__, ret);
	} else {
		usleep_range(1000, 1000);
	}

	return ret;
}

static inline int s2m_rtc_read_time_reg(struct s2m_rtc_info *info)
{
	int ret;
	unsigned int data;

	ret = sec_rtc_read(info->iodev, S2M_RTC_UPDATE, &data);

	if (ret < 0)
		return ret;
#ifdef RTC_UPDATE_INIT
	if (data) {
		dev_err(info->dev, "%s: read value is wrong! data=0x%02x\n",
			__func__, data);
		data = 0x00;
	}
#endif
	data |= RTC_RUDR_MASK;
	data &= ~RTC_WUDR_MASK;

	ret = sec_rtc_write(info->iodev, S2M_RTC_UPDATE, (char)data);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
			__func__, ret);
	} else {
		usleep_range(1000, 1000);
	}

	return ret;
}

static int s2m_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	int ret;

	ret = s2m_rtc_read_time_reg(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_read(info->iodev, S2M_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, tm, info->rtc_24hr_mode);

	dev_dbg(dev, "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	return rtc_valid_tm(tm);
}

static int s2m_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	int ret;

	s2m_tm_to_data(tm, data);

	dev_dbg(dev, "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	ret = sec_rtc_bulk_write(info->iodev, S2M_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 0);

#ifdef CONFIG_RTC_SETTIME_HISTORY
	{
		struct timespec ts;
		struct rtc_time tm_get;

		getnstimeofday(&ts);
		s2m_rtc_read_time(dev, &tm_get);

		memcpy(&s2m_settime_history[s2m_settime_idx % S2M_SETTIME_HISORY_MAX].tm_set, tm, sizeof(struct rtc_time));
		memcpy(&s2m_settime_history[s2m_settime_idx % S2M_SETTIME_HISORY_MAX].tm_get, &tm_get, sizeof(struct rtc_time));
		memcpy(&s2m_settime_history[s2m_settime_idx % S2M_SETTIME_HISORY_MAX].ts, &ts, sizeof(struct timespec));
		
		s2m_settime_idx++;
	}
#endif

	return ret;
}

static int s2m_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	unsigned int val;
	int ret, i;

	ret = sec_rtc_bulk_read(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &alrm->time, info->rtc_24hr_mode);

	dev_dbg(dev, "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour,
		alrm->time.tm_min, alrm->time.tm_sec,
		alrm->time.tm_wday);

	alrm->enabled = 0;

	for (i = 0; i < 7; i++) {
		if (data[i] & ALARM_ENABLE_MASK) {
			alrm->enabled = 1;
			break;
		}
	}

	alrm->pending = 0;
	switch (info->iodev->device_type) {
	case S2MPS14:
		ret = sec_reg_read(info->iodev, S2MPS14_REG_ST2, &val);
		break;

	case S2MPS11X:
		ret = sec_reg_read(info->iodev, S2MPS11_REG_ST2, &val);
		break;

	case S2MPA01:
		ret = sec_reg_read(info->iodev, S2MPA01_REG_ST2, &val);
		break;

	case S2MPU01X:
		if (SEC_PMIC_REV(info->iodev) == 0)
			ret = sec_reg_read(info->iodev, S2MPU01_REG_ST2 -1 , &val);
		else
			ret = sec_reg_read(info->iodev, S2MPU01_REG_ST2, &val);
		break;
	case S2MPU01A:
		if (SEC_PMIC_REV(info->iodev) == 0)
			ret = sec_reg_read(info->iodev, S2MPU01A_REG_ST2 - 1 , &val);
		else
			ret = sec_reg_read(info->iodev, S2MPU01A_REG_ST2, &val);
		break;
	default:
		/* If this happens the core funtion has a problem */
		BUG();
	}
	if (ret < 0)
		return ret;

	if (val & RTCA0E)
		alrm->pending = 1;
	else
		alrm->pending = 0;

	return 0;
}

static int s2m_rtc_workaround_operation(struct s2m_rtc_info *info, u8* data)
{
	int ret;

	/* RUDR */
	ret = s2m_rtc_read_time_reg(info);
	if (ret < 0)
		return ret;

	/* Read time */
	ret = sec_rtc_bulk_read(info->iodev, S2M_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	/* Write time */
	ret = sec_rtc_bulk_write(info->iodev, S2M_RTC_SEC, 7, data);
	return ret;
}

static int s2m_rtc_stop_alarm(struct s2m_rtc_info *info)
{
	u8 data[7];
	int ret, i;
	struct rtc_time tm;

	switch (info->iodev->device_type) {
	case S2MPS14:
		ret = s2m_rtc_workaround_operation(info, data);
		if (ret < 0)
			return ret;
		break;
	case S2MPS11X:
		if (SEC_PMIC_REV(info->iodev) <= S2MPS11_REV_82) {
			ret = s2m_rtc_workaround_operation(info, data);
			if (ret < 0)
				return ret;
		}
		break;
	case S2MPA01:
		if (SEC_PMIC_REV(info->iodev) == S2MPA01_REV_00) {
			ret = s2m_rtc_workaround_operation(info, data);
			if (ret < 0)
				return ret;
		}
		break;
	case S2MPU01X:
		ret = s2m_rtc_workaround_operation(info, data);
		if (ret < 0)
			return ret;
		break;
	case S2MPU01A:
		if (SEC_PMIC_REV(info->iodev) <= S2MPU01A_REV_01) {
			ret = s2m_rtc_workaround_operation(info, data);
			if (ret < 0)
				return ret;
		}
		break;
	default:
		/* If this happens the core funtion has a problem */
		BUG();
	}

	ret = sec_rtc_bulk_read(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &tm, info->rtc_24hr_mode);

	dev_dbg(info->dev, "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	for (i = 0; i < 7; i++)
		data[i] &= ~ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 1);
	return ret;
}

static int s2m_rtc_start_alarm(struct s2m_rtc_info *info)
{
	int ret;
	u8 data[7];
	struct rtc_time tm;

	ret = sec_rtc_bulk_read(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &tm, info->rtc_24hr_mode);

	dev_dbg(info->dev, "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	data[RTC_SEC] |= ALARM_ENABLE_MASK;
	data[RTC_MIN] |= ALARM_ENABLE_MASK;
	data[RTC_HOUR] |= ALARM_ENABLE_MASK;
/* Do not use weekday for alarm */
#if 0
	data[RTC_WEEKDAY] |= ALARM_ENABLE_MASK;
#endif
	if (data[RTC_DATE] & 0x1f)
		data[RTC_DATE] |= ALARM_ENABLE_MASK;
	if (data[RTC_MONTH] & 0xf)
		data[RTC_MONTH] |= ALARM_ENABLE_MASK;
	if (data[RTC_YEAR1] & 0x7f)
		data[RTC_YEAR1] |= ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 1);

	return ret;
}

static int s2m_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	int ret;

	s2m_tm_to_data(&alrm->time, data);

	dev_dbg(dev, "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour, alrm->time.tm_min,
		alrm->time.tm_sec, alrm->time.tm_wday);

	ret = s2m_rtc_stop_alarm(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_write(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 1);
	if (ret < 0)
		return ret;

	if (alrm->enabled)
		ret = s2m_rtc_start_alarm(info);

	return ret;
}

static int s2m_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);

	if (enabled)
		return s2m_rtc_start_alarm(info);
	else
		return s2m_rtc_stop_alarm(info);
}

static irqreturn_t s2m_rtc_alarm_irq(int irq, void *data)
{
	struct s2m_rtc_info *info = data;

	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);

	return IRQ_HANDLED;
}

static const struct rtc_class_ops s2m_rtc_ops = {
	.read_time = s2m_rtc_read_time,
	.set_time = s2m_rtc_set_time,
	.read_alarm = s2m_rtc_read_alarm,
	.set_alarm = s2m_rtc_set_alarm,
	.alarm_irq_enable = s2m_rtc_alarm_irq_enable,
};

static void s2m_rtc_enable_wtsr(struct s2m_rtc_info *info, bool enable)
{
	int ret;
	unsigned int val, mask;

	if (enable)
		val = WTSR_ENABLE_MASK;
	else
		val = 0;

	mask = WTSR_ENABLE_MASK;

	dev_info(info->dev, "%s: %s WTSR\n", __func__,
		 enable ? "enable" : "disable");

	ret = sec_rtc_update(info->iodev, S2M_RTC_WTSR_SMPL, val, mask);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update WTSR reg(%d)\n",
			__func__, ret);
		return;
	}
}

#if !defined(CONFIG_MACH_B2) && !defined(CONFIG_MACH_WINGTIP) \
	&& !defined(CONFIG_MACH_WC1)
static void s2m_rtc_enable_smpl(struct s2m_rtc_info *info, bool enable)
{
	int ret;
	unsigned int val, mask;

	if (enable)
		val = SMPL_ENABLE_MASK;
	else
		val = 0;

	mask = SMPL_ENABLE_MASK;

	dev_info(info->dev, "%s: %s SMPL\n", __func__,
			enable ? "enable" : "disable");

	ret = sec_rtc_update(info->iodev, S2M_RTC_WTSR_SMPL, val, mask);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update SMPL reg(%d)\n",
				__func__, ret);
		return;
	}

	val = 0;
	ret = sec_rtc_read(info->iodev, S2M_RTC_WTSR_SMPL, &val);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to read SMPL reg(%d)\n",
				__func__, ret);
	else
		pr_info("%s: WTSR_SMPL(0x%02x)\n", __func__, val);
}
#endif

static int s2m_rtc_init_reg(struct s2m_rtc_info *info)
{
	unsigned int data, tp_read, tp_read1;
	int ret;
	struct rtc_time tm;

	ret = sec_rtc_read(info->iodev, S2M_RTC_CTRL, &tp_read);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read control reg(%d)\n",
			__func__, ret);
		return ret;
	}

	/* Set RTC control register : Binary mode, 24hour mdoe */
	data = (1 << MODEL24_SHIFT) | tp_read;
	data &= ~(1 << BCD_EN_SHIFT);

	info->rtc_24hr_mode = 1;
	/* In first boot time, Set rtc time to 1/1/2012 00:00:00(SUN) */
	ret = sec_rtc_read(info->iodev, S2M_CAP_SEL, &tp_read1);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read cap sel reg(%d)\n",
				__func__, ret);
		return ret;
	}
	/* In first boot time, Set rtc time to 1/1/2015 00:00:00(THU) */
	if ((tp_read & MODEL24_MASK) == 0 || (tp_read1 != 0xf8)) {
		dev_info(info->dev, "rtc init\n");
		tm.tm_sec = 0;
		tm.tm_min = 0;
		tm.tm_hour = 0;
		tm.tm_wday = 4;
		tm.tm_mday = 1;
		tm.tm_mon = 0;
		tm.tm_year = 115;
		tm.tm_yday = 0;
		tm.tm_isdst = 0;
		ret = s2m_rtc_set_time(info->dev, &tm);
		time_history_rtc_time_set(&tm, ret);
	}

	ret = sec_rtc_update(info->iodev, S2M_CAP_SEL, 0xf8, 0xff);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update cap sel reg(%d)\n",
				__func__, ret);
		return ret;
	}
	ret = sec_rtc_update(info->iodev, S2M_RTC_UPDATE, 0x00, 0x04);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update rtc update reg(%d)\n",
				__func__, ret);
		return ret;
	}

	ret = sec_rtc_update(info->iodev, S2M_RTC_CTRL,
		data, 0x3);

	s2m_rtc_set_time_reg(info, 1);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write control reg(%d)\n",
				__func__, ret);
		return ret;
	}

	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int s2m_rtc_debugfs_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t s2m_rtc_debugfs_read_registers(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	struct s2m_rtc_info *s2m = filp->private_data;
	int i;
	u8 regs_value[S2M_ALARM1_YEAR+1];
	u32 reg = 0;
	char *buf;
	size_t len = 0;
	ssize_t ret;
	struct rtc_time tm;
	struct timespec ts;

	if (!s2m) {
		pr_err("%s : s2m is null\n", __func__);
		return 0;
	}

	if (*ppos != 0)
		return 0;

	if (count < sizeof(buf))
		return -ENOSPC;

	buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = s2m_rtc_read_time_reg(s2m);
	if (ret < 0)
		goto debugfs_read_end;

	len += snprintf(buf + len, PAGE_SIZE - len, "Address Value\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "------- -----\n");

	for (i = 0; i < 4; i++) {
		ret = sec_rtc_read(s2m->iodev, i,
				&regs_value[i]);
	}

	reg = S2M_RTC_SEC;
	ret |= sec_rtc_bulk_read(s2m->iodev, reg,
			7, &regs_value[4]);

	reg = S2M_ALARM0_SEC;
	ret |= sec_rtc_bulk_read(s2m->iodev, reg,
			7, &regs_value[11]);

	reg = S2M_ALARM1_SEC;
	ret |= sec_rtc_bulk_read(s2m->iodev, reg,
			7, &regs_value[18]);

	if (!ret) {
		for (i = 0; i < S2M_ALARM1_YEAR+1; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
				" 0x%02x   0x%02x\n",
				i, regs_value[i]);
		}
	}

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	len += snprintf(buf + len, PAGE_SIZE - len,
		" sys time: %04d/%02d/%02d %02d:%02d:%02d(%d)\n",
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	s2m_data_to_tm(&regs_value[S2M_RTC_SEC], &tm, s2m->rtc_24hr_mode);
	len += snprintf(buf + len, PAGE_SIZE - len,
		" rtc time: %04d/%02d/%02d %02d:%02d:%02d(%d)\n",
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	s2m_data_to_tm(&regs_value[S2M_ALARM0_SEC], &tm, s2m->rtc_24hr_mode);
	len += snprintf(buf + len, PAGE_SIZE - len,
		" rtc alm0: %04d/%02d/%02d %02d:%02d:%02d(%d)\n",
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);
		
	s2m_data_to_tm(&regs_value[S2M_ALARM1_SEC], &tm, s2m->rtc_24hr_mode);
	len += snprintf(buf + len, PAGE_SIZE - len,
		" rtc alm1: %04d/%02d/%02d %02d:%02d:%02d(%d)\n",
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);


	ret = simple_read_from_buffer(buffer, len, ppos, buf, PAGE_SIZE);

debugfs_read_end:
	kfree(buf);
	return ret;
}

static const struct file_operations s2m_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = s2m_rtc_debugfs_open,
	.read = s2m_rtc_debugfs_read_registers,
};

#ifdef CONFIG_RTC_SETTIME_HISTORY
static int s2m_rtc_settime_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t s2m_rtc_settime_read_registers(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	struct s2m_rtc_info *s2m = filp->private_data;
	char *buf;
	size_t len = 0;
	ssize_t ret;
	struct rtc_time tm_sys;
	char is_latest[2]={0x00,0x00};
	int i;

	if (!s2m) {
		pr_err("%s : s2m is null\n", __func__);
		return 0;
	}

	if (*ppos != 0)
		return 0;

	if (count < sizeof(buf))
		return -ENOSPC;

	buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;


	len += snprintf(buf + len, PAGE_SIZE - len, "Settime history\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "Total number of settime=%d\n", s2m_settime_idx);
	len += snprintf(buf + len, PAGE_SIZE - len, "-----  ----------------------\t----------------------\t----------------------\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "-idx-  tm_set                \ttm_get                \tsys_time              \n");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----  ----------------------\t----------------------\t----------------------\n");

	for (i=0; i < S2M_SETTIME_HISORY_MAX; i++) {
		is_latest[0] = 0x20;
		if (((i+1) % S2M_SETTIME_HISORY_MAX) == (s2m_settime_idx % S2M_SETTIME_HISORY_MAX))
			is_latest[0] = 0x2a;

		len += snprintf(buf + len, PAGE_SIZE - len,
			"%s(%02d)  %04d/%02d/%02d %02d:%02d:%02d(%d)\t",
			is_latest, i,
			s2m_settime_history[i].tm_set.tm_year + 1900,
			s2m_settime_history[i].tm_set.tm_mon + 1,
			s2m_settime_history[i].tm_set.tm_mday,
			s2m_settime_history[i].tm_set.tm_hour,
			s2m_settime_history[i].tm_set.tm_min,
			s2m_settime_history[i].tm_set.tm_sec,
			s2m_settime_history[i].tm_set.tm_wday);

		len += snprintf(buf + len, PAGE_SIZE - len,
			"%04d/%02d/%02d %02d:%02d:%02d(%d)\t",
			s2m_settime_history[i].tm_get.tm_year + 1900,
			s2m_settime_history[i].tm_get.tm_mon + 1,
			s2m_settime_history[i].tm_get.tm_mday,
			s2m_settime_history[i].tm_get.tm_hour,
			s2m_settime_history[i].tm_get.tm_min,
			s2m_settime_history[i].tm_get.tm_sec,
			s2m_settime_history[i].tm_get.tm_wday);

			rtc_time_to_tm(s2m_settime_history[i].ts.tv_sec, &tm_sys);

			len += snprintf(buf + len, PAGE_SIZE - len,
				"%04d/%02d/%02d %02d:%02d:%02d(%d)\n",
				tm_sys.tm_year + 1900,
				tm_sys.tm_mon + 1,
				tm_sys.tm_mday,
				tm_sys.tm_hour,
				tm_sys.tm_min,
				tm_sys.tm_sec,
				tm_sys.tm_wday);
	}


	ret = simple_read_from_buffer(buffer, len, ppos, buf, PAGE_SIZE);
	kfree(buf);

	return ret;
}

static const struct file_operations s2m_settime_fops = {
	.owner = THIS_MODULE,
	.open = s2m_rtc_settime_open,
	.read = s2m_rtc_settime_read_registers,
};
#endif /* CONFIG_RTC_SETTIME_HISTORY */
#endif /* CONFIG_DEBUG_FS */

static int __devinit s2m_rtc_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_pmic_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct s2m_rtc_info *s2m;
	int ret;

	s2m = devm_kzalloc(&pdev->dev, sizeof(struct s2m_rtc_info),
				GFP_KERNEL);
	if (!s2m)
		return -ENOMEM;

	s2m->dev = &pdev->dev;
	s2m->iodev = iodev;

	s2m->wtsr_smpl = pdata->wtsr_smpl;

	switch (iodev->device_type) {
	case S2MPS14:
		s2m->irq = pdata->irq_base + S2MPS14_IRQ_RTCA0;
		break;
	case S2MPS11X:
		s2m->irq = pdata->irq_base + S2MPS11_IRQ_RTCA1;
		break;
	case S2MPA01:
		s2m->irq = pdata->irq_base + S2MPA01_IRQ_RTCA0;
		break;

	case S2MPU01X:
		s2m->irq = pdata->irq_base + S2MPU01_IRQ_RTCA0;
		break;
	case S2MPU01A:
		s2m->irq = pdata->irq_base + S2MPU01A_IRQ_RTCA0;
		break;
	default:
		/* If this happens the core funtion has a problem */
		BUG();
	}

	platform_set_drvdata(pdev, s2m);

	ret = s2m_rtc_init_reg(s2m);

	if (s2m->wtsr_smpl) {
		s2m_rtc_enable_wtsr(s2m, true);
/* Don't control smpl for B2 & Orbis model */
#if !defined(CONFIG_MACH_B2) && !defined(CONFIG_MACH_WINGTIP) \
	&& !defined(CONFIG_MACH_WC1)
		s2m_rtc_enable_smpl(s2m, true);
#endif
	}

	device_init_wakeup(&pdev->dev, true);

	s2m->rtc_dev = rtc_device_register("s2m-rtc", &pdev->dev,
			&s2m_rtc_ops, THIS_MODULE);

	if (IS_ERR(s2m->rtc_dev)) {
		ret = PTR_ERR(s2m->rtc_dev);
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
		goto out_rtc;
	}

	ret = request_threaded_irq(s2m->irq, NULL, s2m_rtc_alarm_irq, 0,
			"rtc-alarm0", s2m);

	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			s2m->irq, ret);
		goto out_rtc;
	}


#ifdef CONFIG_DEBUG_FS
		s2m->reg_rtc_debugfs_dir =
			debugfs_create_dir("s2m_rtc_debug", NULL);
		if (s2m->reg_rtc_debugfs_dir) {
			if (!debugfs_create_file("s2m_rtc_regs", 0644,
				s2m->reg_rtc_debugfs_dir,
				s2m, &s2m_debugfs_fops))
				pr_err("%s : debugfs_create_file, error\n", __func__);
#ifdef CONFIG_RTC_SETTIME_HISTORY
			if (!debugfs_create_file("settime_history", 0644,
				s2m->reg_rtc_debugfs_dir,
				s2m, &s2m_settime_fops))
				pr_err("%s : debugfs_create_file, error\n", __func__);
#endif
		} else
			pr_err("%s : debugfs_create_dir, error\n", __func__);



#endif



	return 0;

out_rtc:
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int __devexit s2m_rtc_remove(struct platform_device *pdev)
{
	struct s2m_rtc_info *s2m = platform_get_drvdata(pdev);

	if (s2m) {
		free_irq(s2m->irq, s2m);
		rtc_device_unregister(s2m->rtc_dev);
	}

	return 0;
}

static void s2m_rtc_shutdown(struct platform_device *pdev)
{
	struct s2m_rtc_info *info = platform_get_drvdata(pdev);
	int i, ret;
	unsigned int val = 0;

	if (info->wtsr_smpl) {
		for (i = 0; i < 3; i++) {
			s2m_rtc_enable_wtsr(info, false);
			ret = sec_rtc_read(info->iodev,
					S2M_RTC_WTSR_SMPL, &val);
			if (ret < 0) {
				dev_err(info->dev, "%s: fail to read SMPL reg(%d/%d)\n",
					__func__, (i + 1), ret);
				continue;
			} else {
				pr_info("%s: WTSR_SMPL reg(0x%02x)\n",
					__func__, val);
			}

			if (val & WTSR_ENABLE_MASK) {
				pr_emerg("%s: fail to disable WTSR\n",
				__func__);
			} else {
				pr_info("%s: success to disable WTSR\n",
					__func__);
				break;
			}
		}
	}

/* Don't control smpl for B2 & Orbis model */
#if !defined(CONFIG_MACH_B2) && !defined(CONFIG_MACH_WINGTIP) \
	&& !defined(CONFIG_MACH_WC1)
	/* Disable SMPL when power off */
	s2m_rtc_enable_smpl(info, false);
#endif
}

static const struct platform_device_id s2m_rtc_id[] = {
	{ "s2m-rtc", 0 },
};

static struct platform_driver s2m_rtc_driver = {
	.driver		= {
		.name	= "s2m-rtc",
		.owner	= THIS_MODULE,
	},
	.probe		= s2m_rtc_probe,
	.remove		= __devexit_p(s2m_rtc_remove),
	.shutdown	= s2m_rtc_shutdown,
	.id_table	= s2m_rtc_id,
};

static int __init s2m_rtc_init(void)
{
	return platform_driver_register(&s2m_rtc_driver);
}
module_init(s2m_rtc_init);

static void __exit s2m_rtc_exit(void)
{
	platform_driver_unregister(&s2m_rtc_driver);
}
module_exit(s2m_rtc_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("Samsung RTC driver");
MODULE_LICENSE("GPL");
