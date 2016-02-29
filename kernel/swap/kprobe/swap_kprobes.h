/**
 * @file kprobe/swap_kprobes.h
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
 * SWAP kprobe interface definition.
 */


#ifndef _SWAP_KPROBES_H
#define _SWAP_KPROBES_H

#include <linux/version.h>	// LINUX_VERSION_CODE, KERNEL_VERSION()
#include <linux/notifier.h>
#include <linux/percpu.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/pagemap.h>

#include <swap-asm/swap_kprobes.h>


#ifdef CONFIG_ARM

#define regs_return_value(regs)     ((regs)->ARM_r0)

#endif


/* kprobe_status settings */
/** Kprobe hit active */
#define KPROBE_HIT_ACTIVE	0x00000001
/** Kprobe hit ss */
#define KPROBE_HIT_SS		0x00000002
/** Kprobe reenter */
#define KPROBE_REENTER		0x00000004
/** Kprobe hit ss done */
#define KPROBE_HIT_SSDONE	0x00000008

/** High word */
#define HIWORD(x)               (((x) & 0xFFFF0000) >> 16)
/** Low word */
#define LOWORD(x)               ((x) & 0x0000FFFF)

/** Invalid value */
#define INVALID_VALUE           0xFFFFFFFF
/** Invalid pointer */
#define INVALID_POINTER         (void*)INVALID_VALUE

/** Jprobe entry */
#define JPROBE_ENTRY(pentry)    (kprobe_opcode_t *)pentry

/** Retprobe stack depth */
#define RETPROBE_STACK_DEPTH 64

struct kprobe;
struct pt_regs;
struct kretprobe;
struct kretprobe_instance;

/**
 * @brief Kprobe pre-handler pointer.
 */
typedef int (*kprobe_pre_handler_t) (struct kprobe *, struct pt_regs *);

/**
 * @brief Kprobe break handler pointer.
 */
typedef int (*kprobe_break_handler_t) (struct kprobe *, struct pt_regs *);

/**
 * @brief Kprobe post handler pointer.
 */
typedef void (*kprobe_post_handler_t) (struct kprobe *, struct pt_regs *, unsigned long flags);

/**
 * @brief Kprobe fault handler pointer.
 */
typedef int (*kprobe_fault_handler_t) (struct kprobe *, struct pt_regs *, int trapnr);

/**
 * @brief Kretprobe handler pointer.
 */
typedef int (*kretprobe_handler_t) (struct kretprobe_instance *, struct pt_regs *);

/**
 * @struct kprobe
 * @brief Main kprobe struct.
 */
struct kprobe
{
	struct hlist_node				hlist;              /**< Hash list.*/
	/** List of probes to search by instruction slot.*/
	struct hlist_node				is_hlist;
	/** List of kprobes for multi-handler support.*/
	struct list_head				list;
	/** Indicates that the corresponding module has been ref counted.*/
	unsigned int					mod_refcounted;
	/** Count the number of times this probe was temporarily disarmed.*/
	unsigned long					nmissed;
	/** Location of the probe point. */
	kprobe_opcode_t					*addr;
	/** Allow user to indicate symbol name of the probe point.*/
	char						*symbol_name;
	/** Offset into the symbol.*/
	unsigned int					offset;
	/** Called before addr is executed.*/
	kprobe_pre_handler_t				pre_handler;
	/** Called after addr is executed, unless...*/
	kprobe_post_handler_t				post_handler;
	/** ... called if executing addr causes a fault (eg. page fault).*/
	kprobe_fault_handler_t				fault_handler;
	/** Return 1 if it handled fault, otherwise kernel will see it.*/
	kprobe_break_handler_t				break_handler;
	/** Saved opcode (which has been replaced with breakpoint).*/
	kprobe_opcode_t					opcode;
	/** Copy of the original instruction.*/
	struct arch_specific_insn			ainsn;
	/** Override single-step target address, may be used to redirect 
	 * control-flow to arbitrary address after probe point without
	 * invocation of original instruction; useful for functions
	 * replacement. If jprobe.entry should return address of function or
	 * NULL if original function should be called.
	 * Not supported for X86, not tested for MIPS. */
	kprobe_opcode_t					*ss_addr[NR_CPUS];
#ifdef CONFIG_ARM
	/** Safe/unsafe to use probe on ARM.*/
	unsigned					safe_arm:1;
	/** Safe/unsafe to use probe on Thumb.*/
	unsigned					safe_thumb:1;
#endif
};

/**
 * @brief Kprobe pre-entry handler pointer.
 */
typedef unsigned long (*kprobe_pre_entry_handler_t) (void *priv_arg, struct pt_regs * regs);


/**
 * @struct jprobe
 * @brief Special probe type that uses setjmp-longjmp type tricks to resume
 * execution at a specified entry with a matching prototype corresponding
 * to the probed function - a trick to enable arguments to become
 * accessible seamlessly by probe handling logic.
 * Note:
 * Because of the way compilers allocate stack space for local variables
 * etc upfront, regardless of sub-scopes within a function, this mirroring
 * principle currently works only for probes placed on function entry points.
 */ 
struct jprobe
{
	struct kprobe kp;                   /**< This probes kprobe.*/
	kprobe_opcode_t *entry;             /**< Probe handling code to jump to.*/
	/** Handler which will be called before 'entry'. */
	kprobe_pre_entry_handler_t pre_entry;
	void *priv_arg;                     /**< Private args.*/
};


/**
 * @struct jprobe_instance
 * @brief Jprobe instance struct.
 */
struct jprobe_instance
{
	// either on free list or used list
	struct hlist_node uflist;            /**< Jprobes hash list. */
	struct hlist_node hlist;             /**< Jprobes hash list. */
	struct jprobe *jp;                   /**< Pointer to the target jprobe. */
	/** Pointer to the target task_struct. */
	struct task_struct *task;
};





/**
 * @struct kretprobe
 * @brief Function-return probe
 * Note: User needs to provide a handler function, and initialize maxactive.
 */
struct kretprobe
{
	struct kprobe kp;                    /**< Kprobe of this kretprobe.*/
	kretprobe_handler_t handler;         /**< Handler of this kretprobe.*/
	kretprobe_handler_t entry_handler;   /**< Entry handler of this kretprobe.*/
	/** The maximum number of instances of the probed function that can be
	 * active concurrently. */
	int maxactive;
	/** Tracks the number of times the probed function's return was ignored,
	 * due to maxactive being too low. */
	int nmissed;
	size_t data_size;                    /**< Size of the data. */
	/** List of this probe's free_instances. */
	struct hlist_head free_instances;
	/** List of this probe's used_instances. */
	struct hlist_head used_instances;

#ifdef CONFIG_ARM
	unsigned					arm_noret:1;    /**< No-return flag for ARM.*/
	unsigned					thumb_noret:1;  /**< No-return flag for Thumb.*/
#endif

};

/**
 * @struct kretprobe_instance
 * @brief Instance of kretprobe.
 */
struct kretprobe_instance
{
	// either on free list or used list
	struct hlist_node uflist;       /**< Kretprobe hash list.*/
	struct hlist_node hlist;        /**< Kretprobe hash list.*/
	struct kretprobe *rp;           /**< Pointer to this instance's kretprobe.*/
	unsigned long *ret_addr;        /**< Return address.*/
	unsigned long *sp;              /**< Stack pointer.*/
	struct task_struct *task;       /**< Pointer to the target task_struct.*/
	char data[0];                   /**< Pointer to data.*/
};


extern void swap_kprobes_inc_nmissed_count(struct kprobe *p);

//
// Large value for fast but memory consuming implementation
// it is good when a lot of probes are instrumented
//
//#define KPROBE_HASH_BITS 6
#define KPROBE_HASH_BITS 16
#define KPROBE_TABLE_SIZE (1 << KPROBE_HASH_BITS)


/* Get the kprobe at this addr (if any) - called with preemption disabled */
struct kprobe *swap_get_kprobe(void *addr);


int swap_register_kprobe(struct kprobe *p);
void swap_unregister_kprobe(struct kprobe *p);

int swap_setjmp_pre_handler(struct kprobe *, struct pt_regs *);
int swap_longjmp_break_handler(struct kprobe *, struct pt_regs *);

int swap_register_jprobe(struct jprobe *p);
void swap_unregister_jprobe(struct jprobe *p);
void swap_jprobe_return(void);


int swap_register_kretprobe(struct kretprobe *rp);
void swap_unregister_kretprobe(struct kretprobe *rp);
void swap_unregister_kretprobes(struct kretprobe **rpp, size_t size);

/*
 * use:
 *	swap_unregister_kretprobe[s]_top();
 *	synchronize_sched();
 *	swap_unregister_kretprobe[s]_bottom();
 *
 * rp_disarm - indicates the need for restoration of the return address
 */
void swap_unregister_kretprobe_top(struct kretprobe *rp, int rp_disarm);
void swap_unregister_kretprobes_top(struct kretprobe **rps, size_t size,
				   int rp_disarm);
void swap_unregister_kretprobe_bottom(struct kretprobe *rp);
void swap_unregister_kretprobes_bottom(struct kretprobe **rps, size_t size);


int swap_disarm_urp_inst_for_task(struct task_struct *parent, struct task_struct *task);

int trampoline_probe_handler (struct kprobe *p, struct pt_regs *regs);


DECLARE_PER_CPU(struct kprobe *, swap_current_kprobe);
extern atomic_t kprobe_count;
extern unsigned long sched_addr;

struct kprobe *swap_kprobe_running(void);
void swap_reset_current_kprobe(void);
struct kprobe_ctlblk *swap_get_kprobe_ctlblk(void);

void prepare_singlestep(struct kprobe *p, struct pt_regs *regs);

#endif /* _SWAP_KPROBES_H */

