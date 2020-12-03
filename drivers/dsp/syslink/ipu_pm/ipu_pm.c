/*
 * ipu_pm.c
 *
 * IPU Power Management support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/autoconf.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <syslink/notify.h>
#include <syslink/notify_driver.h>
#include <syslink/notifydefs.h>
#include <syslink/notify_driverdefs.h>
#include <syslink/notify_ducatidriver.h>

/* Power Management headers */
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/dma.h>
#include <plat/dmtimer.h>
#include <plat/clock.h>
#include <plat/i2c.h>
#include <plat/io.h>
#include <plat/iommu.h>
#include <plat/mailbox.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/i2c/twl.h>

/* Module headers */
#include "ipu_pm.h"

/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */
#define A9 3
#define SYS_M3 2
#define APP_M3 1
#define TESLA 0

#define LINE_ID 0
#define NUM_SELF_PROC 2
#define IPU_KFIFO_SIZE 16
#define PM_VERSION 0x00020000

/** ============================================================================
 *  Forward declarations of internal functions
 *  ============================================================================
 */

/* Request a resource on behalf of an IPU client */
static inline int ipu_pm_req_res(u32 res_type, u32 proc_id, u32 rcb_num);

/* Release a resource on behalf of an IPU client */
static inline int ipu_pm_rel_res(u32 res_type, u32 proc_id, u32 rcb_num);

/* Request a sdma channels on behalf of an IPU client */
static inline int ipu_pm_get_sdma_chan(int proc_id, u32 rcb_num);

/* Request a gptimer on behalf of an IPU client */
static inline int ipu_pm_get_gptimer(int proc_id, u32 rcb_num);

/* Request an i2c bus on behalf of an IPU client */
static inline int ipu_pm_get_i2c_bus(int proc_id, u32 rcb_num);

/* Request a gpio on behalf of an IPU client */
static inline int ipu_pm_get_gpio(int proc_id, u32 rcb_num);

/* Request a regulator on behalf of an IPU client */
static inline int ipu_pm_get_regulator(int proc_id, u32 rcb_num);

/* Request an Aux clk on behalf of an IPU client */
static inline int ipu_pm_get_aux_clk(int proc_id, u32 rcb_num);

/* Request sys m3 on behalf of an IPU client */
static inline int ipu_pm_get_sys_m3(int proc_id, u32 rcb_num);

/* Request app m3 on behalf of an IPU client */
static inline int ipu_pm_get_app_m3(int proc_id, u32 rcb_num);

/* Request L3 Bus on behalf of an IPU client */
static inline int ipu_pm_get_l3_bus(int proc_id, u32 rcb_num);

/* Request IVA HD on behalf of an IPU client */
static inline int ipu_pm_get_iva_hd(int proc_id, u32 rcb_num);

/* Request ISS on behalf of an IPU client */
static inline int ipu_pm_get_iss(int proc_id, u32 rcb_num);

/* Release a sdma on behalf of an IPU client */
static inline int ipu_pm_rel_sdma_chan(int proc_id, u32 rcb_num);

/* Release a gptimer on behalf of an IPU client */
static inline int ipu_pm_rel_gptimer(int proc_id, u32 rcb_num);

/* Release an i2c buses on behalf of an IPU client */
static inline int ipu_pm_rel_i2c_bus(int proc_id, u32 rcb_num);

/* Release a gpio on behalf of an IPU client */
static inline int ipu_pm_rel_gpio(int proc_id, u32 rcb_num);

/* Release a regulator on behalf of an IPU client */
static inline int ipu_pm_rel_regulator(int proc_id, u32 rcb_num);

/* Release an Aux clk on behalf of an IPU client */
static inline int ipu_pm_rel_aux_clk(int proc_id, u32 rcb_num);

/* Release sys m3 on behalf of an IPU client */
static inline int ipu_pm_rel_sys_m3(int proc_id, u32 rcb_num);

/* Release app m3 on behalf of an IPU client */
static inline int ipu_pm_rel_app_m3(int proc_id, u32 rcb_num);

/* Release L3 Bus on behalf of an IPU client */
static inline int ipu_pm_rel_l3_bus(int proc_id, u32 rcb_num);

/* Release IVA HD on behalf of an IPU client */
static inline int ipu_pm_rel_iva_hd(int proc_id, u32 rcb_num);

/* Release ISS on behalf of an IPU client */
static inline int ipu_pm_rel_iss(int proc_id, u32 rcb_num);

/* Request a sys m3 constraint on behalf of an IPU client */
static inline int ipu_pm_req_cstr_sys_m3(int proc_id, u32 rcb_num);

/* Request an app m3 constraint on behalf of an IPU client */
static inline int ipu_pm_req_cstr_app_m3(int proc_id, u32 rcb_num);

/* Request a L3 Bus constraint on behalf of an IPU client */
static inline int ipu_pm_req_cstr_l3_bus(int proc_id, u32 rcb_num);

/* Request an IVA HD constraint on behalf of an IPU client */
static inline int ipu_pm_req_cstr_iva_hd(int proc_id, u32 rcb_num);

/* Request an ISS constraint on behalf of an IPU client */
static inline int ipu_pm_req_cstr_iss(int proc_id, u32 rcb_num);

/* Release a sys m3 constraint on behalf of an IPU client */
static inline int ipu_pm_rel_cstr_sys_m3(int proc_id, u32 rcb_num);

/* Release an app m3 constraint on behalf of an IPU client */
static inline int ipu_pm_rel_cstr_app_m3(int proc_id, u32 rcb_num);

/* Release a L3 Bus constraint on behalf of an IPU client */
static inline int ipu_pm_rel_cstr_l3_bus(int proc_id, u32 rcb_num);

/* Release an IVA HD constraint on behalf of an IPU client */
static inline int ipu_pm_rel_cstr_iva_hd(int proc_id, u32 rcb_num);

/* Release an ISS constraint on behalf of an IPU client */
static inline int ipu_pm_rel_cstr_iss(int proc_id, u32 rcb_num);

/** ============================================================================
 *  Globals
 *  ============================================================================
 */

/* Usage Masks */
static u32 GPTIMER_USE_MASK = 0xFFFF;
static u32 I2C_USE_MASK = 0xFFFF;
static u32 AUX_CLK_USE_MASK = 0xFFFF;

/* Previous voltage value of secondary camera regulator */
static u32 cam2_prev_volt;

static struct ipu_pm_object *pm_handle_appm3;
static struct ipu_pm_object *pm_handle_sysm3;
static struct workqueue_struct *ipu_wq;
static struct iommu *p_iommu;
struct omap_mbox *p_mbox_1;
struct omap_mbox *p_mbox_2;

/* Ducati Interrupt Capable Gptimers */
static int ipu_timer_list[NUM_IPU_TIMERS] = {
	GP_TIMER_3,
	GP_TIMER_4,
	GP_TIMER_9,
	GP_TIMER_11};

/* I2C spinlock assignment mapping table */
static int i2c_spinlock_list[I2C_BUS_MAX + 1] = {
	I2C_SL_INVAL,
	I2C_1_SL,
	I2C_2_SL,
	I2C_3_SL,
	I2C_4_SL};

static char *ipu_regulator_name[REGULATOR_MAX] = {
	"cam2pwr"};

static struct ipu_pm_module_object ipu_pm_state = {
	.def_cfg.reserved = 1,
	.gate_handle = NULL
} ;

static struct ipu_pm_params pm_params = {
	.pm_gpio_counter = 0,
	.pm_gptimer_counter = 0,
	.pm_i2c_bus_counter = 0,
	.pm_sdmachan_counter = 0,
	.pm_regulator_counter = 0,
	.pm_aux_clk_counter = 0,
	.pm_sys_m3_counter = 0,
	.pm_app_m3_counter = 0,
	.pm_l3_bus_counter = 0,
	.pm_iva_hd_counter = 0,
	.pm_iss_counter = 0,
	.shared_addr = NULL,
	.timeout = 10000,
	.pm_num_events = NUMBER_PM_EVENTS,
	.pm_resource_event = PM_RESOURCE,
	.pm_notification_event = PM_NOTIFICATION,
	.proc_id = A9,
	.remote_proc_id = -1,
	.line_id = 0
} ;

/*
  Request a resource on behalf of an IPU client
 *
 */
static inline int ipu_pm_req_res(u32 res_type, u32 proc_id, u32 rcb_num)
{
	int retval = PM_SUCCESS;

	switch (res_type) {
	case SDMA:
		retval = ipu_pm_get_sdma_chan(proc_id, rcb_num);
		break;
	case GP_TIMER:
		retval = ipu_pm_get_gptimer(proc_id, rcb_num);
		break;
	case GP_IO:
		retval = ipu_pm_get_gpio(proc_id, rcb_num);
		break;
	case I2C:
		retval = ipu_pm_get_i2c_bus(proc_id, rcb_num);
		break;
	case REGULATOR:
		retval = ipu_pm_get_regulator(proc_id, rcb_num);
		break;
	case AUX_CLK:
		retval = ipu_pm_get_aux_clk(proc_id, rcb_num);
		break;
	case SYSM3:
		retval = ipu_pm_get_sys_m3(proc_id, rcb_num);
		break;
	case APPM3:
		retval = ipu_pm_get_app_m3(proc_id, rcb_num);
		break;
	case L3_BUS:
		retval = ipu_pm_get_l3_bus(proc_id, rcb_num);
		break;
	case IVA_HD:
		retval = ipu_pm_get_iva_hd(proc_id, rcb_num);
		break;
	case ISS:
		retval = ipu_pm_get_iss(proc_id, rcb_num);
		break;
	default:
		pr_err("Unsupported resource\n");
		retval = PM_UNSUPPORTED;
	}

	return retval;
}

/*
  Release a resource on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_res(u32 res_type, u32 proc_id, u32 rcb_num)
{
	int retval = PM_SUCCESS;

	switch (res_type) {
	case SDMA:
		retval = ipu_pm_rel_sdma_chan(proc_id, rcb_num);
		break;
	case GP_TIMER:
		retval = ipu_pm_rel_gptimer(proc_id, rcb_num);
		break;
	case GP_IO:
		retval = ipu_pm_rel_gpio(proc_id, rcb_num);
		break;
	case I2C:
		retval = ipu_pm_rel_i2c_bus(proc_id, rcb_num);
		break;
	case REGULATOR:
		retval = ipu_pm_rel_regulator(proc_id, rcb_num);
		break;
	case AUX_CLK:
		retval = ipu_pm_rel_aux_clk(proc_id, rcb_num);
		break;
	case SYSM3:
		retval = ipu_pm_rel_sys_m3(proc_id, rcb_num);
		break;
	case APPM3:
		retval = ipu_pm_rel_app_m3(proc_id, rcb_num);
		break;
	case L3_BUS:
		retval = ipu_pm_rel_l3_bus(proc_id, rcb_num);
		break;
	case IVA_HD:
		retval = ipu_pm_rel_iva_hd(proc_id, rcb_num);
		break;
	case ISS:
		retval = ipu_pm_rel_iss(proc_id, rcb_num);
		break;
	default:
		pr_err("Unsupported resource\n");
		retval = PM_UNSUPPORTED;
	}

	return retval;
}

/*
  Request a resource constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_req_cstr(u32 res_type, u32 proc_id, u32 rcb_num)
{
	int retval = PM_SUCCESS;

	switch (res_type) {
	case SYSM3:
		retval = ipu_pm_req_cstr_sys_m3(proc_id, rcb_num);
		break;
	case APPM3:
		retval = ipu_pm_req_cstr_app_m3(proc_id, rcb_num);
		break;
	case L3_BUS:
		retval = ipu_pm_req_cstr_l3_bus(proc_id, rcb_num);
		break;
	case IVA_HD:
		retval = ipu_pm_req_cstr_iva_hd(proc_id, rcb_num);
		break;
	case ISS:
		retval = ipu_pm_req_cstr_iss(proc_id, rcb_num);
		break;
	default:
		pr_err("Resource does not support constraints\n");
		retval = PM_UNSUPPORTED;
	}

	return retval;
}

/*
  Release a resource constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_cstr(u32 res_type, u32 proc_id, u32 rcb_num)
{
	int retval = PM_SUCCESS;

	switch (res_type) {
	case SYSM3:
		retval = ipu_pm_rel_cstr_sys_m3(proc_id, rcb_num);
		break;
	case APPM3:
		retval = ipu_pm_rel_cstr_app_m3(proc_id, rcb_num);
		break;
	case L3_BUS:
		retval = ipu_pm_rel_cstr_l3_bus(proc_id, rcb_num);
		break;
	case IVA_HD:
		retval = ipu_pm_rel_cstr_iva_hd(proc_id, rcb_num);
		break;
	case ISS:
		retval = ipu_pm_rel_cstr_iss(proc_id, rcb_num);
		break;
	default:
		pr_err("Resource does not support constraints\n");
		retval = PM_UNSUPPORTED;
	}

	return retval;
}

/*
  Work Function to req/rel a resource
 *
 */
static void ipu_pm_work(struct work_struct *work)
{
	struct ipu_pm_object *handle =
			container_of(work, struct ipu_pm_object, work);
	struct rcb_block *rcb_p;
	struct ipu_pm_msg im;
	struct ipu_pm_params *params = handle->params;
	union message_slicer pm_msg;
	int action_type;
	int res_type;
	int rcb_num;
	int retval = PM_SUCCESS;

	while (kfifo_len(handle->fifo) >= sizeof(im)) {
		kfifo_get(handle->fifo, (unsigned char *)&im, sizeof(im));

		/* Get the payload */
		pm_msg.whole = im.pm_msg;
		/* Get the rcb_num */
		rcb_num = pm_msg.fields.rcb_num;
		/* Get pointer to the proper RCB */
		rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

		/* Get the type of resource and the actions required */
		action_type = rcb_p->msg_type;
		res_type = rcb_p->sub_type;
		switch (action_type) {
		case PM_REQUEST_RESOURCE:
			retval = ipu_pm_req_res(res_type, im.proc_id, rcb_num);
			if (retval != PM_SUCCESS)
				pm_msg.fields.msg_type = PM_FAIL;
			break;
		case PM_RELEASE_RESOURCE:
			retval = ipu_pm_rel_res(res_type, im.proc_id, rcb_num);
			if (retval != PM_SUCCESS)
				pm_msg.fields.msg_type = PM_FAIL;
			break;
		case PM_REQUEST_CONSTRAINTS:
			retval = ipu_pm_req_cstr(res_type, im.proc_id, rcb_num);
			if (retval != PM_SUCCESS)
				pm_msg.fields.msg_type = PM_FAIL;
			break;
		case PM_RELEASE_CONSTRAINTS:
			retval = ipu_pm_rel_cstr(res_type, im.proc_id, rcb_num);
			if (retval != PM_SUCCESS)
				pm_msg.fields.msg_type = PM_FAIL;
			break;
		default:
			pm_msg.fields.msg_type = PM_FAIL;
			retval = PM_UNSUPPORTED;
			break;
		}

		/* Update the payload with the reply msg */
		pm_msg.fields.reply_flag = true;
		pm_msg.fields.parm = retval;

		/* Restore the payload and send to caller*/
		im.pm_msg = pm_msg.whole;

		/* Send the ACK to Remote Proc*/
		retval = notify_send_event(
					params->remote_proc_id,
					params->line_id,
					params->pm_resource_event | \
						(NOTIFY_SYSTEMKEY << 16),
					im.pm_msg,
					true);
		if (retval < 0)
			pr_err("Error sending notify event\n");
	}
}

void ipu_pm_callback(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_msg im;

	/*get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);

	im.proc_id = proc_id;
	im.pm_msg = payload;

	if (kfifo_avail(handle->fifo) >= sizeof(im)) {
		kfifo_put(handle->fifo, (unsigned char *)&im, sizeof(im));
		queue_work(ipu_wq, &handle->work);
	}
}
EXPORT_SYMBOL(ipu_pm_callback);


/*
  Function for PM notifications Callback
 *
 */
void ipu_pm_notify_callback(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload)
{
	/**
	 * Post semaphore based in eventType (payload);
	 * IPU has alreay finished the process for the
	 * notification
	 */
	/* Get the payload */
	struct ipu_pm_object *handle;
	union message_slicer pm_msg;
	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return;

	pm_msg.whole = payload;
	if (pm_msg.fields.msg_type == PM_HIBERNATE) {
		/* Remote proc requested hibernate */
		/* Remote Proc is ready to hibernate */
		handle->rcb_table->state_flag |=
					REMOTE_PROC_DOWN;
		pr_info("M3:Saving ctx(0x%x)\n", handle->rcb_table->state_flag);
		/*TODO: reset IOMMU/SYSM3/APPM3*/
	} else {
		switch (pm_msg.fields.msg_subtype) {
		case PM_SUSPEND:
			handle->pm_event[PM_SUSPEND].pm_msg = payload;
			up(&handle->pm_event[PM_SUSPEND].sem_handle);
			break;
		case PM_RESUME:
			handle->pm_event[PM_RESUME].pm_msg = payload;
			up(&handle->pm_event[PM_RESUME].sem_handle);
			break;
		case PM_HIBERNATE:
			handle->pm_event[PM_HIBERNATE].pm_msg = payload;
			up(&handle->pm_event[PM_HIBERNATE].sem_handle);
			break;
		case PM_PID_DEATH:
			handle->pm_event[PM_PID_DEATH].pm_msg = payload;
			up(&handle->pm_event[PM_PID_DEATH].sem_handle);
			break;
		}
	}
}
EXPORT_SYMBOL(ipu_pm_notify_callback);

/*
  Function for send PM Notifications
 *
 */
int ipu_pm_notifications(enum pm_event_type event_type, void *data)
{
	/**
	 * Function called by linux driver
	 * Recieves evenType: Suspend, Resume, others...
	 * Send event to Ducati
	 * Pend semaphore based in event_type (payload)
	 * Return ACK to caller
	 */

	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	union message_slicer pm_msg;
	int retval;
	int pm_ack = 0;
	int i;
	int proc_id;

	/*get the handle to proper ipu pm object */
	for (i = 0; i < NUM_SELF_PROC; i++) {
		proc_id = i + 1;
		handle = ipu_pm_get_handle(proc_id);
		if (handle == NULL)
			continue;
		params = handle->params;
		if (params == NULL)
			continue;
		switch (event_type) {
		case PM_SUSPEND:
			pm_msg.fields.msg_type = PM_NOTIFICATIONS;
			pm_msg.fields.msg_subtype = PM_SUSPEND;
			pm_msg.fields.parm = PM_SUCCESS;
			/* put general purpose message in share memory */
			handle->rcb_table->gp_msg = (unsigned)data;
			/* send the request to IPU*/
			retval = notify_send_event(
					params->remote_proc_id,
					params->line_id,
					params->pm_notification_event | \
						(NOTIFY_SYSTEMKEY << 16),
					(unsigned int)pm_msg.whole,
					true);
			if (retval < 0)
				goto error_send;
			/* wait until event from IPU (ipu_pm_notify_callback)*/
			retval = down_timeout
					(&handle->pm_event[PM_SUSPEND]
					.sem_handle,
					msecs_to_jiffies(params->timeout));
			pm_msg.whole = handle->pm_event[PM_SUSPEND].pm_msg;
			if (WARN_ON((retval < 0) ||
					(pm_msg.fields.parm != PM_SUCCESS)))
				goto error;
			break;
		case PM_RESUME:
			pm_msg.fields.msg_type = PM_NOTIFICATIONS;
			pm_msg.fields.msg_subtype = PM_RESUME;
			pm_msg.fields.parm = PM_SUCCESS;
			/* put general purpose message in share memory */
			handle->rcb_table->gp_msg = (unsigned)data;
			/* send the request to IPU*/
			retval = notify_send_event(
					params->remote_proc_id,
					params->line_id,
					params->pm_notification_event | \
						(NOTIFY_SYSTEMKEY << 16),
					(unsigned int)pm_msg.whole,
					true);
			if (retval < 0)
				goto error_send;
			/* wait until event from IPU (ipu_pm_notify_callback)*/
			retval = down_timeout
					(&handle->pm_event[PM_RESUME]
					.sem_handle,
					msecs_to_jiffies(params->timeout));
			pm_msg.whole = handle->pm_event[PM_RESUME].pm_msg;
			if (WARN_ON((retval < 0) ||
					(pm_msg.fields.parm != PM_SUCCESS)))
				goto error;
			break;
		case PM_HIBERNATE:
			pm_msg.fields.msg_type = PM_NOTIFICATIONS;
			pm_msg.fields.msg_subtype = PM_HIBERNATE;
			pm_msg.fields.parm = PM_SUCCESS;
			/* put general purpose message in share memory */
			handle->rcb_table->gp_msg = (unsigned)data;
			/* send the request to IPU*/
			retval = notify_send_event(
					params->remote_proc_id,
					params->line_id,
					params->pm_notification_event | \
						(NOTIFY_SYSTEMKEY << 16),
					(unsigned int)pm_msg.whole,
					true);
			if (retval < 0)
				goto error_send;
			/* wait until event from IPU (ipu_pm_notify_callback)*/
			retval = down_timeout
					(&handle->pm_event[PM_HIBERNATE]
					.sem_handle,
					msecs_to_jiffies(params->timeout));
			pm_msg.whole = handle->pm_event[PM_HIBERNATE].pm_msg;
			if (WARN_ON((retval < 0) ||
					(pm_msg.fields.parm != PM_SUCCESS)))
				goto error;
			else {
				/*Remote Proc is ready to hibernate*/
				handle->rcb_table->state_flag |=
							REMOTE_PROC_DOWN;
				pr_info("A9:Saving ctx ...\n");
				/*TODO: reset IOMMU/SYSM3/APPM3*/
			}
			break;
		case PM_PID_DEATH:
			pm_msg.fields.msg_type = PM_NOTIFICATIONS;
			pm_msg.fields.msg_subtype = PM_PID_DEATH;
			pm_msg.fields.parm = PM_SUCCESS;
			/* put general purpose message in share memory */
			handle->rcb_table->gp_msg = (unsigned)data;
			/* send the request to IPU*/
			retval = notify_send_event(
					params->remote_proc_id,
					params->line_id,
					params->pm_notification_event | \
						(NOTIFY_SYSTEMKEY << 16),
					(unsigned int)pm_msg.whole,
					true);
			if (retval < 0)
				goto error_send;
			/* wait until event from IPU (ipu_pm_notify_callback)*/
			retval = down_timeout
					(&handle->pm_event[PM_PID_DEATH]
					.sem_handle,
					msecs_to_jiffies(params->timeout));
			pm_msg.whole = handle->pm_event[PM_PID_DEATH].pm_msg;
			if (WARN_ON((retval < 0) ||
					(pm_msg.fields.parm != PM_SUCCESS)))
				goto error;
			break;
		}
	}
	return pm_ack;

error_send:
	pr_err("Error notify_send event\n");
error:
	pr_err("Error sending Notification events\n");
	return -EBUSY;
}
EXPORT_SYMBOL(ipu_pm_notifications);

/*
  Request a sdma channels on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_sdma_chan(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int pm_sdmachan_num;
	int pm_sdmachan_dummy;
	int ch;
	int ch_aux;
	int retval;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];
	/* Get number of channels from RCB */
	pm_sdmachan_num = rcb_p->num_chan;
	if (WARN_ON((pm_sdmachan_num <= 0) ||
			(pm_sdmachan_num > SDMA_CHANNELS_MAX)))
		return PM_INVAL_NUM_CHANNELS;

	/* Request resource using PRCM API */
	for (ch = 0; ch < pm_sdmachan_num; ch++) {
		retval = omap_request_dma(proc_id,
			"ducati-ss",
			NULL,
			NULL,
			&pm_sdmachan_dummy);
		if (retval == 0) {
			params->pm_sdmachan_counter++;
			rcb_p->channels[ch] = (unsigned char)pm_sdmachan_dummy;
		} else
			goto clean_sdma;
	}
	return PM_SUCCESS;
clean_sdma:
	/*failure, need to free the chanels*/
	for (ch_aux = 0; ch_aux < ch; ch_aux++) {
		pm_sdmachan_dummy = (int)rcb_p->channels[ch_aux];
		omap_free_dma(pm_sdmachan_dummy);
		params->pm_sdmachan_counter--;
	}
	return PM_INSUFFICIENT_CHANNELS;
}

/*
  Request a gptimer on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_gptimer(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	struct omap_dm_timer *p_gpt = NULL;
	int pm_gp_num;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];
	/* Request resource using PRCM API */
	for (pm_gp_num = 0; pm_gp_num < NUM_IPU_TIMERS; pm_gp_num++) {
		if (GPTIMER_USE_MASK & (1 << ipu_timer_list[pm_gp_num])) {
			p_gpt = omap_dm_timer_request_specific
				(ipu_timer_list[pm_gp_num]);
		} else
			continue;
		if (p_gpt != NULL) {
			/* Clear the bit in the usage mask */
			GPTIMER_USE_MASK &= ~(1 << ipu_timer_list[pm_gp_num]);
			break;
		}
	}
	if (p_gpt == NULL)
		return PM_NO_GPTIMER;
	else {
		/* Store the gptimer number and base address */
		rcb_p->fill9 = ipu_timer_list[pm_gp_num];
		rcb_p->mod_base_addr = (unsigned)p_gpt;
		params->pm_gptimer_counter++;
		return PM_SUCCESS;
	}
}

/*
  Request an i2c bus on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_i2c_bus(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	struct clk *p_i2c_clk;
	int i2c_clk_status;
	char i2c_name[I2C_NAME_SIZE];
	int pm_i2c_bus_num;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pm_i2c_bus_num = rcb_p->fill9;
	if (WARN_ON((pm_i2c_bus_num < I2C_BUS_MIN) ||
			(pm_i2c_bus_num > I2C_BUS_MAX)))
		return PM_INVAL_NUM_I2C;

	if (I2C_USE_MASK & (1 << pm_i2c_bus_num)) {
		/* building the name for i2c_clk */
		sprintf(i2c_name, "i2c%d_ck", pm_i2c_bus_num);

		/* Request resource using PRCM API */
		p_i2c_clk = omap_clk_get_by_name(i2c_name);
		if (p_i2c_clk == 0)
			return PM_NO_I2C;
		i2c_clk_status = clk_enable(p_i2c_clk);
		if (i2c_clk_status != 0)
			return PM_NO_I2C;
		/* Clear the bit in the usage mask */
		I2C_USE_MASK &= ~(1 << pm_i2c_bus_num);
		rcb_p->mod_base_addr = (unsigned)p_i2c_clk;
		/* Get the HW spinlock and store it in the RCB */
		rcb_p->data[0] = i2c_spinlock_list[pm_i2c_bus_num];
		params->pm_i2c_bus_counter++;

		return PM_SUCCESS;
	} else
		return PM_NO_I2C;
}

/*
  Request a gpio on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_gpio(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int pm_gpio_num;
	int retval;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pm_gpio_num = rcb_p->fill9;
	retval = gpio_request(pm_gpio_num , "ducati-ss");
	if (retval != 0)
		return PM_NO_GPIO;
	params->pm_gpio_counter++;

	return PM_SUCCESS;
}

/*
  Request a regulator on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_regulator(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	struct regulator *p_regulator = NULL;
	char *regulator_name;
	int pm_regulator_num;
	int retval = 0;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pm_regulator_num = rcb_p->fill9;
	if (WARN_ON((pm_regulator_num < REGULATOR_MIN) ||
			(pm_regulator_num > REGULATOR_MAX)))
		return PM_INVAL_REGULATOR;

	/*
	  FIXME:Only providing 1 regulator, if more are provided
	 *	this check is not valid.
	 */
	if (WARN_ON(params->pm_regulator_counter > 0))
		return PM_INVAL_REGULATOR;

	/* Search the name of regulator based on the id and request it */
	regulator_name = ipu_regulator_name[pm_regulator_num - 1];
	p_regulator = regulator_get(NULL, regulator_name);
	if (p_regulator == 0)
		return PM_NO_REGULATOR;

	/* Get and store the regulator default voltage */
	cam2_prev_volt = regulator_get_voltage(p_regulator);

	/* Set the regulator voltage min = data[0]; max = data[1]*/
	retval = regulator_set_voltage(p_regulator, rcb_p->data[0],
					rcb_p->data[1]);
	if (retval)
		return PM_INVAL_REGULATOR;

	/* Store the regulator handle in the RCB */
	rcb_p->mod_base_addr = (unsigned)p_regulator;
	params->pm_regulator_counter++;

	return PM_SUCCESS;
}

/*
  Request an Aux clk on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_aux_clk(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	u32 tmp = 0;
	int pm_aux_clk_num;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pm_aux_clk_num = rcb_p->fill9;

	if (WARN_ON((pm_aux_clk_num < AUX_CLK_MIN) ||
			(pm_aux_clk_num > AUX_CLK_MAX)))
		return PM_INVAL_AUX_CLK;

	if (AUX_CLK_USE_MASK & (1 << pm_aux_clk_num)) {
		/* Build the value to write */
		MASK_SET_FIELD(tmp, AUX_CLK_ENABLE, 0x1);

		/* Clear the bit in the usage mask */
		AUX_CLK_USE_MASK &= ~(1 << pm_aux_clk_num);

		/* Enabling aux clock */
		__raw_writel(tmp, AUX_CLK_REG(pm_aux_clk_num));

		/* Store the aux clk addres in the RCB */
		rcb_p->mod_base_addr = (unsigned)AUX_CLK_REG(pm_aux_clk_num);
		params->pm_aux_clk_counter++;
	} else
		return PM_NO_AUX_CLK;

	return PM_SUCCESS;
}

/*
  Request sys m3 on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_sys_m3(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Request SYS M3\n");

	params->pm_sys_m3_counter++;

	return PM_SUCCESS;
}

/*
  Request app m3 on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_app_m3(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Request APP M3\n");

	params->pm_app_m3_counter++;

	return PM_SUCCESS;
}

/*
  Request L3 Bus on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_l3_bus(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Request L3 BUS\n");

	params->pm_l3_bus_counter++;

	return PM_SUCCESS;
}

/*
  Request IVA HD on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_iva_hd(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Request IVA_HD\n");

	params->pm_iva_hd_counter++;

	return PM_SUCCESS;
}

/*
  Request ISS on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_iss(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Request ISS\n");

	params->pm_iss_counter++;

	return PM_SUCCESS;
}

/*
  Release a sdma on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_sdma_chan(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int pm_sdmachan_num;
	int pm_sdmachan_dummy;
	int ch;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;

	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Release resource using PRCM API */
	pm_sdmachan_num = rcb_p->num_chan;
	for (ch = 0; ch < pm_sdmachan_num; ch++) {
		pm_sdmachan_dummy = (int)rcb_p->channels[ch];
		omap_free_dma(pm_sdmachan_dummy);
		params->pm_sdmachan_counter--;
	}
	return PM_SUCCESS;
}

/*
  Release a gptimer on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_gptimer(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	struct omap_dm_timer *p_gpt;
	int pm_gptimer_num;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;

	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	p_gpt = (struct omap_dm_timer *)rcb_p->mod_base_addr;
	pm_gptimer_num = rcb_p->fill9;

	/* Check the usage mask */
	if (GPTIMER_USE_MASK & (1 << pm_gptimer_num))
		return PM_NO_GPTIMER;

	/* Set the usage mask for reuse */
	GPTIMER_USE_MASK |= (1 << pm_gptimer_num);

	/* Release resource using PRCM API */
	if (p_gpt != NULL)
		omap_dm_timer_free(p_gpt);
	rcb_p->mod_base_addr = 0;
	params->pm_gptimer_counter--;
	return PM_SUCCESS;
}

/*
  Release an i2c buses on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_i2c_bus(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	struct clk *p_i2c_clk;
	int pm_i2c_bus_num;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;

	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];
	pm_i2c_bus_num = rcb_p->fill9;
	p_i2c_clk = (struct clk *)rcb_p->mod_base_addr;

	/* Check the usage mask */
	if (I2C_USE_MASK & (1 << pm_i2c_bus_num))
		return PM_NO_I2C;

	/* Release resource using PRCM API */
	clk_disable(p_i2c_clk);
	rcb_p->mod_base_addr = 0;

	/* Set the usage mask for reuse */
	I2C_USE_MASK |= (1 << pm_i2c_bus_num);

	params->pm_i2c_bus_counter--;

	return PM_SUCCESS;
}

/*
  Release a gpio on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_gpio(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int pm_gpio_num;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pm_gpio_num = rcb_p->fill9;
	gpio_free(pm_gpio_num);
	params->pm_gpio_counter--;

	return PM_SUCCESS;
}

/*
  Release a regulator on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_regulator(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	struct regulator *p_regulator = NULL;
	int retval = 0;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];
	/* Get the regulator */
	p_regulator = (struct regulator *)rcb_p->mod_base_addr;

	/* Restart the voltage to the default value */
	retval = regulator_set_voltage(p_regulator, cam2_prev_volt,
					cam2_prev_volt);
	if (retval)
		return PM_INVAL_REGULATOR;

	/* Release resource using PRCM API */
	regulator_put(p_regulator);

	rcb_p->mod_base_addr = 0;
	params->pm_regulator_counter--;

	return PM_SUCCESS;
}

/*
  Release an Aux clk on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_aux_clk(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	u32 tmp = 0;
	int pm_aux_clk_num;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pm_aux_clk_num = rcb_p->fill9;

	/* Check the usage mask */
	if (AUX_CLK_USE_MASK & (1 << pm_aux_clk_num))
		return PM_NO_AUX_CLK;

	/* Build the value to write */
	MASK_SET_FIELD(tmp, AUX_CLK_ENABLE, 0x0);

	/* Disabling aux clock */
	__raw_writel(tmp, AUX_CLK_REG(pm_aux_clk_num));

	/* Set the usage mask for reuse */
	AUX_CLK_USE_MASK |= (1 << pm_aux_clk_num);

	rcb_p->mod_base_addr = 0;
	params->pm_aux_clk_counter--;

	return PM_SUCCESS;
}

/*
  Release sys m3 on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_sys_m3(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Release SYS M3\n");

	params->pm_sys_m3_counter--;

	return PM_SUCCESS;
}

/*
  Release app m3 on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_app_m3(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Release APP M3\n");

	params->pm_app_m3_counter--;

	return PM_SUCCESS;
}

/*
  Release L3 Bus on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_l3_bus(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Release L3 BUS\n");

	params->pm_l3_bus_counter--;

	return PM_SUCCESS;

}

/*
  Release IVA HD on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_iva_hd(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Release IVA_HD\n");

	params->pm_iva_hd_counter--;

	return PM_SUCCESS;
}

/*
  Release ISS on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_iss(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	pr_info("Release ISS\n");

	params->pm_iss_counter--;

	return PM_SUCCESS;
}

/*
  Request a sys m3 constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_req_cstr_sys_m3(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Request perfomance Cstr SYS M3:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Request latency Cstr SYS M3:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Request bandwidth Cstr SYS M3:%d\n", bw);
	}

	return PM_SUCCESS;
}

/*
  Request an app m3 constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_req_cstr_app_m3(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Request perfomance Cstr APP M3:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Request latency Cstr APP M3:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Request bandwidth Cstr APP M3:%d\n", bw);
	}

	return PM_SUCCESS;
}
/*
  Request a L3 Bus constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_req_cstr_l3_bus(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Request perfomance Cstr L3 Bus:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Request latency Cstr L3 Bus:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Request bandwidth Cstr L3 Bus:%d\n", bw);
	}

	return PM_SUCCESS;
}

/*
  Request an IVA HD constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_req_cstr_iva_hd(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Request perfomance Cstr IVA HD:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Request latency Cstr IVA HD:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Request bandwidth Cstr IVA HD:%d\n", bw);
	}

	return PM_SUCCESS;
}

/*
  Request an ISS constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_req_cstr_iss(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Request perfomance Cstr ISS:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Request latency Cstr ISS:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Request bandwidth Cstr ISS:%d\n", bw);
	}

	return PM_SUCCESS;
}

/*
  Release a sys m3 constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_cstr_sys_m3(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Release perfomance Cstr SYS M3:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Release latency Cstr SYS M3:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Release bandwidth Cstr SYS M3:%d\n", bw);
	}

	return PM_SUCCESS;
}

/*
  Release an app m3 constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_cstr_app_m3(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Release perfomance Cstr APP M3:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Release latency Cstr APP M3:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Release bandwidth Cstr APP M3:%d\n", bw);
	}

	return PM_SUCCESS;
}

/*
  Release a L3 Bus constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_cstr_l3_bus(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Release perfomance Cstr L3 Bus:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Release latency Cstr L3 Bus:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Release bandwidth Cstr L3 Bus:%d\n", bw);
	}

	return PM_SUCCESS;
}

/*
  Release an IVA HD constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_cstr_iva_hd(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Release perfomance Cstr IVA HD:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Release latency Cstr IVA HD:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Release bandwidth Cstr IVA HD:%d\n", bw);
	}

	return PM_SUCCESS;
}

/*
  Release an ISS constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_cstr_iss(int proc_id, u32 rcb_num)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int perf;
	int lat;
	int bw;
	u32 cstr_flags;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return PM_NOT_INSTANTIATED;

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return PM_NOT_INSTANTIATED;

	/* Get pointer to the proper RCB */
	if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
		return PM_INVAL_RCB_NUM;
	rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		perf = rcb_p->data[1];
		pr_info("Release perfomance Cstr ISS:%d\n", perf);
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		lat = rcb_p->data[2];
		pr_info("Release latency Cstr ISS:%d\n", lat);
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_info("Release bandwidth Cstr ISS:%d\n", bw);
	}

	return PM_SUCCESS;
}

/*
  Function to set init parameters
 *
 */
void ipu_pm_params_init(struct ipu_pm_params *params)
{
	int retval = 0;

	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	memcpy(params, &(pm_params), sizeof(struct ipu_pm_params));
	return;
exit:
	pr_err("ipu_pm_params_init failed status(0x%x)\n", retval);
}
EXPORT_SYMBOL(ipu_pm_params_init);

/*
  Function to calculate ipu_pm mem required
 *
 */
int ipu_pm_mem_req(const struct ipu_pm_params *params)
{
	/* Memory required for ipu pm module */
	/* FIXME: Maybe more than this is needed */
	return sizeof(struct sms);
}
EXPORT_SYMBOL(ipu_pm_mem_req);

/*
  Function to register events
  This function will register the events needed for ipu_pm
  the events reserved for power management are 2 and 3
  both sysm3 and appm3 will use the same events.
 */
int ipu_pm_init_transport(struct ipu_pm_object *handle)
{
	int retval = 0;
	struct ipu_pm_params *params;

	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto pm_register_fail;
	}

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto pm_register_fail;
	}

	retval = notify_register_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_resource_event | \
				(NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_callback,
		(void *)NULL);
	if (retval < 0)
		goto pm_register_fail;

	retval = notify_register_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_notification_event | \
				(NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_notify_callback,
		(void *)NULL);

	if (retval < 0) {
		retval = notify_unregister_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_resource_event | \
				(NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_callback,
		(void *)NULL);
		if (retval < 0)
			pr_err("Error sending notify event\n");
		goto pm_register_fail;
	}
	return retval;

pm_register_fail:
	pr_err("pm register events failed status(0x%x)", retval);
	return retval;
}

/*
  Function to create ipu pm object
 *
 */
struct ipu_pm_object *ipu_pm_create(const struct ipu_pm_params *params)
{
	int i;
	int retval = 0;

	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	if (params->remote_proc_id == SYS_M3) {
		pm_handle_sysm3 = kmalloc(sizeof(struct ipu_pm_object),
						GFP_ATOMIC);

		if (WARN_ON(unlikely(pm_handle_sysm3 == NULL))) {
			retval = -EINVAL;
			goto exit;
		}

		pm_handle_sysm3->rcb_table = (struct sms *)params->shared_addr;

		pm_handle_sysm3->pm_event = kzalloc(sizeof(struct pm_event)
					* params->pm_num_events, GFP_KERNEL);

		if (WARN_ON(unlikely(pm_handle_sysm3->pm_event == NULL))) {
			retval = -EINVAL;
			kfree(pm_handle_sysm3);
			goto exit;
		}

		/* Each event has it own sem */
		for (i = 0; i < params->pm_num_events; i++) {
			sema_init(&pm_handle_sysm3->pm_event[i].sem_handle, 0);
			pm_handle_sysm3->pm_event[i].event_type = i;
		}

		pm_handle_sysm3->params = kzalloc(sizeof(struct ipu_pm_params)
							, GFP_KERNEL);

		if (WARN_ON(unlikely(pm_handle_sysm3->params == NULL))) {
			retval = -EINVAL;
			kfree(pm_handle_sysm3->pm_event);
			kfree(pm_handle_sysm3);
			goto exit;
		}

		memcpy(pm_handle_sysm3->params, params,
			sizeof(struct ipu_pm_params));

		/* Check the SW version on both sides */
		if (WARN_ON(pm_handle_sysm3->rcb_table->pm_version !=
						PM_VERSION))
			pr_warning("Mismatch in PM version Host:0x%08x "
					"Remote:0x%08x", PM_VERSION,
					pm_handle_sysm3->rcb_table->pm_version);

		spin_lock_init(&pm_handle_sysm3->lock);
		INIT_WORK(&pm_handle_sysm3->work, ipu_pm_work);

		pm_handle_sysm3->fifo = kfifo_alloc(
				IPU_KFIFO_SIZE * sizeof(struct ipu_pm_msg),
				GFP_KERNEL, &pm_handle_sysm3->lock);

		if (pm_handle_sysm3->fifo == NULL) {
			retval = -ENOMEM;
			kfree(pm_handle_sysm3->params);
			kfree(pm_handle_sysm3->pm_event);
			kfree(pm_handle_sysm3);
			goto exit;
		}
		return pm_handle_sysm3;
	} else if (params->remote_proc_id == APP_M3) {
		pm_handle_appm3 = kmalloc(sizeof(struct ipu_pm_object),
						GFP_ATOMIC);

		if (WARN_ON(unlikely(pm_handle_appm3 == NULL))) {
			retval = -EINVAL;
			goto exit;
		}

		pm_handle_appm3->rcb_table = (struct sms *)params->shared_addr;

		pm_handle_appm3->pm_event = kzalloc(sizeof(struct pm_event)
					* params->pm_num_events, GFP_KERNEL);

		if (WARN_ON(unlikely(pm_handle_appm3->pm_event == NULL))) {
			retval = -EINVAL;
			kfree(pm_handle_appm3);
			goto exit;
		}

		/* Each event has it own sem */
		for (i = 0; i < params->pm_num_events; i++) {
			sema_init(&pm_handle_appm3->pm_event[i].sem_handle, 0);
			pm_handle_appm3->pm_event[i].event_type = i;
		}

		pm_handle_appm3->params = kzalloc(sizeof(struct ipu_pm_params)
						, GFP_KERNEL);

		if (WARN_ON(unlikely(pm_handle_appm3->params == NULL))) {
			retval = -EINVAL;
			kfree(pm_handle_appm3->pm_event);
			kfree(pm_handle_appm3);
			goto exit;
		}

		memcpy(pm_handle_appm3->params, params,
			sizeof(struct ipu_pm_params));

		/* Check the SW version on both sides */
		if (WARN_ON(pm_handle_appm3->rcb_table->pm_version !=
						PM_VERSION))
			pr_warning("Mismatch in PM version Host:0x%08x "
					"Remote:0x%08x", PM_VERSION,
					pm_handle_appm3->rcb_table->pm_version);

		spin_lock_init(&pm_handle_appm3->lock);
		INIT_WORK(&pm_handle_appm3->work, ipu_pm_work);

		pm_handle_appm3->fifo = kfifo_alloc(
				IPU_KFIFO_SIZE * sizeof(struct ipu_pm_msg),
				GFP_KERNEL, &pm_handle_appm3->lock);
		if  (pm_handle_appm3->fifo == NULL) {
			retval = -ENOMEM;
			kfree(pm_handle_appm3->params);
			kfree(pm_handle_appm3->pm_event);
			kfree(pm_handle_appm3);
		}
		return pm_handle_appm3;
	} else
		retval = -EINVAL;

exit:
	pr_err("ipu_pm_create failed! "
		"status = 0x%x\n", retval);
	return NULL;
}

/*
  Function to delete ipu pm object
 *
 */
void ipu_pm_delete(struct ipu_pm_object *handle)
{
	int retval = 0;
	struct ipu_pm_params *params;

	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	/* Release the shared RCB */
	handle->rcb_table = NULL;

	kfree(handle->pm_event);
	if (params->remote_proc_id == SYS_M3)
		pm_handle_sysm3 = NULL;
	else
		pm_handle_appm3 = NULL;
	kfree(handle->params);
	kfree(handle);
	return;
exit:
	pr_err("ipu_pm_delete is already NULL "
		"status = 0x%x\n", retval);
}

/*
  Function to get ipu pm object
 *
 */
struct ipu_pm_object *ipu_pm_get_handle(int proc_id)
{
	if (proc_id == SYS_M3)
		return pm_handle_sysm3;
	else if (proc_id == APP_M3)
		return pm_handle_appm3;
	else
		return NULL;
}
EXPORT_SYMBOL(ipu_pm_get_handle);

/*
  Function to restore a processor from hibernation
 *
 */
int ipu_pm_save_ctx(int proc_id)
{
	p_iommu = iommu_get("ducati");
	p_mbox_1 = omap_mbox_get("mailbox-1");
	p_mbox_2 = omap_mbox_get("mailbox-2");
	iommu_save_ctx(p_iommu);
	omap_mbox_save_ctx(p_mbox_1);
	omap_mbox_save_ctx(p_mbox_2);
	return 0;
}
EXPORT_SYMBOL(ipu_pm_save_ctx);

/*
  Function to save a processor before hibernation
 *
 */
int ipu_pm_restore_ctx(int proc_id)
{
	p_iommu = iommu_get("ducati");
	p_mbox_1 = omap_mbox_get("mailbox-1");
	p_mbox_2 = omap_mbox_get("mailbox-2");
	iommu_restore_ctx(p_iommu);
	omap_mbox_restore_ctx(p_mbox_1);
	omap_mbox_restore_ctx(p_mbox_2);
	return 0;
}
EXPORT_SYMBOL(ipu_pm_restore_ctx);


/*
  Get the default configuration for the ipu_pm module.
  needed in ipu_pm_setup.
 */
void ipu_pm_get_config(struct ipu_pm_config *cfg)
{
	int retval = 0;

	if (WARN_ON(unlikely(cfg == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	if (atomic_cmpmask_and_lt(&(ipu_pm_state.ref_count),
			IPU_PM_MAKE_MAGICSTAMP(0),
			IPU_PM_MAKE_MAGICSTAMP(1)) == true)
		memcpy(cfg, &ipu_pm_state.def_cfg,
			sizeof(struct ipu_pm_config));
	else
		memcpy(cfg, &ipu_pm_state.cfg, sizeof(struct ipu_pm_config));
	return;

exit:
	if (retval < 0) {
		pr_err("ipu_pm_get_config failed! status = 0x%x", retval);
	}
	return;
}
EXPORT_SYMBOL(ipu_pm_get_config);

/*
  Function to setup ipu pm object
  This function is called in platform_setup()
  This function will load the default configuration for ipu_pm
  in this function we can decide what is going to be controled
  by ipu_pm (DVFS, NOTIFICATIONS, ...) this configuration can
  can be changed on run-time.
  Also the workqueue is created and the local mutex
 */
int ipu_pm_setup(struct ipu_pm_config *cfg)
{
	struct ipu_pm_config tmp_cfg;
	int retval = 0;
	struct mutex *lock = NULL;

	/* This sets the ref_count variable is not initialized, upper 16 bits is
	  written with module Id to ensure correctness of refCount variable.
	 */
	atomic_cmpmask_and_set(&ipu_pm_state.ref_count,
				IPU_PM_MAKE_MAGICSTAMP(0),
				IPU_PM_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&ipu_pm_state.ref_count)
				!= IPU_PM_MAKE_MAGICSTAMP(1)) {
		return 1;
	}

	if (cfg == NULL) {
		ipu_pm_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	/* Create a default gate handle for local module protection */
	lock = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (lock == NULL) {
		retval = -ENOMEM;
		goto exit;
	}
	mutex_init(lock);
	ipu_pm_state.gate_handle = lock;

	/* Create the wq for req/rel resources */
	ipu_wq = create_singlethread_workqueue("ipu_wq");

	/* No proc attached yet */
	pm_handle_appm3 = NULL;
	pm_handle_sysm3 = NULL;

	memcpy(&ipu_pm_state.cfg, cfg, sizeof(struct ipu_pm_config));
	ipu_pm_state.is_setup = true;
	return retval;

exit:
	pr_err("ipu_pm_setup failed! retval = 0x%x", retval);
	return retval;
}
EXPORT_SYMBOL(ipu_pm_setup);

/*
  Function to attach ipu pm object
  This function is called in ipc_attach()
  This function will create the object based on the remoteproc id
  It is also recieving the shared address pointer to use in rcb
 */
int ipu_pm_attach(u16 remote_proc_id, void *shared_addr)
{
	struct ipu_pm_params params;
	struct ipu_pm_object *handle;
	int retval = 0;

	ipu_pm_params_init(&params);
	params.remote_proc_id = remote_proc_id;
	params.shared_addr = (void *)shared_addr;
	params.line_id = LINE_ID;
	params.shared_addr_size = ipu_pm_mem_req(NULL);

	handle = ipu_pm_create(&params);

	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	retval = ipu_pm_init_transport(handle);

	if (retval < 0)
		goto exit;

	return retval;
exit:
	pr_err("ipu_pm_attach failed! retval = 0x%x", retval);
	return retval;
}
EXPORT_SYMBOL(ipu_pm_attach);

/*
  Function to deattach ipu pm object
  This function is called in ipc_detach()
  This function will delete the object based
  on the remoteproc id and unregister the notify
  events used by ipu_pm module
 */
int ipu_pm_detach(u16 remote_proc_id)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	int retval = 0;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(remote_proc_id);
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	/* unregister the events used for ipu_pm */
	retval = notify_unregister_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_resource_event | (NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_callback,
		(void *)NULL);
	if (retval < 0) {
		pr_err("Error registering notify event\n");
		goto exit;
	}
	retval = notify_unregister_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_notification_event | (NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_notify_callback,
		(void *)NULL);
	if (retval < 0) {
		pr_err("Error registering notify event\n");
		goto exit;
	}

	/* Deleting the handle based on remote_proc_id */
	ipu_pm_delete(handle);
	return retval;
exit:
	pr_err("ipu_pm_detach failed handle null retval 0x%x", retval);
	return retval;
}
EXPORT_SYMBOL(ipu_pm_detach);

/*
  Function to destroy ipu_pm module
  this function will destroy the structs
  created to set the configuration
 */
int ipu_pm_destroy(void)
{
	int retval = 0;
	struct mutex *lock = NULL;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&ipu_pm_state.ref_count,
				IPU_PM_MAKE_MAGICSTAMP(0),
				IPU_PM_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}

	if (!(atomic_dec_return(&ipu_pm_state.ref_count)
					== IPU_PM_MAKE_MAGICSTAMP(0))) {
		retval = 1;
		goto exit;
	}

	if (WARN_ON(ipu_pm_state.gate_handle == NULL)) {
		retval = -ENODEV;
		goto exit;
	}

	retval = mutex_lock_interruptible(ipu_pm_state.gate_handle);
	if (retval)
		goto exit;

	lock = ipu_pm_state.gate_handle;
	ipu_pm_state.gate_handle = NULL;
	mutex_unlock(lock);
	kfree(lock);
	/* Delete the wq for req/rel resources */
	destroy_workqueue(ipu_wq);
	return retval;

exit:
	if (retval < 0) {
		pr_err("ipu_pm_destroy failed, retval: %x\n", retval);
	}
	return retval;
}
EXPORT_SYMBOL(ipu_pm_destroy);
