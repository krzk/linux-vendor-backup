/*
 * driver/misc/tsu6721-muic.c - TSU6721 micro USB switch device driver
 *
 * Copyright (C) 2010 Samsung Electronics
 * Seung-Jin Hahn <sjin.hahn@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <plat/udc-hs.h>

#include <linux/muic.h>
#include <linux/tsu6721.h>

extern struct sec_switch_data sec_switch_data;

/* don't access this variable directly!! except get_switch_sel_value function.
 * you must get switch_sel value by using get_switch_sel function. */
static int switch_sel;

/* func : set_switch_sel
 * switch_sel value get from bootloader comand line
 * switch_sel data consist 4 bits
 */
static int set_switch_sel(char *str)
{
	get_option(&str, &switch_sel);
	switch_sel = switch_sel & 0x0f;
	pr_info("%s:%s switch_sel: 0x%x\n", MUIC_DEV_NAME, __func__,
			switch_sel);

	return switch_sel;
}
__setup("pmic_info=", set_switch_sel);

static int get_switch_sel(void)
{
	return switch_sel;
}

static ssize_t tsu6721_muic_show_uart_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tsu6721_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		pr_info("%s:%s AP\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "AP\n");
	case MUIC_PATH_UART_CP:
		pr_info("%s:%s CP\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "CP\n");
	default:
		break;
	}

	pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "UNKNOWN\n");
}

/* declare muic uart path control function prototype. */
static int switch_to_ap_uart(struct tsu6721_muic_data *muic_data);
static int switch_to_cp_uart(struct tsu6721_muic_data *muic_data);

static void tsu6721_disable_interrupt(struct tsu6721_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int value, ret;

	value = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_CTRL);
	value |= CTRL_INT_MASK_MASK;

	ret = i2c_smbus_write_byte_data(i2c, TSU6721_MUIC_REG_CTRL, value);
	if (ret < 0)
		dev_err(&i2c->dev, "%s: err %d\n", __func__, ret);

}

static void tsu6721_enable_interrupt(struct tsu6721_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int value, ret;

	value = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_CTRL);
	value &= (~CTRL_INT_MASK_MASK);

	ret = i2c_smbus_write_byte_data(i2c, TSU6721_MUIC_REG_CTRL, value);
	if (ret < 0)
		dev_err(&i2c->dev, "%s: err %d\n", __func__, ret);

}

static ssize_t tsu6721_muic_set_uart_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct tsu6721_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "AP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		switch_to_ap_uart(muic_data);
	} else if (!strncasecmp(buf, "CP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_CP;
		switch_to_cp_uart(muic_data);
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s uart_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->uart_path);

	return count;
}

static ssize_t tsu6721_muic_show_usb_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tsu6721_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
		pr_info("%s:%s PDA\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "PDA\n");
	case MUIC_PATH_USB_CP:
		pr_info("%s:%s MODEM\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "MODEM\n");
	default:
		break;
	}

	pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t tsu6721_muic_set_usb_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct tsu6721_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "PDA", 3)) {
		pdata->usb_path = MUIC_PATH_USB_AP;
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		pdata->usb_path = MUIC_PATH_USB_CP;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s usb_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->usb_path);

	return count;
}

static ssize_t tsu6721_muic_show_adc(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct tsu6721_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	ret = i2c_smbus_read_byte_data(muic_data->i2c, TSU6721_MUIC_REG_ADC);
	if (ret < 0) {
		pr_err("%s:%s err read adc reg(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", (ret & ADC_ADC_MASK));
}

static ssize_t tsu6721_muic_show_usb_state(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct tsu6721_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s muic_data->attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return 0;
}

#if defined(CONFIG_USB_HOST_NOTIFY)
static ssize_t tsu6721_muic_show_otg_test(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return 0;
}

static ssize_t tsu6721_muic_set_otg_test(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	return 0;
}
#endif

static ssize_t tsu6721_muic_show_attached_dev(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct tsu6721_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s attached_dev:%d\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch(muic_data->attached_dev) {
	case ATTACHED_DEV_NONE_MUIC:
		return sprintf(buf, "No VPS\n");
	case ATTACHED_DEV_USB_MUIC:
		return sprintf(buf, "USB\n");
	case ATTACHED_DEV_CDP_MUIC:
		return sprintf(buf, "CDP\n");
	case ATTACHED_DEV_OTG_MUIC:
		return sprintf(buf, "OTG\n");
	case ATTACHED_DEV_TA_MUIC:
		return sprintf(buf, "TA\n");
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		return sprintf(buf, "JIG UART OFF\n");
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		return sprintf(buf, "JIG UART OFF/VB\n");
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		return sprintf(buf, "JIG UART ON\n");
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		return sprintf(buf, "JIG USB OFF\n");
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "JIG USB ON\n");
	case ATTACHED_DEV_DESKDOCK_MUIC:
		return sprintf(buf, "DESKDOCK\n");
	default:
		break;
	}

	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t tsu6721_muic_show_audio_path(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	return 0;
}

static ssize_t tsu6721_muic_set_audio_path(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return 0;
}


static ssize_t tsu6721_muic_show_apo_factory(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct tsu6721_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_data->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t tsu6721_muic_set_apo_factory(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tsu6721_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13)) {
		muic_data->is_factory_start = true;
		mode = "FACTORY_MODE";
	} else {
		pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return count;
}

static DEVICE_ATTR(uart_sel, 0664, tsu6721_muic_show_uart_sel,
		tsu6721_muic_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664,
		tsu6721_muic_show_usb_sel, tsu6721_muic_set_usb_sel);
static DEVICE_ATTR(adc, 0664, tsu6721_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, 0664, tsu6721_muic_show_usb_state, NULL);
#if defined(CONFIG_USB_HOST_NOTIFY)
static DEVICE_ATTR(otg_test, 0664,
		tsu6721_muic_show_otg_test, tsu6721_muic_set_otg_test);
#endif
static DEVICE_ATTR(attached_dev, 0664, tsu6721_muic_show_attached_dev, NULL);
static DEVICE_ATTR(audio_path, 0664,
		tsu6721_muic_show_audio_path, tsu6721_muic_set_audio_path);
static DEVICE_ATTR(apo_factory, 0664,
		tsu6721_muic_show_apo_factory,
		tsu6721_muic_set_apo_factory);

static struct attribute *tsu6721_muic_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&dev_attr_otg_test.attr,
#endif
	&dev_attr_attached_dev.attr,
	&dev_attr_audio_path.attr,
	&dev_attr_apo_factory.attr,
	NULL
};

static const struct attribute_group tsu6721_muic_group = {
	.attrs = tsu6721_muic_attributes,
};

static int set_ctrl_reg(struct tsu6721_muic_data *muic_data, int shift, bool on)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_val;
	int ret = 0;

	ret = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_CTRL);
	if (ret < 0)
		pr_err("%s:%s err read CTRL(%d)\n", MUIC_DEV_NAME, __func__,
				ret);

	ret &= CTRL_MASK;

	if (on)
		reg_val = ret | (0x1 << shift);
	else
		reg_val = ret & ~(0x1 << shift);

	if (reg_val ^ ret) {
		pr_info("%s:%s reg_val(0x%x)!=CTRL reg(0x%x), update reg\n",
				MUIC_DEV_NAME, __func__, reg_val, ret);

		ret = i2c_smbus_write_byte_data(i2c, TSU6721_MUIC_REG_CTRL,
				reg_val);
		if (ret < 0)
			pr_err("%s:%s err write CTRL(%d)\n", MUIC_DEV_NAME,
					__func__, ret);
	} else {
		pr_info("%s:%s reg_val(0x%x)==CTRL reg(0x%x), just return\n",
				MUIC_DEV_NAME, __func__, reg_val, ret);
		return 0;
	}

	ret = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_CTRL);
	if (ret < 0)
		pr_err("%s:%s err read CTRL(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
	else
		pr_info("%s:%s CTRL reg after change(0x%x)\n", MUIC_DEV_NAME,
				__func__, ret);

	return ret;
}

static int set_int_mask(struct tsu6721_muic_data *muic_data, bool on)
{
	int shift = CTRL_INT_MASK_SHIFT;
	int ret = 0;

	ret = set_ctrl_reg(muic_data, shift, on);

	return ret;
}

#if 0
static int set_raw_data(struct tsu6721_muic_data *muic_data, bool on)
{
	int shift = CTRL_RAW_DATA_SHIFT;
	int ret = 0;

	ret = set_ctrl_reg(muic_data, shift, on);

	return ret;
}
#endif

static int set_manual_sw(struct tsu6721_muic_data *muic_data, bool on)
{
	int shift = CTRL_MANUAL_SW_SHIFT;
	int ret = 0;

	ret = set_ctrl_reg(muic_data, shift, on);

	return ret;
}

static int set_com_sw(struct tsu6721_muic_data *muic_data,
			enum tsu6721_reg_manual_sw1_value reg_val)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	u8 temp = 0;

	temp = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_MANSW1);
	if (temp < 0)
		pr_err("%s:%s err read MANSW1(%d)\n", MUIC_DEV_NAME, __func__,
				temp);

	if (reg_val ^ temp) {
		pr_info("%s:%s reg_val(0x%x)!=MANSW1 reg(0x%x), update reg\n",
				MUIC_DEV_NAME, __func__, reg_val, temp);

		ret = i2c_smbus_write_byte_data(i2c, TSU6721_MUIC_REG_MANSW1,
				reg_val);
		if (ret < 0)
			pr_err("%s:%s err write MANSW1(%d)\n", MUIC_DEV_NAME,
					__func__, reg_val);
	} else {
		pr_info("%s:%s MANSW1 reg(0x%x), just return\n",
				MUIC_DEV_NAME, __func__, reg_val);
		return 0;
	}

	temp = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_MANSW1);
	if (temp < 0)
		pr_err("%s:%s err read MANSW1(%d)\n", MUIC_DEV_NAME, __func__,
				temp);

	if (temp ^ reg_val) {
		pr_err("%s:%s err set MANSW1(0x%x)\n", MUIC_DEV_NAME, __func__,
			temp);
		ret = -EAGAIN;
	}

	return ret;
}

static int com_to_open(struct tsu6721_muic_data *muic_data)
{
	enum tsu6721_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_OPEN;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_sw err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, false);

	return ret;
}

static int com_to_open_with_vbus(struct tsu6721_muic_data *muic_data)
{
	enum tsu6721_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_OPEN_WITH_V_BUS;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_sw err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, false);

	return ret;
}

static int com_to_usb(struct tsu6721_muic_data *muic_data)
{
	enum tsu6721_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_USB;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_usb err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, false);

	return ret;
}

static int com_to_uart(struct tsu6721_muic_data *muic_data)
{
	enum tsu6721_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_UART;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_uart err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, false);

	return ret;
}

static int com_to_audio(struct tsu6721_muic_data *muic_data)
{
	enum tsu6721_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_AUDIO;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_audio err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, false);

	return ret;
}

static int switch_to_dock_audio(struct tsu6721_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_doc_switch)
		pdata->set_gpio_doc_switch(GPIO_LEVEL_HIGH);

	ret = com_to_audio(muic_data);
	if (ret) {
		pr_err("%s:%s com->audio set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_system_audio(struct tsu6721_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_doc_switch)
		pdata->set_gpio_doc_switch(GPIO_LEVEL_LOW);

	return ret;
}


static int switch_to_ap_usb(struct tsu6721_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_usb_sel)
		pdata->set_gpio_usb_sel(MUIC_PATH_USB_AP);

	ret = com_to_usb(muic_data);
	if (ret) {
		pr_err("%s:%s com->usb set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_cp_usb(struct tsu6721_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_usb_sel)
		pdata->set_gpio_usb_sel(MUIC_PATH_USB_CP);

	ret = com_to_usb(muic_data);
	if (ret) {
		pr_err("%s:%s com->usb set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_ap_uart(struct tsu6721_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_uart_sel)
		pdata->set_gpio_uart_sel(MUIC_PATH_UART_AP);

	ret = com_to_uart(muic_data);
	if (ret) {
		pr_err("%s:%s com->uart set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_cp_uart(struct tsu6721_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_uart_sel)
		pdata->set_gpio_uart_sel(MUIC_PATH_UART_CP);

	ret = com_to_uart(muic_data);
	if (ret) {
		pr_err("%s:%s com->uart set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int attach_charger(struct tsu6721_muic_data *muic_data,
			enum muic_attached_dev new_dev)
{
	int ret = 0;

	pr_info("%s:%s new_dev(%d)\n", MUIC_DEV_NAME, __func__, new_dev);

	if (!muic_data->switch_data->charger_cb) {
		pr_err("%s:%s charger_cb is NULL\n", MUIC_DEV_NAME, __func__);
		return -ENXIO;
	}

	if (muic_data->attached_dev == new_dev) {
		pr_info("%s:%s already, ignore.\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	ret = muic_data->switch_data->charger_cb(new_dev);
	if (ret) {
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		pr_err("%s:%s charger_cb(%d) err\n", MUIC_DEV_NAME, __func__,
				ret);
	}

	muic_data->attached_dev = new_dev;

	return ret;
}

static int detach_charger(struct tsu6721_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!switch_data->charger_cb) {
		pr_err("%s:%s charger_cb is NULL\n", MUIC_DEV_NAME, __func__);
		return -ENXIO;
	}

	ret = switch_data->charger_cb(ATTACHED_DEV_NONE_MUIC);
	if (ret) {
		pr_err("%s:%s charger_cb(%d) err\n", MUIC_DEV_NAME, __func__,
				ret);
		return ret;
	}

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_usb_util(struct tsu6721_muic_data *muic_data,
			enum muic_attached_dev new_dev)
{
	int ret = 0;

	ret = attach_charger(muic_data, new_dev);
	if (ret)
		return ret;

	if (muic_data->pdata->usb_path == MUIC_PATH_USB_CP) {
		ret = switch_to_cp_usb(muic_data);
		return ret;
	}

	ret = switch_to_ap_usb(muic_data);
	return ret;
}

static int attach_usb(struct tsu6721_muic_data *muic_data,
			enum muic_attached_dev new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	if (muic_data->attached_dev == new_dev) {
		pr_info("%s:%s duplicated(USB)\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb_util(muic_data, new_dev);
	if (ret)
		return ret;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_CABLE_ATTACHED);

	return ret;
}

static int detach_usb(struct tsu6721_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	pr_info("%s:%s attached_dev type(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	ret = detach_charger(muic_data);
	if (ret)
		return ret;

	ret = com_to_open(muic_data);
	if (ret)
		return ret;

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	if (pdata->usb_path == MUIC_PATH_USB_CP)
		return ret;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_CABLE_DETACHED);

	return ret;
}

static int attach_deskdock(struct tsu6721_muic_data *muic_data,
			enum muic_attached_dev new_dev, u8 vbvolt)
{
	int ret = 0;

	pr_info("%s:%s vbus(%x)\n", MUIC_DEV_NAME, __func__, vbvolt);

	ret = switch_to_dock_audio(muic_data);
	if (ret)
		return ret;
/*
	if (vbvolt) 
	{
		muic_data->attached_dev == ATTACHED_DEV_DESKDOCK_VB_MUIC;
		ret = attach_charger(muic_data, new_dev);
	}
	else if (muic_data->attached_dev == ATTACHED_DEV_DESKDOCK_VB_MUIC)
	{
		ret = detach_charger(muic_data);
	}
	*/
	ret = attach_charger(muic_data, new_dev);

	if (ret)
		return ret;

	muic_data->attached_dev = new_dev;


	if(muic_data->switch_data->dock_cb)
		muic_data->switch_data->dock_cb(MUIC_DOCK_DESKDOCK);

	return ret;
}

static int detach_deskdock(struct tsu6721_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = switch_to_system_audio(muic_data);
	if(ret)
		pr_err("%s:%s err changing audio path(%d)\n", MUIC_DEV_NAME,
			__func__, ret);

	ret = detach_charger(muic_data);
	if(ret)
		pr_err("%s:%s err detach_charger(%d)\n", MUIC_DEV_NAME, __func__, ret);

	if(muic_data->switch_data->dock_cb)
		muic_data->switch_data->dock_cb(MUIC_DOCK_DETACHED);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}


static int attach_jig_uart_boot_off(struct tsu6721_muic_data *muic_data,
				u8 vbvolt)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	enum muic_attached_dev new_dev;
	int ret = 0;

	pr_info("%s:%s JIG UART BOOT-OFF(0x%x)\n", MUIC_DEV_NAME, __func__,
			vbvolt);

	if (pdata->uart_path == MUIC_PATH_UART_AP)
		ret = switch_to_ap_uart(muic_data);
	else
		ret = switch_to_cp_uart(muic_data);

	if (vbvolt) new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
	else new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;

	ret = attach_charger(muic_data, new_dev);

	return ret;
}
static int detach_jig_uart_boot_off(struct tsu6721_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = detach_charger(muic_data);
	if(ret)
		pr_err("%s:%s err detach_charger(%d)\n", MUIC_DEV_NAME, __func__, ret);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_jig_usb_boot_off(struct tsu6721_muic_data *muic_data,
				u8 vbvolt)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	if (!vbvolt)
		return ret;

	if (muic_data->attached_dev == ATTACHED_DEV_JIG_USB_OFF_MUIC) {
		pr_info("%s:%s duplicated(JIG USB OFF)\n", MUIC_DEV_NAME,
				__func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb_util(muic_data, ATTACHED_DEV_JIG_USB_OFF_MUIC);
	if (ret)
		return ret;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_CABLE_ATTACHED);

	return ret;
}

static int attach_jig_usb_boot_on(struct tsu6721_muic_data *muic_data,
				u8 vbvolt)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	if (!vbvolt)
		return ret;

	if (muic_data->attached_dev == ATTACHED_DEV_JIG_USB_ON_MUIC) {
		pr_info("%s:%s duplicated(JIG USB ON)\n", MUIC_DEV_NAME,
				__func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb_util(muic_data, ATTACHED_DEV_JIG_USB_ON_MUIC);
	if (ret)
		return ret;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_CABLE_ATTACHED);

	return ret;
}

static void tsu6721_muic_handle_attach(struct tsu6721_muic_data *muic_data,
			enum muic_attached_dev new_dev, int adc, u8 vbvolt)
{
	int ret;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_usb(muic_data);
		}
		break;
	case ATTACHED_DEV_TA_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_charger(muic_data);
		}
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			if ((muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC) &&\
				new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) {
				if (vbvolt == DEV_TYPE3_VBVOLT) break;
			}
			ret = detach_jig_uart_boot_off(muic_data);
		}
		break;

	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_deskdock(muic_data);
		}
		break;
	default:
		break;
	}

	switch (new_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = attach_usb(muic_data, new_dev);
		break;
	case ATTACHED_DEV_TA_MUIC:
		ret = com_to_open_with_vbus(muic_data);
		ret = attach_charger(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = attach_jig_uart_boot_off(muic_data, vbvolt);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		/* call attach_deskdock to wake up the device */
		ret = attach_deskdock(muic_data, new_dev, vbvolt);
		break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		ret = attach_jig_usb_boot_off(muic_data, vbvolt);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = attach_jig_usb_boot_on(muic_data, vbvolt);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		ret = attach_deskdock(muic_data, new_dev, vbvolt);
		break;
	default:
		pr_warn("%s:%s unsupported dev=%d, adc=0x%x, vbus=%c\n",
				MUIC_DEV_NAME, __func__, new_dev, adc,
				(vbvolt ? 'O' : 'X'));
		break;
	}
}

static void tsu6721_muic_handle_detach(struct tsu6721_muic_data *muic_data)
{
	int ret;

	ret = com_to_open(muic_data);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = detach_usb(muic_data);
		break;
	case ATTACHED_DEV_TA_MUIC:
		ret = detach_charger(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = detach_jig_uart_boot_off(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
		ret = detach_deskdock(muic_data);
		break;
	case ATTACHED_DEV_NONE_MUIC:
		pr_info("%s:%s duplicated(NONE)\n", MUIC_DEV_NAME, __func__);
		break;
	case ATTACHED_DEV_UNKNOWN_MUIC:
		pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
		ret = detach_charger(muic_data);
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	default:
		pr_info("%s:%s invalid attached_dev type(%d)\n", MUIC_DEV_NAME,
			__func__, muic_data->attached_dev);
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	}
}

static void tsu6721_muic_detect_dev(struct tsu6721_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;
	struct i2c_client *i2c = muic_data->i2c;
	enum muic_attached_dev new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int intr = MUIC_INTR_DETACH;
	int vbvolt = 0;
	int val1, val2, val3, adc;

	if(switch_data->filter_vps_cb)
		switch_data->filter_vps_cb(muic_data);

	val1 = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_DEV_T1);
	if (val1 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val1);

	val2 = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_DEV_T2);
	if (val2 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val2);

	val3 = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_DEV_T3);
	if (val3 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val3);
	else
		vbvolt = (val3 & DEV_TYPE3_VBVOLT);

	adc = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_ADC);
	pr_info("%s:%s dev[1:0x%x, 2:0x%x, 3:0x%x], adc:0x%x, vbvolt:0x%x\n",
		MUIC_DEV_NAME, __func__, val1, val2, val3, adc, vbvolt);

	/* Attached */
	switch (val1) {
	case DEV_TYPE1_CDP:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_CDP_MUIC;
		break;
	case DEV_TYPE1_USB:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_USB_MUIC;
		break;
	case DEV_TYPE1_DEDICATED_CHG:
	case DEV_TYPE1_T1_T2_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		break;
	default:
		break;
	}

	switch (val2) {
	case DEV_TYPE2_JIG_UART_OFF:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		break;
	case DEV_TYPE2_JIG_USB_OFF:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		break;
	case DEV_TYPE2_JIG_USB_ON:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		break;
	default:
		break;
	}

	if (val3 & DEV_TYPE3_CHG_TYPE)
	{
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
	}

	switch (adc) {
	case ADC_SEND_END ... (ADC_CEA936ATYPE1_CHG - 1):
		pr_warn("%s:%s unsupported ADC(0x%02x)\n", MUIC_DEV_NAME,
				__func__, adc);
		intr = MUIC_INTR_DETACH;
		break;
	case ADC_JIG_USB_OFF:
		if (new_dev != ATTACHED_DEV_JIG_USB_OFF_MUIC) {
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		}
		break;
	case ADC_JIG_USB_ON:
		if (new_dev != ATTACHED_DEV_JIG_USB_ON_MUIC) {
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		}
		break;
	case ADC_JIG_UART_OFF:
		if (new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC) {
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		}
		break;
	case ADC_JIG_UART_ON:
		/* This is the mode to wake up device during factory mode. */
		if (new_dev != ATTACHED_DEV_JIG_UART_ON_MUIC
				&& muic_data->is_factory_start) {
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
		}
		break;
	case ADC_OPEN:
		/* sometimes muic fails to catch JIG_UART_OFF detaching */
		/* double check with ADC */
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC)
		{
			new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
			intr = MUIC_INTR_DETACH;
		}
		break;	
	default:
		break;
	}

	if (val2 & DEV_TYPE2_AV || val3 & DEV_TYPE3_AV_WITH_VBUS)
	{
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_DESKDOCK_MUIC;
	}

	/* WorkAround for CDP and old 2A Charger */

	if(val1 == 0 && val2 == 0 && val3 == 0x02 && adc == ADC_OPEN)
	{
		int cdp_val1, cdp_val3, cdp_vbvolt = 0 , cdp_adc;
		/* in case of CDP, at least wait for 10ms */
		msleep(20);
		cdp_val1 = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_DEV_T1);
		if (cdp_val1 < 0)
			pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, cdp_val1);

		cdp_val3 = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_DEV_T3);
		if (cdp_val3 < 0)
			pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, cdp_val3);
		else
			cdp_vbvolt = (cdp_val3 & DEV_TYPE3_VBVOLT);

		cdp_adc = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_ADC);
		pr_info("%s:%s CDP check dev[1:0x%x, 3:0x%x], adc:0x%x, vbvolt:0x%x\n",
			MUIC_DEV_NAME, __func__, cdp_val1, cdp_val3, cdp_adc, cdp_vbvolt);

		if(cdp_val1 == DEV_TYPE1_CDP)
		{
		    pr_info("%s:%s DEV_TYPE1_CDP cdp_val1=0x%x cdp_val3=0x%x\n", MUIC_DEV_NAME, __func__, cdp_val1, cdp_val3);
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_CDP_MUIC;
			adc = cdp_adc;
			vbvolt = cdp_vbvolt;
		}
	}

	/* HACK */
#ifndef CONFIG_MACH_KMINI
	/* WorkAround for ghost_usb_attach after detaching DESKDOCK */
	/* USB without VBUS is not possible */
	if ( new_dev == ATTACHED_DEV_USB_MUIC && !vbvolt)
	{
			new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
			intr = MUIC_INTR_DETACH;
	}
#endif

	if (intr == MUIC_INTR_ATTACH) {
		tsu6721_muic_handle_attach(muic_data, new_dev, adc, vbvolt);
	} else {
		tsu6721_muic_handle_detach(muic_data);
	}
}

static irqreturn_t tsu6721_muic_irq_thread(int irq, void *data)
{
	struct tsu6721_muic_data *muic_data = data;
	struct i2c_client *i2c = muic_data->i2c;
	int intr1, intr2;

	pr_info("%s:%s irq(%d)\n", MUIC_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);

	tsu6721_disable_interrupt(muic_data);

	/* read and clear interrupt status bits */
	intr1 = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_INT1);
	intr2 = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_INT2);

	if ((intr1 < 0) || (intr2 < 0)) {
		pr_err("%s: err read interrupt status [1:0x%x, 2:0x%x]\n",
				__func__, intr1, intr2);
		goto skip_detect_dev;
	}

	pr_info("%s:%s intr[1:0x%x, 2:0x%x]\n", MUIC_DEV_NAME, __func__,
			intr1, intr2);

	/* Once Deskdock is attached, ignore ADC changed interrupt */
	/* so that the device is safe from potential ghost interrupt */

	if (muic_data->attached_dev == ATTACHED_DEV_DESKDOCK_MUIC && intr2 == 0x4)
		goto skip_detect_dev;

	/* device detection */
	tsu6721_muic_detect_dev(muic_data);

skip_detect_dev:
	tsu6721_enable_interrupt(muic_data);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static void tsu6721_muic_usb_detect(struct work_struct *work)
{
	struct tsu6721_muic_data *muic_data =
		container_of(work, struct tsu6721_muic_data, usb_work.work);
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;

	pr_info("%s:%s pdata->usb_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->usb_path);

	mutex_lock(&muic_data->muic_mutex);
	muic_data->is_usb_ready = true;

	if (pdata->usb_path != MUIC_PATH_USB_CP) {
		if (switch_data->usb_cb) {
			switch ((int)muic_data->attached_dev) {
			case ATTACHED_DEV_USB_MUIC:
			case ATTACHED_DEV_CDP_MUIC:
			case ATTACHED_DEV_JIG_USB_OFF_MUIC:
			case ATTACHED_DEV_JIG_USB_ON_MUIC:
				switch_data->usb_cb(USB_CABLE_ATTACHED);
				break;
			case ATTACHED_DEV_OTG_MUIC:
				switch_data->usb_cb(USB_OTGHOST_ATTACHED);
				break;
			}
		}
	}
	mutex_unlock(&muic_data->muic_mutex);
}

static void tsu6721_muic_init_detect(struct work_struct *work)
{
	struct tsu6721_muic_data *muic_data =
		container_of(work, struct tsu6721_muic_data, init_work.work);

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* MUIC Interrupt On */
	set_int_mask(muic_data, false);

	tsu6721_muic_irq_thread(-1, muic_data);
}

static int tsu6721_init_rev_info(struct tsu6721_muic_data *muic_data)
{
	u8 dev_id;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	dev_id = i2c_smbus_read_byte_data(muic_data->i2c, TSU6721_MUIC_REG_DEVID);
	if (dev_id < 0) {
		pr_err("%s:%s i2c io error(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
		ret = -ENODEV;
	} else {
		muic_data->muic_vendor = (dev_id & 0x7);
		muic_data->muic_version = ((dev_id & 0xF8) >> 3);
		pr_info("%s:%s device found: vendor=0x%x, ver=0x%x\n",
				MUIC_DEV_NAME, __func__, muic_data->muic_vendor,
				muic_data->muic_version);
	}

	return ret;
}

static int tsu6721_muic_reg_init(struct tsu6721_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	unsigned int ctrl = CTRL_MASK;
	int mansw = -1;
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* mask interrupts
	 * (unmask Interrupt Mask 1 OVP_OCP_OTP_DIS, OVP_EN, Detach, Attach,
	 *  Interrupt Mask 2 OTP_EN, ADC_Change, Reserved_Attach, A/V_Change)
	 */
	ret = i2c_smbus_write_byte_data(i2c, TSU6721_MUIC_REG_INTMASK1,
			REG_INTMASK1_VALUE);
	if (ret < 0)
		pr_err("%s: err mask interrupt1(%d)\n", __func__, ret);

	ret = i2c_smbus_write_byte_data(i2c, TSU6721_MUIC_REG_INTMASK2,
			REG_INTMASK2_VALUE);
	if (ret < 0)
		pr_err("%s: err mask interrupt1(%d)\n", __func__, ret);

	ret = i2c_smbus_read_byte_data(i2c, TSU6721_MUIC_REG_MANSW1);
	if (ret < 0)
		pr_err("%s: err read mansw1(%d)\n", __func__, ret);
	else
		mansw = ret;

	/* Raw Data mode On */
	ctrl &= ~CTRL_RAW_DATA_MASK;

	/* Manual Switching Mode */
	if (mansw > 0)
		ctrl &= ~CTRL_MANUAL_SW_MASK;

	ret = i2c_smbus_write_byte_data(i2c, TSU6721_MUIC_REG_CTRL, ctrl);
	if (ret < 0)
		pr_err("%s: err write ctrl(%d)\n", __func__, ret);

	/* BCDv1.2 Timer  default  1.8s -> 0.6s */
	ret = i2c_smbus_write_byte_data(i2c,	TSU6721_MUIC_REG_TIMER_SET,
			0x05);
	if (ret < 0)
		pr_err("%s: err write timer setting(%d)\n", __func__, ret);

	return ret;
}

static int tsu6721_muic_irq_init(struct tsu6721_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!pdata->irq_gpio) {
		pr_warn("%s:%s No interrupt specified\n", MUIC_DEV_NAME,
				__func__);
		return -ENXIO;
	}

	i2c->irq = gpio_to_irq(pdata->irq_gpio);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, NULL,
				tsu6721_muic_irq_thread,
				(IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND),
				"tsu6721-muic", muic_data);
		if (ret < 0) {
			pr_err("%s:%s failed to reqeust IRQ(%d)\n",
					MUIC_DEV_NAME, __func__, i2c->irq);
			return ret;
		}

		ret = enable_irq_wake(i2c->irq);
		if (ret < 0)
			pr_err("%s:%s failed to enable wakeup src\n",
					MUIC_DEV_NAME, __func__);
	}

	return ret;
}

static int tsu6721_muic_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct muic_platform_data *pdata = i2c->dev.platform_data;
	struct tsu6721_muic_data *muic_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: i2c functionality check error\n", __func__);
		ret = -EIO;
		goto err_return;
	}

	muic_data = kzalloc(sizeof(struct tsu6721_muic_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	if (!pdata) {
		pr_err("%s: failed to get i2c platform data\n", __func__);
		ret = -ENODEV;
		goto err_io;
	}
	muic_data->pdata = pdata;

	muic_data->i2c = i2c;
	i2c_set_clientdata(i2c, muic_data);

	muic_data->switch_data = &sec_switch_data;

	/* HACK */
#ifdef CONFIG_MACH_KMINI
	switch_sel = 0x3;
#endif

	if (pdata->init_gpio_cb)
		ret = pdata->init_gpio_cb(get_switch_sel());
	if (ret) {
		pr_err("%s: failed to init gpio(%d)\n", __func__, ret);
		goto fail_init_gpio;
	}

	mutex_init(&muic_data->muic_mutex);

	muic_data->is_factory_start = false;
#if 0
	wake_lock_init(&muic_data->muic_wake_lock, WAKE_LOCK_SUSPEND,
		"muic wake lock");
#endif

	muic_data->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	muic_data->is_usb_ready = false;

	/* set switch device's driver_data */
	dev_set_drvdata(switch_device, muic_data);

	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &tsu6721_muic_group);
	if (ret) {
		pr_err("%s: failed to create tsu6721 muic attribute group\n",
			__func__);
		goto fail;
	}

	ret = tsu6721_init_rev_info(muic_data);
	if (ret) {
		pr_err("%s: failed to init muic rev register(%d)\n", __func__,
			ret);
		goto fail;
	}

	ret = tsu6721_muic_reg_init(muic_data);
	if (ret) {
		pr_err("%s: failed to init muic register(%d)\n", __func__, ret);
		goto fail;
	}

	if (muic_data->switch_data->init_cb)
		muic_data->switch_data->init_cb();

	ret = tsu6721_muic_irq_init(muic_data);
	if (ret) {
		pr_err("%s: failed to init muic irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	/* initial cable detection */
	INIT_DELAYED_WORK(&muic_data->init_work, tsu6721_muic_init_detect);
	schedule_delayed_work(&muic_data->init_work, msecs_to_jiffies(3000));

	INIT_DELAYED_WORK(&muic_data->usb_work, tsu6721_muic_usb_detect);
	schedule_delayed_work(&muic_data->usb_work, msecs_to_jiffies(13000));

	return 0;

fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, muic_data);
fail:
#if 0
	wake_lock_destroy(&info->muic_wake_lock);
#endif
	mutex_destroy(&muic_data->muic_mutex);
fail_init_gpio:
	i2c_set_clientdata(i2c, NULL);
err_io:
	kfree(muic_data);
err_return:
	return ret;
}

static int __devexit tsu6721_muic_remove(struct i2c_client *i2c)
{
	struct tsu6721_muic_data *muic_data = i2c_get_clientdata(i2c);
	sysfs_remove_group(&switch_device->kobj, &tsu6721_muic_group);

	if (muic_data) {
		pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
		cancel_delayed_work(&muic_data->init_work);
		cancel_delayed_work(&muic_data->usb_work);
		disable_irq_wake(muic_data->i2c->irq);
		free_irq(muic_data->i2c->irq, muic_data);
#if 0
		wake_lock_destroy(&info->muic_wake_lock);
#endif
		mutex_destroy(&muic_data->muic_mutex);
		i2c_set_clientdata(muic_data->i2c, NULL);
		kfree(muic_data);
	}
	return 0;
}

static const struct i2c_device_id tsu6721_i2c_id[] = {
	{ MUIC_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, tsu6721_i2c_id);

static void tsu6721_muic_shutdown(struct i2c_client *i2c)
{
	struct tsu6721_muic_data *muic_data = i2c_get_clientdata(i2c);
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	if (!muic_data->i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	pr_info("%s:%s open D+,D-,V_bus line\n", MUIC_DEV_NAME, __func__);
	ret = com_to_open(muic_data);
	if (ret < 0)
		pr_err("%s:%s fail to open mansw1 reg\n", MUIC_DEV_NAME,
				__func__);

	pr_info("%s:%s muic auto detection enable\n", MUIC_DEV_NAME, __func__);
	ret = set_manual_sw(muic_data, true);
	if (ret < 0) {
		pr_err("%s:%s fail to update reg\n", MUIC_DEV_NAME, __func__);
		return;
	}
}

static struct i2c_driver tsu6721_muic_driver = {
	.driver		= {
		.name	= MUIC_DEV_NAME,
	},
	.probe		= tsu6721_muic_probe,
	.remove		= __devexit_p(tsu6721_muic_remove),
	.shutdown	= tsu6721_muic_shutdown,
	.id_table	= tsu6721_i2c_id,
};

static int __init tsu6721_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return i2c_add_driver(&tsu6721_muic_driver);
}
module_init(tsu6721_muic_init);

static void __exit tsu6721_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	i2c_del_driver(&tsu6721_muic_driver);
}
module_exit(tsu6721_muic_exit);

MODULE_AUTHOR("Seung-Jin Hahn <sjin.hahn@samsung.com>");
MODULE_DESCRIPTION("TSU6721 USB Switch driver");
MODULE_LICENSE("GPL");
