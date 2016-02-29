/*
 *  bcm5935x_wpc_high.c
 *  Samsung BCM5935X WPC high(over reg100) Driver
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
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/input.h>

#define WPC_CFG_FILE_PATH "/opt/usr/media/bcm5935x.cfg"

extern struct class *power_supply_class;

struct device *wpc_high_device;

struct bcm5935x_wpc_info *g_wpc_high;

static enum power_supply_property sec_wpc_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
};

static int bcm5935x_wpc_config_set(struct bcm5935x_wpc_info *wpc,
					  const char *buf, int fw_size)
{
	int i;
	int ret = 0, reg = 0, val = 0;

	for (i = 0; i < (fw_size / 10); i++) {
		sscanf(buf, "%x %x", &reg, &val);

		if (reg < BCM5935X_WPT_WSELCTRL1) {
			ret = bcm5935x_write_reg(wpc->i2c, reg, val);
		}
		else if (reg >= BCM5935X_WPT_WSELCTRL1) {
			ret = bcm5935x_write_reg(wpc->i2c, reg, val);
		}

		buf = buf+11;
	}

	return ret;
}

static int bcm5935x_wpc_config(struct bcm5935x_wpc_info *wpc)
{
	struct file *fp;
	mm_segment_t old_fs;
	int fw_size, nread;
	int error = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(WPC_CFG_FILE_PATH, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(wpc->dev, "failed to open cfg file. %s\n",
			WPC_CFG_FILE_PATH);
		error = -ENOENT;
		goto open_err;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (fw_size > 0) {
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data,
			fw_size, &fp->f_pos);

		if (nread != fw_size) {
			dev_err(wpc->dev, "failed to read wpc cfg file, nread %u Bytes\n",
				nread);
			error = -EIO;
		} else {
			error = bcm5935x_wpc_config_set(wpc,
				(const u8 *)fw_data, fw_size);
		}

		if (error < 0)
			dev_err(wpc->dev, "failed wpc configuration\n");

		kfree(fw_data);
	}

	filp_close(fp, current->files);

open_err:
	set_fs(old_fs);
	return error;
}

static void bcm5935x_wireless_protocol_mode(struct bcm5935x_wpc_info *wpc)
{
	int ret, wsel_proto;
	union power_supply_propval val;

	ret = bcm5935x_read_reg(wpc->i2c, BCM5935X_WPT_WSELSTS);
	wsel_proto = ret & 0x03;
	if (wsel_proto == 0x01) {
		ret = bcm5935x_read_reg(wpc->i2c, BCM5935X_WPT_WSELCTRL2);
		ret = ret & BCM5935X_WPT_WSELCTRL2_M;
		ret |= wsel_proto;
		bcm5935x_write_reg(wpc->i2c, BCM5935X_WPT_WSELCTRL2,ret);
		wpc->protocol = WPC_PROTOCOL;
	} else if (wsel_proto == 0x02) {
		ret = bcm5935x_read_reg(wpc->i2c, BCM5935X_WPT_WSELCTRL2);
		ret = ret & BCM5935X_WPT_WSELCTRL2_M;
		ret |= wsel_proto;
		bcm5935x_write_reg(wpc->i2c, BCM5935X_WPT_WSELCTRL2,ret);
		wpc->protocol = PMA_PROTOCOL;
	} else
		pr_err("%s:unknown protocol[%d]\n", __func__, wsel_proto);

	pr_info("%s:mode[%d],\n",  __func__, wpc->protocol);

	/*disable ZT1N, ZT1P */
	val.intval = 0x0C; 
	psy_do_property("sec-wpc", set,
		POWER_SUPPLY_PROP_CHARGE_TYPE, val);
}

static void bcm5935x_wpc_set_charge(struct bcm5935x_wpc_info *wpc)
{
	int data;

	data = 0x0B;
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSLOPE, data);
	pr_info("%s:reg[0x%x], data[0x%x]\n", __func__,  BCM5935X_WPC_VRECTSLOPE, data);
}

void bcm5935x_wpc_send_EPT_packet(struct bcm5935x_wpc_info *wpc)
{
	msleep(100);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_CTRL1, 0x05);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_RP_VALUE, 0x06);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_CTRL1, 0xB5);
	msleep(200);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_RP_VALUE, 0x55);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_CTRL1, 0xA7);
	pr_info("%s\n", __func__);
}

static void bcm5935x_vrect_change_low(struct bcm5935x_wpc_info *wpc)
{
	union power_supply_propval val;

	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSTART, 0x07);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSTOP, 0x06);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSLOPE, 0x16);
	msleep(2);

	val.intval = wpc->voltage_now;
	psy_do_property("sec-wpc", set,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
	pr_info("%s\n", __func__);
}

static void bcm5935x_vrect_change_high(struct bcm5935x_wpc_info *wpc)
{
	union power_supply_propval val;

	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSTART, 0x07);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSTOP, 0x06);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSLOPE, 0x0E);
	msleep(2);

	val.intval = wpc->voltage_now;
	psy_do_property("sec-wpc", set,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
	pr_info("%s\n", __func__);
}

static void bcm5935x_vrect_change_very_high(struct bcm5935x_wpc_info *wpc)
{
	union power_supply_propval val;

	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSTART, 0x07);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSTOP, 0x06);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSLOPE, 0x0A);
	msleep(2);

	val.intval = wpc->voltage_now;
	psy_do_property("sec-wpc", set,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
	pr_info("%s\n", __func__);
}

static void bcm5935x_vrect_change_for_high_temp(struct bcm5935x_wpc_info *wpc)
{
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSTART, 0x0B);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSTOP, 0x07);
	bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_VRECTSLOPE, 0x1C);
	msleep(2);

	pr_info("%s\n", __func__);
}
void bcm5935x_wpc_send_complete_work(struct work_struct *work)
{
	struct bcm5935x_wpc_info *wpc =
			container_of(work, struct bcm5935x_wpc_info, full_work.work);

	int saved_RP, saved_CTRL1, new_CTRL1, new_CTRL2, new_CTRL1_read;
	int i = 5;

	do {
		saved_RP = bcm5935x_read_reg(wpc->i2c, BCM5935X_WPC_RP_VALUE);
		saved_CTRL1 = bcm5935x_read_reg(wpc->i2c, BCM5935X_WPC_CTRL1);
		bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_RP_VALUE, 0x01);
		new_CTRL1 = (saved_CTRL1 & 0x0D) | 0xB0;
		bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_CTRL1, new_CTRL1);
		new_CTRL1_read = bcm5935x_read_reg(wpc->i2c, BCM5935X_WPC_CTRL1);
		msleep(100);
		new_CTRL2 = (new_CTRL1 & 0x0F) | 0x02;
		bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_CTRL1, new_CTRL2);
		bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_RP_VALUE, saved_RP);

		pr_info("%s:saved_RP[0x%x], saved_CTRL1[0x%x], new_CTRL1[0x%x], new_CTRL1_read[0x%x], new_CTRL2[0x%x]\n",
			__func__, saved_RP, saved_CTRL1, new_CTRL1, new_CTRL1_read, new_CTRL2);
	} while (i-- > 0);
}

static int sec_wpc_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct bcm5935x_wpc_info *wpc =
		container_of(psy, struct bcm5935x_wpc_info, psy_wpc);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = wpc->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = wpc->voltage_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 0;
		/* FOD */
		g_wpc_high->fod_reg[0] = bcm5935x_read_reg(wpc->i2c, 0x141);
		g_wpc_high->fod_reg[1] = bcm5935x_read_reg(wpc->i2c, 0x143);
		g_wpc_high->fod_reg[2] = bcm5935x_read_reg(wpc->i2c, 0x144);
		g_wpc_high->fod_reg[3] = bcm5935x_read_reg(wpc->i2c, 0x145);
		g_wpc_high->fod_reg[4] = bcm5935x_read_reg(wpc->i2c, 0x146);
		g_wpc_high->fod_reg[5] = bcm5935x_read_reg(wpc->i2c, 0x147);
		g_wpc_high->fod_reg[6] = bcm5935x_read_reg(wpc->i2c, 0x11a);
		g_wpc_high->fod_reg[7] = bcm5935x_read_reg(wpc->i2c, 0x11b);
		pr_info ("0x%x, 0x%x\n", g_wpc_high->fod_reg[6], g_wpc_high->fod_reg[7]);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
extern unsigned int batt_booting_chk;

static int sec_wpc_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct bcm5935x_wpc_info *wpc =
		container_of(psy, struct bcm5935x_wpc_info, psy_wpc);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL)
			schedule_delayed_work(&wpc->full_work, 0);
		else if (val->intval == POWER_SUPPLY_STATUS_CHARGING) {
			bcm5935x_wireless_protocol_mode(wpc);
			bcm5935x_wpc_set_charge(wpc);
			bcm5935x_wpc_config(wpc);
		}
		pr_info("%s:val[%d]\n", __func__, val->intval);
		wpc->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		wpc->voltage_now = val->intval;
		pr_info("%s:vout_status[%d],", __func__, wpc->vout_status);
		pr_info("voltage[%d]\n", wpc->voltage_now);
		if (wpc->vout_status == VOUT_UNKNOWN) {
			pr_info("%s: batt_booting_chk = %d\n",
				__func__, batt_booting_chk);
			if (batt_booting_chk) {
				msleep(2000);
				if (val->intval < 4000) {
					bcm5935x_vrect_change_low(wpc);
					wpc->vout_status = VOUT_4500mV;
				} else if ((val->intval >= 4000) && (val->intval < 4200)) {
					bcm5935x_vrect_change_high(wpc);
					wpc->vout_status = VOUT_4700mV;
				} else if (val->intval >= 4200) {
					bcm5935x_vrect_change_very_high(wpc);
					wpc->vout_status = VOUT_4800mV;
				}
			}
		} else if ((wpc->vout_status == VOUT_4500mV) &&
			(val->intval >= 4000)) {
				bcm5935x_vrect_change_high(wpc);
				wpc->vout_status = VOUT_4700mV;
		} else if (wpc->vout_status == VOUT_4700mV) {
			if (val->intval >= 4200) {
				bcm5935x_vrect_change_very_high(wpc);
				wpc->vout_status = VOUT_4800mV;
			}
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		bcm5935x_write_reg(wpc->i2c, BCM5935X_WPC_RP_VALUE, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		bcm5935x_vrect_change_for_high_temp(wpc);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bcm5935x_wpc_set_high_reg(struct bcm5935x_wpc_info *wpc, int reg, int val)
{
	int ret;
	
	ret = bcm5935x_write_reg(wpc->i2c, reg, val);

	return ret;

}

static ssize_t bcm5935x_wpc_store_write_high_reg(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);

	int ret = 0, reg = 0, val = 0;

	sscanf(buf, "%x %x", &reg, &val);
	if (reg < BCM5935X_WPT_WSELCTRL1 || reg > BCM5935X_PMA_RXID5)
		return -EINVAL;

	ret = bcm5935x_wpc_set_high_reg(wpc, reg, val);

	if (ret < 0)
		pr_info("%s: write reg fail[%d]", __func__, ret);

	return count;
}

static int bcm5935x_wpc_get_high_reg(struct bcm5935x_wpc_info *wpc, int reg)
{
	int val;
	
	val = bcm5935x_read_reg(wpc->i2c, reg);

	return val;
}
static ssize_t bcm5935x_wpc_show_read_high_reg(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);
	
	unsigned int ret = 0;
	ret =  sprintf(buf, "0x%x\n", wpc->reg);
	return ret;

}


static ssize_t bcm5935x_wpc_store_read_high_reg(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);

	int reg = 0, val = 0;
	
	sscanf(buf, "%x", &reg);
	if (reg < BCM5935X_WPT_WSELCTRL1 || reg > BCM5935X_PMA_RXID5)
		return -EINVAL;


	val = bcm5935x_wpc_get_high_reg(wpc, reg);

	wpc->reg = val;

	pr_info("%s: reg[0x%x]:[%d]\n", __func__, reg, val);

	return count;
}

static ssize_t bcm5935x_wpc_show_freq_reg(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct bcm5935x_wpc_info *wpc = dev_get_drvdata(dev);
	
	unsigned int ret = 0;
	int val;
	
	val = bcm5935x_read_reg(wpc->i2c, BCM5935X_WPT_MEAS_FREQ);
	ret =  sprintf(buf, "0x%x\n", val);
	return ret;

}

static struct device_attribute dev_attr_read_high_reg = __ATTR(read_high_reg, 0664, bcm5935x_wpc_show_read_high_reg, bcm5935x_wpc_store_read_high_reg);
static struct device_attribute dev_attr_write_high_reg = __ATTR(write_high_reg, 0664, NULL, bcm5935x_wpc_store_write_high_reg);
static struct device_attribute dev_attr_freq_reg = __ATTR(freq_reg, 0664, bcm5935x_wpc_show_freq_reg, NULL);

static struct attribute *bcm5935x_wpc_high_attributes[] = {
	&dev_attr_read_high_reg.attr,
	&dev_attr_write_high_reg.attr,
	&dev_attr_freq_reg.attr,
	NULL,
};

static struct attribute_group bcm5935x_wpc_high_attr_group = {
	.attrs = bcm5935x_wpc_high_attributes,
};

extern int max77836_ldo2_set(bool enable);

void bcm5935x_isr_work(struct work_struct *work)
{
	struct bcm5935x_dev *bcm5935x =
		container_of(work, struct bcm5935x_dev, isr_work.work);
	union power_supply_propval val;
	struct power_supply *psy = power_supply_get_by_name("battery");
	int ret;
	int irq;

	val.intval = bcm5935x->pdata->detect_type;
	psy_do_property("sec-wpc", set,
		POWER_SUPPLY_PROP_TYPE, val);

	irq = gpio_get_value(bcm5935x->pdata->irq_gpio);
	pr_info("%s: irq[%d]\n", __func__, irq);

	if (irq) {
		/* disable ldo2 to supply the power on wpc ic */
		ret = max77836_ldo2_set(false);
		if (ret < 0)
			pr_err("%s: ldo2 set failed.\n", __func__);

		/* WPC detached */
		val.intval = POWER_SUPPLY_TYPE_BATTERY;

		psy_do_property("sec-wpc", set,
			POWER_SUPPLY_PROP_ONLINE, val);
		if (!psy || !psy->set_property)
			pr_err("%s: fail to get battery psy\n", __func__);
		else
			psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
	} else {
		/* WPC attached */
		/* enable ldo2 to supply the power on wpc ic */
		ret = max77836_ldo2_set(true);
		if (ret < 0)
			pr_err("%s: ldo2 set failed.\n", __func__);

		irq = bcm5935x_read_reg(bcm5935x->i2c_low,
				BCM5935X_PMUID);

		if (irq == 0x50) {
			val.intval = POWER_SUPPLY_TYPE_WPC;

			psy_do_property("sec-wpc", set,
				POWER_SUPPLY_PROP_ONLINE, val);
			if (!psy || !psy->set_property) {
				pr_err("%s: fail to get battery psy\n", __func__);
				schedule_delayed_work(&bcm5935x->isr_work, 1000);
			} else
				psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		} else {
			pr_err("%s: err[%d]. no wpc\n", __func__, irq);

			/* disable ldo2 to supply the power on wpc ic */
			ret = max77836_ldo2_set(false);
			if (ret < 0)
				pr_err("%s: ldo2 set failed.\n", __func__);
		}
	}
}

irqreturn_t bcm5935x_irq_thread(int irq, void *data)
{
	struct bcm5935x_dev *bcm5935x = data;

	if (bcm5935x->is_wpc_ready == true)
		schedule_delayed_work(&bcm5935x->isr_work, 0);
	else
		pr_err("%s: wpc is not ready\n", __func__);

	return IRQ_HANDLED;
}

int bcm5935x_irq_init(struct bcm5935x_dev *bcm5935x)
{
	struct bcm5935x_platform_data *pdata = bcm5935x->pdata;
	int ret;

	if (!pdata->irq_gpio) {
		pr_warn("%s No interrupt specified.\n", __func__);
		return 0;
	}

	pr_info("%s: irq_gpio[%d]", __func__, pdata->irq_gpio);
	
	mutex_init(&bcm5935x->irq_lock);

	bcm5935x->irq = gpio_to_irq(pdata->irq_gpio);
	ret = gpio_request(pdata->irq_gpio, "bcm5935x_irq");
	if (ret) {
		pr_err("%s failed requesting gpio(%d)\n",
			__func__, pdata->irq_gpio);
		return ret;
	}

	INIT_DELAYED_WORK_DEFERRABLE(
		&bcm5935x->isr_work, bcm5935x_isr_work);

	if (pdata->detect_type == SEC_WPC_DETECT_MUIC_INT) {
		pr_warn("%s MUIC INT used for WPC detection.\n", __func__);
		return 0;
	}

	ret = request_threaded_irq(bcm5935x->irq, NULL, bcm5935x_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING| IRQF_ONESHOT,
				   "bcm5935x-irq", bcm5935x);
	if (ret) {
		pr_err("%s Failed to request IRQ(%d) ret(%d)\n",
				__func__, bcm5935x->irq, ret);
		return ret;
	}

	ret = enable_irq_wake(bcm5935x->irq);
	if (ret < 0)
		pr_err("%s: Failed to Enable Wakeup Source(%d)\n",
			__func__, ret);

	return 0;
}

static void bcm5935x_init_detect(struct work_struct *work)
{
	struct bcm5935x_dev *bcm5935x =
			container_of(work, struct bcm5935x_dev, init_work.work);

	pr_info("%s\n", __func__);
	bcm5935x->is_wpc_ready = true;

	schedule_delayed_work(&bcm5935x->isr_work, 0);
}

static int __devinit bcm5935x_wpc_high_probe(struct platform_device *pdev)
{
	struct bcm5935x_dev *bcm5935x = dev_get_drvdata(pdev->dev.parent);
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
	wpc->i2c = bcm5935x->i2c_high;

	platform_set_drvdata(pdev, wpc);

	wpc_high_device = device_create(power_supply_class, NULL, 0, NULL, "wpc_high");
	if (IS_ERR(wpc_high_device)) {
		pr_err("Failed to create wpc device for the sysfs\n");
		goto err_device_create;
	}

	/* create sysfs group */
	ret = sysfs_create_group(&wpc_high_device->kobj, &bcm5935x_wpc_high_attr_group);
	if (ret) {
		pr_err("%s: failed to create bcm5935x wpc high attribute group\n",
				__func__);
		goto err_create_group;
	}
	dev_set_drvdata(wpc_high_device, wpc);

	wpc->status = POWER_SUPPLY_STATUS_UNKNOWN;
	wpc->vout_status = VOUT_UNKNOWN;

	wpc->psy_wpc.name		= "sec-wpc2";
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
		goto err_create_group;
	}

	g_wpc_high = wpc;

	ret = bcm5935x_irq_init(bcm5935x);
	if (ret < 0)
		goto err_create_group;

	INIT_DELAYED_WORK_DEFERRABLE(
		&wpc->full_work, bcm5935x_wpc_send_complete_work);

	/* initial WPC detection */
	INIT_DELAYED_WORK(&bcm5935x->init_work, bcm5935x_init_detect);
	schedule_delayed_work(&bcm5935x->init_work,
			msecs_to_jiffies(3000));

	return 0;

err_create_group:
	device_destroy(power_supply_class,wpc_high_device->devt);

err_device_create:
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&wpc->wpc_mutex);
	kfree(wpc);
err_return:
	return ret;
}

static int __devexit bcm5935x_wpc_high_remove(struct platform_device *pdev)
{
	struct bcm5935x_wpc_info *wpc = platform_get_drvdata(pdev);

	sysfs_remove_group(&wpc_high_device->kobj, &bcm5935x_wpc_high_attr_group);

	if (wpc) {
		mutex_destroy(&wpc->wpc_mutex);
		kfree(wpc);
	}

	return 0;
}

void bcm5935x_wpc_high_shutdown(struct device *dev)
{
	pr_info("%s\n", __func__);
}

static struct platform_driver bcm5935x_wpc_high_driver = {
	.driver		= {
		.name	= "bcm5935x_wpc_high",
		.owner	= THIS_MODULE,
		.shutdown = bcm5935x_wpc_high_shutdown,
	},
	.probe		= bcm5935x_wpc_high_probe,
	.remove		= __devexit_p(bcm5935x_wpc_high_remove),
};

static int __init bcm5935x_wpc_high_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&bcm5935x_wpc_high_driver);
}
module_init(bcm5935x_wpc_high_init);

static void __exit bcm5935x_wpc_high_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&bcm5935x_wpc_high_driver);
}
module_exit(bcm5935x_wpc_high_exit);


MODULE_DESCRIPTION("BCM 5935X WPC high(over reg100) driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
