/*
 * rtc-s5m.c
 *
 * Copyright (c) 2011-2014 Samsung Electronics Co., Ltd
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
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/mfd/samsung/s5m8767.h>
#include <linux/mfd/samsung/s2mps14.h>
#include <linux/mfd/samsung/rtc.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/irq.h>

/*
 * Work-around for S5M8767 BIN->BCD time conversion issue.
 *
 * For the S5M8767 chip the time should be set in BCD mode to avoid issue with
 * S5M8767 BIN->BCD conversion (setting Year/Min/Sec to 0x38/0x39 will end
 * with wrong values set).
 * This does not apply to alarm setting.
 */
#define BCD_ALARM	0
#define BCD_TIME	1

/*
 * Maximum number of retries for checking changes in UDR field
 * (to limit possible endless loop).
 *
 * After writing to RTC registers (setting time or alarm) read the UDR field
 * in SEC_RTC_UDR_CON register. UDR is auto-cleared when registers data have
 * been transferred.
 */
#define UDR_READ_RETRY_CNT	5

/* Registers for S2MPS14 or S5M8767 */
struct s5m_rtc_reg_config {
	/* Number of registers used for setting time/alarm0/alarm1 */
	unsigned int regs_count;
	unsigned int ctrl;
	unsigned int time;
	unsigned int alarm0;
	unsigned int alarm1;
	unsigned int smpl_wtsr;
	unsigned int rtc_udr_update;
	unsigned int rtc_udr_mask;
};

/* Register map for S5M8763 and S5M8767 */
static const struct s5m_rtc_reg_config s5m_rtc_regs = {
	.regs_count		= 8,
	.time			= S5M_RTC_SEC,
	.ctrl			= S5M_ALARM1_CONF,
	.alarm0			= S5M_ALARM0_SEC,
	.alarm1			= S5M_ALARM1_SEC,
	.smpl_wtsr		= S5M_WTSR_SMPL_CNTL,
	.rtc_udr_update		= S5M_RTC_UDR_CON,
	.rtc_udr_mask		= S5M_RTC_UDR_MASK,
};

/*
 * Register map for S2MPS14.
 * It may be also suitable for S2MPS11 but this was not tested.
 */
static const struct s5m_rtc_reg_config s2mps_rtc_regs = {
	.regs_count		= 7,
	.time			= S2MPS_RTC_SEC,
	.ctrl			= S2MPS_RTC_CTRL,
	.alarm0			= S2MPS_ALARM0_SEC,
	.alarm1			= S2MPS_ALARM1_SEC,
	.smpl_wtsr		= S2MPS_WTSR_SMPL_CNTL,
	.rtc_udr_update		= S2MPS_RTC_UDR_CON,
	.rtc_udr_mask		= S2MPS_RTC_WUDR_MASK,
};

struct s5m_rtc_info {
	struct device			*dev;
	struct sec_pmic_dev		*iodev;
	struct i2c_client		*rtc;
	struct regmap			*regmap_rtc;
	struct rtc_device		*rtc_dev;
	int				irq;
	int				device_type;
	int				rtc_24hr_mode;
	struct sec_wtsr_smpl		*wtsr_smpl;
	bool				alarm_enabled;
	const struct s5m_rtc_reg_config	*regs;
};

static inline int s5m8767_rtc_calculate_wday(u8 shifted)
{
	int counter = -1;
	while (shifted) {
		shifted >>= 1;
		counter++;
	}
	return counter;
}

/*
 * Read RTC_UDR_CON register and wait till UDR field is cleared.
 * This indicates that time update ended.
 */
static inline void s5m_rtc_wait_for_udr_update(struct s5m_rtc_info *info)
{
	int err, retry = UDR_READ_RETRY_CNT;
	u8 data;

	do {
		err = sec_reg_read(info->regmap_rtc,
				info->regs->rtc_udr_update, &data);
	} while (--retry && (data & info->regs->rtc_udr_mask) && !err);
	if (!retry)
		dev_err(info->dev, "waiting for UDR update, reached max number of retries (last UDR: %d)\n", data & info->regs->rtc_udr_mask);
}

static inline int s5m_check_peding_alarm_interrupt(struct s5m_rtc_info *info,
		struct rtc_wkalrm *alarm)
{
	int ret;
	u8 val;

	switch (info->device_type) {
	case S5M8767X:
	case S5M8763X:
		ret = sec_reg_read(info->regmap_rtc, S5M_RTC_STATUS, &val);
		val &= S5M_ALARM0_STATUS;
		break;
	case S2MPS14X:
		ret = sec_reg_read(info->iodev->regmap_pmic, S2MPS14_REG_ST2, &val);
		val &= S2MPS_ALARM0_STATUS;
		break;
	default:
		return -EINVAL;
	}
	if (ret < 0)
		return ret;

	if (val)
		alarm->pending = 1;
	else
		alarm->pending = 0;

	return 0;
}

static void s5m8767_data_to_tm(u8 *data, struct rtc_time *tm,
				int rtc_24hr_mode)
{
	tm->tm_sec = data[RTC_SEC] & 0x7f;
	tm->tm_min = data[RTC_MIN] & 0x7f;
	if (rtc_24hr_mode)
		tm->tm_hour = data[RTC_HOUR] & 0x1f;
	else {
		tm->tm_hour = data[RTC_HOUR] & 0x0f;
		if (data[RTC_HOUR] & HOUR_PM_MASK)
			tm->tm_hour += 12;
	}

	tm->tm_wday = s5m8767_rtc_calculate_wday(data[RTC_WEEKDAY] & 0x7f);
	tm->tm_mday = data[RTC_DATE] & 0x1f;
	tm->tm_mon = (data[RTC_MONTH] & 0x0f) - 1;
	tm->tm_year = (data[RTC_YEAR1] & 0x7f) + (bcd2bin(data[RTC_YEAR2]) * 100);
	tm->tm_year -= 1900;
	tm->tm_yday = 0;
	tm->tm_isdst = 0;
}

static int s5m8767_tm_to_data(struct device *dev, struct rtc_time *tm, u8 *data, int set_mode)
{
	struct s5m_rtc_info *info = dev_get_drvdata(dev);
	int bcd_mode = 0;

	if ((tm->tm_sec == 56 || tm->tm_sec == 57 ||
				tm->tm_min == 56 || tm->tm_min == 57
				|| tm->tm_year == 56 || tm->tm_year == 57)
				&& set_mode) {
		data[RTC_SEC] = bin2bcd(tm->tm_sec);
		data[RTC_MIN] = bin2bcd(tm->tm_min);

		if (tm->tm_hour >= 12)
			data[RTC_HOUR] = bin2bcd(tm->tm_hour) | HOUR_PM_MASK;
		else
			data[RTC_HOUR] = bin2bcd(tm->tm_hour) & ~HOUR_PM_MASK;

		data[RTC_DATE] = bin2bcd(tm->tm_mday);
		data[RTC_MONTH] = bin2bcd(tm->tm_mon + 1);
		data[RTC_YEAR1] = bin2bcd(tm->tm_year % 100);

		sec_reg_update(info->regmap_rtc, info->regs->ctrl,
			BCD_EN_MASK, BCD_EN_MASK);
		bcd_mode = 1;
	} else {
		data[RTC_SEC] = tm->tm_sec;
		data[RTC_MIN] = tm->tm_min;

		if (tm->tm_hour >= 12)
			data[RTC_HOUR] = tm->tm_hour | HOUR_PM_MASK;
		else
			data[RTC_HOUR] = tm->tm_hour & ~HOUR_PM_MASK;

		data[RTC_DATE] = tm->tm_mday;
		data[RTC_MONTH] = tm->tm_mon + 1;
		data[RTC_YEAR1] = tm->tm_year % 100;
		bcd_mode = 0;
	}


	data[RTC_WEEKDAY] = 1 << tm->tm_wday;
	data[RTC_YEAR2] = bin2bcd((tm->tm_year + 1900) / 100);

	return bcd_mode;
}

static void s2mps_data_to_tm(u8 *data, struct rtc_time *tm,
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

	tm->tm_wday = ffs(data[RTC_WEEKDAY] & 0x7f);
	tm->tm_mday = data[RTC_DATE] & 0x1f;
	tm->tm_mon = (data[RTC_MONTH] & 0x0f) - 1;
	tm->tm_year = (data[RTC_YEAR1] & 0x7f) + 100;
	tm->tm_yday = 0;
	tm->tm_isdst = 0;
}

static int s2mps_tm_to_data(struct device *dev, struct rtc_time *tm, u8 *data)
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
	data[RTC_YEAR1] = tm->tm_year > 100 ? (tm->tm_year - 100) : 0;

	if (tm->tm_year < 100) {
		dev_err(dev, "RTC cannot handle the year %d.\n",
		       1900 + tm->tm_year);
		return -EINVAL;
	} else {
		return 0;
	}
}

static inline int s5m8767_rtc_set_time_reg(struct s5m_rtc_info *info)
{
	int ret;
	u8 data;

	ret = sec_reg_read(info->regmap_rtc, info->regs->rtc_udr_update, &data);
	if (ret < 0)
		return ret;

	data |= info->regs->rtc_udr_mask;
	if (info->device_type == S5M8763X || info->device_type == S5M8767X) {
		data |= S5M_RTC_TIME_EN_MASK;
		data |= S5M_RTC_UDR_T_MASK;
		data &= ~S5M_RTC_TEST_OSC_MASK;
	}

	ret = sec_reg_write(info->regmap_rtc, info->regs->rtc_udr_update, data);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
			__func__, ret);
	else
		s5m_rtc_wait_for_udr_update(info);

	return ret;
}

static inline int s5m8767_rtc_set_alarm_reg(struct s5m_rtc_info *info)
{
	int ret;
	u8 data;

	ret = sec_reg_read(info->regmap_rtc, info->regs->rtc_udr_update, &data);
	if (ret < 0)
		return ret;

	switch (info->device_type) {
	case S5M8763X:
	case S5M8767X:
		data &= ~S5M_RTC_TIME_EN_MASK;
		data |= info->regs->rtc_udr_mask;
		data |= S5M_RTC_UDR_T_MASK;
		data &= ~S5M_RTC_TEST_OSC_MASK;
		break;
	case S2MPS14X:
		/* Set RUDR and WUDR bits to high */
		data |= S2MPS_RTC_RUDR_MASK;
		data |= info->regs->rtc_udr_mask;
		break;
	default:
		return -EINVAL;
	}

	ret = sec_reg_write(info->regmap_rtc, info->regs->rtc_udr_update, data);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
				__func__, ret);
	} else
		s5m_rtc_wait_for_udr_update(info);

	return ret;
}

static void s5m8763_data_to_tm(u8 *data, struct rtc_time *tm)
{
	tm->tm_sec = bcd2bin(data[RTC_SEC]);
	tm->tm_min = bcd2bin(data[RTC_MIN]);

	if (data[RTC_HOUR] & HOUR_12) {
		tm->tm_hour = bcd2bin(data[RTC_HOUR] & 0x1f);
		if (data[RTC_HOUR] & HOUR_PM)
			tm->tm_hour += 12;
	} else
		tm->tm_hour = bcd2bin(data[RTC_HOUR] & 0x3f);

	tm->tm_wday = data[RTC_WEEKDAY] & 0x07;
	tm->tm_mday = bcd2bin(data[RTC_DATE]);
	tm->tm_mon = bcd2bin(data[RTC_MONTH]);
	tm->tm_year = bcd2bin(data[RTC_YEAR1]) + bcd2bin(data[RTC_YEAR2]) * 100;
	tm->tm_year -= 1900;
}

static void s5m8763_tm_to_data(struct rtc_time *tm, u8 *data)
{
	data[RTC_SEC] = bin2bcd(tm->tm_sec);
	data[RTC_MIN] = bin2bcd(tm->tm_min);
	data[RTC_HOUR] = bin2bcd(tm->tm_hour);
	data[RTC_WEEKDAY] = tm->tm_wday;
	data[RTC_DATE] = bin2bcd(tm->tm_mday);
	data[RTC_MONTH] = bin2bcd(tm->tm_mon);
	data[RTC_YEAR1] = bin2bcd(tm->tm_year % 100);
	data[RTC_YEAR2] = bin2bcd((tm->tm_year + 1900) / 100);
}

static int s5m_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct s5m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[info->regs->regs_count];
	int ret;

	if (info->device_type == S2MPS14X) {
		ret = sec_reg_update(info->regmap_rtc,
				info->regs->rtc_udr_update,
				S2MPS_RTC_RUDR_MASK,
				S2MPS_RTC_RUDR_MASK);
		if (ret) {
			dev_err(dev, "Failed to prepare registers for time reading: %d\n",
					ret);
			return ret;
		}
	}
	ret = sec_bulk_read(info->regmap_rtc, info->regs->time, data,
			info->regs->regs_count);
	if (ret < 0)
		goto out;

	switch (info->device_type) {
	case S5M8763X:
		s5m8763_data_to_tm(data, tm);
		break;

	case S5M8767X:
		s5m8767_data_to_tm(data, tm, info->rtc_24hr_mode);
		break;

	case S2MPS14X:
		s2mps_data_to_tm(data, tm, info->rtc_24hr_mode);
		break;

	default:
		ret = (-EINVAL);
		goto out;
	}

	pr_debug("%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

out:
	if (ret >= 0)
		ret = rtc_valid_tm(tm);
	return ret;
}

static int s5m_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct s5m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[info->regs->regs_count];
	int ret, bcd_mode = 0;

	switch (info->device_type) {
	case S5M8763X:
		s5m8763_tm_to_data(tm, data);
		break;
	case S5M8767X:
		bcd_mode = s5m8767_tm_to_data(dev, tm, data, BCD_TIME);
		break;
	case S2MPS14X:
		ret = s2mps_tm_to_data(dev, tm, data);
		if (ret)
			return ret;
		break;
	default:
		return -EINVAL;
	}

	pr_debug( "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	ret = sec_bulk_write(info->regmap_rtc, info->regs->time, data,
			info->regs->regs_count);
        if (ret < 0)
		goto out;

	ret = s5m8767_rtc_set_time_reg(info);
	/*
	 * Simple work-around for S5M876X AM/PM update logic bug - update
	 * the time registers twice.
	 *
	 * Setting AM hour after PM hour for the first time will result in
	 * Date and Weekday incrementation. Second write to registers will
	 * set proper values.
	 */
	if (info->device_type == S5M8763X || info->device_type == S5M8767X)
		ret = s5m8767_rtc_set_time_reg(info);

	if (bcd_mode)
		sec_reg_update(info->regmap_rtc, info->regs->ctrl,
			BCD_EN_MASK, ~BCD_EN_MASK);
out:
	return ret;
}

static int s5m_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s5m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[info->regs->regs_count];
	u8 val;
	int ret, i;

	ret = sec_bulk_read(info->regmap_rtc, info->regs->alarm0, data,
			info->regs->regs_count);
	if (ret < 0)
		goto out;

	switch (info->device_type) {
	case S5M8763X:
		s5m8763_data_to_tm(data, &alrm->time);
		ret = sec_reg_read(info->regmap_rtc, S5M_ALARM0_CONF, &val);
		if (ret < 0)
			goto out;

		alrm->enabled = !!val;
		break;

	case S5M8767X:
		s5m8767_data_to_tm(data, &alrm->time, info->rtc_24hr_mode);
		alrm->enabled = 0;
		for (i = 0; i < info->regs->regs_count; i++) {
			if (data[i] & ALARM_ENABLE_MASK) {
				alrm->enabled = 1;
				break;
			}
		}
		break;

	case S2MPS14X:
		s2mps_data_to_tm(data, &alrm->time, info->rtc_24hr_mode);
		alrm->enabled = 0;
		for (i = 0; i < info->regs->regs_count; i++) {
			if (data[i] & ALARM_ENABLE_MASK) {
				alrm->enabled = 1;
				break;
			}
		}
		break;

	default:
		ret = (-EINVAL);
		goto out;
	}

	pr_debug( "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour,
		alrm->time.tm_min, alrm->time.tm_sec,
		alrm->time.tm_wday);

	ret = s5m_check_peding_alarm_interrupt(info, alrm);
out:
	return ret;
}

static int s5m_rtc_stop_alarm(struct s5m_rtc_info *info)
{
	u8 data[info->regs->regs_count];
	int ret, i;
	struct rtc_time tm;

	ret = sec_bulk_read(info->regmap_rtc, info->regs->alarm0, data,
			info->regs->regs_count);
	if (ret < 0)
		return ret;

	switch (info->device_type) {
	case S5M8763X:
		s5m8763_data_to_tm(data, &tm);
		break;
	case S5M8767X:
		s5m8767_data_to_tm(data, &tm, info->rtc_24hr_mode);
		break;
	case S2MPS14X:
		s2mps_data_to_tm(data, &tm, info->rtc_24hr_mode);
		break;
	default:
		return -EINVAL;
	}

	pr_debug( "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	switch (info->device_type) {
	case S5M8763X:
		ret = sec_reg_write(info->regmap_rtc, S5M_ALARM0_CONF, 0);
		break;

	case S5M8767X:
	case S2MPS14X:
		for (i = 0; i < info->regs->regs_count; i++)
			data[i] &= ~ALARM_ENABLE_MASK;

		ret = sec_bulk_write(info->regmap_rtc, info->regs->alarm0, data,
				info->regs->regs_count);
		if (ret < 0)
			return ret;

		ret = s5m8767_rtc_set_alarm_reg(info);
		break;

	default:
		return -EINVAL;
	}

	info->alarm_enabled = false;

	return ret;
}

static int s5m_rtc_start_alarm(struct s5m_rtc_info *info)
{
	int ret;
	u8 data[info->regs->regs_count];
	u8 alarm0_conf;
	struct rtc_time tm;

	ret = sec_bulk_read(info->regmap_rtc, info->regs->alarm0, data,
			info->regs->regs_count);
	if (ret < 0)
		return ret;

	switch (info->device_type) {
	case S5M8763X:
		s5m8763_data_to_tm(data, &tm);
		break;
	case S5M8767X:
		s5m8767_data_to_tm(data, &tm, info->rtc_24hr_mode);
		break;
	case S2MPS14X:
		s2mps_data_to_tm(data, &tm, info->rtc_24hr_mode);
		break;
	default:
		return -EINVAL;
	}

	pr_debug( "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	switch (info->device_type) {
	case S5M8763X:
		alarm0_conf = 0x77;
		ret = sec_reg_write(info->regmap_rtc, S5M_ALARM0_CONF, alarm0_conf);
		break;

	case S5M8767X:
	case S2MPS14X:
		data[RTC_SEC] |= ALARM_ENABLE_MASK;
		data[RTC_MIN] |= ALARM_ENABLE_MASK;
		data[RTC_HOUR] |= ALARM_ENABLE_MASK;
		data[RTC_WEEKDAY] &= ~ALARM_ENABLE_MASK;
		if (data[RTC_DATE] & 0x1f)
			data[RTC_DATE] |= ALARM_ENABLE_MASK;
		if (data[RTC_MONTH] & 0xf)
			data[RTC_MONTH] |= ALARM_ENABLE_MASK;
		if (data[RTC_YEAR1] & 0x7f)
			data[RTC_YEAR1] |= ALARM_ENABLE_MASK;

		ret = sec_bulk_write(info->regmap_rtc, info->regs->alarm0, data,
				info->regs->regs_count);
		if (ret < 0)
			return ret;
		ret = s5m8767_rtc_set_alarm_reg(info);

		break;

	default:
		return -EINVAL;
	}

	info->alarm_enabled = true;

	return ret;
}

static int s5m_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s5m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[info->regs->regs_count];
	int ret, bcd_mode = 0;
	unsigned char enabled = alrm->enabled;

	switch (info->device_type) {
	case S5M8763X:
		s5m8763_tm_to_data(&alrm->time, data);
		break;
	case S5M8767X:
		bcd_mode = s5m8767_tm_to_data(dev, &alrm->time, data, BCD_ALARM);
		break;
	case S2MPS14X:
		ret = s2mps_tm_to_data(dev, &alrm->time, data);
		if (ret)
			return ret;
		break;
	default:
		return -EINVAL;
	}

	pr_debug( "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour, alrm->time.tm_min,
		alrm->time.tm_sec, alrm->time.tm_wday);

	ret = s5m_rtc_stop_alarm(info);
	if (ret < 0)
		goto out;

	ret = sec_bulk_write(info->regmap_rtc, info->regs->alarm0, data,
			info->regs->regs_count);
	if (ret < 0)
		goto out;

	ret = s5m8767_rtc_set_alarm_reg(info);
	if (ret < 0)
		goto out;

	if (enabled)
		ret = s5m_rtc_start_alarm(info);

	if (bcd_mode)
		sec_reg_update(info->regmap_rtc, info->regs->ctrl,
			BCD_EN_MASK, ~BCD_EN_MASK);
out:
	return ret;
}

static int s5m_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct s5m_rtc_info *info = dev_get_drvdata(dev);
	int ret;

	if (enabled) {
		ret = s5m_rtc_start_alarm(info);
	} else {
		ret = s5m_rtc_stop_alarm(info);
	}

	return ret;
}

static irqreturn_t s5m_rtc_alarm_irq(int irq, void *data)
{
	struct s5m_rtc_info *info = data;

	dev_info(info->dev, "alarm IRQ, irq: %d\n", irq);
	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);

	return IRQ_HANDLED;
}

static const struct rtc_class_ops s5m_rtc_ops = {
	.read_time = s5m_rtc_read_time,
	.set_time = s5m_rtc_set_time,
	.read_alarm = s5m_rtc_read_alarm,
	.set_alarm = s5m_rtc_set_alarm,
	.alarm_irq_enable = s5m_rtc_alarm_irq_enable,
};

static void s5m_rtc_enable_wtsr(struct s5m_rtc_info *info, bool enable)
{
	int ret;
	u8 val, mask;

	if (enable)
		val = WTSR_ENABLE_MASK;
	else
		val = 0;

	mask = WTSR_ENABLE_MASK;

	dev_info(info->dev, "%s: %s WTSR\n", __func__,
		 enable ? "enable" : "disable");

	ret = sec_reg_update(info->regmap_rtc, info->regs->smpl_wtsr, mask, val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update WTSR reg(%d)\n",
			__func__, ret);
		return;
	}
	ret = s5m8767_rtc_set_alarm_reg(info);
}

static void s5m_rtc_enable_smpl(struct s5m_rtc_info *info, bool enable)
{
	int ret;
	u8 val, mask;

	if (enable)
		val = SMPL_ENABLE_MASK;
	else
		val = 0;

	mask = SMPL_ENABLE_MASK;

	dev_info(info->dev, "%s: %s SMPL\n", __func__,
			enable ? "enable" : "disable");

	ret = sec_reg_update(info->regmap_rtc, info->regs->smpl_wtsr, mask, val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update SMPL reg(%d)\n",
				__func__, ret);
		return;
	}
	ret = s5m8767_rtc_set_alarm_reg(info);

	val = 0;
	sec_reg_read(info->regmap_rtc, info->regs->smpl_wtsr, &val);
	pr_info("%s: WTSR_SMPL(0x%02x)\n", __func__, val);
}

static int s5m8767_rtc_init_reg(struct s5m_rtc_info *info)
{
	u8 data[2];
	int ret;

	switch (info->device_type) {
	case S5M8763X:
	case S5M8767X:
		/* Set RTC control register : Binary mode, 24hour mdoe */
		data[0] = (1 << BCD_EN_SHIFT) | (1 << MODEL24_SHIFT);
		data[1] = (0 << BCD_EN_SHIFT) | (1 << MODEL24_SHIFT);

		ret = sec_bulk_write(info->regmap_rtc, S5M_ALARM0_CONF, data, 2);
		break;

	case S2MPS14X:
		data[0] = (0 << BCD_EN_SHIFT) | (1 << MODEL24_SHIFT);
		ret = sec_reg_write(info->regmap_rtc, info->regs->ctrl, data[0]);
		break;

	default:
		return -EINVAL;
	}

	info->rtc_24hr_mode = 1;
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write controlm reg(%d)\n",
				__func__, ret);
		return ret;
	}

	return ret;
}

static struct sec_wtsr_smpl default_wtsr_smpl_data = {
	.wtsr_en = true,
#if defined(CONFIG_SEC_FACTORY_MODE)
	.smpl_en = false, 
#endif
};

static int s5m_rtc_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_platform_data *pdata = iodev->pdata;
	struct s5m_rtc_info *info;
	int ret;

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(struct s5m_rtc_info),
			GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &pdev->dev;
	info->iodev = iodev;
	info->rtc = iodev->rtc;
	info->regmap_rtc = iodev->regmap_rtc;
	info->device_type = iodev->device_type;
	if (pdata->wtsr_smpl)
		info->wtsr_smpl = pdata->wtsr_smpl;
	else
		info->wtsr_smpl = &default_wtsr_smpl_data;

	switch (pdata->device_type) {
	case S5M8763X:
		info->irq = regmap_irq_get_virq(iodev->irq_data,
						S5M8763_IRQ_ALARM0);
		info->regs = &s5m_rtc_regs;
		break;

	case S5M8767X:
		info->irq = regmap_irq_get_virq(iodev->irq_data,
						S5M8767_IRQ_RTCA1);
		info->regs = &s5m_rtc_regs;
		break;

	case S2MPS14X:
		info->irq = regmap_irq_get_virq(iodev->irq_data,
						S2MPS14_IRQ_RTCA0);
		info->regs = &s2mps_rtc_regs;
		break;
	default:
		ret = -EINVAL;
		dev_err(&pdev->dev, "Unsupported device type: %d\n", ret);
		goto out_rtc;
	}

	platform_set_drvdata(pdev, info);

	ret = s5m8767_rtc_init_reg(info);

	if (info->wtsr_smpl) {
		s5m_rtc_enable_wtsr(info, info->wtsr_smpl->wtsr_en);
#if defined(CONFIG_SEC_FACTORY_MODE)
		s5m_rtc_enable_smpl(info, info->wtsr_smpl->smpl_en);
#endif
	}

	device_init_wakeup(&pdev->dev, 1);

	info->rtc_dev = devm_rtc_device_register(&pdev->dev, "s5m-rtc",
			&s5m_rtc_ops, THIS_MODULE);

	if (IS_ERR(info->rtc_dev)) {
		ret = PTR_ERR(info->rtc_dev);
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
		goto out_rtc;
	}

	ret = request_threaded_irq(info->irq, NULL, s5m_rtc_alarm_irq,
				   pdata->irqflags, "rtc-alarm0", info);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			info->irq, ret);

	dev_info(&pdev->dev, "RTC CHIP NAME: %s\n", pdev->id_entry->name);

	return 0;

out_rtc:
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int s5m_rtc_remove(struct platform_device *pdev)
{
	struct s5m_rtc_info *info = platform_get_drvdata(pdev);

	if (info)
		free_irq(info->irq, info);

	return 0;
}

static void s5m_rtc_shutdown(struct platform_device *pdev)
{
	struct s5m_rtc_info *info = platform_get_drvdata(pdev);
	int i;
	u8 val = 0;
	if (info->wtsr_smpl->wtsr_en) {
		for (i = 0; i < 3; i++) {
			s5m_rtc_enable_wtsr(info, false);
			sec_reg_read(info->regmap_rtc, info->regs->smpl_wtsr, &val);
			pr_info("%s: WTSR_SMPL reg(0x%02x)\n", __func__, val);
			if (val & WTSR_ENABLE_MASK)
				pr_emerg("%s: fail to disable WTSR\n", __func__);
			else {
				pr_info("%s: success to disable WTSR\n", __func__);
				break;
			}
		}
	}
	/* Disable SMPL when power off */
	s5m_rtc_enable_smpl(info, false);
}

static const struct platform_device_id s5m_rtc_id[] = {
	{ "s5m-rtc",		S5M8767X },
	{ "s2mps14-rtc",	S2MPS14X },
	{ },
};

#ifdef CONFIG_PM_SLEEP
static int s5m_rtc_resume(struct device *dev)
{
	struct s5m_rtc_info *info = dev_get_drvdata(dev);
	int ret = 0;

	if (device_may_wakeup(dev))
		ret = disable_irq_wake(info->irq);

	return ret;
}

static int s5m_rtc_suspend(struct device *dev)
{
	struct s5m_rtc_info *info = dev_get_drvdata(dev);
	int ret = 0;

	if (device_may_wakeup(dev))
		ret = enable_irq_wake(info->irq);

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(s5m_rtc_pm_ops, s5m_rtc_suspend, s5m_rtc_resume);

static struct platform_driver s5m_rtc_driver = {
	.driver		= {
		.name	= "s5m-rtc",
		.owner	= THIS_MODULE,
		.pm	= &s5m_rtc_pm_ops,
	},
	.probe		= s5m_rtc_probe,
	.remove		= s5m_rtc_remove,
	.shutdown	= s5m_rtc_shutdown,
	.id_table	= s5m_rtc_id,
};

static int __init s5m_rtc_init(void)
{
	return platform_driver_register(&s5m_rtc_driver);
}
module_init(s5m_rtc_init);

static void __exit s5m_rtc_exit(void)
{
	platform_driver_unregister(&s5m_rtc_driver);
}
module_exit(s5m_rtc_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("Samsung S5M RTC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:s5m-rtc");
