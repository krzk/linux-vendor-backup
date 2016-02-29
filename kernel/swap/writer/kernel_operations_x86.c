/**
 * writer/kernel_operations_x86.c
 * @author Alexander Aksenov <a.aksenov@samsung.com>
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
 * X86 arch-dependent operations.
 */

#include <asm/ptrace.h>
#include <asm/uaccess.h>
#include <asm/elf.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include "kernel_operations.h"


/* ======================= ARGS ========================== */

/**
 * @brief Returns arg values.
 *
 * @param[out] args Pointer to array where argument values should be stored/
 * @param cnt Arguments count.
 * @param regs Pointer to register data.
 * @return 0.
 */
int get_args(unsigned long args[], int cnt, struct pt_regs *regs)
{
	int i, stack_args = 0;

	/* If we're in kernel mode on x86, get arguments from bx, cx, dx, si,
	 * di, bp
	 */
	if (!user_mode(regs)) {
		int args_in_regs;
		args_in_regs = cnt < 5 ? cnt : 5;
		stack_args = 6;

		switch (args_in_regs) {
			case 5:
				args[5] = regs->bp;
			case 4:
				args[4] = regs->di;
			case 3:
				args[3] = regs->si;
			case 2:
				args[2] = regs->dx;
			case 1:
				args[1] = regs->cx;
			case 0:
				args[0] = regs->bx;
		}
	}

	/* Get other args from stack */
	for (i = stack_args; i < cnt; ++i) {
		unsigned long *args_in_sp = (unsigned long *)regs->sp +
					    1 + i - stack_args;
		if (get_user(args[i], args_in_sp))
			printk("failed to dereference a pointer, addr=%p\n",
			       args_in_sp);
	}

	return 0;
}


/* ================== KERNEL SHARED MEM ===================== */

/**
 * @brief Gets shared kernel memory addresses.
 *
 * @param mm Pointer to process mm_struct.
 * @param[out] start Pointer to the variable where the first shared mem
 * address should be put.
 * @param[out] end Pointer to the variable where the last shared mem
 * address should be put.
 * @return Pointer to the string with shared mem area name.
 */
const char *get_shared_kmem(struct mm_struct *mm, unsigned long *start,
			    unsigned long *end)
{
	unsigned long vdso;
	struct vm_area_struct *vma_vdso;

	vdso = (unsigned long)mm->context.vdso;
	vma_vdso = find_vma_intersection(mm, vdso, vdso + 1);

	if (vma_vdso == NULL) {
		print_err("Cannot get VDSO mapping\n");
		return NULL;
	}

	*start = vma_vdso->vm_start;
	*end = vma_vdso->vm_end;

	return "[vdso]";
}
