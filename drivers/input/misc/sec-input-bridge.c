/*
 *  sec-input-bridge.c - Specific control input event bridge
 *  for Samsung Electronics
 *
 *  Copyright (C) 2010 Samsung Electronics
 *  Yongsul Oh <yongsul96.oh@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/input/sec-input-bridge.h>
#include <linux/reboot.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#ifdef CONFIG_KNOX_GEARPAY
#include <linux/knox_gearlock.h>
#endif
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
#endif

#define ONEKEY_DEFAULT_TIMEOUT	2000 /* 2 seconds */
#define ONEKEY_DISABLE_TIMEOUT	1000 /* 1 seconds */
#define ONEKEY_DEBUG_TIMEOUT		60000 /* 60 seconds */
#define MAX_ONEKEY_EVENT		10

#define PANIC_STRING			"PANIC"
#define DEBUG_STRING			"DEBUG"
#define SDB_STRING			"SDB"

#ifdef CONFIG_KNOX_GEARPAY
#define PAYMENT_TIMEOUT	500 /* 500 ms */
#endif

enum inpup_mode {
	KEY_LOGDUMP = 0,
	PAYMENT,
	ROTARY_LOGDUMP,
	ONEKEY_DEBUG,
	INPUT_MAX
};

static struct platform_device *gpdev;

struct sec_input_bridge {
	struct sec_input_bridge_platform_data *pdata;
	struct work_struct work;
	struct delayed_work onekey_dwork;
	struct mutex lock;
	struct platform_device *dev;
	struct timer_list onekey_timer;
	struct device *sec_dev;
#ifdef CONFIG_KNOX_GEARPAY
	struct timer_list payment_timer;
	struct work_struct payment_work;
	u32 payment_flag;
#endif
	/*
	 * Because this flag size is 32 byte, Max map table number is 32.
	 */
	u32 send_uevent_flag;
	u8 check_index[32];

	u32 debug_cnt;
	u32 onekey_cnt;
	bool onekey_cnt_start;
};

static void input_bridge_check_panic(char *onekey_env_str)
{

	if (!strncmp(onekey_env_str, PANIC_STRING, strlen(PANIC_STRING)))
		panic("Crash Key");
}

extern int sec_debug_get_debug_level(void);
static int input_bridge_get_debug_enable(void)
{
#ifdef CONFIG_SEC_DEBUG
	return sec_debug_get_debug_level();
#else
	return 0;
#endif
}

static void input_bridge_set_ids(struct input_device_id *ids, unsigned int type,
				 unsigned int code)
{
	switch (type) {
	case EV_KEY:
		ids->flags = INPUT_DEVICE_ID_MATCH_KEYBIT;
		__set_bit(code, ids->keybit);
		break;

	case EV_REL:
		ids->flags = INPUT_DEVICE_ID_MATCH_RELBIT;
		__set_bit(code, ids->relbit);
		break;

	case EV_ABS:
		ids->flags = INPUT_DEVICE_ID_MATCH_ABSBIT;
		__set_bit(code, ids->absbit);
		break;

	case EV_MSC:
		ids->flags = INPUT_DEVICE_ID_MATCH_MSCIT;
		__set_bit(code, ids->mscbit);
		break;

	case EV_SW:
		ids->flags = INPUT_DEVICE_ID_MATCH_SWBIT;
		__set_bit(code, ids->swbit);
		break;

	case EV_LED:
		ids->flags = INPUT_DEVICE_ID_MATCH_LEDBIT;
		__set_bit(code, ids->ledbit);
		break;

	case EV_SND:
		ids->flags = INPUT_DEVICE_ID_MATCH_SNDBIT;
		__set_bit(code, ids->sndbit);
		break;

	case EV_FF:
		ids->flags = INPUT_DEVICE_ID_MATCH_FFBIT;
		__set_bit(code, ids->ffbit);
		break;

	case EV_PWR:
		/* do nothing */
		break;

	default:
		printk(KERN_ERR
		       "input_bridge_set_ids: unknown type %u (code %u)\n",
		       type, code);
		return;
	}

	ids->flags |= INPUT_DEVICE_ID_MATCH_EVBIT;
	__set_bit(type, ids->evbit);
}

static void input_bridge_send_uevent_onekey(struct sec_input_bridge *bridge)
{
	struct platform_device *pdev = bridge->dev;
	struct sec_input_bridge_platform_data *pdata = bridge->pdata;
	char env_str[32];
	char *envp[2] = { env_str, NULL };
	int rc, i;

	for (i = 0; i < pdata->num_onekey; i++) {
		if (pdata->detected_onekey & (1 << i)) {
			pdata->detected_onekey &= ~(1 << i);
			sprintf(env_str, "%s=%s",
				pdata->onekey_map[i].onekey_env_str,
				pdata->onekey_map[i].onekey_env_value);

			rc =  kobject_uevent_env(&bridge->sec_dev->kobj,
					pdata->mmap[ONEKEY_DEBUG].uevent_action, envp);
			if (rc)
				dev_err(&pdev->dev,
					"%s: kobject_uevent_env fail[%s][%d]\n",
					__func__, env_str, rc);
			else
				dev_info(&pdev->dev, "%s: event:[%s]\n", __func__, env_str);
		}
	}
}

#ifdef CONFIG_SEC_DEBUG
extern int sec_debug_get_debug_level(void);
#endif
static void input_bridge_send_uevent(struct sec_input_bridge *bridge, int num)
{
	struct platform_device *pdev = bridge->dev;
	struct sec_input_bridge_platform_data *pdata = bridge->pdata;
	char env_str[16];
	char *envp[] = { env_str, NULL };
	int state;

	if (num == ONEKEY_DEBUG) {
		input_bridge_send_uevent_onekey(bridge);
		return;
	}

#ifdef CONFIG_SEC_DEBUG
	if (!sec_debug_get_debug_level()) {
		dev_info(&pdev->dev,
			"%s: sec_debug disabled.\n", __func__);
		return;
	}
#endif
	sprintf(env_str, "%s=%s",
		pdata->mmap[num].uevent_env_str,
		pdata->mmap[num].uevent_env_value);

	dev_info(&pdev->dev, "%s: event:[%s]\n", __func__, env_str);

	state =  kobject_uevent_env(&bridge->sec_dev->kobj,
			pdata->mmap[num].uevent_action, envp);
	if (state != 0) {
		dev_info(&pdev->dev,
			"%s: kobject_uevent_env fail[%d]\n",
			__func__, pdata->mmap[num].uevent_action);
	} else
		dev_info(&pdev->dev, "%s: now send uevent\n", __func__);
}

static void input_bridge_work(struct work_struct *work)
{
	struct sec_input_bridge *bridge =
		container_of(work, struct sec_input_bridge, work);
	struct sec_input_bridge_platform_data *pdata = bridge->pdata;
	int i;

	mutex_lock(&bridge->lock);

	for (i = 0; i < INPUT_MAX; i++) {
		if (bridge->send_uevent_flag & (1 << i)) {
			if (pdata->mmap[i].enable_uevent)
				input_bridge_send_uevent(bridge, i);
			if (pdata->mmap[i].pre_event_func) {
				pdata->mmap[i].pre_event_func
					(pdata->event_data);
			}
			bridge->send_uevent_flag &= ~(1 << i);
		}
	}

	mutex_unlock(&bridge->lock);

	printk(KERN_INFO "<sec-input-bridge> all process done !!!!\n");
}

#ifdef CONFIG_KNOX_GEARPAY
static void input_bridge_payment_work(struct work_struct *work)
{
	struct sec_input_bridge *bridge =
		container_of(work, struct sec_input_bridge, payment_work);
	struct platform_device *dev = bridge->dev;
	u32 ret = 0;

	mutex_lock(&bridge->lock);

	ret = exynos_smc_gearpay(PHYSBUTTON, true);
	if (ret)
		dev_err(&dev->dev, "%s:PAY LONG PRESS SMC failure %u\n", __func__, ret);
	else
		dev_info(&dev->dev, "%s:PAY LONG PRESS SMC success\n", __func__);

	mutex_unlock(&bridge->lock);
}

static void input_bridge_payment_timer(unsigned long data)
{
	struct sec_input_bridge *sec_bridge =
		(struct sec_input_bridge *)data;

	if (sec_bridge->payment_flag) {
		dev_dbg(&sec_bridge->dev->dev,
			"%s: LONG PRESS IS DETECTED.\n", __func__);
		schedule_work(&sec_bridge->payment_work);
	} else
		dev_dbg(&sec_bridge->dev->dev,
			"%s: LONG PRESS IS FAILED.\n", __func__);

	return;
}

static void input_bridge_check_payment(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	struct input_handler *sec_bridge_handler = handle->handler;
	struct sec_input_bridge *sec_bridge = sec_bridge_handler->private;

	if (value) {
		sec_bridge->payment_flag = true;
		mod_timer(&sec_bridge->payment_timer,\
				jiffies + msecs_to_jiffies(PAYMENT_TIMEOUT));
		dev_dbg(&sec_bridge->dev->dev, "%s: timer is set. (%dms)\n",
					__func__, PAYMENT_TIMEOUT);
	} else {
		sec_bridge->payment_flag = false;
		dev_dbg(&sec_bridge->dev->dev,
			"%s: payment is released.\n", __func__);
	}
}
#endif
static void input_bridge_check_rotarydump(struct input_handle *handle, unsigned int type,
			       unsigned int code, int value)
{
	struct input_handler *sec_bridge_handler = handle->handler;
	struct sec_input_bridge *sec_bridge = sec_bridge_handler->private;
	struct sec_input_bridge_platform_data *pdata = sec_bridge->pdata;
	struct sec_input_bridge_mmap *mmap = &pdata->mmap[ROTARY_LOGDUMP];

	if (mmap->mkey_map[sec_bridge->check_index[ROTARY_LOGDUMP]].type == type) {
		if (type == EV_KEY) {
			if (mmap->mkey_map[sec_bridge->check_index[ROTARY_LOGDUMP]].code == code) {
				sec_bridge->check_index[ROTARY_LOGDUMP] = value;
			} else
				goto out;
		} else if (type == EV_REL) {
			if (mmap->mkey_map[sec_bridge->check_index[ROTARY_LOGDUMP]].code == code) {
				sec_bridge->check_index[ROTARY_LOGDUMP]++;
				if ((sec_bridge->check_index[ROTARY_LOGDUMP]) >= mmap->num_mkey) {
					sec_bridge->send_uevent_flag |= (1 << ROTARY_LOGDUMP);
					schedule_work(&sec_bridge->work);
					goto out;
				}
			} else if (code == REL_X) {
				return;
			} else {
				goto out;
			}
		} else {
			goto out;
		}
	} else {
		goto out;
	}

	return;

out:
	sec_bridge->check_index[ROTARY_LOGDUMP] = 0;
	return;
}

static void input_bridge_onekey_timer(unsigned long data)
{
	struct sec_input_bridge *sec_bridge =
		(struct sec_input_bridge *)data;
	struct sec_input_bridge_platform_data *pdata = sec_bridge->pdata;
	int i;

	for (i = 0; i < pdata->num_onekey; i++) {
		if (pdata->onekey_map[i].key_cnt == sec_bridge->onekey_cnt) {
			if (!pdata->onekey_map[i].debug_low_enable) {
				if (!input_bridge_get_debug_enable())
					continue;
			}

			dev_info(&sec_bridge->dev->dev, "%s: ONEKEY %s DETECTED\n",
				__func__, pdata->onekey_map[i].onekey_env_str);
			sec_bridge->send_uevent_flag |= 1 << ONEKEY_DEBUG;
			pdata->detected_onekey |= 1 << i;
			input_bridge_check_panic(pdata->onekey_map[i].onekey_env_str);
			schedule_work(&sec_bridge->work);
		}
	}

	sec_bridge->onekey_cnt = 0;
	sec_bridge->onekey_cnt_start = false;
}

static void input_bridge_check_onekey_event(struct input_handle *handle, unsigned int type,
			       unsigned int code, int value)
{
	struct input_handler *sec_bridge_handler = handle->handler;
	struct sec_input_bridge *sec_bridge = sec_bridge_handler->private;
	struct sec_input_bridge_platform_data *pdata = sec_bridge->pdata;
	struct sec_input_bridge_mmap *mmap = &pdata->mmap[ONEKEY_DEBUG];
	struct sec_input_bridge_mkey *mkey_map = mmap->mkey_map;

	if (!mmap->enable)
		return;

	if ((code != mkey_map[0].code) || (!value))
		return;

	if (!sec_bridge->onekey_cnt_start) {
		sec_bridge->onekey_cnt = 1;
		sec_bridge->onekey_cnt_start = 1;
		mod_timer(&sec_bridge->onekey_timer,
			jiffies + msecs_to_jiffies(pdata->onekey_timeout));
	} else {
		sec_bridge->onekey_cnt++;
	}
}

static void input_bridge_check_logdump(struct input_handle *handle, unsigned int type,
			       unsigned int code, int value)
{
	struct input_handler *sec_bridge_handler = handle->handler;
	struct sec_input_bridge *sec_bridge = sec_bridge_handler->private;
	struct sec_input_bridge_platform_data *pdata = sec_bridge->pdata;
	struct sec_input_bridge_mmap *mmap = &pdata->mmap[KEY_LOGDUMP];

	if (sec_bridge->check_index[KEY_LOGDUMP] > mmap->num_mkey) {
		printk(KERN_ERR "sec_bridge->check_index[KEY_LOGDUMP] = [%d]",\
			sec_bridge->check_index[KEY_LOGDUMP]);
		sec_bridge->check_index[KEY_LOGDUMP] = 0;
		return;
	}

	if (mmap->mkey_map[sec_bridge->check_index[KEY_LOGDUMP]].code == code) {
		sec_bridge->check_index[KEY_LOGDUMP]++;
		if ((sec_bridge->check_index[KEY_LOGDUMP]) >= mmap->num_mkey) {
			sec_bridge->send_uevent_flag |= (1 << KEY_LOGDUMP);
			schedule_work(&sec_bridge->work);
			sec_bridge->check_index[KEY_LOGDUMP] = 0;
		}
	} else if (mmap->mkey_map[0].code == code)
		sec_bridge->check_index[KEY_LOGDUMP] = 1;
	else
		sec_bridge->check_index[KEY_LOGDUMP] = 0;
}

static void input_bridge_event(struct input_handle *handle, unsigned int type,
			       unsigned int code, int value)
{
	struct input_handler *sec_bridge_handler = handle->handler;
	struct sec_input_bridge *sec_bridge = sec_bridge_handler->private;
	struct sec_input_bridge_platform_data *pdata = sec_bridge->pdata;

	switch (type) {
	case EV_KEY:
		if (pdata->mmap[ONEKEY_DEBUG].enable)
			input_bridge_check_onekey_event
				    (handle, type, code, value);
		if (pdata->mmap[KEY_LOGDUMP].enable)
			input_bridge_check_logdump
				    (handle, type, code, value);
#ifdef CONFIG_KNOX_GEARPAY
		if (code == KEY_BACK)
			input_bridge_check_payment
					(handle, type, code, value);
#endif
		if (pdata->mmap[ROTARY_LOGDUMP].enable)
			input_bridge_check_rotarydump
					    (handle, type, code, value);
		break;

	case EV_REL:
		input_bridge_check_rotarydump
				    (handle, type, code, value);
	default:
		break;
	}

}

static bool input_bridge_match(struct input_handler *handler,
				struct input_dev *dev)
{
	struct sec_input_bridge *bridge = handler->private;
	struct sec_input_bridge_platform_data *pdata = bridge->pdata;
	int i;

	for(i = 0; i < pdata->support_dev_num; i++) {
		if (!strncmp(dev->name, bridge->pdata->support_dev_name[i],
				strlen(pdata->support_dev_name[i])))
			return 1;
	}

	return 0;
}

static int input_bridge_connect(struct input_handler *handler,
				struct input_dev *dev,
				const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "sec-input-bridge";

	error = input_register_handle(handle);
	if (error) {
		dev_err(&dev->dev,
		       "sec-input-bridge: Failed to register input bridge handler, "
		       "error %d\n", error);
		kfree(handle);
		return error;
	}

	error = input_open_device(handle);
	if (error) {
		dev_err(&dev->dev,
		       "sec-input-bridge: Failed to open input bridge device, "
		       "error %d\n", error);
		input_unregister_handle(handle);
		kfree(handle);
		return error;
	}

	dev_info(&dev->dev, "%s: connected %s.\n", __func__, dev->name);

	return 0;
}

static void input_bridge_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static struct input_handler input_bridge_handler = {
	.event = input_bridge_event,
	.match = input_bridge_match,
	.connect = input_bridge_connect,
	.disconnect = input_bridge_disconnect,
	.name = "sec-input-bridge",
};

#ifdef CONFIG_OF
static int sec_input_bridge_onekey_dt(struct device *dev,
			struct sec_input_bridge_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 onekey_cnt_buff[MAX_ONEKEY_EVENT];
	u32 debug_low_buff[MAX_ONEKEY_EVENT];
	const char *out_prop;
	int i, rc;

	rc = of_property_read_u32(np, "onekey_num",
		(unsigned int *)&pdata->num_onekey);
	if (rc) {
		dev_err(dev, "failed to get onekey_num.\n");
		goto error;
	}

	rc = of_property_read_u32(np, "onekey_timeout",
		(unsigned int *)&pdata->onekey_timeout);
	if (rc) {
		dev_err(dev, "failed to get onekey_timeout\n");
		pdata->onekey_timeout = ONEKEY_DEFAULT_TIMEOUT;
	}

	pdata->onekey_map = devm_kzalloc(dev,
			pdata->num_onekey * sizeof(struct sec_input_bridge_onekey),
			GFP_KERNEL);
	if (!pdata->mmap) {
		dev_err(dev, "%s: Failed to allocate memory for onekey.\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	rc = of_property_read_u32_array(np, "onekey_key_cnt", onekey_cnt_buff, pdata->num_onekey);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read onekey_key_cnt\n");
		goto error;
	}

	rc = of_property_read_u32_array(np, "debug_low_enable", debug_low_buff, pdata->num_onekey);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read debug_low_enable\n");
		goto error;
	}

	for (i = 0; i < pdata->num_onekey; i++) {
		pdata->onekey_map[i].key_cnt = onekey_cnt_buff[i];
		pdata->onekey_map[i].debug_low_enable = debug_low_buff[i];

		rc = of_property_read_string_index(np, "onekey_env_str", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "failed to get %d onekey_env_str string\n", i);
			goto error;
		}
		pdata->onekey_map[i].onekey_env_str = (char *)out_prop;

		rc = of_property_read_string_index(np, "onekey_env_value", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "failed to get %d onekey_env_value string\n", i);
			goto error;
		}
		pdata->onekey_map[i].onekey_env_value = (char *)out_prop;
	}

error:
	return rc;
}

static int sec_input_bridge_parse_dt(struct device *dev,
			struct sec_input_bridge_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	struct property *prop;
	enum mkey_check_option option;
	unsigned int *map;
	unsigned int *type;
	const char *out_prop;
	int i, j, rc, map_size, type_size;
	u32 id = 0;

	rc = of_property_read_u32(np, "input_bridge,num_map",\
			(unsigned int *)&pdata->num_map);
	if (rc) {
		dev_err(dev, "failed to get num_map.\n");
		goto error;
	}

	rc = of_property_read_u32(np, "input_bridge,map_key",\
			(unsigned int *)&option);
	if (rc) {
		dev_err(dev, "Unable to read %s\n", "input_bridge,map_key");
		goto error;
	}

	pdata->mmap = devm_kzalloc(dev,
			sizeof(struct sec_input_bridge_mmap)*INPUT_MAX, GFP_KERNEL);
	if (!pdata->mmap) {
		dev_err(dev, "%s: Failed to allocate memory.\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for (i = 0; i < pdata->num_map; i++) {
		rc = of_property_read_string_index(np, "input_bridge,map_id", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "failed to get %d map_id string.[%s]\n", i, out_prop);
			goto error;

		}

		rc = of_property_read_u32(np, out_prop, &id);
		if (rc) {
			dev_err(dev, "failed to get id[%s]\n", out_prop);
			goto error;
		}
		pdata->mmap[id].enable = true;

		rc = of_property_read_string_index(np, "input_bridge,map_codes", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "failed to get %d map_codes string.[%s]\n", i, out_prop);
			goto error;

		}
		prop = of_find_property(np, out_prop, NULL);
		if (!prop) {
			rc = -EINVAL;
			goto error;
		}
		map_size = prop->length / sizeof(unsigned int);
		pdata->mmap[id].num_mkey = map_size;
		map = devm_kzalloc(dev, sizeof(unsigned int)*map_size, GFP_KERNEL);
		if (!map) {
			dev_err(dev, "%s: Failed to allocate map memory.\n", __func__);
			rc = -ENOMEM;
			goto error;
		}

		rc = of_property_read_u32_array(np, out_prop, map, map_size);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read %s\n", out_prop);
			goto error;
		}

		rc = of_property_read_string_index(np, "input_bridge,map_types", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "failed to get %d map_types string.[%s]\n", i, out_prop);
			goto error;

		}
		prop = of_find_property(np, out_prop, NULL);
		if (!prop) {
			rc = -EINVAL;
			goto error;
		}

		type_size = prop->length / sizeof(unsigned int);
		type = devm_kzalloc(dev, sizeof(unsigned int)*type_size, GFP_KERNEL);
		if (!type) {
			dev_err(dev, "%s: Failed to allocate key memory.\n", __func__);
			rc = -ENOMEM;
			goto error;
		}

		rc = of_property_read_u32_array(np, out_prop, type, type_size);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read %s\n", out_prop);
			goto error;
		}

		pdata->mmap[id].mkey_map = devm_kzalloc(dev,
					sizeof(struct sec_input_bridge_mkey)*map_size, GFP_KERNEL);
		if (!pdata->mmap[id].mkey_map) {
			rc = -ENOMEM;
			goto error;
		}

		for (j = 0; j < map_size; j++) {
			pdata->mmap[id].mkey_map[j].type = type[j];
			pdata->mmap[id].mkey_map[j].code = map[j];
		}

		rc = of_property_read_string_index(np, "input_bridge,env_str", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "failed to get %d env_str string\n", i);
			goto error;
		}
		pdata->mmap[id].uevent_env_str = (char *)out_prop;

		rc = of_property_read_string_index(np, "input_bridge,env_value", i, &out_prop);
		if (rc) {
			dev_err(dev, "failed to get %d env_value string\n", i);
			goto error;
		}
		pdata->mmap[id].uevent_env_value = (char *)out_prop;

		pdata->mmap[id].enable_uevent = (unsigned char)of_property_read_bool\
						(np, "input_bridge,enable_uevent");

		rc = of_property_read_u32(np, "input_bridge,uevent_action",\
					(u32 *)&pdata->mmap[i].uevent_action);
		if (rc) {
			dev_err(dev, "failed to get uevent_action.\n");
			goto error;
		}
	}

	rc = of_property_read_u32(np, "input_bridge,dev_num",\
			(unsigned int *)&pdata->support_dev_num);
	if (rc) {
		dev_err(dev, "failed to get support_dev_num.\n");
		goto error;
	}

	pdata->support_dev_name = devm_kzalloc(dev,
			sizeof(char*)*pdata->support_dev_num, GFP_KERNEL);
	if (!pdata->support_dev_name) {
		dev_err(dev, "%s: Failed to allocate memory."\
			"[support_dev_name]\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for (i = 0; i < pdata->support_dev_num; i++) {
		rc = of_property_read_string_index(np, "input_bridge,dev_name_str", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "failed to get %d dev_name_str string\n", i);
			goto error;
		}
		pdata->support_dev_name[i] = (char *)out_prop;
	}

	if (of_find_property(np, "onekey_id", NULL)) {
		rc = sec_input_bridge_onekey_dt(dev, pdata);
		if (rc) {
			dev_err(dev, "failed get onekey_dt\n");
			goto error;
		}
	}

#ifdef DEBUG_BRIDGE
	for (i = 0; i < INPUT_MAX; i++) {
		if (!pdata->mmap[i].enable)
			continue;

		dev_info(dev, "%s: pdata->mmap[%d].num_mkey=[%d]\n",
				__func__, i, pdata->mmap[i].num_mkey);
		for(j = 0; j < pdata->mmap[i].num_mkey; j++) {
			dev_info(dev,
				"%s: pdata->mmap[%d].mkey_map[%d].type=[%d]\n",
				__func__, i, j, pdata->mmap[i].mkey_map[j].type);
			dev_info(dev,
				"%s: pdata->mmap[%d].mkey_map[%d].code=[%d]\n",
				__func__, i, j, pdata->mmap[i].mkey_map[j].code);
		}
		dev_info(dev, "%s: pdata->mmap[%d].uevent_env_str=[%s]\n",
				__func__, i, pdata->mmap[i].uevent_env_str);
		dev_info(dev, "%s: pdata->mmap[%d].uevent_env_value=[%s]\n",
				__func__, i, pdata->mmap[i].uevent_env_value);
		dev_info(dev, "%s: pdata->mmap[%d].enable_uevent=[%d]\n",
				__func__, i, pdata->mmap[i].enable_uevent);
		dev_info(dev, "%s: pdata->mmap[%d].uevent_action=[%d]\n",
				__func__, i, pdata->mmap[i].uevent_action);
	}
	dev_info(dev, "%s: pdata->support_dev_num=[%d]\n", __func__, pdata->support_dev_num);
	for (i = 0; i < pdata->support_dev_num; i++)
		dev_info(dev, "%s: pdata->support_dev_name[%d] = [%s]\n",
			__func__, i, pdata->support_dev_name [i]);
#endif
	return 0;
error:
	return rc;
}

static struct of_device_id input_bridge_of_match[] = {
	{ .compatible = "samsung_input_bridge", },
	{ },
};
MODULE_DEVICE_TABLE(of, input_bridge_of_match);
#else
static int sec_input_bridge_parse_dt(struct device *dev,
			struct sec_input_bridge_platform_data *pdata)
{
	dev_err(dev, "%s\n", __func__);
	return -ENODEV;
}
#endif

static int sec_input_bridge_probe(struct platform_device *pdev)
{

	struct sec_input_bridge_platform_data *pdata;
	struct sec_input_bridge *bridge;
	struct input_device_id *input_bridge_ids;
	int state, i, j, k;
	int total_num_key = 0;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct sec_input_bridge_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate pdata memory\n");
			state = -ENOMEM;
			goto error_1;
		}
		state = sec_input_bridge_parse_dt(&pdev->dev, pdata);
		if (state)
			goto error_1;
	} else {
		pdata = pdev->dev.platform_data;
		if (!pdata) {
			dev_err(&pdev->dev, "Fail samsung input bridge platform data.\n");
			state = -ENOMEM;
			goto error_1;
		}
	}

	if (pdata->num_map == 0) {
		dev_err(&pdev->dev,
			"No samsung input bridge platform data. num_mkey is NULL.\n");
		state = -EINVAL;
		goto error_1;
	}

	bridge = kzalloc(sizeof(struct sec_input_bridge), GFP_KERNEL);
	if (!bridge) {
		dev_err(&pdev->dev, "Failed to allocate bridge memory\n");
		state = -ENOMEM;
		goto error_1;
	}

	bridge->send_uevent_flag = 0;

	for (i = 0; i < pdata->num_map; i++)
		total_num_key += pdata->mmap[i].num_mkey;

	input_bridge_ids =
		kzalloc(sizeof(struct input_device_id[(total_num_key + 1)]),
			GFP_KERNEL);
	if (!input_bridge_ids) {
		dev_err(&pdev->dev, "Failed to allocate input_bridge_ids memory\n");
		state = -ENOMEM;
		goto error_2;
	}
	memset(input_bridge_ids, 0x00, sizeof(struct input_device_id));

	for (i = 0, k = 0; i < pdata->num_map; i++) {
		for (j = 0; j < pdata->mmap[i].num_mkey; j++) {
			input_bridge_set_ids(&input_bridge_ids[k++],
					     pdata->mmap[i].mkey_map[j].type,
					     pdata->mmap[i].mkey_map[j].code);
		}
	}

	input_bridge_handler.private = bridge;
	input_bridge_handler.id_table = input_bridge_ids;

	state = input_register_handler(&input_bridge_handler);
	if (state) {
		dev_err(&pdev->dev, "Failed to register input_bridge_handler\n");
		goto error_3;
	}

	bridge->dev = pdev;
	bridge->pdata = pdata;
	gpdev = pdev;
	platform_set_drvdata(pdev, bridge);

#ifdef CONFIG_KNOX_GEARPAY
	setup_timer(&bridge->payment_timer,
			input_bridge_payment_timer,
			(unsigned long)bridge);

	INIT_WORK(&bridge->payment_work, input_bridge_payment_work);
#endif
	if (pdata->mmap[ONEKEY_DEBUG].enable) {
		setup_timer(&bridge->onekey_timer,
				input_bridge_onekey_timer,
				(unsigned long)bridge);
	}

	INIT_WORK(&bridge->work, input_bridge_work);
	mutex_init(&bridge->lock);

#ifdef CONFIG_SEC_SYSFS
	bridge->sec_dev = sec_device_create(bridge, "sec_input_bridge");
	if (IS_ERR(bridge->sec_dev)) {
		dev_err(&pdev->dev, "Failed to create sec_device_create\n");
		goto error_3;
	}
#else
	bridge->sec_dev = device_create(sec_class, NULL, 0, NULL,
						"sec_input_bridge");
	if (IS_ERR(bridge->sec_dev)) {
		dev_err(&pdev->dev, "%s: Failed to create device"\
				"(sec_input_bridge)!\n", __func__);
		goto error_3;
	} else {
		dev_err(sfd->dev, "%s: sec_class is NULL\n", __func__);
		goto error_3;
	}
#endif
	dev_set_drvdata(bridge->sec_dev, bridge);

	dev_info(&pdev->dev, "%s: done.\n", __func__);

	return 0;

error_3:
	kfree(input_bridge_ids);
error_2:
	kfree(bridge);
error_1:
	return state;

}

static int sec_input_bridge_remove(struct platform_device *dev)
{
	struct sec_input_bridge *bridge = platform_get_drvdata(dev);

#ifdef CONFIG_SEC_SYSFS
		sec_device_destroy(bridge->sec_dev->devt);
#else
		device_destroy(sec_class, bridge->sec_dev->devt);
#endif
	cancel_work_sync(&bridge->work);
	mutex_destroy(&bridge->lock);
	kfree(input_bridge_handler.id_table);
	input_unregister_handler(&input_bridge_handler);
	kfree(bridge);
	platform_set_drvdata(dev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int sec_input_bridge_suspend(struct platform_device *dev,
				    pm_message_t state)
{
	return 0;
}

static int sec_input_bridge_resume(struct platform_device *dev)
{
	return 0;
}
#else
#define sec_input_bridge_suspend		NULL
#define sec_input_bridge_resume		NULL
#endif

static struct platform_driver sec_input_bridge_driver = {
	.probe = sec_input_bridge_probe,
	.remove = sec_input_bridge_remove,
	.suspend = sec_input_bridge_suspend,
	.resume = sec_input_bridge_resume,
	.driver = {
		   .name = "samsung_input_bridge",
#ifdef CONFIG_OF
		   .of_match_table = input_bridge_of_match,
#endif
		   },
};

static int __init sec_input_bridge_init(void)
{
	return platform_driver_register(&sec_input_bridge_driver);
}

static void __exit sec_input_bridge_exit(void)
{
	platform_driver_unregister(&sec_input_bridge_driver);
}

fs_initcall(sec_input_bridge_init);
module_exit(sec_input_bridge_exit);

MODULE_AUTHOR("Yongsul Oh <yongsul96.oh@samsung.com>");
MODULE_DESCRIPTION("Input Event -> Specific Control Bridge");
MODULE_LICENSE("GPL");
