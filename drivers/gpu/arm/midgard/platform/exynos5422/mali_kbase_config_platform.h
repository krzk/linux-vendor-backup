/*
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mali_kbase_config.h>

#define GPU_FREQ_KHZ_MAX    600000
#define GPU_FREQ_KHZ_MIN    177000

extern int get_cpu_clock_speed(u32 *cpu_clock);
extern struct kbase_platform_funcs_conf platform_funcs;
extern struct kbase_pm_callback_conf pm_callbacks;

#define CPU_SPEED_FUNC (&get_cpu_clock_speed)
#define GPU_SPEED_FUNC (NULL)
#define PLATFORM_FUNCS (&platform_funcs)
#define POWER_MANAGEMENT_CALLBACKS (&pm_callbacks)
#define CONFIG_MALI_MIDGARD_DEBUG_SYS
