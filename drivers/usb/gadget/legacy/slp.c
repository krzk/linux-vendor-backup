/*
 * Gadget Driver for SLP based on Android
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *         Benoit Goby <benoit@android.com>
 * Modified : Jaewon Kim <jaewon02.kim@samsung.com>
 *
 * Heavily based on android.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/platform_device.h>

#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>

#include "gadget_chips.h"

#include "../function/f_sdb.c"
#include "../function/f_acm.c"
#define USB_ETH_RNDIS y
#define USB_FRNDIS_INCLUDED y
#include "../function/f_rndis.c"
#include "../function/rndis.c"
#include "../function/u_ether.c"

/* Default vendor and product IDs, overridden by userspace */
#define VENDOR_ID		0x04E8
#define PRODUCT_ID		0x6860
#define USB_MODE_VERSION        "1.1"

static const char longname[] = "Gadget SLP";

enum slp_multi_config_id {
	USB_CONFIGURATION_1 = 1,
	USB_CONFIGURATION_2 = 2,
	USB_CONFIGURATION_DUAL = 0xFF,
};

struct slp_multi_usb_function {
	char *name;
	void *config;

	struct device *dev;
	char *dev_name;
	struct device_attribute **attributes;

	/* for slp_multi_dev.funcs_fconf */
	struct list_head fconf_list;
	/* for slp_multi_dev.funcs_sconf */
	struct list_head sconf_list;

	/* Optional: initialization during gadget bind */
	int (*init)(struct slp_multi_usb_function *, struct usb_composite_dev *);
	/* Optional: cleanup during gadget unbind */
	void (*cleanup)(struct slp_multi_usb_function *);
	/* Optional: called when the function is added the list of
	 *		enabled functions */
	void (*enable)(struct slp_multi_usb_function *);
	/* Optional: called when it is removed */
	void (*disable)(struct slp_multi_usb_function *);

	int (*bind_config)(struct slp_multi_usb_function *,
			   struct usb_configuration *);

	/* Optional: called when the configuration is removed */
	void (*unbind_config)(struct slp_multi_usb_function *,
			      struct usb_configuration *);
	/* Optional: handle ctrl requests before the device is configured */
	int (*ctrlrequest)(struct slp_multi_usb_function *,
					struct usb_composite_dev *,
					const struct usb_ctrlrequest *);
};

struct slp_multi_dev {
	struct slp_multi_usb_function **functions;

	/* for each configuration control */
	struct list_head funcs_fconf;
	struct list_head funcs_sconf;

	struct usb_composite_dev *cdev;
	struct device *dev;

	bool enabled;
	bool dual_config;
	int disable_depth;
	struct mutex mutex;
	bool connected;
	bool sw_connected;
	char ffs_aliases[256];
};

static struct class *slp_multi_class;
static struct slp_multi_dev *_slp_multi_dev;
static int slp_multi_bind_config(struct usb_configuration *c);
static void slp_multi_unbind_config(struct usb_configuration *c);

/* string IDs are assigned dynamically */
#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

static char manufacturer_string[256];
static char product_string[256];
static char serial_string[256];

/* String Table */
static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer_string,
	[STRING_PRODUCT_IDX].s = product_string,
	[STRING_SERIAL_IDX].s = serial_string,
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_device_descriptor device_desc = {
	.bLength              = sizeof(device_desc),
	.bDescriptorType      = USB_DT_DEVICE,
	.bcdUSB               = __constant_cpu_to_le16(0x200),
	.bDeviceClass         = USB_CLASS_PER_INTERFACE,
	.idVendor             = __constant_cpu_to_le16(VENDOR_ID),
	.idProduct            = __constant_cpu_to_le16(PRODUCT_ID),
	.bcdDevice            = __constant_cpu_to_le16(0xffff),
	.bNumConfigurations   = 1,
};

static struct usb_configuration first_config_driver = {
	.label			= "slp_first_config",
	.unbind			= slp_multi_unbind_config,
	.bConfigurationValue	= USB_CONFIGURATION_1,
	.bmAttributes		= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.MaxPower		= 500, /* 500ma */
};

static struct usb_configuration second_config_driver = {
	.label			= "slp_second_config",
	.unbind			= slp_multi_unbind_config,
	.bConfigurationValue	= USB_CONFIGURATION_2,
	.bmAttributes		= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.MaxPower		= 500, /* 500ma */
};

static void slp_multi_enable(struct slp_multi_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;

	if (WARN_ON(!dev->disable_depth))
		return;

	if (--dev->disable_depth == 0) {
		if (!dev->dual_config)
			usb_add_config(cdev, &first_config_driver,
					slp_multi_bind_config);
		else
			usb_add_config(cdev, &second_config_driver,
					slp_multi_bind_config);

		usb_gadget_connect(cdev->gadget);
	}
}

static void slp_multi_disable(struct slp_multi_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;

	if (dev->disable_depth++ == 0) {
		usb_gadget_disconnect(cdev->gadget);
		/* Cancel pending control requests */
		usb_ep_dequeue(cdev->gadget->ep0, cdev->req);
		if (!dev->dual_config)
			usb_remove_config(cdev, &first_config_driver);
		else
			usb_remove_config(cdev, &second_config_driver);
	}
}

/*-------------------------------------------------------------------------*/
/* Supported functions initialization */
struct sdb_data {
	bool opened;
	bool enabled;
};

static int
sdb_function_init(struct slp_multi_usb_function *f,
		struct usb_composite_dev *cdev)
{
	f->config = kzalloc(sizeof(struct sdb_data), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;

	return sdb_setup();
}

static void sdb_function_cleanup(struct slp_multi_usb_function *f)
{
	sdb_cleanup();
	kfree(f->config);
}

static int
sdb_function_bind_config(struct slp_multi_usb_function *f,
		struct usb_configuration *c)
{
	return sdb_bind_config(c);
}

static void sdb_slp_multi_function_enable(struct slp_multi_usb_function *f)
{
	struct slp_multi_dev *dev = _slp_multi_dev;
	struct sdb_data *data = f->config;

	data->enabled = true;

	/* Disable the gadget until sdbd is ready */
	if (!data->opened)
		slp_multi_disable(dev);
}

static void sdb_slp_multi_function_disable(struct slp_multi_usb_function *f)
{
	struct slp_multi_dev *dev = _slp_multi_dev;
	struct sdb_data *data = f->config;

	data->enabled = false;

	/* Balance the disable that was called in closed_callback */
	if (!data->opened)
		slp_multi_enable(dev);
}

static struct slp_multi_usb_function sdb_function = {
	.name		= "sdb",
	.enable		= sdb_slp_multi_function_enable,
	.disable	= sdb_slp_multi_function_disable,
	.init		= sdb_function_init,
	.cleanup	= sdb_function_cleanup,
	.bind_config	= sdb_function_bind_config,
};

static void sdb_ready_callback(void)
{
	struct slp_multi_dev *dev = _slp_multi_dev;
	struct sdb_data *data = sdb_function.config;

	mutex_lock(&dev->mutex);

	data->opened = true;

	if (data->enabled)
		slp_multi_enable(dev);

	mutex_unlock(&dev->mutex);
}

static void sdb_closed_callback(void)
{
	struct slp_multi_dev *dev = _slp_multi_dev;
	struct sdb_data *data = sdb_function.config;

	mutex_lock(&dev->mutex);

	data->opened = false;

	if (data->enabled)
		slp_multi_disable(dev);

	mutex_unlock(&dev->mutex);
}

#define MAX_ACM_INSTANCES 4
struct acm_function_config {
	int instances;
	int instances_on;
	struct usb_function *f_acm[MAX_ACM_INSTANCES];
	struct usb_function_instance *f_acm_inst[MAX_ACM_INSTANCES];
};

static int
acm_function_init(struct slp_multi_usb_function *f,
		struct usb_composite_dev *cdev)
{
	int i;
	int ret;
	struct acm_function_config *config;

	config = kzalloc(sizeof(struct acm_function_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	f->config = config;

	for (i = 0; i < MAX_ACM_INSTANCES; i++) {
		config->f_acm_inst[i] = usb_get_function_instance("acm");
		if (IS_ERR(config->f_acm_inst[i])) {
			ret = PTR_ERR(config->f_acm_inst[i]);
			goto err_usb_get_function_instance;
		}
		config->f_acm[i] = usb_get_function(config->f_acm_inst[i]);
		if (IS_ERR(config->f_acm[i])) {
			ret = PTR_ERR(config->f_acm[i]);
			goto err_usb_get_function;
		}
	}
	return 0;
err_usb_get_function_instance:
	while (i-- > 0) {
		usb_put_function(config->f_acm[i]);
err_usb_get_function:
		usb_put_function_instance(config->f_acm_inst[i]);
	}
	return ret;
}

static void acm_function_cleanup(struct slp_multi_usb_function *f)
{
	int i;
	struct acm_function_config *config = f->config;

	for (i = 0; i < MAX_ACM_INSTANCES; i++) {
		usb_put_function(config->f_acm[i]);
		usb_put_function_instance(config->f_acm_inst[i]);
	}
	kfree(f->config);
	f->config = NULL;
}

static int
acm_function_bind_config(struct slp_multi_usb_function *f,
		struct usb_configuration *c)
{
	int i;
	int ret = 0;
	struct acm_function_config *config = f->config;

	config->instances_on = config->instances;
	for (i = 0; i < config->instances_on; i++) {
		ret = usb_add_function(c, config->f_acm[i]);
		if (ret) {
			pr_err("Could not bind acm%u config\n", i);
			goto err_usb_add_function;
		}
	}

	return 0;

err_usb_add_function:
	while (i-- > 0)
		usb_remove_function(c, config->f_acm[i]);
	return ret;
}

static void acm_function_unbind_config(struct slp_multi_usb_function *f,
				       struct usb_configuration *c)
{
	int i;
	struct acm_function_config *config = f->config;

	for (i = 0; i < config->instances_on; i++)
		usb_remove_function(c, config->f_acm[i]);
}

static ssize_t acm_instances_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct acm_function_config *config = f->config;

	return sprintf(buf, "%d\n", config->instances);
}

static ssize_t acm_instances_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct acm_function_config *config = f->config;
	int value;

	sscanf(buf, "%d", &value);
	if (value > MAX_ACM_INSTANCES)
		value = MAX_ACM_INSTANCES;
	config->instances = value;
	return size;
}

static DEVICE_ATTR(instances, S_IRUGO | S_IWUSR, acm_instances_show,
						 acm_instances_store);
static struct device_attribute *acm_function_attributes[] = {
	&dev_attr_instances,
	NULL
};

static struct slp_multi_usb_function acm_function = {
	.name		= "acm",
	.init		= acm_function_init,
	.cleanup	= acm_function_cleanup,
	.bind_config	= acm_function_bind_config,
	.unbind_config	= acm_function_unbind_config,
	.attributes	= acm_function_attributes,
};

struct rndis_function_config {
	u8 ethaddr[ETH_ALEN];
	u32 vendorID;
	char manufacturer[256];
	bool wceis;
	u8 rndis_string_defs0_id;
	struct usb_function *f_rndis;
	struct usb_function_instance *fi_rndis;
};
static char host_addr_string[18];

static int
rndis_function_init(struct slp_multi_usb_function *f,
		struct usb_composite_dev *cdev)
{
	struct rndis_function_config *config;
	int status, i;

	config = kzalloc(sizeof(struct rndis_function_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	config->fi_rndis = usb_get_function_instance("rndis");
	if (IS_ERR(config->fi_rndis)) {
		status = PTR_ERR(config->fi_rndis);
		goto rndis_alloc_inst_error;
	}

	/* maybe allocate device-global string IDs */
	if (rndis_string_defs[0].id == 0) {

		/* control interface label */
		status = usb_string_id(cdev);
		if (status < 0)
			goto rndis_init_error;
		config->rndis_string_defs0_id = status;
		rndis_string_defs[0].id = status;
		rndis_control_intf.iInterface = status;

		/* data interface label */
		status = usb_string_id(cdev);
		if (status < 0)
			goto rndis_init_error;
		rndis_string_defs[1].id = status;
		rndis_data_intf.iInterface = status;

		/* IAD iFunction label */
		status = usb_string_id(cdev);
		if (status < 0)
			goto rndis_init_error;
		rndis_string_defs[2].id = status;
		rndis_iad_descriptor.iFunction = status;
	}

	/* create a fake MAC address from our serial number. */
	for (i = 0; (i < 256) && serial_string[i]; i++) {
		/* XOR the USB serial across the remaining bytes */
		config->ethaddr[i % (ETH_ALEN - 1) + 1] ^= serial_string[i];
	}
	config->ethaddr[0] &= 0xfe;	/* clear multicast bit */
	config->ethaddr[0] |= 0x02;	/* set local assignment bit (IEEE802) */

	snprintf(host_addr_string, sizeof(host_addr_string),
		"%02x:%02x:%02x:%02x:%02x:%02x",
		config->ethaddr[0],	config->ethaddr[1],
		config->ethaddr[2],	config->ethaddr[3],
		config->ethaddr[4], config->ethaddr[5]);

	f->config = config;
	return 0;

 rndis_init_error:
	usb_put_function_instance(config->fi_rndis);
rndis_alloc_inst_error:
	kfree(config);
	return status;
}

static void rndis_function_cleanup(struct slp_multi_usb_function *f)
{
	struct rndis_function_config *rndis = f->config;

	usb_put_function_instance(rndis->fi_rndis);
	kfree(rndis);
	f->config = NULL;
}

static int rndis_function_bind_config(struct slp_multi_usb_function *f,
				      struct usb_configuration *c)
{
	int ret = -EINVAL;
	struct rndis_function_config *rndis = f->config;
	struct f_rndis_opts *rndis_opts;
	struct net_device *net;

	if (!rndis) {
		dev_err(f->dev, "error rndis_pdata is null\n");
		return ret;
	}

	rndis_opts =
		container_of(rndis->fi_rndis, struct f_rndis_opts, func_inst);
	net = rndis_opts->net;

	gether_set_qmult(net, QMULT_DEFAULT);
	gether_set_host_addr(net, host_addr_string);
	gether_set_dev_addr(net, host_addr_string);
	rndis_opts->vendor_id = rndis->vendorID;
	rndis_opts->manufacturer = rndis->manufacturer;

	if (rndis->wceis) {
		/* "Wireless" RNDIS; auto-detected by Windows */
		rndis_iad_descriptor.bFunctionClass =
		    USB_CLASS_WIRELESS_CONTROLLER;
		rndis_iad_descriptor.bFunctionSubClass = 0x01;
		rndis_iad_descriptor.bFunctionProtocol = 0x03;
		rndis_control_intf.bInterfaceClass =
		    USB_CLASS_WIRELESS_CONTROLLER;
		rndis_control_intf.bInterfaceSubClass = 0x01;
		rndis_control_intf.bInterfaceProtocol = 0x03;
	}

	/* Android team reset "rndis_string_defs[0].id" when RNDIS unbinded
	 * in f_rndis.c but, that makes failure of rndis_bind_config() by
	 * the overflow of "next_string_id" value in usb_string_id().
	 * So, Android team also reset "next_string_id" value in android.c
	 * but SLP does not reset "next_string_id" value. And we decided to
	 * re-update "rndis_string_defs[0].id" by old value.
	 * 20120224 yongsul96.oh@samsung.com
	 */
	if (rndis_string_defs[0].id == 0)
		rndis_string_defs[0].id = rndis->rndis_string_defs0_id;

	rndis->f_rndis = usb_get_function(rndis->fi_rndis);
	if (IS_ERR(rndis->f_rndis))
		return PTR_ERR(rndis->f_rndis);

	ret = usb_add_function(c, rndis->f_rndis);
	if (ret < 0) {
		usb_put_function(rndis->f_rndis);
		dev_err(f->dev, "rndis_bind_config failed(ret:%d)\n", ret);
	}

	return ret;
}

static void rndis_function_unbind_config(struct slp_multi_usb_function *f,
					 struct usb_configuration *c)
{
	struct rndis_function_config *rndis = f->config;

	usb_put_function(rndis->f_rndis);
}

static ssize_t rndis_manufacturer_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	return snprintf(buf, PAGE_SIZE, "%s\n", config->manufacturer);
}

static ssize_t rndis_manufacturer_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	if ((size >= sizeof(config->manufacturer)) ||
		(sscanf(buf, "%s", config->manufacturer) != 1))
		return -EINVAL;

	return size;
}

static DEVICE_ATTR(manufacturer, S_IRUGO | S_IWUSR, rndis_manufacturer_show,
		   rndis_manufacturer_store);

static ssize_t rndis_wceis_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	return snprintf(buf, PAGE_SIZE, "%d\n", config->wceis);
}

static ssize_t rndis_wceis_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;

	if (sscanf(buf, "%d", &value) == 1) {
		config->wceis = value;
		return size;
	}
	return -EINVAL;
}

static DEVICE_ATTR(wceis, S_IRUGO | S_IWUSR, rndis_wceis_show,
		   rndis_wceis_store);

static ssize_t rndis_ethaddr_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *rndis = f->config;

	return snprintf(buf, PAGE_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		       rndis->ethaddr[0], rndis->ethaddr[1], rndis->ethaddr[2],
		       rndis->ethaddr[3], rndis->ethaddr[4], rndis->ethaddr[5]);
}

static DEVICE_ATTR(ethaddr, S_IRUGO, rndis_ethaddr_show,
		   NULL);

static ssize_t rndis_vendorID_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	return snprintf(buf, PAGE_SIZE, "%04x\n", config->vendorID);
}

static ssize_t rndis_vendorID_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;

	if (sscanf(buf, "%04x", &value) == 1) {
		config->vendorID = value;
		return size;
	}
	return -EINVAL;
}

static DEVICE_ATTR(vendorID, S_IRUGO | S_IWUSR, rndis_vendorID_show,
		   rndis_vendorID_store);

static struct device_attribute *rndis_function_attributes[] = {
	&dev_attr_manufacturer,
	&dev_attr_wceis,
	&dev_attr_ethaddr,
	&dev_attr_vendorID,
	NULL
};

static struct slp_multi_usb_function rndis_function = {
	.name = "rndis",
	.init = rndis_function_init,
	.cleanup = rndis_function_cleanup,
	.bind_config = rndis_function_bind_config,
	.unbind_config = rndis_function_unbind_config,
	.attributes = rndis_function_attributes,
};

static struct slp_multi_usb_function *supported_functions[] = {
	&sdb_function,
	&acm_function,
	&rndis_function,
	NULL
};

static int slp_multi_init_functions(struct slp_multi_usb_function **functions,
				  struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *dev = _slp_multi_dev;
	struct slp_multi_usb_function *f;
	struct device_attribute **attrs;
	struct device_attribute *attr;
	int err;
	int index = 0;

	for (; (f = *functions++); index++) {
		f->dev_name = kasprintf(GFP_KERNEL, "f_%s", f->name);
		f->dev = device_create(slp_multi_class, dev->dev,
				MKDEV(0, index), f, f->dev_name);
		if (IS_ERR(f->dev)) {
			pr_err("%s: Failed to create dev %s", __func__,
							f->dev_name);
			err = PTR_ERR(f->dev);
			goto err_create;
		}

		if (f->init) {
			err = f->init(f, cdev);
			if (err) {
				pr_err("%s: Failed to init %s", __func__,
								f->name);
				goto err_out;
			}
		}

		attrs = f->attributes;
		if (attrs) {
			while ((attr = *attrs++) && !err)
				err = device_create_file(f->dev, attr);
		}
		if (err) {
			pr_err("%s: Failed to create function %s attributes",
					__func__, f->name);
			goto err_out;
		}
	}
	return 0;

err_out:
	device_destroy(slp_multi_class, f->dev->devt);
err_create:
	kfree(f->dev_name);

	return err;
}

static void slp_multi_cleanup_functions(struct slp_multi_usb_function **functions)
{
	struct slp_multi_usb_function *f;

	while (*functions) {
		f = *functions++;

		if (f->dev) {
			device_destroy(slp_multi_class, f->dev->devt);
			kfree(f->dev_name);
		}

		if (f->cleanup)
			f->cleanup(f);
	}
}

static int
slp_multi_bind_enabled_functions(struct slp_multi_dev *dev,
			       struct usb_configuration *c)
{
	struct slp_multi_usb_function *f = NULL;
	int ret;

	if (c->bConfigurationValue == USB_CONFIGURATION_1) {
		list_for_each_entry(f, &dev->funcs_fconf, fconf_list) {
			ret = f->bind_config(f, c);
			if (ret) {
				pr_err("%s: %s failed", __func__, f->name);
				return ret;
			}
		}
	} else if (c->bConfigurationValue == USB_CONFIGURATION_2) {
		list_for_each_entry(f, &dev->funcs_sconf, sconf_list) {
			ret = f->bind_config(f, c);
			if (ret) {
				pr_err("%s: %s failed", __func__, f->name);
				return ret;
			}
		}
	} else {
		pr_err("%s: %s Not supported configuration(%d)", __func__,
					f->name, c->bConfigurationValue);
		return -EINVAL;
	}

	return 0;
}

static void
slp_multi_unbind_enabled_functions(struct slp_multi_dev *dev,
			       struct usb_configuration *c)
{
	struct slp_multi_usb_function *f;

	if (c->bConfigurationValue == USB_CONFIGURATION_1) {
		list_for_each_entry(f, &dev->funcs_fconf, fconf_list) {
			if (f->unbind_config)
				f->unbind_config(f, c);
		}
	} else if (c->bConfigurationValue == USB_CONFIGURATION_2) {
		list_for_each_entry(f, &dev->funcs_sconf, sconf_list) {
			if (f->unbind_config)
				f->unbind_config(f, c);
		}
	}
}

static int slp_multi_enable_function(struct slp_multi_dev *dev,
					char *name, int type)
{
	struct slp_multi_usb_function **functions = dev->functions;
	struct slp_multi_usb_function *f;
	while ((f = *functions++)) {
		if (!strcmp(name, f->name)) {
			if (type == USB_CONFIGURATION_1)
				list_add_tail(&f->fconf_list,
						&dev->funcs_fconf);
			else if (type == USB_CONFIGURATION_2)
				list_add_tail(&f->sconf_list,
						&dev->funcs_sconf);
			return 0;
		}
	}
	return -EINVAL;
}

/*-------------------------------------------------------------------------*/
/* /sys/class/slp_multi_usb/slp_multi%d/ interface */

static ssize_t
funcs_sconf_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	struct slp_multi_dev *dev = dev_get_drvdata(pdev);
	struct slp_multi_usb_function *f;
	char *buff = buf;

	mutex_lock(&dev->mutex);

	list_for_each_entry(f, &dev->funcs_sconf, sconf_list)
		buff += sprintf(buff, "%s,", f->name);

	mutex_unlock(&dev->mutex);

	if (buff != buf)
		*(buff-1) = '\n';
	return buff - buf;
}

static ssize_t
funcs_sconf_store(struct device *pdev, struct device_attribute *attr,
			       const char *buff, size_t size)
{
	struct slp_multi_dev *dev = dev_get_drvdata(pdev);
	char *name;
	char buf[256], *b;
	int err;

	mutex_lock(&dev->mutex);

	if (dev->enabled) {
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	INIT_LIST_HEAD(&dev->funcs_sconf);

	strlcpy(buf, buff, sizeof(buf));
	b = strim(buf);

	while (b) {
		name = strsep(&b, ",");
		if (!name)
			continue;

		err = slp_multi_enable_function(dev, name, USB_CONFIGURATION_2);
		if (err)
			pr_err("slp_multi_usb: Cannot enable '%s' (%d)",
							   name, err);
	}

	mutex_unlock(&dev->mutex);

	return size;
}

static ssize_t
funcs_fconf_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	struct slp_multi_dev *dev = dev_get_drvdata(pdev);
	struct slp_multi_usb_function *f;
	char *buff = buf;

	mutex_lock(&dev->mutex);

	list_for_each_entry(f, &dev->funcs_fconf, fconf_list)
		buff += sprintf(buff, "%s,", f->name);

	mutex_unlock(&dev->mutex);

	if (buff != buf)
		*(buff-1) = '\n';
	return buff - buf;
}

static ssize_t
funcs_fconf_store(struct device *pdev, struct device_attribute *attr,
			       const char *buff, size_t size)
{
	struct slp_multi_dev *dev = dev_get_drvdata(pdev);
	char *name;
	char buf[256], *b;
	int err;

	mutex_lock(&dev->mutex);

	if (dev->enabled) {
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	INIT_LIST_HEAD(&dev->funcs_fconf);

	strlcpy(buf, buff, sizeof(buf));
	b = strim(buf);

	while (b) {
		name = strsep(&b, ",");
		if (!name)
			continue;

		err = slp_multi_enable_function(dev, name, USB_CONFIGURATION_1);
		if (err)
			pr_err("slp_multi_usb: Cannot enable '%s' (%d)",
							   name, err);
	}

	mutex_unlock(&dev->mutex);

	return size;
}

static ssize_t enable_show(struct device *pdev, struct device_attribute *attr,
			   char *buf)
{
	struct slp_multi_dev *dev = dev_get_drvdata(pdev);

	return sprintf(buf, "%d\n", dev->enabled);
}

static ssize_t enable_store(struct device *pdev, struct device_attribute *attr,
			    const char *buff, size_t size)
{
	struct slp_multi_dev *dev = dev_get_drvdata(pdev);
	struct usb_composite_dev *cdev = dev->cdev;
	struct slp_multi_usb_function *f;
	int enabled = 0;

	if (!cdev)
		return -ENODEV;

	mutex_lock(&dev->mutex);

	sscanf(buff, "%d", &enabled);
	if (enabled && !dev->enabled) {
		/*
		 * Update values in composite driver's copy of
		 * device descriptor.
		 */
		cdev->desc.idVendor = device_desc.idVendor;
		cdev->desc.idProduct = device_desc.idProduct;
		cdev->desc.bcdDevice = device_desc.bcdDevice;
		cdev->desc.bDeviceClass = device_desc.bDeviceClass;
		cdev->desc.bDeviceSubClass = device_desc.bDeviceSubClass;
		cdev->desc.bDeviceProtocol = device_desc.bDeviceProtocol;

		list_for_each_entry(f, &dev->funcs_fconf, fconf_list) {
			if (!strcmp(f->name, "acm"))
				cdev->desc.bcdDevice = cpu_to_le16(0x0400);
		}

		list_for_each_entry(f, &dev->funcs_sconf, sconf_list) {
			if (!strcmp(f->name, "acm"))
				cdev->desc.bcdDevice = cpu_to_le16(0x400);
			dev->dual_config = true;
		}

		slp_multi_enable(dev);
		dev->enabled = true;
	} else if (!enabled && dev->enabled) {
		slp_multi_disable(dev);
		dev->enabled = false;
		dev->dual_config = false;
	} else {
		pr_err("slp_multi_usb: already %s\n",
				dev->enabled ? "enabled" : "disabled");
	}

	mutex_unlock(&dev->mutex);
	return size;
}

static ssize_t state_show(struct device *pdev, struct device_attribute *attr,
			   char *buf)
{
	struct slp_multi_dev *dev = dev_get_drvdata(pdev);
	struct usb_composite_dev *cdev = dev->cdev;
	char *state = "DISCONNECTED";
	unsigned long flags;

	if (!cdev)
		goto out;

	spin_lock_irqsave(&cdev->lock, flags);
	if (cdev->config)
		state = "CONFIGURED";
	else if (dev->connected)
		state = "CONNECTED";
	spin_unlock_irqrestore(&cdev->lock, flags);
out:
	return sprintf(buf, "%s\n", state);
}

#define DESCRIPTOR_ATTR(field, format_string)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	return sprintf(buf, format_string, device_desc.field);		\
}									\
static ssize_t								\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	int value;							\
	if (sscanf(buf, format_string, &value) == 1) {			\
		device_desc.field = value;				\
		return size;						\
	}								\
	return -1;							\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);

#define DESCRIPTOR_STRING_ATTR(field, buffer)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	return sprintf(buf, "%s", buffer);				\
}									\
static ssize_t								\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	if (size >= sizeof(buffer))					\
		return -EINVAL;						\
	return strlcpy(buffer, buf, sizeof(buffer));			\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);


DESCRIPTOR_ATTR(idVendor, "%04x\n")
DESCRIPTOR_ATTR(idProduct, "%04x\n")
DESCRIPTOR_ATTR(bcdDevice, "%04x\n")
DESCRIPTOR_ATTR(bDeviceClass, "%d\n")
DESCRIPTOR_ATTR(bDeviceSubClass, "%d\n")
DESCRIPTOR_ATTR(bDeviceProtocol, "%d\n")
DESCRIPTOR_STRING_ATTR(iManufacturer, manufacturer_string)
DESCRIPTOR_STRING_ATTR(iProduct, product_string)
DESCRIPTOR_STRING_ATTR(iSerial, serial_string)

static DEVICE_ATTR(funcs_fconf, S_IRUGO | S_IWUSR,
				funcs_fconf_show, funcs_fconf_store);
static DEVICE_ATTR(funcs_sconf, S_IRUGO | S_IWUSR,
				funcs_sconf_show, funcs_sconf_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, enable_show, enable_store);
static DEVICE_ATTR(state, S_IRUGO, state_show, NULL);

static struct device_attribute *slp_multi_usb_attributes[] = {
	&dev_attr_idVendor,
	&dev_attr_idProduct,
	&dev_attr_bcdDevice,
	&dev_attr_bDeviceClass,
	&dev_attr_bDeviceSubClass,
	&dev_attr_bDeviceProtocol,
	&dev_attr_iManufacturer,
	&dev_attr_iProduct,
	&dev_attr_iSerial,
	&dev_attr_funcs_fconf,
	&dev_attr_funcs_sconf,
	&dev_attr_enable,
	&dev_attr_state,
	NULL
};

/*-------------------------------------------------------------------------*/
/* Composite driver */

static int slp_multi_bind_config(struct usb_configuration *c)
{
	struct slp_multi_dev *dev = _slp_multi_dev;
	int ret = 0;

	ret = slp_multi_bind_enabled_functions(dev, c);
	if (ret)
		return ret;

	return 0;
}

static void slp_multi_unbind_config(struct usb_configuration *c)
{
	struct slp_multi_dev *dev = _slp_multi_dev;

	slp_multi_unbind_enabled_functions(dev, c);
}

static int slp_multi_bind(struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *dev = _slp_multi_dev;
	struct usb_gadget	*gadget = cdev->gadget;
	int			id, ret;

	/*
	 * Start disconnected. Userspace will connect the gadget once
	 * it is done configuring the functions.
	 */
	usb_gadget_disconnect(gadget);

	ret = slp_multi_init_functions(dev->functions, cdev);
	if (ret)
		return ret;

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_MANUFACTURER_IDX].id = id;
	device_desc.iManufacturer = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_PRODUCT_IDX].id = id;
	device_desc.iProduct = id;

	/* Default strings - should be updated by userspace */
	strncpy(manufacturer_string, "Samsung", sizeof(manufacturer_string)-1);
	strncpy(product_string, "TIZEN", sizeof(product_string) - 1);
	snprintf(serial_string, 18, "%s", "01234TEST");

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;

	usb_gadget_set_selfpowered(gadget);
	dev->cdev = cdev;

	return 0;
}

static int slp_multi_usb_unbind(struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *dev = _slp_multi_dev;

	slp_multi_cleanup_functions(dev->functions);
	return 0;
}

/* HACK: slp_multi needs to override setup for accessory to work */
static int (*composite_setup_func)(struct usb_gadget *gadget, const struct usb_ctrlrequest *c);

static int
slp_multi_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *c)
{
	struct slp_multi_dev		*dev = _slp_multi_dev;
	struct usb_composite_dev	*cdev = get_gadget_data(gadget);
	struct usb_request		*req = cdev->req;
	struct slp_multi_usb_function	*f;
	struct slp_multi_usb_function **functions = dev->functions;
	unsigned long flags;
	int value = -EOPNOTSUPP;
	int index = 0;

	req->zero = 0;
	req->length = 0;
	gadget->ep0->driver_data = cdev;

	for (; (f = *functions++); index++) {
		if (f->ctrlrequest) {
			value = f->ctrlrequest(f, cdev, c);
			if (value >= 0)
				break;
		}
	}

	/* Special case the accessory function.
	 * It needs to handle control requests before it is enabled.
	 */
	if (value < 0)
		value = composite_setup_func(gadget, c);

	spin_lock_irqsave(&cdev->lock, flags);
	if (!dev->connected) {
		dev->connected = 1;
	}

	spin_unlock_irqrestore(&cdev->lock, flags);

	return value;
}

static void slp_multi_disconnect(struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *dev = _slp_multi_dev;

	/* accessory HID support can be active while the
	   accessory function is not actually enabled,
	   so we need to inform it when we are disconnected.
	 */
	dev->connected = 0;
}

static struct usb_composite_driver slp_multi_usb_driver = {
	.name		= "slp_multi_composite",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.bind		= slp_multi_bind,
	.unbind		= slp_multi_usb_unbind,
	.disconnect	= slp_multi_disconnect,
	.max_speed	= USB_SPEED_HIGH,
};

static int slp_multi_create_device(struct slp_multi_dev *dev)
{
	struct device_attribute **attrs = slp_multi_usb_attributes;
	struct device_attribute *attr;
	int err;

	dev->dev = device_create(slp_multi_class, NULL,
					MKDEV(0, 0), NULL, "usb0");
	if (IS_ERR(dev->dev))
		return PTR_ERR(dev->dev);

	dev_set_drvdata(dev->dev, dev);

	while ((attr = *attrs++)) {
		err = device_create_file(dev->dev, attr);
		if (err) {
			device_destroy(slp_multi_class, dev->dev->devt);
			return err;
		}
	}
	return 0;
}
static CLASS_ATTR_STRING(version, S_IRUSR | S_IRGRP | S_IROTH,
					USB_MODE_VERSION);

static int __init init(void)
{
	struct slp_multi_dev *dev;
	int err;


	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		return -ENOMEM;
	}

	dev->disable_depth = 1;
	dev->functions = supported_functions;
	_slp_multi_dev = dev;

	INIT_LIST_HEAD(&dev->funcs_fconf);
	INIT_LIST_HEAD(&dev->funcs_sconf);
	mutex_init(&dev->mutex);

	slp_multi_class = class_create(THIS_MODULE, "usb_mode");
	if (IS_ERR(slp_multi_class)) {
		err = PTR_ERR(slp_multi_class);
		goto err_create_class;
	}

	err = class_create_file(slp_multi_class, &class_attr_version.attr);
	if (err) {
		pr_err("%s: failed to create class file\n", __func__);
		goto err_create;
	}

	err = slp_multi_create_device(dev);
	if (err) {
		pr_err("%s: failed to create slp_multi device %d", __func__, err);
		goto err_create;
	}

	err = usb_composite_probe(&slp_multi_usb_driver);
	if (err) {
		pr_err("%s: failed to probe driver %d", __func__, err);
		goto err_create;
	}

	/* HACK: exchange composite's setup with ours */
	composite_setup_func = slp_multi_usb_driver.gadget_driver.setup;
	slp_multi_usb_driver.gadget_driver.setup = slp_multi_setup;

	return 0;

err_create:
	class_destroy(slp_multi_class);
err_create_class:
	kfree(dev);

	return err;
}
late_initcall(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&slp_multi_usb_driver);
	class_destroy(slp_multi_class);
	kfree(_slp_multi_dev);
	_slp_multi_dev = NULL;
}
module_exit(cleanup);

MODULE_AUTHOR("Jaewon Kim");
MODULE_DESCRIPTION("SLP Composite USB Driver similar to Android Composite");
MODULE_LICENSE("GPL");
MODULE_VERSION(USB_MODE_VERSION);
