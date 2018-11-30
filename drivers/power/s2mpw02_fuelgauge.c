/*
 *  s2mpw02_fuelgauge.c
 *  Samsung S2MPW02 Fuel Gauge Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 *  Developed by Nguyen Tien Dat (tiendat.nt@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define SINGLE_BYTE	1

#include <linux/power/s2mpw02_fuelgauge.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/of_gpio.h>
#include <linux/mfd/samsung/s2mpw02.h>
#include <linux/mfd/samsung/s2mpw02-regulator.h>
#include <linux/mfd/samsung/rtc-s2mp.h>
#include <linux/wakelock.h>

static enum power_supply_property s2mpw02_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
	POWER_SUPPLY_PROP_CURRENT_SUSPEND,
#endif
};

static int s2mpw02_fg_write_reg_byte(struct i2c_client *client, int reg, u8 data)
{
	int ret, i = 0;

	ret = s2mpw02_write_reg(client, reg,  data);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = s2mpw02_write_reg(client, reg, data);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}

	return ret;
}

static int s2mpw02_fg_write_reg(struct i2c_client *client, int reg, u8 *buf)
{
#if SINGLE_BYTE
	int ret = 0;

	s2mpw02_fg_write_reg_byte(client, reg, buf[0]);
	s2mpw02_fg_write_reg_byte(client, reg+1, buf[1]);
#else
	int ret, i = 0;

	ret = s2mpw02_bulk_write(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = s2mpw02_bulk_write(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

static int s2mpw02_fg_read_reg_byte(struct i2c_client *client, int reg, void *data)
{
	int ret;
	u8 temp = 0;

	ret = s2mpw02_read_reg(client, reg, &temp);
	if (ret < 0)
		return ret;
	*(u8 *)data = (u8)temp;

	return ret;
}

static int s2mpw02_fg_read_reg(struct i2c_client *client, int reg, u8 *buf)
{
#if SINGLE_BYTE
	int ret = 0;
	u8 data1 = 0, data2 = 0;

	s2mpw02_fg_read_reg_byte(client, reg, &data1);
	s2mpw02_fg_read_reg_byte(client, reg + 1, &data2);
	buf[0] = data1;
	buf[1] = data2;
#else
	int ret = 0, i = 0;

	ret = s2mpw02_bulk_read(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = s2mpw02_bulk_read(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

static void s2mpw02_fg_test_read(struct i2c_client *i2c)
{
	u8 data;
	char str[2000] = {0,};
	int i;

	s2mpw02_fg_read_reg_byte(i2c, S2MPW02_FG_REG_STATUS, &data);
	sprintf(str+strlen(str), "%x:%02x ", S2MPW02_FG_REG_STATUS, data);
	s2mpw02_fg_read_reg_byte(i2c, S2MPW02_FG_REG_STATUS2, &data);
	sprintf(str+strlen(str), "%x:%02x ", S2MPW02_FG_REG_STATUS2, data);

	for (i = S2MPW02_FG_REG_INTM; i <= S2MPW02_FG_REG_RZADJ2; i++) {
		s2mpw02_fg_read_reg_byte(i2c, i, &data);
		sprintf(str+strlen(str), "%x:%02x ", i, data);
	}

	for (i = S2MPW02_FG_REG_IRQ_LVL; i <= S2MPW02_FG_REG_CURR2; i++) {
		s2mpw02_fg_read_reg_byte(i2c, i, &data);
		sprintf(str+strlen(str), "%x:%02x ", i, data);
	}

	for (i = S2MPW02_FG_REG_ADC_DATA; i <= S2MPW02_FG_REG_VTL_SHIFT; i++) {
		s2mpw02_fg_read_reg_byte(i2c, i, &data);
		sprintf(str+strlen(str), "%x:%02x ", i, data);
	}

	s2mpw02_fg_read_reg_byte(i2c, S2MPW02_FG_OTP_6, &data);
	sprintf(str+strlen(str), "%x:%02x ", S2MPW02_FG_OTP_6, data);

	pr_info("%s: %s\n", __func__, str);
}

static void s2mpw02_restart_gauging(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 temp;

	mutex_lock(&fuelgauge->fg_lock);

	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_CONFIG2, &temp);
	temp |= (CHG_I2C_EN_MASK | CHG_I2C_MASK);
	s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_CONFIG2, temp);

	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_CONFIG, &temp);
	temp |= (DUMP_DONE_MASK | RESTART_MASK);
	s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_CONFIG, temp);
	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_CONFIG, &temp);

	dev_info(&fuelgauge->i2c->dev, "%s: dump done 0x%x\n", __func__, temp);

	msleep(500);
	mutex_unlock(&fuelgauge->fg_lock);
}

#if 0
static int s2mpw02_init_regs(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	int ret = 0;
	pr_err("%s: s2mpw02 fuelgauge initialize\n", __func__);
	return ret;
}
#endif

static void s2mpw02_alert_init(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	/* VBAT Threshold setting: 3.55V */
	data[0] = 0x00;

	/* SOC Threshold setting */
	data[0] = data[0] | (fuelgauge->pdata->fuel_alert_soc << SOC_L_LVL_SHIFT);

	data[1] = 0x00;
	s2mpw02_fg_write_reg(fuelgauge->i2c, S2MPW02_FG_REG_IRQ_LVL, data);
}

static bool s2mpw02_check_status(struct i2c_client *client)
{
	u8 data[2];
	bool ret = false;

	/* check if Smn was generated */
	if (s2mpw02_fg_read_reg(client, S2MPW02_FG_REG_STATUS, data) < 0)
		return ret;

	dev_dbg(&client->dev, "%s: status to (%02x%02x)\n",
		__func__, data[1], data[0]);

	if (data[1] & SOC_L_MASK)
		return true;
	else
		return false;
}

static int s2mpw02_set_temperature(struct s2mpw02_fuelgauge_data *fuelgauge,
			int temperature)
{
	char val;
	u8 temp1, temp2;
	int temperature_level;

	val = temperature / 10;

	if (val < 0)
		temperature_level = TEMP_LEVEL_VERY_LOW;
	else if (val < 15)
		temperature_level = TEMP_LEVEL_LOW;
	else if (val > 35)
		temperature_level = TEMP_LEVEL_HIGH;
	else
		temperature_level = TEMP_LEVEL_MID;

	if (fuelgauge->before_temp_level == temperature_level)
		return temperature;

	fuelgauge->before_temp_level = temperature_level;

	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_MONOUT_CFG, &temp1);
	temp1 &= 0x0F;
	temp2 = 0;

	if (val < 15) {
		s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_RTEMP, 10);
	} else if (val >= 15 && val <= 35) {
		s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_RTEMP, 25);
	} else if (val > 35) {
		s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_RTEMP, 40);
	}

	dev_info(&fuelgauge->i2c->dev, "%s: temperature to (%d)\n",
		__func__, temperature);

	return temperature;
}

static int s2mpw02_get_temperature(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	s32 temperature = 0;

	/*
	 *  use monitor regiser.
	 *  monitor register default setting is temperature
	 */
	if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_RTEMP, data) < 0)
		return -ERANGE;

	/* data[] store 2's compliment format number */
	if (data[0] & (0x1 << 7)) {
		/* Negative */
		temperature = ((~(data[0])) & 0xFF) + 1;
		temperature *= -10;
	} else {
		temperature = data[0] & 0x7F;
		temperature *= 10;
	}

	dev_dbg(&fuelgauge->i2c->dev, "%s: temperature (%d)\n",
		__func__, temperature);

	return temperature;
}

extern int yu_battery_capacity;

/* soc should be 0.01% unit */
static int s2mpw02_get_soc(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2];
	u16 compliment;
	int rsoc, i;

	mutex_lock(&fuelgauge->fg_lock);

	for (i = 0; i < 5; i++) {
		if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_RSOC, data) < 0)
			goto err;
		if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_RSOC, check_data) < 0)
			goto err;
		if ((data[0] == check_data[0]) && (data[1] == check_data[1]))
			break;

		dev_dbg(&fuelgauge->i2c->dev,
			"[DEBUG]%s: data0 (%d) data1 (%d) check_data0 (%d) check_data1 (%d)\n",
			__func__, data[0], data[1], check_data[0], check_data[1]);
	}

	mutex_unlock(&fuelgauge->fg_lock);

	dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		rsoc = ((~compliment) & 0xFFFF) + 1;
		rsoc = (rsoc * (-10000)) / (0x1 << 12);
	} else {
		rsoc = compliment & 0x7FFF;
		rsoc = ((rsoc * 10000) / (0x1 << 12));
	}

	dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: raw capacity (0x%x:%d)\n", __func__,
		compliment, rsoc);

	return min(rsoc, 10000) / 10;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mpw02_get_rawsoc(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2];
	u16 compliment;
	int rsoc, i;

	mutex_lock(&fuelgauge->fg_lock);

	for (i = 0; i < 5; i++) {
		if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_RSOC, data) < 0)
			goto err;
		if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_RSOC, check_data) < 0)
			goto err;
		if ((data[0] == check_data[0]) && (data[1] == check_data[1]))
			break;
		dev_dbg(&fuelgauge->i2c->dev,
			"[DEBUG]%s: data0 (%d) data1 (%d) check_data0 (%d) check_data1 (%d)\n",
			__func__, data[0], data[1], check_data[0], check_data[1]);
	}

	mutex_unlock(&fuelgauge->fg_lock);

	dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: data0 (%d) data1 (%d)\n",
		__func__, data[0], data[1]);
	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		rsoc = ((~compliment) & 0xFFFF) + 1;
		rsoc = (rsoc * (-10000)) / (0x1 << 12);
	} else {
		rsoc = compliment & 0x7FFF;
		rsoc = ((rsoc * 10000) / (0x1 << 12));
	}

	dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: raw capacity (0x%x:%d)\n", __func__,
			compliment, rsoc);
	fuelgauge->log.raw_soc = rsoc;

	return min(rsoc, 10000);

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mpw02_get_current(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2], temp;
	u16 compliment;
	int curr = 0;

	mutex_lock(&fuelgauge->fg_lock);

	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_MONOUT_CFG, &temp);
	temp &= ~MONOUT_SEL_MASK;
	temp |= MONOUT_SEL_RCUR;
	s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_MONOUT_CFG, temp);

	if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_CURR, data) < 0) {
		mutex_unlock(&fuelgauge->fg_lock);
		return -EINVAL;
	}

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(fuelgauge->dev, "%s: rCUR_CC(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 12;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 12;
	}

	dev_dbg(&fuelgauge->i2c->dev, "%s: current (%d)mA (0x%4x)\n", __func__, curr, compliment);
	fuelgauge->log.curr = curr;

	return curr;
}

#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
static int s2mpw02_get_current_suspend(
	struct s2mpw02_fuelgauge_data *fuelgauge, int type)
{
	int val = -1;

	if (type == POWER_SUPPLY_CURRENT_SUSPEND_CHARGING ||
		type == POWER_SUPPLY_CURRENT_SUSPEND_DISCHARGING) {
		val = fuelgauge->suspend_current;
		fuelgauge->suspend_current = 0;
	}

	return val;
}

static int s2mpw02_calc_current_suspend(
	struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2], temp;
	u16 compliment, status;
	int curr = 0;

	mutex_lock(&fuelgauge->fg_lock);

	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_MONOUT_CFG, &temp);
	temp &= ~MONOUT_SEL_MASK;
	temp |= MONOUT_SEL_RCUR;
	s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_MONOUT_CFG, temp);

	if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_CURR, data) < 0) {
		mutex_unlock(&fuelgauge->fg_lock);
		return -EINVAL;
	}

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);

	status = compliment & (0x1 << 15);
	if (status) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 12;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * 1000) >> 12;
	}
	fuelgauge->suspend_current = curr;

	pr_info("%s: status=%d: current=%d: compliment=0x%x\n",
		__func__, status, curr, compliment);

	return 0;
}
#endif

static int s2mpw02_get_ocv(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 rocv = 0;
	u16 compliment;

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_ROCV, data) < 0) {
		mutex_unlock(&fuelgauge->fg_lock);
		return -EINVAL;
	}

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	rocv = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_dbg(&fuelgauge->i2c->dev, "%s: rocv (%d, 0x%4x)\n", __func__, rocv, compliment);
	fuelgauge->log.rocv = rocv;

	return rocv;
}

static int s2mpw02_get_vbat(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2], val;
	u32 vbat = 0;
	int ret;
	u16 compliment;

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_RVBAT, data) < 0) {
		mutex_unlock(&fuelgauge->fg_lock);
		return -EINVAL;
	}

	mutex_unlock(&fuelgauge->fg_lock);

	dev_dbg(&fuelgauge->i2c->dev, "%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
	vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;
	compliment = (data[1] << 8) | (data[0]);

	dev_dbg(&fuelgauge->i2c->dev, "%s: vbat (%d, 0x%4x)\n", __func__, vbat, compliment);
	fuelgauge->log.vbat = vbat;

	if (vbat >= 4400) {
		s2mpw02_get_ocv(fuelgauge);
		ret = s2mpw02_read_reg(fuelgauge->pmic, S2MPW02_PMIC_REG_LDO_CTRL4, &val);
		if (ret < 0) {
			pr_err("%s: LDO_CTRL4 read error\n", __func__);
			return ret;
		}

		dev_info(&fuelgauge->i2c->dev, "%s: LDO_CTRL4(0x4E): (0x%x)\n", __func__, val);
	}

	return vbat;
}

static int s2mpw02_get_avgvbat(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 new_vbat, old_vbat = 0;
	int cnt;

	mutex_lock(&fuelgauge->fg_lock);

	for (cnt = 0; cnt < 5; cnt++) {
		if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_RVBAT, data) < 0) {
			mutex_unlock(&fuelgauge->fg_lock);
			return -EINVAL;
		}

		new_vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

		if (cnt == 0)
			old_vbat = new_vbat;
		else
			old_vbat = new_vbat / 2 + old_vbat / 2;
	}

	dev_dbg(&fuelgauge->i2c->dev, "%s: avgvbat (%d)\n", __func__, old_vbat);
	fuelgauge->log.avgvbat = old_vbat;
	mutex_unlock(&fuelgauge->fg_lock);

	return old_vbat;
}

/* capacity is  0.1% unit */
static void s2mpw02_fg_get_scaled_capacity(
		struct s2mpw02_fuelgauge_data *fuelgauge,
		union power_supply_propval *val)
{
	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		(fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	dev_dbg(fuelgauge->dev, "%s: scaled capacity (%d.%d)\n",
			__func__, val->intval/10, val->intval%10);

	fuelgauge->log.scaled_capacity = val->intval;
}

/* capacity is integer */
static void s2mpw02_fg_get_atomic_capacity(
		struct s2mpw02_fuelgauge_data *fuelgauge,
		union power_supply_propval *val)
{
	union power_supply_propval raw_soc_val;
	raw_soc_val.intval = s2mpw02_get_rawsoc(fuelgauge) / 100;

	if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
		dev_dbg(fuelgauge->dev, "%s: ATOMIC capacity (old %d : new %d)\n",
				__func__, fuelgauge->capacity_old, val->intval);

		fuelgauge->log.capacity_old = fuelgauge->capacity_old;
		fuelgauge->log.capacity = val->intval;

		if (fuelgauge->capacity_old < val->intval)
			val->intval = fuelgauge->capacity_old + 1;
		else if (fuelgauge->capacity_old > val->intval)
			val->intval = fuelgauge->capacity_old - 1;
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		if (!fuelgauge->is_charging &&
				fuelgauge->capacity_old < val->intval) {
			dev_err(fuelgauge->dev, "%s: ABNORMAL capacity (old %d : new %d)\n",
					__func__, fuelgauge->capacity_old, val->intval);
			fuelgauge->log.capacity_old = fuelgauge->capacity_old;
			fuelgauge->log.capacity = val->intval;
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int s2mpw02_fg_check_capacity_max(
		struct s2mpw02_fuelgauge_data *fuelgauge, int capacity_max)
{
	int new_capacity_max = capacity_max;

	if (new_capacity_max < (fuelgauge->pdata->capacity_max -
				fuelgauge->pdata->capacity_max_margin - 10)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max -
			 fuelgauge->pdata->capacity_max_margin);

		dev_info(fuelgauge->dev, "%s: set capacity max(%d --> %d)\n",
				__func__, capacity_max, new_capacity_max);
	} else if (new_capacity_max > (fuelgauge->pdata->capacity_max +
				fuelgauge->pdata->capacity_max_margin)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin);

		dev_info(fuelgauge->dev, "%s: set capacity max(%d --> %d)\n",
				__func__, capacity_max, new_capacity_max);
	}

	return new_capacity_max;
}

static int s2mpw02_fg_calculate_dynamic_scale(
		struct s2mpw02_fuelgauge_data *fuelgauge, int capacity)
{
	union power_supply_propval raw_soc_val;
	raw_soc_val.intval = s2mpw02_get_rawsoc(fuelgauge) / 10;

	dev_info(fuelgauge->dev, "%s: raw_soc_val.intval(%d), capacity(%d)\n",
			__func__, raw_soc_val.intval, capacity);
	dev_info(fuelgauge->dev, "capacity_max(%d), capacity_max_margin(%d), fuelgauge->capacity_max (%d)\n",
			fuelgauge->pdata->capacity_max, fuelgauge->pdata->capacity_max_margin, fuelgauge->capacity_max);

	if (raw_soc_val.intval <
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin) {
		fuelgauge->capacity_max =
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin;
		dev_dbg(fuelgauge->dev, "%s: capacity_max (%d)",
				__func__, fuelgauge->capacity_max);
	} else {
		fuelgauge->capacity_max =
			(raw_soc_val.intval >
			 fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin) ?
			(fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin) :
			raw_soc_val.intval;
		dev_dbg(fuelgauge->dev, "%s: raw soc (%d)",
				__func__, fuelgauge->capacity_max);
	}

	if (capacity != 100) {
		fuelgauge->capacity_max = s2mpw02_fg_check_capacity_max(
			fuelgauge, (fuelgauge->capacity_max * 100 / (capacity + 1)));
	} else  {
		fuelgauge->capacity_max =
			(fuelgauge->capacity_max * 99 / 100);
	}

	/* update capacity_old for sec_fg_get_atomic_capacity algorithm */
	fuelgauge->capacity_old = capacity;

	if (fuelgauge->capacity_max >= 1000)
		fuelgauge->capacity_max = 1000;

	dev_info(fuelgauge->dev, "%s: %d is used for capacity_max\n",
			__func__, fuelgauge->capacity_max);

	return fuelgauge->capacity_max;
}

bool s2mpw02_fuelgauge_fuelalert_init(struct s2mpw02_fuelgauge_data *fuelgauge, int soc)
{
	u8 data[2];

	/* 1. Set s2mpw02 alert configuration. */
	s2mpw02_alert_init(fuelgauge);

	if (s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_INT, data) < 0)
		return -1;

	/*Enable VBAT, SOC */
	data[1] &= ~(VBAT_L_IM_MASK | SOC_L_IM_MASK);

	/*Disable IDLE_ST, INIT_ST */
	data[1] |= (IDLE_ST_IM_MASK | INIT_ST_IM_MASK);

	s2mpw02_fg_write_reg(fuelgauge->i2c, S2MPW02_FG_REG_INT, data);

	dev_dbg(&fuelgauge->i2c->dev, "%s: irq_reg(%02x%02x) irq(%d)\n",
			__func__, data[1], data[0], fuelgauge->pdata->fg_irq);

	return true;
}

bool s2mpw02_fuelgauge_is_fuelalerted(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	return s2mpw02_check_status(fuelgauge->i2c);
}

bool s2mpw02_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct s2mpw02_fuelgauge_data *fuelgauge = irq_data;
	int ret;

	ret = s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_INT, 0x00);
	if (ret < 0)
		dev_err(&fuelgauge->i2c->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

bool s2mpw02_hal_fg_full_charged(struct i2c_client *client)
{
	return true;
}

static int s2mpw02_fg_update_bat_param(struct s2mpw02_fuelgauge_data *fuelgauge, int step)
{
	int i, ret = 0;
	u8 param0, param1, param2;
	u8 batcap0, batcap1;
	u8 por_state = 0;

	mutex_lock(&fuelgauge->fg_lock);

	if (!fuelgauge->fg_num_age_step)
		goto exit_fg_update_bat_param;

	if (step >= 1)
		step -= 1;

	fuelgauge->fg_age_step = step;

	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_RBATCAP, &batcap0);
	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_RBATCAP + 1, &batcap1);

	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_PARAM1, &param0);
	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_PARAM1 + 1, &param1);
	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_PARAM1 + 2, &param2);

	if ((param0 != fuelgauge->pdata->age_data_info[step].model_param1[0]) ||
		(param1 != fuelgauge->pdata->age_data_info[step].model_param1[1]) ||
		(param2 != fuelgauge->pdata->age_data_info[step].model_param1[2]) ||
		(batcap0 != fuelgauge->pdata->age_data_info[step].batcap[0]) ||
		(batcap1 != fuelgauge->pdata->age_data_info[step].batcap[1])) {

		/*Set 0x17[4] to restore batt param at the bootloader when abnormal power off*/
		s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_CONFIG2, &por_state);
		por_state |= 0x10;
		s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_CONFIG2, por_state);

		for (i = 0; i < 2; i++) {
			s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_RBATCAP + i,
				fuelgauge->pdata->age_data_info[step].batcap[i]);
		}

		for (i = 0; i < S2MPW02_FG_PARAM1_NUM; i++) {
			s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_PARAM1 + i,
				fuelgauge->pdata->age_data_info[step].model_param1[i]);
		}

		for (i = 0; i < S2MPW02_FG_PARAM2_NUM; i++) {
			s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_PARAM2 + i,
				fuelgauge->pdata->age_data_info[step].model_param2[i]);
		}

		/* Successfully finished, clear 0x17[4] */
		s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_CONFIG2, &por_state);
		por_state &= ~(0x10);
		s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_CONFIG2, por_state);

		ret = 1;
	}

exit_fg_update_bat_param:
	mutex_unlock(&fuelgauge->fg_lock);
	return ret;
}

static int s2mpw02_fuelgauge_get_age_step(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 data[2] = {0,};
	int i;

	s2mpw02_fg_read_reg(fuelgauge->i2c, S2MPW02_FG_REG_RBATCAP, data);

	for (i = 0; i < fuelgauge->fg_num_age_step; i++) {
		if ((fuelgauge->pdata->age_data_info[i].batcap[0] == data[0]) &&
			(fuelgauge->pdata->age_data_info[i].batcap[1] == data[1])) {
			return i;
		}
	}

	return -1;
}

static int s2mpw02_fg_get_scaled_capacity_max(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	int ret;
	u8 val;

	ret = s2mpw02_read_reg(fuelgauge->rtc, S2MP_RTC_REG_CTRL, &val);
	if (ret < 0) {
		pr_err("%s: S2MP_RTC_REG_CTRL read error\n", __func__);
		return -EIO;
	}

	val = (val & FG_DYNAMIC_SCALE_MASK) >> FG_DYNAMIC_SCALE_SHIFT;

	fuelgauge->capacity_max = fuelgauge->capacity_max - (val * 10);

	pr_info("%s fuelgauge->capacity_max = %d\n", __func__, fuelgauge->capacity_max);

	if (fuelgauge->capacity_max > 1000)
		fuelgauge->capacity_max = 1000;

	return 0;
}

static int s2mpw02_fg_set_scaled_capacity_max(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	u8 val;
	int scale_factor = 0;
	int ret = 0;

	scale_factor = (fuelgauge->pdata->capacity_max - fuelgauge->capacity_max) / 10;
	scale_factor &= (FG_DYNAMIC_SCALE_MASK >>  FG_DYNAMIC_SCALE_SHIFT);

	pr_info("%s fuelgauge->capacity_max = %d(%d) scale_factor = %d\n",
		__func__, fuelgauge->capacity_max, fuelgauge->pdata->capacity_max, scale_factor);

	ret = s2mpw02_update_reg(fuelgauge->rtc, S2MP_RTC_REG_CTRL,
		scale_factor << FG_DYNAMIC_SCALE_SHIFT,	FG_DYNAMIC_SCALE_MASK);

	if (ret < 0) {
		pr_err("%s: fail to update scale factor(%d)\n", __func__, ret);
		return ret;
	}

	s2mpw02_read_reg(fuelgauge->rtc, S2MP_RTC_REG_CTRL, &val);
	pr_info("%s S2MP_RTC_REG_CTRL = %x\n", __func__, val);

	return ret;
}

static int s2mpw02_fuelgauge_monitor_work(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	struct s2mpw02_fuelgauge_log_data *log = &fuelgauge->log;
	int age_step;

	age_step = s2mpw02_fuelgauge_get_age_step(fuelgauge);

	pr_info("%s vbat(%dmV) avgvbat(%dmV) rocv(%dmV) current(%dmA) "
		"capacity(new:%d old:%d raw:%d scaled:%d.%d) age_step(%d) cap_max(%d)\n", __func__,
		log->vbat, log->avgvbat, log->rocv, log->curr, log->capacity,
		log->capacity_old, log->raw_soc,
		log->scaled_capacity / 10, log->scaled_capacity % 10,
		age_step, fuelgauge->capacity_max);

	s2mpw02_fg_test_read(fuelgauge->i2c);
	return 0;
}

static int s2mpw02_fg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mpw02_fuelgauge_data *fuelgauge =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = psp;

	pr_debug("%s %d psp=%d\n", __func__, __LINE__ ,psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		return -ENODATA;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = s2mpw02_get_vbat(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTERY_VOLTAGE_AVERAGE:
			val->intval = s2mpw02_get_avgvbat(fuelgauge);
			break;
		case SEC_BATTERY_VOLTAGE_OCV:
			val->intval = s2mpw02_get_ocv(fuelgauge);
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = s2mpw02_get_current(fuelgauge);
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = s2mpw02_get_rawsoc(fuelgauge);
		} else {
			val->intval = s2mpw02_get_soc(fuelgauge);

			if (fuelgauge->pdata->capacity_calculation_type &
				(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
					SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
				s2mpw02_fg_get_scaled_capacity(fuelgauge, val);

			/* capacity should be between 0% and 100%
			 * (0.1% degree)
			 */
			if (val->intval > 1000)
				val->intval = 1000;
			if (val->intval < 0)
				val->intval = 0;

			/* get only integer part */
			val->intval /= 10;

			/* check whether doing the wake_unlock */
			if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
					fuelgauge->is_fuel_alerted) {
				//wake_unlock(&fuelgauge->fuel_alert_wake_lock);
				s2mpw02_fuelgauge_fuelalert_init(fuelgauge,
						fuelgauge->pdata->fuel_alert_soc);
			}

			/* (Only for atomic capacity)
			 * In initial time, capacity_old is 0.
			 * and in resume from sleep,
			 * capacity_old is too different from actual soc.
			 * should update capacity_old
			 * by val->intval in booting or resume.
			 */
			if (fuelgauge->initial_update_of_soc) {
				/* updated old capacity */
				fuelgauge->capacity_old = val->intval;
				fuelgauge->initial_update_of_soc = false;
				break;
			}

			if (fuelgauge->pdata->capacity_calculation_type &
				(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC | SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL))
				s2mpw02_fg_get_atomic_capacity(fuelgauge, val);
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = s2mpw02_get_temperature(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = 0;
		break;
#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
	case POWER_SUPPLY_PROP_CURRENT_SUSPEND:
		val->intval = s2mpw02_get_current_suspend(fuelgauge, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
			case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
				val->intval = 0;
				s2mpw02_fuelgauge_monitor_work(fuelgauge);
				break;
			case POWER_SUPPLY_EXT_PROP_LONG_LIFE_STEP:
				val->intval = s2mpw02_fuelgauge_get_age_step(fuelgauge);
				break;
			default:
				break;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mpw02_fg_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct s2mpw02_fuelgauge_data *fuelgauge =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = psp;

	dev_dbg(&fuelgauge->i2c->dev, "%s:psp[%d]\n", __func__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		dev_info(&fuelgauge->i2c->dev, "capacity_calculation_type[%d]\n",
			fuelgauge->pdata->capacity_calculation_type);
		if (fuelgauge->pdata->capacity_calculation_type &
				SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
			s2mpw02_fg_calculate_dynamic_scale(fuelgauge, 100);
			s2mpw02_fg_set_scaled_capacity_max(fuelgauge);
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		switch (val->intval) {
		case SEC_BAT_CHG_MODE_BUCK_OFF:
		case SEC_BAT_CHG_MODE_CHARGING_OFF:
			fuelgauge->is_charging = false;
			break;
		case SEC_BAT_CHG_MODE_CHARGING:
			fuelgauge->is_charging = true;
			break;
		};
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			s2mpw02_restart_gauging(fuelgauge);
			fuelgauge->initial_update_of_soc = true;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		s2mpw02_set_temperature(fuelgauge, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		dev_info(&fuelgauge->i2c->dev,
			"%s: capacity_max changed, %d -> %d\n",
			__func__, fuelgauge->capacity_max, val->intval);
		fuelgauge->capacity_max = s2mpw02_fg_check_capacity_max(fuelgauge, val->intval);
		fuelgauge->initial_update_of_soc = true;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		/* s2mpw02_fg_reset_capacity_by_jig_connection(fuelgauge->i2c); */
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		dev_info(&fuelgauge->i2c->dev,
			"%s: POWER_SUPPLY_PROP_CALIBRATE\n", __func__);
		s2mpw02_restart_gauging(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
			case POWER_SUPPLY_EXT_PROP_UPDATE_BATTERY_DATA:
				s2mpw02_fg_update_bat_param(fuelgauge, val->intval);
				break;
			default:
				break;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static void s2mpw02_fg_isr_work(struct work_struct *work)
{
	struct s2mpw02_fuelgauge_data *fuelgauge =
		container_of(work, struct s2mpw02_fuelgauge_data, isr_work.work);
	u8 fg_alert_status = 0;

	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_STATUS2, &fg_alert_status);
	dev_info(&fuelgauge->i2c->dev, "%s : fg_alert_status(0x%x)\n",
		__func__, fg_alert_status);

	fg_alert_status &= (VBAT_L_MASK | SOC_L_MASK);
	if (fg_alert_status & VBAT_L_MASK)
		pr_info("%s : Battery Voltage is very Low!\n", __func__);

	if (fg_alert_status & SOC_L_MASK)
		pr_info("%s : Battery Level is Very Low!\n", __func__);

	if (!fg_alert_status)
		pr_info("%s : SOC and VBAT are OK!\n", __func__);
	//wake_unlock(&fuelgauge->fuel_alert_wake_lock);
}

static irqreturn_t s2mpw02_fg_irq_thread(int irq, void *irq_data)
{
	struct s2mpw02_fuelgauge_data *fuelgauge = irq_data;
	u8 fg_irq = 0;

	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_INT, &fg_irq);
	dev_info(&fuelgauge->i2c->dev, "%s: fg_irq(0x%x)\n",
		__func__, fg_irq);
	//wake_lock(&fuelgauge->fuel_alert_wake_lock);
	schedule_delayed_work(&fuelgauge->isr_work, 0);

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static int s2mpw02_fuelgauge_parse_dt(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mpw02-fuelgauge");
	int ret;
	const u32 *p;
	int len, i;

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		fuelgauge->pdata->fg_irq = of_get_named_gpio(np, "fuelgauge,fuel_int", 0);
		if (fuelgauge->pdata->fg_irq < 0)
			pr_err("%s error reading fg_irq = %d\n",
				__func__, fuelgauge->pdata->fg_irq);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&fuelgauge->pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&fuelgauge->pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&fuelgauge->pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&fuelgauge->pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&fuelgauge->pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);
		fuelgauge->pdata->repeated_fuelalert = of_property_read_bool(np,
				"fuelgauge,repeated_fuelalert");

		/* get battery_params node */
		p = of_get_property(np, "fuelgauge,battery_data", &len);
		if (!p)
			pr_info("%s battery_params node NULL\n", __func__);
		else {
			fuelgauge->fg_num_age_step = len / sizeof(struct fg_age_data_info);
			fuelgauge->pdata->age_data_info = kzalloc(len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "fuelgauge,battery_data",
					(int *)fuelgauge->pdata->age_data_info, len/sizeof(int));
			pr_info("%s: fg_num_age_step %d len = %d\n",
				__func__, fuelgauge->fg_num_age_step, len);

			for (i = 0; i < fuelgauge->fg_num_age_step; i++)
				pr_info("%s: age_step = %d batcap[0] = %02x model_param1[0] = %02x"
				" model_param2[0] = %02x\n",
					__func__, i, fuelgauge->pdata->age_data_info[i].batcap[0],
					fuelgauge->pdata->age_data_info[i].model_param1[0],
					fuelgauge->pdata->age_data_info[i].model_param2[0]);
		}

		np = of_find_node_by_name(NULL, "battery");
		if (!np) {
			pr_err("%s np NULL\n", __func__);
		} else {
			ret = of_property_read_string(np,
				"battery,fuelgauge_name",
				(char const **)&fuelgauge->pdata->fuelgauge_name);
		}
	}

	return 0;
}

static struct of_device_id s2mpw02_fuelgauge_match_table[] = {
	{ .compatible = "samsung,s2mpw02-fuelgauge",},
	{},
};
#else
static int s2mpw02_fuelgauge_parse_dt(struct s2mpw02_fuelgauge_data *fuelgauge)
{
	return -ENOSYS;
}

#define s2mpw02_fuelgauge_match_table NULL
#endif /* CONFIG_OF */

static int s2mpw02_fg_src_switch_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	pr_debug("%s psp=%d\n", __func__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		val->intval = 0;
		break;
	default:
		break;
	}
	return 0;
}

static int s2mpw02_fg_src_switch_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct s2mpw02_fuelgauge_data *fuelgauge =
		power_supply_get_drvdata(psy);

	dev_dbg(&fuelgauge->i2c->dev, "%s:psp[%d]\n", __func__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		if (val->intval == 1) {
			pr_info("%s fg sensing switch to Vsys\n", __func__);
			s2mpw02_update_reg(fuelgauge->i2c,
				S2MPW02_FG_OTP_6, FG_SAMPLE_TIME_4S, FG_SAMPLE_MASK);

			s2mpw02_update_reg(fuelgauge->pmic, S2MPW02_PMIC_REG_LDO_CTRL4,
				S2MPW02_FG_SENSING_VSYS, S2MPW02_FG_SENSING_MASK);

			s2mpw02_restart_gauging(fuelgauge);
		} else if (val->intval == 0) {
			pr_info("%s fg sensing switch to Vbat\n", __func__);
			s2mpw02_update_reg(fuelgauge->pmic, S2MPW02_PMIC_REG_LDO_CTRL4,
				S2MPW02_FG_SENSING_VBATFG, S2MPW02_FG_SENSING_MASK);

			s2mpw02_update_reg(fuelgauge->i2c, S2MPW02_FG_OTP_6,
				FG_SAMPLE_TIME_1S, FG_SAMPLE_MASK);

			s2mpw02_restart_gauging(fuelgauge);
		} else {
			pr_err("%s() invalid param\n", __func__);
		}
		break;
	default:
		break;
	}
	return 0;
}

static enum power_supply_property s2mpw02_fg_src_switch_ps_props[] = {
	POWER_SUPPLY_PROP_ENERGY_NOW,
};

static const struct power_supply_desc fg_src_power_supply_desc = {
	.name = "fg_src_switch",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = s2mpw02_fg_src_switch_ps_props,
	.num_properties = ARRAY_SIZE(s2mpw02_fg_src_switch_ps_props),
	.get_property = s2mpw02_fg_src_switch_get_property,
	.set_property = s2mpw02_fg_src_switch_set_property,
};

static int s2mpw02_fuelgauge_probe(struct platform_device *pdev)
{
	struct s2mpw02_dev *s2mpw02 = dev_get_drvdata(pdev->dev.parent);
	struct s2mpw02_fuelgauge_data *fuelgauge;
	struct power_supply_config psy_cfg = {};
	union power_supply_propval raw_soc_val;
	int ret = 0;
	u8 temp;

	pr_info("%s: S2MPW02 Fuelgauge Driver Loading\n", __func__);

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->dev = &pdev->dev;
	fuelgauge->i2c = s2mpw02->fuelgauge;
	fuelgauge->pmic = s2mpw02->pmic;
	fuelgauge->rtc = s2mpw02->rtc;


	/* fuelgauge i2c write enable */
	// will be moved to bootloader
	s2mpw02_fg_read_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_INTM, &temp);
	temp |= FG_IF_EN_MASK;
	s2mpw02_fg_write_reg_byte(fuelgauge->i2c, S2MPW02_FG_REG_INTM, temp);

	fuelgauge->pdata = devm_kzalloc(&pdev->dev, sizeof(*(fuelgauge->pdata)),
			GFP_KERNEL);
	if (!fuelgauge->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mpw02_fuelgauge_parse_dt(fuelgauge);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, fuelgauge);

	if (fuelgauge->pdata->fuelgauge_name == NULL)
		fuelgauge->pdata->fuelgauge_name = "sec-fuelgauge";

	fuelgauge->psy_fg_desc.name          = fuelgauge->pdata->fuelgauge_name;
	fuelgauge->psy_fg_desc.type          = POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg_desc.get_property  = s2mpw02_fg_get_property;
	fuelgauge->psy_fg_desc.set_property  = s2mpw02_fg_set_property;
	fuelgauge->psy_fg_desc.properties    = s2mpw02_fuelgauge_props;
	fuelgauge->psy_fg_desc.num_properties =
			ARRAY_SIZE(s2mpw02_fuelgauge_props);

	/* temperature level init */
	fuelgauge->before_temp_level = TEMP_LEVEL_MID;

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	raw_soc_val.intval = s2mpw02_get_rawsoc(fuelgauge);
	raw_soc_val.intval = raw_soc_val.intval / 10;

	if (raw_soc_val.intval > fuelgauge->capacity_max)
		s2mpw02_fg_calculate_dynamic_scale(fuelgauge, 100);

	s2mpw02_fg_get_scaled_capacity_max(fuelgauge);

	psy_cfg.drv_data = fuelgauge;
	fuelgauge->psy_fg = power_supply_register(&pdev->dev, &fuelgauge->psy_fg_desc, &psy_cfg);
	if (IS_ERR(fuelgauge->psy_fg)) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		ret = PTR_ERR(fuelgauge->psy_fg);
		goto err_data_free;
	}

	fuelgauge->psy_fg_src_switch =
		power_supply_register(&pdev->dev, &fg_src_power_supply_desc, &psy_cfg);
	if (IS_ERR(fuelgauge->psy_fg_src_switch)) {
		pr_err("%s: Failed to Register psy_fg_src_switch\n", __func__);
		power_supply_unregister(fuelgauge->psy_fg);
		ret = PTR_ERR(fuelgauge->psy_fg_src_switch);
		goto err_data_free;
	}

	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		s2mpw02_fuelgauge_fuelalert_init(fuelgauge,
					fuelgauge->pdata->fuel_alert_soc);
		//wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
		//			WAKE_LOCK_SUSPEND, "fuel_alerted");
		if (fuelgauge->pdata->fg_irq > 0) {
			INIT_DELAYED_WORK(&fuelgauge->isr_work, s2mpw02_fg_isr_work);

			fuelgauge->fg_irq = gpio_to_irq(fuelgauge->pdata->fg_irq);
			dev_info(&pdev->dev,
				"%s : fg_irq = %d\n", __func__, fuelgauge->fg_irq);
			if (fuelgauge->fg_irq > 0) {
				ret = request_threaded_irq(fuelgauge->fg_irq,
					NULL, s2mpw02_fg_irq_thread,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"fuelgauge-irq", fuelgauge);
				if (ret < 0) {
					dev_err(&pdev->dev,
						"%s: Failed to Request IRQ\n", __func__);
					goto err_supply_unreg;
				}
				ret = enable_irq_wake(fuelgauge->fg_irq);
				if (ret < 0)
					dev_err(&pdev->dev,
					"%s: Failed to Enable Wakeup Source(%d)\n",
					__func__, ret);
			} else {
				dev_err(&pdev->dev, "%s: Failed gpio_to_irq(%d)\n",
						__func__, fuelgauge->fg_irq);
				goto err_supply_unreg;
			}
		}
	}

	fuelgauge->initial_update_of_soc = true;

	pr_info("%s: S2MPW02 Fuelgauge Driver Loaded\n", __func__);
	s2mpw02_fg_test_read(fuelgauge->i2c);
	return 0;

err_supply_unreg:
	power_supply_unregister(fuelgauge->psy_fg);
	power_supply_unregister(fuelgauge->psy_fg_src_switch);
err_data_free:
	if (pdev->dev.of_node)
		kfree(fuelgauge->pdata);

err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(fuelgauge);

	return ret;
}

static const struct i2c_device_id s2mpw02_fuelgauge_id[] = {
	{"s2mpw02-fuelgauge", 0},
	{}
};

static void s2mpw02_fuelgauge_shutdown(struct platform_device *pdev)
{
}

static int s2mpw02_fuelgauge_remove(struct platform_device *pdev)
{
/*
	struct s2mpw02_fuelgauge_data *fuelgauge = platform_get_drvdata(pdev);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);
*/
	return 0;
}

#if defined CONFIG_PM
static int s2mpw02_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int s2mpw02_fuelgauge_resume(struct device *dev)
{
#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
	struct s2mpw02_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

	s2mpw02_calc_current_suspend(fuelgauge);
#endif
	return 0;
}
#else
#define s2mpw02_fuelgauge_suspend NULL
#define s2mpw02_fuelgauge_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(s2mpw02_fuelgauge_pm_ops, s2mpw02_fuelgauge_suspend,
		s2mpw02_fuelgauge_resume);

static struct platform_driver s2mpw02_fuelgauge_driver = {
	.driver = {
		.name = "s2mpw02-fuelgauge",
		.owner = THIS_MODULE,
		.pm = &s2mpw02_fuelgauge_pm_ops,
		.of_match_table = s2mpw02_fuelgauge_match_table,
	},
	.probe  = s2mpw02_fuelgauge_probe,
	.remove = s2mpw02_fuelgauge_remove,
	.shutdown   = s2mpw02_fuelgauge_shutdown,
/*	.id_table   = s2mpw02_fuelgauge_id,	*/
};

static int __init s2mpw02_fuelgauge_init(void)
{
	int ret = 0;
	pr_info("%s: S2MPW02 Fuelgauge Init\n", __func__);
	ret = platform_driver_register(&s2mpw02_fuelgauge_driver);

	return ret;
}

static void __exit s2mpw02_fuelgauge_exit(void)
{
	platform_driver_unregister(&s2mpw02_fuelgauge_driver);
}
device_initcall(s2mpw02_fuelgauge_init);
module_exit(s2mpw02_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung S2MPW02 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
