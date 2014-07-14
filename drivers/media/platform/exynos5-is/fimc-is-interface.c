/*
 * Samsung EXYNOS5 FIMC-IS (Imaging Subsystem) driver
*
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 * Kil-yeon Lim <kilyeon.im@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "fimc-is.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-backend.h"
#include "fimc-is-fw.h"

#define FIMC_IS_GROUP_ID_3A0	0 /* hardware: CH0 */
#define FIMC_IS_GROUP_ID_3A1	1 /* hardware: 3AA */
#define FIMC_IS_GROUP_ID_ISP	2 /* hardware: CH1 */
#define FIMC_IS_GROUP_ID_DIS	3
#define FIMC_IS_GROUP_ID_SHIFT	16
#define FIMC_IS_GROUP_ID 					\
        ((FIMC_IS_GROUP_ID_3A1  | (1 <<FIMC_IS_GROUP_ID_ISP)) 	\
         << FIMC_IS_GROUP_ID_SHIFT)

#define init_request_barrier(itf) mutex_init(&itf->request_barrier)
#define enter_request_barrier(itf) mutex_lock(&itf->request_barrier)
#define exit_request_barrier(itf) mutex_unlock(&itf->request_barrier)

struct workqueue_struct *fimc_is_workqueue;

#ifdef FIMC_IS_DEBUG
#define read_shared_reg(__r, __offset) ({			\
        pr_info("[FIMC IS ITF] Reading shared reg: base: 0x%08X"\
                " offset: 0x%08X\n",				\
                (unsigned int)(__r), (unsigned int)(__offset)); \
                readl((__r) + (__offset));			\
})

#define write_shared_reg(__r, __offset, __v) ({				\
        pr_info("[FIMC IS ITF]  Writting shared reg: 0x%08x"		\
                " offset:0x%08x (%d)\n",					\
                (unsigned int)(__r), (unsigned int)(__offset), __v);	\
                writel((__v), (__r) + (__offset));			\
})

#define dump_message(msg) 							\
do{										\
        struct fimc_is_msg* __m =(struct fimc_is_msg*) msg;			\
        printk("[FIMC IS MSG] >> %s: \n", __func__);				\
        printk("[FIMC IS MSG] ID [%d] COMMAND [%d] INSTANCE [%d]\n", 		\
                __m->id, __m->command, __m->instance);				\
        printk("[FIMC IS MSG] PARAMS: {%d, %d, %d, %d}\n", 			\
                __m->param[0], __m->param[1], __m->param[2], __m->param[3]); 	\
}while(0)

#else
#define read_shared_reg(__r, __offset) \
        readl((__r) +(__offset))
#define write_shared_reg(__r, __offset, __v) \
        writel(__v, (__r) + (__offset))

#define dump_message(msg)

#endif


static inline void setup_cmd(struct fimc_is_msg *msg,
                             void __iomem *base_reg,
                             u32 offset,
                             u32 param_count)
{
        /*
         * Base command layout ( consecutive 32 bits registers )
         * | REG_0 | -> | command ID          |
         * | REG_1 | -> | sesnor ID (instance)|
         * | REG_2 | -> | commads params      |
         */
        memset(msg, 0, sizeof(msg));
        msg->command  = read_shared_reg(base_reg, offset);
        msg->instance = read_shared_reg(base_reg, offset + MCUCTL_SREG_SIZE);
        memcpy(msg->param, base_reg + offset + 2*MCUCTL_SREG_SIZE,
               param_count * sizeof(msg->param[0]));
}

static inline void write_hic_shared_reg(struct fimc_is_interface *itf,
                                        struct fimc_is_msg *msg)
{
        struct mcuctl_sreg_desc shared_reg_desc;
        void __iomem *base_reg = itf->shared_regs;
        /* It is safe to assume that the calls to get the
         * MCUCTL_HIC_REG data will always succeede despite the
         * actual firmware version
         */
        mcuctl_sreg_get_desc(itf->fw_data, MCUCTL_HIC_REG,
                             &shared_reg_desc);
        write_shared_reg(base_reg, shared_reg_desc.base_offset, msg->command);
        write_shared_reg(base_reg,
                         shared_reg_desc.base_offset + MCUCTL_SREG_SIZE,
                         msg->instance);
        memcpy(base_reg + shared_reg_desc.base_offset + 2*MCUCTL_SREG_SIZE,
               msg->param, shared_reg_desc.data_range * sizeof(u32));

}

static inline int itf_get_cmd(struct fimc_is_interface *itf,
        struct fimc_is_msg *msg, unsigned int index)
{
        void __iomem *regs = itf->shared_regs;
        unsigned int reg_range_id;
        struct mcuctl_sreg_desc shared_reg_desc;

	switch (index) {
	case INTR_GENERAL:
                reg_range_id = MCUCTL_IHC_REG;
                break;
        case INTR_3A0C_DONE:
                reg_range_id = MCUCTL_3AA0C_REG;
		break;
        case INTR_SCC_DONE:
                reg_range_id = MCUCTL_SCC_REG;
		break;
        case INTR_SCP_DONE:
                reg_range_id = MCUCTL_SCP_REG;
		break;
	case INTR_META_DONE:
                reg_range_id = MCUCTL_META_REG;
		break;
	case INTR_SHOT_DONE:
                reg_range_id = MCUCTL_SHOT_REG;
		break;
	default:
                reg_range_id = MCUCTL_END_REG;
		dev_err(itf->dev, "%s Unknown command\n", __func__);
		break;
	}

        if (!mcuctl_sreg_get_desc(itf->fw_data, reg_range_id,
                                  &shared_reg_desc)) {
                if (index == INTR_GENERAL) {
                /*
                 * Move to the very next register
                 * @see generic MCUCTL shared reg layout
                 */
                shared_reg_desc.base_offset += MCUCTL_SREG_SIZE;
                }
                setup_cmd(msg, regs, shared_reg_desc.base_offset,
                          shared_reg_desc.data_range);
                switch(index) {
                        case INTR_3A0C_DONE:
                        case INTR_SCC_DONE:
                        case INTR_SCP_DONE:
                        case INTR_SHOT_DONE:
                                msg->command  = IHC_FRAME_DONE;
                        default:
                                break;
                }
                return 0;
        }
        return -EINVAL;
}

static inline unsigned int itf_get_intr(struct fimc_is_interface *itf)
{
	unsigned int status;
        void __iomem *shared_regs = itf->shared_regs;
        unsigned long offset;
        struct fimc_is_fw_data *fw_data = itf->fw_data;

        status = read_shared_reg(itf->regs, INTMSR1);
        /*
         * Again it is safe to assume that the following registers
         * are always valid despite the actual firmware version
         */
        mcuctl_sreg_get_offset(fw_data, MCUCTL_IHC_REG, &offset);
        status |= read_shared_reg(shared_regs, offset);

        mcuctl_sreg_get_offset(fw_data, MCUCTL_SHOT_REG, &offset);
        status |= read_shared_reg(shared_regs, offset);

        mcuctl_sreg_get_offset(fw_data, MCUCTL_SCC_REG, &offset);
        status |= read_shared_reg(shared_regs, offset);

        mcuctl_sreg_get_offset(fw_data, MCUCTL_SCP_REG, &offset);
        status |= read_shared_reg(shared_regs, offset);

        if (mcuctl_sreg_is_valid(fw_data, MCUCTL_META_REG)) {
                mcuctl_sreg_get_offset(fw_data, MCUCTL_META_REG, &offset);
                status |= read_shared_reg(shared_regs, offset);
        }
	return status;
}

static void itf_set_state(struct fimc_is_interface *itf,
		unsigned long state)
{
	unsigned long flags;
	spin_lock_irqsave(&itf->slock_state, flags);
	__set_bit(state, &itf->state);
	spin_unlock_irqrestore(&itf->slock_state, flags);
}

static void itf_clr_state(struct fimc_is_interface *itf,
		unsigned long state)
{
	unsigned long flags;
	spin_lock_irqsave(&itf->slock_state, flags);
	__clear_bit(state, &itf->state);
	spin_unlock_irqrestore(&itf->slock_state, flags);
}

static int itf_get_state(struct fimc_is_interface *itf,
		unsigned long state)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&itf->slock_state, flags);
	ret = test_bit(state, &itf->state);
	spin_unlock_irqrestore(&itf->slock_state, flags);
	return ret;
}

static void itf_init_wakeup(struct fimc_is_interface *itf)
{
	itf_set_state(itf, IS_IF_STATE_INIT);
	wake_up(&itf->irq_queue);
}

void itf_busy_wakeup(struct fimc_is_interface *itf)
{
	itf_clr_state(itf, IS_IF_STATE_BUSY);
	wake_up(&itf->irq_queue);
}

static int itf_wait_hw_ready(struct fimc_is_interface *itf)
{
	int t;
	for (t = TRY_RECV_AWARE_COUNT; t >= 0; t--) {
		unsigned int cfg = readl(itf->regs + INTMSR0);
		if (INTMSR0_GET_INTMSD(0, cfg) == 0)
			return 0;

	}
	dev_err(itf->dev, "INTMSR0's 0 bit is not cleared.\n");
	return -EINVAL;
}

static int itf_wait_idlestate(struct fimc_is_interface *itf)
{
	int ret;

	ret = wait_event_timeout(itf->irq_queue,
			!itf_get_state(itf, IS_IF_STATE_BUSY),
			FIMC_IS_COMMAND_TIMEOUT);
	if (!ret) {
		dev_err(itf->dev, "%s Timeout\n", __func__);
		return -ETIME;
	}
	return 0;
}

int fimc_is_itf_wait_init_state(struct fimc_is_interface *itf)
{
	int ret;

	ret = wait_event_timeout(itf->irq_queue,
		itf_get_state(itf, IS_IF_STATE_INIT),
		FIMC_IS_STARTUP_TIMEOUT);

	if (!ret) {
		dev_err(itf->dev, "%s Timeout\n", __func__);
		return -ETIME;
	}
	return 0;
}

/* Send Host to IS command interrupt */
static void itf_hic_interrupt(struct fimc_is_interface *itf)
{
	writel(INTGR0_INTGD(0), itf->regs + INTGR0);
}

static int itf_send_sensor_number(struct fimc_is_interface *itf)
{
	struct fimc_is_msg msg = {
		.command = ISR_DONE,
		.param = { IHC_GET_SENSOR_NUMBER, 1 },
	};
	unsigned long flags;

	spin_lock_irqsave(&itf->slock, flags);
        write_hic_shared_reg(itf, &msg);
	itf_hic_interrupt(itf);
	spin_unlock_irqrestore(&itf->slock, flags);

	return 0;
}

static int fimc_is_itf_set_cmd(struct fimc_is_interface *itf,
	struct fimc_is_msg *msg)
{
	int ret = 0;
	bool block_io = true;
	unsigned long flags;

	enter_request_barrier(itf);

	switch (msg->command) {
	case HIC_STREAM_ON:
		if (itf->streaming == IS_IF_STREAMING_ON)
			goto exit;
		break;
	case HIC_STREAM_OFF:
		if (itf->streaming == IS_IF_STREAMING_OFF)
			goto exit;
		break;
	case HIC_PROCESS_START:
		if (itf->processing == IS_IF_PROCESSING_ON)
			goto exit;
		break;
	case HIC_PROCESS_STOP:
		if (itf->processing == IS_IF_PROCESSING_OFF)
			goto exit;
		break;
	case HIC_POWER_DOWN:
		if (itf->pdown_ready == IS_IF_POWER_DOWN_READY)
			goto exit;
		break;
	case HIC_SHOT:
	case ISR_DONE:
		block_io = false;
		break;
	}

	ret = itf_wait_hw_ready(itf);
	if (ret) {
		dev_err(itf->dev, "%s: itf_wait_hw_ready() failed", __func__);
		ret = -EBUSY;
		goto exit;
	}

	spin_lock_irqsave(&itf->slock, flags);
	itf_set_state(itf, IS_IF_STATE_BUSY);
        write_hic_shared_reg(itf, msg);
	itf_hic_interrupt(itf);
	spin_unlock_irqrestore(&itf->slock, flags);

	if (!block_io)
		goto exit;

	ret = itf_wait_idlestate(itf);
	if (ret) {
                dev_err(itf->dev, "Timeout on command id: %d\n", msg->command);
		itf_clr_state(itf, IS_IF_STATE_BUSY);
		ret = -ETIME;
		goto exit;
	}

	if (itf->reply.command == ISR_DONE) {

                /* @TODO : locking ? */
		switch (msg->command) {
		case HIC_STREAM_ON:
			itf->streaming = IS_IF_STREAMING_ON;
			break;
		case HIC_STREAM_OFF:
			itf->streaming = IS_IF_STREAMING_OFF;
			break;
		case HIC_PROCESS_START:
			itf->processing = IS_IF_PROCESSING_ON;
			break;
		case HIC_PROCESS_STOP:
			itf->processing = IS_IF_PROCESSING_OFF;
			break;
		case HIC_POWER_DOWN:
			itf->pdown_ready = IS_IF_POWER_DOWN_READY;
			break;
		case HIC_OPEN_SENSOR:
			if (itf->reply.param[0] == HIC_POWER_DOWN) {
				dev_err(itf->dev, "firmware power down");
				itf->pdown_ready = IS_IF_POWER_DOWN_READY;
				ret = -ECANCELED;
				goto exit;
                        } else {
                                /* Fix possible state-machine inconsistency */
                                itf->streaming   = IS_IF_STREAMING_OFF;
                                itf->processing  = IS_IF_PROCESSING_OFF;
				itf->pdown_ready = IS_IF_POWER_DOWN_NREADY;
                        }
			break;
		default:
			break;
		}
	} else {
		dev_err(itf->dev, "ISR_NDONE occured");
		ret = -EINVAL;
	}
exit:
	exit_request_barrier(itf);

	if (ret)
		dev_err(itf->dev, "Error returned from FW. See debugfs for error log\n");

	return ret;
}

static int fimc_is_itf_set_cmd_shot(struct fimc_is_interface *itf,
		struct fimc_is_msg *msg)
{
	unsigned long flags;
        struct mcuctl_sreg_desc shared_reg_desc;
	spin_lock_irqsave(&itf->slock, flags);
        write_hic_shared_reg(itf, msg);

        if (!mcuctl_sreg_get_desc(itf->fw_data, MCUCTL_FRAME_COUNT_REG,
                                  &shared_reg_desc)) {
                unsigned long offset = shared_reg_desc.base_offset;
                /* Frame count for sensor:0 */
                offset += (--shared_reg_desc.data_range) * MCUCTL_SREG_SIZE;
                write_shared_reg(itf->shared_regs, offset, msg->param[2]);
        }

	itf_hic_interrupt(itf);
	spin_unlock_irqrestore(&itf->slock, flags);
	return 0;
}

static int itf_init_workqueue(void)
{
        fimc_is_workqueue = alloc_workqueue("fimc-is", WQ_FREEZABLE,0);
        return fimc_is_workqueue ? 0 : -ENOMEM;
}

static void shot_done_work_fn(struct work_struct* work)
{
        struct fimc_is_interface *itf = container_of(work,
                         struct fimc_is_interface, shot_done_work);
        struct fimc_is *is = fimc_interface_to_is(itf);
        fimc_is_pipeline_shot(&is->pipeline[0]);
}

static void itf_handle_general(struct fimc_is_interface *itf,
		struct fimc_is_msg *msg)
{
	bool is_blocking = true;

	switch (msg->command) {

	case IHC_GET_SENSOR_NUMBER:
		pr_debug("IS version : %d.%d\n",
                        itf->drv_version, msg->param[0]);
                if (fimc_is_fw_config(&itf->fw_data, msg->param[0]))
                        dev_err(itf->dev,
			"Failed to initialize fw data. Using default settings");
		/* Respond with sensor number */
		itf_send_sensor_number(itf);
		itf_init_wakeup(itf);
		break;
	case ISR_DONE:
		switch (msg->param[0]) {
		case HIC_OPEN_SENSOR:
			pr_debug("open done\n");
			break;
                case HIC_CLOSE_SENSOR:
                        pr_debug("close done\n");
                        break;
		case HIC_GET_SET_FILE_ADDR:
			pr_debug("saddr(%p) done\n",
				(void *)msg->param[1]);
			break;
		case HIC_LOAD_SET_FILE:
			pr_debug("setfile done\n");
			break;
		case HIC_SET_A5_MEM_ACCESS:
			pr_debug("cfgmem done\n");
			break;
		case HIC_PROCESS_START:
			pr_debug("process_on done\n");
			break;
		case HIC_PROCESS_STOP:
			pr_debug("process_off done\n");
			break;
		case HIC_STREAM_ON:
			pr_debug("stream_on done\n");
			break;
		case HIC_STREAM_OFF:
			pr_debug("stream_off done\n");
			break;
		case HIC_SET_PARAMETER:
			pr_debug("s_param done\n");
			break;
		case HIC_GET_STATIC_METADATA:
			pr_debug("g_capability done\n");
			break;
		case HIC_PREVIEW_STILL:
			pr_debug("a_param(%dx%d) done\n",
					msg->param[1], msg->param[2]);
			break;
		case HIC_POWER_DOWN:
			pr_debug("powerdown done\n");
			break;
		/* Non-blocking command */
		case HIC_SHOT:
			is_blocking = false;
			dev_err(itf->dev, "shot done is not acceptable\n");
			break;
		case HIC_SET_CAM_CONTROL:
			is_blocking = false;
			dev_err(itf->dev, "camctrl is not acceptable\n");
			break;
                case HIC_SENSOR_MODE_CHANGE:
                        pr_debug("Sensor mode changed\n");
                        break;
                case HIC_GET_IP_STATUS:
                        break;
		default:
			is_blocking = false;
			dev_err(itf->dev, "unknown done is invokded\n");
			break;
		}
		break;
	case ISR_NDONE:
		switch (msg->param[0]) {
		case HIC_SHOT:
			is_blocking = false;
			dev_err(itf->dev, "shot NOT done is not acceptable\n");
			break;
		case HIC_SET_CAM_CONTROL:
			is_blocking = false;
			pr_debug("camctrl NOT done\n");
			break;
		case HIC_SET_PARAMETER:
			dev_err(itf->dev, "s_param NOT done\n");
			dev_err(itf->dev, "param2 : 0x%08X\n", msg->param[1]);
			dev_err(itf->dev, "param3 : 0x%08X\n", msg->param[2]);
			dev_err(itf->dev, "param4 : 0x%08X\n", msg->param[3]);
			break;
                case HIC_GET_IP_STATUS:
                        break;
		default:
			dev_err(itf->dev, "command(%d) not done",
					msg->param[0]);
			break;
		}
		break;
	case IHC_SET_FACE_MARK:
		is_blocking = false;
		dev_err(itf->dev, "FACE_MARK(%d,%d,%d) is not acceptable\n",
			msg->param[0], msg->param[1], msg->param[2]);
		break;
	case IHC_AA_DONE:
		is_blocking = false;
		dev_err(itf->dev, "AA_DONE(%d,%d,%d) is not acceptable\n",
			msg->param[0], msg->param[1], msg->param[2]);
		break;
	case IHC_FLASH_READY:
		is_blocking = false;
		dev_err(itf->dev, "IHC_FLASH_READY is not acceptable");
		break;
	case IHC_NOT_READY:
		is_blocking = false;
                dev_err(itf->dev, "IHC_NOT_READY has been reported: reset required\n");
		break;
        case IHC_REPORT_ERR:
                is_blocking = false;
                dev_err(itf->dev, "IHC_REPORT_ERR has occured");
	default:
		is_blocking = false;
		dev_err(itf->dev, "%s: unknown (#%08X) command\n",
				__func__, msg->command);
		break;
	}

	if (is_blocking) {
		memcpy(&itf->reply, msg, sizeof(struct fimc_is_msg));
		itf_busy_wakeup(itf);
	}
}

static void itf_handle_scaler_done(struct fimc_is_interface *itf,
		struct fimc_is_msg *msg)
{
	struct fimc_is *is = fimc_interface_to_is(itf);
	struct fimc_is_pipeline *pipeline = &is->pipeline[msg->instance];
	struct fimc_is_buf *buf;
	struct fimc_is_scaler *scl;
	const struct fimc_is_fmt *fmt;
	struct timeval *tv;
	struct timespec ts;
	unsigned int wh, i;
	unsigned int fcount = msg->param[0];
        unsigned long *subip_state;

        /*
         * Scaler done message layout
         * 1: frame count
         * 2: status != 0 -> frame not done
         * 3: requested frame count
         */
	if (msg->param[3] == SCALER_SCC) {
		scl = &pipeline->scaler[SCALER_SCC];
                subip_state = &pipeline->subip_state[IS_SCC];

	} else {
		scl = &pipeline->scaler[SCALER_SCP];
                subip_state = &pipeline->subip_state[IS_SCP];
	}

	fmt = scl->fmt;

	fimc_is_pipeline_buf_lock(pipeline);
	if (!list_empty(&scl->run_queue)) {

		wh = scl->width * scl->height;
		buf = fimc_is_scaler_run_queue_get(scl);
		for (i = 0; i < fmt->num_planes; i++)
			vb2_set_plane_payload(&buf->vb, i,
					(wh * fmt->depth[i]) / 8);

		/* Set timestamp */
		ktime_get_ts(&ts);
		tv = &buf->vb.v4l2_buf.timestamp;
		tv->tv_sec = ts.tv_sec;
		tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
		buf->vb.v4l2_buf.sequence = fcount;

		pr_debug("SCP buffer done %d/%d\n",
				msg->param[0], msg->param[2]);
                if (msg->param[1])
                        vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
                else
                        vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
	}
	fimc_is_pipeline_buf_unlock(pipeline);
        clear_bit(COMP_RUN, subip_state);
	wake_up(&scl->event_q);
}

static void itf_handle_shot_done(struct fimc_is_interface *itf,
		struct fimc_is_msg *msg)
{
	struct fimc_is *is = fimc_interface_to_is(itf);
        struct fimc_is_pipeline *pipeline =
                        &is->pipeline[msg->instance &~FIMC_IS_GROUP_ID];
	unsigned int status = msg->param[1];
	struct fimc_is_buf *bayer_buf;

        /*
         * Shot done message layout:
         * 1: frame count
         * 2: status
         * 3: status for shot: 0 - success
         */

	/* DQ the bayer input buffer */
	fimc_is_pipeline_buf_lock(pipeline);
	bayer_buf = fimc_is_isp_run_queue_get(&pipeline->isp);
	if (bayer_buf) {
                if (msg->param[2])
                        vb2_buffer_done(&bayer_buf->vb, VB2_BUF_STATE_ERROR);
                else
                        vb2_buffer_done(&bayer_buf->vb, VB2_BUF_STATE_DONE);
		pr_debug("Bayer buffer done.\n");
	}
	fimc_is_pipeline_buf_unlock(pipeline);

	/* Clear state & call shot again */
	clear_bit(PIPELINE_RUN, &pipeline->state);
        clear_bit(COMP_RUN, &pipeline->subip_state[IS_ISP]);
        queue_work(fimc_is_workqueue, &itf->shot_done_work);
        if (status != ISR_DONE) {
                fimc_is_pipeline_reset(pipeline);
                dev_err(itf->dev, "Shot failure (0x%08X : 0x%08X) \n",
                        status,  msg->param[2]);

        }
}

static inline void fimc_is_handle_irq(struct fimc_is_interface *itf,
                               struct fimc_is_msg *msg,
                               u32 intsrc)
{
        void __iomem *shared_regs = itf->shared_regs;
        unsigned long offset;
        unsigned int reg_range_id = MCUCTL_END_REG;

        switch(intsrc) {
        case INTR_GENERAL:
                itf_handle_general(itf, msg);
                reg_range_id = MCUCTL_IHC_REG;
                break;
        case INTR_ISP_DONE:
                reg_range_id = MCUCTL_ISP_REG;
                break;
        case INTR_3A0C_DONE:
                reg_range_id = MCUCTL_3AA0C_REG;
                break;
        case INTR_3A1C_DONE:
                reg_range_id = MCUCTL_3AA1C_REG;
                break;
        case INTR_SHOT_DONE:
                itf_handle_shot_done(itf, msg);
                reg_range_id = MCUCTL_SHOT_REG;
                break;
        case INTR_SCC_DONE:
                msg->param[3] = SCALER_SCC;
		itf_handle_scaler_done(itf, msg);
                reg_range_id = MCUCTL_SCC_REG;
                break;
        case INTR_SCP_DONE:
                msg->param[3] = SCALER_SCP;
                itf_handle_scaler_done(itf, msg);
                reg_range_id = MCUCTL_SCP_REG;
                break;
        case INTR_META_DONE:
                reg_range_id = MCUCTL_META_REG;
                break;
        }
        if (!(mcuctl_sreg_get_offset(itf->fw_data, reg_range_id, &offset))){
                write_shared_reg(shared_regs, offset, 0);
        }
}

/* Main FIMC-IS interrupt handler */
static irqreturn_t itf_irq_handler(int irq, void *data)
{
	struct fimc_is_interface *itf = data;
	struct fimc_is_msg msg;
	unsigned int status, intr;
        unsigned int intr_src;
	status = itf_get_intr(itf);

	for (intr = INTR_GENERAL; intr < INTR_MAX_MAP; intr++) {

		if (status & BIT(intr)) {
                        intr_src = FIMC_IS_FW_GET_INTR_SRC(itf->fw_data, intr);
                        itf_get_cmd(itf, &msg, intr_src);
                        fimc_is_handle_irq(itf, &msg, intr_src);
			status &= ~BIT(intr);
			writel(BIT(intr), itf->regs + INTCR1);
                 }

        }
	if (status != 0)
		dev_err(itf->dev, "status is NOT all clear(0x%08X)", status);

	return IRQ_HANDLED;
}

int fimc_is_itf_open_sensor(struct fimc_is_interface *itf,
		unsigned int instance,
		unsigned int sensor_id,
		unsigned int i2c_channel,
		unsigned int sensor_ext)
{
	struct fimc_is_msg msg = {
		.command = HIC_OPEN_SENSOR,
		.instance = instance,
                .param = {sensor_id, i2c_channel, sensor_ext },
        };

        return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_open_sensor_ext(struct fimc_is_interface *itf,
                                unsigned int instance,
                                unsigned int sensor_id,
                                unsigned int sensor_ext)
{
        struct fimc_is_msg msg = {
                .command = HIC_OPEN_SENSOR,
                .instance = instance,
                .param = { sensor_id, sensor_ext, FIMC_IS_GROUP_ID >> 16 },
	};

	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_sensor_close(struct fimc_is_interface *itf,
                             unsigned int instance)
{
        struct fimc_is_msg msg = {
                .command = HIC_CLOSE_SENSOR,
                .instance = instance,
                .param = { 0,},
        };
        return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_sensor_mode(struct fimc_is_interface *itf,
                            unsigned int instance,
                            unsigned int mode)
{
        struct fimc_is_msg msg = {
                .command  = HIC_SENSOR_MODE_CHANGE,
                .instance = instance,
                .param = { mode },
        };
        return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_get_setfile_addr(struct fimc_is_interface *itf,
		unsigned int instance, unsigned int *setfile_addr)
{
	int ret;
	struct fimc_is_msg msg = {
		.command = HIC_GET_SET_FILE_ADDR,
		.instance = instance,
	};

	ret = fimc_is_itf_set_cmd(itf, &msg);
	*setfile_addr = itf->reply.param[1];

	return ret;
}

int fimc_is_itf_load_setfile(struct fimc_is_interface *itf,
		unsigned int instance)
{
	struct fimc_is_msg msg = {
		.command = HIC_LOAD_SET_FILE,
		.instance = instance,
	};

	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_stream_on(struct fimc_is_interface *itf,
		unsigned int instance)
{
	struct fimc_is_msg msg = {
		.command = HIC_STREAM_ON,
		.instance = instance,
	};

	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_stream_off(struct fimc_is_interface *itf,
		unsigned int instance)
{
	struct fimc_is_msg msg = {
		.command = HIC_STREAM_OFF,
		.instance = instance,
	};

	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_process_on(struct fimc_is_interface *itf,
		unsigned int instance)
{
	struct fimc_is_msg msg = {
		.command = HIC_PROCESS_START,
                .instance = instance | FIMC_IS_GROUP_ID,
	};

	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_process_off(struct fimc_is_interface *itf,
		unsigned int instance)
{
	struct fimc_is_msg msg = {
		.command = HIC_PROCESS_STOP,
                .instance = instance |  FIMC_IS_GROUP_ID,
	};

	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_set_param(struct fimc_is_interface *itf,
		unsigned int instance,
                unsigned int scenario,
		unsigned int lindex,
		unsigned int hindex)
{
	struct fimc_is_msg msg = {
		.command = HIC_SET_PARAMETER,
                .instance = instance ,
                .param = { scenario, 0, lindex, hindex },
	};
        unsigned int param_count = hweight32(lindex);
        param_count += hweight32(hindex);
        msg.param[1] = param_count;
	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_preview_still(struct fimc_is_interface *itf,
		unsigned int instance)
{
	struct fimc_is_msg msg = {
		.command = HIC_PREVIEW_STILL,
                .instance = instance | FIMC_IS_GROUP_ID,
	};

	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_change_sensor_mode(struct fimc_is_interface *itf,
                                   unsigned int instance,
                                   unsigned int mode)
{
        struct fimc_is_msg msg = {
                .command = HIC_SENSOR_MODE_CHANGE,
                .instance = instance,
                .param = { mode, },
        };
        return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_get_capability(struct fimc_is_interface *itf,
	unsigned int instance, unsigned int address)
{
	struct fimc_is_msg msg = {
		.command = HIC_GET_STATIC_METADATA,
		.instance = instance,
		.param[0] = address,
	};

	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_map_mem(struct fimc_is_interface *itf,
		unsigned int instance, unsigned int address,
		unsigned int size)
{
	struct fimc_is_msg msg = {
                .command = HIC_SET_A5_MEM_ACCESS, /* HIC_SET_A5_MMAP */
                .instance = instance | FIMC_IS_GROUP_ID,
		.param = { address, size },
	};

	return fimc_is_itf_set_cmd(itf, &msg);
}

int fimc_is_itf_unmap_mem(struct fimc_is_interface *itf,
                          unsigned int instance)
{
        if (FIMC_IS_FW_CMD_SUPPORT(itf->fw_data, HIC_SET_A5_UNMAP)) {

                struct fimc_is_msg msg = {
                        .command  = HIC_SET_A5_UNMAP,
                        .instance = instance | FIMC_IS_GROUP_ID,
                };

                return fimc_is_itf_set_cmd(itf, &msg);
        }
        return 0;
}

int fimc_is_itf_shot_nblk(struct fimc_is_interface *itf,
		unsigned int instance, unsigned int bayer,
		unsigned int shot, unsigned int fcount, unsigned int rcount)
{
	struct fimc_is_msg msg = {
		.command = HIC_SHOT,
                .instance = instance  | FIMC_IS_GROUP_ID,
		.param = { bayer, shot, fcount, rcount },
	};
        dump_message(&msg);
	return fimc_is_itf_set_cmd_shot(itf, &msg);
}

int fimc_is_itf_power_down(struct fimc_is_interface *itf,
		unsigned int instance)
{
	int ret;
	struct fimc_is_msg msg = {
		.command = HIC_POWER_DOWN,
		.instance = instance,
	};

	ret = fimc_is_itf_set_cmd(itf, &msg);
	itf_clr_state(itf, IS_IF_STATE_INIT);

	return ret;
}

int fimc_is_itf_hw_running(struct fimc_is_interface *itf)
{
        if (FIMC_IS_FW_CMD_SUPPORT(itf->fw_data, HIC_GET_IP_STATUS)){
                struct fimc_is_msg msg = {
                        .command = HIC_GET_IP_STATUS,
                };
                return (-ETIME !=  fimc_is_itf_set_cmd(itf, &msg)) ? 1 : 0;
        }
        return 0;
}

int fimc_is_itf_i2c_lock(struct fimc_is_interface *itf,
                         unsigned int instance,
                         int i2c_clk, bool lock)
{
        if (FIMC_IS_FW_CMD_SUPPORT(itf->fw_data, HIC_I2C_CONTROL_LOCK)) {

                struct fimc_is_msg msg = {
                        .command  = HIC_I2C_CONTROL_LOCK,
                        .instance = instance,
                        .param    = {lock, i2c_clk},
                };

                return fimc_is_itf_set_cmd_shot(itf, &msg);

        }
        return 0;
}

int fimc_is_itf_sys_ctrl(struct fimc_is_interface *itf,
                         unsigned int instance,
                         int cmd, int val)
{
        if (FIMC_IS_FW_CMD_SUPPORT(itf->fw_data, HIC_SYSTEM_CONTROL)) {

                struct fimc_is_msg msg = {
                        .command  = HIC_SYSTEM_CONTROL,
                        .instance = instance,
                        .param    =  {cmd, val},
                };

                return fimc_is_itf_set_cmd_shot(itf, &msg);
        }
        return 0;
}

void fimc_is_itf_notify_frame_done(struct fimc_is_interface *itf)
{
        struct mcuctl_sreg_desc  shared_reg_desc;

        if (!(mcuctl_sreg_get_desc(itf->fw_data, MCUCTL_FRAME_COUNT_REG,
                                         &shared_reg_desc))) {
                unsigned int frame_count;
                unsigned long offset = shared_reg_desc.base_offset;
                /* Frame count for sensor:0*/
                offset += (--shared_reg_desc.data_range) * MCUCTL_SREG_SIZE;
                frame_count = read_shared_reg(itf->shared_regs, offset);
                write_shared_reg(itf->shared_regs, offset, ++frame_count);

        }
}

/* Debugfs for showing FW debug messages */
static int fimc_is_log_show(struct seq_file *s, void *data)
{
	struct fimc_is_interface *itf = s->private;
	struct fimc_is *is = fimc_interface_to_is(itf);
        struct fimc_is_fw_mem fw_minfo;
        u8 *buf;

        if (FIMC_IS_FW_CALL_OP(mem_config, &fw_minfo))
                return -ENODEV;

        buf = (u8 *)(is->minfo.fw.vaddr + fw_minfo.dbg_offset);

	if (is->minfo.fw.vaddr == 0) {
		dev_err(itf->dev, "Firmware memory is not initialized\n");
		return -EIO;
	}
	seq_printf(s, "%s\n", buf);
	return 0;
}

static int fimc_is_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, fimc_is_log_show, inode->i_private);
}

static const struct file_operations fimc_is_debugfs_fops = {
	.open		= fimc_is_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void fimc_is_debugfs_remove(struct fimc_is_interface *itf)
{
	debugfs_remove(itf->debugfs_entry);
	itf->debugfs_entry = NULL;
}

static int fimc_is_debugfs_create(struct fimc_is_interface *itf)
{
	struct dentry *dentry;

	itf->debugfs_entry = debugfs_create_dir("fimc_is", NULL);

	dentry = debugfs_create_file("fw_log", S_IRUGO, itf->debugfs_entry,
					itf, &fimc_is_debugfs_fops);
	if (!dentry)
		fimc_is_debugfs_remove(itf);

	return itf->debugfs_entry == NULL ? -EIO : 0;
}

int fimc_is_interface_init(struct fimc_is_interface *itf,
		void __iomem *regs, int irq)
{
	struct fimc_is *is = fimc_interface_to_is(itf);
	struct device *dev = &is->pdev->dev;
	int ret;

	if (!regs || !irq) {
		dev_err(itf->dev, "Invalid args\n");
		return -EINVAL;
	}

	itf->regs = regs;
        itf->shared_regs = (void __iomem*)(regs + ISSR(0));
	itf->dev = &is->pdev->dev;
        fimc_is_fw_set_default(&itf->fw_data);
	init_waitqueue_head(&itf->irq_queue);
	spin_lock_init(&itf->slock_state);
	spin_lock_init(&itf->slock);

        /* Init the work queue */
        itf_init_workqueue();
        INIT_WORK(&itf->shot_done_work, shot_done_work_fn);
	/* Register interrupt handler */
	ret = devm_request_irq(dev, irq, itf_irq_handler,
			       0, dev_name(dev), itf);
	if (ret) {
		dev_err(dev, "Failed to install irq (%d)\n", ret);
		return ret;
	}

	/* Initialize context vars */
	itf->streaming = IS_IF_STREAMING_INIT;
	itf->processing = IS_IF_PROCESSING_INIT;
	itf->pdown_ready = IS_IF_POWER_DOWN_READY;
	itf->debug_cnt = 0;
	init_request_barrier(itf);

	/* Debugfs for FW debug log */
	fimc_is_debugfs_create(itf);

	return 0;
}
