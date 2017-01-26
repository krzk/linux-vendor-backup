#include <linux/module.h>
#include <linux/dcache.h>
#include <linux/percpu.h>
#include <linux/namei.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <swap/swap_fops.h>
#include <swap/swap_syshook.h>
#include <kprobe/swap_kprobes.h>
#include <kprobe/swap_ktd.h>
#include <writer/event_filter.h>
#include <asm/unistd32.h>
#include "ks_map.h"
#include "ksf_msg.h"
#include "file_ops.h"

#define FOPS_PREFIX "[FILE_OPS] "

#define PT_FILE 0x4 /* probe type FILE(04) */

/* path buffer size */
enum { PATH_LEN = 512 };

struct file_probe {
	int id;
	const char *args;
	int subtype;
	struct syshook_probe sysp;
	int syscall_id;
};

#define to_file_probe(_p) container_of(_p, struct file_probe, sysp)

/* common private data */
struct file_private {
	struct dentry *dentry;
};

/* open/creat private data */
struct open_private {
	int dfd;
	const char __user *name;
	int ret;
};

/* locks private data */
struct flock_private {
	struct dentry *dentry;
	int subtype;
};

#define DECLARE_HANDLER(_name) \
	void _name(struct syshook_probe *, struct pt_regs *)

/* generic handlers forward declaration */
static DECLARE_HANDLER(generic_entry_handler);
static DECLARE_HANDLER(generic_ret_handler);
/* open/creat handlers */
static DECLARE_HANDLER(open_entry_handler);
static DECLARE_HANDLER(open_ret_handler);
/* lock handlers */
static DECLARE_HANDLER(lock_entry_handler);
static DECLARE_HANDLER(lock_ret_handler);

#define FILE_OPS_OPEN_LIST \
	X(open, sdd), \
	X(openat, dsdd), \
	X(creat, sd)

#define FILE_OPS_CLOSE_LIST \
	X(close, d)

#define FILE_OPS_READ_LIST \
	X(read, dpd), \
	X(readv, dpd), \
	X(pread64, dpxx), \
	X(preadv, dpxxx)

#define FILE_OPS_WRITE_LIST \
	X(write, dpd), \
	X(writev, dpd), \
	X(pwrite64, dpxx), \
	X(pwritev, dpxxx)

#define FILE_OPS_LOCK_LIST \
	X(fcntl, ddd), \
	X(fcntl64, ddd), \
	X(flock, dd)

#define FILE_OPS_LIST \
	FILE_OPS_OPEN_LIST, \
	FILE_OPS_CLOSE_LIST, \
	FILE_OPS_READ_LIST, \
	FILE_OPS_WRITE_LIST, \
	FILE_OPS_LOCK_LIST

#define X(_name, _args) \
	id_sys_##_name
enum {
	FILE_OPS_LIST
};
#undef X

#define __FILE_PROBE_INITIALIZER(_name, _args, _subtype, _dtype, _entry, _ret) \
	{ \
		.id = id_sys_##_name, \
		.args = #_args, \
		.subtype = _subtype, \
		.syscall_id = __NR_##_name, \
		.sysp = { \
			.sys_entry = _entry, \
			.sys_exit = _ret, \
		} \
	}

static struct file_probe fprobes[] = {
#define X(_name, _args) \
	[id_sys_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_OPEN, \
						    struct open_private, \
						    open_entry_handler, \
						    open_ret_handler)
	FILE_OPS_OPEN_LIST,
#undef X

#define X(_name, _args) \
	[id_sys_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_CLOSE, \
						    struct file_private, \
						    generic_entry_handler, \
						    generic_ret_handler)
	FILE_OPS_CLOSE_LIST,
#undef X

#define X(_name, _args) \
	[id_sys_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_READ, \
						    struct file_private, \
						    generic_entry_handler, \
						    generic_ret_handler)
	FILE_OPS_READ_LIST,
#undef X

#define X(_name, _args) \
	[id_sys_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_WRITE, \
						    struct file_private, \
						    generic_entry_handler, \
						    generic_ret_handler)
	FILE_OPS_WRITE_LIST,
#undef X

#define X(_name, _args) \
	[id_sys_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_OTHER, \
						    struct flock_private, \
						    lock_entry_handler, \
						    lock_ret_handler)
	FILE_OPS_LOCK_LIST
#undef X
};

static void *fops_key_func(void *);
static int fops_cmp_func(void *, void *);

/* percpu buffer to hold the filepath inside handlers */
static DEFINE_PER_CPU(char[PATH_LEN], __path_buf);

/* map to hold 'interesting' files */
static DEFINE_MAP(__map, fops_key_func, fops_cmp_func);
static DEFINE_RWLOCK(__map_lock);

/* enabled/disabled flag */
static int fops_enabled;
static DEFINE_MUTEX(fops_lock);

/* GET/PUT debug stuff */
static int file_get_put_balance;
static int dentry_get_put_balance;


/* should be called only from handlers (with preemption disabled) */
static inline char *fops_path_buf(void)
{
	return __get_cpu_var(__path_buf);
}

static inline unsigned fops_dcount(const struct dentry *dentry)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
	return dentry->d_count;
#else
	return d_count(dentry);
#endif
}

/* kernel function args */
#define fops_karg(_type, _regs, _idx) ((_type)swap_get_karg(_regs, _idx))
/* syscall args */
#define fops_sarg(_type, _regs, _idx) ((_type)swap_get_sarg(_regs, _idx))
/* retval */
#define fops_ret(_type, _regs) ((_type)regs_return_value(_regs))

#define F_ADDR(_rp) ((unsigned long)(_rp)->kp.addr) /* function address */
#define R_ADDR(_ri) ((unsigned long)(_ri)->ret_addr) /* return adress */

static void *fops_key_func(void *data)
{
	/* use ((struct dentry *)data)->d_inode pointer as map key to handle
	 * symlinks/hardlinks the same way as the original file */
	return data;
}

static int fops_cmp_func(void *key_a, void *key_b)
{
	return key_a - key_b;
}

static inline struct map *__get_map(void)
{
	return &__map;
}

static inline struct map *get_map_read(void)
{
	read_lock(&__map_lock);

	return __get_map();
}

static inline void put_map_read(struct map *map)
{
	read_unlock(&__map_lock);
}

static inline struct map *get_map_write(void)
{
	write_lock(&__map_lock);

	return __get_map();
}

static inline void put_map_write(struct map *map)
{
	write_unlock(&__map_lock);
}

static DEFINE_SPINLOCK(file_lock);

static struct file *__fops_fget(int fd)
{
	struct file *file;

	spin_lock(&file_lock);
	file = fget(fd);
	if (IS_ERR_OR_NULL(file))
		file = NULL;
	else
		file_get_put_balance++;
	spin_unlock(&file_lock);

	return file;
}

static void __fops_fput(struct file *file)
{
	spin_lock(&file_lock);
	file_get_put_balance--;
	fput(file);
	spin_unlock(&file_lock);
}


static DEFINE_SPINLOCK(dentry_lock);

static struct dentry *__fops_dget(struct dentry *dentry)
{
	struct dentry *d;

	spin_lock(&dentry_lock);
	dentry_get_put_balance++;
	d = dget(dentry);
	spin_unlock(&dentry_lock);

	return d;
}

static void __fops_dput(struct dentry *dentry)
{
	spin_lock(&dentry_lock);
	dentry_get_put_balance--;
	dput(dentry);
	spin_unlock(&dentry_lock);
}

static int fops_dinsert(struct dentry *dentry)
{
	struct map *map;
	int ret;

	map = get_map_write();
	ret = insert(map, __fops_dget(dentry));
	put_map_write(map);

	if (ret)
		__fops_dput(dentry);

	/* it's ok if dentry is already inserted */
	return ret == -EEXIST ? 0 : ret;
}

static struct dentry *fops_dsearch(struct dentry *dentry)
{
	struct dentry *found;
	struct map *map;

	map = get_map_read();
	found = search(map, map->key_f(dentry));
	put_map_read(map);

	return found;
}

static struct dentry *fops_dremove(struct dentry *dentry)
{
	struct dentry *removed;
	struct map *map;

	map = get_map_write();
	removed = remove(map, map->key_f(dentry));
	put_map_write(map);

	if (removed)
		__fops_dput(removed);

	return removed;
}

static int fops_fcheck(struct task_struct *task, struct file *file)
{
	struct dentry *dentry;

	if (!task || !file)
		return -EINVAL;

	dentry = file->f_path.dentry;

	/* check if it is a regular file */
	if (!S_ISREG(dentry->d_inode->i_mode))
		return -EBADF;

	if (check_event(task))
		/* it is 'our' task: just add the dentry to the map */
		return fops_dinsert(dentry) ? : -EAGAIN;
	else
		/* not 'our' task: check if the file is 'interesting' */
		return fops_dsearch(dentry) ? 0 : -ESRCH;
}

static char *fops_fpath(struct file *file, char *buf, int buflen)
{
	char *filename;

	path_get(&file->f_path);
	filename = d_path(&file->f_path, buf, buflen);
	path_put(&file->f_path);

	if (IS_ERR_OR_NULL(filename)) {
		printk(FOPS_PREFIX "d_path FAILED: %ld\n", PTR_ERR(filename));
		buf[0] = '\0';
		filename = buf;
	}

	return filename;
}

static void fops_ktd_init(struct task_struct *task, void *data);
static void fops_ktd_exit(struct task_struct *task, void *data);

static struct ktask_data fops_ktd = {
	.init = fops_ktd_init,
	.exit = fops_ktd_exit,
};

static void fops_ktd_init(struct task_struct *task, void *data)
{
	memset(data, 0, fops_ktd.size);
}

static void fops_ktd_exit(struct task_struct *task, void *data)
{
}

static void *priv_data_get(void)
{
	return swap_ktd(&fops_ktd, current);
}

static void generic_entry_handler(struct syshook_probe *p, struct pt_regs *regs)
{
	struct file_probe *fprobe = to_file_probe(p);
	struct file_private *priv = (struct file_private *)priv_data_get();
	int fd = fops_sarg(int, regs, 0);
	struct file *file;

	file = __fops_fget(fd);
	if (fops_fcheck(current, file) == 0) {
		char *buf;

		preempt_disable();
		buf = fops_path_buf();
		ksf_msg_file_entry(regs, fd, fops_fpath(file, buf, PATH_LEN),
				   fprobe->subtype);
		preempt_enable();

		priv->dentry = file->f_path.dentry;
	} else {
		priv->dentry = NULL;
	}

	if (file)
		__fops_fput(file);
}

static void generic_ret_handler(struct syshook_probe *p, struct pt_regs *regs)
{
	struct file_private *priv = (struct file_private *)priv_data_get();

	if (priv->dentry) {
		struct file_probe *fprobe = to_file_probe(p);

		ksf_msg_file_exit(regs, fprobe->subtype, 'x');
	}
}

static int open_private_init(const char *args, struct pt_regs *regs,
			     struct open_private *priv)
{
	int ret = 0;

	switch (args[0]) {
	case 'd': /* file name: relative to fd */
		if (args[1] != 's') {
			ret = -EINVAL;
			break;
		}
		priv->dfd = fops_sarg(int, regs, 0);
		priv->name = fops_sarg(const char __user *, regs, 1);
		break;
	case 's': /* file name: absolute or relative to CWD */
		priv->dfd = AT_FDCWD;
		priv->name = fops_sarg(const char __user *, regs, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	priv->ret = ret;

	return ret;
}

static void open_entry_handler(struct syshook_probe *p, struct pt_regs *regs)
{
	struct file_probe *fprobe = to_file_probe(p);
	struct open_private *priv = (struct open_private *)priv_data_get();

	open_private_init(fprobe->args, regs, priv);
	/* FIXME entry event will be sent in open_ret_handler: cannot
	 * perform a file lookup in atomic context */
}

static void open_ret_handler(struct syshook_probe *p, struct pt_regs *regs)
{
	struct open_private *priv = (struct open_private *)priv_data_get();

	if (priv->ret == 0) {
		struct file_probe *fprobe = to_file_probe(p);
		int fd = fops_ret(int, regs);
		struct file *file = __fops_fget(fd);

		if (fops_fcheck(current, file) == 0) {
			char *buf;
			const char *path;

			preempt_disable();
			buf = fops_path_buf();
			path = fops_fpath(file, buf, PATH_LEN);
			ksf_msg_file_entry_open(regs, fd, path,
						fprobe->subtype, priv->name);
			preempt_enable();

			ksf_msg_file_exit(regs, fprobe->subtype, 'x');
		}

		if (file)
			__fops_fput(file);
	}
}

/* wrapper for 'struct flock*' data */
struct lock_arg {
	int type;
	int whence;
	s64 start;
	s64 len;
};

/* TODO copy_from_user */
#define __lock_arg_init(_type, _regs, _arg) \
	do { \
		_type __user *flock = fops_sarg(_type __user *, _regs, 2); \
		_arg->type = flock->l_type; \
		_arg->whence = flock->l_whence; \
		_arg->start = flock->l_start; \
		_arg->len = flock->l_len; \
	} while (0)

static int lock_arg_init(int id, struct pt_regs *regs, struct lock_arg *arg)
{
	unsigned int cmd = fops_sarg(unsigned int, regs, 1);
	int ret = 0;

	switch (id) {
	case id_sys_fcntl:
		if (cmd == F_SETLK || cmd == F_SETLKW)
			__lock_arg_init(struct flock, regs, arg);
		else
			ret = -EINVAL;
		break;
	case id_sys_fcntl64:
		if (cmd == F_SETLK64 || cmd == F_SETLKW64)
			__lock_arg_init(struct flock64, regs, arg);
		else if (cmd == F_SETLK || cmd == F_SETLKW)
			__lock_arg_init(struct flock, regs, arg);
		else
			ret = -EINVAL;
		break;
	case id_sys_flock: /* TODO is it really needed? */
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void lock_entry_handler(struct syshook_probe *p, struct pt_regs *regs)
{
	struct file_probe *fprobe = to_file_probe(p);
	struct flock_private *priv = (struct flock_private *)priv_data_get();
	int fd = fops_sarg(int, regs, 0);
	struct file *file;

	file = __fops_fget(fd);
	if (fops_fcheck(current, file) == 0) {
		int subtype = fprobe->subtype;
		struct lock_arg arg;
		char *buf, *filepath;

		preempt_disable();
		buf = fops_path_buf();
		filepath = fops_fpath(file, buf, PATH_LEN);

		if (lock_arg_init(fprobe->id, regs, &arg) == 0) {
			subtype = arg.type == F_UNLCK ?
					FOPS_LOCK_RELEASE :
					FOPS_LOCK_START;
			ksf_msg_file_entry_lock(regs, fd, filepath, subtype,
						arg.type, arg.whence,
						arg.start, arg.len);
		} else {
			ksf_msg_file_entry(regs, fd, filepath, subtype);
		}
		preempt_enable();

		priv->dentry = file->f_path.dentry;
		priv->subtype = subtype;
	} else {
		priv->dentry = NULL;
	}

	if (file)
		__fops_fput(file);
}

static void lock_ret_handler(struct syshook_probe *p, struct pt_regs *regs)
{
	struct flock_private *priv = (struct flock_private *)priv_data_get();

	if (priv->dentry) {
		struct file_probe *fprobe = to_file_probe(p);

		ksf_msg_file_exit(regs, fprobe->subtype, 'x');
	}
}

static void filp_close_handler(struct file *file)
{
	struct dentry *dentry = file->f_path.dentry;

	/* release the file if it is going to be removed soon */
	if (dentry && fops_dcount(dentry) == 2)
		fops_dremove(dentry);
}

static struct swap_fops_hooks fops_hooks = {
	.owner = THIS_MODULE,
	.filp_close = filp_close_handler,
};


static void fops_unregister_probes(struct file_probe *fprobes, int cnt)
{
	int i = cnt;

	/* probes are unregistered in reverse order */
	while (--i >= 0) {
		struct syshook_probe *sysp = &fprobes[i].sysp;

		swap_syshook_unreg_compat(sysp);
	}

	/* unregister helper probes */
	swap_fops_unreg(&fops_hooks);
}

static int fops_register_probes(struct file_probe *fprobes, int cnt)
{
	int ret, i = 0;

	/* register helper probes */
	ret = swap_fops_reg(&fops_hooks);
	if (ret)
		goto fail;

	/* register syscalls */
	for (i = 0; i < cnt; i++) {
		struct syshook_probe *sysp = &fprobes[i].sysp;

		if (!sysp->sys_entry)
			sysp->sys_entry = generic_entry_handler;

		if (!sysp->sys_exit)
			sysp->sys_exit = generic_ret_handler;

		ret = swap_syshook_reg_compat(sysp, fprobes[i].syscall_id);
		if (ret)
			goto fail_unreg;
	}

	return 0;

fail_unreg:
	fops_unregister_probes(fprobes, i);

fail:
	printk(FOPS_PREFIX "Failed to register syshook\n");

	return ret;
}

static char *__fops_dpath(struct dentry *dentry, char *buf, int buflen)
{
	static const char *NA = "N/A";
	char *filename = dentry_path_raw(dentry, buf, buflen);

	if (IS_ERR_OR_NULL(filename)) {
		printk(FOPS_PREFIX "dentry_path_raw FAILED: %ld\n",
		       PTR_ERR(filename));
		strncpy(buf, NA, buflen);
		filename = buf;
	}

	return filename;
}

/* just a simple wrapper for passing to clear function */
static int __fops_dput_wrapper(void *data, void *arg)
{
	static char buf[PATH_LEN]; /* called under write lock => static is ok */
	struct dentry *dentry = data;
	struct inode *inode = dentry->d_inode;

	printk(FOPS_PREFIX "Releasing dentry(%p/%p/%d): %s\n",
	       dentry, inode, inode ? inode->i_nlink : 0,
	      __fops_dpath(dentry, buf, PATH_LEN));
	__fops_dput(dentry);

	return 0;
}

bool file_ops_is_init(void)
{
	return fops_enabled;
}

int file_ops_init(void)
{
	int ret = -EINVAL;

	mutex_lock(&fops_lock);

	if (fops_enabled) {
		printk(FOPS_PREFIX "Handlers already enabled\n");
		goto unlock;
	}

	fops_ktd.size = max3(sizeof(struct file_private),
			     sizeof(struct open_private),
			     sizeof(struct flock_private));
	ret = swap_ktd_reg(&fops_ktd);
	if (ret)
		goto unlock;

	ret = fops_register_probes(fprobes, ARRAY_SIZE(fprobes));
	if (ret)
		swap_ktd_unreg(&fops_ktd);
	if (ret == 0)
		fops_enabled = 1;

unlock:
	mutex_unlock(&fops_lock);

	return ret;
}

void file_ops_exit(void)
{
	struct map *map;

	mutex_lock(&fops_lock);

	if (!fops_enabled) {
		printk(FOPS_PREFIX "Handlers not enabled\n");
		goto unlock;
	}

	/* 1. unregister probes */
	fops_unregister_probes(fprobes, ARRAY_SIZE(fprobes));

	/* 2. clear the map */
	map = get_map_write();
	printk(FOPS_PREFIX "Clearing map: entries(%d)\n", map->size);
	clear(map, __fops_dput_wrapper, NULL);
	WARN(file_get_put_balance, "File GET/PUT balance: %d\n",
	     file_get_put_balance);
	WARN(dentry_get_put_balance, "Dentry GET/PUT balance: %d\n",
	     dentry_get_put_balance);
	put_map_write(map);

	/* 3. unregister ktask_data */
	swap_ktd_unreg(&fops_ktd);

	/* 4. drop the flag */
	fops_enabled = 0;

unlock:
	mutex_unlock(&fops_lock);
}
