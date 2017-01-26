/**
 * ks_features/ks_features.c
 * @author Vyacheslav Cherkashin: SWAP ks_features implement
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 *  SWAP kernel features
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <ksyms/ksyms.h>
#include <kprobe/swap_kprobes.h>
#include <master/swap_initializer.h>
#include <writer/event_filter.h>
#ifdef CONFIG_SWAP_CONTEXT_HOOKS
#include <swap/swap_context_hooks.h>
#endif	/* CONFIG_SWAP_CONTEXT_HOOKS */
#include "ksf_msg.h"
#include "ks_features.h"
#include "file_ops.h"

#ifdef CONFIG_SWAP_SYSHOOK
# include "ks_feature_hooks.c"
#else
# include "ks_feature_handlers.c"
#endif



/* ====================== SWITCH_CONTEXT ======================= */
static int switch_entry_handler(struct kretprobe_instance *ri,
				struct pt_regs *regs)
{
	if (check_event(current))
		ksf_switch_entry(current);

	return 0;
}

static int switch_ret_handler(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	if (check_event(current))
		ksf_switch_exit(current);

	return 0;
}

/**
 * @var switch_rp
 * Kretprobe for scheduler.
 */
struct kretprobe switch_rp = {
	.entry_handler = switch_entry_handler,
	.handler = switch_ret_handler
};

static DEFINE_MUTEX(mutex_sc_enable);
static int sc_enable;


#ifdef CONFIG_SWAP_CONTEXT_HOOKS
static struct swap_ctx_hook switch_to_hook;

/**
 * @brief Get scheduler address.
 *
 * @return 0 on success, negative error code on error.
 */
int init_switch_context(void)
{
	return 0;
}

static void ksf_switch(struct task_struct *prev, struct task_struct *next)
{
	if (check_event(prev))
		ksf_switch_entry(prev);
	if (check_event(next))
		ksf_switch_exit(next);
}

static int register_ctx_handler(void)
{
	switch_to_hook.hook_switch_to = ksf_switch;
	swap_context_hooks_reg(&switch_to_hook);
	return 0;
}

static void unregister_ctx_handler(void)
{
	swap_context_hooks_unreg(&switch_to_hook);
}
#else  /* CONFIG_SWAP_CONTEXT_HOOKS */
/**
 * @brief Get scheduler address.
 *
 * @return 0 on success, negative error code on error.
 */
int init_switch_context(void)
{
	switch_rp.kp.addr = swap_ksyms("__switch_to");
	if (switch_rp.kp.addr == 0) {
		printk(KERN_INFO "ERROR: not found '__switch_to'\n");
		return -EINVAL;
	}
	return 0;
}

static int register_ctx_handler(void)
{
	return swap_register_kretprobe(&switch_rp);
}

static void unregister_ctx_handler(void)
{
	swap_unregister_kretprobe(&switch_rp);
}
#endif	/* CONFIG_SWAP_CONTEXT_HOOKS */

/**
 * @brief Unregisters probe on context switching.
 *
 * @return Void.
 */
void exit_switch_context(void)
{
	if (sc_enable)
		unregister_ctx_handler();
}

static int register_switch_context(void)
{
	int ret = -EINVAL;

	mutex_lock(&mutex_sc_enable);
	if (sc_enable) {
		printk(KERN_INFO "switch context profiling is already run!\n");
		goto unlock;
	}

	ret = register_ctx_handler();
	if (ret == 0)
		sc_enable = 1;

unlock:
	mutex_unlock(&mutex_sc_enable);

	return ret;
}

static int unregister_switch_context(void)
{
	int ret = 0;

	mutex_lock(&mutex_sc_enable);
	if (sc_enable == 0) {
		printk(KERN_INFO "switch context profiling is not running!\n");
		ret = -EINVAL;
		goto unlock;
	}

	unregister_ctx_handler();

	sc_enable = 0;
unlock:
	mutex_unlock(&mutex_sc_enable);

	return ret;
}
/* ====================== SWITCH_CONTEXT ======================= */




static void set_pst(struct feature *f, size_t id)
{
	ksp[id].type |= f->type;
}

static void unset_pst(struct feature *f, size_t id)
{
	ksp[id].type &= !f->type;
}

static void do_uninstall_features(struct feature *f, size_t i)
{
	const size_t end = ((size_t) 0) - 1;

	for (; i != end; --i) {
		size_t id;

		id = f->feature_list[i];
		if (get_counter(id) == 0) {
			pr_err("syscall %s not installed\n", get_sys_name(id));
			BUG();
		}

		dec_counter(id);
		if (get_counter(id) == 0) {
			int ret;

			ret = unregister_syscall(id);
			if (ret)
				pr_err("syscall %s uninstall error, ret=%d\n",
				       get_sys_name(id), ret);
		}

		unset_pst(f, id);
	}
}

static int do_install_features(struct feature *f)
{
	int ret;
	size_t i, id;

	for (i = 0; i < f->cnt; ++i) {
		id = f->feature_list[i];
		set_pst(f, id);

		if (get_counter(id) == 0) {
			ret = register_syscall(id);
			if (ret) {
				printk(KERN_INFO "syscall %s install error, ret=%d\n",
				       get_sys_name(id), ret);

				do_uninstall_features(f, --i);
				return ret;
			}
		}

		inc_counter(id);
	}

	return 0;
}

static DEFINE_MUTEX(mutex_features);

static int install_features(struct feature *f)
{
	int ret = 0;

	mutex_lock(&mutex_features);
	if (f->enable) {
		printk(KERN_INFO "energy profiling is already run!\n");
		ret = -EINVAL;
		goto unlock;
	}

	ret = do_install_features(f);

	f->enable = 1;
unlock:
	mutex_unlock(&mutex_features);
	return ret;
}

static int uninstall_features(struct feature *f)
{
	int ret = 0;

	mutex_lock(&mutex_features);
	if (f->enable == 0) {
		printk(KERN_INFO "feature[%d] is not running!\n",
		       feature_index(f));
		ret = -EINVAL;
		goto unlock;
	}
	do_uninstall_features(f, f->cnt - 1);
	f->enable = 0;
unlock:
	mutex_unlock(&mutex_features);

	return ret;
}

static struct feature *get_feature(enum feature_id id)
{
	if (id < 0 || id >= (int)feature_cnt)
		return NULL;

	return &features[id];
}

/**
 * @brief Sets probes related to specified feature.
 *
 * @param id Feature id.
 * @return 0 on success, negative error code on error.
 */
int set_feature(enum feature_id id)
{
	struct feature *f;
	int ret;

	switch (id) {
	case FID_FILE:
		ret = file_ops_init();
		break;
	case FID_SWITCH:
		ret = register_switch_context();
		break;
	default:
		f = get_feature(id);
		ret = f ? install_features(f) : -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(set_feature);

/**
 * @brief Unsets probes related to specified feature.
 *
 * @param id Feature id.
 * @return 0 on success, negative error code on error.
 */
int unset_feature(enum feature_id id)
{
	struct feature *f;
	int ret = 0;

	switch (id) {
	case FID_FILE:
		file_ops_exit();
		break;
	case FID_SWITCH:
		ret = unregister_switch_context();
		break;
	default:
		f = get_feature(id);
		ret = f ? uninstall_features(f) : -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(unset_feature);

static int init_syscall_features(void)
{
	return init_syscalls();
}

static void uninit_syscall_features(void)
{
	size_t id;

	for (id = 0; id < syscall_cnt; ++id) {
		if (get_counter(id) > 0)
			unregister_syscall(id);
	}
}


static int once(void)
{
	int ret;

	ret = init_switch_context();
	if (ret)
		return ret;

	ret = init_syscall_features();

	return ret;
}

static void core_uninit(void)
{
	uninit_syscall_features();
	exit_switch_context();

	if (file_ops_is_init())
		file_ops_exit();
}

SWAP_LIGHT_INIT_MODULE(once, NULL, core_uninit, NULL, NULL);

MODULE_LICENSE("GPL");

/* debug */
static void print_feature(struct feature *f)
{
	size_t i;

	for (i = 0; i < f->cnt; ++i)
		pr_info("    feature[%3zu]: %s\n", i,
			get_sys_name(f->feature_list[i]));
}

/**
 * @brief Prints features.
 *
 * @return Void.
 */
void print_features(void)
{
	int i;

	printk(KERN_INFO "print_features:\n");
	for (i = 0; i < feature_cnt; ++i) {
		printk(KERN_INFO "feature: %d\n", i);
		print_feature(&features[i]);
	}
}

/**
 * @brief Prints all syscalls.
 *
 * @return Void.
 */
void print_all_syscall(void)
{
	int i;

	printk(KERN_INFO "SYSCALL:\n");
	for (i = 0; i < syscall_cnt; ++i)
		printk(KERN_INFO "    [%2d] %s\n",
		       get_counter(i), get_sys_name(i));
}
/* debug */
