/*
 * kernel/power/autosleep.c
 *
 * Opportunistic sleep support.
 *
 * Copyright (C) 2012 Rafael J. Wysocki <rjw@sisk.pl>
 */

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#ifdef CONFIG_PM_SLEEP_HISTORY
#include <linux/power/sleep_history.h>
#endif
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/slp_mon_ws_dev.h>
#include <linux/power/sleep_monitor.h>
#endif

#include "power.h"

static suspend_state_t autosleep_state;
static struct workqueue_struct *autosleep_wq;
/*
 * Note: it is only safe to mutex_lock(&autosleep_lock) if a wakeup_source
 * is active, otherwise a deadlock with try_to_suspend() is possible.
 * Alternatively mutex_lock_interruptible() can be used.  This will then fail
 * if an auto_sleep cycle tries to freeze processes.
 */
static DEFINE_MUTEX(autosleep_lock);
static struct wakeup_source *autosleep_ws;

static void try_to_suspend(struct work_struct *work)
{
	unsigned int initial_count, final_count;
	int error = 0;

#ifdef CONFIG_PM_SLEEP_HISTORY
	static unsigned int autosleep_active;
	struct timespec ts;

	mutex_lock(&autosleep_lock);
	if (autosleep_active == 0) {
		autosleep_active = 1;
		ts = current_kernel_time();
		sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_ENTRY, &ts, NULL, 0, NULL);
	}
	mutex_unlock(&autosleep_lock);
#endif
	pr_info("PM: %s: entry\n", __func__);

	if (!pm_get_wakeup_count(&initial_count, true)) {
		pr_info("PM: %s: in_progress_count is not zero\n", __func__);
		goto out;
	}

	mutex_lock(&autosleep_lock);

	if (!pm_save_wakeup_count(initial_count) ||
		system_state != SYSTEM_RUNNING) {
		mutex_unlock(&autosleep_lock);
		pr_info("PM: %s: can't not save initial_count\n", __func__);
		goto out;
	}

#ifdef CONFIG_PM_SLEEP_HISTORY
	autosleep_active = 0;
	ts = current_kernel_time();
	sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_EXIT, &ts, autosleep_ws, 0, NULL);
#endif
#ifdef CONFIG_SLEEP_MONITOR
{
	char slp_mon_ws_name[SLEEP_MON_WS_ARRAY_SIZE][SLEEP_MON_WS_NAME_LENGTH] = {{0,},};
	static ktime_t slp_mon_ws_prv_t[SLEEP_MON_WS_ARRAY_SIZE];
	int i = 0;
	int ret = 0;

	for (i = 0; i < SLEEP_MON_WS_ARRAY_SIZE; i++)
		slp_mon_ws_prv_t[i] = ktime_set(0, 0);

	ret = sleep_monitor_wakeup_sources(slp_mon_ws_name, slp_mon_ws_prv_t);
	init_ws_data();

	if (ret) {
		for (i = 0; i < ret; i++) {
			if (slp_mon_ws_name[i] && (ktime_to_ms(slp_mon_ws_prv_t[i]) != 0)) {
				add_slp_mon_ws_list(slp_mon_ws_name[i], slp_mon_ws_prv_t[i]);
				pr_info("[slp_mon] %s: ws_name: %s ws_prv_t: %lld\n",
						__func__, slp_mon_ws_name[i], ktime_to_ms(slp_mon_ws_prv_t[i]));
			}
		}
		pm_del_prevent_sleep_time();
	} else {
		if (ktime_to_ms(autosleep_ws->prevent_time) != 0) {
			strncpy(slp_mon_ws_name[0], autosleep_ws->name, SLEEP_MON_WS_NAME_LENGTH);
			slp_mon_ws_name[0][SLEEP_MON_WS_NAME_LENGTH - 1] = 0;
			add_slp_mon_ws_list(slp_mon_ws_name[0], autosleep_ws->prevent_time);
		}
	}

	pm_del_prevent_sleep_time();
}
#endif
	if (autosleep_state == PM_SUSPEND_ON) {
		mutex_unlock(&autosleep_lock);
		pr_info("PM: %s: autosleep_state is PM_SUSPEND_ON\n", __func__);
		return;
	}
	if (autosleep_state >= PM_SUSPEND_MAX)
		hibernate();
	else
		error = pm_suspend(autosleep_state);

	mutex_unlock(&autosleep_lock);

	if (error)
		goto out;

#ifdef CONFIG_PM_SLEEP_HISTORY
	mutex_lock(&autosleep_lock);
	if (autosleep_active == 0) {
		autosleep_active = 1;
		ts = current_kernel_time();
		sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_ENTRY, &ts, NULL, 0, NULL);
	}
	mutex_unlock(&autosleep_lock);
#endif

	if (!pm_get_wakeup_count(&final_count, false)) {
		pr_info("PM: %s: in_progress_count is not zero\n", __func__);
		goto out;
	}

	/*
	 * If the wakeup occured for an unknown reason, wait to prevent the
	 * system from trying to suspend and waking up in a tight loop.
	 */
	if (final_count == initial_count)
		schedule_timeout_uninterruptible(HZ / 2);

 out:
#ifdef CONFIG_PM_SLEEP_HISTORY
	mutex_lock(&autosleep_lock);
	if (autosleep_state == PM_SUSPEND_ON) {
		autosleep_active = 0;
		ts = current_kernel_time();
		sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_EXIT, &ts, autosleep_ws, 0, NULL);
	}
	mutex_unlock(&autosleep_lock);
#endif
	/*
	 * If the device failed to suspend, wait to prevent the
	 * system from trying to suspend and waking up in a tight loop.
	 */
	if (error) {
		pr_info("PM: suspend returned(%d)\n", error);
		schedule_timeout_uninterruptible(HZ / 2);
	}
	queue_up_suspend_work();
}

static DECLARE_WORK(suspend_work, try_to_suspend);

void queue_up_suspend_work(void)
{
	if (!work_pending(&suspend_work) && autosleep_state > PM_SUSPEND_ON)
		queue_work_on(0, autosleep_wq, &suspend_work);
}

suspend_state_t pm_autosleep_state(void)
{
	return autosleep_state;
}

int pm_autosleep_lock(void)
{
	return mutex_lock_interruptible(&autosleep_lock);
}

void pm_autosleep_unlock(void)
{
	mutex_unlock(&autosleep_lock);
}

int pm_autosleep_set_state(suspend_state_t state)
{

#ifndef CONFIG_HIBERNATION
	if (state >= PM_SUSPEND_MAX)
		return -EINVAL;
#endif

	__pm_stay_awake(autosleep_ws);

	mutex_lock(&autosleep_lock);

	autosleep_state = state;

	pr_info("PM: %s: state: %d\n", __func__, state);

	if (state >= PM_SUSPEND_MAX || state < 0) {
		WARN_ON(1);
	}

	__pm_relax(autosleep_ws);

	if (state > PM_SUSPEND_ON) {
		pm_wakep_autosleep_enabled(true);
		queue_up_suspend_work();
	} else {
		pm_wakep_autosleep_enabled(false);
	}

	mutex_unlock(&autosleep_lock);
	return 0;
}

int __init pm_autosleep_init(void)
{
	autosleep_ws = wakeup_source_register("autosleep");
	if (!autosleep_ws)
		return -ENOMEM;

	autosleep_wq = alloc_ordered_workqueue("autosleep", 0);
	if (autosleep_wq)
		return 0;

	wakeup_source_unregister(autosleep_ws);
	return -ENOMEM;
}
