/*
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include "swap_initializer.h"


static int set_enable(int enable)
{
	int ret = 0, change, enable_current;

	enable_current = swap_init_stat_get();

	change = ((!!enable_current) << 1) | (!!enable);
	switch (change) {
	case 0b01: /* init */
		ret = swap_init_init();
		break;
	case 0b10: /* uninit */
		ret = swap_init_uninit();
		break;
	default:
		ret = -EINVAL;
		break;
	}

	swap_init_stat_put();

	return ret;
}

static ssize_t read_enable(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	char buf[3];
	int enable;

	enable = swap_init_stat_get();
	swap_init_stat_put();

	if (enable)
		buf[0] = '1';
	else
		buf[0] = '0';
	buf[1] = '\n';
	buf[2] = '\0';

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static int do_write_enable(const char *buf, size_t size)
{
	if (size < 1)
		return -EINVAL;

	switch (buf[0]) {
	case '1':
		return set_enable(1);
	case '0':
		return set_enable(0);
	}

	return -EINVAL;
}

static ssize_t write_enable(struct file *file, const char __user *user_buf,
			    size_t count, loff_t *ppos)
{
	int ret;
	char buf[32];
	size_t buf_size;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	ret = do_write_enable(buf, buf_size);

	return ret ? ret : count;
}

static const struct file_operations fops_enable = {
	.owner = THIS_MODULE,
	.read = read_enable,
	.write = write_enable,
	.llseek = default_llseek,
};


static struct dentry *swap_dir;

/**
 * @brief Get debugfs dir.
 *
 * @return Pointer to dentry stuct.
 */
struct dentry *swap_debugfs_getdir(void)
{
	return swap_dir;
}
EXPORT_SYMBOL_GPL(swap_debugfs_getdir);

static int debugfs_dir_init(void)
{
	swap_dir = debugfs_create_dir("swap", NULL);
	if (swap_dir == NULL)
		return -ENOMEM;

	return 0;
}

static void debugfs_dir_exit(void)
{
	struct dentry *dir = swap_dir;

	swap_dir = NULL;
	debugfs_remove_recursive(dir);
}

/**
 * @brief Initializes SWAP debugfs.
 *
 * @return 0 on success, negative error code on error.
 */
int swap_debugfs_init(void)
{
	int ret;
	struct dentry *dentry;

	ret = debugfs_dir_init();
	if (ret)
		return ret;

	dentry = debugfs_create_file("enable", 0600, swap_dir, NULL,
				     &fops_enable);
	if (dentry == NULL) {
		debugfs_dir_exit();
		return -ENOMEM;
	}

	return 0;
}

/**
 * @brief Deinitializes SWAP debugfs and recursively removes all its files.
 *
 * @return Void.
 */
void swap_debugfs_uninit(void)
{
	debugfs_dir_exit();
}
