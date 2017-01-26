#ifndef _LINUX_SWAP_US_HOOKS_H
#define _LINUX_SWAP_US_HOOKS_H


#ifdef CONFIG_SWAP_US_HOOKS

#include <linux/compiler.h>

struct file;
struct module;
struct task_struct;

struct swap_us_hooks {
	struct module *owner;
	void (*page_fault)(unsigned long addr);
	void (*copy_process_pre)(void);
	void (*copy_process_post)(struct task_struct *task);
	void (*mm_release)(struct task_struct *task);
	void (*munmap)(unsigned long start, unsigned long end);
	void (*mmap)(struct file *file, unsigned long addr);
	void (*set_comm)(struct task_struct *task);
	void (*change_leader)(struct task_struct *p, struct task_struct *n);
};


extern struct swap_us_hooks *swap_us_hooks_user;

int swap_us_hooks_set(struct swap_us_hooks *hooks);
void swap_us_hooks_reset(void);


/* private interface */
struct swap_us_hooks *swap_us_hooks_get(void);
void swap_us_hooks_put(struct swap_us_hooks *hooks);


#define SWAP_US_HOOKS_CALL(hook_name, ...) \
	if (unlikely(swap_us_hooks_user)) { \
		struct swap_us_hooks *hooks = swap_us_hooks_get(); \
		if (hooks) { \
			hooks->hook_name(__VA_ARGS__); \
			swap_us_hooks_put(hooks); \
		} \
	}

#else /* CONFIG_SWAP_US_HOOKS */

#define SWAP_US_HOOKS_CALL(hook_name, ...)

#endif /* CONFIG_SWAP_US_HOOKS */


static inline void suh_page_fault(unsigned long addr)
{
	SWAP_US_HOOKS_CALL(page_fault, addr);
}

static inline void suh_copy_process_pre(void)
{
	SWAP_US_HOOKS_CALL(copy_process_pre);
}

static inline void suh_copy_process_post(struct task_struct *task)
{
	SWAP_US_HOOKS_CALL(copy_process_post, task);
}

static inline void suh_mm_release(struct task_struct *task)
{
	SWAP_US_HOOKS_CALL(mm_release, task);
}

static inline void suh_munmap(unsigned long start, unsigned long end)
{
	SWAP_US_HOOKS_CALL(munmap, start, end);
}

static inline void suh_mmap(struct file *file, unsigned long addr)
{
	SWAP_US_HOOKS_CALL(mmap, file, addr);
}

static inline void suh_set_comm(struct task_struct *task)
{
	SWAP_US_HOOKS_CALL(set_comm, task);
}

static inline void suh_change_leader(struct task_struct *prev,
				     struct task_struct *next)
{
	SWAP_US_HOOKS_CALL(change_leader, prev, next);
}

#endif /* _LINUX_SWAP_US_HOOKS_H */
