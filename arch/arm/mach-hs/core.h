#ifndef __HISILICON_CORE_H
#define __HISILICON_CORE_H

#include <linux/reboot.h>

extern void hs_set_cpu_jump(int cpu, void *jump_addr);
extern int hs_get_cpu_jump(int cpu);
extern void secondary_startup(void);
extern void hs_map_io(void);
extern struct smp_operations hs_smp_ops;
extern void hs_restart(enum reboot_mode, const char *cmd);

#endif
