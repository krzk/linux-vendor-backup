/*
* ipu_pm.h
*
* Syslink IPU Power Managament support functions for TI OMAP processors.
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
*
*  --------------------------------------------------------------------
* | rcb_num                             |      |        |       |      |
* | msg_type                            |      |        |       |      |
* | sub_type                            |      |        |       |      |
* | rqst_cpu                            |    1-|word    |       |      |
* | extd_mem_flag                       |      |        |       |      |
* | num_chan                            |      |        |       |      |
* | fill9                               |      |        |       |      |
* |-------------------------------------|    ------  4-words 4-words   |
* | process_id                          |    1-word     |       |      |
* |-------------------------------------|    ------     |       |      |
* | sem_hnd                             |    1-word     |       |      |
* |-------------------------------------|    ------     |       |      |
* | mod_base_addr                       |    1-word     |       |      |
* |-------------------------------------|    ------   -----   -----    |
* | channels[0]  | data[0] | datax[0]   |      |        |       |      |
* | channels[1]  |         |            |    1-word     |       |   RCB_SIZE
* | channels[2]  |         |            |      |        |       |      =
* | channels[3]  |         |            |      |        |       |    8WORDS
* |--------------|---------|------------|    ------     |       |      |
* | channels[4]  | data[0] | datax[1]   |      |        |       |      |
* | channels[5]  |         |            |    1-word     |  RCB_SIZE-5  |
* | channels[6]  |         |            |      |        |       |      |
* | channels[7]  |         |            |      |     RCB_SIZE-4 |      |
* |--------------|---------|------------|    ------     |       |      |
* | channels[8]  | data[0] | datax[2]   |      |        |       |      |
* | channels[9]  |         |            |    1-word     |       |      |
* | channels[10] |         |            |      |        |       |      |
* | channels[11] |         |            |      |        |       |      |
* |--------------|---------|------------|    ------     |     -----    |
* | channels[12] | data[0] |extd_mem_hnd|      |        |       |      |
* | channels[13] |         |            |    1-word     |     1-word   |
* | channels[14] |         |            |      |        |       |      |
* | channels[15] |         |            |      |        |       |      |
*  --------------------------------------------------------------------
*
*The Ducati Power Management sub-system uses a structure called RCB_struct or
*just RCB to share information with the MPU about a particular resource involved
*in the communication. The information stored in this structure is needed to get
*attributes and other useful data about the resource.
*The fisrt fields of the RCB resemble the Rcb message sent across the NotifyDver
*It retains the rcb_num, msg_type and msg_subtype from the rcb message as its
*first 3 fields. The rqst_cpu fields indicates which remote processor originates
*the request/release petition. When a particular resource is requested, some of
*its parameters should be specify.
*For devices like Gptimer and GPIO, the most significant attribute its itemID.
*This value should be placed in the "fill9" field of the Rcb sruct. This field
*should be fill by the requester if asking for a particular resource or by the
*receiver if the resource granted is other than the one asked.
*
*Other variables related with the resource are:
*"sem_hnd" which storage the semaphore handle associated in the ducati side.
*We are pending on this semaphore when asked for the resource and
*posted when granted.
*"mod_base_addr". It is the virtual base addres for the resource.
*"process_id". It is the Task Id where the petition for the resource was called.
*
*The last 16 bytes of the structure could be interpreted in 3 different ways
*according to the context.
*1) For the case of the Rcb is for SDMA. The last 16 bytes correspond to a array
*  of 16 channels[ ]. Each entry has the number of the SDMA channel granted.
*  As many number of channels indicated in num_chan as many are meaningful
*  in the channels[] array.
*2) If the extd_mem_flag bit is NOT set the 16 last bytes are used as a data[]
*  array. Each entry is 4bytes long so the maximum number of entries is 4.
*3) If the extd_mem_flag bit is NOT set the 16 last bytes are used as an array
*  datax[ ]  3 members Each entry  4bytes long  and one additional field of
*  "extd_mem_hnd" which is a pointer to the continuation of this datax array
*/

#ifndef _IPU_PM_H_
#define _IPU_PM_H_

#include <linux/types.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>

/* Pm notify ducati driver */
/* Suspend/resume/other... */
#define NUMBER_PM_EVENTS 4

#define PM_CSTR_PERF_MASK	0x00000001
#define PM_CSTR_LAT_MASK	0x00000002
#define PM_CSTR_BW_MASK		0x00000004

#define RCB_SIZE 8

#define DATA_MAX (RCB_SIZE - 4)
#define DATAX_MAX (RCB_SIZE - 5)
#define SDMA_CHANNELS_MAX 16
#define I2C_BUS_MIN 1
#define I2C_BUS_MAX 4
#define REGULATOR_MIN 1
#define REGULATOR_MAX 1
#define AUX_CLK_MIN 1
#define AUX_CLK_MAX 3

#define GP_TIMER_3 3
#define GP_TIMER_4 4
#define GP_TIMER_9 9
#define GP_TIMER_11 11
#define NUM_IPU_TIMERS 4

#define I2C_SL_INVAL -1
#define I2C_1_SL 0
#define I2C_2_SL 1
#define I2C_3_SL 2
#define I2C_4_SL 3

#define RCB_MIN 1
#define RCB_MAX 33

#define PM_RESOURCE 2
#define PM_NOTIFICATION 3
#define PM_SUCCESS 0
#define PM_FAILURE -1
#define PM_SHM_BASE_ADDR 0x9cff0000

/* Auxiliar Clocks Registers */
#define SCRM_BASE			OMAP2_L4_IO_ADDRESS(0x4a30A000)
#define SCRM_BASE_AUX_CLK		0x00000314
#define SCRM_BASE_AUX_CLK_REQ		0x00000214
#define SCRM_AUX_CLK_OFFSET		0x4
/* Auxiliar Clocks bit fields */
#define SCRM_AUX_CLK_POLARITY		0x0
#define SCRM_AUX_CLK_SRCSELECT		0x1
#define SCRM_AUX_CLK_ENABLE		0x8
#define SCRM_AUX_CLK_CLKDIV		0x10
/* Auxiliar Clocks Masks */
#define SCRM_AUX_CLK_POLARITY_MASK	0x00000001
#define SCRM_AUX_CLK_SRCSELECT_MASK	0x00000006
#define SCRM_AUX_CLK_ENABLE_MASK	0x00000100
#define SCRM_AUX_CLK_CLKDIV_MASK	0x000F0000
/* Auxiliar Clocks Request bit fields */
#define SCRM_AUX_CLK_REQ_POLARITY	0x0
#define SCRM_AUX_CLK_REQ_ACCURACY	0x1
#define SCRM_AUX_CLK_REQ_MAPPING	0x2
/* Auxiliar Clocks Request Masks */
#define SCRM_AUX_CLK_REQ_POLARITY_MASK	0x00000001
#define SCRM_AUX_CLK_REQ_ACCURACY_MASK	0x00000002
#define SCRM_AUX_CLK_REQ_MAPPING_MASK	0x0000001C

/* Macro to set a val in a bitfield*/
#define MASK_SET_FIELD(tmp, bitfield, val)	{	\
						tmp |=	\
						((val << SCRM_##bitfield)\
						& SCRM_##bitfield##_MASK);\
						}

/* Macro to return the address of the aux clk */
#define AUX_CLK_REG(clk)	(SCRM_BASE + (SCRM_BASE_AUX_CLK + \
					(SCRM_AUX_CLK_OFFSET * clk)))

/*
 *  IPU_PM_MODULEID
 *  Unique module ID
 */
#define IPU_PM_MODULEID      (0x6A6A)
/* A9 state flag 0000 | 0000 Ducati internal use*/
#define REMOTE_PROC_DOWN      0x00010000

/* Macro to make a correct module magic number with refCount */
#define IPU_PM_MAKE_MAGICSTAMP(x) ((IPU_PM_MODULEID << 12u) | (x))

enum pm_failure_codes{
	PM_INSUFFICIENT_CHANNELS = 1,
	PM_NO_GPTIMER,
	PM_NO_GPIO,
	PM_NO_I2C,
	PM_NO_REGULATOR,
	PM_REGULATOR_IN_USE,
	PM_INVAL_RCB_NUM,
	PM_INVAL_NUM_CHANNELS,
	PM_INVAL_NUM_I2C,
	PM_INVAL_REGULATOR,
	PM_NOT_INSTANTIATED,
	PM_UNSUPPORTED,
	PM_NO_AUX_CLK,
	PM_INVAL_AUX_CLK
};

enum pm_msgtype_codes{PM_FAIL,
	PM_NULLMSG,
	PM_ACKNOWLEDGEMENT,
	PM_REQUEST_RESOURCE,
	PM_RELEASE_RESOURCE,
	PM_NOTIFICATIONS,
	PM_ENABLE_RESOURCE,
	PM_WRITE_RESOURCE,
	PM_READ_RESOURCE,
	PM_DISABLE_RESOURCE,
	PM_REQUEST_CONSTRAINTS,
	PM_RELEASE_CONSTRAINTS,
	PM_NOTIFY_HIBERNATE
};

enum pm_regulator_action{PM_SET_VOLTAGE,
	PM_SET_CURRENT,
	PM_SET_MODE,
	PM_GET_MODE,
	PM_GET_CURRENT,
	PM_GET_VOLTAGE
};

enum res_type{
	IVA_HD,
	ISS,
	SDMA,
	GP_TIMER,
	GP_IO,
	I2C,
	REGULATOR,
	AUX_CLK,
	SYSM3,
	APPM3,
	L3_BUS,
};

enum pm_event_type{PM_SUSPEND,
	PM_RESUME,
	PM_PID_DEATH,
	PM_HIBERNATE
};

struct rcb_message {
	unsigned rcb_flag:1;
	unsigned rcb_num:6;
	unsigned reply_flag:1;
	unsigned msg_type:4;
	unsigned msg_subtype:4;
	unsigned parm:16;
};

union message_slicer {
	struct rcb_message fields;
	int whole;
};

struct rcb_block {
	unsigned rcb_num:6;
	unsigned msg_type:4;
	unsigned sub_type:4;
	unsigned rqst_cpu:4;
	unsigned extd_mem_flag:1;
	unsigned num_chan:4;
	unsigned fill9:9;

	unsigned process_id;
	unsigned *sem_hnd;
	unsigned mod_base_addr;
	union {
		unsigned int data[DATA_MAX];
		struct {
			unsigned datax[DATAX_MAX];
			unsigned extd_mem_hnd;
		};
		unsigned char channels[SDMA_CHANNELS_MAX];
	};
};

struct sms {
	unsigned rat;
	unsigned pm_version;
	unsigned gp_msg;
	unsigned state_flag;
	struct rcb_block rcb[RCB_MAX];
};

struct pm_event {
	enum pm_event_type event_type;
	struct semaphore sem_handle;
	int pm_msg;
};

struct ipu_pm_params {
	int pm_gptimer_counter;
	int pm_gpio_counter;
	int pm_sdmachan_counter;
	int pm_i2c_bus_counter;
	int pm_regulator_counter;
	int pm_aux_clk_counter;
	int pm_sys_m3_counter;
	int pm_app_m3_counter;
	int pm_l3_bus_counter;
	int pm_iva_hd_counter;
	int pm_iss_counter;
	int timeout;
	void *shared_addr;
	int shared_addr_size;
	int pm_num_events;
	int pm_resource_event;
	int pm_notification_event;
	int proc_id;
	int remote_proc_id;
	int line_id;
	void *gate_mp;
};

/* This structure defines attributes for initialization of the ipu_pm module. */
struct ipu_pm_config {
	u32 reserved;
};

/* Defines the ipu_pm state object, which contains all the module
 * specific information. */
struct ipu_pm_module_object {
	atomic_t ref_count;
	/* Reference count */
	struct ipu_pm_config cfg;
	/* ipu_pm configuration structure */
	struct ipu_pm_config def_cfg;
	/* Default module configuration */
	struct mutex *gate_handle;
	/* Handle of gate to be used for local thread safety */
	bool is_setup;
	/* Indicates whether the ipu_pm module is setup. */
};

/* Store the payload and processor id for the wq */
struct ipu_pm_msg {
	u16 proc_id;
	int pm_msg;
};

/* ipu_pm handle one for each proc SYSM3/APPM3 */
struct ipu_pm_object {
	struct sms *rcb_table;
	struct pm_event *pm_event;
	struct ipu_pm_params *params;
	struct work_struct work;
	struct kfifo *fifo;
	spinlock_t lock;
};

/* Function for PM resources Callback */
void ipu_pm_callback(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload);

/* Function for PM notifications Callback */
void ipu_pm_notify_callback(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload);

/* Function for send PM Notifications */
int ipu_pm_notifications(enum pm_event_type event_type, void *data);

/* Function to set init parameters */
void ipu_pm_params_init(struct ipu_pm_params *params);

/* Function to calculate ipu pm mem */
int ipu_pm_mem_req(const struct ipu_pm_params *params);

/* Function to config ipu_pm module */
void ipu_pm_get_config(struct ipu_pm_config *cfg);

/* Function to set up ipu_pm module */
int ipu_pm_setup(struct ipu_pm_config *cfg);

/* Function to create ipu pm object */
struct ipu_pm_object *ipu_pm_create(const struct ipu_pm_params *params);

/* Function to delete ipu pm object */
void ipu_pm_delete(struct ipu_pm_object *handle);

/* Function to destroy ipu_pm module */
int ipu_pm_destroy(void);

/* Function to attach ipu_pm module */
int ipu_pm_attach(u16 remote_proc_id, void *shared_addr);

/* Function to deattach ipu_pm module */
int ipu_pm_detach(u16 remote_proc_id);

/* Function to register the ipu_pm events */
int ipu_pm_init_transport(struct ipu_pm_object *handle);

/* Function to get ipu pm object */
struct ipu_pm_object *ipu_pm_get_handle(int proc_id);

/* Function to save a processor from hibernation */
int ipu_pm_save_ctx(int proc_id);

/* Function to restore a processor from hibernation */
int ipu_pm_restore_ctx(int proc_id);

#endif
