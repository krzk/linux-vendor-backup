#ifndef __REGS_DEBUG_H
#define __REGS_DEBUG_H

#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/jiffies.h>
#include <mach/sec_debug.h>

#ifdef CONFIG_SEC_DEBUG_REG_ACCESS
struct sec_debug_regs_access {
	u32 vaddr;
	u32 value;
	u32 stack;
	u32 pc;
	unsigned long time;
	unsigned int status;
};

#define sec_debug_regs_read_start(a)	({u32 cpu_id, stack, lr;	\
		if (get_sec_debug_level()) {			\
		asm volatile(						\
			"	mrc	p15, 0, %0, c0, c0, 5\n"	\
			"	ands %0, %0, #0xf\n"			\
			"	mov %2, r13\n"				\
			"	mov %1, lr\n"				\
			: "=&r" (cpu_id), "=&r" (lr), "=&r" (stack)	\
			:						\
			: "memory");					\
			\
		sec_debug_last_regs_access[cpu_id].value = 0;		\
		sec_debug_last_regs_access[cpu_id].vaddr = (u32)a;	\
		sec_debug_last_regs_access[cpu_id].stack = stack;	\
		sec_debug_last_regs_access[cpu_id].pc = lr;		\
		sec_debug_last_regs_access[cpu_id].time = jiffies;     \
		sec_debug_last_regs_access[cpu_id].status = 1;	\
		}})

#define sec_debug_regs_write_start(v, a)	({u32 cpu_id, stack, lr;	\
		if (get_sec_debug_level()) {			\
		asm volatile(						\
			"	mrc	p15, 0, %0, c0, c0, 5\n"	\
			"	ands %0, %0, #0xf\n"			\
			"	mov %2, r13\n"				\
			"	mov %1, lr\n"				\
			: "=&r" (cpu_id), "=&r" (lr), "=&r" (stack)	\
			:						\
			: "memory");					\
			\
		sec_debug_last_regs_access[cpu_id].value = (u32)(v);	\
		sec_debug_last_regs_access[cpu_id].vaddr = (u32)(a);	\
		sec_debug_last_regs_access[cpu_id].stack = stack;	\
		sec_debug_last_regs_access[cpu_id].pc = lr;		\
		sec_debug_last_regs_access[cpu_id].time = jiffies;     \
		sec_debug_last_regs_access[cpu_id].status = 2;	\
		}})

#define sec_debug_regs_access_done()	({u32 cpu_id, lr;		\
		if (get_sec_debug_level()) {			\
		asm volatile(						\
			"	mrc	p15, 0, %0, c0, c0, 5\n"	\
			"	ands %0, %0, #0xf\n"			\
			"	mov %1, lr\n"				\
			: "=&r" (cpu_id), "=&r" (lr)			\
			:						\
			: "memory");					\
			\
		sec_debug_last_regs_access[cpu_id].time = jiffies;     \
		sec_debug_last_regs_access[cpu_id].status = 0;	\
		}})
#endif
#endif /* __REGS_DEBUG_H */
