#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/limits.h>
#include <linux/list.h>
#include "loader_defs.h"
#include "loader_control.h"
#include "loader_module.h"

struct bin_desc {
	struct list_head list;
	struct dentry *dentry;
	char *filename;
};

struct list_desc {
	struct list_head list;
	rwlock_t lock;
	int cnt;
};

static struct list_desc ignored = {
	.list = LIST_HEAD_INIT(ignored.list),
	.lock = __RW_LOCK_UNLOCKED(&ignored.lock),
	.cnt = 0
};

static struct bin_desc *__alloc_binary(struct dentry *dentry, char *name,
				       int namelen)
{
	struct bin_desc *p = NULL;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	INIT_LIST_HEAD(&p->list);
	p->filename = kmalloc(namelen + 1, GFP_KERNEL);
	if (!p->filename)
		goto fail;
	memcpy(p->filename, name, namelen);
	p->filename[namelen] = '\0';
	p->dentry = dentry;

	return p;
fail:
	kfree(p);
	return NULL;
}

static void __free_binary(struct bin_desc *p)
{
	kfree(p->filename);
	kfree(p);
}

static void __free_ign_binaries(void)
{
	struct bin_desc *p, *n;
	struct list_head rm_head;

	INIT_LIST_HEAD(&rm_head);
	write_lock(&ignored.lock);
	list_for_each_entry_safe(p, n, &ignored.list, list) {
		list_move(&p->list, &rm_head);
	}
	ignored.cnt = 0;
	write_unlock(&ignored.lock);

	list_for_each_entry_safe(p, n, &rm_head, list) {
		list_del(&p->list);
		put_dentry(p->dentry);
		__free_binary(p);
	}
}

static bool __check_dentry_already_exist(struct dentry *dentry)
{
	struct bin_desc *p;
	bool ret = false;

	read_lock(&ignored.lock);
	list_for_each_entry(p, &ignored.list, list) {
		if (p->dentry == dentry) {
			ret = true;
			goto out;
		}
	}
out:
	read_unlock(&ignored.lock);

	return ret;
}

static int __add_ign_binary(struct dentry *dentry, char *filename)
{
	struct bin_desc *p;
	size_t len;

	if (__check_dentry_already_exist(dentry)) {
		printk(LOADER_PREFIX "Binary already exist\n");
		return EALREADY;
	}

	/* Filename should be < PATH_MAX */
	len = strnlen(filename, PATH_MAX);
	if (len == PATH_MAX)
		return -EINVAL;

	p = __alloc_binary(dentry, filename, len);
	if (!p)
		return -ENOMEM;

	write_lock(&ignored.lock);
	list_add_tail(&p->list, &ignored.list);
	ignored.cnt++;
	write_unlock(&ignored.lock);

	return 0;
}

static unsigned int __get_ign_names(char ***filenames_p)
{
	unsigned int i, ret = 0;
	struct bin_desc *p;
	char **a = NULL;

	read_lock(&ignored.lock);
	if (ignored.cnt == 0)
		goto out;

	a = kmalloc(sizeof(*a) * ignored.cnt, GFP_KERNEL);
	if (!a)
		goto out;

	i = 0;
	list_for_each_entry(p, &ignored.list, list) {
		if (i >= ignored.cnt)
			break;
		a[i++] = p->filename;
	}

	*filenames_p = a;
	ret = i;
out:
	read_unlock(&ignored.lock);
	return ret;
}



int lc_add_ignored_binary(char *filename)
{
	struct dentry *dentry = get_dentry(filename);
	int res = 0;

	if (dentry == NULL)
		return -EINVAL;

	res = __add_ign_binary(dentry, filename);
	if (res != 0)
		put_dentry(dentry);

	return res > 0 ? 0 : res;
}

int lc_clean_ignored_bins(void)
{
	__free_ign_binaries();

	return 0;
}

unsigned int lc_get_ignored_names(char ***filenames_p)
{
	return __get_ign_names(filenames_p);
}

void lc_release_ignored_names(char ***filenames_p)
{
	kfree(*filenames_p);
}

bool lc_check_dentry_is_ignored(struct dentry *dentry)
{
	struct bin_desc *p;
	bool ret = false;

	if (dentry == NULL)
		return false;

	read_lock(&ignored.lock);

	list_for_each_entry(p, &ignored.list, list) {
		if (p->dentry == dentry) {
			ret = true;
			break;
		}
	}

	read_unlock(&ignored.lock);

	return ret;
}

int lc_init(void)
{
	return 0;
}

void lc_exit(void)
{
	__free_ign_binaries();
}
