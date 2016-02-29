/*
 *  bcm5935x_wpc_low.c
 *  Samsung BCM5935X WPC low(under reg100) Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
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

#include <linux/battery/bcm5935x.h>
#include <linux/battery/bcm5935x_adc.h>

extern struct class *power_supply_class;

struct device *wpc_device;

extern unsigned int system_rev;

static enum power_supply_property sec_wpc_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TYPE,
};

static int bcm5935x_set_ioctrl(struct bcm5935x_wpc_info *wpc, int val)
{
	int ret, data;

	ret = bcm5935x_read_reg(wpc->i2c,
		BCM5935X_IOCTRL);
	data = ret & BCM5935X_IOCTRL_M;
	data |= val;

	ret = bcm5935x_write_reg(wpc->i2c, BCM5935X_IOCTRL, data);

	if (ret < 0)
		pr_err("%s: err %d\n", __func__, ret);

	return ret;
}

static int bcm5935x_wpc_get_version(struct bcm5935x_wpc_info *wpc)
{
	int val, ret;

	val = bcm5935x_read_reg(wpc->i2c, BCM5935X_PMUID);

	pr_info("%s: PMUID[%d]\n", __func__, val);

	if (val == 0x50) ret = 1;
	else ret = 0;

	return ret;
}

static void bcm5935x_vout_change_low(struct bcm5935x_wpc_info *wpc)
{
	bcm5935x_write_reg(wpc->i2c, BCM5935X_BSRSEL, 0x0F);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_BSRCTRL8, 0x1E);

	pr_info("%s\n", __func__);
}

static void bcm5935x_vout_change_high(struct bcm5935x_wpc_info *wpc)
{

	bcm5935x_write_reg(wpc->i2c, BCM5935X_BSRSEL, 0x17);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_BSRCTRL8, 0x1E);

	pr_info("%s\n", __func__);
}

static void bcm5935x_vout_change_very_high(struct bcm5935x_wpc_info *wpc)
{

	bcm5935x_write_reg(wpc->i2c, BCM5935X_BSRSEL, 0x37);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_BSRCTRL8, 0x0E);

	pr_info("%s\n", __func__);
}

extern struct bcm5935x_wpc_info *g_wpc_high;

static int bcm5935x_read_fod_data(struct bcm5935x_wpc_info *wpc)
{
	uint8 i;
	/* this function is sample code to show how to use bcm59350_adc.c functions */
	/* Computed waiting time to start the power sample after the interrupt. */
	uint16  wait_time_for_sample;
	uint8  max_power_samples;
	uint32 val_vrect, val_vbuck, val_tdie, val_ibuck, val_irect;
	uint8 val_power;

	/* register 0xb4, b5, b6, b7, 141, 143, 144, 145, 146, 147 are used for adc reading */
	uint8 val_0xb4, val_0xb5, val_0xb6, val_0xb7;
	uint8 val_0x141, val_0x143, val_0x144, val_0x145, val_0x146, val_0x147;
	/* register 0x89, 8b, 8d, 0a, 11, 0c, 12, 0f, 13, 0d are used for Vrect, Vbuck, Ibuck reading */
	uint8 val_0x89, val_0x8b, val_0x8d;
	uint8 val_0x0a, val_0x11, val_0x0c, val_0x12, val_0x0f, val_0x13, val_0x0d;
	/* register 0x11a is used for reading max power */
	uint8 val_0x11a;
	uint8 val_0x11b;
	uint32 sum_received_power = 0;

	union power_supply_propval val;

	psy_do_property("sec-wpc2", get,
		POWER_SUPPLY_PROP_CURRENT_NOW, val);
	/* reading following regsters via i2c. */
	val_0xb4 = bcm5935x_read_reg(wpc->i2c, 0xb4);
	val_0xb5 = bcm5935x_read_reg(wpc->i2c, 0xb5);
	val_0xb6 = bcm5935x_read_reg(wpc->i2c, 0xb6);
	val_0xb7 = bcm5935x_read_reg(wpc->i2c, 0xb7);
	val_0x141 = g_wpc_high->fod_reg[0];
	val_0x143 = g_wpc_high->fod_reg[1];
	val_0x144 = g_wpc_high->fod_reg[2];
	val_0x145 = g_wpc_high->fod_reg[3];
	val_0x146 = g_wpc_high->fod_reg[4];
	val_0x147 = g_wpc_high->fod_reg[5];
	val_0x11a = g_wpc_high->fod_reg[6];
	val_0x11b = g_wpc_high->fod_reg[7];

	if ((val_0xb4 < 0) || (val_0xb5 < 0) ||
		(val_0xb6 < 0) || (val_0xb7 < 0))
		return -ENXIO;

	bcm5935x_write_reg(wpc->i2c, BCM5935X_ADCCTRL1, 0xBF);
	/* reading following regsters via i2c. */
	val_0x89 = bcm5935x_read_reg(wpc->i2c, 0x89);
	val_0x8b = bcm5935x_read_reg(wpc->i2c, 0x8b);
	val_0x8d = bcm5935x_read_reg(wpc->i2c, 0x8d);
	val_0x0a = bcm5935x_read_reg(wpc->i2c, 0x0a);
	val_0x11 = bcm5935x_read_reg(wpc->i2c, 0x11);
	val_0x0c = bcm5935x_read_reg(wpc->i2c, 0x0c);
	val_0x12 = bcm5935x_read_reg(wpc->i2c, 0x12);
	val_0x0f = bcm5935x_read_reg(wpc->i2c, 0x0f);
	val_0x13 = bcm5935x_read_reg(wpc->i2c, 0x13);
	val_0x0d = bcm5935x_read_reg(wpc->i2c, 0x0d);
	val_0x12 = bcm5935x_read_reg(wpc->i2c, 0x12);

	if ((val_0x89 < 0) || (val_0x8b < 0) ||
		(val_0x8d < 0) || (val_0x0a < 0) ||
		(val_0x11 < 0) || (val_0x0c < 0) ||
		(val_0x12 < 0) || (val_0x0f < 0) ||
		(val_0x13 < 0) || (val_0x0d < 0) ||
		(val_0x12 < 0))
		return -ENXIO;

	bcm59350_read_adc_init(val_0xb4, val_0xb5, val_0xb6,
		val_0xb7, val_0x141, val_0x143, val_0x144, val_0x145,
		val_0x146, val_0x147);
	/* caculate Vrect */
	val_vrect = get_bcm59350_vrect(val_0x8b, val_0x8d, val_0x0a, val_0x11);
	/* caculate Vbuck */
	val_vbuck = get_bcm59350_vbuck(val_0x89, val_0x8d, val_0x0c, val_0x12);
	/* caculate Die Temperature */
	val_tdie = get_bcm59350_tdie(val_0x0f, val_0x13);
	/* caculate Ibuck */
	val_ibuck = get_bcm59350_ibuck(val_vrect, val_tdie, val_0x0d, val_0x12);
	/* caculate Irect */
	val_irect = get_bcm59350_irect(val_vrect, val_tdie, val_vbuck, val_ibuck);

	bcm59350_power_packet_init(val_0x11a, val_0x11b, &max_power_samples,
		&wait_time_for_sample);
	/* Calculate Received power */
	for (i=0; i<max_power_samples; i++)	{
		sum_received_power += get_bcm59350_receivedPower (val_vrect, val_irect);
	}
	/* get power value to write BRCM_A4WP_REG_PMU_WPT_WPC_RP_VALUE (0x112) register */
	val_power = get_bcm59350_PowerRect_value (sum_received_power, max_power_samples);

	/* Write val_power value to BRCM_A4WP_REG_PMU_WPT_WPC_RP_VALUE (0x112) register here */
	val.intval = val_power;
	psy_do_property("sec-wpc2", set,
		POWER_SUPPLY_PROP_CURRENT_NOW, val);

	pr_info("Vrect = %u, Vbuck = %u Irect = %u, Ibuck = %u ",
		val_vrect, val_vbuck, val_irect, val_ibuck);
	pr_info("Power = %u, Tdie = %u wait_time = %u ms max_power = %u\n",
		val_power, val_tdie, wait_time_for_sample, max_power_samples);

	return wait_time_for_sample;
}

static void bcm5935x_fod_work(struct work_struct *work)
{
	struct bcm5935x_wpc_info *wpc =
		container_of(work, struct bcm5935x_wpc_info,
		fod_work.work);
	int ret;

	wake_lock(&wpc->fod_wake_lock);
	wpc->fod_cnt--;

	ret = bcm5935x_read_fod_data(wpc);

	wake_unlock(&wpc->fod_wake_lock);

	if (ret < 0) {
		wpc->fod_cnt = 15;
		pr_err("%s: wpc no ack.\n", __func__);
	} else {
		pr_info("%s:fod_cnt[%d], wait_time_for_sample[%d]\n",
			__func__, wpc->fod_cnt, ret);
		if (wpc->online == POWER_SUPPLY_TYPE_WPC) {
			if (wpc->fod_cnt > 0)
				queue_delayed_work(wpc->fod_wqueue,
				&wpc->fod_work, ret);
			else if (wpc->fod_cnt == 0) {
				wpc->fod_status = FOD_DONE;
				if (system_rev >= 0x0C)
					bcm5935x_wpc_send_EPT_packet(g_wpc_high);
			}
		}
	}
}

static int sec_wpc_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct bcm5935x_wpc_info *wpc =
		container_of(psy, struct bcm5935x_wpc_info, psy_wpc);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = wpc->online;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = wpc->ioctrl;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = wpc->voltage_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = wpc->detect_type;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sec_wpc_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct bcm5935x_wpc_info *wpc =
		container_of(psy, struct bcm5935x_wpc_info, psy_wpc);
	union power_supply_propval value;
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		value.intval = POWER_SUPPLY_TYPE_UNKNOWN;

		if (val->intval == POWER_SUPPLY_TYPE_UNKNOWN) {
			ret = bcm5935x_wpc_get_version(wpc);
			if (ret == 1)
				value.intval = POWER_SUPPLY_TYPE_WPC;
		} else if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			cancel_delayed_work(&wpc->fod_work);
			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			g_wpc_high->vout_status = VOUT_UNKNOWN;
			wpc->fod_status = FOD_UNKNOWN;
			wpc->fod_cnt = 15;
		} else if (val->intval == POWER_SUPPLY_TYPE_WPC) {
			value.intval = POWER_SUPPLY_TYPE_WPC;
		}
		wpc->online = value.intval;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		value.intval = val->intval;
		bcm5935x_set_ioctrl(wpc, value.intval);
		wpc->ioctrl = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		wpc->voltage_now = val->intval;
		if (val->intval < 4000)
			bcm5935x_vout_change_low(wpc);
		else if ((val->intval >= 4000) && (val->intval < 4200))
			bcm5935x_vout_change_high(wpc);
		else if (val->intval >= 4200)
			bcm5935x_vout_change_very_high(wpc);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		pr_info("%s:fod_status[%d], fod_cnt[%d]\n",
			__func__, wpc->fod_status, wpc->fod_cnt);

		if ((wpc->fod_status == FOD_UNKNOWN) &&
			(wpc->fod_cnt == 15)) {
			queue_delayed_work(wpc->fod_wqueue, &wpc->fod_work, 0);
		} else if (wpc->fod_status == FOD_DONE)
			bcm5935x_wpc_send_EPT_packet(g_wpc_high);

		break;
	case POWER_SUPPLY_PROP_TYPE:
		wpc->detect_type = val->intval;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bcm5935x_wpc_set_reg(struct bcm5935x_wpc_info *wpc, int reg, int val)
{
	int ret;
	
	ret = bcm5935x_write_reg(wpc->i2c, reg, val);

	return ret;

}

static ssize_t bcm5935x_wpc_store_write_reg(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);

	int ret = 0, reg = 0, val = 0;

	sscanf(buf, "%x %x", &reg, &val);
	if (reg < BCM5935X_INTL || reg > BCM5935X_PDCMPSYN0)
		return -EINVAL;

	ret = bcm5935x_wpc_set_reg(wpc, reg, val);

	if (ret < 0)
		pr_info("%s: write reg fail[%d]", __func__, ret);

	return count;
}

static int bcm5935x_wpc_get_reg(struct bcm5935x_wpc_info *wpc, int reg)
{
	int val;
	
	val = bcm5935x_read_reg(wpc->i2c, reg);

	return val;
}
static ssize_t bcm5935x_wpc_show_read_reg(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);
	
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", wpc->reg);
	return ret;

}


static ssize_t bcm5935x_wpc_store_read_reg(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);

	int reg = 0, val = 0;
	
	sscanf(buf, "%x", &reg);
	if (reg < BCM5935X_INTL || reg > BCM5935X_PDCMPSYN0)
		return -EINVAL;


	val = bcm5935x_wpc_get_reg(wpc, reg);

	wpc->reg = val;

	pr_info("%s: reg[0x%x]:[%d]\n", __func__, reg, val);

	return count;
}

static int bcm5935x_wpc_get_vrect_value(struct bcm5935x_wpc_info *wpc)
{
	int data1, data8, cal_data8, val;
	
	bcm5935x_write_reg(wpc->i2c, BCM5935X_ADCCTRL1, 0xBF);
	data1 = bcm5935x_read_reg(wpc->i2c, BCM5935X_ADCDATA1);
	data8 = bcm5935x_read_reg(wpc->i2c, BCM5935X_ADCDATA8);
	cal_data8 = data8 & 0x0C;
	cal_data8 = cal_data8 >> 2;
	val = data1 * 4 + cal_data8;

	pr_info("BCM5935x WPC val[%d], data1[%d], data8[%d], cal_data8[%d]\n",
		val, data1, data8, cal_data8);

	return val;
}

static int bcm5935x_wpc_get_vbuck_value(struct bcm5935x_wpc_info *wpc)
{
	int data3, data9, cal_data9, val;
	
	bcm5935x_write_reg(wpc->i2c, BCM5935X_ADCCTRL1, 0xBF);
	data3 = bcm5935x_read_reg(wpc->i2c, BCM5935X_ADCDATA3);
	data9 = bcm5935x_read_reg(wpc->i2c, BCM5935X_ADCDATA9);
	cal_data9 = data9 & 0x0C;
	cal_data9 = cal_data9 >> 2;
	val = data3 * 4 + cal_data9;

	pr_info("BCM5935x WPC val[%d], data3[%d], data9[%d], cal_data9[%d]\n",
		val, data3, data9, cal_data9);

	return val;
}

static int bcm5935x_wpc_get_ibuck_value(struct bcm5935x_wpc_info *wpc)
{
	int data4, data9, cal_data9, val;

	/* during 15 test, fod_work make too much load on wpc ic */
	/* It seems to affect to lcd on operation, thus disable it.     */
	cancel_delayed_work(&wpc->fod_work);

	bcm5935x_write_reg(wpc->i2c, BCM5935X_ADCCTRL1, 0xBF);
	data4 = bcm5935x_read_reg(wpc->i2c, BCM5935X_ADCDATA4);
	data9 = bcm5935x_read_reg(wpc->i2c, BCM5935X_ADCDATA9);
	cal_data9 = data9 & 0xC0;
	cal_data9 = cal_data9 >> 6;
	val = data4 * 4 + cal_data9;

	pr_info("%s: val[%d], data4[%d], data9[%d], cal_data9[%d]\n",
		__func__, val, data4, data9, cal_data9);

	return val;
}

static ssize_t bcm5935x_WPC_show_VRECT(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);

	int val;
	int vrect_i,vrect_f1, vrect_f2;
	
	val = bcm5935x_wpc_get_vrect_value(wpc);

	val = 2 * 12 * val;
	vrect_i = val / 1024;
	vrect_f1 = ((val * 10) / 1024) - (vrect_i * 10);
	vrect_f2 = ((val * 100) / 1024) - (vrect_i * 100) - (vrect_f1 * 10);

	pr_info("BCM5935x WPC vrect[%d.%d%d] val[%d]\n",
		vrect_i, vrect_f1, vrect_f2, val);

	return sprintf(buf, "%d.%d%d\n", vrect_i, vrect_f1, vrect_f2);
}

static ssize_t bcm5935x_WPC_show_VBUCK(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);
	
	int val;
	int vbuck_i, vbuck_f1,vbuck_f2;

	val = bcm5935x_wpc_get_vbuck_value(wpc);
	
	val = 11 * 12 * val / 10;
	vbuck_i = val / 1024;
	vbuck_f1 = ((val * 10) / 1024) - (vbuck_i * 10);
	vbuck_f2 = ((val * 100) / 1024) - (vbuck_i * 100) - (vbuck_f1 * 10);

	pr_info("BCM5935x WPC vbuck[%d.%d%d], val[%d]\n",
		vbuck_i, vbuck_f1, vbuck_f2, val);
	
	return sprintf(buf, "%d.%d%d\n", vbuck_i, vbuck_f1, vbuck_f2);
}

static ssize_t bcm5935x_WPC_show_IBUCK(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);
	
	int val;
	int ibuck_i, ibuck_f1,ibuck_f2, ibuck_f3;

	val = bcm5935x_wpc_get_ibuck_value(wpc);
	
	val = 33 * val / 10;
	ibuck_i = val / 1024;
	ibuck_f1 = ((val * 10) / 1024) - (ibuck_i * 10);
	ibuck_f2 = ((val * 100) / 1024) - (ibuck_i * 100) - (ibuck_f1 * 10);
	ibuck_f3 = ((val * 1000) / 1024) - (ibuck_i * 1000) - (ibuck_f1 * 100) - (ibuck_f2 * 10);

	pr_info("BCM5935x WPC ibuck[%d.%d%d%d], val[%d]\n",
		ibuck_i, ibuck_f1, ibuck_f2, ibuck_f3, val);

	val = (ibuck_i * 1000) + (ibuck_f1 * 100) + (ibuck_f2 * 10) + ibuck_f3;
	
	return sprintf(buf, "%d\n", val);
}

static ssize_t bcm5935x_WPC_show_status(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);

	int val;
	
	val = bcm5935x_wpc_get_version(wpc);

	return sprintf(buf, "%d\n", val);
}

static ssize_t bcm5935x_WPC_show_tdie(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);

	int val, reg_0x0f, reg_0x13;
	
	reg_0x0f = bcm5935x_read_reg(wpc->i2c, BCM5935X_ADCDATA6);
	reg_0x13 = bcm5935x_read_reg(wpc->i2c, BCM5935X_ADCDATA10);

	val = get_bcm59350_tdie(reg_0x0f, reg_0x13);

	pr_info("%s:data6[0x%x], data10[0x%x], ret[%d]\n",
		__func__, reg_0x0f, reg_0x13, val);
	
	return sprintf(buf, "%d\n", val);
}

static struct device_attribute dev_attr_read_reg = __ATTR(read_reg, 0664, bcm5935x_wpc_show_read_reg, bcm5935x_wpc_store_read_reg);
static struct device_attribute dev_attr_write_reg = __ATTR(write_reg, 0664, NULL, bcm5935x_wpc_store_write_reg);
static struct device_attribute dev_attr_vrect = __ATTR(vrect, 0664, bcm5935x_WPC_show_VRECT, NULL);
static struct device_attribute dev_attr_vbuck = __ATTR(vbuck, 0664, bcm5935x_WPC_show_VBUCK, NULL);
static struct device_attribute dev_attr_ibuck = __ATTR(ibuck, 0664, bcm5935x_WPC_show_IBUCK, NULL);
static struct device_attribute dev_attr_status = __ATTR(status, 0664, bcm5935x_WPC_show_status, NULL);
static struct device_attribute dev_attr_tdie = __ATTR(tdie, 0664, bcm5935x_WPC_show_tdie, NULL);

static struct attribute *bcm5935x_wpc_low_attributes[] = {
	&dev_attr_read_reg.attr,
	&dev_attr_write_reg.attr,
	&dev_attr_vrect.attr,
	&dev_attr_vbuck.attr,
	&dev_attr_ibuck.attr,
	&dev_attr_status.attr,
	&dev_attr_tdie.attr,
	NULL,
};

static struct attribute_group bcm5935x_wpc_low_attr_group = {
	.attrs = bcm5935x_wpc_low_attributes,
};

static int __devinit bcm5935x_wpc_low_probe(struct platform_device *pdev)
{
	struct bcm5935x_dev *bcm5935x = dev_get_drvdata(pdev->dev.parent);
	//struct max14577_platform_data *mfd_pdata = dev_get_platdata(bcm5935x->dev);
	struct bcm5935x_wpc_info *wpc;
	int ret = 0;

	wpc = kzalloc(sizeof(struct bcm5935x_wpc_info), GFP_KERNEL);
	if (!wpc) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	wpc->dev = &pdev->dev;
	mutex_init(&wpc->wpc_mutex);
	wpc->i2c = bcm5935x->i2c_low;

	platform_set_drvdata(pdev, wpc);

	wpc_device = device_create(power_supply_class, NULL, 0, NULL, "wpc");
	if (IS_ERR(wpc_device)) {
		pr_err("Failed to create wpc device for the sysfs\n");
		goto err_device_create;
	}

	/* create sysfs group */
	ret = sysfs_create_group(&wpc_device->kobj, &bcm5935x_wpc_low_attr_group);
	if (ret) {
		pr_err("%s: failed to create bcm5935x wpc low attribute group\n",
				__func__);
		goto err_create_group;
	}
	dev_set_drvdata(wpc_device, wpc);

	wake_lock_init(&wpc->fod_wake_lock, WAKE_LOCK_SUSPEND,
				   "wpc-fod");

	/* create work queue */
	wpc->fod_wqueue =
		alloc_workqueue(dev_name(&pdev->dev), WQ_UNBOUND |
		WQ_MEM_RECLAIM, 1);
	if (!wpc->fod_wqueue) {
		dev_err(wpc->dev,
			"%s: Fail to Create Workqueue\n", __func__);
		goto err_create_group;
	}

	INIT_DELAYED_WORK(&wpc->fod_work, bcm5935x_fod_work);

	wpc->online = POWER_SUPPLY_TYPE_UNKNOWN;
	wpc->fod_status = FOD_UNKNOWN;
	wpc->fod_cnt = 15;

	wpc->psy_wpc.name		= "sec-wpc";
	wpc->psy_wpc.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	wpc->psy_wpc.get_property	= sec_wpc_get_property;
	wpc->psy_wpc.set_property	= sec_wpc_set_property;
	wpc->psy_wpc.properties	= sec_wpc_props;
	wpc->psy_wpc.num_properties =
		ARRAY_SIZE(sec_wpc_props);

	ret = power_supply_register(&pdev->dev, &wpc->psy_wpc);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Failed to Register psy_wpc\n", __func__);
		goto err_create_wqueue;
	}

	return 0;

err_create_wqueue:
	destroy_workqueue(wpc->fod_wqueue);

err_create_group:
	device_destroy(power_supply_class,wpc_device->devt);
	wake_lock_destroy(&wpc->fod_wake_lock);

err_device_create:
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&wpc->wpc_mutex);
	kfree(wpc);
err_return:
	return ret;
}

static int __devexit bcm5935x_wpc_low_remove(struct platform_device *pdev)
{
	struct bcm5935x_wpc_info *data = platform_get_drvdata(pdev);

	sysfs_remove_group(&wpc_device->kobj, &bcm5935x_wpc_low_attr_group);

	if (data) {
		mutex_destroy(&data->wpc_mutex);
		kfree(data);
	}

	return 0;
}

void bcm5935x_wpc_low_shutdown(struct device *dev)
{
	pr_info("%s\n", __func__);
}

static struct platform_driver bcm5935x_wpc_low_driver = {
	.driver		= {
		.name	= "bcm5935x_wpc_low",
		.owner	= THIS_MODULE,
		.shutdown = bcm5935x_wpc_low_shutdown,
	},
	.probe		= bcm5935x_wpc_low_probe,
	.remove		= __devexit_p(bcm5935x_wpc_low_remove),
};

static int __init bcm5935x_wpc_low_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&bcm5935x_wpc_low_driver);
}
module_init(bcm5935x_wpc_low_init);

static void __exit bcm5935x_wpc_low_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&bcm5935x_wpc_low_driver);
}
module_exit(bcm5935x_wpc_low_exit);


MODULE_DESCRIPTION("BCM 5935X WPC low(under reg100) driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
