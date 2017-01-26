#include <linux/rwsem.h>
#include <linux/module.h>
#include <swap/swap_syshook.h>
#include <asm/syscall.h>


static DECLARE_RWSEM(syscall_hook_sem);
static struct hlist_head syscall_hook[__NR_syscalls] = {
	[0 ... __NR_syscalls - 1] = HLIST_HEAD_INIT,
};


static void syshook_set(void)
{
	struct task_struct *p, *t;

	read_lock(&tasklist_lock);
	for_each_process_thread(p, t) {
		set_tsk_thread_flag(t, TIF_SWAP_SYSHOOK);
	}
	read_unlock(&tasklist_lock);
}

static void syshook_unset(void)
{
	struct task_struct *p, *t;

	read_lock(&tasklist_lock);
	for_each_process_thread(p, t) {
		clear_tsk_thread_flag(t, TIF_SWAP_SYSHOOK);
	}
	read_unlock(&tasklist_lock);
}

static int syshook_cnt;

static int do_syshook_reg(struct syshook_probe *p, unsigned long nr_call,
			  unsigned long nr_max, struct hlist_head *hook_table)
{
	if (nr_call >= nr_max) {
		pr_err("ERROR: nr_call=%lu is very big\n", nr_call);
		return -EINVAL;
	}

	INIT_HLIST_NODE(&p->node);

	down_write(&syscall_hook_sem);
	hlist_add_head(&p->node, &hook_table[nr_call]);

	if (syshook_cnt == 0)
		syshook_set();

	++syshook_cnt;
	up_write(&syscall_hook_sem);

	return 0;
}

int swap_syshook_reg(struct syshook_probe *p, unsigned long nr_call)
{
	return do_syshook_reg(p, nr_call, __NR_syscalls, syscall_hook);
}
EXPORT_SYMBOL_GPL(swap_syshook_reg);

void swap_syshook_unreg(struct syshook_probe *p)
{
	down_write(&syscall_hook_sem);
	--syshook_cnt;
	if (syshook_cnt == 0)
		syshook_unset();

	hlist_del(&p->node);
	up_write(&syscall_hook_sem);
}
EXPORT_SYMBOL_GPL(swap_syshook_unreg);

#ifdef CONFIG_COMPAT
static struct hlist_head compat_syscall_hook[__NR_compat_syscalls] = {
	[0 ... __NR_compat_syscalls - 1] = HLIST_HEAD_INIT,
};

int swap_syshook_reg_compat(struct syshook_probe *p, unsigned long nr_call)
{
	return do_syshook_reg(p, nr_call, __NR_compat_syscalls, compat_syscall_hook);
}
EXPORT_SYMBOL_GPL(swap_syshook_reg_compat);

static unsigned long nr_syscalls(struct pt_regs *regs)
{
	return compat_user_mode(regs) ? __NR_compat_syscalls : __NR_syscalls;
}

static struct hlist_head *sys_head(struct pt_regs *regs, unsigned long nr_call)
{
	return compat_user_mode(regs) ?
		&compat_syscall_hook[nr_call] :
		&syscall_hook[nr_call];
}
#else /* CONFIG_COMPAT */
# define nr_syscalls(regs)		__NR_syscalls
# define sys_head(regs, nr_call)	(&syscall_hook[nr_call])
#endif /* CONFIG_COMPAT */

void swap_syshook_entry(struct pt_regs *regs)
{
	struct syshook_probe *p;
	unsigned long nr_call = syscall_get_nr(current, regs);
	struct hlist_head *head;

	if (nr_call >= nr_syscalls(regs))
		return;

	head = sys_head(regs, nr_call);

	down_read(&syscall_hook_sem);
	hlist_for_each_entry(p, head, node) {
		if (p->sys_entry)
			p->sys_entry(p, regs);
	}
	up_read(&syscall_hook_sem);
}

void swap_syshook_exit(struct pt_regs *regs)
{
	struct syshook_probe *p;
	unsigned long nr_call = syscall_get_nr(current, regs);
	struct hlist_head *head;

	if (nr_call >= nr_syscalls(regs))
		return;

	head = sys_head(regs, nr_call);

	down_read(&syscall_hook_sem);
	hlist_for_each_entry(p, head, node) {
		if (p->sys_exit)
			p->sys_exit(p, regs);
	}
	up_read(&syscall_hook_sem);
}
