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

#include "power.h"

static int autosleep_state_banner = 0x4b4f4f47;
static suspend_state_t autosleep_state_before = 0x5a5a0000;
static suspend_state_t autosleep_state_1=100;
static suspend_state_t autosleep_state=100;
static suspend_state_t autosleep_state_2=100;
static suspend_state_t autosleep_state_after = 0xa5a50000;

static struct workqueue_struct *autosleep_wq;
/*
 * Note: it is only safe to mutex_lock(&autosleep_lock) if a wakeup_source
 * is active, otherwise a deadlock with try_to_suspend() is possible.
 * Alternatively mutex_lock_interruptible() can be used.  This will then fail
 * if an auto_sleep cycle tries to freeze processes.
 */
static DEFINE_MUTEX(autosleep_lock);
static struct wakeup_source *autosleep_ws;


// TODO: Need to remove. This is to debug memory corruption issue
void temp_func(void)
{
	printk("%s(%08x) %d(%08x, %d, %d, %d, %08x)\n", __func__, autosleep_state_banner, 0,
		autosleep_state_before, autosleep_state_1, autosleep_state, autosleep_state_2, autosleep_state_after);

	return;
}



static void try_to_suspend(struct work_struct *work)
{
	unsigned int initial_count, final_count;
	int error = 0;
#ifdef CONFIG_PM_SLEEP_HISTORY
	int i;
	static unsigned int autosleep_active;
	static struct wakeup_source *last_ws[4];
	struct timespec ts;

	if (autosleep_active == 0) {
		autosleep_active = 1;
		ts = current_kernel_time();
		sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_ENTRY,
			&ts, NULL);
	}
#endif
	if (!pm_get_wakeup_count(&initial_count, true))
		goto out;

	mutex_lock(&autosleep_lock);

	if (!pm_save_wakeup_count(initial_count)) {
		mutex_unlock(&autosleep_lock);
		goto out;
	}

#ifdef CONFIG_PM_SLEEP_HISTORY
	memset(last_ws, 0, sizeof(last_ws));
	pm_get_last_wakeup_sources(&last_ws[0],
		sizeof(last_ws)/sizeof(struct wakeup_source *));

	autosleep_active = 0;
	ts=current_kernel_time();
	if (last_ws[0]) {
		sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_EXIT,
			&ts, last_ws[0]);

		for (i = 1;  i < sizeof(last_ws)/sizeof(struct wakeup_source *); i++) {
			if (last_ws[i])
				sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_EXIT,
					NULL, last_ws[i]);
		}
		memset(last_ws, 0, sizeof(last_ws));
		pm_del_prevent_sleep_time();
	} else
		sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_EXIT,
			&ts, autosleep_ws);
#endif
	if (autosleep_state == PM_SUSPEND_ON) {
		mutex_unlock(&autosleep_lock);
		return;
	}
	if (autosleep_state >= PM_SUSPEND_MAX)
		hibernate();
	else
		error = pm_suspend(autosleep_state);

	mutex_unlock(&autosleep_lock);

#ifdef CONFIG_PM_SLEEP_HISTORY
	if (autosleep_active == 0) {
		autosleep_active = 1;
		ts =current_kernel_time();
		sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_ENTRY,
			&ts, NULL);
	}
#endif
	if (error)
		goto out;

	if (!pm_get_wakeup_count(&final_count, false))
		goto out;

	/*
	 * If the wakeup occured for an unknown reason, wait to prevent the
	 * system from trying to suspend and waking up in a tight loop.
	 */
	if (final_count == initial_count)
		schedule_timeout_uninterruptible(HZ / 2);

 out:
#ifdef CONFIG_PM_SLEEP_HISTORY
	memset(last_ws, 0, sizeof(last_ws));
	pm_get_last_wakeup_sources(&last_ws[0],
		sizeof(last_ws)/sizeof(struct wakeup_source *));

	if (autosleep_state == PM_SUSPEND_ON) {
		autosleep_active = 0;
		ts = current_kernel_time();
		if (last_ws[0]) {
			sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_EXIT,
				&ts, last_ws[0]);

			for (i = 1; i < sizeof(last_ws)/sizeof(struct wakeup_source *); i++) {
				if (last_ws[i])
					sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_EXIT,
						NULL, last_ws[i]);
			}
			memset(last_ws, 0, sizeof(last_ws));
			pm_del_prevent_sleep_time();
		} else
			sleep_history_marker(SLEEP_HISTORY_AUTOSLEEP_EXIT,
				&ts, autosleep_ws);
	}
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

	printk("%s(%08x) %d(%08x, %d, %d, %d, %08x)\n", __func__, autosleep_state_banner, state,
		autosleep_state_before, autosleep_state_1, autosleep_state, autosleep_state_2, autosleep_state_after);
	
	autosleep_state_banner = 0x6b6f6f77;
	autosleep_state_before++;
	autosleep_state_1 = state;
	autosleep_state = state;
	autosleep_state_2 = state;
	autosleep_state_after++;

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
