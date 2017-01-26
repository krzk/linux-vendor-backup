#ifndef _ENERGY_SUMMARY_H
#define _ENERGY_SUMMARY_H

enum energy_mon_type {
	ENERGY_MON_TYPE_BOOTING = 0,
	ENERGY_MON_TYPE_BATTERY,
	ENERGY_MON_TYPE_MONITOR,
	/* Add here */
	ENERGY_MON_TYPE_DUMP,


	ENERGY_MON_TYPE_CHARGING,
	ENERGY_MON_TYPE_DISCHARGING,
	ENERGY_MON_TYPE_MAX
};

enum energy_mon_wakeup_source {
	ENERGY_MON_WAKEUP_INPUT = 0,
	ENERGY_MON_WAKEUP_SSP = 1,
	ENERGY_MON_WAKEUP_RTC = 2,
	ENERGY_MON_WAKEUP_BT = 3,
	ENERGY_MON_WAKEUP_WIFI = 4,
	ENERGY_MON_WAKEUP_CP = 5,

	ENERGY_MON_WAKEUP_MAX
};

enum energy_mon_state {
	ENERGY_MON_STATE_CHARGING = 0,
	ENERGY_MON_STATE_DISCHARGING = 1,
	/* Add here */

	ENERGY_MON_STATE_UNKNOWN = 2,
};

enum energy_mon_disp_type {
	ENERGY_MON_DISP_ON = 0,
	ENERGY_MON_DISP_OFF = 1,
	/* Add here */

	ENERGY_MON_DISP_MAX,
};




#ifdef CONFIG_ENERGY_MONITOR
extern int energy_monitor_marker(int type, int state);
extern int energy_monitor_record_disp_time(int type);
extern int energy_monitor_record_wakeup_reason(int irq, char *irq_name);
extern int energy_monitor_get_last_wakeup(void);
extern bool energy_monitor_is_last_wakeup_by_wifi(void);



#else
static inline int energy_monitor_marker(int type, int state) {}
static inline int energy_monitor_record_disp_time(int type) {}
static inline int energy_monitor_record_wakeup_reason(int irq, char *irq_name) {}
static extern int energy_monitor_get_last_wakeup(void) {}
static extern bool energy_monitor_is_last_wakeup_by_wifi(void) {}

#endif


#endif
