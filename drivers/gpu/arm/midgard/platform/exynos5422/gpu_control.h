/* drivers/gpu/t6xx/kbase/src/platform/gpu_control.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_control.h
 * DVFS
 */

#ifndef _GPU_CONTROL_H_
#define _GPU_CONTROL_H_
#define CONFIG_MALI_MIDGARD_RT_PM

#include "mali_kbase_config_platform.h"

typedef enum {
	GPU_CONTROL_CLOCK_ON = 0,
	GPU_CONTROL_CLOCK_OFF,
	GPU_CONTROL_CHANGE_CLK_VOL,
	GPU_CONTROL_PREPARE_ON,
	GPU_CONTROL_IS_POWER_ON,
	GPU_CONTROL_SET_MARGIN,
} gpu_control_state;

typedef enum {
	GPU_CONTROL_PM_QOS_INIT = 0,
	GPU_CONTROL_PM_QOS_DEINIT,
	GPU_CONTROL_PM_QOS_SET,
	GPU_CONTROL_PM_QOS_RESET,
} gpu_pmqos_state;

/* GPU NOTIFIER */
#define GPU_THROTTLING_90_95    480
#define GPU_THROTTLING_95_100   266
#define GPU_THROTTLING_100_105  177
#define GPU_THROTTLING_105_110  177
#define GPU_TRIPPING_110        177
#define VOLTAGE_OFFSET_MARGIN   37500
#define RUNTIME_PM_DELAY_TIME   100

#ifdef CONFIG_MALI_MIDGARD_RT_PM
#define RUNTIME_PM_DELAY_TIME 100
#endif /* CONFIG_MALI_MIDGARD_RT_PM */

/* GPU DVFS HANDLER */
#define MALI_DVFS_START_FREQ		266
#define MALI_DVFS_BL_CONFIG_FREQ	266

#ifdef CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ
#define CONFIG_BUS_DEVFREQ
#endif

#define GPU_DVFS_FREQUENCY		100
#ifdef CONFIG_CPU_THERMAL_IPA
#define GPU_DVFS_TIME_INTERVAL	5
#endif /* CONFIG_CPU_THERMAL_IPA */

/* GPU DVFS GOVERNOR */
#define G3D_GOVERNOR_DEFAULT_CLOCK_DEFAULT  266
#define G3D_GOVERNOR_DEFAULT_CLOCK_STATIC   266
#define G3D_GOVERNOR_STATIC_PERIOD          10
#define G3D_GOVERNOR_DEFAULT_CLOCK_BOOSTER  266

/* GPU CONTROL */
#define COLD_MINIMUM_VOL		950000
#define GPU_DEFAULT_VOLTAGE		1037500

#define MALI_T6XX_DEFAULT_CLOCK (MALI_DVFS_START_FREQ*MHZ)

struct exynos_pm_domain *gpu_get_pm_domain(struct kbase_device *kbdev);
int get_cpu_clock_speed(u32 *cpu_clock);
int gpu_is_power_on(void);
int gpu_power_init(struct kbase_device *kbdev);
int gpu_is_clock_on(struct exynos_context *platform);
int gpu_clock_on(struct exynos_context *platform);
int gpu_clock_off(struct exynos_context *platform);
int gpu_set_clock(struct exynos_context *platform, int freq);
int gpu_clock_init(struct kbase_device *kbdev);
int gpu_set_voltage(struct exynos_context *platform, int vol);
int gpu_regulator_enable(struct exynos_context *platform);
int gpu_regulator_disable(struct exynos_context *platform);
int gpu_regulator_init(struct exynos_context *platform);

int gpu_control_state_set(struct kbase_device *kbdev, gpu_control_state state, int param);
int gpu_control_module_init(struct kbase_device *kbdev);
void gpu_control_module_term(struct kbase_device *kbdev);

#endif /* _GPU_CONTROL_H_ */
