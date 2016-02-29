/**
 * uprobe/swap_uprobes.c
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial
 * implementation; Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for
 * separating core and arch parts
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
 * Copyright (C) Samsung Electronics, 2006-2010
 *
 * @section DESCRIPTION
 *
 * Uprobes implementation.
 */


#include <linux/hash.h>
#include <linux/mempolicy.h>
#include <linux/module.h>

#include <master/swap_initializer.h>
#include <kprobe/swap_slots.h>
#include <kprobe/swap_kdebug.h>
#include <kprobe/swap_kprobes_deps.h>

#include <swap-asm/swap_uprobes.h>

#include "swap_uprobes.h"


enum {
	UPROBE_HASH_BITS  = 10,
	UPROBE_TABLE_SIZE = (1 << UPROBE_HASH_BITS)
};

struct hlist_head uprobe_insn_slot_table[UPROBE_TABLE_SIZE];
struct hlist_head uprobe_table[UPROBE_TABLE_SIZE];

DEFINE_SPINLOCK(uretprobe_lock);	/* Protects uretprobe_inst_table */
static struct hlist_head uretprobe_inst_table[UPROBE_TABLE_SIZE];

#define DEBUG_PRINT_HASH_TABLE 0

#if DEBUG_PRINT_HASH_TABLE
void print_uprobe_hash_table(void)
{
	int i;
	struct hlist_head *head;
	struct kprobe *p;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	// print uprobe table
	for (i = 0; i < UPROBE_TABLE_SIZE; ++i) {
		head = &uprobe_insn_slot_table[i];
		swap_hlist_for_each_entry_rcu(p, node, head, is_hlist_arm) {
			printk("####### find U tgid=%u, addr=%x\n",
					p->tgid, p->addr);
		}
	}
}
#endif

/*
 * Keep all fields in the uprobe consistent
 */
static inline void copy_uprobe(struct kprobe *old_p, struct kprobe *p)
{
	memcpy(&p->opcode, &old_p->opcode, sizeof(kprobe_opcode_t));
	memcpy(&p->ainsn, &old_p->ainsn, sizeof(struct arch_specific_insn));
#ifdef CONFIG_ARM
	p->safe_arm = old_p->safe_arm;
	p->safe_thumb = old_p->safe_thumb;
#endif
}

/*
 * Aggregate handlers for multiple uprobes support - these handlers
 * take care of invoking the individual uprobe handlers on p->list
 */
static int aggr_pre_uhandler(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe *kp;
	int ret;

	list_for_each_entry_rcu(kp, &p->list, list) {
		if (kp->pre_handler) {
			ret = kp->pre_handler(kp, regs);
			if (ret) {
				return ret;
			}
		}
	}

	return 0;
}

static void aggr_post_uhandler(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	struct kprobe *kp;

	list_for_each_entry_rcu(kp, &p->list, list) {
		if (kp->post_handler) {
			kp->post_handler(kp, regs, flags);
		}
	}
}

static int aggr_fault_uhandler(struct kprobe *p, struct pt_regs *regs, int trapnr)
{
	return 0;
}

static int aggr_break_uhandler(struct kprobe *p, struct pt_regs *regs)
{
	return 0;
}

/*
 * Add the new probe to old_p->list. Fail if this is the
 * second ujprobe at the address - two ujprobes can't coexist
 */
static int add_new_uprobe(struct kprobe *old_p, struct kprobe *p)
{
	if (p->break_handler) {
		if (old_p->break_handler) {
			return -EEXIST;
		}

		list_add_tail_rcu(&p->list, &old_p->list);
		old_p->break_handler = aggr_break_uhandler;
	} else {
		list_add_rcu (&p->list, &old_p->list);
	}

	if (p->post_handler && !old_p->post_handler) {
		old_p->post_handler = aggr_post_uhandler;
	}

	return 0;
}

/*
 * Fill in the required fields of the "manager uprobe". Replace the
 * earlier uprobe in the hlist with the manager uprobe
 */
static inline void add_aggr_uprobe(struct kprobe *ap, struct kprobe *p)
{
	copy_uprobe(p, ap);

	ap->addr = p->addr;
	ap->pre_handler = aggr_pre_uhandler;
	ap->fault_handler = aggr_fault_uhandler;

	if (p->post_handler) {
		ap->post_handler = aggr_post_uhandler;
	}

	if (p->break_handler) {
		ap->break_handler = aggr_break_uhandler;
	}

	INIT_LIST_HEAD(&ap->list);
	list_add_rcu(&p->list, &ap->list);

	hlist_replace_rcu(&p->hlist, &ap->hlist);
}

/*
 * This is the second or subsequent uprobe at the address - handle
 * the intricacies
 */
static int register_aggr_uprobe(struct kprobe *old_p, struct kprobe *p)
{
	int ret = 0;
	struct kprobe *ap;

	if (old_p->pre_handler == aggr_pre_uhandler) {
		copy_uprobe(old_p, p);
		ret = add_new_uprobe(old_p, p);
	} else {
		struct uprobe *uap = kzalloc(sizeof(*uap), GFP_KERNEL);
		if (!uap) {
			return -ENOMEM;
		}

		uap->task = kp2up(p)->task;
		ap = up2kp(uap);
		add_aggr_uprobe(ap, old_p);
		copy_uprobe(ap, p);
		ret = add_new_uprobe(ap, p);
	}

	return ret;
}

static void arm_uprobe(struct uprobe *p)
{
	kprobe_opcode_t insn = BREAKPOINT_INSTRUCTION;
	int ret = write_proc_vm_atomic(p->task, (unsigned long)p->kp.addr,
				       &insn, sizeof(insn));
	if (!ret) {
		panic("arm_uprobe: failed to write memory "
		      "tgid=%u addr=%p!\n", p->task->tgid, p->kp.addr);
	}
}

/**
 * @brief Disarms uprobe.
 *
 * @param p Pointer to the uprobe's kprobe.
 * @param task Pointer to the target task.
 * @return Void.
 */
void disarm_uprobe(struct kprobe *p, struct task_struct *task)
{
	int ret = write_proc_vm_atomic(task, (unsigned long)p->addr,
			               &p->opcode, sizeof(p->opcode));
	if (!ret) {
		panic("disarm_uprobe: failed to write memory "
		      "tgid=%u, addr=%p!\n", task->tgid, p->addr);
	}
}
EXPORT_SYMBOL_GPL(disarm_uprobe);

static void init_uprobes_insn_slots(void)
{
	int i;
	for (i = 0; i < UPROBE_TABLE_SIZE; ++i) {
		INIT_HLIST_HEAD(&uprobe_insn_slot_table[i]);
	}
}

static void init_uprobe_table(void)
{
	int i;
	for (i = 0; i < UPROBE_TABLE_SIZE; ++i) {
		INIT_HLIST_HEAD(&uprobe_table[i]);
	}
}

static void init_uretprobe_inst_table(void)
{
	int i;
	for (i = 0; i < UPROBE_TABLE_SIZE; ++i) {
		INIT_HLIST_HEAD (&uretprobe_inst_table[i]);
	}
}

/**
 * @brief Gets uprobe's kprobe.
 *
 * @param addr Probe's address.
 * @param tgid Probes's thread group ID.
 * @return Pointer to the kprobe on success,\n
 * NULL otherwise.
 */
struct kprobe *get_ukprobe(void *addr, pid_t tgid)
{
	struct hlist_head *head;
	struct kprobe *p;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	head = &uprobe_table[hash_ptr(addr, UPROBE_HASH_BITS)];
	swap_hlist_for_each_entry_rcu(p, node, head, hlist) {
		if (p->addr == addr && kp2up(p)->task->tgid == tgid) {
			return p;
		}
	}

	return NULL;
}

/**
 * @brief Adds uprobe to hlist when trampoline have been made.
 *
 * @param p Pointer to the uprobe's kprobe.
 * @return Void.
 */
void add_uprobe_table(struct kprobe *p)
{
	INIT_HLIST_NODE(&p->is_hlist);
	hlist_add_head_rcu(&p->is_hlist, &uprobe_insn_slot_table[hash_ptr(p->ainsn.insn, UPROBE_HASH_BITS)]);
}

/**
 * @brief Gets kprobe by insn slot.
 *
 * @param addr Probe's address.
 * @param tgit Probe's thread group ID.
 * @param regs Pointer to CPU registers data.
 * @return Pointer to the kprobe on success,\n
 * NULL otherwise.
 */
struct kprobe *get_ukprobe_by_insn_slot(void *addr, pid_t tgid, struct pt_regs *regs)
{
	struct hlist_head *head;
	struct kprobe *p;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	/* TODO: test - two processes invokes instrumented function */
	head = &uprobe_insn_slot_table[hash_ptr(addr, UPROBE_HASH_BITS)];
	swap_hlist_for_each_entry_rcu(p, node, head, is_hlist) {
		if (p->ainsn.insn == addr && kp2up(p)->task->tgid == tgid) {
			return p;
		}
	}

	return NULL;
}


static void remove_uprobe(struct uprobe *up)
{
	arch_remove_uprobe(up);
}

static struct hlist_head *uretprobe_inst_table_head(void *hash_key)
{
	return &uretprobe_inst_table[hash_ptr (hash_key, UPROBE_HASH_BITS)];
}

/* Called with uretprobe_lock held */
static void add_urp_inst(struct uretprobe_instance *ri)
{
	/*
	 * Remove rp inst off the free list -
	 * Add it back when probed function returns
	 */
	hlist_del(&ri->uflist);

	/* Add rp inst onto table */
	INIT_HLIST_NODE(&ri->hlist);
	hlist_add_head(&ri->hlist, uretprobe_inst_table_head(ri->task->mm));

	/* Also add this rp inst to the used list. */
	INIT_HLIST_NODE(&ri->uflist);
	hlist_add_head(&ri->uflist, &ri->rp->used_instances);
}

/* Called with uretprobe_lock held */
static void recycle_urp_inst(struct uretprobe_instance *ri)
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

/* Called with uretprobe_lock held */
static struct uretprobe_instance *get_used_urp_inst(struct uretprobe *rp)
{
	struct uretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->used_instances, uflist) {
		return ri;
	}

	return NULL;
}

/**
 * @brief Gets free uretprobe instanse for the specified uretprobe without
 * allocation. Called with uretprobe_lock held.
 *
 * @param rp Pointer to the uretprobe.
 * @return Pointer to the uretprobe_instance on success,\n
 * NULL otherwise.
 */
struct uretprobe_instance *get_free_urp_inst_no_alloc(struct uretprobe *rp)
{
	struct uretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->free_instances, uflist) {
		return ri;
	}

	return NULL;
}

/* Called with uretprobe_lock held */
static void free_urp_inst(struct uretprobe *rp)
{
	struct uretprobe_instance *ri;
	while ((ri = get_free_urp_inst_no_alloc(rp)) != NULL) {
		hlist_del(&ri->uflist);
		kfree(ri);
	}
}

#define COMMON_URP_NR 10

static int alloc_nodes_uretprobe(struct uretprobe *rp)
{
	int alloc_nodes;
	struct uretprobe_instance *inst;
	int i;

#if 1//def CONFIG_PREEMPT
	rp->maxactive += max(COMMON_URP_NR, 2 * NR_CPUS);
#else
	rp->maxacpptive += NR_CPUS;
#endif
	alloc_nodes = COMMON_URP_NR;

	for (i = 0; i < alloc_nodes; ++i) {
		inst = kmalloc(sizeof(*inst), GFP_ATOMIC);
		if (inst == NULL) {
			free_urp_inst(rp);
			return -ENOMEM;
		}
		INIT_HLIST_NODE(&inst->uflist);
		hlist_add_head(&inst->uflist, &rp->free_instances);
	}

	return 0;
}

/* Called with uretprobe_lock held */
static struct uretprobe_instance *get_free_urp_inst(struct uretprobe *rp)
{
	struct uretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->free_instances, uflist) {
		return ri;
	}

	if (!alloc_nodes_uretprobe(rp)) {
		swap_hlist_for_each_entry(ri, node, &rp->free_instances, uflist) {
			return ri;
		}
	}

	return NULL;
}
// ===================================================================

/**
 * @brief Registers uprobe.
 *
 * @param up Pointer to the uprobe to register.
 * @return 0 on success,\n
 * negative error code on error.
 */
int swap_register_uprobe(struct uprobe *up)
{
	int ret = 0;
	struct kprobe *p, *old_p;

	p = &up->kp;
	if (!p->addr) {
		return -EINVAL;
	}

	DBPRINTF("p->addr = 0x%p p = 0x%p\n", p->addr, p);

// thumb address = address-1;
#if defined(CONFIG_ARM)
	// TODO: must be corrected in 'bundle'
	if ((unsigned long) p->addr & 0x01) {
		p->addr = (kprobe_opcode_t *)((unsigned long)p->addr & 0xfffffffe);
	}
#endif

	p->ainsn.insn = NULL;
	p->mod_refcounted = 0;
	p->nmissed = 0;
	INIT_LIST_HEAD(&p->list);
#ifdef KPROBES_PROFILE
	p->start_tm.tv_sec = p->start_tm.tv_usec = 0;
	p->hnd_tm_sum.tv_sec = p->hnd_tm_sum.tv_usec = 0;
	p->count = 0;
#endif

	// get the first item
	old_p = get_ukprobe(p->addr, kp2up(p)->task->tgid);
	if (old_p) {
#ifdef CONFIG_ARM
		p->safe_arm = old_p->safe_arm;
		p->safe_thumb = old_p->safe_thumb;
#endif
		ret = register_aggr_uprobe(old_p, p);
		DBPRINTF("goto out\n", ret);
		goto out;
	}

	ret = arch_prepare_uprobe(up);
	if (ret) {
		DBPRINTF("goto out\n", ret);
		goto out;
	}

	DBPRINTF ("before out ret = 0x%x\n", ret);

	// TODO: add uprobe (must be in function)
	INIT_HLIST_NODE(&p->hlist);
	hlist_add_head_rcu(&p->hlist, &uprobe_table[hash_ptr(p->addr, UPROBE_HASH_BITS)]);
	arm_uprobe(up);

out:
	DBPRINTF("out ret = 0x%x\n", ret);
	return ret;
}

/**
 * @brief Unregisters uprobe.
 *
 * @param up Pointer to the uprobe.
 * @param disarm Disarm flag. When true uprobe is disarmed.
 * @return Void.
 */
void __swap_unregister_uprobe(struct uprobe *up, int disarm)
{
	struct kprobe *p, *old_p, *list_p;
	int cleanup_p;

	p = &up->kp;
	old_p = get_ukprobe(p->addr, kp2up(p)->task->tgid);
	if (unlikely(!old_p)) {
		return;
	}

	if (p != old_p) {
		list_for_each_entry_rcu(list_p, &old_p->list, list) {
			if (list_p == p) {
				/* uprobe p is a valid probe */
				goto valid_p;
			}
		}

		return;
	}

valid_p:
	if ((old_p == p) || ((old_p->pre_handler == aggr_pre_uhandler) &&
	    (p->list.next == &old_p->list) && (p->list.prev == &old_p->list))) {
		/* Only probe on the hash list */
		if (disarm)
			disarm_uprobe(&up->kp, up->task);

		hlist_del_rcu(&old_p->hlist);
		cleanup_p = 1;
	} else {
		list_del_rcu(&p->list);
		cleanup_p = 0;
	}

	if (cleanup_p) {
		if (p != old_p) {
			list_del_rcu(&p->list);
			kfree(old_p);
		}

		if (!in_atomic()) {
			synchronize_sched();
		}

		remove_uprobe(up);
	} else {
		if (p->break_handler) {
			old_p->break_handler = NULL;
		}

		if (p->post_handler) {
			list_for_each_entry_rcu (list_p, &old_p->list, list) {
				if (list_p->post_handler) {
					cleanup_p = 2;
					break;
				}
			}

			if (cleanup_p == 0) {
				old_p->post_handler = NULL;
			}
		}
	}
}
EXPORT_SYMBOL_GPL(__swap_unregister_uprobe);

/**
 * @brief Unregisters uprobe. Main interface function, wrapper for
 * __swap_unregister_uprobe.
 *
 * @param up Pointer to the uprobe.
 * @return Void.
 */
void swap_unregister_uprobe(struct uprobe *up)
{
	__swap_unregister_uprobe(up, 1);
}

/**
 * @brief Registers ujprobe.
 *
 * @param uj Pointer to the ujprobe function.
 * @return 0 on success,\n
 * error code on error.
 */
int swap_register_ujprobe(struct ujprobe *jp)
{
	int ret = 0;

	/* Todo: Verify probepoint is a function entry point */
	jp->up.kp.pre_handler = setjmp_upre_handler;
	jp->up.kp.break_handler = longjmp_break_uhandler;

	ret = swap_register_uprobe(&jp->up);

	return ret;
}
EXPORT_SYMBOL_GPL(swap_register_ujprobe);

/**
 * @brief Unregisters ujprobe.
 *
 * @param jp Pointer to the ujprobe.
 * @param disarm Disarm flag, passed to __swap_unregister_uprobe.
 * @return Void.
 */
void __swap_unregister_ujprobe(struct ujprobe *jp, int disarm)
{
	__swap_unregister_uprobe(&jp->up, disarm);
	/*
	 * Here is an attempt to unregister even those probes that have not been
	 * installed (hence not added to the hlist).
	 * So if we try to delete them from the hlist we will get NULL pointer
	 * dereference error. That is why we check whether this node
	 * really belongs to the hlist.
	 */
	if (!(hlist_unhashed(&jp->up.kp.is_hlist))) {
		hlist_del_rcu(&jp->up.kp.is_hlist);
	}
}
EXPORT_SYMBOL_GPL(__swap_unregister_ujprobe);

/**
 * @brief Unregisters ujprobe. Main interface function, wrapper for
 * __swap_unregister_ujprobe.
 *
 * @param jp Pointer to the jprobe.
 * @return Void.
 */
void swap_unregister_ujprobe(struct ujprobe *jp)
{
	__swap_unregister_ujprobe(jp, 1);
}
EXPORT_SYMBOL_GPL(swap_unregister_ujprobe);

/**
 * @brief Trampoline uprobe handler.
 *
 * @param p Pointer to the uprobe's kprobe.
 * @param regs Pointer to CPU register data.
 * @return 1
 */
int trampoline_uprobe_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct uretprobe_instance *ri = NULL;
	struct kprobe *kp;
	struct hlist_head *head;
	unsigned long flags, tramp_addr, orig_ret_addr = 0;
	struct hlist_node *tmp;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	tramp_addr = arch_get_trampoline_addr(p, regs);
	spin_lock_irqsave(&uretprobe_lock, flags);

	head = uretprobe_inst_table_head(current->mm);

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
	 *       uretprobe_trampoline
	 */
	swap_hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task != current) {
			/* another task is sharing our hash bucket */
			continue;
		}

		kp = NULL;
		if (ri->rp) {
			kp = up2kp(&ri->rp->up);

			if (ri->rp->handler)
				ri->rp->handler(ri, regs);
		}

		orig_ret_addr = (unsigned long)ri->ret_addr;
		recycle_urp_inst(ri);

		if ((orig_ret_addr != tramp_addr && kp == p) || kp == NULL) {
			/*
			 * This is the real return address. Any other
			 * instances associated with this task are for
			 * other calls deeper on the call stack
			 */
			break;
		}
	}

	spin_unlock_irqrestore(&uretprobe_lock, flags);
	arch_set_orig_ret_addr(orig_ret_addr, regs);

	return 1;
}

static int pre_handler_uretprobe(struct kprobe *p, struct pt_regs *regs)
{
	struct uprobe *up = container_of(p, struct uprobe, kp);
	struct uretprobe *rp = container_of(up, struct uretprobe, up);
#ifdef CONFIG_ARM
	int noret = thumb_mode(regs) ? rp->thumb_noret : rp->arm_noret;
#endif
	struct uretprobe_instance *ri;
	unsigned long flags;

#ifdef CONFIG_ARM
	if (noret)
		return 0;
#endif

	/* TODO: consider to only swap the RA after the last pre_handler fired */
	spin_lock_irqsave(&uretprobe_lock, flags);

	/* TODO: test - remove retprobe after func entry but before its exit */
	if ((ri = get_free_urp_inst(rp)) != NULL) {
		ri->rp = rp;
		ri->task = current;

		if (rp->entry_handler)
			rp->entry_handler(ri, regs);

		arch_prepare_uretprobe(ri, regs);

		add_urp_inst(ri);
	} else {
		++rp->nmissed;
	}

	spin_unlock_irqrestore(&uretprobe_lock, flags);

	return 0;
}

/**
 * @brief Registers uretprobe.
 *
 * @param rp Pointer to the uretprobe.
 * @return 0 on success,\n
 * negative error code on error.
 */
int swap_register_uretprobe(struct uretprobe *rp)
{
	int i, ret = 0;
	struct uretprobe_instance *inst;

	DBPRINTF ("START\n");

	rp->up.kp.pre_handler = pre_handler_uretprobe;
	rp->up.kp.post_handler = NULL;
	rp->up.kp.fault_handler = NULL;
	rp->up.kp.break_handler = NULL;

	/* Pre-allocate memory for max kretprobe instances */
	if (rp->maxactive <= 0) {
#if 1//def CONFIG_PREEMPT
		rp->maxactive = max(10, 2 * NR_CPUS);
#else
		rp->maxactive = NR_CPUS;
#endif
	}

	INIT_HLIST_HEAD(&rp->used_instances);
	INIT_HLIST_HEAD(&rp->free_instances);

	for (i = 0; i < rp->maxactive; i++) {
		inst = kmalloc(sizeof(*inst), GFP_ATOMIC);
		if (inst == NULL) {
			free_urp_inst(rp);
			return -ENOMEM;
		}

		INIT_HLIST_NODE(&inst->uflist);
		hlist_add_head(&inst->uflist, &rp->free_instances);
	}

	rp->nmissed = 0;

	/* Establish function entry probe point */
	ret = swap_register_uprobe(&rp->up);
	if (ret)
		return ret;

	arch_opcode_analysis_uretprobe(rp);

	return 0;
}
EXPORT_SYMBOL_GPL(swap_register_uretprobe);

/**
 * @brief Disarms uretprobe instances for the specified child task.
 *
 * @param parent Pointer to the parent task struct.
 * @param task Pointer to the child task struct.
 * @return 0
 */
int swap_disarm_urp_inst_for_task(struct task_struct *parent, struct task_struct *task)
{
	unsigned long flags;
	struct uretprobe_instance *ri;
	struct hlist_head *head;
	struct hlist_node *tmp;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	spin_lock_irqsave(&uretprobe_lock, flags);

	head = uretprobe_inst_table_head(parent->mm);
	swap_hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (parent == ri->task) {
			arch_disarm_urp_inst(ri, task);
		}
	}

	spin_unlock_irqrestore(&uretprobe_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(swap_disarm_urp_inst_for_task);

/**
 * @brief Disarms uretprobes for specified task.
 *
 * @param task Pointer to the task_struct.
 * @return Void.
 */
void swap_discard_pending_uretprobes(struct task_struct *task)
{
	unsigned long flags;
	struct uretprobe_instance *ri;
	struct hlist_head *head;
	struct hlist_node *tmp;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	spin_lock_irqsave(&uretprobe_lock, flags);

	head = uretprobe_inst_table_head(task->mm);
	swap_hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task == task) {
			printk("%s (%d/%d): pending urp inst: %08lx\n",
			       task->comm, task->tgid, task->pid,
			       (unsigned long)ri->rp->up.kp.addr);
			arch_disarm_urp_inst(ri, task);
			recycle_urp_inst(ri);
		}
	}

	spin_unlock_irqrestore(&uretprobe_lock, flags);
}
EXPORT_SYMBOL_GPL(swap_discard_pending_uretprobes);

/**
 * @brief Unregisters uretprobe.
 *
 * @param rp Pointer to the ureprobe.
 * @param disarm Disarm flag, passed to __swap_unregister_uprobe
 * @return Void.
 */
void __swap_unregister_uretprobe(struct uretprobe *rp, int disarm)
{
	unsigned long flags;
	struct uretprobe_instance *ri;

	__swap_unregister_uprobe(&rp->up, disarm);
	spin_lock_irqsave (&uretprobe_lock, flags);

	while ((ri = get_used_urp_inst(rp)) != NULL) {
		if (arch_disarm_urp_inst(ri, ri->task) != 0)
			printk("%s (%d/%d): cannot disarm urp instance (%08lx)\n",
					ri->task->comm, ri->task->tgid, ri->task->pid,
					(unsigned long)rp->up.kp.addr);
		recycle_urp_inst(ri);
	}

	if (hlist_empty(&rp->used_instances)) {
		struct kprobe *p = &rp->up.kp;

		if (!(hlist_unhashed(&p->is_hlist))) {
			hlist_del_rcu(&p->is_hlist);
		}
	}

	while ((ri = get_used_urp_inst(rp)) != NULL) {
		ri->rp = NULL;
		hlist_del(&ri->uflist);
	}

	spin_unlock_irqrestore(&uretprobe_lock, flags);
	free_urp_inst(rp);
}
EXPORT_SYMBOL_GPL(__swap_unregister_uretprobe);

/**
 * @brief Unregistets uretprobe. Main interface function, wrapper for
 * __swap_unregister_uretprobe.
 *
 * @param rp Pointer to the uretprobe.
 * @return Void.
 */
void swap_unregister_uretprobe(struct uretprobe *rp)
{
	__swap_unregister_uretprobe(rp, 1);
}
EXPORT_SYMBOL_GPL(swap_unregister_uretprobe);

/**
 * @brief Unregisters all uprobes for task's thread group ID.
 *
 * @param task Pointer to the task_struct
 * @return Void.
 */
void swap_unregister_all_uprobes(struct task_struct *task)
{
	struct hlist_head *head;
	struct kprobe *p;
	int i;
	struct hlist_node *tnode;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	for (i = 0; i < UPROBE_TABLE_SIZE; ++i) {
		head = &uprobe_table[i];
		swap_hlist_for_each_entry_safe(p, node, tnode, head, hlist) {
			if (kp2up(p)->task->tgid == task->tgid) {
				struct uprobe *up = container_of(p, struct uprobe, kp);
				printk("%s: delete uprobe at %p[%lx] for "
				       "%s/%d\n", __func__, p->addr,
				       (unsigned long)p->opcode,
				       task->comm, task->pid);
				swap_unregister_uprobe(up);
			}
		}
	}
}
EXPORT_SYMBOL_GPL(swap_unregister_all_uprobes);

/**
 * @brief Arch-independent wrapper for arch_ujprobe_return.
 *
 * @return Void.
 */
void swap_ujprobe_return(void)
{
	arch_ujprobe_return();
}
EXPORT_SYMBOL_GPL(swap_ujprobe_return);

static int once(void)
{
	init_uprobe_table();
	init_uprobes_insn_slots();
	init_uretprobe_inst_table();

	return 0;
}

SWAP_LIGHT_INIT_MODULE(once, swap_arch_init_uprobes, swap_arch_exit_uprobes,
		       NULL, NULL);

MODULE_LICENSE ("GPL");
