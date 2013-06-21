/*
 * Copyright (C) 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
 * Copyright (C) 2013 Mauro Ribeiro <mauro.ribeiro@hardkernel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "linux/kernel.h"
#include "linux/mm.h"
#include <linux/uaccess.h>
#include <asm/memory.h>   
#include <linux/unistd.h> 
#include "linux/semaphore.h"
#include <linux/vmalloc.h>  
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/sched.h> /* wake_up_process() */
#include <linux/kthread.h> /* kthread_create(), kthread_run() */
#include <linux/err.h> /* IS_ERR(), PTR_ERR() */
#include <linux/delay.h>
#include <linux/platform_device.h>
#include "asm-generic/int-ll64.h" 
#include <linux/errno.h>
#include <linux/slab.h> 
#include <linux/delay.h>
#include <linux/init.h> 
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>  
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/module.h>
#include <video/odroid_ump.h>

#include "linux/ump_kernel_linux.h"
#include "common/ump_kernel_memory_backend.h"
#include <ump/ump_kernel_interface_ref_drv.h>

int (*disp_get_ump_secure_id) (struct fb_info *info, unsigned long arg, int buf);
EXPORT_SYMBOL(disp_get_ump_secure_id);

static int _disp_get_ump_secure_id(struct fb_info *info, unsigned long arg, int buf) {

	pr_emerg("odroid_ump: _disp called\n");
	pr_emerg("odroid_ump: 1\n");
	int buf_len = info->fix.smem_len;

	pr_emerg("odroid_ump: 2\n");
	if (info->var.yres * 2 == info->var.yres_virtual) {
		pr_emerg("odroid_ump: 3\n");
		buf_len = buf_len >> 1;	
	} else {
		pr_emerg("HardkernelUMP: Double buffer disabled!\n");
	}
	
	pr_emerg("odroid_ump: 4\n");
	u32 __user *psecureid = (u32 __user *) arg;
	pr_emerg("odroid_ump: 5\n");
	ump_secure_id secure_id;
	pr_emerg("odroid_ump: 6\n");
	ump_dd_physical_block ump_memory_description;
	pr_emerg("odroid_ump: 7\n");
	ump_dd_handle ump_wrapped_buffer;
	pr_emerg("odroid_ump: 8\n");
	ump_memory_description.addr = info->fix.smem_start + (buf_len * buf);
	pr_emerg("odroid_ump: 9\n");
	ump_memory_description.size = info->fix.smem_len;
	pr_emerg("odroid_ump: 10\n");
	                                
	if(buf > 0) { 
		pr_emerg("odroid_ump: 11\n");
		ump_memory_description.addr += (buf_len * (buf - 1));
		pr_emerg("odroid_ump: 12\n");
		ump_memory_description.size = buf_len;
		pr_emerg("odroid_ump: 13\n");
	} 
	pr_emerg("odroid_ump: 14\n");
	ump_wrapped_buffer = ump_dd_handle_create_from_phys_blocks(&ump_memory_description, 1);
pr_emerg("odroid_ump: 15\n");
	secure_id = ump_dd_secure_id_get(ump_wrapped_buffer);
	pr_emerg("odroid_ump: 16\n");
	return put_user((unsigned int)secure_id, psecureid);
}

static int __init disp_ump_module_init(void) {

	disp_get_ump_secure_id = _disp_get_ump_secure_id;
	pr_emerg("Hardkernel UMP: Loaded!\n");

	return 0;
}

static void __exit disp_ump_module_exit(void) {
	disp_get_ump_secure_id = NULL;
}

module_init(disp_ump_module_init);
module_exit(disp_ump_module_exit);

MODULE_AUTHOR("Henrik Nordstrom <henrik@henriknordstrom.net>");
MODULE_AUTHOR("Mauro Ribeiro <mauro.ribeiro@hardkernel.com>");
MODULE_DESCRIPTION("ODROID UMP Glue Driver");
MODULE_LICENSE("GPL");
