/* cpufreq_pmqos_wc1b.c */

#include <linux/cpuidle.h>

/* ROTARY BOOSTER + */
#define ROTARY_BOOSTER_DELAY	200
static int rotary_min_cpu_freq = 600000;
static int rotary_min_mif_freq = 200000;
/* ROTARY BOOSTER - */

/* HARD KEY BOOSTER + */
static int hard_key_min_cpu_freq = 1000000;
#define KEY_BOOSTER_DELAY	200
/* HARD KEY BOOSTER - */

/* TOUCH WAKEUP BOOSTER + */
static int touch_wakeup_min_cpu_freq = 1000000;
#define TOUCH_WAKEUP_BOOSTER_DELAY	200
/* TOUCH WAKEUP BOOSTER - */

#define TOUCH_BOOSTER_OFF_TIME		100
#define TOUCH_BOOSTER_CHG_TIME		200

#define TOUCH_BOOSTER_MIF_PRESS		200000
#define TOUCH_BOOSTER_MIF_MOVE		200000
#define TOUCH_BOOSTER_MIF_RELEASE	200000

#define TOUCH_BOOSTER_INT_PRESS		100000
#define TOUCH_BOOSTER_INT_MOVE		100000
#define TOUCH_BOOSTER_INT_RELEASE	100000

static struct pm_qos_request pm_qos_cpu_req;
static struct pm_qos_request pm_qos_cpu_online_req;

static struct pm_qos_request pm_qos_mif_req;
static struct pm_qos_request pm_qos_int_req;

unsigned int press_cpu_freq, release_cpu_freq;
int touch_cpu_online;

unsigned int press_mif_freq = TOUCH_BOOSTER_MIF_PRESS;
unsigned int release_mif_freq = TOUCH_BOOSTER_MIF_RELEASE;

unsigned int press_int_freq = TOUCH_BOOSTER_INT_PRESS;
unsigned int release_int_freq = TOUCH_BOOSTER_INT_RELEASE;


void touch_booster_press_sub(void)
{
	press_cpu_freq = cpufreq_get_touch_boost_press();
	touch_cpu_online = touch_cpu_get_online_min();

	if (!pm_qos_request_active(&pm_qos_cpu_req))
		pm_qos_add_request(&pm_qos_cpu_req
			, PM_QOS_CPU_FREQ_MIN, press_cpu_freq);

	if (!pm_qos_request_active(&pm_qos_mif_req))
		pm_qos_add_request(&pm_qos_mif_req
			, PM_QOS_BUS_THROUGHPUT, press_mif_freq);

	if (!pm_qos_request_active(&pm_qos_int_req))
		pm_qos_add_request(&pm_qos_int_req
			, PM_QOS_DEVICE_THROUGHPUT, press_int_freq);

	if (touch_cpu_online > 1) {
		if (!pm_qos_request_active(&pm_qos_cpu_online_req))
			pm_qos_add_request(&pm_qos_cpu_online_req
				, PM_QOS_CPU_ONLINE_MIN, touch_cpu_online);
	}

	cpuidle_set_w_aftr_enable(0);
}


void touch_booster_move_sub(void)
{
	unsigned int move_cpu_freq = cpufreq_get_touch_boost_move();
	unsigned int move_mif_freq = TOUCH_BOOSTER_MIF_MOVE;
	unsigned int move_int_freq = TOUCH_BOOSTER_INT_MOVE;

	pm_qos_update_request(&pm_qos_cpu_req, move_cpu_freq);
	pm_qos_update_request(&pm_qos_mif_req, move_mif_freq);
	pm_qos_update_request(&pm_qos_int_req, move_int_freq);
}


void touch_booster_release_sub(void)
{
	release_cpu_freq = cpufreq_get_touch_boost_release();

	pm_qos_update_request(&pm_qos_cpu_req, release_cpu_freq);
	pm_qos_update_request(&pm_qos_mif_req, release_mif_freq);
	pm_qos_update_request(&pm_qos_int_req, release_int_freq);
}



void touch_booster_off_sub(void)
{
 	if (pm_qos_request_active(&pm_qos_cpu_req))
		pm_qos_remove_request(&pm_qos_cpu_req);

	if (pm_qos_request_active(&pm_qos_mif_req))
		pm_qos_remove_request(&pm_qos_mif_req);

	if (pm_qos_request_active(&pm_qos_int_req))
		pm_qos_remove_request(&pm_qos_int_req);

	if (pm_qos_request_active(&pm_qos_cpu_online_req))
		pm_qos_remove_request(&pm_qos_cpu_online_req);

	cpuidle_w_after_oneshot_log_en();
	cpuidle_set_w_aftr_enable(1);
}


