/**
 * kprobe/swap_kprobes.c
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: initial implementation for ARM and MIPS
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation;
 * Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
 *
 * @section LICENSE
 *
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
 * @section COPYRIGHT
 *
 * Copyright (C) IBM Corporation, 2002, 2004
 * Copyright (C) Samsung Electronics, 2006-2010
 *
 * @section DESCRIPTION
 *
 * SWAP kprobe implementation. Dynamic kernel functions instrumentation.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#include <linux/config.h>
#endif

#include <linux/hash.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/pagemap.h>

#include <ksyms/ksyms.h>
#include <master/swap_initializer.h>
#include <swap-asm/swap_kprobes.h>

#include "swap_slots.h"
#include "swap_kdebug.h"
#include "swap_kprobes.h"
#include "swap_kprobes_deps.h"

/**
 * @var sched_addr
 * @brief Scheduler address.
 */
unsigned long sched_addr;
static unsigned long exit_addr;
static unsigned long do_group_exit_addr;
static unsigned long sys_exit_group_addr;
static unsigned long sys_exit_addr;

/**
 * @var sm
 * @brief Current slot manager. Slots are the places where trampolines are
 * located.
 */
struct slot_manager sm;

DEFINE_PER_CPU(struct kprobe *, swap_current_kprobe) = NULL;
static DEFINE_PER_CPU(struct kprobe_ctlblk, kprobe_ctlblk);

static DEFINE_SPINLOCK(kretprobe_lock);	/* Protects kretprobe_inst_table */
static DEFINE_PER_CPU(struct kprobe *, kprobe_instance) = NULL;

struct hlist_head kprobe_table[KPROBE_TABLE_SIZE];
static struct hlist_head kretprobe_inst_table[KPROBE_TABLE_SIZE];

/**
 * @var kprobe_count
 * @brief Count of kprobes.
 */
atomic_t kprobe_count;
EXPORT_SYMBOL_GPL(kprobe_count);


static void *(*module_alloc)(unsigned long size) = NULL;
static void *(*module_free)(struct module *mod, void *module_region) = NULL;

static void *__wrapper_module_alloc(unsigned long size)
{
	return module_alloc(size);
}

static void *__wrapper_module_free(void *module_region)
{
	return module_free(NULL, module_region);
}

static void *sm_alloc(struct slot_manager *sm)
{
	return __wrapper_module_alloc(PAGE_SIZE);
}

static void sm_free(struct slot_manager *sm, void *ptr)
{
	__wrapper_module_free(ptr);
}

static void init_sm(void)
{
	sm.slot_size = KPROBES_TRAMP_LEN;
	sm.alloc = sm_alloc;
	sm.free = sm_free;
	INIT_HLIST_HEAD(&sm.page_list);
}

static void exit_sm(void)
{
	/* FIXME: free */
}

static void kretprobe_assert(struct kretprobe_instance *ri,
			     unsigned long orig_ret_address,
			     unsigned long trampoline_address)
{
	if (!orig_ret_address || (orig_ret_address == trampoline_address)) {
		struct task_struct *task;
		if (ri == NULL) {
			panic("kretprobe BUG!: ri = NULL\n");
		}

		task = ri->task;

		if (task == NULL) {
			panic("kretprobe BUG!: task = NULL\n");
		}

		if (ri->rp == NULL) {
			panic("kretprobe BUG!: ri->rp = NULL\n");
		}

		panic("kretprobe BUG!: Processing kretprobe %p @ %p (%d/%d - %s)\n",
		      ri->rp, ri->rp->kp.addr, ri->task->tgid, ri->task->pid, ri->task->comm);
	}
}

/* We have preemption disabled.. so it is safe to use __ versions */
static inline void set_kprobe_instance(struct kprobe *kp)
{
	__get_cpu_var(kprobe_instance) = kp;
}

static inline void reset_kprobe_instance(void)
{
	__get_cpu_var(kprobe_instance) = NULL;
}

/**
 * @brief Gets the current kprobe on this CPU.
 *
 * @return Pointer to the current kprobe.
 */
struct kprobe *swap_kprobe_running(void)
{
	return __get_cpu_var(swap_current_kprobe);
}

/**
 * @brief Sets the current kprobe to NULL.
 *
 * @return Void.
 */
void swap_reset_current_kprobe(void)
{
	__get_cpu_var(swap_current_kprobe) = NULL;
}

/**
 * @brief Gets kprobe_ctlblk for the current CPU.
 *
 * @return Current CPU struct kprobe_ctlblk.
 */
struct kprobe_ctlblk *swap_get_kprobe_ctlblk(void)
{
	return &__get_cpu_var(kprobe_ctlblk);
}

/*
 * This routine is called either:
 * 	- under the kprobe_mutex - during kprobe_[un]register()
 * 				OR
 * 	- with preemption disabled - from arch/xxx/kernel/kprobes.c
 */

/**
 * @brief Gets kprobe.
 *
 * @param addr Probe address.
 * @return Kprobe for addr.
 */
struct kprobe *swap_get_kprobe(void *addr)
{
	struct hlist_head *head;
	struct kprobe *p;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	head = &kprobe_table[hash_ptr (addr, KPROBE_HASH_BITS)];
	swap_hlist_for_each_entry_rcu(p, node, head, hlist) {
		if (p->addr == addr) {
			return p;
		}
	}

	return NULL;
}

/*
 * Aggregate handlers for multiple kprobes support - these handlers
 * take care of invoking the individual kprobe handlers on p->list
 */
static int aggr_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe *kp;
	int ret;

	list_for_each_entry_rcu(kp, &p->list, list) {
		if (kp->pre_handler) {
			set_kprobe_instance(kp);
			ret = kp->pre_handler(kp, regs);
			if (ret)
				return ret;
		}
		reset_kprobe_instance();
	}

	return 0;
}

static void aggr_post_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	struct kprobe *kp;

	list_for_each_entry_rcu(kp, &p->list, list) {
		if (kp->post_handler) {
			set_kprobe_instance(kp);
			kp->post_handler(kp, regs, flags);
			reset_kprobe_instance();
		}
	}
}

static int aggr_fault_handler(struct kprobe *p, struct pt_regs *regs, int trapnr)
{
	struct kprobe *cur = __get_cpu_var(kprobe_instance);

	/*
	 * if we faulted "during" the execution of a user specified
	 * probe handler, invoke just that probe's fault handler
	 */
	if (cur && cur->fault_handler) {
		if (cur->fault_handler(cur, regs, trapnr))
			return 1;
	}

	return 0;
}

static int aggr_break_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe *cur = __get_cpu_var(kprobe_instance);
	int ret = 0;
	DBPRINTF ("cur = 0x%p\n", cur);
	if (cur)
		DBPRINTF ("cur = 0x%p cur->break_handler = 0x%p\n", cur, cur->break_handler);

	if (cur && cur->break_handler) {
		if (cur->break_handler(cur, regs))
			ret = 1;
	}
	reset_kprobe_instance();

	return ret;
}

/**
 * @brief Walks the list and increments nmissed count for multiprobe case.
 *
 * @param p Pointer to the missed kprobe.
 * @return Void.
 */
void swap_kprobes_inc_nmissed_count(struct kprobe *p)
{
	struct kprobe *kp;
	if (p->pre_handler != aggr_pre_handler) {
		p->nmissed++;
	} else {
		list_for_each_entry_rcu(kp, &p->list, list) {
			++kp->nmissed;
		}
	}
}

static int alloc_nodes_kretprobe(struct kretprobe *rp);

/* Called with kretprobe_lock held */
static struct kretprobe_instance *get_free_rp_inst(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->free_instances, uflist) {
		return ri;
	}

	if (!alloc_nodes_kretprobe(rp)) {
		swap_hlist_for_each_entry(ri, node, &rp->free_instances,
					  uflist) {
			return ri;
		}
	}

	return NULL;
}

/* Called with kretprobe_lock held */
static struct kretprobe_instance *
get_free_rp_inst_no_alloc(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->free_instances, uflist) {
		return ri;
	}

	return NULL;
}

/* Called with kretprobe_lock held */
static struct kretprobe_instance *get_used_rp_inst(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->used_instances, uflist) {
		return ri;
	}

	return NULL;
}

/* Called with kretprobe_lock held */
static void add_rp_inst(struct kretprobe_instance *ri)
{
	/*
	 * Remove rp inst off the free list -
	 * Add it back when probed function returns
	 */
	hlist_del(&ri->uflist);

	/* Add rp inst onto table */
	INIT_HLIST_NODE(&ri->hlist);

	hlist_add_head(&ri->hlist, &kretprobe_inst_table[hash_ptr(ri->task, KPROBE_HASH_BITS)]);

	/* Also add this rp inst to the used list. */
	INIT_HLIST_NODE(&ri->uflist);
	hlist_add_head(&ri->uflist, &ri->rp->used_instances);
}

/* Called with kretprobe_lock held */
static void recycle_rp_inst(struct kretprobe_instance *ri)
{
	if (ri->rp) {
		hlist_del(&ri->hlist);
		/* remove rp inst off the used list */
		hlist_del(&ri->uflist);
		/* put rp inst back onto the free list */
		INIT_HLIST_NODE(&ri->uflist);
		hlist_add_head(&ri->uflist, &ri->rp->free_instances);
	}
}

static struct hlist_head *kretprobe_inst_table_head(void *hash_key)
{
	return &kretprobe_inst_table[hash_ptr(hash_key, KPROBE_HASH_BITS)];
}

static void free_rp_inst(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	while ((ri = get_free_rp_inst_no_alloc(rp)) != NULL) {
		hlist_del(&ri->uflist);
		kfree(ri);
	}
}

/*
 * Keep all fields in the kprobe consistent
 */
static inline void copy_kprobe(struct kprobe *old_p, struct kprobe *p)
{
	memcpy(&p->opcode, &old_p->opcode, sizeof(kprobe_opcode_t));
	memcpy(&p->ainsn, &old_p->ainsn, sizeof(struct arch_specific_insn));
#ifdef CONFIG_ARM
	p->safe_arm = old_p->safe_arm;
	p->safe_thumb = old_p->safe_thumb;
#endif
}

/*
 * Add the new probe to old_p->list. Fail if this is the
 * second jprobe at the address - two jprobes can't coexist
 */
static int add_new_kprobe(struct kprobe *old_p, struct kprobe *p)
{
	if (p->break_handler) {
		if (old_p->break_handler) {
			return -EEXIST;
		}

		list_add_tail_rcu(&p->list, &old_p->list);
		old_p->break_handler = aggr_break_handler;
	} else {
		list_add_rcu(&p->list, &old_p->list);
	}

	if (p->post_handler && !old_p->post_handler) {
		old_p->post_handler = aggr_post_handler;
	}

	return 0;
}

/**
 * hlist_replace_rcu - replace old entry by new one
 * @old : the element to be replaced
 * @new : the new element to insert
 *
 * The @old entry will be replaced with the @new entry atomically.
 */
inline void swap_hlist_replace_rcu(struct hlist_node *old,
				   struct hlist_node *new)
{
	struct hlist_node *next = old->next;

	new->next = next;
	new->pprev = old->pprev;
	smp_wmb();
	if (next)
		new->next->pprev = &new->next;
	if (new->pprev)
		*new->pprev = new;
	old->pprev = LIST_POISON2;
}

/*
 * Fill in the required fields of the "manager kprobe". Replace the
 * earlier kprobe in the hlist with the manager kprobe
 */
static inline void add_aggr_kprobe(struct kprobe *ap, struct kprobe *p)
{
	copy_kprobe(p, ap);
	ap->addr = p->addr;
	ap->pre_handler = aggr_pre_handler;
	ap->fault_handler = aggr_fault_handler;
	if (p->post_handler)
		ap->post_handler = aggr_post_handler;
	if (p->break_handler)
		ap->break_handler = aggr_break_handler;

	INIT_LIST_HEAD(&ap->list);
	list_add_rcu(&p->list, &ap->list);

	swap_hlist_replace_rcu(&p->hlist, &ap->hlist);
}

/*
 * This is the second or subsequent kprobe at the address - handle
 * the intricacies
 */
static int register_aggr_kprobe(struct kprobe *old_p, struct kprobe *p)
{
	int ret = 0;
	struct kprobe *ap;
	DBPRINTF ("start\n");

	DBPRINTF ("p = %p old_p = %p \n", p, old_p);
	if (old_p->pre_handler == aggr_pre_handler) {
		DBPRINTF ("aggr_pre_handler \n");

		copy_kprobe(old_p, p);
		ret = add_new_kprobe(old_p, p);
	} else {
		DBPRINTF ("kzalloc\n");
#ifdef kzalloc
		ap = kzalloc(sizeof(struct kprobe), GFP_KERNEL);
#else
		ap = kmalloc(sizeof(struct kprobe), GFP_KERNEL);
		if (ap)
			memset(ap, 0, sizeof(struct kprobe));
#endif
		if (!ap)
			return -ENOMEM;
		add_aggr_kprobe(ap, old_p);
		copy_kprobe(ap, p);
		DBPRINTF ("ap = %p p = %p old_p = %p \n", ap, p, old_p);
		ret = add_new_kprobe(ap, p);
	}

	return ret;
}

static void remove_kprobe(struct kprobe *p)
{
	/* TODO: check boostable for x86 and MIPS */
	swap_slot_free(&sm, p->ainsn.insn);
}

/**
 * @brief Registers kprobe.
 *
 * @param p Pointer to the target kprobe.
 * @return 0 on success, error code on error.
 */
int swap_register_kprobe(struct kprobe *p)
{
	struct kprobe *old_p;
	int ret = 0;
	/*
	 * If we have a symbol_name argument look it up,
	 * and add it to the address.  That way the addr
	 * field can either be global or relative to a symbol.
	 */
	if (p->symbol_name) {
		if (p->addr)
			return -EINVAL;
		p->addr = (kprobe_opcode_t *)swap_ksyms(p->symbol_name);
	}

	if (!p->addr)
		return -EINVAL;
	DBPRINTF ("p->addr = 0x%p\n", p->addr);
	p->addr = (kprobe_opcode_t *)(((char *)p->addr) + p->offset);
	DBPRINTF ("p->addr = 0x%p p = 0x%p\n", p->addr, p);

#ifdef KPROBES_PROFILE
	p->start_tm.tv_sec = p->start_tm.tv_usec = 0;
	p->hnd_tm_sum.tv_sec = p->hnd_tm_sum.tv_usec = 0;
	p->count = 0;
#endif
	p->mod_refcounted = 0;
	p->nmissed = 0;
	INIT_LIST_HEAD(&p->list);

	old_p = swap_get_kprobe(p->addr);
	if (old_p) {
		ret = register_aggr_kprobe(old_p, p);
		if (!ret)
			atomic_inc(&kprobe_count);
		goto out;
	}

	if ((ret = swap_arch_prepare_kprobe(p, &sm)) != 0)
		goto out;

	DBPRINTF ("before out ret = 0x%x\n", ret);
	INIT_HLIST_NODE(&p->hlist);
	hlist_add_head_rcu(&p->hlist, &kprobe_table[hash_ptr(p->addr, KPROBE_HASH_BITS)]);
	swap_arch_arm_kprobe(p);

out:
	DBPRINTF ("out ret = 0x%x\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(swap_register_kprobe);

static void swap_unregister_valid_kprobe(struct kprobe *p, struct kprobe *old_p)
{
	struct kprobe *list_p;

	if ((old_p == p) || ((old_p->pre_handler == aggr_pre_handler) &&
	    (p->list.next == &old_p->list) && (p->list.prev == &old_p->list))) {
		/* Only probe on the hash list */
		swap_arch_disarm_kprobe(p);
		hlist_del_rcu(&old_p->hlist);
		remove_kprobe(old_p);

		if (p != old_p) {
			list_del_rcu(&old_p->list);
			kfree(old_p);
		}
		/* Synchronize and remove probe in bottom */
	} else {
		list_del_rcu(&p->list);

		if (p->break_handler)
			old_p->break_handler = NULL;
		if (p->post_handler) {
			list_for_each_entry_rcu(list_p, &old_p->list, list)
				if (list_p->post_handler)
					return;

			old_p->post_handler = NULL;
		}
	}
	/* Set NULL addr for reusability if symbol_name is used */
	if (p->symbol_name)
		p->addr = NULL;
}

/**
 * @brief Unregistes kprobe.
 *
 * @param kp Pointer to the target kprobe.
 * @return Void.
 */
void swap_unregister_kprobe(struct kprobe *kp)
{
	struct kprobe *old_p, *list_p;

	old_p = swap_get_kprobe(kp->addr);
	if (unlikely (!old_p))
		return;

	if (kp != old_p) {
		list_for_each_entry_rcu(list_p, &old_p->list, list)
			if (list_p == kp)
				goto unreg_valid_kprobe;
		/* kprobe invalid */
		return;
	}

unreg_valid_kprobe:
	swap_unregister_valid_kprobe(kp, old_p);
}
EXPORT_SYMBOL_GPL(swap_unregister_kprobe);

/**
 * @brief Registers jprobe.
 *
 * @param jp Pointer to the target jprobe.
 * @return swap_register_kprobe result.
 */
int swap_register_jprobe(struct jprobe *jp)
{
	/* Todo: Verify probepoint is a function entry point */
	jp->kp.pre_handler = swap_setjmp_pre_handler;
	jp->kp.break_handler = swap_longjmp_break_handler;

	return swap_register_kprobe(&jp->kp);
}
EXPORT_SYMBOL_GPL(swap_register_jprobe);

/**
 * @brief Unregisters jprobe.
 *
 * @param jp Pointer to the target jprobe.
 * @return Void.
 */
void swap_unregister_jprobe(struct jprobe *jp)
{
	swap_unregister_kprobe(&jp->kp);
}
EXPORT_SYMBOL_GPL(swap_unregister_jprobe);

/*
 * This kprobe pre_handler is registered with every kretprobe. When probe
 * hits it will set up the return probe.
 */
static int pre_handler_kretprobe(struct kprobe *p, struct pt_regs *regs)
{
	struct kretprobe *rp = container_of(p, struct kretprobe, kp);
	struct kretprobe_instance *ri;
	unsigned long flags = 0;

	/* TODO: consider to only swap the RA after the last pre_handler fired */
	spin_lock_irqsave(&kretprobe_lock, flags);

	/* TODO: test - remove retprobe after func entry but before its exit */
	if ((ri = get_free_rp_inst(rp)) != NULL) {
		ri->rp = rp;
		ri->task = current;

		if (rp->entry_handler) {
			rp->entry_handler(ri, regs);
		}

		swap_arch_prepare_kretprobe(ri, regs);

		add_rp_inst(ri);
	} else {
		++rp->nmissed;
	}

	spin_unlock_irqrestore(&kretprobe_lock, flags);

	return 0;
}

/**
 * @brief Trampoline probe handler.
 *
 * @param p Pointer to the fired kprobe.
 * @param regs Pointer to CPU registers data.
 * @return orig_ret_address
 */
int trampoline_probe_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kretprobe_instance *ri = NULL;
	struct hlist_head *head;
	unsigned long flags, orig_ret_address = 0;
	unsigned long trampoline_address;

	struct kprobe_ctlblk *kcb;

	struct hlist_node *tmp;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	trampoline_address = (unsigned long)&swap_kretprobe_trampoline;

	preempt_disable();
	kcb = swap_get_kprobe_ctlblk();

	spin_lock_irqsave(&kretprobe_lock, flags);

	/*
	 * We are using different hash keys (current and mm) for finding kernel
	 * space and user space probes.  Kernel space probes can change mm field in
	 * task_struct.  User space probes can be shared between threads of one
	 * process so they have different current but same mm.
	 */
	head = kretprobe_inst_table_head(current);

#ifdef CONFIG_X86
	regs->XREG(cs) = __KERNEL_CS | get_kernel_rpl();
	regs->EREG(ip) = trampoline_address;
	regs->ORIG_EAX_REG = 0xffffffff;
#endif

	/*
	 * It is possible to have multiple instances associated with a given
	 * task either because an multiple functions in the call path
	 * have a return probe installed on them, and/or more then one
	 * return probe was registered for a target function.
	 *
	 * We can handle this because:
	 *     - instances are always inserted at the head of the list
	 *     - when multiple return probes are registered for the same
	 *       function, the first instance's ret_addr will point to the
	 *       real return address, and all the rest will point to
	 *       kretprobe_trampoline
	 */
	swap_hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task != current)
			/* another task is sharing our hash bucket */
			continue;
		if (ri->rp && ri->rp->handler) {
			__get_cpu_var(swap_current_kprobe) = &ri->rp->kp;
			swap_get_kprobe_ctlblk()->kprobe_status = KPROBE_HIT_ACTIVE;
			ri->rp->handler(ri, regs);
			__get_cpu_var(swap_current_kprobe) = NULL;
		}

		orig_ret_address = (unsigned long)ri->ret_addr;
		recycle_rp_inst(ri);
		if (orig_ret_address != trampoline_address)
			/*
			 * This is the real return address. Any other
			 * instances associated with this task are for
			 * other calls deeper on the call stack
			 */
			break;
	}
	kretprobe_assert(ri, orig_ret_address, trampoline_address);

	if (kcb->kprobe_status == KPROBE_REENTER) {
		restore_previous_kprobe(kcb);
	} else {
		swap_reset_current_kprobe();
	}

	spin_unlock_irqrestore(&kretprobe_lock, flags);
	preempt_enable_no_resched();

	/*
	 * By returning a non-zero value, we are telling
	 * kprobe_handler() that we don't want the post_handler
	 * to run (and have re-enabled preemption)
	 */

	return (int)orig_ret_address;
}

#define SCHED_RP_NR 200
#define COMMON_RP_NR 10

static int alloc_nodes_kretprobe(struct kretprobe *rp)
{
	int alloc_nodes;
	struct kretprobe_instance *inst;
	int i;

	DBPRINTF("Alloc aditional mem for retprobes");

	if ((unsigned long)rp->kp.addr == sched_addr) {
		rp->maxactive += SCHED_RP_NR;//max (100, 2 * NR_CPUS);
		alloc_nodes = SCHED_RP_NR;
	} else {
#if 1//def CONFIG_PREEMPT
		rp->maxactive += max (COMMON_RP_NR, 2 * NR_CPUS);
#else
		rp->maxacpptive += NR_CPUS;
#endif
		alloc_nodes = COMMON_RP_NR;
	}

	for (i = 0; i < alloc_nodes; i++) {
		inst = kmalloc(sizeof(*inst) + rp->data_size, GFP_ATOMIC);
		if (inst == NULL) {
			free_rp_inst(rp);
			return -ENOMEM;
		}
		INIT_HLIST_NODE(&inst->uflist);
		hlist_add_head(&inst->uflist, &rp->free_instances);
	}

	DBPRINTF ("addr=%p, *addr=[%lx %lx %lx]", rp->kp.addr, (unsigned long) (*(rp->kp.addr)), (unsigned long) (*(rp->kp.addr + 1)), (unsigned long) (*(rp->kp.addr + 2)));
	return 0;
}

/**
 * @brief Registers kretprobes.
 *
 * @param rp Pointer to the target kretprobe.
 * @return 0 on success, error code on error.
 */
int swap_register_kretprobe(struct kretprobe *rp)
{
	int ret = 0;
	struct kretprobe_instance *inst;
	int i;
	DBPRINTF ("START");

	rp->kp.pre_handler = pre_handler_kretprobe;
	rp->kp.post_handler = NULL;
	rp->kp.fault_handler = NULL;
	rp->kp.break_handler = NULL;

	/* Pre-allocate memory for max kretprobe instances */
	if ((unsigned long)rp->kp.addr == exit_addr) {
		rp->kp.pre_handler = NULL; //not needed for do_exit
		rp->maxactive = 0;
	} else if ((unsigned long)rp->kp.addr == do_group_exit_addr) {
		rp->kp.pre_handler = NULL;
		rp->maxactive = 0;
	} else if ((unsigned long)rp->kp.addr == sys_exit_group_addr) {
		rp->kp.pre_handler = NULL;
		rp->maxactive = 0;
	} else if ((unsigned long)rp->kp.addr == sys_exit_addr) {
		rp->kp.pre_handler = NULL;
		rp->maxactive = 0;
	} else if (rp->maxactive <= 0) {
#if 1//def CONFIG_PREEMPT
		rp->maxactive = max (COMMON_RP_NR, 2 * NR_CPUS);
#else
		rp->maxactive = NR_CPUS;
#endif
	}
	INIT_HLIST_HEAD(&rp->used_instances);
	INIT_HLIST_HEAD(&rp->free_instances);
	for (i = 0; i < rp->maxactive; i++) {
		inst = kmalloc(sizeof(*inst) + rp->data_size, GFP_KERNEL);
		if (inst == NULL) {
			free_rp_inst(rp);
			return -ENOMEM;
		}
		INIT_HLIST_NODE(&inst->uflist);
		hlist_add_head(&inst->uflist, &rp->free_instances);
	}

	DBPRINTF ("addr=%p, *addr=[%lx %lx %lx]", rp->kp.addr, (unsigned long) (*(rp->kp.addr)), (unsigned long) (*(rp->kp.addr + 1)), (unsigned long) (*(rp->kp.addr + 2)));
	rp->nmissed = 0;
	/* Establish function entry probe point */
	if ((ret = swap_register_kprobe(&rp->kp)) != 0)
		free_rp_inst(rp);

	DBPRINTF ("addr=%p, *addr=[%lx %lx %lx]", rp->kp.addr, (unsigned long) (*(rp->kp.addr)), (unsigned long) (*(rp->kp.addr + 1)), (unsigned long) (*(rp->kp.addr + 2)));

	return ret;
}
EXPORT_SYMBOL_GPL(swap_register_kretprobe);

static int swap_disarm_krp_inst(struct kretprobe_instance *ri);

static void swap_disarm_krp(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->used_instances, uflist) {
		if (swap_disarm_krp_inst(ri) != 0) {
			printk("%s (%d/%d): cannot disarm krp instance (%08lx)\n",
					ri->task->comm, ri->task->tgid, ri->task->pid,
					(unsigned long)rp->kp.addr);
		}
	}
}

/**
 * @brief Kretprobes unregister top. Unregisters kprobes.
 *
 * @param rps Pointer to the array of pointers to the target kretprobes.
 * @param size Size of rps array.
 * @param rp_disarm Disarm flag. If set kretprobe is disarmed.
 * @return Void.
 */
void swap_unregister_kretprobes_top(struct kretprobe **rps, size_t size,
				   int rp_disarm)
{
	unsigned long flags;
	const size_t end = ((size_t) 0) - 1;

	spin_lock_irqsave(&kretprobe_lock, flags);
	for (--size; size != end; --size) {
		swap_unregister_kprobe(&rps[size]->kp);
		if (rp_disarm)
			swap_disarm_krp(rps[size]);
	}
	spin_unlock_irqrestore(&kretprobe_lock, flags);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobes_top);

/**
 * @brief swap_unregister_kretprobes_top wrapper for a single kretprobe.
 *
 * @param rp Pointer to the target kretprobe.
 * @param rp_disarm Disarm flag.
 * @return Void.
 */
void swap_unregister_kretprobe_top(struct kretprobe *rp, int rp_disarm)
{
	swap_unregister_kretprobes_top(&rp, 1, rp_disarm);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobe_top);

/**
 * @brief Kretprobe unregister bottom. Here is kretprobe memory is released.
 *
 * @param rp Pointer to the target kretprobe.
 * @return Void.
 */
void swap_unregister_kretprobe_bottom(struct kretprobe *rp)
{
	unsigned long flags;
	struct kretprobe_instance *ri;

	spin_lock_irqsave(&kretprobe_lock, flags);

	while ((ri = get_used_rp_inst(rp)) != NULL) {
		recycle_rp_inst(ri);
	}
	free_rp_inst(rp);

	spin_unlock_irqrestore(&kretprobe_lock, flags);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobe_bottom);

/**
 * @brief swap_unregister_kretprobe_bottom wrapper for several kretprobes.
 *
 * @param rps Pointer to the array of the target kretprobes pointers.
 * @param size Size of rps array.
 * @return Void.
 */
void swap_unregister_kretprobes_bottom(struct kretprobe **rps, size_t size)
{
	const size_t end = ((size_t) 0) - 1;

	for (--size; size != end; --size)
		swap_unregister_kretprobe_bottom(rps[size]);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobes_bottom);

/**
 * @brief Unregisters kretprobes.
 *
 * @param rpp Pointer to the array of the target kretprobes pointers.
 * @param size Size of rpp array.
 * @return Void.
 */
void swap_unregister_kretprobes(struct kretprobe **rpp, size_t size)
{
	swap_unregister_kretprobes_top(rpp, size, 1);

	if (!in_atomic())
		synchronize_sched();

	swap_unregister_kretprobes_bottom(rpp, size);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobes);

/**
 * @brief swap_unregister_kretprobes wrapper for a single kretprobe.
 *
 * @param rp Pointer to the target kretprobe.
 * @return Void.
 */
void swap_unregister_kretprobe(struct kretprobe *rp)
{
	swap_unregister_kretprobes(&rp, 1);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobe);

static void inline rm_task_trampoline(struct task_struct *p, struct kretprobe_instance *ri)
{
	arch_set_task_pc(p, (unsigned long)ri->ret_addr);
}

static int swap_disarm_krp_inst(struct kretprobe_instance *ri)
{
	unsigned long *tramp = (unsigned long *)&swap_kretprobe_trampoline;
	unsigned long *sp = ri->sp;
	unsigned long *found = NULL;
	int retval = -ENOENT;

	if (!sp) {
		unsigned long pc = arch_get_task_pc(ri->task);

		printk("---> [%d] %s (%d/%d): pc = %08lx, ra = %08lx, tramp= %08lx (%08lx)\n",
		       task_cpu(ri->task),
		       ri->task->comm, ri->task->tgid, ri->task->pid,
		       pc, (long unsigned int)ri->ret_addr,
		       (long unsigned int)tramp,
		       (long unsigned int)(ri->rp ? ri->rp->kp.addr: NULL));

		/* __switch_to retprobe handling */
		if (pc == (unsigned long)tramp) {
			rm_task_trampoline(ri->task, ri);
			return 0;
		}

		return -EINVAL;
	}

	while (sp > ri->sp - RETPROBE_STACK_DEPTH) {
		if (*sp == (unsigned long)tramp) {
			found = sp;
			break;
		}
		sp--;
	}

	if (found) {
		printk("---> [%d] %s (%d/%d): tramp (%08lx) found at %08lx (%08lx /%+d) - %p\n",
		       task_cpu(ri->task),
		       ri->task->comm, ri->task->tgid, ri->task->pid,
		       (long unsigned int)tramp,
		       (long unsigned int)found, (long unsigned int)ri->sp,
		       found - ri->sp, ri->rp ? ri->rp->kp.addr: NULL);
		*found = (unsigned long)ri->ret_addr;
		retval = 0;
	} else {
		printk("---> [%d] %s (%d/%d): tramp (%08lx) NOT found at sp = %08lx - %p\n",
				task_cpu(ri->task),
				ri->task->comm, ri->task->tgid, ri->task->pid,
				(long unsigned int)tramp,
				(long unsigned int)ri->sp, ri->rp ? ri->rp->kp.addr: NULL);
	}

	return retval;
}

static int init_module_deps(void)
{
	int ret;

	sched_addr = swap_ksyms("__switch_to");
	exit_addr = swap_ksyms("do_exit");
	sys_exit_group_addr = swap_ksyms("sys_exit_group");
        do_group_exit_addr = swap_ksyms("do_group_exit");
        sys_exit_addr = swap_ksyms("sys_exit");

	if (sched_addr == 0 ||
	    exit_addr == 0 ||
	    sys_exit_group_addr == 0 ||
	    do_group_exit_addr == 0 ||
	    sys_exit_addr == 0) {
		return -ESRCH;
	}

	ret = init_module_dependencies();
	if (ret) {
		return ret;
	}

	return arch_init_module_deps();
}

static int once(void)
{
	int i, ret;
	const char *sym;

	sym = "module_alloc";
	module_alloc = (void *)swap_ksyms(sym);
	if (module_alloc == NULL)
		goto not_found;

	sym = "module_free";
	module_free = (void *)swap_ksyms(sym);
	if (module_alloc == NULL)
		goto not_found;

	ret = init_module_deps();
	if (ret)
		return ret;

	/*
	 * FIXME allocate the probe table, currently defined statically
	 * initialize all list heads
	 */
	for (i = 0; i < KPROBE_TABLE_SIZE; ++i) {
		INIT_HLIST_HEAD(&kprobe_table[i]);
		INIT_HLIST_HEAD(&kretprobe_inst_table[i]);
	}

	return 0;

not_found:
	printk("ERROR: symbol '%s' not found\n", sym);
	return -ESRCH;
}

static int init_kprobes(void)
{
	init_sm();
	atomic_set(&kprobe_count, 0);

	return swap_arch_init_kprobes();
}

static void exit_kprobes(void)
{
	swap_arch_exit_kprobes();
	exit_sm();
}

SWAP_LIGHT_INIT_MODULE(once, init_kprobes, exit_kprobes, NULL, NULL);

MODULE_LICENSE("GPL");
