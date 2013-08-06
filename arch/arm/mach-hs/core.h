#ifndef __HISILICON_CORE_H
#define __HISILICON_CORE_H

#include <linux/reboot.h>

extern void hs_set_cpu_jump(int cpu, void *jump_addr);
extern int hs_get_cpu_jump(int cpu);
extern void secondary_startup(void);
extern void hs_map_io(void);
extern struct smp_operations hs_smp_ops;
extern void hs_restart(enum reboot_mode, const char *cmd);

extern void __init hs_hotplug_init(void);
extern void hs_cpu_die(unsigned int cpu);
extern int hs_cpu_kill(unsigned int cpu);
extern void hs_set_cpu(int cpu, bool enable);

#endif
