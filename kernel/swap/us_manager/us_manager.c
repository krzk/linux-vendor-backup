/*
 *  SWAP uprobe manager
 *  modules/us_manager/us_manager.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013	 Vyacheslav Cherkashin: SWAP us_manager implement
 *
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include "pf/pf_group.h"
#include "sspt/sspt_proc.h"
#include "helper.h"
#include "us_manager.h"
#include "debugfs_us_manager.h"

#include <writer/event_filter.h>
#include <master/swap_initializer.h>

/* FIXME: move /un/init_msg() elsewhere and remove this include  */
#include <writer/swap_writer_module.h>		/* for /un/init_msg() */



static DEFINE_MUTEX(mutex_inst);
static enum status_type status = ST_OFF;


static void do_usm_stop(void)
{
	unregister_helper_top();
	uninstall_all();
	unregister_helper_bottom();
	sspt_proc_free_all();
}

static int do_usm_start(void)
{
	int ret;

	ret = register_helper();
	if (ret)
		return ret;

	install_all();

	return 0;
}

/**
 * @brief Get instrumentation status
 *
 * @return Instrumentation status
 */
enum status_type usm_get_status(void)
{
	mutex_lock(&mutex_inst);
	return status;
}
EXPORT_SYMBOL_GPL(usm_get_status);

/**
 * @brief Put instrumentation status
 *
 * @param st Instrumentation status
 * @return Void
 */
void usm_put_status(enum status_type st)
{
	status = st;
	mutex_unlock(&mutex_inst);
}
EXPORT_SYMBOL_GPL(usm_put_status);

/**
 * @brief Stop instrumentation
 *
 * @return Error code
 */
int usm_stop(void)
{
	int ret = 0;

	if (usm_get_status() == ST_OFF) {
		printk("US instrumentation is not running!\n");
		ret = -EINVAL;
		goto put;
	}

	do_usm_stop();

put:
	usm_put_status(ST_OFF);

	return ret;
}
EXPORT_SYMBOL_GPL(usm_stop);

/**
 * @brief Start instrumentation
 *
 * @return Error code
 */
int usm_start(void)
{
	int ret = -EINVAL;
	enum status_type st;

	st = usm_get_status();
	if (st == ST_ON) {
		printk("US instrumentation is already run!\n");
		goto put;
	}

	ret = do_usm_start();
	if (ret == 0)
		st = ST_ON;

put:
	usm_put_status(st);

	return ret;
}
EXPORT_SYMBOL_GPL(usm_start);





/* ============================================================================
 * ===                                QUIET                                 ===
 * ============================================================================
 */
static enum quiet_type quiet = QT_ON;

/**
 * @brief Set quiet mode
 *
 * @param q Quiet mode
 * @return Void
 */
void set_quiet(enum quiet_type q)
{
	quiet = q;
}
EXPORT_SYMBOL_GPL(set_quiet);

/**
 * @brief Get quiet mode
 *
 * @return Quiet mode
 */
enum quiet_type get_quiet(void)
{
	return quiet;
}
EXPORT_SYMBOL_GPL(get_quiet);





/* ============================================================================
 * ===                              US_FILTER                               ===
 * ============================================================================
 */
static int us_filter(struct task_struct *task)
{
	return !!sspt_proc_get_by_task(task);
}

static struct ev_filter ev_us_filter = {
	.name = "traced_process_only",
	.filter = us_filter
};

static int init_us_filter(void)
{
	int ret;

	ret = event_filter_register(&ev_us_filter);
	if (ret)
		return ret;

	return event_filter_set(ev_us_filter.name);
}

static void exit_us_filter(void)
{
	event_filter_unregister(&ev_us_filter);
}





static int init_us_manager(void)
{
	int ret;

	ret = init_us_filter();
	if (ret)
		return ret;

	ret = init_msg(32*1024); /* TODO: move to writer */
	if (ret)
		exit_us_filter();

	return ret;
}

static void exit_us_manager(void)
{
	if (status == ST_ON)
		do_usm_stop();

	uninit_msg();
	exit_us_filter();
}

SWAP_LIGHT_INIT_MODULE(once_helper, init_us_manager, exit_us_manager,
		       init_debugfs_us_manager, exit_debugfs_us_manager);

MODULE_LICENSE ("GPL");
