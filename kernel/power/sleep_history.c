/*
 * debugfs file to keep track of suspend
 *
 * Copyright (C) 2012 SAMSUNG, Inc.
 * Junho Jang <vincent.jang@samsung.com>
 * Sanghyeon Lee <sirano06.lee@samsung.com>
 * Sangin Lee <sangin78.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/syscore_ops.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/circ_buf.h>
#include <linux/rtc.h>
#include <linux/irq.h>
#include <linux/power_supply.h>
#include <linux/power/sleep_history.h>
#include <linux/suspend.h>
#ifdef CONFIG_MSM_SMD
#include <mach/msm_smd.h>
#endif
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

//#define SLEEP_HISTORY_DEBUG_LOG 1
#define SLEEP_HISTORY_MAGIC_NUMBER 0x5a
#define WS_ARRAY_MAX 10
#define SLEEP_HISTORY_RINGBUFFER_SIZE 2048
#define sleep_history_ring_incr(n, s)	(((n) + 1) & ((s) - 1))
#define sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr) \
{\
	if (ts) {	\
		ts_ptr = &((ring_buf_ptr + *head_ptr)->ts);	\
		memcpy(ts_ptr, ts, sizeof(struct timespec));	\
	}	\
}
#define sleep_history_set_type(type, ring_buf_ptr, head_ptr) (ring_buf_ptr + *head_ptr)->type = (int)type

struct slp_ws {
	char name[16];
	ktime_t prevent_time;
};

union wakeup_history {
	unsigned int irq;
	struct slp_ws ws;
};

struct battery_history {
	int status;
	int capacity;
};

struct sleep_history {
	char type;
	char failed_step;
	int suspend_count;
	struct timespec ts;
	struct battery_history battery;
	union wakeup_history ws;
#ifdef CONFIG_SLEEP_MONITOR
	int pretty_group[SLEEP_MONITOR_GROUP_SIZE];
#endif
};

struct sleep_history_data {
	struct circ_buf sleep_history;
};

static struct sleep_history_data sleep_history_data;
static struct suspend_stats suspend_stats_bkup;

#ifdef CONFIG_DEBUG_FS
static struct sleep_history_data copy_history_data;
static int sleep_history_index = 1;
static char *type_text[] = {
        "none", "autosleep", "autosleep",
        "suspend", "suspend",
        "irq"
};

static char *suspend_text[] = {
        "failed", "freeze", "prepare", "suspend", "suspend_late",
         "suspend_noirq", "resume_noirq", "resume_early",  "resume"
};

static int print_sleep_history_time(
	char *buffer, int count, struct timespec *entry_ts, struct timespec *exit_ts, int offset)
{
	struct timespec delta;
	struct rtc_time entry_tm, exit_tm;

	if (entry_ts)
		rtc_time_to_tm(entry_ts->tv_sec, &entry_tm);
	else
		rtc_time_to_tm(0, &entry_tm);
	if (exit_ts)
		rtc_time_to_tm(exit_ts->tv_sec, &exit_tm);
	else
		rtc_time_to_tm(0, &exit_tm);
	if (entry_ts && exit_ts)
		delta = timespec_sub(*exit_ts, *entry_ts);

	offset += snprintf(buffer + offset, count - offset,
				"%04d-%02d-%02d/%02d:%02d:%02d "
				"%04d-%02d-%02d/%02d:%02d:%02d ",
				entry_tm.tm_year + 1900, entry_tm.tm_mon + 1,
				entry_tm.tm_mday, entry_tm.tm_hour,
				entry_tm.tm_min, entry_tm.tm_sec,
				exit_tm.tm_year + 1900, exit_tm.tm_mon + 1,
				exit_tm.tm_mday, exit_tm.tm_hour,
				exit_tm.tm_min, exit_tm.tm_sec);

	if (entry_ts && exit_ts)
		offset += snprintf(buffer + offset, count - offset, "%7d ", (int)delta.tv_sec);
	else
		offset += snprintf(buffer + offset, count - offset, "        ");

	return offset;
}

static int print_sleep_history_battery(
	char *buffer, int count, struct battery_history *entry, struct battery_history *exit, int offset)
{
	int capacity_delta = -1;

	if (!entry || !exit)
		return -EINVAL;

	if (entry->capacity  != -1 && exit->capacity  != -1)
		capacity_delta = exit->capacity - entry->capacity;

	offset += snprintf(buffer + offset, count - offset, "%3d %3d %3d %d %d   ",
					entry->capacity, exit->capacity, capacity_delta,
					entry->status, exit->status);

	return offset;
}

static bool is_valid_access_pointer(struct sleep_history *ring_buf_ptr, int tail)
{
	if((ring_buf_ptr + tail) == NULL || (ring_buf_ptr + tail)->type == SLEEP_HISTORY_MAGIC_NUMBER)
		return false;
	else
		return true;
}

#ifdef CONFIG_SLEEP_MONITOR
static int print_sleep_monitor_pretty_value(char* buffer, int count,
								struct sleep_history *ring_buf_ptr, int tail_n, int offset)
{
	int i = 0;

	if (is_valid_access_pointer(ring_buf_ptr, tail_n) == false)
		return -EINVAL;
	else {
		offset += snprintf(buffer + offset, count - offset, "  ");
		for (i = SLEEP_MONITOR_GROUP_SIZE - 1; i >= 0; i--) {
			offset += snprintf(buffer + offset, count - offset, "%08x/",(ring_buf_ptr + tail_n)->pretty_group[i]);
		}
		offset += snprintf(buffer + offset, count - offset, "  ");
	}
	return offset;
}
#endif

static int history_buf_check(int* head_n, int* tail_n)
{
	if (*head_n - 1 == *tail_n || *head_n == *tail_n)
			return SLEEP_HISTORY_PRINT_STOP;
	else if (*head_n > *tail_n)
			(*tail_n)++;
	else if (*head_n < *tail_n) {
			if (*tail_n < SLEEP_HISTORY_RINGBUFFER_SIZE - 1)
				(*tail_n)++;
			else if (*tail_n >= SLEEP_HISTORY_RINGBUFFER_SIZE - 1)
				*tail_n = 0 ;
	}
	return SLEEP_HISTORY_PRINT_CONTINUE;
}

static int sleep_history_headline_show(char* buffer, size_t count, int offset)
{
	offset += snprintf(buffer + offset, count - offset, "    type      count     entry time          ");
	offset += snprintf(buffer + offset, count - offset, "exit time           ");
	offset += snprintf(buffer + offset, count - offset, "    diff      ");
	offset += snprintf(buffer + offset, count - offset, "battery                   sleep_monitor         ");
	offset += snprintf(buffer + offset, count - offset, "                                wakeup source\n");
	offset += snprintf(buffer + offset, count - offset, "--- --------- --------- ------------------- ");
	offset += snprintf(buffer + offset, count - offset, "------------------- ");
	offset += snprintf(buffer + offset, count - offset, "------- ");
	offset += snprintf(buffer + offset, count - offset, "---------------- -----------------------------------");
	offset += snprintf(buffer + offset, count - offset, "---- -------------------------------------------------------------------------------------------\n");

	return offset;
}

static int sleep_history_print_control(char __user *buffer,
					size_t count, loff_t *ppos, int (*func)(char *, size_t, int))
{

	static char *buf = NULL;
	size_t buf_size = PAGE_SIZE;
	int offset = 0, size_for_copy = count;
	static unsigned int rest_size = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf_size = min(buf_size, count);

	buf =vmalloc(buf_size);
	memset(buf, 0, buf_size);

	if (!buf)
		return -ENOMEM;

	if (*ppos == 0)
		offset += sleep_history_headline_show(buf, buf_size, offset);
	else
		offset += func(buf, buf_size, offset);

	if (offset <= count) {
		size_for_copy = offset;
		rest_size = 0;
	} else {
		size_for_copy = count;
		rest_size = offset - size_for_copy;
	}

	if (size_for_copy >  0) {
		if (copy_to_user(buffer, buf , size_for_copy)) {
			vfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	}

	vfree(buf);
	return size_for_copy;
}


static int print_autoline_display(char* buffer, size_t count, int offset)
{
	int wakeup_count = 0;
	struct sleep_history* ring_buf_ptr;
	struct timespec *entry_ts = 0, *exit_ts = 0;
	struct battery_history batt_entry, batt_exit;
	union wakeup_history wakeup[WS_ARRAY_MAX];
	int *tail = 0;
	int *head = 0;

	ring_buf_ptr = (struct sleep_history*)copy_history_data.sleep_history.buf;
	tail = &copy_history_data.sleep_history.tail;
	head = &copy_history_data.sleep_history.head;

	if (*tail < 0 || *tail >= SLEEP_HISTORY_RINGBUFFER_SIZE)
		return offset;

	if (!is_valid_access_pointer(ring_buf_ptr, *tail))
		return offset;

	/* get autosleep entry info*/
	entry_ts = &(ring_buf_ptr + *tail)->ts;
	batt_entry.status = (ring_buf_ptr + *tail)->battery.status;
	batt_entry.capacity = (ring_buf_ptr + *tail)->battery.capacity;

	/* increase tail count */
	if (history_buf_check(head, tail) == SLEEP_HISTORY_PRINT_STOP)
		return offset;

	/* get autosleep exit info */
	exit_ts = &(ring_buf_ptr + *tail)->ts;
	if (!exit_ts || !entry_ts) {
		pr_info("[slp_history] auto line time fail\n");
		return offset;
	}
	batt_exit.status = (ring_buf_ptr + *tail)->battery.status;
	batt_exit.capacity = (ring_buf_ptr + *tail)->battery.capacity;
	memset(wakeup, 0, sizeof(union wakeup_history) * WS_ARRAY_MAX);
	wakeup[wakeup_count] = ((ring_buf_ptr + *tail)->ws);

	/* print sleep history */
	offset += snprintf(buffer + offset, count - offset, "%3d %9s           ",
				sleep_history_index++, type_text[(int)((ring_buf_ptr + *tail)->type)]);
	offset = print_sleep_history_time(buffer, count, entry_ts, exit_ts, offset);
	offset = print_sleep_history_battery(buffer, count, &batt_entry, &batt_exit, offset);
	offset += snprintf(buffer + offset, count - offset, "                                         ");
	offset += snprintf(buffer + offset, count - offset,
					"%s:%lld ", wakeup[wakeup_count].ws.name,
					ktime_to_ms(wakeup[wakeup_count].ws.prevent_time));
	wakeup_count++;

	/* check & print additional wakeup source */
	do {
		if (history_buf_check(head, tail) == SLEEP_HISTORY_PRINT_STOP)
			return offset;

		if ((ring_buf_ptr + *tail)->type == SLEEP_HISTORY_AUTOSLEEP_EXIT) {
			wakeup[wakeup_count] = ((ring_buf_ptr + *tail)->ws);
			offset += snprintf(buffer + offset, count - offset,
					"%s:%lld ", wakeup[wakeup_count].ws.name,
					ktime_to_ms(wakeup[wakeup_count].ws.prevent_time));
			wakeup_count++;
		} else
			break;

	} while (wakeup_count < WS_ARRAY_MAX);
	offset += snprintf(buffer + offset, count - offset, "\n");

	/* suspend fail case */
	if ((ring_buf_ptr + *tail)->type == SLEEP_HISTORY_AUTOSLEEP_ENTRY) {
		/* get entry info */
		entry_ts = exit_ts;
		batt_entry.status = batt_exit.status;
		batt_entry.capacity = batt_exit.capacity;

		/* get exit info */
		exit_ts = &(ring_buf_ptr + *tail)->ts;
		batt_exit.status = (ring_buf_ptr + *tail)->battery.status;
		batt_exit.capacity = (ring_buf_ptr + *tail)->battery.capacity;

		/* print info */
		if ((ring_buf_ptr + *tail)->failed_step > 0)
			offset += snprintf(buffer + offset, count - offset, "%3d %9s %9s ",
				sleep_history_index++,
				suspend_text[0], suspend_text[(int)(ring_buf_ptr + *tail)->failed_step]);
		else {
			(*tail)--;
			return offset;
		}
		offset = print_sleep_history_time(buffer, count, entry_ts, exit_ts, offset);
		offset = print_sleep_history_battery(buffer, count, &batt_entry, &batt_exit, offset);
		offset += snprintf(buffer + offset, count - offset, "\n");
	}

	return offset;
}

static int print_suspendline_display(char *buffer, size_t count, int offset)
{
	int j = 0;
	int wakeup_count = 0;
#ifdef CONFIG_SLEEP_MONITOR
	int pretty_offset = 0;
#endif
	struct sleep_history* ring_buf_ptr;
	struct timespec *entry_ts = 0, *exit_ts = 0;
	struct battery_history batt_entry, batt_exit;
	union wakeup_history wakeup[WS_ARRAY_MAX];
	struct irq_desc *desc = 0;
	int *tail = 0;
	int *head = 0;

	ring_buf_ptr = (struct sleep_history*)	copy_history_data.sleep_history.buf;
	head = &copy_history_data.sleep_history.head;
	tail = &copy_history_data.sleep_history.tail;

	/* get entry info */
	entry_ts = &(ring_buf_ptr + *tail)->ts;
	batt_entry.status = (ring_buf_ptr + *tail)->battery.status;
	batt_entry.capacity = (ring_buf_ptr + *tail)->battery.capacity;
	wakeup_count = 0;
	memset(wakeup, 0, sizeof(union wakeup_history)*WS_ARRAY_MAX);

	/* get irq info */
	do {
		if (history_buf_check(head, tail) == SLEEP_HISTORY_PRINT_STOP)
			return offset;

		if ((ring_buf_ptr + *tail)->type == SLEEP_HISTORY_WAKEUP_IRQ)
			wakeup[wakeup_count++] = ((ring_buf_ptr + *tail)->ws);
		else
			break;

	} while (wakeup_count < WS_ARRAY_MAX);

	/* get exit info */
	exit_ts = &(ring_buf_ptr + *tail)->ts;
	if (!exit_ts || !entry_ts) {
		pr_info("sleep_history suspend line time fail\n");
		return offset;
	}
	batt_exit.status = (ring_buf_ptr + *tail)->battery.status;
	batt_exit.capacity = (ring_buf_ptr + *tail)->battery.capacity;

	/* print data */
	if (((int)(ring_buf_ptr + *tail)->failed_step != SLEEP_HISTORY_MAGIC_NUMBER) &&
		(ring_buf_ptr + *tail)->failed_step > 0)
		offset += snprintf(buffer + offset, count - offset, "%3d %9s %9s ", sleep_history_index++,
			suspend_text[0],	suspend_text[(int)(ring_buf_ptr + *tail)->failed_step]);
	else
		offset += snprintf(buffer + offset, count - offset, "%3d %9s %9d ", sleep_history_index++,
								type_text[(int)(ring_buf_ptr + *tail)->type],
								(ring_buf_ptr + *tail)->suspend_count);

	offset = print_sleep_history_time(buffer, count, entry_ts, exit_ts, offset);
	offset = print_sleep_history_battery(buffer, count, &batt_entry, &batt_exit, offset);

#ifdef CONFIG_SLEEP_MONITOR
	if (*tail - wakeup_count - 1 < 0)
		pretty_offset = SLEEP_HISTORY_RINGBUFFER_SIZE - (*tail - wakeup_count - 1);
	else
		pretty_offset = *tail - wakeup_count - 1;
	offset = print_sleep_monitor_pretty_value(buffer, count, ring_buf_ptr, pretty_offset, offset);
#else
	offset += snprintf(buffer + offset, count - offset, "                                         ");
#endif

	/* print saved irq data */
	for (j  = 0; j < wakeup_count; j++) {
		if (wakeup[j].irq) {
			if (wakeup[j].irq != NR_IRQS) {
				desc = irq_to_desc(wakeup[j].irq);
				if (desc && desc->action && desc->action->name) {
#ifdef CONFIG_MSM_SMD
					const char *msm_subsys;

					msm_subsys = smd_irq_to_subsystem(wakeup[j].irq);
					if (msm_subsys)
						offset += snprintf(buffer + offset, count - offset, "%d,%s:%s/ ",
							wakeup[j].irq, desc->action->name, msm_subsys);
					else
#endif
					offset += snprintf(buffer + offset, count - offset, "%d,%s/ ",
									wakeup[j].irq, desc->action->name);
				}
			} else
				offset += snprintf(buffer + offset,  count - offset, "%d, NR_IRQS", wakeup[j].irq);
		}
	}
	offset += snprintf(buffer + offset,  count - offset,  "\n");

	return offset;
}



static ssize_t sleep_history_debug_read(struct file *file,
							char __user *buffer, size_t count, loff_t *ppos)
{
	int copy_size = 0;
	int *head = 0;
	int *tail = 0;
	struct sleep_history *buf_ptr = 0;

	head = &copy_history_data.sleep_history.head;
	tail = &copy_history_data.sleep_history.tail;
	buf_ptr = (struct sleep_history*)copy_history_data.sleep_history.buf;

	if ((*head == 0 && *tail == 0) || (*head  == 1 && *tail  == 0))
		return 0;

	if ((*head < 0 || *head >= SLEEP_HISTORY_RINGBUFFER_SIZE) ||
		(*tail < 0 || *tail >= SLEEP_HISTORY_RINGBUFFER_SIZE)) {
		pr_info("[slp_history] invalid size -> head = %d,  tail =%d\n", *head, *tail);
		return 0;
	}

	if (!is_valid_access_pointer(buf_ptr, *tail))
		return 0;

	while(1) {
		if (((buf_ptr + *tail)->type == SLEEP_HISTORY_AUTOSLEEP_ENTRY) ||
			((buf_ptr + *tail)->type == SLEEP_HISTORY_SUSPEND_ENTRY))
			break;

		if (history_buf_check(head, tail) == SLEEP_HISTORY_PRINT_STOP)
			return 0;
	}

	switch ((buf_ptr + *tail)->type) {
		case SLEEP_HISTORY_AUTOSLEEP_ENTRY :
			copy_size += sleep_history_print_control(buffer, count, ppos, print_autoline_display);
			break;
		case SLEEP_HISTORY_SUSPEND_ENTRY :
			copy_size += sleep_history_print_control(buffer, count, ppos, print_suspendline_display);
			break;
		default : break;
	}

	if (copy_size > 500)
		pr_info("[slp_history] %s - copy_size = %d\n", __func__, copy_size);

	return copy_size;
}

int sleep_history_debug_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	pr_info("[slp_history] debug open\n");
	if (sleep_history_index != 0xFFFFFFFF)
		pr_info("[slp_history] sleep_history index invalid\n");
	copy_history_data = sleep_history_data;
	sleep_history_index = 1;

	return 0;
}

int sleep_history_debug_release(struct inode *inode, struct file *file)
{
	pr_info("[slp_history] debug release\n");
	sleep_history_index= 0xFFFFFFFF;
	return 0;
}

static const struct file_operations sleep_history_debug_fops = {
	.read		= sleep_history_debug_read,
	.open		= sleep_history_debug_open,
	.release		= sleep_history_debug_release,
};

static int __init sleep_history_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("sleep_history", S_IRUSR, NULL, NULL,
		&sleep_history_debug_fops);
	if (!d) {
		pr_err("Failed to create sleep_history debug file\n");
		return -ENOMEM;
	}

	return 0;
}

late_initcall(sleep_history_debug_init);
#endif

int sleep_history_marker(int type, struct timespec *ts, void *wakeup)
{
	int err;
	int *head_ptr, *tail_ptr;
	struct sleep_history *ring_buf_ptr;
	struct rtc_time tm;
	struct timespec *ts_ptr;
	union wakeup_history *wakeup_ptr;
	struct power_supply *psy_battery = 0;
	union power_supply_propval value;
#ifdef CONFIG_PM_SLAVE_WAKELOCKS
	struct slave_wakelock *last_slave_wl[4];
	int j;
#endif /* CONFIG_PM_SLAVE_WAKELOCKS */

	if (type >= SLEEP_HISTORY_TYPE_MAX  || (!ts && !wakeup))
		return -EINVAL;

#ifdef SLEEP_HISTORY_DEBUG_LOG
	pr_info("marker : %d\n", type);
#endif

	ring_buf_ptr = (struct sleep_history *)(sleep_history_data.sleep_history.buf);
	head_ptr = &sleep_history_data.sleep_history.head;
	tail_ptr = &sleep_history_data.sleep_history.tail;

	memset(ring_buf_ptr + *head_ptr, 0, sizeof(struct sleep_history));
	sleep_history_set_type(type, ring_buf_ptr, head_ptr);
	rtc_time_to_tm(0, &tm);
	psy_battery = power_supply_get_by_name("battery");
	if (psy_battery) {
		err = psy_battery->get_property(psy_battery,
				POWER_SUPPLY_PROP_STATUS, &value);
		if (err < 0)
			value.intval = -1;
		(ring_buf_ptr + *head_ptr)->battery.status = value.intval;

		err = psy_battery->get_property(psy_battery,
				POWER_SUPPLY_PROP_CAPACITY, &value);
		if (err < 0)
			value.intval = -1;
		(ring_buf_ptr + *head_ptr)->battery.capacity = value.intval;
	}

	switch (type) {
#ifdef CONFIG_PM_AUTOSLEEP
	case SLEEP_HISTORY_AUTOSLEEP_ENTRY:
		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);
		if (suspend_stats_bkup.fail != suspend_stats.fail) {
			int last_step;
			last_step = suspend_stats.last_failed_step + REC_FAILED_NUM - 1;
			last_step %= REC_FAILED_NUM;
			(ring_buf_ptr + *head_ptr)->failed_step =  suspend_stats.failed_steps[last_step];

		}
		break;

	case SLEEP_HISTORY_AUTOSLEEP_EXIT:
		memcpy(&suspend_stats_bkup, &suspend_stats, sizeof(struct suspend_stats));
		pr_info("marker: %d: ", type);
		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);
		if (ts) {
			rtc_time_to_tm(ts->tv_sec, &tm);
			pr_cont("t:%d/%d/%d/%d/%d/%d ",
				tm.tm_year, tm.tm_mon,
				tm.tm_mday, tm.tm_hour,
				tm.tm_min, tm.tm_sec);
		}

		if (wakeup) {
			wakeup_ptr = &((ring_buf_ptr + *head_ptr)->ws);
			memcpy(wakeup_ptr->ws.name, ((struct wakeup_source*)wakeup)->name, 15);
			wakeup_ptr->ws.name[15] = '\0';
			wakeup_ptr->ws.prevent_time =
				((struct wakeup_source *)wakeup)->prevent_time;
			pr_cont("ws:%s/%lld ", wakeup_ptr->ws.name,
				ktime_to_ms(wakeup_ptr->ws.prevent_time));

#ifdef CONFIG_PM_SLAVE_WAKELOCKS
			memset(last_slave_wl, 0, sizeof(last_slave_wl));
			if (pm_get_last_slave_wakelocks(wakeup, &last_slave_wl[0], 4)) {
				for (j = 0;  last_slave_wl[j] && j < 4; j++) {
					if (CIRC_SPACE(*head_ptr, *tail_ptr, SLEEP_HISTORY_RINGBUFFER_SIZE) == 0)
						*tail_ptr = sleep_history_ring_incr(*tail_ptr,
									SLEEP_HISTORY_RINGBUFFER_SIZE);
					*head_ptr = sleep_history_ring_incr(*head_ptr,
									SLEEP_HISTORY_RINGBUFFER_SIZE);

					ring_buf_ptr = (struct sleep_history *)(sleep_history_data.sleep_history.buf);
					head_ptr = &sleep_history_data.sleep_history.head;
					tail_ptr =	&sleep_history_data.sleep_history.tail;
					memset((ring_buf_ptr + *head_ptr), 0, sizeof(struct sleep_history));
					sleep_history_set_type(type, ring_buf_ptr, head_ptr);

					wakeup_ptr = &((ring_buf_ptr + *head_ptr)->ws);
					memcpy(wakeup_ptr->ws.name, last_slave_wl[j]->name, 15);
					wakeup_ptr->ws.name[15] = '\0';
					wakeup_ptr->ws.prevent_time =
						last_slave_wl[j]->prevent_time;
					pr_cont("swl=%s/%lld ", last_slave_wl[j]->name,
						ktime_to_ms(last_slave_wl[j]->prevent_time));

				}
				pm_del_slave_wakelock_prevent_sleep_time(wakeup);
			}
#endif /* CONFIG_PM_SLAVE_WAKELOCKS */
		}
		break;
#endif

	case SLEEP_HISTORY_WAKEUP_IRQ:
		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);

		if (wakeup) {
			wakeup_ptr = &((ring_buf_ptr + *head_ptr)->ws);
			wakeup_ptr->irq = *((unsigned int *)wakeup);
		}
		break;

	case SLEEP_HISTORY_SUSPEND_ENTRY:
		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);
#ifdef CONFIG_SLEEP_MONITOR
		memset((ring_buf_ptr + *head_ptr)->pretty_group, 0, sizeof(int) * SLEEP_MONITOR_GROUP_SIZE);
		sleep_monitor_get_pretty((ring_buf_ptr + *head_ptr)->pretty_group,
										SLEEP_MONITOR_CALL_SUSPEND);
#endif
		break;

	case SLEEP_HISTORY_SUSPEND_EXIT:
		if (suspend_stats_bkup.fail != suspend_stats.fail ||
			suspend_stats_bkup.failed_prepare != suspend_stats.failed_prepare ||
			suspend_stats_bkup.failed_suspend != suspend_stats.failed_suspend ||
			suspend_stats_bkup.failed_suspend_late != suspend_stats.failed_suspend_late ||
			suspend_stats_bkup.failed_suspend_noirq != suspend_stats.failed_suspend_noirq) {
			int last_step;
			last_step = suspend_stats.last_failed_step + REC_FAILED_NUM - 1;
			last_step %= REC_FAILED_NUM;
			(ring_buf_ptr + *head_ptr)->failed_step =
				suspend_stats.failed_steps[last_step];
		} else {
			(ring_buf_ptr + *head_ptr)->suspend_count =
				suspend_stats.success + 1;
		}
		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);

#ifdef CONFIG_SLEEP_MONITOR
		memset((ring_buf_ptr + *head_ptr)->pretty_group, 0, sizeof(int) * SLEEP_MONITOR_GROUP_SIZE);
		sleep_monitor_get_pretty((ring_buf_ptr + *head_ptr)->pretty_group,
										SLEEP_MONITOR_CALL_RESUME);
#endif
		break;
	default:
		return -EPERM;
	}

	if (CIRC_SPACE(*head_ptr, *tail_ptr, SLEEP_HISTORY_RINGBUFFER_SIZE) == 0)
		*tail_ptr = sleep_history_ring_incr(*tail_ptr,
					SLEEP_HISTORY_RINGBUFFER_SIZE);
	*head_ptr = sleep_history_ring_incr(*head_ptr,
					SLEEP_HISTORY_RINGBUFFER_SIZE);

	return 0;
}


static int sleep_history_syscore_init(void)
{
	/* Init circ buf for sleep history*/
	sleep_history_data.sleep_history.head = 0;
	sleep_history_data.sleep_history.tail = 0;
	sleep_history_data.sleep_history.buf = (char *)kmalloc(sizeof(struct sleep_history) *
							SLEEP_HISTORY_RINGBUFFER_SIZE, GFP_KERNEL);
	memset(sleep_history_data.sleep_history.buf, 0,
				sizeof(struct sleep_history) * SLEEP_HISTORY_RINGBUFFER_SIZE);

	return 0;
}

static void sleep_history_syscore_exit(void)
{
	kfree(sleep_history_data.sleep_history.buf);
}
module_init(sleep_history_syscore_init);
module_exit(sleep_history_syscore_exit);
