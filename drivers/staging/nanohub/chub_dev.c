/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/platform_data/nanohub.h>

#include "chub_dev.h"
#include "chub.h"
#include "chub_dbg.h"

#ifdef USE_HAL_IF
static int contexthub_dev_open(struct inode *inode, struct file *file)
{
	struct host_data *chub_dev = container_of(file->private_data,
			struct host_data, miscdev);

	dev_info(chub_dev->dev, "%s\n", __func__);
	return 0;
}

static ssize_t contexthub_dev_read(struct file *file, char *buffer, size_t length,
			    loff_t *offset)
{
	struct host_data *chub_dev = container_of(file->private_data,
			struct host_data, miscdev);

	dev_info(chub_dev->dev, "%s\n", __func__);
	return 0;
}

static ssize_t contexthub_dev_write(struct file *file, const char *buffer,
			     size_t length, loff_t *offset)
{
	struct host_data *chub_dev = container_of(file->private_data,
			struct host_data, miscdev);

	dev_info(chub_dev->dev, "%s\n", __func__);
	return 0;
}

static unsigned int contexthub_dev_poll(struct file *file, poll_table *wait)
{
	struct host_data *chub_dev = container_of(file->private_data,
			struct host_data, miscdev);

	dev_info(chub_dev->dev, "%s\n", __func__);
	return 0;
}

static int contexthub_dev_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static const struct file_operations contexthub_fileops = {
	.owner = THIS_MODULE,
	.open = contexthub_dev_open,
	.read = contexthub_dev_read,
	.write = contexthub_dev_write,
	.poll = contexthub_dev_poll,
	.release = contexthub_dev_release,
};
#endif

static inline void mcu_wakeup_set_value(struct host_data *data,
					     int val)
{
	struct nanohub_platform_data *pdata = data->pdata;

	if (atomic_read(&pdata->wakeup_chub) != val) {
		atomic_set(&pdata->wakeup_chub, val);
		val = val ? IRQ_EVT_A2C_WAKEUP_CLR : IRQ_EVT_A2C_WAKEUP;
		ipc_add_evt(IPC_EVT_A2C, val);
	}
}

static inline bool nanohub_has_priority_lock_locked(struct host_data *data)
{
	return  atomic_read(&data->wakeup_lock_cnt) > atomic_read(&data->wakeup_cnt);
}

static inline void mcu_wakeup_gpio_get_locked(struct host_data *data)
{
	atomic_inc(&data->wakeup_lock_cnt);
	if (atomic_inc_return(&data->wakeup_cnt) == 1 &&
	    !nanohub_has_priority_lock_locked(data))
		mcu_wakeup_set_value(data, 0);
}

static inline bool mcu_wakeup_gpio_put_locked(struct host_data *data)
{
	bool gpio_done = atomic_dec_and_test(&data->wakeup_cnt);
	bool done = atomic_dec_and_test(&data->wakeup_lock_cnt);

	if (!nanohub_has_priority_lock_locked(data))
		mcu_wakeup_set_value(data, gpio_done ? 1 : 0);

	return done;
}

static inline bool mcu_wakeup_try_lock(struct host_data *data, int key)
{
	/* implementation contains memory barrier */
	return atomic_cmpxchg(&data->wakeup_acquired, 0, key) == 0;
}

static inline void mcu_wakeup_unlock(struct host_data *data, int key)
{
	WARN(atomic_cmpxchg(&data->wakeup_acquired, key, 0) != key,
	     "%s: failed to unlock with key %d; current state: %d",
	     __func__, key, atomic_read(&data->wakeup_acquired));
}

static inline bool mcu_wakeup_gpio_is_locked(struct host_data *data)
{
	return atomic_read(&data->wakeup_lock_cnt) != 0;
}

static inline int nanohub_irq1_fired(struct host_data *data)
{
	return !atomic_read(&data->pdata->irq1_apInt);
}

int request_wakeup_ex(struct host_data *data, long timeout_ms,
		      int key)
{
	long timeout;
	unsigned long flag;

	spin_lock_irqsave(&data->wakeup_wait.lock, flag);

	mcu_wakeup_gpio_get_locked(data);
	timeout = (timeout_ms != MAX_SCHEDULE_TIMEOUT) ?
		   msecs_to_jiffies(timeout_ms) : MAX_SCHEDULE_TIMEOUT;

	timeout = wait_event_interruptible_timeout_locked(
			data->wakeup_wait,
			(nanohub_irq1_fired(data) && mcu_wakeup_try_lock(data, key)),
			timeout);

	if (timeout <= 0) {
		mcu_wakeup_gpio_put_locked(data);

		if (timeout == 0)
			timeout = -ETIME;
	} else {
		timeout = 0;
	}

	spin_unlock_irqrestore(&data->wakeup_wait.lock, flag);

	return timeout;
}

void release_wakeup_ex(struct host_data *data, int key)
{
	bool done;
	unsigned long flag;

	spin_lock_irqsave(&data->wakeup_wait.lock, flag);

	done = mcu_wakeup_gpio_put_locked(data);
	mcu_wakeup_unlock(data, key);

	spin_unlock_irqrestore(&data->wakeup_wait.lock, flag);

	if (!done)
		wake_up_interruptible_sync(&data->wakeup_wait);
#ifdef USE_CHUB_THREAD
	else if (nanohub_irq1_fired(data))
		pr_info("%s: chub is off\n");
#endif
}

inline void contexthub_handle_irq1(struct host_data *data)
{
	bool locked;

	spin_lock(&data->wakeup_wait.lock);
	locked = mcu_wakeup_gpio_is_locked(data);
	spin_unlock(&data->wakeup_wait.lock);

	if (locked)
		wake_up_interruptible_sync(&data->wakeup_wait);
#ifdef USE_CHUB_THREAD
	else
		pr_info("%s: ap no locked\n", __func__);
#endif
}

enum {
	KEY_WAKEUP_NONE,
	KEY_WAKEUP,
	KEY_WAKEUP_LOCK,
};

static inline int request_wakeup_timeout(struct host_data *data, int timeout)
{
	int ret = request_wakeup_ex(data, timeout, KEY_WAKEUP);

	if (ret)
		dev_err(data->dev, "%s failed: ret=%d\n",	__func__, ret);
	return ret;
}

static inline int request_wakeup(struct host_data *data)
{
	int ret = request_wakeup_ex(data, MAX_SCHEDULE_TIMEOUT, KEY_WAKEUP);

	if (ret)
		dev_err(data->dev, "%s failed: ret=%d\n",	__func__, ret);
	return ret;
}

static inline void release_wakeup(struct host_data *data)
{
	release_wakeup_ex(data, KEY_WAKEUP);
}

static ssize_t contexthub_dev_download_bl(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct host_data *data = dev_get_drvdata(dev);
	int ret;

	ret = contexthub_download_bl(data);
	return ret < 0 ? ret : count;
}

static ssize_t contexthub_dev_download_kernel(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret = contexthub_download_kernel(dev);

	return ret < 0 ? ret : count;
}

static ssize_t contexthub_dev_hw_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct host_data *data = dev_get_drvdata(dev);
	int ret;

	/* TODO: add wait lock */
	ret = contexthub_reset(data);
	return ret < 0 ? ret : count;
}

static ssize_t contexthub_dev_poweron(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct host_data *data = dev_get_drvdata(dev);
	int ret;

	/* TODO: add wait lock */
	ret = contexthub_poweron(data);
	return ret < 0 ? ret : count;
}

static struct device_attribute attributes[] = {
	__ATTR(download_bl, 0220, NULL, contexthub_dev_download_bl),
	__ATTR(download_kernel, 0220, NULL, contexthub_dev_download_kernel),
	__ATTR(reset, 0220, NULL, contexthub_dev_hw_reset),
	__ATTR(poweron, 0220, NULL, contexthub_dev_poweron),
};

static int contexhub_drv_init(struct device *dev)
{
	int ret = 0;
	int i;
	struct host_data *chub = dev_get_drvdata(dev);

	for (i = 0, ret = 0; i < ARRAY_SIZE(attributes); i++) {
		ret = device_create_file(dev, &attributes[i]);
		if (ret) {
			dev_err(dev,
				"create sysfs attr %d [%s] failed; err=%d\n",
				i, attributes[i].attr.name, ret);
			return -EINVAL;
		}
	}

	atomic_set(&chub->wakeup_cnt, 0);
	atomic_set(&chub->wakeup_lock_cnt, 0);
	atomic_set(&chub->wakeup_acquired, 0);
	init_waitqueue_head(&chub->wakeup_wait);

#ifdef USE_HAL_IF
	chub->miscdev.minor = MISC_DYNAMIC_MINOR;
	chub->miscdev.name = "contexthub";
	chub->miscdev.fops = &contexthub_fileops;
	ret = misc_register(&chub->miscdev);
	if (ret) {
		dev_err(dev, "%s: Fail to register misc dev. ret(%d)\n",
			__func__, ret);
		return -EINVAL;
	}
	dev_info(dev, "%s: registered as misc dev. ret:%d\n", __func__, ret);
#endif

	return 0;
}

static struct host_data *contexthub;

int contexthub_is_run(void)
{
	if (contexthub->pdata->powermode_on)
		return nanohub_irq1_fired(contexthub);
	else
		return 1;
}

#define WAKEUP_TIMEOUT_MS	1000

int contexthub_request(void)
{
	if (contexthub->pdata->powermode_on)
		return request_wakeup_timeout(contexthub, WAKEUP_TIMEOUT_MS);
	else
		return 0;
}

void contexthub_release(void)
{
	if (contexthub->pdata->powermode_on)
		release_wakeup(contexthub);
}

int contexthub_read(uint8_t *rx, int timeout)
{
	return contexthub_ipc_read(contexthub, rx, timeout);
}

int contexthub_write(uint8_t *tx, int length)
{
	if (length > PACKET_SIZE_MAX) {
		pr_err("%s: invaild size:%d max is %d\n", __func__, length, PACKET_SIZE_MAX);
		return -EINVAL;
	}

	return contexthub_ipc_write(contexthub, tx, length);
}

struct host_data *contexthub_probe(struct device *dev, void *ipc_data)
{
	struct host_data *chub;
	int ret;

	if (!dev || !ipc_data) {
		pr_err("%s: Invalid platform_device.\n", __func__);
		return NULL;
	}

	chub = devm_kzalloc(dev, sizeof(struct host_data), GFP_KERNEL);
	if (!chub)
		return NULL;

	dev_set_drvdata(dev, chub);
	chub->dev = dev;
	chub->pdata = devm_kzalloc(dev, sizeof(struct nanohub_platform_data), GFP_KERNEL);
	if (!chub->pdata) {
		kfree(chub);
		return NULL;
	}
	chub->pdata->mailbox_client = ipc_data;

	ret = contexhub_drv_init(chub->dev);
	if (ret) {
		kfree(chub);
		dev_err(dev, "%s: Fails to drv init. ret:%d\n", __func__, ret);
		return NULL;
	}

	dev_info(dev, "%s is done\n", __func__);
	contexthub = chub;

	return chub;
}

int contexthub_remove(struct device *dev)
{
	struct host_data *chub = dev_get_drvdata(dev);

	if (!chub)
		return 0;

	kfree(chub);
	return 0;
}
MODULE_DESCRIPTION("Exynos Contexthub driver");
