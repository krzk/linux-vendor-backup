
/*
 * debugfs file to keep track of suspend
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the impliesd warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Sangin Lee <sangin78.lee@samsung.com>       Initial Release           *
 *       Sanghyeon Lee <sirano06.lee@samsung.com>                              *
 *       Junyoun Kim <junyouns.kim@samsung.com>                                *
 * ---- -------------------------------------------- ------------------------- *
 * 2.0   Hunsup Jung <hunsup.jung@samsung.com>       Remove unnecessary code   *
 *                                                   Add info - battery type   *
 *                                                            - battery temp   *
 *                                                   Remove info - status      *
 * ---- -------------------------------------------- ------------------------- *
 * 3.0   Junho Jang <vincent.jang@samsung.com>       Remove unnecessary code   *
 *                                                   Add info - sap activity, traffic   *
 *                                                   - wakeup source   *
 *                                                   - cpu time 	 *
 *                                                   - cpu frequency	 *
 *                                                   - cpu idle	 *
 *                                                   - gpu frequency	 *
 *                                                   - ff 	 *
 *                                                   - exynos sleep 	 *
 *                                                   - sensorhub event 	 *
 *                                                   - move model specific info. to DT	 *
 *                                                   - hw suspend energy estimator	 *
 *                                                   - alarm	 *
 *                                                   - netstat	 *
 * ---- -------------------------------------------- ------------------------- *
 */


#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/security.h>

#include <linux/power/energy_monitor.h>
#include <linux/battery/sec_battery.h>
#include <linux/time_history.h>
#include <linux/power/disp_stat.h>

#ifdef CONFIG_SENSORHUB_STAT
#include <linux/sensorhub_stat.h>
#endif
#ifdef CONFIG_ALARM_HISTORY
#include "../staging/android/alarm_history.h"
#endif
#ifdef CONFIG_SAP_PID_STAT
#include <linux/sap_pid_stat.h>
#endif
#ifdef CONFIG_PID_STAT
#include <linux/pid_stat.h>
#endif
#ifdef CONFIG_NET_STAT_TIZEN
#include <linux/net_stat_tizen.h>
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
#include <linux/power/slave_wakelock.h>
#endif
#ifdef CONFIG_SID_SYS_STATS
#include <linux/sid_sys_stats.h>
#endif
#ifdef CONFIG_ARM64_EXYNOS_CPUIDLE_STAT
#include <linux/cpuidle_stat.h>
#endif
#ifdef CONFIG_CPU_FREQ_STAT_TIZEN
#include <linux/cpufreq_stats_tizen.h>
#endif
#ifdef CONFIG_GPUFREQ_STAT
#include <linux/gpufreq_stat.h>
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
#include <linux/platform_data/sec_thermistor_history.h>
#endif
#ifdef CONFIG_FF_STAT_TIZEN
#include <linux/ff_stat_tizen.h>
#endif
#ifdef CONFIG_SLEEP_STAT_EXYNOS
#include <linux/sleep_stat_exynos.h>
#endif
#ifdef CONFIG_LBS_HISTORY
#include <linux/lbs_history.h>
#endif
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
static struct device *sec_energy_monitor;
#endif

#define ENERGY_MON_VERSION "3.0"

#define ENERGY_MON_HISTORY_NUM 64
#define ENERGY_MONITOR_MAX_MONITOR_INTERVAL 86400 /* second */

// TODO: Need to seperate model specific information
#if defined(CONFIG_MACH_VOLT)
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#elif defined(CONFIG_SOLIS)
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#elif defined(CONFIG_POP)
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#elif defined(CONFIG_GALILEO_LARGE)
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#elif defined(CONFIG_GALILEO_SMALL)
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#else
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#endif

#define DEFAULT_UNIT_BATTERY_mAs (300*3600/100) /* 300mAh */

#ifdef CONFIG_ALARM_HISTORY
#define ALARM_STAT_ARRAY_SIZE 4
#endif
#ifdef CONFIG_SAP_PID_STAT
#define SAP_TRAFFIC_ARRAY_SIZE 4
#endif
#ifdef CONFIG_PID_STAT
#define TCP_TRAFFIC_ARRAY_SIZE 4
#endif
#ifdef CONFIG_NET_STAT_TIZEN
#define NET_STAT_ARRAY_SIZE 6
#endif
#ifdef CONFIG_PM_SLEEP
#define WS_ARRAY_SIZE 6
#endif
#ifdef CONFIG_SID_SYS_STATS
#define SID_ARRAY_SIZE 6
#endif
#ifdef CONFIG_ARM64_EXYNOS_CPUIDLE_STAT
#define BLOCKER_ARRAY_SIZE 4
#define BLOCKER_NAME_SIZE 9
#endif
#ifdef CONFIG_LBS_HISTORY
#define LBS_STAT_ARRAY_SIZE 4
#endif

/* max time supported by wakeup_stat */
#define ENERGY_MON_MAX_WAKEUP_STAT_TIME 16

/* sleep current estimator related constants */
#define ENERGY_MON_MAX_SLEEP_ESTIMATOR_CNT 20
#define MIN_SLEEP_TIME_S 500 /* second */

/* penalty score value */
#define ENERGY_MON_SCORE_VERSION "1.0"
#define ENERGY_MON_SCORE_CPU_TIME	BIT(15)
#define ENERGY_MON_SCORE_CPU_IDLE	BIT(14)
#define ENERGY_MON_SCORE_SOC_LPM	BIT(13)
#define ENERGY_MON_SCORE_CPU_CLK	BIT(12)
#define ENERGY_MON_SCORE_GPU_CLK	BIT(11)
#define ENERGY_MON_SCORE_TCP	BIT(10)
#define ENERGY_MON_SCORE_SAP	BIT(9)
#define ENERGY_MON_SCORE_SH	BIT(8)
#define ENERGY_MON_SCORE_DISP	BIT(7)
#define ENERGY_MON_SCORE_LBS	BIT(6)
#define ENERGY_MON_SCORE_GPS	BIT(5)
#define ENERGY_MON_SCORE_SLAVE_WAKELOCK	BIT(4)
#define ENERGY_MON_SCORE_WS	BIT(3)
#define ENERGY_MON_SCORE_SUSPEND	BIT(2)
#define ENERGY_MON_SCORE_SOC_SLEEP	BIT(1)
#define ENERGY_MON_SCORE_FAST_DRAIN	BIT(0)
#define ENERGY_MON_SCORE_MAX (ENERGY_MON_SCORE_FAST_DRAIN |\
	ENERGY_MON_SCORE_SOC_SLEEP |\
	ENERGY_MON_SCORE_SUSPEND |\
	ENERGY_MON_SCORE_WS |\
	ENERGY_MON_SCORE_SLAVE_WAKELOCK |\
	ENERGY_MON_SCORE_GPS |\
	ENERGY_MON_SCORE_LBS |\
	ENERGY_MON_SCORE_CPU_TIME |\
	ENERGY_MON_SCORE_CPU_IDLE |\
	ENERGY_MON_SCORE_SOC_LPM |\
	ENERGY_MON_SCORE_CPU_CLK |\
	ENERGY_MON_SCORE_GPU_CLK |\
	ENERGY_MON_SCORE_TCP |\
	ENERGY_MON_SCORE_SAP |\
	ENERGY_MON_SCORE_SH |\
	ENERGY_MON_SCORE_DISP)

#define energy_mon_dbg(debug_level_mask, fmt, ...)\
do {\
	if (debug_level & debug_level_mask)\
		pr_info("energy_mon_%d: " fmt, debug_level_mask, ##__VA_ARGS__);\
} while (0)

enum ENERGY_MON_DEBUG_LEVEL {
	ENERGY_MON_DEBUG_INFO = BIT(0),
	ENERGY_MON_DEBUG_ERR = BIT(1),
	ENERGY_MON_DEBUG_WARN = BIT(2),
	ENERGY_MON_DEBUG_DBG = BIT(3),
	ENERGY_MON_DEBUG_WAKEUP_STAT = BIT(4),
	ENERGY_MON_DEBUG_SLEEP_ESTIMATOR = BIT(5),
};

enum energy_mon_print_step {
	STEP_SUMMARY = 0,
	STEP_DISP_STAT,
#ifdef CONFIG_SENSORHUB_STAT
	STEP_SENSORHUB_WAKEUP,
	STEP_SENSORHUB_ACTIVITY,
#endif
#ifdef CONFIG_ALARM_HISTORY
	STEP_ALARM_STAT,
#endif
#ifdef CONFIG_SAP_PID_STAT
	STEP_SAP_WAKEUP,
	STEP_SAP_ACTIVITY,
	STEP_SAP_TRAFFIC,
#endif
#ifdef CONFIG_PID_STAT
	STEP_PID_STAT,
#endif
#ifdef CONFIG_NET_STAT_TIZEN
	STEP_NET_STAT1,
	STEP_NET_STAT2,
	STEP_NET_STAT3,
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
	STEP_SLAVE_WAKELOCK,
#endif
#ifdef CONFIG_PM_SLEEP
	STEP_WAKEUP_SOURCE,
#endif
#ifdef CONFIG_SID_SYS_STATS
	STEP_SID_SYS_STATS,
#endif
#ifdef CONFIG_ARM64_EXYNOS_CPUIDLE_STAT
	STEP_CPUIDLE_STAT,
	STEP_CPUIDLE_LPM_BLOCKER,
#endif
#ifdef CONFIG_CPU_FREQ_STAT_TIZEN
	STEP_CPUFREQ_STATS,
#endif
#ifdef CONFIG_GPUFREQ_STAT
	STEP_GPUFREQ_STAT,
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	STEP_SEC_THERM_HISTORY,
#endif
#ifdef CONFIG_FF_STAT_TIZEN
	STEP_FF_STAT,
#endif
#ifdef CONFIG_SLEEP_STAT_EXYNOS
	STEP_SLEEP_STAT,
#endif
#ifdef CONFIG_SENSORHUB_STAT
	STEP_GPS_STAT,
	STEP_HR_STAT,
#endif
#ifdef CONFIG_LBS_HISTORY
	STEP_LBS_HISTORY,
#endif
	STEP_CE,
	STEP_MAX
};

enum energy_mon_print_type {
	ENERGY_MON_PRINT_TITLE = 0,
	ENERGY_MON_PRINT_MAIN,
	ENERGY_MON_PRINT_TAIL,
};

struct consumed_energy {
	unsigned int sleep;
	unsigned int disp_aod;
	unsigned int disp_on;
	unsigned int cpu_idle;
	unsigned int cpu_active;
	unsigned int gpu_active;
	unsigned int ff;
	unsigned int gps;
	unsigned int lbs;
	unsigned int hr;
	unsigned int bt;
	unsigned int wifi;
};

struct energy_mon_data {
	int type;
	int log_count;
	int suspend_count;
	int bat_status;
	int bat_capacity;
	int cable_type;
	int bat_temp;
#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
	int current_suspend; /* mAh */
#endif
	struct timespec ts_real;
	struct timespec ts_boot;
	struct timespec ts_kern;
	struct timespec ts_disp;
	struct timespec ts_aod;
	int wakeup_cause[ENERGY_MON_WAKEUP_MAX];
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
	ktime_t wakeup_time[ENERGY_MON_WAKEUP_MAX];
#endif
	struct disp_stat_emon disp_stat;
#ifdef CONFIG_SENSORHUB_STAT
	struct sensorhub_stat_info sh_stat;
#endif
#ifdef CONFIG_ALARM_HISTORY
	struct alarm_expire_stat alarm_stat[ALARM_STAT_ARRAY_SIZE];
#endif
#ifdef CONFIG_SAP_PID_STAT
	struct sap_pid_wakeup_stat sap_wakeup;
	struct sap_stat_traffic sap_traffic[SAP_TRAFFIC_ARRAY_SIZE];
#endif
#ifdef CONFIG_PID_STAT
	struct pid_stat_traffic tcp_traffic[TCP_TRAFFIC_ARRAY_SIZE];
#endif
#ifdef CONFIG_NET_STAT_TIZEN
	struct net_stat_tizen_emon net_stat[NET_STAT_ARRAY_SIZE];
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
	struct slp_mon_slave_wakelocks slwl[SLWL_ARRAY_SIZE];
#endif
#ifdef CONFIG_PM_SLEEP
	struct emon_wakeup_source ws[WS_ARRAY_SIZE];
#endif
#ifdef CONFIG_SID_SYS_STATS
	struct sid_sys_stats sid[SID_ARRAY_SIZE];
#endif
#ifdef CONFIG_ARM64_EXYNOS_CPUIDLE_STAT
	struct emon_cpuidle_stat cpuidle_stat;
	struct emon_lpm_blocker lpm_blocker[BLOCKER_ARRAY_SIZE];
#endif
#ifdef CONFIG_CPU_FREQ_STAT_TIZEN
	struct cpufreq_stats_tizen cpufreq_stats;
#endif
#ifdef CONFIG_GPUFREQ_STAT
	struct gpufreq_stat gpufreq_stat;
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	struct sec_therm_history_info therm_history[MAX_SEC_THERM_DEVICE_NUM];
#endif
#ifdef CONFIG_FF_STAT_TIZEN
	struct ff_stat_emon ff_stat;
#endif
#ifdef CONFIG_SLEEP_STAT_EXYNOS
	struct sleep_stat_exynos sleep_stat;
#endif
#ifdef CONFIG_SENSORHUB_STAT
	struct sensorhub_gps_stat sh_gps;
	struct sensorhub_hr_stat sh_hr;
#endif
#ifdef CONFIG_LBS_HISTORY
	struct lbs_stat_emon lbs_stat[LBS_STAT_ARRAY_SIZE];
#endif

	struct consumed_energy ce;
	unsigned int penalty_score;
};

struct energy_mon_penalty {
	unsigned int score;
	int threshold_time;
	int threshold_batt;
	int threshold_disp;
	int threshold_gps;
	int threshold_lbs;
	int threshold_cpuidle;
	int threshold_cpufreq;
	int threshold_gpufreq;
	int threshold_ws;
	int threshold_slwl;
	int threshold_sid;
	ktime_t last_threshold_change;
};

struct energy_mon_irq_map_table {
	const char *irq_name;
	int wakeup_idx;
};

struct fast_batt_drain_cause {
	unsigned int cpu_sid;
	unsigned int lbs_sid;
	char slwl_name[SLWL_NAME_LENGTH];
	char ws_name[WS_NAME_SIZE];
	unsigned int soc_sleep;
};

struct disp_power_value {
	unsigned int brightness;
	unsigned int value;
};

struct active_power_value {
	unsigned int speed;
	unsigned int value;
};

struct energy_mon_power_value {
	unsigned int disp_aod;
	unsigned int disp_on_table_size;
	struct disp_power_value *disp_on;
	unsigned int cpu_idle;
	unsigned int cpu_lpm;
	unsigned int cpu_awake;
	unsigned int cpu_active_table_size;
	struct active_power_value *cpu_active;
	unsigned int cpu_whitelist_table_size;
	unsigned int *cpu_whitelist;
	unsigned int gpu_active_table_size;
	struct active_power_value *gpu_active;
	unsigned int bt_active;
	unsigned int bt_tx;
	unsigned int bt_rx;
	unsigned int wifi_scan;
	unsigned int ff;
	unsigned int gps;
	unsigned int hr;
};

struct energy_monitor_cb {
	int running;
	int data_index;
	int read_index;

	int charging_count;
	int discharging_count;

	int wakeup_cause[ENERGY_MON_WAKEUP_MAX];
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
	int last_wakeup_idx;
	ktime_t last_wakeup_time;
	ktime_t wakeup_time[ENERGY_MON_WAKEUP_MAX];
	ktime_t prev_wakeup_time[ENERGY_MON_WAKEUP_MAX]; /* should be same size as wakeup_time */
	unsigned int wakeup_time_stats[ENERGY_MON_WAKEUP_MAX][ENERGY_MON_MAX_WAKEUP_STAT_TIME + 1];
#endif
#ifdef CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
	int estimator_index;
	long long estimator_average[ENERGY_MON_MAX_SLEEP_ESTIMATOR_CNT];
#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
	int current_suspend_cnt;
	int current_suspend;
#endif
#endif
	int disp_state;
	struct timespec disp_total;
	struct timespec disp_last_on;

	struct energy_mon_data boot;
	struct energy_mon_data data[ENERGY_MON_HISTORY_NUM];
	struct energy_mon_data dump;

	struct energy_mon_data charging;
	struct energy_mon_data discharging;

	struct energy_mon_data charging_dump;
	struct energy_mon_data discharging_dump;

	struct fast_batt_drain_cause fbdc;

	/* get the following info from device tree */
	long long unit_bat_capacity;
	int use_raw_soc;
	const char *ps_raw_soc;
	const char *ps_hw_suspend_energy;
	struct energy_mon_penalty penalty;
	int irq_map_table_size;
	struct energy_mon_irq_map_table *irq_map_table;
	struct energy_mon_power_value power_value;
};

/* global variable */
static struct energy_monitor_cb energy_mon;

static int energy_monitor_enable = 1;
static unsigned int monitor_interval = 120; /* 0: Disable, 1~86400: Enable */
static unsigned int logging_interval = 3590; /* 0: Disable, 1~86400: Enable */
static struct delayed_work monitor_work;

static int debug_level = ENERGY_MON_DEBUG_INFO |
					ENERGY_MON_DEBUG_ERR |
					ENERGY_MON_DEBUG_WARN |
					ENERGY_MON_DEBUG_SLEEP_ESTIMATOR |
					ENERGY_MON_DEBUG_WAKEUP_STAT;

static void energy_mon_get_time_info(struct energy_mon_data *p_curr)
{
	if (!p_curr) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: pointer is NULL\n", __func__);
		return;
	}

	p_curr->ts_real = current_kernel_time();
	get_monotonic_boottime(&p_curr->ts_boot);
	p_curr->ts_kern = ktime_to_timespec(ktime_get());
	p_curr->ts_disp = disp_stat_get_fb_total_time();
	p_curr->ts_aod = disp_stat_get_aod_total_time();
	p_curr->suspend_count = suspend_stats.success;

	/* Debug logs */
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s\n", __func__);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_real: %15lu.%09lu\n", p_curr->ts_real.tv_sec,
																   p_curr->ts_real.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_boot: %15lu.%09lu\n", p_curr->ts_boot.tv_sec,
																   p_curr->ts_boot.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_kern: %15lu.%09lu\n", p_curr->ts_kern.tv_sec,
																   p_curr->ts_kern.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_disp: %15lu.%09lu\n", p_curr->ts_disp.tv_sec,
																   p_curr->ts_disp.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_aod: %15lu.%09lu\n", p_curr->ts_aod.tv_sec,
																  p_curr->ts_aod.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "suspend_count=%04d\n", p_curr->suspend_count);
}

static void energy_mon_get_battery_info(struct energy_mon_data *p_curr)
{
	struct power_supply *psy = NULL;
	union power_supply_propval value;
	enum power_supply_property props[] = {
		POWER_SUPPLY_PROP_STATUS,
		POWER_SUPPLY_PROP_ONLINE,
		POWER_SUPPLY_PROP_TEMP,
		POWER_SUPPLY_PROP_CAPACITY
	};
	int i, err;

	if (!p_curr) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: pointer is NULL\n", __func__);
		return;
	}

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: cannot find battery power supply\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(props); i++) {
		if (power_supply_get_property(psy, props[i], &value))
			continue;

		switch (props[i]) {
		case POWER_SUPPLY_PROP_STATUS:
			p_curr->bat_status = value.intval;
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			p_curr->cable_type = value.intval;
			break;
		case POWER_SUPPLY_PROP_TEMP:
			p_curr->bat_temp = value.intval;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			p_curr->bat_capacity = value.intval * 100;
			break;
		default:
			break;
		}
	}
	power_supply_put(psy);

	if (energy_mon.use_raw_soc) {
		psy = power_supply_get_by_name(energy_mon.ps_raw_soc);
		if (psy) {
			value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
			err = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CAPACITY, &value);
			p_curr->bat_capacity = value.intval;
			power_supply_put(psy);
		} else
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s: cannot find fg power supply\n", __func__);
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "bat_stat=%d, bat_capa=%03d\n",
		p_curr->bat_status,
		p_curr->bat_capacity);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "cable_type=%d, bat_temp=%03d\n",
		p_curr->cable_type,
		p_curr->bat_temp);

#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
		p_curr->current_suspend = energy_mon.current_suspend;
		energy_mon.current_suspend_cnt = 0;
		energy_mon.current_suspend = 0;
#endif
}

#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
static void energy_mon_get_wakeup_stat(
	struct energy_mon_data *p_curr, int backup_stat)
{
	int i;

	if (!p_curr) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: p_curr is NULL\n", __func__);
		return;
	}

	for (i = 0; i < ENERGY_MON_WAKEUP_MAX; i++) {
		p_curr->wakeup_time[i] =
			ktime_sub(energy_mon.wakeup_time[i],
						energy_mon.prev_wakeup_time[i]);
	}

	if (backup_stat)
		memcpy(energy_mon.prev_wakeup_time,
				energy_mon.wakeup_time,
				sizeof(energy_mon.prev_wakeup_time));
}
#endif

static int calculate_permil(s64 residency, s64 profile_time)
{
	if (!residency)
		return 0;

	residency *= 1000;
	do_div(residency, profile_time);

	return (int)residency;
}

static unsigned int energy_monitor_calc_penalty_battery(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	int diff_soc;
	long diff_boot = 0;
	long long average;
	int aod_offset = 0;

	if (skip_penalty_check)
		return score;

	diff_soc = p_curr->bat_capacity - p_prev->bat_capacity;
	diff_boot = p_curr->ts_boot.tv_sec - p_prev->ts_boot.tv_sec;

	average = abs(energy_mon.unit_bat_capacity *
		(p_curr->bat_capacity - p_prev->bat_capacity));
	do_div(average, diff_boot);

	/*
	 * penalty_score scheme
	 *	 - notify fast battery drain when average current above threshold_batt + aod_offset
	 */
	if (diff_soc < 0) {
		if (p_curr->ce.disp_aod > 0)
			aod_offset = p_curr->ce.disp_aod;

		if (average * 10 >= energy_mon.penalty.threshold_batt + aod_offset)
			score = ENERGY_MON_SCORE_FAST_DRAIN;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%d: %lld %d %d\n", __func__,
		p_curr->log_count , score, average,
		energy_mon.penalty.threshold_batt, aod_offset);

	return score;
}

#ifdef CONFIG_DISP_STAT
static unsigned int energy_monitor_calc_penalty_disp(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int residence_time;
	unsigned int uAh = 0, aod_uAh;
	int i;

	if (energy_mon.power_value.disp_on_table_size > 0) {
		for (i = 0; i < energy_mon.power_value.disp_on_table_size; i++) {
			power_value = energy_mon.power_value.disp_on[i].value;
			residence_time = ktime_to_ms(p_curr->disp_stat.fb_time[i]);

			uAh += power_value * residence_time / 3600000;
		}

		p_curr->ce.disp_on = uAh;

		if (uAh > energy_mon.penalty.threshold_disp)
			score = ENERGY_MON_SCORE_DISP;
	}

	power_value = energy_mon.power_value.disp_aod;
	residence_time = ktime_to_ms(p_curr->disp_stat.aod_time);

	aod_uAh = power_value * residence_time / 3600;

	p_curr->ce.disp_aod = aod_uAh;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %u %u\n", __func__,
		p_curr->log_count, score, uAh, aod_uAh);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_disp(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_SENSORHUB_STAT
static unsigned int energy_monitor_calc_penalty_sh(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;

	if (skip_penalty_check)
		return score;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x\n", __func__, p_curr->log_count, score);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_sh(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_ALARM_HISTORY
static unsigned int energy_monitor_calc_penalty_alarm(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;

	if (skip_penalty_check)
		return score;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x\n", __func__, p_curr->log_count, score);

	return 0;
}
#else
static inline unsigned int energy_monitor_calc_penalty_alarm(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_SAP_PID_STAT
static unsigned int energy_monitor_calc_penalty_sap(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;

	if (skip_penalty_check)
		return score;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x\n", __func__, p_curr->log_count, score);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_sap(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_PID_STAT
static unsigned int energy_monitor_calc_penalty_tcp(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;

	if (skip_penalty_check)
		return score;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x\n", __func__, p_curr->log_count, score);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_tcp(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_PM_SLEEP
static unsigned int energy_monitor_calc_penalty_wakeup_sources(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	long diff_boot;
	long ws_time;
	long long permil;

	if (skip_penalty_check)
		return score;

	diff_boot = p_curr->ts_boot.tv_sec - p_prev->ts_boot.tv_sec;
	ws_time = ktime_to_timeval(p_curr->ws[0].emon_total_time).tv_sec;

	permil = ws_time * 1000;
	do_div(permil, diff_boot);

	if (permil > energy_mon.penalty.threshold_ws) {
		score = ENERGY_MON_SCORE_WS;
		memset(energy_mon.fbdc.ws_name, 0, WS_NAME_SIZE);
		memcpy(energy_mon.fbdc.ws_name,
				p_curr->ws[0].name, WS_NAME_SIZE - 1);
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %lld %d\n", __func__,
		p_curr->log_count, score, permil, energy_mon.penalty.threshold_ws);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_wakeup_sources(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_SLAVE_WAKELOCK
static unsigned int energy_monitor_calc_penalty_slave_wakelock(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	long diff_boot;
	long slwl_time;
	long long permil;

	if (skip_penalty_check)
		return score;

	diff_boot = p_curr->ts_boot.tv_sec - p_prev->ts_boot.tv_sec;
	slwl_time = ktime_to_timeval(p_curr->slwl[0].prevent_time).tv_sec;

	permil = slwl_time * 1000;
	do_div(permil, diff_boot);

	if (permil > energy_mon.penalty.threshold_slwl) {
		score = ENERGY_MON_SCORE_SLAVE_WAKELOCK;
		memset(energy_mon.fbdc.slwl_name, 0, SLWL_NAME_LENGTH);
		memcpy(energy_mon.fbdc.slwl_name,
				p_curr->slwl[0].slwl_name, SLWL_NAME_LENGTH - 1);
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %lld %d\n", __func__,
		p_curr->log_count, score, permil, energy_mon.penalty.threshold_slwl);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_slave_wakelock(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_ARM64_EXYNOS_CPUIDLE_STAT
static unsigned int energy_monitor_calc_penalty_exynos_cpuidle(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	int permil;
	ktime_t active;
	ktime_t idle;
	ktime_t lpm;
	ktime_t zero = ktime_set(0,0);

	if (skip_penalty_check)
		return score;

	active = p_curr->cpuidle_stat.total_stat_time;
	idle = p_curr->cpuidle_stat.cpuidle[0].total_idle_time;
	permil = calculate_permil(ktime_to_ms(idle), ktime_to_ms(active));
	if (permil <= energy_mon.penalty.threshold_cpuidle)
		score = ENERGY_MON_SCORE_CPU_IDLE;

	lpm = p_curr->cpuidle_stat.lpm.usage.time;
	if (ktime_compare(lpm, zero) == 0)
		score |= ENERGY_MON_SCORE_SOC_LPM;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %d %d\n", __func__,
		p_curr->log_count, score, permil, energy_mon.penalty.threshold_cpuidle);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_exynos_cpuidle(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_CPU_FREQ_STAT_TIZEN
static unsigned int energy_monitor_get_cpu_active_power_value(
	unsigned int clk)
{
	int i;

 	if (energy_mon.power_value.cpu_active_table_size > 0) {
		for (i = 0; i < energy_mon.power_value.cpu_active_table_size; i++) {
			if (energy_mon.power_value.cpu_active[i].speed == clk)
				return energy_mon.power_value.cpu_active[i].value;
		}
	}

	return 0;
}

static unsigned int energy_monitor_calc_penalty_cpu_clock(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int residence_time;
	unsigned int uAh = 0;
	int i;

	for (i = 0; i < p_curr->cpufreq_stats.state_num; i++) {
		power_value = energy_monitor_get_cpu_active_power_value
						(p_curr->cpufreq_stats.freq_table[i]);
		residence_time = jiffies_to_msecs(p_curr->cpufreq_stats.time_in_state[i]);

		uAh += power_value * residence_time / 3600;
	}
	p_curr->ce.cpu_active = uAh;

	if (uAh > energy_mon.penalty.threshold_cpufreq)
		score = ENERGY_MON_SCORE_CPU_CLK;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %u\n", __func__, p_curr->log_count, score, uAh);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_cpu_clock(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_GPUFREQ_STAT
static unsigned int energy_monitor_get_gpu_active_power_value(
	unsigned int clk)
{
	int i;

 	if (energy_mon.power_value.gpu_active_table_size > 0) {
		for (i = 0; i < energy_mon.power_value.gpu_active_table_size; i++) {
			if (energy_mon.power_value.gpu_active[i].speed == clk)
				return energy_mon.power_value.gpu_active[i].value;
		}
	}

	return 0;
}

static unsigned int energy_monitor_calc_penalty_gpu_clock(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int residence_time;
	unsigned int uAh = 0;
	int i;

	for (i = 0; i < p_curr->gpufreq_stat.table_size; i++) {
		power_value = energy_monitor_get_gpu_active_power_value
						(p_curr->gpufreq_stat.table[i].clock);
		residence_time = jiffies_to_msecs(p_curr->gpufreq_stat.table[i].time);

		uAh += power_value * residence_time / 3600;
	}
	p_curr->ce.gpu_active = uAh;

	if (uAh > energy_mon.penalty.threshold_gpufreq)
		score = ENERGY_MON_SCORE_GPU_CLK;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %u\n", __func__, p_curr->log_count, score, uAh);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_gpu_clock(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_SLEEP_STAT_EXYNOS
static unsigned int energy_monitor_calc_penalty_exynos_sleep(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;

	if (skip_penalty_check)
		return score;

	if (p_curr->sleep_stat.acpm_sleep_mif_down == 0 ||
		p_curr->sleep_stat.acpm_sleep_soc_down == 0 ||
		p_curr->sleep_stat.acpm_sicd_mif_down == 0 ||
		p_curr->sleep_stat.acpm_sicd_soc_down == 0)
		score = ENERGY_MON_SCORE_SOC_SLEEP;

	if (p_curr->sleep_stat.fail > p_curr->sleep_stat.suspend_success)
		score |= ENERGY_MON_SCORE_SUSPEND;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x\n", __func__, p_curr->log_count, score);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_exynos_sleep(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_SENSORHUB_STAT
static unsigned int energy_monitor_calc_penalty_ff(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int residence_time;
	unsigned int uAh = 0;

	power_value = energy_mon.power_value.ff;
	residence_time = ktime_to_ms(p_curr->ff_stat.total_time);
	uAh = power_value * residence_time / 3600;
	p_curr->ce.ff = uAh;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %u\n", __func__, p_curr->log_count, score, uAh);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_ff(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_SENSORHUB_STAT
static unsigned int energy_monitor_calc_penalty_gps(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int residence_time = 0;
	unsigned int uAh = 0;
	long diff_boot;
	long gps_time;
	int permil;

	power_value = energy_mon.power_value.gps;

	residence_time = p_curr->sh_gps.gps_time;
	if (residence_time > 3600)
		residence_time = 3600;
	uAh = power_value * residence_time * 1000 / 3600;
	p_curr->ce.gps = uAh;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: %u\n", __func__, uAh);

	if (skip_penalty_check)
		return score;

	diff_boot = p_curr->ts_boot.tv_sec - p_prev->ts_boot.tv_sec;
	gps_time = p_curr->sh_gps.gps_time;

	permil = calculate_permil(gps_time, diff_boot);
	if (permil > energy_mon.penalty.threshold_gps)
		score = ENERGY_MON_SCORE_GPS;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %d %d\n", __func__,
		p_curr->log_count, score, permil, energy_mon.penalty.threshold_gps);

	return score;
}

static unsigned int energy_monitor_calc_penalty_hr(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int residence_time = 0;
	unsigned int uAh = 0;

	power_value = energy_mon.power_value.hr;

	residence_time = p_curr->sh_hr.success_dur + p_curr->sh_hr.fail_dur;
	if (residence_time > 3600)
		residence_time = 3600;
	uAh = power_value * residence_time * 1000 / 3600;
	p_curr->ce.hr = uAh;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: %u\n", __func__, p_curr->log_count, uAh);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_gps(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}

static inline unsigned int energy_monitor_calc_penalty_hr(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_LBS_HISTORY
static unsigned int energy_monitor_calc_penalty_lbs(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int uAh = 0;
	ktime_t gps_time;
	s64 total_gps_time_ms = 0;
	int i;

	power_value = energy_mon.power_value.gps;

	for (i = 0; i < LBS_STAT_ARRAY_SIZE; i++) {
		gps_time = ktime_add(p_curr->lbs_stat[i].usage[LBS_METHOD_GPS].total_time,
			p_curr->lbs_stat[i].usage[LBS_METHOD_BATCH_GPS].total_time);
		total_gps_time_ms += ktime_to_ms(gps_time);
		if (total_gps_time_ms > 3600000)
			total_gps_time_ms = 3600000;
	}
	uAh = power_value * (unsigned int)total_gps_time_ms / 3600;
	p_curr->ce.lbs = uAh;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %u\n", __func__, p_curr->log_count, score, uAh);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_lbs(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_NET_STAT_TIZEN
static unsigned int energy_monitor_calc_penalty_wifi(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int residence_time = 0;
	unsigned int uAh = 0;
	int i;

	power_value = energy_mon.power_value.wifi_scan;

	/* currently we only support consumped energy for wifi scan */
	for (i = 0; i < NET_STAT_ARRAY_SIZE; i++)
		if (p_curr->net_stat[i].scan_req)
			residence_time = ktime_to_ms(p_curr->net_stat[i].scan_time);
	uAh = power_value * residence_time / 3600;
	p_curr->ce.wifi = uAh;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %u\n", __func__, p_curr->log_count, score, uAh);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_wifi(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_NET_STAT_TIZEN
static unsigned int energy_monitor_calc_penalty_bt(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int residence_time = 0;
	unsigned int uAh = 0;
	int i;
	ktime_t zero = ktime_set(0,0);

	power_value = energy_mon.power_value.bt_active;

	/* currently we only support consumped energy for bt active */
	for (i = 0; i < NET_STAT_ARRAY_SIZE; i++)
		if (ktime_compare(p_curr->net_stat[i].bt_active_time, zero) > 0)
			residence_time = ktime_to_ms(p_curr->net_stat[i].bt_active_time);

	uAh = power_value * residence_time / 3600;
	p_curr->ce.bt = uAh;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x: %u: %u\n", __func__,
		p_curr->log_count, score, uAh, residence_time);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_bt(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

#ifdef CONFIG_SID_SYS_STATS
static unsigned int energy_monitor_is_sid_whitelist(u32 usid)
{
	//int i;
	//unsigned int whitelist_table_size;

	//whitelist_table_size = energy_mon.power_value.cpu_whitelist_table_size;

	//for (i = 0; i < whitelist_table_size; i++)
	if (usid < 5000) {
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"%s: usid(%u) is in whitelist\n", __func__, usid);
		return 1;
	}

	return 0;
}

static unsigned int energy_monitor_calc_penalty_sid(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{
	unsigned int score = 0;
	unsigned int power_value;
	unsigned int permil;
	unsigned int uAh = 0;
	int i;

	energy_mon.fbdc.cpu_sid = 0;

	power_value = p_curr->ce.cpu_active;
	for (i = 0; i < SID_ARRAY_SIZE; i++) {
		if(energy_monitor_is_sid_whitelist(p_curr->sid[i].usid))
			continue;

		permil = p_curr->sid[i].permil;
		if (permil >= 1000)
			continue;
		uAh = power_value * permil / 1000;
		if (uAh > energy_mon.penalty.threshold_sid) {
			energy_mon.fbdc.cpu_sid = p_curr->sid[i].sid;
			score = ENERGY_MON_SCORE_CPU_TIME;

			energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
				"%s: %d: score=0x%x: usid=%u: "
				"power_value=%u: permi=%u: uAh=%u\n",
				__func__, p_curr->log_count, score, p_curr->sid[i].usid,
				power_value, permil, uAh);

			return score;
		}
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x %u %u\n", __func__,
		p_curr->log_count, score, uAh, energy_mon.fbdc.cpu_sid);

	return score;
}
#else
static inline unsigned int energy_monitor_calc_penalty_sid(
	struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev,
	int skip_penalty_check)
{ return 0;}
#endif

static unsigned int energy_monitor_calc_penalty_score(
	struct energy_mon_data *p_curr, struct energy_mon_data *p_prev)
{
	struct timespec ts_last_th_change;
	long diff_boot = 0, diff_last_th_change;
	unsigned int score = 0;
	int skip_penalty_check = 0;

	if (!p_prev || !p_curr) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: pointer is NULL\n", __func__);
		return 0;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: prev[%d]=%d: curr[%d]=%d\n", __func__,
		p_prev->log_count, p_prev->bat_status,
		p_prev->log_count, p_curr->bat_status);

	if (p_prev->bat_status == POWER_SUPPLY_STATUS_CHARGING ||
		p_prev->bat_status == POWER_SUPPLY_STATUS_FULL) {
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"%s: if charging status, just skip\n", __func__);
		return 0;
	}

	diff_boot = p_curr->ts_boot.tv_sec - p_prev->ts_boot.tv_sec;

	if (diff_boot <= energy_mon.penalty.threshold_time) {
		skip_penalty_check = 1;
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"%s: elapsed time smaller than threshold, skip_penalty_check=%d\n",
			__func__, skip_penalty_check);
	}

	ts_last_th_change = ktime_to_timespec(energy_mon.penalty.last_threshold_change);
	diff_last_th_change = p_curr->ts_boot.tv_sec - ts_last_th_change.tv_sec;

	if (diff_last_th_change <= energy_mon.penalty.threshold_time) {
		skip_penalty_check = 1;
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"%s: elapsed time from last threshold change smaller than "
			"threshold, skip_penalty_check=%d\n",
			__func__, skip_penalty_check);
	}

	score |= energy_monitor_calc_penalty_disp(p_curr, p_prev, skip_penalty_check);
	/* energy_monitor_calc_penalty_battery must be behind energy_monitor_calc_penalty_disp */
	score |= energy_monitor_calc_penalty_battery(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_sh(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_alarm(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_sap(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_tcp(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_wakeup_sources(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_slave_wakelock(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_exynos_cpuidle(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_cpu_clock(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_gpu_clock(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_exynos_sleep(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_ff(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_gps(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_hr(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_lbs(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_wifi(p_curr, p_prev, skip_penalty_check);
	score |= energy_monitor_calc_penalty_bt(p_curr, p_prev, skip_penalty_check);
	/* calculate_penalty_sid must be last in energy_monitor_calculate_penalty_xx functions */
	score |= energy_monitor_calc_penalty_sid(p_curr, p_prev, skip_penalty_check);

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %d: score=0x%x\n", __func__, p_curr->log_count,score);

	return score;
}

static void energy_mon_get_additional_info(int type,
		struct energy_mon_data *p_curr,	struct energy_mon_data *p_prev)
{
	if (!p_curr) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,"%s: pointer is NULL\n", __func__);
		return;
	}

	disp_stat_get_stat(type, &p_curr->disp_stat);
#ifdef CONFIG_SENSORHUB_STAT
	sensorhub_stat_get_stat(type, &p_curr->sh_stat);
#endif
#ifdef CONFIG_ALARM_HISTORY
	alarm_history_get_stat(type, p_curr->alarm_stat, ALARM_STAT_ARRAY_SIZE);
#endif
#ifdef CONFIG_SAP_PID_STAT
	sap_stat_get_wakeup(&p_curr->sap_wakeup);
	sap_stat_get_traffic(type, p_curr->sap_traffic, SAP_TRAFFIC_ARRAY_SIZE);
#endif
#ifdef CONFIG_PID_STAT
	pid_stat_get_traffic(type, p_curr->tcp_traffic, TCP_TRAFFIC_ARRAY_SIZE);
#endif
#ifdef CONFIG_NET_STAT_TIZEN
	net_stat_tizen_get_stat(type, p_curr->net_stat, NET_STAT_ARRAY_SIZE);
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
	get_sleep_monitor_slave_wakelock(type, p_curr->slwl, SLWL_ARRAY_SIZE);
#endif
#ifdef CONFIG_PM_SLEEP
	pm_get_large_wakeup_sources(type, p_curr->ws, WS_ARRAY_SIZE);
#endif
#ifdef CONFIG_SID_SYS_STATS
	get_sid_cputime(type, p_curr->sid, SID_ARRAY_SIZE);
#endif
#ifdef CONFIG_ARM64_EXYNOS_CPUIDLE_STAT
	cpuidle_stats_get_stats(type, &p_curr->cpuidle_stat);
	cpuidle_stats_get_blocker(type, p_curr->lpm_blocker, BLOCKER_ARRAY_SIZE);
#endif
#ifdef CONFIG_CPU_FREQ_STAT_TIZEN
	cpufreq_stats_tizen_get_stats(type, &p_curr->cpufreq_stats);
#endif
#ifdef CONFIG_GPUFREQ_STAT
	gpufreq_stat_get_stat(type, &p_curr->gpufreq_stat);
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	if (type != ENERGY_MON_TYPE_DUMP) {
		int i;

		get_sec_therm_history_energy_mon(type, p_curr->therm_history);
		for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "thm: %d: %d~%d, %d[%d/%d] %d\n",
					i, p_curr->therm_history[i].min, p_curr->therm_history[i].max,
					p_curr->therm_history[i].sum / p_curr->therm_history[i].cnt,
					p_curr->therm_history[i].sum, p_curr->therm_history[i].cnt,
					p_curr->therm_history[i].reset);
	}
#endif
#ifdef CONFIG_FF_STAT_TIZEN
	ff_stat_tizen_get_stat(type, &p_curr->ff_stat);
#endif
#ifdef CONFIG_SLEEP_STAT_EXYNOS
	sleep_stat_exynos_get_stat(type, &p_curr->sleep_stat);
#endif
#ifdef CONFIG_SENSORHUB_STAT
	sensorhub_stat_get_gps_info(type, &p_curr->sh_gps);
	sensorhub_stat_get_hr_info(type, &p_curr->sh_hr);
#endif
#ifdef CONFIG_LBS_HISTORY
	lbs_stat_get_stat(type, p_curr->lbs_stat, LBS_STAT_ARRAY_SIZE);
#endif
	if (type != ENERGY_MON_TYPE_DUMP) {
		p_curr->penalty_score =
			energy_monitor_calc_penalty_score(p_curr, p_prev);
		if (p_curr->penalty_score > 0) {
			energy_mon.penalty.score |= p_curr->penalty_score;
			energy_monitor_penalty_score_notify();
		}
	}
}

static int energy_monintor_inject_additional_info(
		int type, struct energy_mon_data *p_accu,
		struct energy_mon_data *p_curr,	struct energy_mon_data *p_prev)
{
	int i;

	/* accumulate wifi scan time */
	for (i = 0; i < NET_STAT_ARRAY_SIZE; i++) {
		if (p_curr->net_stat[i].scan_req) {
			p_accu->net_stat[i].scan_req += p_curr->net_stat[i].scan_req;
			p_accu->net_stat[i].scan_time = ktime_add(
				p_accu->net_stat[i].scan_time , p_curr->net_stat[i].scan_time);
		}
	}

	return 0;
}

static int energy_monitor_inject_data(int type,
	struct energy_mon_data *p_curr, struct energy_mon_data *p_prev)
{
	struct energy_mon_data *p_buff;
	struct timespec time_diff;
	int charging_discharging;
	int i;

	/*
	 * If type is battery and monitor, inject data to charging buffer and
	 * if type is dump, inject data to dump buffer and charging_dump buffer
	 */
	if (type == ENERGY_MON_TYPE_BATTERY || type == ENERGY_MON_TYPE_MONITOR) {
		if (!p_prev || !p_curr) {
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s: pointer is NULL\n", __func__);
			return -EINVAL;
		}

		if (p_prev->bat_status == POWER_SUPPLY_STATUS_CHARGING) {
			p_buff = &energy_mon.charging;
			energy_mon.charging_count++;
			charging_discharging = ENERGY_MON_STATE_CHARGING;
		} else if (p_prev->bat_status == POWER_SUPPLY_STATUS_DISCHARGING) {
			p_buff = &energy_mon.discharging;
			energy_mon.discharging_count++;
			charging_discharging = ENERGY_MON_STATE_DISCHARGING;
		} else {
			/* If not logging case - i.e POWER_SUPPLY_STATUS_FULL and so on, just return 0 */
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
					"%s: not logging case, type=%d, prev bat_status=%d\n",
					__func__, type, p_prev->bat_status);
			return 0;
		}
	} else if (type == ENERGY_MON_TYPE_DUMP) {
		if (energy_mon.dump.bat_status == POWER_SUPPLY_STATUS_CHARGING) {
			p_buff = &energy_mon.charging_dump;
			charging_discharging = ENERGY_MON_STATE_CHARGING;
		} else if (energy_mon.dump.bat_status != POWER_SUPPLY_STATUS_CHARGING) {
			p_buff = &energy_mon.discharging_dump;
			charging_discharging = ENERGY_MON_STATE_DISCHARGING;
		} else {
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s: unknown case\n", __func__);
			return -EPERM;
		}

		memcpy(&energy_mon.discharging_dump, &energy_mon.discharging,
				sizeof(struct energy_mon_data));
		memcpy(&energy_mon.charging_dump, &energy_mon.charging,
				sizeof(struct energy_mon_data));

		if (energy_mon.data_index == 0)
			p_prev = &energy_mon.boot;
		else {
			int prev_idx = (energy_mon.data_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM;

			p_prev = &energy_mon.data[prev_idx];
		}
		p_curr = &energy_mon.dump;
	} else {
		/* If not logging case, just return 0 */
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
				"%s: not logging case, type=%d", __func__, type);
		return 0;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: use %s buffer\n",
		__func__, charging_discharging ? "discharging" : "charging");

	/* Data checking routine */
	switch (charging_discharging) {
	case ENERGY_MON_STATE_DISCHARGING:
		if (p_curr->bat_capacity > p_prev->bat_capacity)
			energy_mon_dbg(ENERGY_MON_DEBUG_WARN,
				"%s: capacity is changed from %d to %d even discharged.\n",
				__func__, p_prev->bat_capacity, p_curr->bat_capacity);
		break;
	case ENERGY_MON_STATE_CHARGING:
		if (p_curr->bat_capacity < p_prev->bat_capacity)
			energy_mon_dbg(ENERGY_MON_DEBUG_WARN,
				"%s: capacity is changed from %d to %d even charged.\n",
				__func__, p_prev->bat_capacity, p_curr->bat_capacity);
		break;
	default:
		break;
	}

	if (p_curr->bat_capacity > p_prev->bat_capacity)
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
			"%s: capacity is changed from %d to %d\n", __func__,
			p_prev->bat_capacity, p_curr->bat_capacity);

	if (type == ENERGY_MON_TYPE_BATTERY || type == ENERGY_MON_TYPE_MONITOR) {
		p_buff->log_count++;
		p_buff->bat_capacity += p_curr->bat_capacity - p_prev->bat_capacity;
		p_buff->suspend_count += p_curr->suspend_count - p_prev->suspend_count;
	}

	/* If diff_time is negative, change to zero */
	time_diff = timespec_sub(p_curr->ts_kern, p_prev->ts_kern);
	if (time_diff.tv_sec < 0) {
		time_diff.tv_sec = 0;
		time_diff.tv_nsec = 0;
	}
	p_buff->ts_kern = timespec_add(p_buff->ts_kern, time_diff);

	time_diff = timespec_sub(p_curr->ts_boot, p_prev->ts_boot);
	if (time_diff.tv_sec < 0) {
		time_diff.tv_sec = 0;
		time_diff.tv_nsec = 0;
	}
	p_buff->ts_boot = timespec_add(p_buff->ts_boot, time_diff);

	time_diff = timespec_sub(p_curr->ts_disp, p_prev->ts_disp);
	if (time_diff.tv_sec < 0) {
		time_diff.tv_sec = 0;
		time_diff.tv_nsec = 0;
	}
	p_buff->ts_disp = timespec_add(p_buff->ts_disp, time_diff);

	time_diff = timespec_sub(p_curr->ts_aod, p_prev->ts_aod);
	if (time_diff.tv_sec < 0) {
		time_diff.tv_sec = 0;
		time_diff.tv_nsec = 0;
	}
	p_buff->ts_aod = timespec_add(p_buff->ts_aod, time_diff);

	for (i = 0; i < ENERGY_MON_WAKEUP_MAX; i++) {
		p_buff->wakeup_cause[i] +=
			p_curr->wakeup_cause[i] - p_prev->wakeup_cause[i];
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
		p_buff->wakeup_time[i] = ktime_add(
			p_buff->wakeup_time[i], p_curr->wakeup_time[i]);
#endif
	}

	/* Inject addtional information */
	energy_monintor_inject_additional_info(type, p_buff, p_curr, p_prev);

	/* Debug logs */
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s\n", __func__);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"ts_boot: %15lu.%09lu\n",
		p_buff->ts_boot.tv_sec,
		p_buff->ts_boot.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"ts_kern: %15lu.%09lu\n",
		p_buff->ts_kern.tv_sec,
		p_buff->ts_kern.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"ts_disp: %15lu.%09lu\n",
		p_buff->ts_disp.tv_sec,
		p_buff->ts_disp.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"ts_aod: %15lu.%09lu\n",
		p_buff->ts_aod.tv_sec,
		p_buff->ts_aod.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"bat_stat=%d, bat_capa=%03d\n",
		p_buff->bat_status,
		p_buff->bat_capacity);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"suspend_count=%04d\n",
		p_buff->suspend_count);

	return 0;
}

int energy_monitor_marker(int type)
{
	/* Do common works */
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: type=%d\n", __func__, type);

	if (type == ENERGY_MON_TYPE_BOOTING) {
		/* Call LCD on command at boot time */
		//disp_on_logging();
		energy_mon.running = 1;
	} else if (!energy_mon.running) {
		/* If marker is called before running(e.g. booting call), just ignore it */
		energy_mon_dbg(ENERGY_MON_DEBUG_WARN,
			"%s: called before running\n", __func__);
		return 0;
	}

	/* Assign proper buffer to save */
	if (type == ENERGY_MON_TYPE_BOOTING)
		p_curr = &energy_mon.boot;
	else if (type == ENERGY_MON_TYPE_DUMP)
		p_curr = &energy_mon.dump;
	else {
		p_curr = &energy_mon.data[energy_mon.data_index % ENERGY_MON_HISTORY_NUM];

		if (energy_mon.data_index == 0) {
			/* If it is 1st marker, use boot data as previous one */
			p_prev = &energy_mon.boot;
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
				"%s: use boot buffer as previous one\n", __func__);
		} else {
			p_prev = &energy_mon.data[(energy_mon.data_index - 1) % ENERGY_MON_HISTORY_NUM];
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
				"%s: use %d buffer as previous one\n",
				__func__, energy_mon.data_index);
		}
		p_curr->log_count = energy_mon.data_index;
	}

	/* Get time informations */
	energy_mon_get_time_info(p_curr);

	/* Get battery informations */
	energy_mon_get_battery_info(p_curr);

	/* Get wakeup reason informations */
	memcpy(p_curr->wakeup_cause,
			energy_mon.wakeup_cause, sizeof(energy_mon.wakeup_cause));
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"sizeof(energy_mon.wakeup_cause)=%lu\n",
		sizeof(energy_mon.wakeup_cause));

#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
	/* Get wakeup stat informations */
	if (type == ENERGY_MON_TYPE_BATTERY || type == ENERGY_MON_TYPE_MONITOR)
		energy_mon_get_wakeup_stat(p_curr, 1);
	else
		energy_mon_get_wakeup_stat(p_curr, 0);
#endif

	/* Get additional informations */
	if (type == ENERGY_MON_TYPE_BATTERY ||
		type == ENERGY_MON_TYPE_MONITOR ||
		type == ENERGY_MON_TYPE_DUMP)
		energy_mon_get_additional_info(type, p_curr, p_prev);

	/* Inject to charging/discharging buffer */
	if (type == ENERGY_MON_TYPE_BATTERY || type == ENERGY_MON_TYPE_MONITOR)
		energy_monitor_inject_data(type, p_curr, p_prev);
	else if (type == ENERGY_MON_TYPE_BOOTING) {
		/* Compensate disp time at boot. Add ts_kern to ts_disp */
		energy_mon.boot.ts_disp = p_curr->ts_kern;
	}

	/* Add data_index after fill all datas if data ring buffer is used */
	if (type >= ENERGY_MON_TYPE_BATTERY && type < ENERGY_MON_TYPE_DUMP)
		energy_mon.data_index++;

	return 0;
}
EXPORT_SYMBOL_GPL(energy_monitor_marker);

static int energy_monitor_background_marker(void)
{
	struct timespec ts_curr;
	struct timespec ts_elapsed;
	struct energy_mon_data *p_prev;

	get_monotonic_boottime(&ts_curr);

	if (energy_mon.data_index == 0)
		p_prev = &energy_mon.boot;
	else
		p_prev = &energy_mon.data[(energy_mon.data_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	ts_elapsed = timespec_sub(ts_curr, p_prev->ts_boot);


	if (ts_elapsed.tv_sec >= logging_interval)
		energy_monitor_marker(ENERGY_MON_TYPE_MONITOR);

	return 0;
}

static int energy_monitor_find_wakeup_index(const char *irq_name)
{
	int i;

 	if (energy_mon.irq_map_table_size > 0) {
		for (i = 0; i < energy_mon.irq_map_table_size; i++) {
			if (!strcmp(irq_name, energy_mon.irq_map_table[i].irq_name))
				return energy_mon.irq_map_table[i].wakeup_idx;
		}
	}

	return -1;
}

int energy_monitor_record_wakeup_reason(int irq, char *irq_name)
{
	struct irq_desc *desc = NULL;
	int wakeup_idx = -1;

	if (irq_name) {
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
			"%s: irq=N/A(%s)\n", __func__, irq_name);
		wakeup_idx = energy_monitor_find_wakeup_index(irq_name);
	} else if (irq > 0) {
		desc = irq_to_desc(irq);
		if (desc && desc->action && desc->action->name) {
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
				"%s: irq=%d(%s)\n", __func__, irq, desc->action->name);
			wakeup_idx = energy_monitor_find_wakeup_index(desc->action->name);
		}
	} else
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: irq=%d\n", __func__, irq);

	if (wakeup_idx >= 0) {
		energy_mon.wakeup_cause[wakeup_idx]++;
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
		energy_mon.last_wakeup_idx = wakeup_idx;
#endif
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: %02d/%02d/%02d/%02d/%02d/%02d\n",
		__func__,
		energy_mon.wakeup_cause[0],
		energy_mon.wakeup_cause[1],
		energy_mon.wakeup_cause[2],
		energy_mon.wakeup_cause[3],
		energy_mon.wakeup_cause[4],
		energy_mon.wakeup_cause[5]);

	return 0;
}
EXPORT_SYMBOL_GPL(energy_monitor_record_wakeup_reason);


/*
 * Functions for printing out
 */

static bool is_last_read_index(void)
{
	if (energy_mon.read_index - energy_mon.data_index >= ENERGY_MON_HISTORY_NUM)
		return 1;
	else
		return 0;
}

static ssize_t energy_monitor_print_time_logs(char *buf, int buf_size, int type)
{
	ssize_t ret = 0, temp_ret = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	long long average;
	int need_to_show_average = 0;
	int diff_soc;
	__kernel_time_t diff_boot = 0, diff_kern, diff_disp, diff_aod;
	int diff_wakeup[ENERGY_MON_WAKEUP_MAX];
	long kern_percent = 0, disp_percent = 0, aod_percent = 0;
	struct rtc_time tm_real;
	char temp_buf[256];
	char score_print[10] = "N/A";
	char print_type;
	char bat_status;
	char average_c;
	int i;
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
	s64 total_time_ms;
	s64 wakeup_time_ms[ENERGY_MON_WAKEUP_MAX];
	ktime_t total_time = ktime_set(0,0);
#endif

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, type=%d\n", __func__, buf_size, type);

	/* Assign proper buffer to use */
	if (type == ENERGY_MON_TYPE_BOOTING) {
		print_type = 'b';
		p_curr = &energy_mon.boot;
		p_prev = NULL;
	} else if (type == ENERGY_MON_TYPE_DUMP) {
		print_type = 'd';
		need_to_show_average = 1;
		p_curr = &energy_mon.dump;
		if (energy_mon.data_index == 0)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.data_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (type == ENERGY_MON_TYPE_BATTERY || type == ENERGY_MON_TYPE_MONITOR) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];

		snprintf(score_print, 10, "%X", p_curr->penalty_score);

		/* If nothing is marked */
		if (energy_mon.data_index <= ENERGY_MON_HISTORY_NUM && energy_mon.read_index % ENERGY_MON_HISTORY_NUM == 0) {
			p_prev = &energy_mon.boot;
			need_to_show_average = 1;
		} else if ((energy_mon.read_index - energy_mon.data_index) % ENERGY_MON_HISTORY_NUM == 0)
			p_prev = NULL;
		else {
			p_prev = &energy_mon.data[(energy_mon.read_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
			need_to_show_average = 1;
		}
	} else if (type == ENERGY_MON_TYPE_CHARGING) {
		print_type = 'C';
		need_to_show_average = 1;
		p_curr = &energy_mon.charging_dump;
		p_prev = NULL;
	} else if (type == ENERGY_MON_TYPE_DISCHARGING) {
		print_type = 'D';
		need_to_show_average = 1;
		p_curr = &energy_mon.discharging_dump;
		p_prev = NULL;
	} else {
		// TODO: Need to check return value
		// TODO: What shall I do if there is no valid case
		return 0;
	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);

	if (p_curr->bat_status == POWER_SUPPLY_STATUS_CHARGING)
		bat_status = 'C';
	else if (p_curr->bat_status == POWER_SUPPLY_STATUS_DISCHARGING)
		bat_status = 'D';
	else if (p_curr->bat_status == POWER_SUPPLY_STATUS_NOT_CHARGING)
		bat_status = 'N';
	else if (p_curr->bat_status == POWER_SUPPLY_STATUS_FULL)
		bat_status = 'F';
	else
		bat_status = 'U';

	if (need_to_show_average) {
		if (type == ENERGY_MON_TYPE_CHARGING ||
				type == ENERGY_MON_TYPE_DISCHARGING) {
			diff_soc = p_curr->bat_capacity;
			diff_boot = p_curr->ts_boot.tv_sec;
			diff_kern = p_curr->ts_kern.tv_sec;
			diff_disp = p_curr->ts_disp.tv_sec;
			diff_aod = p_curr->ts_aod.tv_sec;
			average = abs(energy_mon.unit_bat_capacity * p_curr->bat_capacity);
			memcpy(diff_wakeup, p_curr->wakeup_cause, sizeof(p_curr->wakeup_cause));
		} else {
			diff_soc = p_curr->bat_capacity - p_prev->bat_capacity;
			diff_boot = p_curr->ts_boot.tv_sec - p_prev->ts_boot.tv_sec;
			diff_kern = p_curr->ts_kern.tv_sec - p_prev->ts_kern.tv_sec;
			diff_disp = p_curr->ts_disp.tv_sec - p_prev->ts_disp.tv_sec;
			diff_aod = p_curr->ts_aod.tv_sec - p_prev->ts_aod.tv_sec;

			/* If diff_time is negative, change to zero */
			if (diff_boot < 0)
				diff_boot = 0;
			if (diff_kern < 0)
				diff_kern = 0;
			else if (diff_kern > diff_boot)
				diff_kern = diff_boot;
			if (diff_disp < 0)
				diff_disp = 0;
			else if (diff_disp > diff_boot)
				diff_disp = diff_boot;
			if (diff_aod < 0)
				diff_aod = 0;
			else if (diff_aod > diff_boot)
				diff_aod = diff_boot;

			average = abs(energy_mon.unit_bat_capacity *
						(p_curr->bat_capacity - p_prev->bat_capacity));
			for (i = 0; i < ENERGY_MON_WAKEUP_MAX; i++)
				diff_wakeup[i] = p_curr->wakeup_cause[i] - p_prev->wakeup_cause[i];
		}

		if (diff_soc > 0)
			average_c = '+';
		else
			average_c = ' ';

		/* To prevent Device by Zero */
		if (diff_boot) {
			kern_percent = diff_kern*10000/diff_boot;
			disp_percent = diff_disp*10000/diff_boot;
			aod_percent = diff_aod*10000/diff_boot;
			do_div(average, diff_boot);
		}

		temp_ret += snprintf(temp_buf + temp_ret, sizeof(temp_buf) - temp_ret,
				"/%6d/%5lu/%5lu/%5lu/%5lu/%c%3ld.%02ldmA",
				diff_soc, diff_boot, diff_kern, diff_disp, diff_aod,
				average_c, (long)average/100, (long)average%100);
#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
		temp_ret += snprintf(temp_buf + temp_ret, sizeof(temp_buf) - temp_ret,
				"%3d.%02d",
				p_curr->current_suspend, 0);
#endif
		temp_ret += snprintf(temp_buf + temp_ret, sizeof(temp_buf) - temp_ret,
				"/%3ld.%02ld/%3ld.%02ld/%3ld.%02ld",
				kern_percent/100, kern_percent%100,
				disp_percent/100, disp_percent%100,
				aod_percent/100, aod_percent%100);
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
		temp_ret += snprintf(temp_buf + temp_ret, sizeof(temp_buf) - temp_ret,
				"/%3d/%3d/%3d/%3d/%3d/%3d",
				diff_wakeup[0],
				diff_wakeup[1],
				diff_wakeup[2],
				diff_wakeup[3],
				diff_wakeup[4],
				diff_wakeup[5]);

		for (i = 0; i < ENERGY_MON_WAKEUP_MAX; i++) {
			total_time = ktime_add(total_time, p_curr->wakeup_time[i]);
			wakeup_time_ms[i] = ktime_to_ms(p_curr->wakeup_time[i]);
		}
		total_time_ms = ktime_to_ms(total_time);

		temp_ret += snprintf(temp_buf + temp_ret, sizeof(temp_buf) - temp_ret,
				"/%3lld/%3lld/%3lld/%3lld/%3lld/%3lld/%3lld/%3lld\n",
				wakeup_time_ms[0] * 100 / total_time_ms,
				wakeup_time_ms[1] * 100 / total_time_ms,
				wakeup_time_ms[2] * 100 / total_time_ms,
				wakeup_time_ms[3] * 100 / total_time_ms,
				wakeup_time_ms[4] * 100 / total_time_ms,
				wakeup_time_ms[5] * 100 / total_time_ms,
				wakeup_time_ms[6] * 100 / total_time_ms,
				wakeup_time_ms[7] * 100 / total_time_ms);
#else
		temp_ret += snprintf(temp_buf + temp_ret, sizeof(temp_buf) - temp_ret,
				"/%3d/%3d/%3d/%3d/%3d/%3d\n",
				diff_wakeup[0],
				diff_wakeup[1],
				diff_wakeup[2],
				diff_wakeup[3],
				diff_wakeup[4],
				diff_wakeup[5]);
#endif
	} else {
		snprintf(temp_buf, sizeof(temp_buf), "\n");
	}
	rtc_time_to_tm(p_curr->ts_real.tv_sec + alarm_get_tz(), &tm_real);

	ret += snprintf(buf + ret, buf_size - ret,
		"%c/%03d/%c/%2d/%4d/%6d/%04d/%10lu.%03lu/%04d-%02d-%02d %02d:%02d:%02d/%6lu.%03lu/%6lu.%03lu/%6lu.%03lu/%6lu.%03lu/%5s%s"
		, print_type, p_curr->log_count
		, bat_status, p_curr->cable_type, p_curr->bat_temp
		, p_curr->bat_capacity, p_curr->suspend_count
		, p_curr->ts_real.tv_sec, p_curr->ts_real.tv_nsec/NSEC_PER_MSEC
		, tm_real.tm_year + 1900, tm_real.tm_mon + 1
		, tm_real.tm_mday, tm_real.tm_hour
		, tm_real.tm_min, tm_real.tm_sec
		, p_curr->ts_boot.tv_sec, p_curr->ts_boot.tv_nsec/NSEC_PER_MSEC
		, p_curr->ts_kern.tv_sec, p_curr->ts_kern.tv_nsec/NSEC_PER_MSEC
		, p_curr->ts_disp.tv_sec, p_curr->ts_disp.tv_nsec/NSEC_PER_MSEC
		, p_curr->ts_aod.tv_sec, p_curr->ts_aod.tv_nsec/NSEC_PER_MSEC
		, score_print
		, temp_buf);

	return ret;
}

static ssize_t energy_mon_summary_print(char *buf, int buf_size,
		ssize_t ret, enum energy_mon_print_type p_type)
{
	struct timespec dump_time;
	struct rtc_time tm_real;

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		/* Print out Energy monitor version */
		ret += snprintf(buf + ret, buf_size - ret,
				"energy_mon_status_raw/ver%s", ENERGY_MON_VERSION);

		/* Print out UTC and Local time */
		dump_time = current_kernel_time();
		rtc_time_to_tm(dump_time.tv_sec, &tm_real);
		ret += snprintf(buf + ret, buf_size - ret,
				"/%04d-%02d-%02d %02d:%02d:%02d(UTC)"
				, tm_real.tm_year + 1900, tm_real.tm_mon + 1
				, tm_real.tm_mday, tm_real.tm_hour
				, tm_real.tm_min, tm_real.tm_sec);
		rtc_time_to_tm(dump_time.tv_sec + alarm_get_tz(), &tm_real);
		ret += snprintf(buf + ret, buf_size - ret,
				"/%04d-%02d-%02d %02d:%02d:%02d(LOCAL)\n\n"
				, tm_real.tm_year + 1900, tm_real.tm_mon + 1
				, tm_real.tm_mday, tm_real.tm_hour
				, tm_real.tm_min, tm_real.tm_sec);

		ret += snprintf(buf + ret, buf_size - ret, "[summary]\n");
		ret += snprintf(buf + ret, buf_size - ret,
				"T/CNT/B/CT/TEMP/CAPA__/SUSP/REAL_TIME_UTC_/REAL_TIME_RTC_LOCAL/"
				"BOOT_TIME_/KERN_TIME_/DISP_TIME_/_AOD_TIME_/Score/"
				"dSOC__/dBOOT/dKERN/dDISP/_dAOD/"
				"_CUR_AVER/"
#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
				"HAVSC/"
#endif
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
				"KERN_%%/DISP_%%/AOD__%%"
				"/INP/SSP/RTC/BT_/WIF/CP_"
				"/INP/SSP/RTC/BT_/WIF/CP_/WU_/NT_\n");
#else
				"KERN_%%/DISP_%%/AOD__%%/INP/SSP/RTC/BT_/WIF/CP_\n");
#endif
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_BOOTING);
	} else if (p_type == ENERGY_MON_PRINT_MAIN)
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_BATTERY);
	else {
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_DUMP);
		ret += snprintf(buf + ret, buf_size - ret, "\n");
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_CHARGING);
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_DISCHARGING);
		ret += snprintf(buf + ret, buf_size - ret, "\n");
	}

	return ret;
}

static ssize_t energy_monitor_print_disp_stat(char *buf,
				int buf_size, enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < NUM_DIST_STAT_BR_LEVELS; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
						"/%07d", NUM_DIST_STAT_BR_LEVELS - i);
		ret += snprintf(buf + ret, buf_size - ret,
				"[ds]\n%c/idx%s\n",
				print_type, temp_buf);
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n",
		__func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < NUM_DIST_STAT_BR_LEVELS; i++)
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
				"/%07lld",
				ktime_to_ms(p_curr->disp_stat.fb_time[i]));
	ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
			"/%07lld",
			ktime_to_ms(p_curr->disp_stat.aod_time));

	ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
			"/%07u",
			p_curr->ce.disp_on);

	ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
			"/%07u",
			p_curr->ce.disp_aod);

	ret += snprintf(buf + ret, buf_size - ret,
			"%c/%03d%s\n",
			print_type, p_curr->log_count, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

#ifdef CONFIG_SENSORHUB_STAT
static ssize_t energy_monitor_print_sh_wakeup(char *buf,
				int buf_size, enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SHUB_LIB_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/%02d", i);
		ret += snprintf(buf + ret, buf_size - ret,
				"[sh_wakeup]\n%c/idx%s\n",
				print_type, temp_buf);
	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n",
		__func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < SHUB_LIB_MAX; i++)
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
				"/%02d",
				p_curr->sh_stat.wakeup[i]);

	ret += snprintf(buf + ret, buf_size - ret,
			"%c/%03d%s\n",
			print_type, p_curr->log_count, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

static ssize_t energy_monitor_print_sh_event(char *buf,
				int buf_size, enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SHUB_LIB_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/%02d", i);
		ret += snprintf(buf + ret, buf_size - ret,
				"[sh_event]\n%c/idx%s\n",
				print_type, temp_buf);
	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n",
		__func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < SHUB_LIB_MAX; i++)
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
				"/%02d",
				p_curr->sh_stat.event[i]);

	ret += snprintf(buf + ret, buf_size - ret,
			"%c/%03d%s\n",
			print_type, p_curr->log_count, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

#endif

#ifdef CONFIG_ALARM_HISTORY
static ssize_t energy_monitor_print_alarm_stat(char *buf, int buf_size,
		enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < ALARM_STAT_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/___________top%d@_sec", i);
		ret += snprintf(buf + ret, buf_size - ret,
				"[alarm]\n%c/idx%s\n",
				print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n",
		__func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < ALARM_STAT_ARRAY_SIZE; i++) {
		ret_temp += snprintf(temp_buf + ret_temp,
				sizeof(temp_buf) - ret_temp,
				"/%15s@%02lu%02lu",
				p_curr->alarm_stat[i].comm,
				p_curr->alarm_stat[i].wakeup_count,
				p_curr->alarm_stat[i].expire_count);
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_SAP_PID_STAT
static ssize_t energy_monitor_print_sap_wakeup(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];

		if (energy_mon.data_index <= ENERGY_MON_HISTORY_NUM)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.read_index +
				ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_TAIL) {
		print_type = 'd';
		p_curr = &energy_mon.dump;
		if (energy_mon.read_index == 0)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.data_index +
				ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_MAIN) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
		p_prev = &energy_mon.data[(energy_mon.read_index +
			ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else {
		// TODO: Need to check return value
		// TODO: What shall I do if there is no valid case
		return 0;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp, "/%03d", i);
		ret += snprintf(buf + ret, buf_size - ret,
				"[sap_wakeup]\n%c/idx%s\n",print_type, temp_buf);
	}

	ret_temp = 0;

	if (p_prev) {
		for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/%03d",
						p_curr->sap_wakeup.wakeup_cnt[i] -
						p_prev->sap_wakeup.wakeup_cnt[i]);
	} else {
		for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/%03d",
						p_curr->sap_wakeup.wakeup_cnt[i]);
		}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p, p_prev=%p\n",
		__func__, p_curr, p_prev);
	return ret;
}

static ssize_t energy_monitor_print_sap_activity(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];

		if (energy_mon.data_index <= ENERGY_MON_HISTORY_NUM)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.read_index +
				ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_TAIL) {
		print_type = 'd';
		p_curr = &energy_mon.dump;
		if (energy_mon.read_index == 0)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.data_index +
				ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_MAIN) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
		p_prev = &energy_mon.data[(energy_mon.read_index +
			ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else {
		// TODO: Need to check return value
		// TODO: What shall I do if there is no valid case
		return 0;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp, "/%03d", i);
		ret += snprintf(buf + ret, buf_size - ret, "[sap_activity]\n%c/idx%s\n"
			, print_type, temp_buf);
	}

	ret_temp = 0;

	if (p_prev) {
		for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/%03d",
						p_curr->sap_wakeup.activity_cnt[i] -
						p_prev->sap_wakeup.activity_cnt[i]);
	} else {
		for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/%03d",
						p_curr->sap_wakeup.activity_cnt[i]);
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p, p_prev=%p\n",
		__func__, p_curr, p_prev);
	return ret;
}

static ssize_t energy_monitor_print_sap_traffic(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SAP_TRAFFIC_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/___________top%d@sndbytes(count)rcvbytes(count)", i);
		ret += snprintf(buf + ret, buf_size - ret, "[sap_traffic]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);

	ret_temp = 0;
	for (i = 0; i < SAP_TRAFFIC_ARRAY_SIZE; i++)
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/%15s@%8d(%5d)%8d(%5d)",
					p_curr->sap_traffic[i].aspid.id,
					p_curr->sap_traffic[i].snd,
					p_curr->sap_traffic[i].snd_count,
					p_curr->sap_traffic[i].rcv,
					p_curr->sap_traffic[i].rcv_count);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

#endif

#ifdef CONFIG_PID_STAT
static ssize_t energy_monitor_print_pid_stat_logs(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < TCP_TRAFFIC_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/___________top%d@sndbytes(count)rcvbytes(count)", i);
		ret += snprintf(buf + ret, buf_size - ret, "[pid_stat]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);

	ret_temp = 0;
	for (i = 0; i < TCP_TRAFFIC_ARRAY_SIZE; i++)
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/%15s@%8d(%5d)%8d(%5d)",
					p_curr->tcp_traffic[i].comm,
					p_curr->tcp_traffic[i].snd,
					p_curr->tcp_traffic[i].snd_count,
					p_curr->tcp_traffic[i].rcv,
					p_curr->tcp_traffic[i].rcv_count);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_NET_STAT_TIZEN
static ssize_t energy_monitor_print_net_stat1(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < NET_STAT_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/sndbytes(count)rcvbytes(count)");
		ret += snprintf(buf + ret, buf_size - ret, "[nstat1]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < NET_STAT_ARRAY_SIZE; i++)
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/%8lld(%5lld)%8lld(%5lld)",
					p_curr->net_stat[i].tx_bytes,
					p_curr->net_stat[i].tx_packets,
					p_curr->net_stat[i].rx_bytes,
					p_curr->net_stat[i].rx_packets);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

static ssize_t energy_monitor_print_net_stat2(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < NET_STAT_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/  s  u  d  s  c  d  f");
		ret += snprintf(buf + ret, buf_size - ret, "[nstat2]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < NET_STAT_ARRAY_SIZE; i++)
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/%3d%3d%3d%3d%3d%3d%3d",
					p_curr->net_stat[i].state,
					p_curr->net_stat[i].up,
					p_curr->net_stat[i].down,
					p_curr->net_stat[i].wifi_state,
					p_curr->net_stat[i].connection,
					p_curr->net_stat[i].disconnection,
					p_curr->net_stat[i].connection_fail);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

static ssize_t energy_monitor_print_net_stat3(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300] = {0,};
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
					"/ req rom time");
		ret += snprintf(buf + ret, buf_size - ret, "[nstat3]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < NET_STAT_ARRAY_SIZE; i++)
		if (p_curr->net_stat[i].scan_req)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/%4d%4d %-8lld",
						p_curr->net_stat[i].scan_req,
						p_curr->net_stat[i].roaming_done,
						ktime_to_ms(p_curr->net_stat[i].scan_time));

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_SLAVE_WAKELOCK
static ssize_t energy_monitor_print_slave_wakelock_logs(char *buf, int buf_size,
		enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SLWL_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/___________top%d@_sec", i);
		ret += snprintf(buf + ret, buf_size - ret,
				"[slave_wakelock]\n%c/idx%s\n", print_type, temp_buf);
	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < SLWL_ARRAY_SIZE; i++)
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
					"/%15s@%04ld",
					p_curr->slwl[i].slwl_name,
					ktime_to_timeval(p_curr->slwl[i].prevent_time).tv_sec);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n"
		, print_type, p_curr->log_count
		, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_PM_SLEEP
static ssize_t energy_monitor_print_wakeup_source(char *buf, int buf_size,
		enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < WS_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/___________top%d@_sec", i);
		ret += snprintf(buf + ret, buf_size - ret,
				"[ws]\n%c/idx%s\n",
				print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n",
		__func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < WS_ARRAY_SIZE; i++) {
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/%15s@%04ld",
					p_curr->ws[i].name,
					ktime_to_timeval(p_curr->ws[i].emon_total_time).tv_sec);
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_SID_SYS_STATS
static ssize_t energy_monitor_print_sid_sys_stats(char *buf, int buf_size,
		enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SID_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/___________top%d@_sec", i);
		ret += snprintf(buf + ret, buf_size - ret,
				"[sid]\n%c/idx%s\n",
				print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n",
		__func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < SID_ARRAY_SIZE; i++) {
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/%7d@%08lld@%3u",
					p_curr->sid[i].usid,
					(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(p_curr->sid[i].ttime)),
					p_curr->sid[i].permil);
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_ARM64_EXYNOS_CPUIDLE_STAT
static ssize_t energy_monitor_print_cpuidle_stat(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	char temp_buf[300];
	char print_type;
	int i, j;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
					"/        active/ c0_total_idle"
					"/ c1_total_idle/    c0_cstate1"
					"/    c1_cstate1/    c0_cstate2"
					"/    c1_cstate2/           lpm");
		ret += snprintf(buf + ret, buf_size - ret, "[cpuidle]\n%c/idx%s\n"
				, print_type, temp_buf);
	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);

	ret_temp = 0;
	ret_temp += snprintf(temp_buf + ret_temp,
				sizeof(temp_buf) - ret_temp,
				"/ %13llu",
				ktime_to_ms(p_curr->cpuidle_stat.total_stat_time));
	for (i = 0; i < p_curr->cpuidle_stat.cpu_count; i++)
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/ %13llu",
					ktime_to_ms(p_curr->cpuidle_stat.cpuidle[i].total_idle_time));
	for (j = 0; j < p_curr->cpuidle_stat.state_count; j++)
		for (i = 0; i < p_curr->cpuidle_stat.cpu_count; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/ %13llu",
						ktime_to_ms(p_curr->cpuidle_stat.cpuidle[i].usage[j].time));
	ret_temp += snprintf(temp_buf + ret_temp,
				sizeof(temp_buf) - ret_temp,
				"/ %13llu",
				ktime_to_ms(p_curr->cpuidle_stat.lpm.usage.time));

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

static ssize_t energy_monitor_print_lpm_blocker(char *buf, int buf_size,
		enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char name[BLOCKER_NAME_SIZE] = {0,};
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < BLOCKER_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/___________top%d@_count", i);
		ret += snprintf(buf + ret, buf_size - ret,
				"[lpm_blocker]\n%c/idx%s\n",
				print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n",
		__func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < BLOCKER_ARRAY_SIZE; i++) {
		if (p_curr->lpm_blocker[i].name)
			memcpy(name, p_curr->lpm_blocker[i].name, BLOCKER_NAME_SIZE-1);
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/ %8s@%12lld",
					name,
					p_curr->lpm_blocker[i].count);
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_CPU_FREQ_STAT_TIZEN
static ssize_t energy_monitor_print_cpufreq_stats(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < p_curr->cpufreq_stats.state_num; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/%10u", p_curr->cpufreq_stats.freq_table[i]);
		ret += snprintf(buf + ret, buf_size - ret, "[cpu_freq]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < p_curr->cpufreq_stats.state_num; i++)
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/%10llu",
					(unsigned long long)
					jiffies_to_msecs(p_curr->cpufreq_stats.time_in_state[i]));

	ret_temp += snprintf(temp_buf + ret_temp,
				sizeof(temp_buf) - ret_temp,
				"/%10u",
				p_curr->ce.cpu_active);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_GPUFREQ_STAT
static ssize_t energy_monitor_print_gpufreq_stat(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < p_curr->gpufreq_stat.table_size; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"/%10u",
						p_curr->gpufreq_stat.table[i].clock);
		ret += snprintf(buf + ret, buf_size - ret, "[gpu_freq]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < p_curr->gpufreq_stat.table_size; i++)
		ret_temp += snprintf(temp_buf + ret_temp,
					sizeof(temp_buf) - ret_temp,
					"/%10llu",
					(unsigned long long)
					jiffies_to_msecs(p_curr->gpufreq_stat.table[i].time));

	ret_temp += snprintf(temp_buf + ret_temp,
				sizeof(temp_buf) - ret_temp,
				"/%10u",
				p_curr->ce.gpu_active);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count,
			temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
static ssize_t energy_monitor_print_sec_therm_history(char *buf,
	int buf_size, enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	if (p_type == ENERGY_MON_PRINT_TAIL) {
		ret += snprintf(buf + ret, buf_size - ret, "\n");
		return ret;
	}

	print_type = '*';
	p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
							sizeof(temp_buf) - ret_temp,
							"//___%12s ___",
							sec_therm_dev_name[i]); /* len : 20 */
		ret_temp += snprintf(temp_buf + ret_temp,
						sizeof(temp_buf) - ret_temp,
						"\n%c    ", print_type);
		for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
			ret_temp += snprintf(temp_buf + ret_temp,
							sizeof(temp_buf) - ret_temp,
							"//RND/MIN/MAX/AVG/CNT");
		ret += snprintf(buf + ret, buf_size - ret,
					"[therm_hist]\n%c/idx%s\n", print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
		if (p_curr->therm_history[i].reset) {
			ret_temp += snprintf(temp_buf + ret_temp,
							sizeof(temp_buf) - ret_temp,
							"//  -/  -/  -/  -/  -");
		} else {
			ret_temp += snprintf(temp_buf + ret_temp,
							sizeof(temp_buf) - ret_temp,
							"//%3d/%3d/%3d/%3d/%3d",
							p_curr->therm_history[i].round,
							p_curr->therm_history[i].min,
							p_curr->therm_history[i].max,
							p_curr->therm_history[i].sum / p_curr->therm_history[i].cnt,
							p_curr->therm_history[i].cnt);
		}
	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n",
			print_type, p_curr->log_count, temp_buf);

	return ret;
}
#endif

#ifdef CONFIG_FF_STAT_TIZEN
static ssize_t energy_monitor_print_ff_stat(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	char print_type;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		ret += snprintf(buf + ret, buf_size - ret, "[ff]\n");
		ret += snprintf(buf + ret, buf_size - ret,
				"%c/idx/________time/_______count/\n", print_type);
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d/",
			print_type, p_curr->log_count);
	ret += snprintf(buf + ret, buf_size - ret, "%12lld/",
			ktime_to_ms(p_curr->ff_stat.total_time));
	ret += snprintf(buf + ret, buf_size - ret, "%12lu/",
			p_curr->ff_stat.play_count);
	ret += snprintf(buf + ret, buf_size - ret, "%12u/\n",
			p_curr->ce.ff);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);
	return ret;
}
#endif

#ifdef CONFIG_SLEEP_STAT_EXYNOS
static ssize_t energy_monitor_print_sleep_stat(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	char print_type;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		ret += snprintf(buf + ret, buf_size - ret, "[sleep_stat]\n");
		ret += snprintf(buf + ret, buf_size - ret, "%c/idx", print_type);
		ret += snprintf(buf + ret, buf_size - ret,
				"/ fail/ ffrz/ fprp/ fsup/ fsul/ fsun/ sups"
				"/ slew/ slsd/ slmd/ siew/ sisd/ simd\n");
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d/",
			print_type, p_curr->log_count);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/",
			p_curr->sleep_stat.fail);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/",
			p_curr->sleep_stat.failed_freeze);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/",
			p_curr->sleep_stat.failed_prepare);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/",
			p_curr->sleep_stat.failed_suspend);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/",
			p_curr->sleep_stat.failed_suspend_late);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/",
			p_curr->sleep_stat.failed_suspend_noirq);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/",
			p_curr->sleep_stat.suspend_success);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/",
			p_curr->sleep_stat.acpm_sleep_early_wakeup);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/",
			p_curr->sleep_stat.acpm_sleep_soc_down);
	ret += snprintf(buf + ret, buf_size - ret, "%5d/ ",
			p_curr->sleep_stat.acpm_sleep_mif_down);
	ret += snprintf(buf + ret, buf_size - ret, "%d/ ",
			p_curr->sleep_stat.acpm_sicd_early_wakeup);
	ret += snprintf(buf + ret, buf_size - ret, "%d/ ",
			p_curr->sleep_stat.acpm_sicd_soc_down);
	ret += snprintf(buf + ret, buf_size - ret, "%d\n",
			p_curr->sleep_stat.acpm_sicd_mif_down);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);
	return ret;
}
#endif

#ifdef CONFIG_SENSORHUB_STAT // GPS, HR
static ssize_t energy_monitor_print_gps_stat(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0;
	struct energy_mon_data *p_curr = NULL;
	char print_type;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		ret += snprintf(buf + ret, buf_size - ret, "[gps]\n");
		ret += snprintf(buf + ret, buf_size - ret, "%c/idx/time/", print_type);
		ret += snprintf(buf + ret, buf_size - ret, "___last_gps_user/ext/\n");
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d/%04d/",
			print_type, p_curr->log_count, p_curr->sh_gps.gps_time);
	ret += snprintf(buf + ret, buf_size - ret, "%016llx/",
			p_curr->sh_gps.last_gps_user);
	ret += snprintf(buf + ret, buf_size - ret, " %02u/\n",
			p_curr->sh_gps.last_gps_ext);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	return ret;
}

static ssize_t energy_monitor_print_hr_stat(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0;
	struct energy_mon_data *p_curr = NULL;
	char print_type;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		ret += snprintf(buf + ret, buf_size - ret, "[hr]\n");
		ret += snprintf(buf + ret, buf_size - ret, "%c/idx/succ/cn/fail/cn/\n",
				print_type);
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d/%04d/%02d/%04d/%02d/\n",
			print_type, p_curr->log_count,
			p_curr->sh_hr.success_dur, p_curr->sh_hr.success_cnt,
			p_curr->sh_hr.fail_dur, p_curr->sh_hr.fail_cnt);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	return ret;
}
#endif

#ifdef CONFIG_LBS_HISTORY
static ssize_t energy_monitor_print_lbs_stat(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	char temp_buf[300];
	char print_type;
	int i;
	ktime_t gps_time;
	s64 gps_time_ms;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n",
		__func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < LBS_STAT_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf)-ret_temp,
						"/_top%d@____________msec", i);
		ret += snprintf(buf + ret, buf_size - ret, "[lbs_stat]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < LBS_STAT_ARRAY_SIZE; i++) {
		gps_time = ktime_add(p_curr->lbs_stat[i].usage[LBS_METHOD_GPS].total_time,
			p_curr->lbs_stat[i].usage[LBS_METHOD_BATCH_GPS].total_time);
		gps_time_ms = ktime_to_ms(gps_time);
		if (gps_time_ms > 3600000)
			gps_time_ms = 3600000;
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
					"/%5d@%7lld(%7lld)",
					p_curr->lbs_stat[i].usid,
					ktime_to_ms(p_curr->lbs_stat[i].total_time),
					gps_time_ms);
	}
	ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf) - ret_temp,
				"/%5d",
				p_curr->ce.lbs);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n"
			, print_type, p_curr->log_count
			, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

static ssize_t energy_monitor_print_ce(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0;
	unsigned int sum;
	struct energy_mon_data *p_curr = NULL;
	char print_type;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index %
			ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		ret += snprintf(buf + ret, buf_size - ret, "[ce]\n");
		ret += snprintf(buf + ret, buf_size - ret, "%c/idx", print_type);
		ret += snprintf(buf + ret, buf_size - ret,
				"/    da/    do/    ci/    ca/    ga/    ff/"
				"    gp/    lb/    hr/    bt/    wf/   sum\n");
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d/",
			print_type, p_curr->log_count);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.disp_aod);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.disp_on);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.cpu_idle);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.cpu_active);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.gpu_active);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.ff);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.gps);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.lbs);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.hr);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.bt);
	ret += snprintf(buf + ret, buf_size - ret, "%6d/",
			p_curr->ce.wifi);
	sum = p_curr->ce.disp_aod + p_curr->ce.disp_on + p_curr->ce.cpu_idle +
		p_curr->ce.cpu_active + p_curr->ce.gpu_active + p_curr->ce.ff +
		p_curr->ce.gps + p_curr->ce.lbs + p_curr->ce.hr + p_curr->ce.wifi +
		p_curr->ce.bt;
	ret += snprintf(buf + ret, buf_size - ret, "%6d\n",
			sum);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: p_curr=%p\n", __func__, p_curr);
	return ret;
}

static ssize_t energy_mon_print(char *buf, int buf_size, ssize_t ret,
	enum energy_mon_print_step print_step, enum energy_mon_print_type p_type)
{
	switch (print_step) {
	case STEP_SUMMARY:
		ret += energy_mon_summary_print(buf, buf_size, ret, p_type);
		break;
	case STEP_DISP_STAT:
		ret += energy_monitor_print_disp_stat(buf + ret, buf_size, p_type);
		break;
#ifdef CONFIG_SENSORHUB_STAT
	case STEP_SENSORHUB_WAKEUP:
		ret += energy_monitor_print_sh_wakeup(buf + ret, buf_size, p_type);
		break;
	case STEP_SENSORHUB_ACTIVITY:
		ret += energy_monitor_print_sh_event(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_ALARM_HISTORY
	case STEP_ALARM_STAT:
		ret += energy_monitor_print_alarm_stat(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SAP_PID_STAT
	case STEP_SAP_WAKEUP:
		ret += energy_monitor_print_sap_wakeup(buf + ret, buf_size, p_type);
		break;
	case STEP_SAP_ACTIVITY:
		ret += energy_monitor_print_sap_activity(buf + ret, buf_size, p_type);
		break;
	case STEP_SAP_TRAFFIC:
		ret += energy_monitor_print_sap_traffic(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_PID_STAT
	case STEP_PID_STAT:
		ret += energy_monitor_print_pid_stat_logs(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_NET_STAT_TIZEN
	case STEP_NET_STAT1:
		ret += energy_monitor_print_net_stat1(buf + ret, buf_size, p_type);
		break;
	case STEP_NET_STAT2:
		ret += energy_monitor_print_net_stat2(buf + ret, buf_size, p_type);
		break;
	case STEP_NET_STAT3:
		ret += energy_monitor_print_net_stat3(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
	case STEP_SLAVE_WAKELOCK:
		ret += energy_monitor_print_slave_wakelock_logs(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_PM_SLEEP
	case STEP_WAKEUP_SOURCE:
		ret += energy_monitor_print_wakeup_source(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SID_SYS_STATS
	case STEP_SID_SYS_STATS:
		ret += energy_monitor_print_sid_sys_stats(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_ARM64_EXYNOS_CPUIDLE_STAT
	case STEP_CPUIDLE_STAT:
		ret += energy_monitor_print_cpuidle_stat(buf + ret, buf_size, p_type);
		break;
	case STEP_CPUIDLE_LPM_BLOCKER:
		ret += energy_monitor_print_lpm_blocker(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_CPU_FREQ_STAT_TIZEN
	case STEP_CPUFREQ_STATS:
		ret += energy_monitor_print_cpufreq_stats(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_GPUFREQ_STAT
	case STEP_GPUFREQ_STAT:
		ret += energy_monitor_print_gpufreq_stat(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	case STEP_SEC_THERM_HISTORY:
		ret += energy_monitor_print_sec_therm_history(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_FF_STAT_TIZEN
	case STEP_FF_STAT:
		ret += energy_monitor_print_ff_stat(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SLEEP_STAT_EXYNOS
	case STEP_SLEEP_STAT:
		ret += energy_monitor_print_sleep_stat(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SENSORHUB_STAT
	case STEP_GPS_STAT:
		ret += energy_monitor_print_gps_stat(buf + ret, buf_size, p_type);
		break;
	case STEP_HR_STAT:
		ret += energy_monitor_print_hr_stat(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_LBS_HISTORY
	case STEP_LBS_HISTORY:
		ret += energy_monitor_print_lbs_stat(buf + ret, buf_size, p_type);
		break;
#endif
	case STEP_CE:
		ret += energy_monitor_print_ce(buf + ret, buf_size, p_type);
		break;
	default:
		break;
	}

	return ret;
}

static ssize_t read_status_raw(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	static char buf[PAGE_SIZE];
	static enum energy_mon_print_step print_step;
	static int need_to_print_title;
	ssize_t ret = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s:read_index=%d, write_index=%d\n",
		__func__, energy_mon.read_index, energy_mon.data_index);

	if (*ppos == 0) {
		energy_monitor_marker(ENERGY_MON_TYPE_DUMP);
		energy_monitor_inject_data(ENERGY_MON_TYPE_DUMP, NULL, NULL);

		/* Start print step */
		print_step = STEP_SUMMARY;
		/* Initialize read index */
		energy_mon.read_index = energy_mon.data_index;
		ret += energy_mon_print(buf, sizeof(buf), ret, print_step, ENERGY_MON_PRINT_TITLE);
		need_to_print_title = 0;
	} else if (print_step != STEP_MAX) {
		 if (energy_mon.data_index == 0 || is_last_read_index()) {
			ret += energy_mon_print(buf, sizeof(buf), ret, print_step, ENERGY_MON_PRINT_TAIL);

			/* Go to next print_step */
			print_step++;
			/* Initialize read index */
			energy_mon.read_index = energy_mon.data_index;
			need_to_print_title = 1;
		} else {
			/* Skip buffer when it is not used yet */
			while (energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM].log_count == -1)
				energy_mon.read_index++;

			if (need_to_print_title == 1) {
				ret += energy_mon_print(buf, sizeof(buf), ret, print_step, ENERGY_MON_PRINT_TITLE);
				need_to_print_title = 0;
			} else
				ret += energy_mon_print(buf, sizeof(buf), ret, print_step, ENERGY_MON_PRINT_MAIN);

			energy_mon.read_index++;
		}
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: ret = %d\n", __func__, (int)ret);

	return ret;
}

#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
static char *energy_mononitor_get_ws_name_string(int wakeup_idx)
{
	/* Need to make sync with enum energy_mon_wakeup_source in the energy_monitor.h */
	static char *wakeup_text[] = {
		"INPUT", "SSP", "RTC", "BT", "WIFI", "CP", "WU", "NT"
	};

	if (wakeup_idx >= ENERGY_MON_WAKEUP_MAX)
		return NULL;

	return wakeup_text[wakeup_idx];
}

static ssize_t read_status_wakeup_time(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char buf[1536];
	int i, j;
	int wakeup_count;
	s64 wakeup_time_ms, average_time_ms, total_time_ms;
	ktime_t total_time = ktime_set(0,0);

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		ret += snprintf(buf + ret, sizeof(buf) - ret,
				"WAKEUP/CNT/__TIME_TOTAL/TIME_AVERA/PCT\n");
		for (i = 0; i < ENERGY_MON_WAKEUP_MAX; i++)
			total_time = ktime_add(total_time, energy_mon.wakeup_time[i]);
		total_time_ms = ktime_to_ms(total_time);

		for (i = 0; i < ENERGY_MON_WAKEUP_MAX; i++) {
			if (i == energy_mon.last_wakeup_idx)
				wakeup_count = energy_mon.wakeup_cause[i] - 1;
			else
				wakeup_count = energy_mon.wakeup_cause[i];

			wakeup_time_ms = ktime_to_ms(energy_mon.wakeup_time[i]);
			if (wakeup_count)
				average_time_ms = wakeup_time_ms / wakeup_count;
			else
				average_time_ms = 0;

			ret += snprintf(buf + ret, sizeof(buf) - ret,
				"%6s/%3d/%12lld/%10lld/%3lld\n",
				energy_mononitor_get_ws_name_string(i), wakeup_count,
				wakeup_time_ms, average_time_ms,
				wakeup_time_ms * 100 / total_time_ms);
		}

		for (i = 0; i < ENERGY_MON_MAX_WAKEUP_STAT_TIME + 1; i++) {
			ret += snprintf(buf + ret, sizeof(buf) - ret,
					"%2d ~ %2ds : ", i, i + 1);
			for (j = 0; j < ENERGY_MON_WAKEUP_MAX; j++) {
				ret += snprintf(buf + ret, sizeof(buf) - ret,
						"%3d ", energy_mon.wakeup_time_stats[j][i]);
			}
			ret += snprintf(buf + ret, sizeof(buf) - ret, "\n");
		}
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: ret = %d\n", __func__, (int)ret);

	if (ret > sizeof(buf))
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: buffer overflow!!! ret = %d\n", __func__, (int)ret);

	return ret;
}
#endif

#ifdef CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
static ssize_t read_sleep_current(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	static char buf[800];
	int i;
	long long average_sum = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		ret += snprintf(buf + ret, sizeof(buf) - ret, "CNT/AVERAGE_CUR\n");
		for (i = 0; i < energy_mon.estimator_index; i++) {
			ret += snprintf(buf + ret, sizeof(buf) - ret,
					"%3d/%3ld.%02ldmA\n",
					i,
					(long)energy_mon.estimator_average[i]/100,
					(long)energy_mon.estimator_average[i]%100);
			average_sum += energy_mon.estimator_average[i];
		}
		if (energy_mon.estimator_index != 0) {
			do_div(average_sum, energy_mon.estimator_index);
			ret += snprintf(buf + ret, sizeof(buf) - ret,
					"avg/%3ld.%02ldmA\n",
					(long)average_sum / 100,
					(long)average_sum % 100);
		}
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: ret = %d\n", __func__, (int)ret);

	if (ret > sizeof(buf))
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: buffer overflow!!! ret = %d\n", __func__, (int)ret);

	return ret;
}
#endif /* CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR */

static ssize_t read_monitor_interval(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char buf[10];

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0)
		ret += snprintf(buf, sizeof(buf), "%u\n", monitor_interval);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	return ret;
}

static ssize_t write_monitor_interval(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	unsigned int new_interval = 0;
	int ret = 0;

	ret = kstrtouint(user_buf, 0, &new_interval);
	if (ret < 0)
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: kstrtouint is failed\n", __func__);

	/* Maximum interval is 1 day */
	if (new_interval > ENERGY_MONITOR_MAX_MONITOR_INTERVAL)
		new_interval = ENERGY_MONITOR_MAX_MONITOR_INTERVAL;

	monitor_interval = new_interval;

	if (monitor_interval > 0) {
		schedule_delayed_work(&monitor_work, monitor_interval * HZ);
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: monitor thread is started\n", __func__);
	} else if (monitor_interval == 0) {
		cancel_delayed_work(&monitor_work);
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: monior thread is canceled\n", __func__);
	}

	return count;
}

static ssize_t read_logging_interval(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char buf[10];

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0)
		ret += snprintf(buf, sizeof(buf), "%u\n", logging_interval);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	return ret;
}

static ssize_t write_logging_interval(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	unsigned int new_interval = 0;
	int ret = 0;

	ret = kstrtouint(user_buf, 0, &new_interval);
	if (ret < 0)
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: kstrtouint is failed\n", __func__);

	logging_interval = new_interval;

	return count;
}

static const struct file_operations status_raw_fops = {
	.read = read_status_raw,
};

static const struct file_operations status_raw2_fops = {
	.read = read_status_raw,
};

#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
static const struct file_operations status_wakeup_time_fops = {
	.read = read_status_wakeup_time,
};
#endif /* CONFIG_ENERGY_MONITOR_WAKEUP_STAT */

#ifdef CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
static const struct file_operations sleep_current_fops = {
	.read = read_sleep_current,
};
#endif /* CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR */

static const struct file_operations monitor_interval_fops = {
	.write		= write_monitor_interval,
	.read		= read_monitor_interval,
};

static const struct file_operations logging_interval_fops = {
	.write		= write_logging_interval,
	.read		= read_logging_interval,
};

#ifdef CONFIG_SEC_SYSFS
static ssize_t energy_monitor_get_last_discharging_info(
	struct device *dev, struct device_attribute *attr, char *buf) {
	static struct energy_mon_data last_discharging_info;
	static bool is_first = true;
	int diff_soc;
	__kernel_time_t diff_boot = 0;

	energy_monitor_marker(ENERGY_MON_TYPE_DUMP);
	energy_monitor_inject_data(ENERGY_MON_TYPE_DUMP, NULL, NULL);

	if (is_first) {
		diff_soc = energy_mon.discharging_dump.bat_capacity * -1;
		diff_boot = energy_mon.discharging_dump.ts_boot.tv_sec;
		is_first = false;
	} else {
		diff_soc = (energy_mon.discharging_dump.bat_capacity -
						last_discharging_info.bat_capacity) * -1;
		diff_boot = energy_mon.discharging_dump.ts_boot.tv_sec -
						last_discharging_info.ts_boot.tv_sec;
	}

	if (diff_soc < 0)
		diff_soc = 0;
	if (diff_boot < 0)
		diff_boot = 0;

	memcpy(&last_discharging_info, &energy_mon.discharging_dump, sizeof(struct energy_mon_data));

	return scnprintf(buf, PAGE_SIZE, "dSOC:%d dBOOT:%lu\n", diff_soc, diff_boot);
}
static DEVICE_ATTR(get_last_discharging_info, 0440,
	energy_monitor_get_last_discharging_info,
	NULL);

static ssize_t energy_monitor_show_penalty_score(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret += scnprintf(buf, PAGE_SIZE, "%d\n", energy_mon.penalty.score);

	return ret;
}

static ssize_t energy_monitor_store_penalty_score(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	if (val > ENERGY_MON_SCORE_MAX)
		return -EINVAL;

	energy_mon.penalty.score = val;

	return count;
}

static DEVICE_ATTR(penalty_score, 0660,
	energy_monitor_show_penalty_score,
	energy_monitor_store_penalty_score);

static ssize_t energy_monitor_store_penalty_threshold(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	const char *str;
	size_t len;
	char threshold_type[10] = {0,};
	int threshold = 0;
	int err;

	str = buf;

	while (*str && !isspace(*str))
		str++;

	len = str - buf;
	if (!len || len >= sizeof(threshold_type))
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a threshold string appended. */
		err = kstrtos32(skip_spaces(str), 10, &threshold);
		if (err)
			return -EINVAL;
	}

	memcpy(threshold_type, buf, len);

	if (!strcmp(threshold_type, "time"))
		energy_mon.penalty.threshold_time = threshold;

	if (!strcmp(threshold_type, "batt"))
		energy_mon.penalty.threshold_batt = threshold * 1000; /* uA */

	if (!strcmp(threshold_type, "disp"))
		energy_mon.penalty.threshold_disp = threshold;

	if (!strcmp(threshold_type, "gps"))
		energy_mon.penalty.threshold_gps = threshold;

	if (!strcmp(threshold_type, "lbs"))
		energy_mon.penalty.threshold_lbs = threshold;

	if (!strcmp(threshold_type, "cpuidle"))
		energy_mon.penalty.threshold_cpuidle= threshold;

	if (!strcmp(threshold_type, "ws"))
		energy_mon.penalty.threshold_ws = threshold;

	if (!strcmp(threshold_type, "slwl"))
		energy_mon.penalty.threshold_slwl = threshold;

	if (!strcmp(threshold_type, "sid"))
		energy_mon.penalty.threshold_sid = threshold;

	energy_mon.penalty.last_threshold_change = ktime_get_boottime();

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: %s: %d\n",
			__func__, threshold_type, threshold);

	return count;
}

static DEVICE_ATTR(penalty_threshold, 0200,
	NULL,
	energy_monitor_store_penalty_threshold);

static ssize_t energy_monitor_show_penalty_cause(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret += scnprintf(buf, PAGE_SIZE, "%s\n", ENERGY_MON_SCORE_VERSION);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 0 */
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 1 */
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 2 */

	/* Bit 3 */
	if (energy_mon.penalty.score & ~ENERGY_MON_SCORE_WS) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%s\n",
				energy_mon.fbdc.ws_name);

	}

	/* Bit 4 */
	if (energy_mon.penalty.score & ~ENERGY_MON_SCORE_SLAVE_WAKELOCK) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%s\n",
				energy_mon.fbdc.slwl_name);

	}

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 5 */

	/* Bit 6 */
	if (energy_mon.penalty.score & ~ENERGY_MON_SCORE_LBS) {
		char *ctx = NULL;
		unsigned n;

		if (security_secid_to_secctx(energy_mon.fbdc.lbs_sid, &ctx, &n) == 0) {
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%s \n", ctx);
			security_release_secctx(ctx, n);
		}
	}

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 7 */
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 8 */
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 9 */
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 10 */
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 11 */
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 12 */
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 13 */
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n"); /* Bit 14 */

	/* Bit 15 */
	if (energy_mon.penalty.score & ~ENERGY_MON_SCORE_CPU_TIME) {
		char *ctx = NULL;
		unsigned n;

		if (security_secid_to_secctx(energy_mon.fbdc.cpu_sid, &ctx, &n) == 0) {
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%s \n", ctx);
			security_release_secctx(ctx, n);
		}
	}

	return ret;
}

static DEVICE_ATTR(penalty_cause, 0400,
	energy_monitor_show_penalty_cause,
	NULL);

static ssize_t energy_monitor_show_batr(
	struct device *dev, struct device_attribute *attr,
	char *buf)
{
	ssize_t ret = 0;
	int i;
	struct energy_mon_data *p_curr = &energy_mon.discharging_dump;
	long long avg_curr;
	__kernel_time_t diff_boot;
	unsigned int scan_req = 0;
	struct timespec scan_time = {0,};
	ktime_t now = ktime_get();

	energy_monitor_marker(ENERGY_MON_TYPE_DUMP);
	energy_monitor_inject_data(ENERGY_MON_TYPE_DUMP, NULL, NULL);

	diff_boot = p_curr->ts_boot.tv_sec;
	avg_curr = abs(energy_mon.unit_bat_capacity * p_curr->bat_capacity);
	if (diff_boot)
		do_div(avg_curr, diff_boot);

	for (i = 0; i < NET_STAT_ARRAY_SIZE; i++) {
		if (energy_mon.discharging_dump.net_stat[i].scan_req) {
			scan_req = energy_mon.discharging_dump.net_stat[i].scan_req;
			scan_time = ktime_to_timespec(
				energy_mon.discharging_dump.net_stat[i].scan_time);
		}
	}

	ret += scnprintf(buf, PAGE_SIZE,
			"{\"TIME\":%llu,"
			"\"RUN_TM\":%lu,\"SCRON_TM\":%lu,"
			"\"SCROFF_TM\":%lu,\"BATDISC\":%lld,"
			"\"SCROFF_UPTM\":%lu,\"AOD_TM\":%lu,"
			"\"WFSCN_TM\":%lu,\"WFSCN_CNT\":%u}\n",
			ktime_to_ms(now),
			diff_boot,
			p_curr->ts_disp.tv_sec,
			p_curr->ts_boot.tv_sec - p_curr->ts_disp.tv_sec,
			avg_curr,
			p_curr->ts_kern.tv_sec - p_curr->ts_disp.tv_sec,
			p_curr->ts_aod.tv_sec,
			scan_time.tv_sec, scan_req);

	return ret;
}

static DEVICE_ATTR(batr, 0400,
	energy_monitor_show_batr,
	NULL);

static struct attribute *sec_energy_monitor_attrs[] = {
	&dev_attr_get_last_discharging_info.attr,
	&dev_attr_penalty_score.attr,
	&dev_attr_penalty_threshold.attr,
	&dev_attr_penalty_cause.attr,
	&dev_attr_batr.attr,
	NULL
};

static const struct attribute_group sec_energy_monitor_attr_group = {
	.attrs = sec_energy_monitor_attrs,
};

void energy_monitor_penalty_score_notify(void)
{
	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s\n", __func__);
	sysfs_notify(power_kobj, NULL, "emon_penalty_score");
}
#endif

static int energy_monitor_debug_init(void)
{
	struct dentry *d;
#ifdef CONFIG_SEC_SYSFS
	int err;
#endif

	d = debugfs_create_dir("energy_monitor", NULL);
	if (d) {
		if (!debugfs_create_file("status_raw",
				0600, d, NULL, &status_raw_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s : debugfs_create_file, error\n", "status_raw");
		if (!debugfs_create_file("status_raw2",
				0600, d, NULL, &status_raw2_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s : debugfs_create_file, error\n", "status_raw2");
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
		if (!debugfs_create_file("status_wakeup_time",
				0600, d, NULL, &status_wakeup_time_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s : debugfs_create_file, error\n", "status_wakeup_time");
#endif
#ifdef CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
		if (!debugfs_create_file("sleep_current",
				0600, d, NULL, &sleep_current_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s : debugfs_create_file, error\n", "sleep_current");
#endif
		if (!debugfs_create_file("monitor_interval",
				0600, d, NULL, &monitor_interval_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s : debugfs_create_file, error\n", "monitor_interval");
		if (!debugfs_create_file("logging_interval",
				0600, d, NULL, &logging_interval_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s : debugfs_create_file, error\n", "monitor_interval");

		debugfs_create_u32("debug_level", 0600, d, &debug_level);
		debugfs_create_u32("enable", 0600, d, &energy_monitor_enable);
	}

#ifdef CONFIG_SEC_SYSFS
	sec_energy_monitor = sec_device_create(NULL, "energy_monitor");
	if (IS_ERR(sec_energy_monitor)) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"sec_energy_monitor create fail\n");
		return -ENODEV;
	}

	err = sysfs_create_group(&sec_energy_monitor->kobj,
			&sec_energy_monitor_attr_group);
	if (err < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"sec_energy_monitor_attr create fail\n");
		return err;
	}
#endif

	return 0;
}

#if defined(CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR)
int energy_monitor_sleep_current_estimator(unsigned long event)
{
	struct power_supply *psy = NULL;
	union power_supply_propval value;
	int err = -1;

	static struct timespec ts_susp;
	static int raw_soc_susp;
	static int valid_suspend;

	struct timespec ts_resume;
	struct timespec ts_elapsed;
	static int valid_count;
	long long average;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		valid_suspend = 0;
		psy = power_supply_get_by_name("battery");
		if (!psy) {
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s: cannot find battery power supply\n", __func__);
			break;
		}

		err = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_STATUS, &value);
		if (err) {
			power_supply_put(psy);
			break;
		}
		power_supply_put(psy);

		/* Check only in discharging */
		if (value.intval == POWER_SUPPLY_STATUS_DISCHARGING) {
			if (energy_mon.use_raw_soc) {
				psy = power_supply_get_by_name(energy_mon.ps_raw_soc);
				if (psy) {
					value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
					err = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_CAPACITY, &value);
					if (err < 0)
						value.intval = -1;
					raw_soc_susp = value.intval;

					power_supply_put(psy);
				} else {
					energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
						"%s: cannot find fg power supply\n", __func__);
					raw_soc_susp = -1;
				}
			} else {
				psy = power_supply_get_by_name("battery");
				if (psy) {
					err = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_CAPACITY, &value);
					if (err < 0)
						value.intval = -1;
					raw_soc_susp = value.intval * 100;

					power_supply_put(psy);
				}
			}
			ts_susp = current_kernel_time();
			valid_suspend = 1;
		}
		break;
	case PM_POST_SUSPEND:
		if (energy_mon.use_raw_soc) {
			psy = power_supply_get_by_name(energy_mon.ps_raw_soc);
			if (psy) {
				value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
				err = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CAPACITY, &value);
				if (err < 0)
					value.intval = -1;

				power_supply_put(psy);
			}
		} else {
			psy = power_supply_get_by_name("battery");
			if (psy) {
				err = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CAPACITY, &value);
				if (err < 0)
					value.intval = -1;

				power_supply_put(psy);
			}
		}
		ts_resume = current_kernel_time();

		/* Calculate elapsed time and power consumption */
		ts_elapsed = timespec_sub(ts_resume, ts_susp);

		if (valid_suspend &&
			ts_elapsed.tv_sec > MIN_SLEEP_TIME_S &&
			raw_soc_susp - value.intval > 0) {
			average = (raw_soc_susp - value.intval) *
				energy_mon.unit_bat_capacity;

			valid_count++;
			do_div(average, ts_elapsed.tv_sec);

			/* Save only first 20 results */
			if (energy_mon.estimator_index < ENERGY_MON_MAX_SLEEP_ESTIMATOR_CNT)
				energy_mon.estimator_average[energy_mon.estimator_index++] = average;

			energy_mon_dbg(ENERGY_MON_DEBUG_SLEEP_ESTIMATOR,
				"%s: %3ld.%02ldmA from %u to %u for %lus\n",
				__func__, (long)average/100,
				(long)average%100, raw_soc_susp,
				value.intval, ts_elapsed.tv_sec);
		}

#ifdef CONFIG_HW_SUSPEND_ENERGY_ESTIMATOR
		if (ts_elapsed.tv_sec > 8) {
			psy = power_supply_get_by_name(energy_mon.ps_hw_suspend_energy);
			if (psy) {
				if (valid_suspend) {
					value.intval = POWER_SUPPLY_CURRENT_SUSPEND_DISCHARGING;
					err = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_CURRENT_SUSPEND, &value);
				} else {
					value.intval = POWER_SUPPLY_CURRENT_SUSPEND_CHARGING;
					err = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_CURRENT_SUSPEND, &value);
				}
				power_supply_put(psy);

				if (value.intval != 0) {
					energy_mon.current_suspend_cnt++;
					energy_mon.current_suspend = (energy_mon.current_suspend +
						value.intval) / energy_mon.current_suspend_cnt;

					energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
						"%s: %d: %3d.%02dmA for %lus\n",
						__func__, energy_mon.current_suspend_cnt,
						energy_mon.current_suspend, 0, ts_elapsed.tv_sec);
				} else
					energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
						"%s: suspend current read fail\n", __func__);
			} else
				energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
					"%s: no power supply for current suspend\n", __func__);
		}else {
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: elapse time too short for current suspend\n", __func__);
		}
#endif
		break;
	}
	return 0;
}
#endif /* CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR */

#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
static unsigned int energy_monitor_find_wakeup_stat_wakeup_index(
						int last_wakeup_idx)
{
	unsigned int index;

	if (last_wakeup_idx == ENERGY_MON_WAKEUP_SSP &&
			sensorhub_stat_is_wristup_wakeup())
		index = ENERGY_MON_WAKEUP_WU;
	else if (last_wakeup_idx == ENERGY_MON_WAKEUP_BT &&
			sap_stat_is_noti_wakeup())
		index = ENERGY_MON_WAKEUP_NOTI;
	else
		index = last_wakeup_idx;

	return index;
}

static unsigned int energy_monitor_find_wakeup_stat_time_index(
						ktime_t time)
{
	unsigned int index;
	s64 time_sec;

	time_sec = ktime_to_ms(time) / MSEC_PER_SEC;
	if (time_sec > ENERGY_MON_MAX_WAKEUP_STAT_TIME)
		index = ENERGY_MON_MAX_WAKEUP_STAT_TIME + 1;
	else
		index = (unsigned int)time_sec;

	return index;
}

static int energy_monitor_update_wakeup_stat(unsigned long event)
{
	ktime_t now, delta;
	unsigned int wakeup_idx, time_idx;
	int last_wakeup_idx = energy_mon.last_wakeup_idx;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		if (energy_mon.last_wakeup_idx < ENERGY_MON_WAKEUP_MAX &&
				energy_mon.last_wakeup_idx >= 0) {
			now = ktime_get();
			delta = ktime_sub(now, energy_mon.last_wakeup_time);

			wakeup_idx =
				energy_monitor_find_wakeup_stat_wakeup_index(last_wakeup_idx);

			energy_mon.wakeup_time[wakeup_idx] =
				ktime_add(energy_mon.wakeup_time[wakeup_idx], delta);

			energy_mon_dbg(ENERGY_MON_DEBUG_WAKEUP_STAT,
				"%s for %lld (%d)\n",
				energy_mononitor_get_ws_name_string(wakeup_idx),
				ktime_to_ms(delta),
				suspend_stats.success);

			if (delta.tv64 >= 0) {
				time_idx =
					energy_monitor_find_wakeup_stat_time_index(delta);

				energy_mon.wakeup_time_stats[wakeup_idx][time_idx]++;

				energy_mon_dbg(ENERGY_MON_DEBUG_WAKEUP_STAT,
					"ms=%lld: wakeup_idx=%d %d: time_idx=%d\n",
					ktime_to_ms(delta),
					last_wakeup_idx, wakeup_idx,
					time_idx);
			}

		}
		energy_mon.last_wakeup_idx = -1;
		break;

	case PM_POST_SUSPEND:
		energy_mon.last_wakeup_time = ktime_get();
		break;
	}

	return 0;
}
#endif

static int energy_monitor_pm_notifier(struct notifier_block *nb,
				unsigned long event, void *unused)
{

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: event=%lu\n", __func__, event);

	/* If monitor interval is 0, do nothing */
	if (monitor_interval) {
		switch (event) {
		case PM_SUSPEND_PREPARE:
			energy_monitor_background_marker();
			cancel_delayed_work(&monitor_work);
			break;

		case PM_POST_SUSPEND:
			schedule_delayed_work(&monitor_work, monitor_interval * HZ);
			break;
		}
	}

#if defined(CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR)
	energy_monitor_sleep_current_estimator(event);
#endif

#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
	energy_monitor_update_wakeup_stat(event);
#endif

	return NOTIFY_DONE;
}

static struct notifier_block energy_monitor_notifier_block = {
	.notifier_call = energy_monitor_pm_notifier,
	.priority = 0,
};

static void energy_monitor_work(struct work_struct *work)
{
	static int thread_cnt;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: (count=%d)\n", __func__, thread_cnt++);

	/* Call background marker when logging interval is setted */
	if (logging_interval)
		energy_monitor_background_marker();

	if (monitor_interval)
		schedule_delayed_work(&monitor_work, monitor_interval * HZ);
	else
		cancel_delayed_work(&monitor_work);
}

static int __init energy_monitor_dt_get_etc_power_value(void)
{
	struct device_node *root;

	root = of_find_node_by_path("/tizen_energy_monitor/power_value");
	if (!root) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s:missing power_value in DT\n",
			__func__);
		goto fail;
	}

	/* get power value of ff */
	if (of_property_read_u32(root, "ff",
			&energy_mon.power_value.ff))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,ff in DT\n");

	/* get power value of gps */
	if (of_property_read_u32(root, "gps",
			&energy_mon.power_value.gps))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,gps in DT\n");

	/* get power value of hr */
	if (of_property_read_u32(root, "hr",
			&energy_mon.power_value.hr))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,hr in DT\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: %u %u %u\n",
		__func__, energy_mon.power_value.ff,
		energy_mon.power_value.gps, energy_mon.power_value.hr);

	return 0;
fail:
	energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: parsing fail\n", __func__);

	return -1;
}

static int __init energy_monitor_dt_get_wifi_power_value(void)
{
	struct device_node *root;

	/* alloc memory for power_value.cpu_active */
	root = of_find_node_by_path("/tizen_energy_monitor/power_value/wifi");
	if (!root) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s:missing power_value,wifi in DT\n",
			__func__);
		goto fail;
	}

	/* get power value of wifi scan */
	if (of_property_read_u32(root, "scan",
			&energy_mon.power_value.wifi_scan))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,wifi,scan in DT\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: %u\n",
		__func__, energy_mon.power_value.wifi_scan);

	return 0;
fail:
	energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: parsing fail\n", __func__);

	return -1;
}

static int __init energy_monitor_dt_get_bt_power_value(void)
{
	struct device_node *root;

	/* alloc memory for power_value.cpu_active */
	root = of_find_node_by_path("/tizen_energy_monitor/power_value/bt");
	if (!root) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s:missing power_value,bt in DT\n",
			__func__);
		goto fail;
	}

	/* get power value of bt */
	if (of_property_read_u32(root, "active",
			&energy_mon.power_value.bt_active))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,bt,active in DT\n");

	if (of_property_read_u32(root, "tx",
			&energy_mon.power_value.bt_tx))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,bt,tx in DT\n");

	if (of_property_read_u32(root, "rx",
			&energy_mon.power_value.bt_rx))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,bt,rx in DT\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: %u %u %d\n",
		__func__,
		energy_mon.power_value.bt_active,
		energy_mon.power_value.bt_tx,
		energy_mon.power_value.bt_rx);

	return 0;
fail:
	energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: parsing fail\n", __func__);

	return -1;
}

static int __init energy_monitor_dt_get_gpu_power_value(void)
{
	struct device_node *root;
	int i;
	int size, num_speed;
	unsigned int *temp_array = NULL;
	unsigned int alloc_size;

	/* alloc memory for power_value.cpu_active */
	root = of_find_node_by_path("/tizen_energy_monitor/power_value/gpu");
	if (!root) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s:missing power_value,gpu in DT\n",
			__func__);
		goto alloc_fail;
	}

	size = of_property_count_u32_elems(root, "active");
	if (size < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: missing power_value,gpu,active in DT\n",
			__func__);
		goto alloc_fail;
	}
	energy_mon.power_value.gpu_active_table_size = size;

	alloc_size = size * sizeof(struct active_power_value);
	energy_mon.power_value.gpu_active = kzalloc(alloc_size, GFP_KERNEL);
	if (!energy_mon.power_value.gpu_active)
		goto alloc_fail;

	/* alloc temp array for cpu active */
	alloc_size = size * sizeof(unsigned int);
	temp_array = kzalloc(alloc_size, GFP_KERNEL);
	if (!temp_array)
		goto fail;

	/* get power value for gpu active */
	num_speed = of_property_count_u32_elems(root, "speed");
	if (num_speed < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: missing power_value,gpu,speed in DT\n",
			__func__);
		goto fail;
	}
	if (energy_mon.power_value.gpu_active_table_size != num_speed) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: gpu,speed count does not match gpu,active count\n",
			__func__);
		goto fail;
	}

	if (!of_property_read_u32_array(root, "active", temp_array, size)) {
		for (i = 0; i < size; i++) {
			energy_mon.power_value.gpu_active[i].value = temp_array[i];
			energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
				"%s: power_value.gpu_active[%d].value=%d\n",
				__func__, i, energy_mon.power_value.gpu_active[i].value);
		}
	} else
		goto fail;

	if (!of_property_read_u32_array(root, "speed", temp_array, num_speed)) {
		for (i = 0; i < num_speed; i++) {
			energy_mon.power_value.gpu_active[i].speed = temp_array[i];
			energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
				"%s: power_value.gpu_active[%d].speed=%d\n",
				__func__, i, energy_mon.power_value.gpu_active[i].speed);
		}
	} else
		goto fail;

	kfree(temp_array);

	return 0;
fail:
	kfree(energy_mon.power_value.gpu_active);
	kfree(temp_array);
alloc_fail:
	energy_mon.power_value.gpu_active_table_size = 0;
	energy_mon.power_value.gpu_active = NULL;
	energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: parsing fail\n", __func__);

	return -1;
}

static int __init energy_monitor_dt_get_cpu_power_value(void)
{
	struct device_node *root;
	int i;
	int active_size, whitelist_size, num_speed;
	unsigned int *temp_array = NULL;
	unsigned int alloc_size;

	/* alloc memory for power_value.cpu_active */
	root = of_find_node_by_path("/tizen_energy_monitor/power_value/cpu");
	if (!root) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s:missing power_value,cpu in DT\n",
			__func__);
		goto active_alloc_fail;
	}

	active_size = of_property_count_u32_elems(root, "active");
	if (active_size < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: missing power_value,cpu,active in DT\n",
			__func__);
		goto active_alloc_fail;
	}
	energy_mon.power_value.cpu_active_table_size = active_size;

	alloc_size = active_size * sizeof(struct active_power_value);
	energy_mon.power_value.cpu_active = kzalloc(alloc_size, GFP_KERNEL);
	if (!energy_mon.power_value.cpu_active)
		goto active_alloc_fail;

	/* alloc array for cpu white list */
	whitelist_size = of_property_count_u32_elems(root, "whitelist");
	if (whitelist_size < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: missing power_value,cpu,whitelist in DT\n",
			__func__);
		goto active_alloc_fail;
	}
	energy_mon.power_value.cpu_whitelist_table_size = whitelist_size;

	alloc_size = whitelist_size * sizeof(unsigned int);
	energy_mon.power_value.cpu_whitelist = kzalloc(alloc_size, GFP_KERNEL);
	if (!energy_mon.power_value.cpu_whitelist)
		goto whitelist_alloc_fail;

	alloc_size = whitelist_size * sizeof(unsigned int);
	temp_array = kzalloc(alloc_size, GFP_KERNEL);
	if (!temp_array)
		goto fail;

	if (!of_property_read_u32_array(root, "whitelist",
					temp_array, whitelist_size)) {
		for (i = 0; i < whitelist_size; i++) {
			energy_mon.power_value.cpu_whitelist[i] = temp_array[i];
			energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
				"%s: power_value.cpu,whitelist[%d]=%d\n",
				__func__, i, energy_mon.power_value.cpu_whitelist[i]);
		}
	}
	else
		goto fail;
	kfree(temp_array);


	/* get power value of cpu idle */
	if (of_property_read_u32(root, "idle",
			&energy_mon.power_value.cpu_idle))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,cpu,idle in DT\n");

	if (of_property_read_u32(root, "lpm",
			&energy_mon.power_value.cpu_lpm))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,cpu,lpm in DT\n");

	if (of_property_read_u32(root, "awake",
			&energy_mon.power_value.cpu_awake))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matchingpower_value,cpu,awake in DT\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: %u %u %u\n",
		__func__, energy_mon.power_value.cpu_idle,
		energy_mon.power_value.cpu_lpm, energy_mon.power_value.cpu_awake);

	/* alloc temp array for cpu active */
	alloc_size = active_size * sizeof(unsigned int);
	temp_array = kzalloc(alloc_size, GFP_KERNEL);
	if (!temp_array)
		goto fail;

	/* get power value for cpu active */
	num_speed = of_property_count_u32_elems(root, "speed");
	if (num_speed < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: missing power_value,cpu,speed in DT\n",
			__func__);
		goto fail;
	}
	if (energy_mon.power_value.cpu_active_table_size != num_speed) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: cpu_speed count does not match cpu_active count\n",
			__func__);
		goto fail;
	}

	if (!of_property_read_u32_array(root, "active",
					temp_array, active_size)) {
		for (i = 0; i < active_size; i++) {
			energy_mon.power_value.cpu_active[i].value = temp_array[i];
			energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
				"%s: power_value.cpu_active[%d].value=%d\n",
				__func__, i, energy_mon.power_value.cpu_active[i].value);
		}
	} else
		goto fail;

	if (!of_property_read_u32_array(root, "speed",
					temp_array, num_speed)) {
		for (i = 0; i < num_speed; i++) {
			energy_mon.power_value.cpu_active[i].speed = temp_array[i];
			energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
				"%s: power_value.cpu_active[%d].speed=%d\n",
				__func__, i, energy_mon.power_value.cpu_active[i].speed);
		}
	} else
		goto fail;

	kfree(temp_array);

	return 0;
fail:
	kfree(energy_mon.power_value.cpu_whitelist);
whitelist_alloc_fail:
	kfree(energy_mon.power_value.cpu_active);
active_alloc_fail:
	kfree(temp_array);
	energy_mon.power_value.cpu_active_table_size = 0;
	energy_mon.power_value.cpu_active = NULL;
	energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: parsing fail\n", __func__);

	return -1;
}

static int __init energy_monitor_dt_get_disp_power_value(void)
{
	struct device_node *root;
	int i;
	int size, num_br;
	unsigned int *temp_array = NULL;
	unsigned int alloc_size;

	/* alloc memory for power_value.disp.on */
	root = of_find_node_by_path("/tizen_energy_monitor/power_value/disp");
	if (!root) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s:missing power_value,disp in DT\n",
			__func__);
		goto alloc_fail;
	}

	size = of_property_count_u32_elems(root, "on");
	if (size < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: missing power_value,disp,on in DT\n",
			__func__);
		goto alloc_fail;
	}
	energy_mon.power_value.disp_on_table_size = size;

	alloc_size = size * sizeof(struct disp_power_value);
	energy_mon.power_value.disp_on = kzalloc(alloc_size, GFP_KERNEL);
	if (!energy_mon.power_value.disp_on)
		goto alloc_fail;

	/* alloc temp array for disp on */
	alloc_size = size * sizeof(unsigned int);
	temp_array = kzalloc(alloc_size, GFP_KERNEL);
	if (!temp_array)
		goto fail;

	/* get power value for disp on */
	num_br = of_property_count_u32_elems(root, "brightness");
	if (num_br < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: missing power_value,disp,brightness in DT\n",
			__func__);
		goto fail;
	}
	if (energy_mon.power_value.disp_on_table_size != num_br) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: disp,brightness count does not match disp,on count\n",
			__func__);
		goto fail;
	}

	if (!of_property_read_u32_array(root, "on", temp_array, size)) {
		for (i = 0; i < size; i++) {
			energy_mon.power_value.disp_on[i].value = temp_array[i];
			energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
				"%s: power_value.disp_on[%d].value=%d\n",
				__func__, i, energy_mon.power_value.disp_on[i].value);
		}
	} else
		goto fail;

	if (!of_property_read_u32_array(root, "brightness", temp_array, num_br)) {
		for (i = 0; i < num_br; i++) {
			energy_mon.power_value.disp_on[i].brightness = temp_array[i];
			energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
				"%s: power_value.disp_on[%d].brightness=%d\n",
				__func__, i, energy_mon.power_value.disp_on[i].brightness);
		}
	} else
		goto fail;

	kfree(temp_array);

	/* get power value of aod */
	if (of_property_read_u32(root, "aod",
			&energy_mon.power_value.disp_aod))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching power_value,disp,aod in DT\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: %u\n",
		__func__, energy_mon.power_value.disp_aod);

	return 0;
fail:
	kfree(energy_mon.power_value.disp_on);
	kfree(temp_array);
alloc_fail:
	energy_mon.power_value.disp_on_table_size = 0;
	energy_mon.power_value.disp_on = NULL;
	energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: parsing fail\n", __func__);

	return -1;
}

#define MAX_IRQ_TABLE 16
static int __init energy_monitor_dt_get_irq_map_table(
	struct device_node *np)
{
	struct device_node *root;
	int i;
	int size, idx_size;
	const char *temp_irq_name[MAX_IRQ_TABLE];
	int temp_wakeup_idx[MAX_IRQ_TABLE];
	unsigned int alloc_size;

	/* alloc memory for irq_map_table */
	root = of_find_node_by_name(np, "irq_map_table");
	if (!root) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s:missing irq_map_table in DT\n",
			__func__);
		goto alloc_fail;
	}

	size = of_property_count_strings(root, "irq-name");
	if (size < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: missing irq_map_table,irq-name in DT\n",
			__func__);
		goto alloc_fail;
	}
	if (size > MAX_IRQ_TABLE) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: irq_map_table count reach the max count which allowd\n",
			__func__);
		goto alloc_fail;
	}
	energy_mon.irq_map_table_size = size;

	alloc_size = size * sizeof(struct energy_mon_irq_map_table);
	energy_mon.irq_map_table = kzalloc(alloc_size, GFP_KERNEL);
	if (!energy_mon.irq_map_table)
		goto alloc_fail;

	/* get irq name */
	of_property_read_string_array(root, "irq-name", temp_irq_name, size);
	for (i = 0; i < size; i++) {
		energy_mon.irq_map_table[i].irq_name = temp_irq_name[i];
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"%s: irq_name[%d]=%s\n", __func__,
			i, energy_mon.irq_map_table[i].irq_name);
	}

	/* get wakeup index */
	idx_size = of_property_count_u32_elems(root, "wakeup-idx");
	if (idx_size < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: missing irq_map_table,wakeup-idx in DT\n",
			__func__);
		goto fail;
	}
	if (size != idx_size) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
			"%s: irq-name count does not match wakeup-idx count\n",
			__func__);
		goto fail;
	}

	if (!of_property_read_u32_array(root, "wakeup-idx",
					temp_wakeup_idx, idx_size)) {
		for (i = 0; i < idx_size; i++) {
			energy_mon.irq_map_table[i].wakeup_idx = temp_wakeup_idx[i];
			energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
				"%s: wakeup-idx[%d]=%d\n",
				__func__, i, energy_mon.irq_map_table[i].wakeup_idx);
		}
	} else
		goto fail;

	return 0;
fail:
	kfree(energy_mon.irq_map_table);
alloc_fail:
	energy_mon.irq_map_table_size = 0;
	energy_mon.irq_map_table = NULL;
	energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: parsing fail\n", __func__);

	return -1;
}

static int __init energy_monitor_dt_init(void)
{
	struct device_node *np = of_find_node_by_name(NULL, "tizen_energy_monitor");
	int unit_bat_capacity = 0;

	if (of_property_read_s32(np, "unit_bat_capacity",
			&unit_bat_capacity)) {
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"unit_bat_capacity is mandatory for energy monitor\n");
		energy_mon.unit_bat_capacity = DEFAULT_UNIT_BATTERY_mAs;
	} else
		energy_mon.unit_bat_capacity = (long long)unit_bat_capacity;

	if (of_property_read_s32(np, "use_raw_soc",
			&energy_mon.use_raw_soc))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: use_raw_soc\n");

	if (of_property_read_string(np, "ps_raw_soc",
			(char const **)&energy_mon.ps_raw_soc))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: ps_raw_soc\n");
		energy_mon.ps_raw_soc = "sec-fuelgauge";

	if (of_property_read_string(np, "ps_hw_suspend_energy",
			(char const **)&energy_mon.ps_hw_suspend_energy))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: ps_hw_suspend_energy\n");
		energy_mon.ps_hw_suspend_energy = "sec-fuelgauge";

	if (of_property_read_s32(np, "threshold_time",
			&energy_mon.penalty.threshold_time))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_time\n");

	if (of_property_read_s32(np, "threshold_batt",
			&energy_mon.penalty.threshold_batt))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_batt\n");

	if (of_property_read_s32(np, "threshold_disp",
			&energy_mon.penalty.threshold_disp))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_disp\n");

	if (of_property_read_s32(np, "threshold_gps",
			&energy_mon.penalty.threshold_gps))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_gps\n");

	if (of_property_read_s32(np, "threshold_lbs",
			&energy_mon.penalty.threshold_lbs))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_lbs\n");

	if (of_property_read_s32(np, "threshold_cpuidle",
			&energy_mon.penalty.threshold_cpuidle))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_cpuidle\n");

	if (of_property_read_s32(np, "threshold_cpufreq",
			&energy_mon.penalty.threshold_cpufreq))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_cpufreq\n");

	if (of_property_read_s32(np, "threshold_gpufreq",
			&energy_mon.penalty.threshold_gpufreq))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_gpufreq\n");

	if (of_property_read_s32(np, "threshold_ws",
			&energy_mon.penalty.threshold_ws))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_ws\n");

	if (of_property_read_s32(np, "threshold_slwl",
			&energy_mon.penalty.threshold_slwl))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_slwl\n");

	if (of_property_read_s32(np, "threshold_sid",
			&energy_mon.penalty.threshold_sid))
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
			"No matching property: threshold_sid\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO,
		"%s: %lld %d %s %s %d %d %d %d %d %d %d %d %d %d %d\n", __func__,
		energy_mon.unit_bat_capacity,
		energy_mon.use_raw_soc,
		energy_mon.ps_raw_soc,
		energy_mon.ps_hw_suspend_energy,
		energy_mon.penalty.threshold_time,
		energy_mon.penalty.threshold_batt,
		energy_mon.penalty.threshold_disp,
		energy_mon.penalty.threshold_gps,
		energy_mon.penalty.threshold_lbs,
		energy_mon.penalty.threshold_cpuidle,
		energy_mon.penalty.threshold_cpufreq,
		energy_mon.penalty.threshold_cpufreq,
		energy_mon.penalty.threshold_ws,
		energy_mon.penalty.threshold_slwl,
		energy_mon.penalty.threshold_sid);

	energy_monitor_dt_get_irq_map_table(np);
	energy_monitor_dt_get_disp_power_value();
	energy_monitor_dt_get_cpu_power_value();
	energy_monitor_dt_get_gpu_power_value();
	energy_monitor_dt_get_wifi_power_value();
	energy_monitor_dt_get_bt_power_value();
	energy_monitor_dt_get_etc_power_value();

	return 0;
}

static int __init energy_monitor_init(void)
{
	int ret;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s\n", __func__);

	ret = energy_monitor_dt_init();
	if (ret)
		return ret;

	/* Initialize datas */
	for (i = 0; i < ENERGY_MON_HISTORY_NUM ; i++)
		energy_mon.data[i].log_count = -1;

	energy_mon.last_wakeup_idx = -1;

	/* Check size of control block for information */
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: sizeof(struct energy_mon_data) is %lu\n",
		__func__, sizeof(struct energy_mon_data));
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: sizeof(energy_mon) is %lu(ENERGY_MON_HISTORY_NUM=%d)\n",
		__func__, sizeof(energy_mon), ENERGY_MON_HISTORY_NUM);

	energy_monitor_debug_init();

	register_pm_notifier(&energy_monitor_notifier_block);

	INIT_DELAYED_WORK(&monitor_work, energy_monitor_work);
	if (monitor_interval)
		schedule_delayed_work(&monitor_work, monitor_interval * HZ);

	return 0;
}

static void energy_monitor_exit(void)
{
	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s\n", __func__);
}

late_initcall(energy_monitor_init);
module_exit(energy_monitor_exit);
