/*
 * drivers/extcon/extcon-port.c
 *
 * MUIC/JACK compatible extcon driver with MUIC/JACK Extcon device driver
 *
 * Copyright (C) 2012 Samsung Electronics
 * Chanwoo Choi <cw00.choi@samsung.com>
 *
 * based on drivers/misc/jack.c
 * Copyright (C) 2009 Samsung Electronics
 * Minkyu Kang <mk7.samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/extcon.h>
#include <linux/extcon/extcon-port.h>
#include <linux/mfd/max77693.h>

/* Name of legacy symlink in /sys/devices/platform */
#define SYSFS_LEGACY_PLATFORM_DEVICE_NAME	"jack"
/************************************************************/
/************* extcon-port platform driver ******************/
/************************************************************/

struct extcon_cable_block {
	char extcon_name[32];
	char name[32];

	struct extcon_specific_cable_nb obj;
	struct work_struct wq;
	struct notifier_block nb;

	bool prev_attached;
	bool attached;

	void (*function)(struct extcon_cable_block *cable);
};

struct extcon_port {
	char extcon_name_muic[CABLE_NAME_MAX];
	struct extcon_dev *edev_muic;
	struct extcon_cable_block *cables_muic;

	char extcon_name_jack[CABLE_NAME_MAX];
	struct extcon_dev *edev_jack;
	struct extcon_cable_block *cables_jack;

	char extcon_name_hdmi[CABLE_NAME_MAX];
	struct extcon_dev *edev_hdmi;
	struct extcon_cable_block *cables_hdmi;

	struct jack_platform_data *pdata;
};

static struct platform_device *extcon_port_pdev;

/* Dummy event handler */
void jack_event_handler(char *name, int value) {}
EXPORT_SYMBOL_GPL(jack_event_handler);

int jack_get_data(const char *name)
{
	struct extcon_port *extcon_port
		= platform_get_drvdata(extcon_port_pdev);

	if (!strcmp(name, "usb"))
		return extcon_port->pdata->usb_online;
	else if (!strcmp(name, "charger"))
		return extcon_port->pdata->charger_online;
	else if (!strcmp(name, "hdmi"))
		return extcon_port->pdata->hdmi_online;
	else if (!strcmp(name, "earjack"))
		return extcon_port->pdata->earjack_online;
	else if (!strcmp(name, "earkey"))
		return extcon_port->pdata->earkey_online;
	else if (!strcmp(name, "ums"))
		return extcon_port->pdata->ums_online;
	else if (!strcmp(name, "cdrom"))
		return extcon_port->pdata->cdrom_online;
	else if (!strcmp(name, "jig"))
		return extcon_port->pdata->jig_online;
	else if (!strcmp(name, "host"))
		return extcon_port->pdata->host_online;
	else if (!strcmp(name, "cradle"))
		return extcon_port->pdata->cradle_online;


	return -EINVAL;
}
EXPORT_SYMBOL_GPL(jack_get_data);

#define EXTCON_PORT_ENTRY(name)						\
static ssize_t extcon_port_show_##name(struct device *dev,		\
		struct device_attribute *attr, char *buf)		\
{									\
	struct extcon_port *extcon_port = dev_get_drvdata(dev);		\
									\
	if (!extcon_port || !extcon_port->pdata)			\
		return 0;						\
									\
	return sprintf(buf, "%d\n", extcon_port->pdata->name);		\
}									\
static DEVICE_ATTR(name, S_IRUGO, extcon_port_show_##name, NULL);

EXTCON_PORT_ENTRY(usb_online);
EXTCON_PORT_ENTRY(charger_online);
EXTCON_PORT_ENTRY(hdmi_online);
EXTCON_PORT_ENTRY(earjack_online);
EXTCON_PORT_ENTRY(earkey_online);
EXTCON_PORT_ENTRY(jig_online);
EXTCON_PORT_ENTRY(host_online);
EXTCON_PORT_ENTRY(cradle_online);

static int extcon_port_create_sysfs(struct extcon_port *extcon_port)
{
	struct jack_platform_data *pdata = extcon_port->pdata;
	int ret;

	if (pdata->usb_online != -1)
		ret = device_create_file(&extcon_port_pdev->dev,
				&dev_attr_usb_online);
	if (pdata->charger_online != -1)
		ret = device_create_file(&extcon_port_pdev->dev,
				&dev_attr_charger_online);
	if (pdata->hdmi_online != -1)
		ret = device_create_file(&extcon_port_pdev->dev,
				&dev_attr_hdmi_online);
	if (pdata->earjack_online != -1)
		ret = device_create_file(&extcon_port_pdev->dev,
				&dev_attr_earjack_online);
	if (pdata->earkey_online != -1)
		ret = device_create_file(&extcon_port_pdev->dev,
				&dev_attr_earkey_online);
	if (pdata->jig_online != -1)
		ret = device_create_file(&extcon_port_pdev->dev,
				&dev_attr_jig_online);
	if (pdata->host_online != -1)
		ret = device_create_file(&extcon_port_pdev->dev,
				&dev_attr_host_online);
	if (pdata->cradle_online != -1)
		ret = device_create_file(&extcon_port_pdev->dev,
				&dev_attr_cradle_online);

	return 0;
}

static void extcon_port_set_data(struct jack_platform_data *pdata,
		const char *name, int value)
{
	if (!strcmp(name, "usb"))
		pdata->usb_online = value;
	else if (!strcmp(name, "charger"))
		pdata->charger_online = value;
	else if (!strcmp(name, "hdmi"))
		pdata->hdmi_online = value;
	else if (!strcmp(name, "earjack"))
		pdata->earjack_online = value;
	else if (!strcmp(name, "earkey"))
		pdata->earkey_online = value;
	else if (!strcmp(name, "ums"))
		pdata->ums_online = value;
	else if (!strcmp(name, "cdrom"))
		pdata->cdrom_online = value;
	else if (!strcmp(name, "jig"))
		pdata->jig_online = value;
	else if (!strcmp(name, "host"))
		pdata->host_online = value;
	else if (!strcmp(name, "cradle"))
		pdata->cradle_online = value;
}

static int extcon_port_event_handler(char *name, int value)
{
	struct extcon_port *extcon_port;
	char env_str[16];
	char *envp[] = { env_str, NULL };

	if (!extcon_port_pdev) {
		printk(KERN_ERR "jack device is not allocated\n");
		return -ENODEV;
	}

	extcon_port = platform_get_drvdata(extcon_port_pdev);
	extcon_port_set_data(extcon_port->pdata, name, value);
	sprintf(env_str, "CHGDET=%s", name);

	dev_info(&extcon_port_pdev->dev, "jack event %s\n", env_str);
	kobject_uevent_env(&extcon_port_pdev->dev.kobj, KOBJ_CHANGE, envp);

	return 0;
}

static void extcon_muic_function(struct extcon_cable_block *cable)
{
	int attached = cable->attached;
	int ret;
	char muic_name[10];

	if (!strcmp(cable->name, "USB")) {
		strcpy(muic_name, "usb");
	} else if (!strcmp(cable->name, "USB-Host")) {
		strcpy(muic_name, "host");
	} else if (!strcmp(cable->name, "TA")
		|| !strcmp(cable->name, "Fast-charger")
		|| !strcmp(cable->name, "Slow-charger")
		|| !strcmp(cable->name, "Charger-downstream")
		|| !strcmp(cable->name, "MHL_TA")) {
		strcpy(muic_name, "charger");
	} else if (!strcmp(cable->name, "JIG-USB-ON")
		|| !strcmp(cable->name, "JIG-USB-OFF")
		|| !strcmp(cable->name, "JIG-UART-OFF")) {
		strcpy(muic_name, "jig");
	} else if (!strcmp(cable->name, "Dock-Smart")) {
		strcpy(muic_name, "cradle");

		if (attached)
			attached = MAX77693_MUIC_DOCK_SMARTDOCK;
	} else if (!strcmp(cable->name, "Dock-Car")) {
		strcpy(muic_name, "cradle");

		if (attached)
			attached = MAX77693_MUIC_DOCK_CARDOCK;
	} else if (!strcmp(cable->name, "Dock-Desk")
		|| !strcmp(cable->name, "Dock-Audio")) {
		strcpy(muic_name, "cradle");

		if (attached)
			attached = MAX77693_MUIC_DOCK_DESKDOCK;
	} else {
		pr_err("Cannot detect unknown cable\n");
		goto out;
	}

	ret = extcon_port_event_handler(muic_name, attached);
	if (ret < 0) {
		pr_info("Faild to set jack event handler(%s)\n",
				muic_name);
		goto out;
	}

	if (!strcmp(cable->name, "USB")) {
		ret = extcon_port_event_handler("charger", attached);
		if (ret < 0) {
			pr_info("Faild to set jack event handler(%s)\n",
					muic_name);
			goto out;
		}
	}
out:
	return;
}

static void extcon_jack_function(struct extcon_cable_block *cable)
{
	int state = 0;
	int ret;
	char jack_name[10];

	if (!strcmp(cable->name, "Headset")) {
		strcpy(jack_name, "earjack");

		if (cable->attached)
			state = 3;

	} else if (!strcmp(cable->name, "Headphone")) {
		strcpy(jack_name, "earjack");

		if (cable->attached)
			state = 1;

	} else if (!strcmp(cable->name, "Microphone")) {
		strcpy(jack_name, "earkey");
		state = cable->attached;

	} else {
		pr_err("Cannot detect unknown cable\n");
		goto out;
	}

	ret = extcon_port_event_handler(jack_name, state);
	if (ret < 0) {
		pr_info("Faild to set jack event handler(%s)\n",
				jack_name);
	}
out:
	return;
}

static void extcon_hdmi_function(struct extcon_cable_block *cable)
{
	if (!strcmp(cable->name, "HDMI")) {
		int ret;

		ret = extcon_port_event_handler("hdmi", cable->attached);
		if (ret < 0) {
			pr_info("Faild to set hdmi event handler(%s)\n",
					cable->name);
		}
	}

	return;
}

static void extcon_work(struct work_struct *work)
{
	struct extcon_cable_block *cable =
		container_of(work, struct extcon_cable_block, wq);

	if (cable->function)
		cable->function(cable);
}

static int extcon_notifier(struct notifier_block *self,
			unsigned long event, void *ptr)
{
	struct extcon_cable_block *cable =
		container_of(self, struct extcon_cable_block, nb);

	/*
	 * The newly state of charger cable.
	 * If cable is attached, cable->attached is true.
	 */
	cable->prev_attached = cable->attached;
	cable->attached = event;

	/*
	 * Setup work for controlling charger(regulator)
	 * according to charger cable.
	 */
	schedule_work(&cable->wq);

	return NOTIFY_DONE;
}

int extcon_port_register(
		char *extcon_name,
		struct extcon_dev *edev,
		struct extcon_cable_block *cables,
		void (*fn)(struct extcon_cable_block *))
{
	struct extcon_cable_block *cable;
	int ret = 0, num_cables;
	int i;

	if (!extcon_name)
		return -EINVAL;

	edev = extcon_get_extcon_dev(extcon_name);
	if (!edev)
		return -ENODEV;

	num_cables = edev->max_supported;

	cables = kzalloc(sizeof(struct extcon_cable_block) * num_cables,
			GFP_KERNEL);
	if (!cables) {
		pr_err("failed to allocate array of extcon_cable_block(%s)\n",
				extcon_name);
		return -ENOMEM;
	}

	for (i = 0 ; i < num_cables ; i++) {
		cable = &cables[i];

		strcpy(cable->extcon_name, extcon_name);
		strcpy(cable->name, edev->supported_cable[i]);
		cable->function = fn;

		INIT_WORK(&cable->wq, extcon_work);
		cable->nb.notifier_call = extcon_notifier;
		ret = extcon_register_interest(&cable->obj,
			cable->extcon_name, cable->name, &cable->nb);
		if (ret < 0) {
			pr_err("Cannot register extcon_dev for %s(cable: %s)\n",
					cable->extcon_name, cable->name);
			goto err_extcon;
		}
	}

	return 0;

err_extcon:
	for (i = 0 ; i < num_cables ; i++) {
		cable = &cables[i];

		/* Unregister only extcon device which is initialized */
		if (cable->nb.notifier_call) {
			ret = extcon_unregister_interest(&cable->obj);

			pr_err("Unregister extcon_dev for %s(cable: %s)\n",
					cable->extcon_name, cable->name);
		}
	}
	kfree(cables);

	return ret;
}

int extcon_port_unregister(struct extcon_dev *edev,
		struct extcon_cable_block *cables)
{
	struct extcon_cable_block *cable;
	int num_cables;
	int ret = 0;
	int i;

	if (!edev)
		return -ENODEV;

	num_cables = edev->max_supported;

	for (i = 0 ; i < num_cables ; i++) {
		cable = &cables[i];

		/* Unregister only extcon device which is initialized */
		if (cable->nb.notifier_call) {
			ret = extcon_unregister_interest(&cable->obj);

			pr_err("Unregister extcon_dev for %s(cable: %s)\n",
					cable->extcon_name, cable->name);
		}
	}
	kfree(cables);

	return ret;
}

#ifdef CONFIG_OF
static struct jack_platform_data *extcon_port_dt_parse(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct jack_platform_data *pdata;
	const struct extcon_dev *edev;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		return NULL;
	}

	edev = extcon_get_edev_by_phandle(&pdev->dev, 0);
	if (IS_ERR(edev))
		dev_warn(&pdev->dev, "Failed to get extcon dev for muic from DT\n");
	else {
		dev_info(&pdev->dev, "Using extcon for muic device: %s\n",
				edev->name);
		pdata->extcon_name_muic = edev->name;
	}

	edev = extcon_get_edev_by_phandle(&pdev->dev, 1);
	if (IS_ERR(edev))
		dev_warn(&pdev->dev, "Failed to get extcon dev for jack from DT\n");
	else {
		dev_info(&pdev->dev, "Using extcon for jack device: %s\n",
				edev->name);
		pdata->extcon_name_jack = edev->name;
	}

	edev = extcon_get_edev_by_phandle(&pdev->dev, 2);
	if (IS_ERR(edev))
		dev_warn(&pdev->dev, "Failed to get extcon dev for hdmi from DT\n");
	else {
		dev_info(&pdev->dev, "Using extcon for hdmi device: %s\n",
				edev->name);
		pdata->extcon_name_hdmi = edev->name;
	}

	if (!of_property_read_bool(np, "samsung,extcon-online-usb"))
		pdata->usb_online = -1;
	if (!of_property_read_bool(np, "samsung,extcon-online-charger"))
		pdata->charger_online = -1;
	if (!of_property_read_bool(np, "samsung,extcon-online-hdmi"))
		pdata->hdmi_online = -1;
	if (!of_property_read_bool(np, "samsung,extcon-online-earjack"))
		pdata->earjack_online = -1;
	if (!of_property_read_bool(np, "samsung,extcon-online-earkey"))
		pdata->earkey_online = -1;
	if (!of_property_read_bool(np, "samsung,extcon-online-ums"))
		pdata->ums_online = -1;
	if (!of_property_read_bool(np, "samsung,extcon-online-cdrom"))
		pdata->cdrom_online = -1;
	if (!of_property_read_bool(np, "samsung,extcon-online-jig"))
		pdata->jig_online = -1;
	if (!of_property_read_bool(np, "samsung,extcon-online-host"))
		pdata->host_online = -1;
	if (!of_property_read_bool(np, "samsung,extcon-online-cradle"))
		pdata->cradle_online = -1;

	return pdata;
}
#else
#define extcon_port_dt_parse	NULL
#endif /* CONFIG_OF */

static int extcon_port_probe(struct platform_device *pdev)
{
	struct jack_platform_data *pdata;
	struct device_node *np = pdev->dev.of_node;
	struct extcon_port *extcon_port;
	int ret;

	if (!np) {
		dev_err(&pdev->dev, "Driver requires platform data from DT\n");
		return -EINVAL;
	}

	pdata = extcon_port_dt_parse(pdev);

	if (!pdata) {
		dev_err(&pdev->dev, "Failed to parse jack platform data from DT\n");
		return -EINVAL;
	}
	pdev->dev.platform_data = pdata;

	extcon_port = kzalloc(sizeof(struct extcon_port), GFP_KERNEL);
	if (!extcon_port) {
		dev_err(&pdev->dev, "failed to allocate driver data\n");
		ret = -ENOMEM;
		goto err;
	}
	platform_set_drvdata(pdev, extcon_port);
	extcon_port->pdata = pdata;
	extcon_port_pdev = pdev;

	ret = extcon_port_create_sysfs(extcon_port);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to create sysfs\n");
		goto err_sysfs;
	}

	if (!pdata->extcon_name_muic) {
		dev_err(&pdev->dev,
			"Cannot set name of muic device by platform data\n");
	} else {
		strcpy(extcon_port->extcon_name_muic, pdata->extcon_name_muic);
		ret = extcon_port_register(extcon_port->extcon_name_muic,
				extcon_port->edev_muic,
				extcon_port->cables_muic,
				extcon_muic_function);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to register extcon port (%s:%d)\n",
				pdata->extcon_name_muic, ret);
			goto err_extcon_muic;
		}
	}

	if (!pdata->extcon_name_jack) {
		dev_err(&pdev->dev,
			"Cannot set name of jack device by platform data\n");
	} else {
		strcpy(extcon_port->extcon_name_jack, pdata->extcon_name_jack);
		ret = extcon_port_register(extcon_port->extcon_name_jack,
				extcon_port->edev_jack,
				extcon_port->cables_jack,
				extcon_jack_function);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to register extcon port (%s:%d)\n",
				pdata->extcon_name_jack, ret);
			goto err_extcon_jack;
		}
	}

	if (!pdata->extcon_name_hdmi) {
		dev_err(&pdev->dev,
			"Cannot set name of hdmi device by platform data\n");
	} else {
		strcpy(extcon_port->extcon_name_hdmi, pdata->extcon_name_hdmi);
		ret = extcon_port_register(extcon_port->extcon_name_hdmi,
				extcon_port->edev_hdmi,
				extcon_port->cables_hdmi,
				extcon_hdmi_function);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to register extcon port (%s:%d)\n",
				pdata->extcon_name_hdmi, ret);
			goto err_extcon_hdmi;
		}
	}

	ret = sysfs_create_link(&platform_bus.kobj, &pdev->dev.kobj,
			SYSFS_LEGACY_PLATFORM_DEVICE_NAME);
	if (ret)
		dev_err(&pdev->dev,
			"Failed to create legacy symlink to platform/jack: %d\n",
			ret);

	return ret;

err_extcon_hdmi:
	ret = extcon_port_unregister(extcon_port->edev_hdmi,
				extcon_port->cables_hdmi);
err_extcon_jack:
	ret = extcon_port_unregister(extcon_port->edev_jack,
				extcon_port->cables_jack);
err_extcon_muic:
	ret = extcon_port_unregister(extcon_port->edev_muic,
				extcon_port->cables_muic);
err_sysfs:
	kfree(extcon_port);
err:
	return ret;
}

static int extcon_port_remove(struct platform_device *pdev)
{
	struct jack_platform_data *pdata = pdev->dev.platform_data;
	struct extcon_port *extcon_port = platform_get_drvdata(pdev);

	if (pdata->usb_online != -1)
		device_remove_file(&extcon_port_pdev->dev,
				&dev_attr_usb_online);
	if (pdata->charger_online != -1)
		device_remove_file(&extcon_port_pdev->dev,
				&dev_attr_charger_online);
	if (pdata->hdmi_online != -1)
		device_remove_file(&extcon_port_pdev->dev,
				&dev_attr_hdmi_online);
	if (pdata->earjack_online != -1)
		device_remove_file(&extcon_port_pdev->dev,
				&dev_attr_earjack_online);
	if (pdata->earkey_online != -1)
		device_remove_file(&extcon_port_pdev->dev,
				&dev_attr_earkey_online);
	if (pdata->jig_online != -1)
		device_remove_file(&extcon_port_pdev->dev,
				&dev_attr_jig_online);
	if (pdata->host_online != -1)
		device_remove_file(&extcon_port_pdev->dev,
				&dev_attr_host_online);
	if (pdata->cradle_online != -1)
		device_remove_file(&extcon_port_pdev->dev,
				&dev_attr_cradle_online);

	platform_set_drvdata(pdev, NULL);

	extcon_port_unregister(extcon_port->edev_hdmi,
			extcon_port->cables_hdmi);
	extcon_port_unregister(extcon_port->edev_muic,
			extcon_port->cables_jack);
	extcon_port_unregister(extcon_port->edev_jack,
			extcon_port->cables_muic);

	kfree(extcon_port);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id extcon_port_dt_match[] = {
	{ .compatible = "samsung,extcon-port" },
	{},
};
#endif

static struct platform_driver extcon_port_driver = {
	.probe		= extcon_port_probe,
	.remove		= extcon_port_remove,
	.driver		= {
		.name	= "extcon-port",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(extcon_port_dt_match),
	},
};

static int __init extcon_port_init(void)
{
	return platform_driver_register(&extcon_port_driver);
}
late_initcall(extcon_port_init);

static void __exit extcon_port_exit(void)
{
	platform_driver_unregister(&extcon_port_driver);
}
module_exit(extcon_port_exit);

MODULE_AUTHOR("Chanwoo Choi <cw00.choi@samsung.com>");
MODULE_DESCRIPTION("MUIC/JACK compatible extcon driver with MUIC/JACK device");
MODULE_LICENSE("GPL");
