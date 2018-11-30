/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _CONTEXTHUB_DRV_H
#define _CONTEXTHUB_DRV_H

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/platform_data/nanohub.h>

#define CHUB_OS_NAME "os.chub.bin"
/* TODO: add embos Packet size #define PACKET_SIZE_MAX (270) */

struct host_data {
	atomic_t wakeup_cnt;
	atomic_t wakeup_lock_cnt;
	atomic_t wakeup_acquired;
	wait_queue_head_t wakeup_wait;
	struct device *dev;
	struct nanohub_platform_data *pdata;
#ifdef USE_HAL_IF
	struct miscdevice miscdev;
#endif
};

#define wait_event_interruptible_timeout_locked(q, cond, tmo)		\
({									\
	long __ret = (tmo);						\
	DEFINE_WAIT(__wait);						\
	if (!(cond)) {							\
		for (;;) {						\
			__wait.flags &= ~WQ_FLAG_EXCLUSIVE;		\
			if (list_empty(&__wait.task_list))		\
				__add_wait_queue_tail(&(q), &__wait);	\
			set_current_state(TASK_INTERRUPTIBLE);		\
			if ((cond))					\
				break;					\
			if (signal_pending(current)) {			\
				__ret = -ERESTARTSYS;			\
				break;					\
			}						\
			spin_unlock_irq(&(q).lock);				\
			__ret = schedule_timeout(__ret);		\
			spin_lock_irq(&(q).lock);				\
			if (!__ret) {					\
				if ((cond))				\
					__ret = 1;			\
				break;					\
			}						\
		}							\
		__set_current_state(TASK_RUNNING);			\
		if (!list_empty(&__wait.task_list))			\
			list_del_init(&__wait.task_list);		\
		else if (__ret == -ERESTARTSYS &&			\
			 /*reimplementation of wait_abort_exclusive() */\
			 waitqueue_active(&(q)))			\
			__wake_up_locked_key(&(q), TASK_INTERRUPTIBLE,	\
			NULL);						\
	} else {							\
		__ret = 1;						\
	}								\
	__ret;								\
})

struct host_data *contexthub_probe(struct device *dev, void *ipc_data);
int contexthub_remove(struct device *dev);
int contexthub_read(uint8_t *rx, int timeout);
int contexthub_write(uint8_t *tx, int length);
void contexthub_handle_irq1(struct host_data *data);

int request_wakeup_ex(struct host_data *data, long timeout_ms,
		      int key);
void release_wakeup_ex(struct host_data *data, int key);
int contexthub_request(void);
void contexthub_release(void);
int contexthub_is_run(void);
#endif
