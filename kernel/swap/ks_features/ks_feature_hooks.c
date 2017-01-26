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
#include <swap/swap_syshook.h>
#include <asm/syscall.h>
#include "ksf_msg.h"
#include "ks_features.h"
#include "syscall_list.h"
#include "features_data.c"
#include "file_ops.h"
#include "writer/kernel_operations.h"

#include "ks_features_data.h"


static void sys_entry_hendler(struct syshook_probe *p, struct pt_regs *regs)
{
	if (check_event(current)) {
		struct ks_probe *ksp = container_of(p, struct ks_probe, shp);
		const char *fmt = ksp->args;
		const unsigned long func_addr = ksp->sys_addr;
		enum probe_t type = ksp->type;

		ksf_msg_entry(regs, func_addr, type, fmt);
	}
}

static void sys_exit_hendler(struct syshook_probe *p, struct pt_regs *regs)
{
	if (check_event(current)) {
		struct ks_probe *ksp = container_of(p, struct ks_probe, shp);
		const unsigned long func_addr = ksp->sys_addr;
		const unsigned long ret_addr = get_regs_ret_func(regs);
		enum probe_t type = ksp->type;

		ksf_msg_exit(regs, func_addr, ret_addr, type, 'x');
	}
}

static int register_syscall(size_t id)
{
	int ret = 0;

	if (id >= syscall_cnt)
		return -EINVAL;

	ksp[id].shp.sys_entry = sys_entry_hendler;
	ksp[id].shp.sys_exit = sys_exit_hendler;
#ifdef CONFIG_COMPAT
	ret = swap_syshook_reg_compat(&ksp[id].shp, ksp[id].id);
#else
	ret = swap_syshook_reg(&ksp[id].shp, ksp[id].id);
#endif

	if (ret != 0)
		pr_err("ERROR: cannot register hook '%s' id=%zd sysid=%u\n",
		       get_sys_name(id), id, ksp[id].id);

	return ret;
}

static int unregister_syscall(size_t id)
{
	if (id >= syscall_cnt)
		return -EINVAL;

#ifdef CONFIG_COMPAT
	swap_syshook_unreg_compat(&ksp[id].shp);
#else
	swap_syshook_unreg(&ksp[id].shp);
#endif

	return 0;
}

static int init_syscalls(void)
{
	size_t i, sys_id;
	unsigned long addr;
	const char *name;
	const char *sys_call_table_name;
	unsigned long *syscall_table;

#ifdef CONFIG_COMPAT
	sys_call_table_name = "compat_sys_call_table";
#else
	sys_call_table_name = "sys_call_table";
#endif

	syscall_table = (unsigned long *)swap_ksyms(sys_call_table_name);
	if (syscall_table == NULL) {
		pr_warn("WARN: '%s' not found\n", sys_call_table_name);
		return 0;
	}

	for (i = 0; i < syscall_cnt; ++i) {
		name = get_sys_name(i);
		sys_id = ksp[i].id;
		addr = syscall_table[sys_id];
		if (addr == 0) {
			pr_warn("WARN: %s() not found\n", name);
		}

		ksp[i].sys_addr = addr;
	}

	return 0;
}
