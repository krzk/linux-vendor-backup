/*
 * Tizen Global Lock device driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * Author: YoungJun Cho <yj44.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2, as published
 * by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hash.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "tizen_global_lock.h"

#define TGL_DEV_NAME		"tgl"

#define TGL_VERSION_MAJOR	1
#define TGL_VERSION_MINOR	1

#define TGL_HASH_BITS		4
#define TGL_HASH_BUCKETS	(1 << TGL_HASH_BITS)

#define TGL_DEFAULT_TIMEOUT_MS	1000

/**
 * struct tgl_device - tgl device structure
 * @dev: (misc )device pointer
 * @version: version data
 * @heads: hash heads for global node data
 * @lock: lock for hash heads
 */
static struct tgl_device {
	struct device *dev;

	struct tgl_ver_data version;

	struct hlist_head heads[TGL_HASH_BUCKETS];
	struct mutex lock;
} tgl;

/**
 * struct tgl_session - tgl session structure
 * @heads: hash heads for session node data
 * @locked_heads: hash heads for locked session node data
 * @lock: lock for hash heads
 */
struct tgl_session {
	struct hlist_head heads[TGL_HASH_BUCKETS];
	struct hlist_head locked_heads[TGL_HASH_BUCKETS];
	struct mutex lock;
};

/**
 * struct tgl_data - tgl data structure
 * @kref: reference count
 * @key: key for hash table
 * @type: lock type
 * @cnt: lock cnt
 * @owner: the last owner's tgid for debug
 * @timeout_ms: timeout for waiting event
 * @q: waiting event queue for lock
 * @list: waiting list
 * @lock: lock for waiting list
 * @data1: user data 1
 * @data2: user data 2
 */
struct tgl_data {
	struct kref kref;

	unsigned int key;
	atomic_t type;
	atomic_t cnt;
	pid_t owner;

	unsigned int timeout_ms;
	wait_queue_head_t q;
	struct list_head list;
	struct mutex lock;

	unsigned int data1;
	unsigned int data2;
};

/**
 * struct tgl_node - tgl node structure
 * @kref: reference count
 * @node: hash node for data
 * @data: struct tgl_data pointer
 */
struct tgl_node {
	struct kref kref;

	struct hlist_node node;

	struct tgl_data *data;
};

/**
 * struct tgl_wait_node - tgl wait node structure
 * @node: node for wait list
 * @session: struct tgl_session pointer
 */
struct tgl_wait_node {
	struct list_head node;
	struct tgl_session *session;
};

static inline unsigned int tgl_hash_idx(unsigned int key)
{
	return hash_32(key, TGL_HASH_BITS);
}

static inline void tgl_set_type(struct tgl_data *data, enum tgl_type_data type)
{
	atomic_set(&data->type, (int)type);
}

static inline bool tgl_is_type_none(struct tgl_data *data)
{
	return (atomic_read(&data->type) == (int)TGL_TYPE_NONE);
}

static inline bool tgl_is_type_read(struct tgl_data *data)
{
	return (atomic_read(&data->type) == (int)TGL_TYPE_READ);
}

static inline bool tgl_is_locked(struct tgl_data *data)
{
	return (atomic_read(&data->cnt) != 0);
}

static inline void tgl_lock_get(struct tgl_data *data)
{
	atomic_inc(&data->cnt);
}

static inline void tgl_lock_put(struct tgl_data *data)
{
	atomic_dec(&data->cnt);
}

static struct tgl_data *tgl_create_data(struct tgl_reg_data *reg_data)
{
	struct tgl_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return NULL;

	kref_init(&data->kref);

	data->key = reg_data->key;
	data->timeout_ms = reg_data->timeout_ms ? reg_data->timeout_ms :
				TGL_DEFAULT_TIMEOUT_MS;

	mutex_init(&data->lock);
	init_waitqueue_head(&data->q);
	INIT_LIST_HEAD(&data->list);

	return data;
}

static void tgl_release_data(struct kref *kref)
{
	struct tgl_data *data = container_of(kref, struct tgl_data, kref);

	mutex_destroy(&data->lock);
	kfree(data);
}

static void tgl_data_put(struct tgl_data *data)
{
	kref_put(&data->kref, tgl_release_data);
}

static struct tgl_node *tgl_create_node(struct tgl_data *data)
{
	struct tgl_node *node;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return NULL;

	kref_init(&node->kref);

	INIT_HLIST_NODE(&node->node);

	node->data = data;

	return node;
}

static void tgl_node_get(struct tgl_node *node)
{
	kref_get(&node->kref);
}

static void tgl_release_node(struct kref *kref)
{
	struct tgl_node *node = container_of(kref, struct tgl_node, kref);

	hlist_del(&node->node);
	kfree(node);
}

static int tgl_node_put(struct tgl_node *node)
{
	return kref_put(&node->kref, tgl_release_node);
}

static int tgl_add_data(struct hlist_head *heads, struct mutex *lock,
			struct tgl_data *data)
{
	struct tgl_node *node;
	struct hlist_head *head = &heads[tgl_hash_idx(data->key)];

	node = tgl_create_node(data);
	if (!node) {
		dev_err(tgl.dev, "%s: failed to create node data[%u]\n",
			__func__, data->key);
		return -ENOMEM;
	}

	if (lock)
		mutex_lock(lock);

	hlist_add_head(&node->node, head);

	if (lock)
		mutex_unlock(lock);

	return 0;
}

static int tgl_remove_data(struct hlist_head *heads, struct mutex *lock,
				struct tgl_data *data,
				void (*fini)(struct tgl_data *))
{
	struct tgl_node *node;
	struct tgl_data *tmp_data;
	struct hlist_head *head = &heads[tgl_hash_idx(data->key)];
	struct hlist_node *pos, *tnode;
	int ret = -ENOENT;

	if (lock)
		mutex_lock(lock);

	hlist_for_each_entry_safe(node, pos, tnode, head, node) {
		tmp_data = node->data;
		if (tmp_data->key == data->key) {
			ret = tgl_node_put(node);
			break;
		}
	}

	if ((ret > 0) && fini) {
		fini(data);
		ret = 0;
	}

	if (lock)
		mutex_unlock(lock);

	return ret;
}

static int tgl_open(struct inode *inode, struct file *file)
{
	struct tgl_session *session;
	int i;

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;

	mutex_init(&session->lock);
	for (i = 0; i < TGL_HASH_BUCKETS; i++) {
		INIT_HLIST_HEAD(&session->heads[i]);
		INIT_HLIST_HEAD(&session->locked_heads[i]);
	}

	file->private_data = (void *)session;

	return 0;
}

static void tgl_wakeup_q(struct tgl_data *data)
{
	if (!tgl_is_locked(data)) {
		/* reset lock info */
		tgl_set_type(data, TGL_TYPE_NONE);
		data->owner = 0;
	}

	/* wake up next waiter */
	if (waitqueue_active(&data->q))
		wake_up(&data->q);
}

static void tgl_remove_wait_node(struct tgl_data *data,
					struct tgl_session *session)
{
	struct tgl_wait_node *wait_node, *twait_node;

	mutex_lock(&data->lock);

	list_for_each_entry_safe(wait_node, twait_node, &data->list, node) {
		if (wait_node->session == session) {
			list_del(&wait_node->node);
			break;
		}
	}

	mutex_unlock(&data->lock);
}

static void tgl_cleanup_data(struct tgl_session *session)
{
	struct tgl_node *node;
	struct hlist_node *pos, *tnode;
	struct tgl_data *data;
	int ret, i;

	mutex_lock(&session->lock);

	/* unlock all locked nodes in session */
	for (i = 0; i < TGL_HASH_BUCKETS; i++) {
		hlist_for_each_entry_safe(node, pos, tnode,
						&session->locked_heads[i],
						node) {
			data = node->data;
			do {
				/* for multiple lock */
				tgl_lock_put(data);
				ret = tgl_node_put(node);
			} while (!ret);

			tgl_wakeup_q(data);
		}
	}

	/* remove nodes from session */
	for (i = 0; i < TGL_HASH_BUCKETS; i++) {
		hlist_for_each_entry_safe(node, pos, tnode,
						&session->heads[i],
						node) {
			data = node->data;

			/* check waiter */
			tgl_remove_wait_node(data, session);
			do {
				/* for multiple registration */
				ret = tgl_node_put(node);
			} while (!ret);

			/* remove it from global */
			ret = tgl_remove_data(tgl.heads, &tgl.lock, data,
						tgl_data_put);
			if (ret)
				dev_err(tgl.dev,
					"%s: failed to remove data[%u] from "
					"global[%d]\n",
					__func__, data->key, ret);
		}
	}

	mutex_unlock(&session->lock);
}

static int tgl_release(struct inode *inode, struct file *file)
{
	struct tgl_session *session = (struct tgl_session *)file->private_data;

	tgl_cleanup_data(session);

	mutex_destroy(&session->lock);
	kfree(session);

	file->private_data = NULL;

	return 0;
}

static struct tgl_node *tgl_find_node(struct hlist_head *heads,
					unsigned int key)
{
	struct tgl_node *node, *found = NULL;
	struct tgl_data *data;
	struct hlist_head *head = &heads[tgl_hash_idx(key)];
	struct hlist_node *pos;

	hlist_for_each_entry(node, pos, head, node) {
		data = node->data;
		if (data->key == key) {
			found = node;
			break;
		}
	}

	return found;
}

static int tgl_get_version(struct tgl_session *session, void __user *arg)
{
	struct tgl_ver_data ver_data;

	memcpy(&ver_data, &tgl.version, sizeof(ver_data));

	if (copy_to_user(arg, &ver_data, sizeof(ver_data)))
		return -EFAULT;

	return 0;
}

static int tgl_register(struct tgl_session *session, void __user *arg)
{
	struct tgl_reg_data reg_data;
	struct tgl_node *node;
	struct tgl_data *data;
	int ret;

	if (copy_from_user(&reg_data, (void __user *)arg, sizeof(reg_data)))
		return -EFAULT;

	mutex_lock(&session->lock);

	/* check session first for multiple registration */
	node = tgl_find_node(session->heads, reg_data.key);
	if (node) {
		tgl_node_get(node);
		mutex_unlock(&session->lock);
		return 0;
	}

	mutex_unlock(&session->lock);

	/* check global */
	mutex_lock(&tgl.lock);

	node = tgl_find_node(tgl.heads, reg_data.key);
	if (!node) {
		data = tgl_create_data(&reg_data);
		if (!data) {
			mutex_unlock(&tgl.lock);
			dev_err(tgl.dev, "%s: failed to create data[%u]\n",
				__func__, reg_data.key);
			return -ENOMEM;
		}

		ret = tgl_add_data(tgl.heads, NULL, data);
		if (ret) {
			mutex_unlock(&tgl.lock);
			dev_err(tgl.dev,
				"%s: failed to add data[%u] to global[%d]\n",
				__func__, data->key, ret);
			tgl_data_put(data);
			return ret;
		}
	} else {
		tgl_node_get(node);
		data = node->data;
	}

	mutex_unlock(&tgl.lock);

	/* add to session */
	ret = tgl_add_data(session->heads, &session->lock, data);
	if (ret) {
		int tmp_ret;

		dev_err(tgl.dev,
			"%s: failed to add data[%u] to session[%d]\n",
			__func__, data->key, ret);
		tmp_ret = tgl_remove_data(tgl.heads, &tgl.lock, data,
						tgl_data_put);
		if (tmp_ret) {
			dev_err(tgl.dev,
				"%s: failed to remove data[%u] from "
				"global[%d]\n", __func__, data->key, tmp_ret);
		}
	}

	return ret;
}

static int tgl_unregister(struct tgl_session *session, void __user *arg)
{
	struct tgl_reg_data reg_data;
	struct tgl_node *node;
	struct tgl_data *data;
	int ret;

	if (copy_from_user(&reg_data, (void __user *)arg, sizeof(reg_data)))
		return -EFAULT;

	/* check in session */
	mutex_lock(&session->lock);

	node = tgl_find_node(session->heads, reg_data.key);
	if (!node) {
		mutex_unlock(&session->lock);
		dev_err(tgl.dev,
			"%s: failed to find node data[%u] in session\n",
			__func__, reg_data.key);
		return -ENOENT;
	}
	data = node->data;

	/* for single registration and locked state */
	if (atomic_read(&node->kref.refcount) == 1) {
		if (tgl_find_node(session->locked_heads, reg_data.key)) {
			mutex_unlock(&session->lock);
			dev_err(tgl.dev,
				"%s: node data[%u] is in locked session yet\n",
				__func__, reg_data.key);
			return -EBUSY;
		}

		/* check waiter */
		tgl_remove_wait_node(data, session);
	}

	/* remove from session */
	if (tgl_node_put(node)) {
		/* remove from global */
		ret = tgl_remove_data(tgl.heads, &tgl.lock, data, tgl_data_put);
		if (ret) {
			mutex_unlock(&session->lock);
			dev_err(tgl.dev,
				"%s: failed to remove data[%u] from "
				"global[%d]\n", __func__, reg_data.key, ret);
			return ret;
		}
	}

	mutex_unlock(&session->lock);

	return 0;
}

static bool tgl_check_lock_condition(struct tgl_data *data,
					struct tgl_wait_node *wait_node,
					enum tgl_type_data type)
{
	bool ret = false;

	mutex_lock(&data->lock);

	/* FIXME: more generic way */
	if (&wait_node->node == data->list.next) {
		if (
			/* unlocked state */
			(!tgl_is_locked(data) && tgl_is_type_none(data)) ||
			/* read locked state */
			(tgl_is_locked(data) && tgl_is_type_read(data) &&
				(type == TGL_TYPE_READ)))
			ret = true;
	}

	mutex_unlock(&data->lock);

	return ret;
}

static int tgl_lock(struct tgl_session *session, void __user *arg)
{
	struct tgl_lock_data lock_data;
	struct tgl_node *node;
	struct tgl_data *data;
	struct tgl_wait_node wait_node;
	pid_t owner = task_tgid_nr(current);
	int ret;

	if (copy_from_user(&lock_data, (void __user *)arg, sizeof(lock_data)))
		return -EFAULT;

	/* check in session */
	mutex_lock(&session->lock);

	node = tgl_find_node(session->heads, lock_data.key);
	if (!node) {
		mutex_unlock(&session->lock);
		dev_err(tgl.dev,
			"%s: failed to find node data[%u] in session\n",
			__func__, lock_data.key);
		return -ENOENT;
	}
	data = node->data;

	/* for multiple lock */
	if (tgl_is_locked(data) &&
		tgl_is_type_read(data) && (lock_data.type == TGL_TYPE_READ)) {
		mutex_lock(&data->lock);
		/* for no waiter */
		if (list_empty(&data->list)) {
			mutex_unlock(&data->lock);
			tgl_lock_get(data);
			data->owner = owner;
			goto add_data;
		} else
			mutex_unlock(&data->lock);
	}

	/* add to waiter */
	mutex_lock(&data->lock);
	list_add_tail(&wait_node.node, &data->list);
	mutex_unlock(&data->lock);

	mutex_unlock(&session->lock);
	ret = wait_event_timeout(data->q, tgl_check_lock_condition(data,
						&wait_node, lock_data.type),
					msecs_to_jiffies(data->timeout_ms));
	mutex_lock(&session->lock);
	if (!ret) {
		dev_err(tgl.dev,
			"%s: timed out to lock with [%u] because of tgid[%d]",
			__func__, lock_data.key, data->owner);
		mutex_lock(&data->lock);
		list_del(&wait_node.node);
		mutex_unlock(&data->lock);
		tgl_wakeup_q(data);
		mutex_unlock(&session->lock);
		return -ETIMEDOUT;
	}

	tgl_lock_get(data);

	/* set lock info */
	tgl_set_type(data, lock_data.type);
	data->owner = owner;

	mutex_lock(&data->lock);
	list_del(&wait_node.node);
	/* for multiple lock, wake up next waiter */
	if (tgl_is_type_read(data) && !list_empty(&data->list)) {
		mutex_unlock(&data->lock);
		if (waitqueue_active(&data->q))
			wake_up(&data->q);
	} else
		mutex_unlock(&data->lock);

add_data:
	/* add to locked session */
	node = tgl_find_node(session->locked_heads, data->key);
	if (!node) {
		ret = tgl_add_data(session->locked_heads, NULL, data);
		if (ret) {
			dev_err(tgl.dev,
				"%s: failed to add data[%u] to locked "
				"session[%d]\n", __func__, data->key, ret);
			tgl_lock_put(data);
			tgl_wakeup_q(data);
			mutex_unlock(&session->lock);
			return ret;
		}
	} else
		tgl_node_get(node);

	mutex_unlock(&session->lock);

	return 0;
}

static int tgl_unlock(struct tgl_session *session, void __user *arg)
{
	struct tgl_lock_data lock_data;
	struct tgl_node *node;
	struct tgl_data *data;

	if (copy_from_user(&lock_data, (void __user *)arg, sizeof(lock_data)))
		return -EFAULT;

	/* check in locked session */
	mutex_lock(&session->lock);

	node = tgl_find_node(session->locked_heads, lock_data.key);
	if (!node) {
		node = tgl_find_node(session->heads, lock_data.key);
		if (node) {
			/* check waiter */
			tgl_remove_wait_node(node->data, session);
			mutex_unlock(&session->lock);
			return 0;
		}

		mutex_unlock(&session->lock);
		dev_err(tgl.dev,
			"%s: failed to find node data[%u] in locked session\n",
			__func__, lock_data.key);
		return -ENOENT;
	}
	data = node->data;

	tgl_lock_put(data);

	/* remove from locked session */
	if (tgl_node_put(node))
		tgl_wakeup_q(data);

	mutex_unlock(&session->lock);

	return 0;
}

static int tgl_set_usr_data(struct tgl_session *session, void __user *arg)
{
	struct tgl_usr_data usr_data;
	struct tgl_node *node;
	struct tgl_data *data;

	if (copy_from_user(&usr_data, (void __user *)arg, sizeof(usr_data)))
		return -EFAULT;

	mutex_lock(&session->lock);

	node = tgl_find_node(session->heads, usr_data.key);
	if (!node) {
		mutex_unlock(&session->lock);
		dev_err(tgl.dev,
			"%s: failed to find node data[%u] in session\n",
			__func__, usr_data.key);
		return -ENOENT;
	}
	data = node->data;

	data->data1 = usr_data.data1;
	data->data2 = usr_data.data2;

	mutex_unlock(&session->lock);

	return 0;
}

static int tgl_get_usr_data(struct tgl_session *session, void __user *arg)
{
	struct tgl_usr_data usr_data;
	struct tgl_node *node;
	struct tgl_data *data;

	if (copy_from_user(&usr_data, (void __user *)arg, sizeof(usr_data)))
		return -EFAULT;

	mutex_lock(&session->lock);

	node = tgl_find_node(session->heads, usr_data.key);
	if (!node) {
		mutex_unlock(&session->lock);
		dev_err(tgl.dev,
			"%s: failed to find node data[%u] in session\n",
			__func__, usr_data.key);
		return -ENOENT;
	}
	data = node->data;

	usr_data.data1 = data->data1;
	usr_data.data2 = data->data2;
	usr_data.status = tgl_is_locked(data) ? TGL_STATUS_LOCKED :
				TGL_STATUS_UNLOCKED;

	mutex_unlock(&session->lock);

	if (copy_to_user(arg, &usr_data, sizeof(usr_data)))
		return -EFAULT;

	return 0;
}

static long tgl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tgl_session *session = (struct tgl_session *)file->private_data;
	int ret;

	switch (cmd) {
	case TGL_IOCTL_GET_VERSION:
		ret = tgl_get_version(session, (void __user *)arg);
		if (ret)
			dev_err(tgl.dev,
				"%s: failed to get version[%d]\n",
				__func__, ret);
	case TGL_IOCTL_REGISTER:
		ret = tgl_register(session, (void __user *)arg);
		if (ret)
			dev_err(tgl.dev,
				"%s: failed to regsiter[%d]\n", __func__, ret);
		break;
	case TGL_IOCTL_UNREGISTER:
		ret = tgl_unregister(session, (void __user *)arg);
		if (ret)
			dev_err(tgl.dev,
				"%s: failed to unregsiter[%d]\n",
				__func__, ret);
		break;
	case TGL_IOCTL_LOCK:
		ret = tgl_lock(session, (void __user *)arg);
		if (ret)
			dev_err(tgl.dev,
				"%s: failed to lock[%d]\n", __func__, ret);
		break;
	case TGL_IOCTL_UNLOCK:
		ret = tgl_unlock(session, (void __user *)arg);
		if (ret)
			dev_err(tgl.dev,
				"%s: failed to unlock[%d]\n", __func__, ret);
		break;
	case TGL_IOCTL_SET_DATA:
		ret = tgl_set_usr_data(session, (void __user *)arg);
		if (ret)
			dev_err(tgl.dev,
				"%s: failed to set user data[%d]\n",
				__func__, ret);
		break;
	case TGL_IOCTL_GET_DATA:
		ret = tgl_get_usr_data(session, (void __user *)arg);
		if (ret)
			dev_err(tgl.dev,
				"%s: failed to get user data[%d]\n",
				__func__, ret);
		break;
	default:
		dev_err(tgl.dev,
			"%s: failed to call ioctl: tgid[%d]:0x%x\n",
			__func__, task_tgid_nr(current), cmd);
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static const struct file_operations tgl_fops = {
	.owner = THIS_MODULE,
	.open = tgl_open,
	.release = tgl_release,
	.unlocked_ioctl = tgl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tgl_ioctl,
#endif
};

static struct miscdevice tgl_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TGL_DEV_NAME,
	.fops = &tgl_fops,
};

static int __init tgl_init(void)
{
	struct tgl_ver_data *version = &tgl.version;
	int ret, i;

	version->major = TGL_VERSION_MAJOR;
	version->minor = TGL_VERSION_MINOR;

	ret = misc_register(&tgl_misc_device);
	if (ret) {
		pr_err("%s: failed to register misc device[%d]\n",
			__func__, ret);
		return ret;
	}
	tgl.dev = tgl_misc_device.this_device;

	mutex_init(&tgl.lock);
	for (i = 0; i < TGL_HASH_BUCKETS; i++)
		INIT_HLIST_HEAD(&tgl.heads[i]);

	return 0;
}
module_init(tgl_init);

static void __exit tgl_exit(void)
{
	mutex_destroy(&tgl.lock);
	misc_deregister(&tgl_misc_device);
}
module_exit(tgl_exit);

MODULE_DESCRIPTION("Tizen Global Lock");
MODULE_AUTHOR("YoungJun Cho <yj44.cho@samsung.com>");
MODULE_LICENSE("GPL v2");
