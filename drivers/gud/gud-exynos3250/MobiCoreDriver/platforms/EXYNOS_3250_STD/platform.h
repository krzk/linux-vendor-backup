/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * Header file of MobiCore Driver Kernel Module Platform
 * specific structures
 *
 * Internal structures of the McDrvModule
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 */
#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

#include <mach/irqs.h>

/* MobiCore Interrupt. */
#define MC_INTR_SSIQ                          IRQ_SPI(222)

/* Enable mobicore mem traces */
#define MC_MEM_TRACES

#define COUNT_OF_CPUS 2

/* Values of MPIDR regs in  cpu0, cpu1 */
#define CPU_IDS {0x0000, 0x0001}

#define MC_VM_UNMAP

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD


#define TBASE_CORE_SWITCHER

#endif /* _MC_DRV_PLATFORM_H_ */
