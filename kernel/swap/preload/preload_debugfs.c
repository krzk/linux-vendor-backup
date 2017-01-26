#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/limits.h>
#include <asm/uaccess.h>
#include <master/swap_debugfs.h>
#include "preload.h"
#include "preload_module.h"
#include "preload_debugfs.h"
#include "preload_control.h"

static const char PRELOAD_FOLDER[] = "preload";
static const char PRELOAD_HANDLER[] = "handler";
static const char PRELOAD_TARGET[] = "target_binaries";
static const char PRELOAD_IGNORED[] = "ignored_binaries";
static const char PRELOAD_BINARIES_LIST[] = "bins_list";
static const char PRELOAD_BINARIES_ADD[] = "bins_add";
static const char PRELOAD_BINARIES_REMOVE[] = "bins_remove";

static struct dentry *preload_root;
static struct dentry *target_list = NULL;
static struct dentry *target_add = NULL;
static struct dentry *target_remove = NULL;
static struct dentry *ignored_list = NULL;
static struct dentry *ignored_add = NULL;
static struct dentry *ignored_remove = NULL;


/* ===========================================================================
 * =                                BIN PATH                                 =
 * ===========================================================================
 */

static ssize_t bin_add_write(struct file *file, const char __user *buf,
			   size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	path = kmalloc(len, GFP_KERNEL);
	if (path == NULL) {
		ret = -ENOMEM;
		goto bin_add_write_out;
	}

	if (copy_from_user(path, buf, len)) {
		ret = -EINVAL;
		goto bin_add_write_out;
	}

	path[len - 1] = '\0';

	if (file->f_path.dentry == target_add)
		ret = pc_add_instrumented_binary(path);
	else if (file->f_path.dentry == ignored_add)
		ret = pc_add_ignored_binary(path);
	else {
		/* Should never occur */
		printk(PRELOAD_PREFIX "%s() called for invalid file %s!\n", __func__,
		       file->f_path.dentry->d_name.name);
		ret = -EINVAL;
		goto bin_add_write_out;
	}


	if (ret != 0) {
		printk(PRELOAD_PREFIX "Cannot add binary %s\n", path);
		ret = -EINVAL;
		goto bin_add_write_out;
	}

	ret = len;

bin_add_write_out:
	kfree(path);

	return ret;
}

static ssize_t bin_remove_write(struct file *file, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	ssize_t ret;

	if (file->f_path.dentry == target_remove)
		ret = pc_clean_instrumented_bins();
	else if (file->f_path.dentry == ignored_remove)
		ret = pc_clean_ignored_bins();
	else {
		/* Should never occur */
		printk(PRELOAD_PREFIX "%s() called for invalid file %s!\n", __func__,
		       file->f_path.dentry->d_name.name);
		ret = -EINVAL;
		goto bin_remove_write_out;
	}

	if (ret != 0) {
		printk(PRELOAD_PREFIX "Error during clean!\n");
		ret = -EINVAL;
		goto bin_remove_write_out;
	}

	ret = len;

bin_remove_write_out:
	return ret;
}

static ssize_t bin_list_read(struct file *file, char __user *usr_buf,
				 size_t count, loff_t *ppos)
{
	unsigned int i;
	unsigned int files_cnt = 0;
	ssize_t len = 0, tmp, ret = 0;
	char **filenames = NULL;
	char *buf = NULL;
	char *ptr = NULL;

	if (file->f_path.dentry == target_list) {
		files_cnt = pc_get_target_names(&filenames);
	} else if (file->f_path.dentry == ignored_list) {
		files_cnt = pc_get_ignored_names(&filenames);
	} else {
		/* Should never occur */
		printk(PRELOAD_PREFIX "%s() called for invalid file %s!\n", __func__,
		       file->f_path.dentry->d_name.name);
		ret = 0;
		goto bin_list_read_out;
	}

	if (files_cnt == 0) {
		printk(PRELOAD_PREFIX "Cannot read binaries names!\n");
		ret = 0;
		goto bin_list_read_fail;
	}

	for (i = 0; i < files_cnt; i++)
		len += strlen(filenames[i]);

	buf = kmalloc(len + files_cnt, GFP_KERNEL);
	if (buf == NULL) {
		ret = 0;
		goto bin_list_read_fail;
	}

	ptr = buf;

	for (i = 0; i < files_cnt; i++) {
		tmp = strlen(filenames[i]);
		memcpy(ptr, filenames[i], tmp);
		ptr += tmp;
		*ptr = '\n';
		ptr += 1;
	}

	ret = simple_read_from_buffer(usr_buf, count, ppos, buf, len);

	kfree(buf);

bin_list_read_fail:
	if (file->f_path.dentry == target_list) {
		pc_release_target_names(&filenames);
	} else if (file->f_path.dentry == ignored_list)  {
		pc_release_ignored_names(&filenames);
	} else {
		/* Should never occur */
		printk(PRELOAD_PREFIX "%s() called for invalid file %s!\n", __func__,
		       file->f_path.dentry->d_name.name);
		ret = 0;
	}

bin_list_read_out:
	return ret;
}

static const struct file_operations bin_list_file_ops = {
	.owner = THIS_MODULE,
	.read = bin_list_read
};

static const struct file_operations bin_add_file_ops = {
	.owner = THIS_MODULE,
	.write = bin_add_write,
};

static const struct file_operations bin_remove_file_ops = {
	.owner = THIS_MODULE,
	.write = bin_remove_write,
};

/* ===========================================================================
 * =                              HANDLER                                    =
 * ===========================================================================
 */


static ssize_t handler_write(struct file *file, const char __user *buf,
			     size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	path = kmalloc(len, GFP_KERNEL);
	if (path == NULL) {
		ret = -ENOMEM;
		goto handlers_path_write_out;
	}

	if (copy_from_user(path, buf, len)) {
		ret = -EINVAL;
		goto handlers_path_write_out;
	}

	path[len - 1] = '\0';

	if (pm_set_handler(path) != 0) {
		printk(PRELOAD_PREFIX "Cannot set handler path %s\n", path);
		ret = -EINVAL;
		goto handlers_path_write_out;
	}

	ret = len;

handlers_path_write_out:
	kfree(path);

	return ret;
}

static const struct file_operations handler_file_ops = {
	.owner = THIS_MODULE,
	.write = handler_write,
};




int pd_init(void)
{
	struct dentry *swap_dentry, *root, *target_path, *ignored_path, *handler;
	int ret;

	ret = -ENODEV;
	if (!debugfs_initialized())
		goto fail;

	ret = -ENOENT;
	swap_dentry = swap_debugfs_getdir();
	if (!swap_dentry)
		goto fail;

	ret = -ENOMEM;
	root = debugfs_create_dir(PRELOAD_FOLDER, swap_dentry);
	if (IS_ERR_OR_NULL(root))
		goto fail;

	preload_root = root;

	target_path = debugfs_create_dir(PRELOAD_TARGET, root);
	if (IS_ERR_OR_NULL(target_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	target_list = debugfs_create_file(PRELOAD_BINARIES_LIST,
					  PRELOAD_DEFAULT_PERMS, target_path, NULL,
					  &bin_list_file_ops);
	if (IS_ERR_OR_NULL(target_list)) {
		ret = -ENOMEM;
		goto remove;
	}

	target_add = debugfs_create_file(PRELOAD_BINARIES_ADD,
					 PRELOAD_DEFAULT_PERMS, target_path, NULL,
					 &bin_add_file_ops);
	if (IS_ERR_OR_NULL(target_add)) {
		ret = -ENOMEM;
		goto remove;
	}

	target_remove = debugfs_create_file(PRELOAD_BINARIES_REMOVE,
					    PRELOAD_DEFAULT_PERMS, target_path,
					    NULL, &bin_remove_file_ops);
	if (IS_ERR_OR_NULL(target_remove)) {
		ret = -ENOMEM;
		goto remove;
	}

	ignored_path = debugfs_create_dir(PRELOAD_IGNORED, root);
	if (IS_ERR_OR_NULL(ignored_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	ignored_list = debugfs_create_file(PRELOAD_BINARIES_LIST,
					   PRELOAD_DEFAULT_PERMS, ignored_path,
					   NULL, &bin_list_file_ops);
	if (IS_ERR_OR_NULL(ignored_list)) {
		ret = -ENOMEM;
		goto remove;
	}

	ignored_add = debugfs_create_file(PRELOAD_BINARIES_ADD,
					  PRELOAD_DEFAULT_PERMS, ignored_path, NULL,
					  &bin_add_file_ops);
	if (IS_ERR_OR_NULL(ignored_add)) {
		ret = -ENOMEM;
		goto remove;
	}

	ignored_remove = debugfs_create_file(PRELOAD_BINARIES_REMOVE,
					     PRELOAD_DEFAULT_PERMS, ignored_path, NULL,
					     &bin_remove_file_ops);
	if (IS_ERR_OR_NULL(ignored_remove)) {
		ret = -ENOMEM;
		goto remove;
	}

	handler = debugfs_create_file(PRELOAD_HANDLER, PRELOAD_DEFAULT_PERMS,
				  preload_root, NULL, &handler_file_ops);
	if (IS_ERR_OR_NULL(handler)) {
		ret = -ENOMEM;
		goto remove;
	}

	return 0;

remove:

	debugfs_remove_recursive(root);

fail:
	printk(PRELOAD_PREFIX "Debugfs initialization failure: %d\n", ret);

	return ret;
}

void pd_exit(void)
{
	if (preload_root)
		debugfs_remove_recursive(preload_root);
	target_list = NULL;
	target_add = NULL;
	target_remove = NULL;
	ignored_list = NULL;
	ignored_add = NULL;
	ignored_remove = NULL;
	preload_root = NULL;
}
