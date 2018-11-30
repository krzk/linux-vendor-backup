/*
* Samsung debugging features for Samsung's SoC's.
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifndef SEC_DEBUG_H 
#define SEC_DEBUG_H

#include <linux/module.h>

#ifdef CONFIG_SEC_DEBUG
extern int  sec_debug_setup(void);
extern void sec_debug_reboot_handler(void);
extern void sec_debug_panic_handler(void *buf, bool dump);
extern void sec_debug_post_panic_handler(void);

extern int  sec_debug_get_debug_level(void);
extern void sec_debug_disable_printk_process(void);

/* getlog support */
extern void sec_getlog_supply_kernel(void *klog_buf);
extern void sec_getlog_supply_platform(unsigned char *buffer, const char *name);

extern void sec_gaf_supply_rqinfo(unsigned short curr_offset, unsigned short rq_offset);
#else
#define sec_debug_setup()			(-1)
#define sec_debug_reboot_handler()		do { } while(0)
#define sec_debug_panic_handler(a,b)		do { } while(0)
#define sec_debug_post_panic_handler()		do { } while(0)	

#define sec_debug_get_debug_level()		(0)
#define sec_debug_disable_printk_process()	do { } while(0)

#define sec_getlog_supply_kernel(a)		do { } while(0)
#define sec_getlog_supply_platform(a,b)		do { } while(0)

#define sec_gaf_supply_rqinfo(a,b)		do { } while(0)
#endif /* CONFIG_SEC_DEBUG */


#ifdef CONFIG_SEC_DEBUG_USER_RESET
extern void sec_debug_store_bug_string(const char *fmt, ...);
extern void sec_debug_store_fault_addr(unsigned long addr, struct pt_regs *regs);
extern void sec_debug_store_backtrace(struct pt_regs *regs);
#else
#define sec_debug_store_bug_string(a,...)	do { } while(0)
#define sec_debug_store_fault_addr(a,b)		do { } while(0)
#define sec_debug_store_backtrace(a)		do { } while(0)
#endif /* CONFIG_SEC_DEBUG_USER_RESET */

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
#define SEC_LKMSG_MAGICKEY 0x0000000a6c6c7546
extern void sec_debug_save_last_kmsg(unsigned char *head_ptr, unsigned char *curr_ptr, size_t buf_size);
#else
#define sec_debug_save_last_kmsg(a, b, c)		do { } while (0)
#endif /* CONFIG_SEC_DEBUG_LAST_KMSG */

/*
 * Samsung TN Logging Options 
 */
#ifdef CONFIG_SEC_AVC_LOG
extern void sec_debug_avc_log(char *fmt, ...);
#else
#define sec_debug_avc_log(a,...)		do { } while(0)
#endif /* CONFIG_SEC_AVC_LOG */

/**
 * sec_debug_tsp_log : Leave tsp log in tsp_msg file.
 * ( Timestamp + Tsp logs )
 * sec_debug_tsp_log_msg : Leave tsp log in tsp_msg file and
 * add additional message between timestamp and tsp log.
 * ( Timestamp + additional Message + Tsp logs )
 */
#ifdef CONFIG_SEC_DEBUG_TSP_LOG
extern void sec_debug_tsp_log(char *fmt, ...);
extern void sec_debug_tsp_log_msg(char *msg, char *fmt, ...);
#if defined(CONFIG_TOUCHSCREEN_FTS)
extern void tsp_dump(void);
#elif defined(CONFIG_TOUCHSCREEN_SEC_TS)
extern void tsp_dump_sec(void);
#endif
#else
#define sec_debug_tsp_log(a,...)		do { } while(0)
#define sec_debug_tsp_log_msg(a,b,...)		do { } while(0)
#endif /* CONFIG_SEC_DEBUG_TSP_LOG */

extern int sec_debug_force_error(const char *val, struct kernel_param *kp);

enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT		= 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC	= 0x000000C8,
	UPLOAD_CAUSE_HARDKEY_RESET	= 0x000000CA,
	UPLOAD_CAUSE_FORCED_UPLOAD	= 0x00000022,
	UPLOAD_CAUSE_USER_FORCED_UPLOAD	= 0x00000074,
	UPLOAD_CAUSE_CP_ERROR_FATAL	= 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT		= 0x0000002F,
	UPLOAD_CAUSE_HSIC_DISCONNECTED	= 0x000000DD,
};

#ifdef CONFIG_SEC_DEBUG_RESET_REASON

enum sec_debug_reset_reason_t {
	RR_S = 1,
	RR_W = 2,
	RR_D = 3,
	RR_K = 4,
	RR_M = 5,
	RR_P = 6,
	RR_R = 7,
	RR_B = 8,
	RR_N = 9,
	RR_T = 10,
};

extern unsigned int reset_reason;
#endif

#if defined(CONFIG_SEC_INITCALL_DEBUG)
#define SEC_INITCALL_DEBUG_MIN_TIME	10000

extern void sec_initcall_debug_add(initcall_t fn,
	unsigned long long duration);
#endif

#ifdef CONFIG_SEC_DUMP_SUMMARY

#define SEC_DEBUG_SUMMARY_MAGIC0 0xFFFFFFFF
#define SEC_DEBUG_SUMMARY_MAGIC1 0x5ECDEB6
#define SEC_DEBUG_SUMMARY_MAGIC2 0x14F014F0
#define SEC_DEBUG_SUMMARY_MAGIC3 0x00010004

#define DUMP_SUMMARY_MAX_SIZE	(0x300000)
#define SCHED_LOG_MAX 512

struct sec_debug_ksyms {
	uint32_t magic;
	uint32_t kallsyms_all;
	uint64_t addresses_pa;
	uint64_t names_pa;
	uint64_t num_syms;
	uint64_t token_table_pa;
	uint64_t token_index_pa;
	uint64_t markers_pa;
	struct ksect {
		uint64_t sinittext;
		uint64_t einittext;
		uint64_t stext;
		uint64_t etext;
		uint64_t end;
	} sect;
	uint64_t relative_base;
	uint64_t offsets_pa;
};

struct irq_log {
	unsigned long long time;
	int irq;
	void *fn;
	int en;
	int preempt_count;
	void *context;
};

struct irq_exit_log {
	unsigned int irq;
	unsigned long long time;
	unsigned long long end_time;
	unsigned long long elapsed_time;
};

struct sched_log {
	unsigned long long time;
	char comm[TASK_COMM_LEN];
	pid_t pid;
	struct task_struct *pTask;
};

struct sec_debug_summary_corelog {
	atomic_t idx_irq[CONFIG_NR_CPUS];
	struct irq_log irq[CONFIG_NR_CPUS][SCHED_LOG_MAX];
	atomic_t idx_irq_exit[CONFIG_NR_CPUS];
	struct irq_exit_log irq_exit[CONFIG_NR_CPUS][SCHED_LOG_MAX];
	atomic_t idx_sched[CONFIG_NR_CPUS];
	struct sched_log sched[CONFIG_NR_CPUS][SCHED_LOG_MAX];
};

struct sec_debug_summary_sched_log {
	uint64_t sched_idx_paddr;
	uint64_t sched_buf_paddr;
	unsigned int sched_struct_sz;
	unsigned int sched_array_cnt;
	uint64_t irq_idx_paddr;
	uint64_t irq_buf_paddr;
	unsigned int irq_struct_sz;
	unsigned int irq_array_cnt;
	uint64_t irq_exit_idx_paddr;
	uint64_t irq_exit_buf_paddr;
	unsigned int irq_exit_struct_sz;
	unsigned int irq_exit_array_cnt;
};

struct sec_debug_summary_kernel_log {
	uint64_t first_idx_paddr;
	uint64_t next_idx_paddr;
	uint64_t log_paddr;
	uint64_t size_paddr;
};

struct sec_debug_summary_excp_kernel {
	char pc_sym[64];
	char lr_sym[64];
	char panic_caller[64];
	char panic_msg[128];
	char thread[32];
};

struct sec_debug_summary_cpu_info {
	char policy_name[16];
	int freq_min;
	int freq_max;
	int freq_cur;
};

struct sec_debug_summary_data_kernel {
	char name[16];
	char state[16];
	int nr_cpus;

	struct sec_debug_summary_excp_kernel excp;
	struct sec_debug_summary_cpu_info cpu_info[CONFIG_NR_CPUS];
};

struct sec_debug_summary {
	unsigned int magic[4];

	unsigned long reserved_out_buf;
	unsigned long reserved_out_size;

	char summary_cmdline[2048];
	char summary_linuxbanner[1024];

	struct sec_debug_summary_data_kernel kernel;
	struct sec_debug_summary_corelog sched_log;
	struct sec_debug_ksyms ksyms;
};

extern void sec_debug_task_sched_log_short_msg(char *msg);
extern void sec_debug_task_sched_log(int cpu, struct task_struct *task);
extern void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en);
extern void sec_debug_irq_enterexit_log(unsigned int irq,
						unsigned long long start_time);
extern void sec_debug_set_kallsyms_info(struct sec_debug_ksyms *ksyms, int magic);

int sec_debug_save_cpu_info(void);
int sec_debug_save_die_info(const char *str, struct pt_regs *regs);
int sec_debug_save_panic_info(const char *str, unsigned long caller);
#endif

#endif /* SEC_DEBUG_H */
