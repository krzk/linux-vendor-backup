/**
 * @file writer/swap_writer_module.h
 * @author Alexander Aksenov <a.aksenov@samsung.com>
 * @author Vyacheslav Cherkashin
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
 * Write module interface defenition.
 */

#ifndef _SWAP_MSG_H
#define _SWAP_MSG_H

#include <linux/types.h>

/**
 * @enum PROBE_TYPE
 * @brief Probe types.
 */
enum PROBE_TYPE {
	PT_US	= 1,            /**< User probe */
	PT_KS	= 3             /**< Kernel probe */
};

/**
 * @enum PROBE_SUB_TYPE
 * @brief Probe sub types.
 */
enum PROBE_SUB_TYPE {
	PST_NONE	= 0x00,     /**< Common */
	PST_KS_FILE	= 0x01,     /**< File feature */
	PST_KS_IPC	= 0x02,     /**< Ipc feature */
	PST_KS_PROCESS	= 0x04, /**< Process feature */
	PST_KS_SIGNAL	= 0x08, /**< Signal feature */
	PST_KS_NETWORK	= 0x10, /**< Network feature */
	PST_KS_DESC	= 0x20      /**< Desc feature */
};

struct pt_regs;
struct dentry;
struct task_struct;
struct vm_area_struct;

int init_msg(size_t buf_size);
void uninit_msg(void);

void reset_discarded(void);
unsigned int get_discarded_count(void);
void reset_seq_num(void);

int proc_info_msg(struct task_struct *task, struct dentry *dentry);
void terminate_msg(struct task_struct *task);
void pcoc_map_msg(struct vm_area_struct *vma);
void proc_unmap_msg(unsigned long start, unsigned long end);
void proc_comm_msg(struct task_struct *task);
int sample_msg(struct pt_regs *regs);

int entry_event(const char *fmt, unsigned long func_addr, struct pt_regs *regs,
		enum PROBE_TYPE pt, int sub_type);
int exit_event(char ret_type, struct pt_regs *regs, int pt, int sub_type,
	       unsigned long func_addr, unsigned long ret_addr);

int switch_entry(struct pt_regs *regs);
int switch_exit(struct pt_regs *regs);

int error_msg(const char *fmt, ...);

int raw_msg(char *buf, size_t len);

int custom_entry_event(unsigned long func_addr, struct pt_regs *regs,
		       int pt, int sub_type, const char *fmt, ...);
int custom_exit_event(unsigned long func_addr, unsigned long ret_addr,
		      struct pt_regs *regs, int pt, int sub_type,
		      const char *fmt, ...);

#endif /* _SWAP_MSG_H */
