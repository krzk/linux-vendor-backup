/*
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/sched.h>
#include <writer/swap_msg.h>
#include <writer/kernel_operations.h>
#include <kprobe/swap_kprobes.h>
#include "ksf_msg.h"


#define KSF_PREFIX	KERN_INFO "[KSF] "





/* ============================================================================
 * =                       MSG_SYSCALL_* (ENTRY/EXIT)                         =
 * ============================================================================
 */
struct msg_sys_header {
	u32 pid;
	u32 tid;
	u16 probe_type;
	u16 probe_sub_type;
	u64 pc_addr;
	u64 caller_pc_addr;
	u32 cpu_num;
} __packed;

struct msg_sys_entry {
	struct msg_sys_header h;
	u32 cnt_args;
	char args[0];
} __packed;

struct msg_sys_exit {
	struct msg_sys_header h;
	char ret_val[0];
} __packed;


static void pack_header(struct msg_sys_header *h, unsigned long func_addr,
			unsigned long ret_addr, enum probe_t type)
{
	struct task_struct *task = current;

	h->pid = task->tgid;
	h->tid = task->pid;
	h->probe_type = (u16)PT_KS;
	h->probe_sub_type = (u16)type;
	h->pc_addr = func_addr;
	h->caller_pc_addr = ret_addr;
	h->cpu_num = smp_processor_id();
}

static void pack_entry_header(struct msg_sys_entry *e, struct pt_regs *regs,
			      unsigned long func_addr, enum probe_t type,
			      const char *fmt)
{
	pack_header(&e->h, func_addr, get_regs_ret_func(regs), type);
	e->cnt_args = strlen(fmt);
}

static void pack_exit_header(struct msg_sys_exit *e, unsigned long func_addr,
			     unsigned long ret_addr, enum probe_t type)
{
	pack_header(&e->h, func_addr, ret_addr, type);
}

void ksf_msg_entry(struct pt_regs *regs, unsigned long func_addr,
		   enum probe_t type, const char *fmt)
{
	int ret;
	struct swap_msg *m;
	struct msg_sys_entry *ent;
	size_t size;

	return;

	m = swap_msg_get(MSG_SYSCALL_ENTRY);

	ent = swap_msg_payload(m);
	pack_entry_header(ent, regs, func_addr, type, fmt);

	size = swap_msg_size(m) - sizeof(*ent);
	ret = swap_msg_pack_args(ent->args, size, fmt, regs);
	if (ret < 0) {
		printk(KSF_PREFIX "ERROR: arguments packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, sizeof(*ent) + ret);

put_msg:
	swap_msg_put(m);
}

void ksf_msg_exit(struct pt_regs *regs, unsigned long func_addr,
		  unsigned long ret_addr, enum probe_t type, char ret_type)
{
	int ret;
	struct swap_msg *m;
	struct msg_sys_exit *ext;
	size_t size;

	return;

	m = swap_msg_get(MSG_SYSCALL_EXIT);

	ext = swap_msg_payload(m);
	pack_exit_header(ext, func_addr, ret_addr, type);

	size = swap_msg_size(m) - sizeof(*ext);
	ret = swap_msg_pack_ret_val(ext->ret_val, size, ret_type, regs);
	if (ret < 0) {
		printk(KSF_PREFIX "ERROR: ret value packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, sizeof(*ext) + ret);

put_msg:
	swap_msg_put(m);
}





/* ============================================================================
 * =                    MSG_FILE_FUNCTION_* (ENTRY/EXIT)                      =
 * ============================================================================
 */
struct msg_file_entry {
	u32 pid;
	u32 tid;
	u16 probe_type;
	u16 probe_sub_type;
	u64 pc_addr;
	u64 caller_pc_addr;
	u32 cpu_num;
	u32 cnt_args;
	char args[0];
} __packed;

static void *pack_file_entry_head(void *data, size_t size, struct pt_regs *regs,
				  enum file_api_t api, u32 cnt_args)
{
	struct msg_file_entry *ent = (struct msg_file_entry *)data;
	struct task_struct *task = current;

	ent->pid = task->tgid;
	ent->tid = task->pid;
	ent->probe_type = PT_FILE;
	ent->probe_sub_type = api;
	ent->pc_addr = swap_get_instr_ptr(regs);
	ent->caller_pc_addr =  get_regs_ret_func(regs);
	ent->cpu_num = smp_processor_id();
	ent->cnt_args = cnt_args;

	return data + sizeof(*ent);
}



void ksf_msg_file_entry(struct pt_regs *regs, int fd, const char *path,
			enum file_api_t api)
{
	int ret;
	void *p, *p_begin;
	size_t size;
	struct swap_msg *m;

	m = swap_msg_get(MSG_FILE_FUNCTION_ENTRY);
	p_begin = p = swap_msg_payload(m);
	size = swap_msg_size(m);

	p = pack_file_entry_head(p, size, regs, api, 2);
	ret = swap_msg_pack_custom_args(p, size, "Sx", path, fd);
	if (ret < 0) {
		pr_err(KSF_PREFIX "buffer is too small\n");
		goto put_msg;
	}
	p += ret;

	swap_msg_flush(m, p - p_begin);

put_msg:
	swap_msg_put(m);
}

void ksf_msg_file_entry_open(struct pt_regs *regs, int fd, const char *path,
			     enum file_api_t api, const char __user *ofile)
{
	int ret;
	void *p, *p_begin;
	size_t size;
	struct swap_msg *m;

	m = swap_msg_get(MSG_FILE_FUNCTION_ENTRY);
	p_begin = p = swap_msg_payload(m);
	size = swap_msg_size(m);

	p = pack_file_entry_head(p, size, regs, api, 3);
	ret = swap_msg_pack_custom_args(p, size, "Sxs", path, fd, ofile);
	if (ret < 0) {
		printk(KSF_PREFIX "buffer is too small\n");
		goto put_msg;
	}
	p += ret;

	swap_msg_flush(m, p - p_begin);

put_msg:
	swap_msg_put(m);
}

void ksf_msg_file_entry_lock(struct pt_regs *regs, int fd, const char *path,
			     enum file_api_t api,
			     int type, int whence, s64 start, s64 len)
{
	int ret;
	void *p, *p_begin;
	size_t size;
	struct swap_msg *m;

	m = swap_msg_get(MSG_FILE_FUNCTION_ENTRY);
	p_begin = p = swap_msg_payload(m);
	size = swap_msg_size(m);

	p = pack_file_entry_head(p, size, regs, api, 6);
	ret = swap_msg_pack_custom_args(p, size, "Sxddxx", path, fd,
					type, whence, start, len);
	if (ret < 0) {
		printk(KSF_PREFIX "buffer is too small\n");
		goto put_msg;
	}
	p += ret;

	swap_msg_flush(m, p - p_begin);

put_msg:
	swap_msg_put(m);
}


struct msg_file_exit {
	u32 pid;
	u32 tid;
	u16 probe_type;
	u16 probe_sub_type;
	u64 pc_addr;
	u64 caller_pc_addr;
	u32 cpu_num;
	char ret_val[0];
} __packed;

void ksf_msg_file_exit(struct pt_regs *regs, enum file_api_t api,
		       char ret_type)
{
	struct task_struct *task = current;
	int ret;
	struct swap_msg *m;
	struct msg_file_exit *ext;
	size_t size;

	m = swap_msg_get(MSG_FILE_FUNCTION_EXIT);

	ext = swap_msg_payload(m);
	ext->pid = task->tgid;
	ext->tid = task->pid;
	ext->probe_type = PT_FILE;
	ext->probe_sub_type = api;
	ext->pc_addr = swap_get_instr_ptr(regs);
	ext->caller_pc_addr = get_regs_ret_func(regs);
	ext->cpu_num = smp_processor_id();

	size = swap_msg_size(m) - sizeof(*ext);
	ret = swap_msg_pack_ret_val(ext->ret_val, size, ret_type, regs);
	if (ret < 0) {
		printk(KSF_PREFIX "ERROR: ret value packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, sizeof(*ext) + ret);

put_msg:
	swap_msg_put(m);
}





/* ============================================================================
 * =                    MSG_FILE_FUNCTION_* (ENTRY/EXIT)                      =
 * ============================================================================
 */
struct msg_context_switch {
	u64 pc_addr;
	u32 pid;
	u32 tid;
	u32 cpu_num;
} __packed;

static void context_switch(struct task_struct *task, enum swap_msg_id id)
{
	struct swap_msg *m;
	struct msg_context_switch *mcs;
	void *p;

	m = swap_msg_get(id);
	p = swap_msg_payload(m);

	mcs = p;
	mcs->pc_addr = 0;
	mcs->pid = task->tgid;
	mcs->tid = task->pid;
	mcs->cpu_num = smp_processor_id();

	swap_msg_flush_wakeupoff(m, sizeof(*mcs));
	swap_msg_put(m);
}

void ksf_switch_entry(struct task_struct *task)
{
	context_switch(task, MSG_CONTEXT_SWITCH_ENTRY);
}

void ksf_switch_exit(struct task_struct *task)
{
	context_switch(task, MSG_CONTEXT_SWITCH_EXIT);
}
