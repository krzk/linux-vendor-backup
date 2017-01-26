#ifndef _SWAP_FOPS_H
#define _SWAP_FOPS_H


struct file;


#ifdef CONFIG_SWAP_FOPS_HOOKS

#include <linux/list.h>

struct module;

struct swap_fops_hooks {
	struct hlist_node node;
	struct module *owner;
	void (*filp_close)(struct file *filp);
};

int swap_fops_reg(struct swap_fops_hooks *hooks);
void swap_fops_unreg(struct swap_fops_hooks *hooks);

void swap_fops_set(void);
void swap_fops_unset(void);


/* private interface */
extern int swap_fops_counter;
void call_fops_filp_close(struct file *filp);

static inline void swap_fops_filp_close(struct file *filp)
{
	if (unlikely(swap_fops_counter))
		call_fops_filp_close(filp);
}

#else /* CONFIG_SWAP_FOPS_HOOKS */

static inline void swap_fops_filp_close(struct file *filp) {}

#endif /* CONFIG_SWAP_FOPS_HOOKS */


#endif /* _SWAP_FOPS_H */
