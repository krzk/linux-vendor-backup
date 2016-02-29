/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/dmabuf-sync.h>

#include "dmabuf-sync-profile.h"

#define PROFILE_START	0x1
#define PROFILE_STOP	0x2
#define PROFILE_CLEAR	0x3

struct dmabuf_sync_profile {
	struct list_head	list;
	unsigned long long	time;
	unsigned long		dmabuf;
	size_t			size;
	unsigned long		task;
	unsigned int		type;
	unsigned int		owner;
	char			dma_name[64];
	char			process_name[TASK_COMM_LEN];
};

struct dmabuf_sync_profile_debugfs {
	const char		*name;
	bool			is_writable;
	int (*show)(struct seq_file *, void *);
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
};

static struct list_head profile_node_head;
static struct mutex profile_lock;
static unsigned int profile_debugfs_enabled;

static struct dentry *profile_debugfs_dir;

void dmabuf_sync_profile_collect(struct dmabuf_sync *sync, unsigned int type)
{
	struct dmabuf_sync_profile *node;
	struct dmabuf_sync_object *sobj;

	if (profile_debugfs_enabled != PROFILE_START)
		return;

	if (!sync) {
		WARN_ON(1);
		return;
	}

	list_for_each_entry(sobj, &sync->syncs, head) {
		node = kzalloc(sizeof(*node), GFP_KERNEL);
		if (!node) {
			WARN_ON(1);
			/* TODO */
			return;
		}

		node->dmabuf = (unsigned long)sobj->dmabuf;
		node->size = sobj->dmabuf->size;
		node->type = type;
		node->owner = sobj->access_type;

		node->time = cpu_clock(raw_smp_processor_id());
		do_div(node->time, NSEC_PER_USEC);

		node->task = (unsigned long)current;
		strncpy(node->process_name, current->comm,
			ARRAY_SIZE(node->process_name) - 1);
		strncpy(node->dma_name, sync->name,
			ARRAY_SIZE(node->dma_name) - 1);

		mutex_lock(&profile_lock);
		list_add_tail(&node->list, &profile_node_head);
		mutex_unlock(&profile_lock);
	}
}

void dmabuf_sync_profile_collect_single(struct dma_buf *dmabuf, unsigned int type,
					unsigned int owner)
{
	struct dmabuf_sync_profile *node;

	if (profile_debugfs_enabled != PROFILE_START)
		return;

	if (!dmabuf) {
		WARN_ON(1);
		return;
	}

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		WARN_ON(1);
		return;
	}

	node->dmabuf = (unsigned long)dmabuf;
	node->size = dmabuf->size;
	node->type = type;
	node->owner = owner;

	node->time = cpu_clock(raw_smp_processor_id());
	do_div(node->time, NSEC_PER_USEC);

	node->task = (unsigned long)current;
	strncpy(node->process_name, current->comm,
			ARRAY_SIZE(node->process_name) - 1);

	mutex_lock(&profile_lock);
	list_add_tail(&node->list, &profile_node_head);
	mutex_unlock(&profile_lock);
}

#if defined(CONFIG_DEBUG_FS)
static int profile_debugfs_show(struct seq_file *s, void *data)
{
	struct dmabuf_sync_profile *node;

	if (!profile_debugfs_enabled) {
		printk(KERN_INFO "start dmabuf sync profile.\n");
		return 0;
	}

	mutex_lock(&profile_lock);

	list_for_each_entry(node, &profile_node_head, list) {
		unsigned long sec, remainder;

		sec = node->time;
		remainder = do_div(sec, USEC_PER_SEC);

		seq_printf(s, "0x%08x\t", (unsigned int)node->dmabuf);
		seq_printf(s, "0x%08x\t", (unsigned int)node->size);

		switch (node->type) {
		case DMABUF_SYNC_PROFILE_TRY_LOCK:
			seq_printf(s, "try_lock\t");
			break;
		case DMABUF_SYNC_PROFILE_LOCK:
			seq_printf(s, "    lock\t");
			break;
		case DMABUF_SYNC_PROFILE_START:
			seq_printf(s, "   start\t");
			break;
		case DMABUF_SYNC_PROFILE_END:
			seq_printf(s, "     end\t");
			break;
		case DMABUF_SYNC_PROFILE_UNLOCK:
			seq_printf(s, "  unlock\t");
			break;
		default:
			break;
		}

		if (node->owner & DMA_BUF_ACCESS_DMA)
			seq_printf(s, "DMA-%5s\t", node->dma_name);
		else
			seq_printf(s, "CPU-%5s\t", "NULL");

		seq_printf(s, "%5lu.%06lu\t", sec, remainder);
		seq_printf(s, "0x%8x\t", (unsigned int)node->task);
		seq_printf(s, "%s\n", node->process_name);
	}

	mutex_unlock(&profile_lock);

	return 0;
}

static ssize_t profile_debugfs_write(struct file *file,
						const char __user *buf,
						size_t size, loff_t *ppos)
{
	unsigned long value;
	char *cmd_str;

	cmd_str = kzalloc(size, GFP_KERNEL);
	if (!cmd_str)
		return -ENOMEM;

	if (copy_from_user(cmd_str, buf, size) != 0) {
		size = -EINVAL;
		goto err_free;
	}

	if (!strncmp(cmd_str, "start", 5))
		value = PROFILE_START;
	else if (!strncmp(cmd_str, "stop", 4))
		value = PROFILE_STOP;
	else if (!strncmp(cmd_str, "clear", 5))
		value = PROFILE_CLEAR;
	else {
		printk(KERN_WARNING "invalid command\n");
		size = -EINVAL;
		goto err_free;
	}

	if (profile_debugfs_enabled == value) {
		printk(KERN_WARNING "same value.\n");
		size = -EINVAL;
		goto err_free;
	}

	profile_debugfs_enabled = value;

	if (value == PROFILE_CLEAR) {
		struct dmabuf_sync_profile *node, *node_next;

		mutex_lock(&profile_lock);

		list_for_each_entry_safe(node, node_next, &profile_node_head,
					 list) {
			list_del_init(&node->list);
			kfree(node);
		}

		mutex_unlock(&profile_lock);

		printk(KERN_INFO "cleared all profile data.\n");
	}

err_free:
	kfree(cmd_str);

	return size;
}

static struct dmabuf_sync_profile_debugfs profile_debugfs_tbl[] = {
	{"profile", true, profile_debugfs_show,
				profile_debugfs_write},
};

static int dmabuf_sync_profile_debugfs_open(struct inode *inode, struct file *file)
{
	struct dmabuf_sync_profile_debugfs *tbl = inode->i_private;

	return single_open(file, tbl->show, tbl);
}

static int dmabuf_sync_profile_debugfs_write(struct file *file,
						const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct seq_file *seq = file->private_data;
	struct dmabuf_sync_profile_debugfs *tbl = seq->private;

	if (!tbl->write) {
		printk(KERN_WARNING "No write function\n");
		return -EACCES;
	}

	return tbl->write(file, user_buf, count, ppos);
}

static const struct file_operations profile_debugfs_fops = {
	.open		= dmabuf_sync_profile_debugfs_open,
	.read		= seq_read,
	.write		= dmabuf_sync_profile_debugfs_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dmabuf_sync_profile_init_debugfs(void)
{
	int ret, i;

	profile_debugfs_dir = debugfs_create_dir("dmabuf_sync_profile", NULL);
	if (!profile_debugfs_dir)
		return -EFAULT;

	for (i = 0; i < ARRAY_SIZE(profile_debugfs_tbl); i++) {
		struct dentry *entry;
		umode_t mode = S_IFREG | S_IRUGO;

		if (profile_debugfs_tbl[i].is_writable)
			mode |= S_IWUSR;

		entry = debugfs_create_file(profile_debugfs_tbl[i].name, mode,
						profile_debugfs_dir,
						&profile_debugfs_tbl[i],
						&profile_debugfs_fops);
		if (!entry) {
			ret = -EFAULT;
			goto err;
		}
	}

	return 0;

err:
	debugfs_remove_recursive(profile_debugfs_dir);
	return ret;
}

#endif

static int __init dmabuf_sync_profile_module_init(void)
{
	INIT_LIST_HEAD(&profile_node_head);

	mutex_init(&profile_lock);

#if defined(CONFIG_DEBUG_FS)
	return dmabuf_sync_profile_init_debugfs();
#endif
	return 0;
}

static void __exit dmabuf_sync_profile_module_deinit(void)
{
#if defined(CONFIG_DEBUG_FS)
	if (profile_debugfs_dir)
		debugfs_remove_recursive(profile_debugfs_dir);
#endif
}

module_init(dmabuf_sync_profile_module_init);
module_exit(dmabuf_sync_profile_module_deinit);

