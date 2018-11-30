/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * Boojin Kim <boojin.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/iio/iio.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/random.h>
#include <linux/rtc.h>
#include <linux/clk.h>
#include <linux/timekeeping.h>

#include "chub.h"
#include "chub_ipc.h"
#include "chub_dbg.h"
#ifdef CONFIG_NANOHUB
#include "main.h"
#endif
#ifdef CONFIG_CONTEXTHUB_DRV
#include "chub_dev.h"
#include "bl_image.h"
#include "os_image.h"
#endif
#if defined(CONFIG_SOC_EXYNOS9110)
#include <linux/smc.h>
#endif

inline struct device *get_sensorhal_dev(struct host_data *data)
{
#if defined(CONFIG_NANOHUB)
	return data->io[ID_NANOHUB_SENSOR].dev;
#elif defined(CONFIG_CONTEXTHUB_DRV)
	return data->dev;
#endif
}

int contexthub_download_image(struct host_data *data, int bl)
{
	const struct firmware *entry;
	struct device *dev = get_sensorhal_dev(data);
	int ret;

	if (bl) {
		ret = request_firmware(&entry, "bl.unchecked.bin", dev);
		if (ret) {
#if defined(CONFIG_CONTEXTHUB_DRV)
			memcpy(ipc_get_base(IPC_REG_BL), bl_unchecked_bin, bl_unchecked_bin_len);
			return 0;
#else
			dev_err(dev, "%s, bl request_firmware failed\n",
				__func__);
			return ret;
#endif
		}

		memcpy(ipc_get_base(IPC_REG_BL), entry->data, entry->size);
		release_firmware(entry);

		dev_info(dev, "%s: bootloader(size:0x%x) on %lx\n",
			 __func__, (int)entry->size,
			 (unsigned long)ipc_get_base(IPC_REG_BL));
	} else {
		ret = request_firmware(&entry, CHUB_OS_NAME, dev);
		if (ret) {
#if defined(CONFIG_CONTEXTHUB_DRV)
			memcpy(ipc_get_base(IPC_REG_OS), os_chub_bin, os_chub_bin_len);
			return 0;
#else
			dev_err(dev, "%s, %s request_firmware failed\n",
				__func__, CHUB_OS_NAME);
			return ret;
#endif
		}

		memcpy(ipc_get_base(IPC_REG_OS), entry->data, entry->size);
		release_firmware(entry);

		dev_info(dev, "%s: %s(size:0x%x) on %lx\n", __func__,
			 CHUB_OS_NAME, (int)entry->size,
			 (unsigned long)ipc_get_base(IPC_REG_OS));
	}

	return 0;
}

int contexthub_poweron(struct host_data *data)
{
	int ret = 0;
	struct device *dev = get_sensorhal_dev(data);

	if (!atomic_read(&data->pdata->chub_status)) {
		ret = contexthub_download_image(data, 1);
		if (ret) {
			dev_warn(dev, "fails to download bootloader\n");
			return ret;
		}

		ret = contexthub_ipc_write_event(data, MAILBOX_EVT_INIT_IPC);
		if (ret) {
			dev_warn(dev, "fails to init ipc\n");
			return ret;
		}

		chub_dbg_init(dev);
		ret = contexthub_download_image(data, 0);
		if (ret) {
			dev_warn(dev, "fails to download kernel\n");
			return ret;
		}
		ret = contexthub_ipc_write_event(data, MAILBOX_EVT_POWER_ON);
		if (ret) {
			dev_warn(dev, "fails to poweron\n");
			return ret;
		}

		if (atomic_read(&data->pdata->chub_status) == CHUB_ST_RUN)
			dev_info(dev, "contexthub power-on");
		else
			dev_warn(dev, "contexthub fails to power-on");
	} else {
		ret = -EINVAL;
	}

	if (ret)
		dev_warn(dev, "fails to %s with %d. Status is %d\n",
			 __func__, ret, atomic_read(&data->pdata->chub_status));
	return ret;
}

static void __contexthub_hw_reset(struct host_data *data, int boot0)
{
	int ret;

	if (!data)
		pr_err("%s fails to get data\n", __func__);

	if (boot0)
		ret = contexthub_ipc_write_event(data, MAILBOX_EVT_SHUTDOWN);
	else
		ret = contexthub_ipc_write_event(data, MAILBOX_EVT_RESET);

	if (ret)
		pr_warn("%s: fail to reset on boot0 %d\n", __func__, boot0);
}

int contexthub_reset(struct host_data *data)
{
	/* TODO: add wait lock */
	__contexthub_hw_reset(data, 1);
	__contexthub_hw_reset(data, 0);

	return 0;
}

int contexthub_download_bl(struct host_data *data)
{
	int ret;

	__contexthub_hw_reset(data, 1);

	ret = contexthub_download_image(data, 1);

	if (!ret)
		ret = contexthub_download_image(data, 0);

	if (!ret)
		__contexthub_hw_reset(data, 0);

	return ret;
}

int contexthub_download_kernel(struct device *dev)
{
	const struct firmware *fw_entry;
	int ret;

	ret = request_firmware(&fw_entry, CHUB_OS_NAME, dev);
	if (ret) {
		dev_err(dev, "%s: err=%d\n", __func__, ret);
		return -EIO;
	}
	memcpy(ipc_get_base(IPC_REG_OS), fw_entry->data, fw_entry->size);
	release_firmware(fw_entry);
	return 0;
}

#ifdef PACKET_LOW_DEBUG
static void debug_dumpbuf(unsigned char *buf, int len)
{
	print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET, 16, 1, buf, len,
		       false);
}
#endif

static inline int get_recv_channel(struct recv_ctrl *recv)
{
	int i;
	unsigned long min_order = 0;
	int min_order_evt = INVAL_CHANNEL;

	for (i = 0; i < IPC_BUF_NUM; i++) {
		if (recv->container[i]) {
			if (!min_order) {
				min_order = recv->container[i];
				min_order_evt = i;
			} else if (recv->container[i] < min_order) {
				min_order = recv->container[i];
				min_order_evt = i;
			}
		}
	}

	if (min_order_evt != INVAL_CHANNEL)
		recv->container[min_order_evt] = 0;

	return min_order_evt;
}

static int contexthub_ipc_init(struct host_data *data)
{
	struct contexthub_ipc_info *chub = data->pdata->mailbox_client;
	struct device *chub_dev = &chub->pdev->dev;

	chub->ipc_map = ipc_get_chub_map();
	if (!chub->ipc_map) {
		dev_info(chub_dev, "%s: fails to get ipc map\n", __func__);
		return -EINVAL;
	}

	if (data->pdata->powermode_on == INIT_CHUB_VAL)
		data->pdata->powermode_on = ipc_get_chub_bootmode();

	/* init debug-log */
	chub->fw_log = log_register_buffer(chub_dev, 0,
					   (void *)&chub->ipc_map->logbuf.eq,
					   "fw", 1);
	if (!chub->fw_log) {
		dev_info(chub_dev, "%s: fails to init debug-log\n", __func__);
		return -EINVAL;
	}
#ifdef LOWLEVEL_DEBUG
	chub->dd_log_buffer = vmalloc(SZ_256K + sizeof(struct LOG_BUFFER *));
	chub->dd_log_buffer->index_reader = 0;
	chub->dd_log_buffer->index_writer = 0;
	chub->dd_log_buffer->size = SZ_256K;
	chub->dd_log =
	    log_register_buffer(chub_dev, 1, chub->dd_log_buffer, "dd", 0);
#endif

	dev_info(chub_dev,
		 "IPC map information\n\tinfo(base:%p size:%zu)\n\tipc(base:%p size:%zu)\n\tlogbuf(base:%p size:%d)\n",
		 chub, sizeof(struct contexthub_ipc_info),
		 ipc_get_base(IPC_REG_IPC), sizeof(struct ipc_map_area),
		 ipc_get_base(IPC_REG_LOG), chub->ipc_map->logbuf.size);

	return 0;
}

static void chub_dump_and_reset(struct contexthub_ipc_info *ipc,
				enum CHUB_ERR_TYPE err)
{
	int ret;

	contexthub_ipc_write_event(ipc->data, MAILBOX_EVT_DUMP_STATUS);
	chub_dbg_dump_hw(ipc, err);

	/* reset chub */
	dev_dbg(&ipc->pdev->dev, "Contexthub should be shutdown\n");

	ret = contexthub_ipc_write_event(ipc->data, MAILBOX_EVT_SHUTDOWN);
	if (!ret) {
#ifdef DEBUG_IMAGE
		chub_dbg_check_and_download_image(ipc);
#endif
		dev_dbg(&ipc->pdev->dev, "Contexthub sholud be reset\n");
		contexthub_ipc_write_event(ipc->data, MAILBOX_EVT_RESET);
		mdelay(100);
	} else {
		dev_warn(&ipc->pdev->dev, "Fail to shutdown contexthub\n");
	}
}

static inline bool read_is_locked(struct contexthub_ipc_info *ipc)
{
	return atomic_read(&ipc->read_lock.cnt) != 0;
}

static inline void read_get_locked(struct contexthub_ipc_info *ipc)
{
	atomic_inc(&ipc->read_lock.cnt);
}

static inline void read_put_unlocked(struct contexthub_ipc_info *ipc)
{
	atomic_dec(&ipc->read_lock.cnt);
}

#if defined(CONFIG_NANOHUB)
/* by nanohub kernel RxBufStruct. packet header is 10 + 2 bytes to align */
struct rxbuf {
	u8 pad;
	u8 pre_preamble;
	u8 buf[PACKET_SIZE_MAX];
	u8 post_preamble;
};

static int mailbox_read_process(uint8_t *rx, struct ipc_content *content)
{
	struct rxbuf *rxstruct;
	struct nanohub_packet *packet;

	rxstruct = (struct rxbuf *)content->buf;
	packet = (struct nanohub_packet *)&rxstruct->pre_preamble;
	memcpy_fromio(rx, (void *)packet, content->size);

	return NANOHUB_PACKET_SIZE(packet->len);
}
#elif defined(CONFIG_CONTEXTHUB_DRV)
/* pass ipc buffer to host rxbuf */
static int mailbox_read_process(uint8_t *rx, struct ipc_content *content)
{
	/* TODO: rx buffer pasing */
	memcpy_fromio(rx, content->buf, content->size);
	return content->size;
}
#endif

int contexthub_ipc_read(void *data, uint8_t *rx, int timeout)
{
	struct host_data *nanohub = data;
	struct contexthub_ipc_info *ipc = nanohub->pdata->mailbox_client;
	struct ipc_content *content;
	int ch = INVAL_CHANNEL;
	unsigned long flag;
	int ret;

	if (ipc->read_lock.flag) {
search_channel:
		ch = get_recv_channel(&ipc->recv_order);

		if (ch == INVAL_CHANNEL)
			goto fail_get_channel;
		else
			ipc->read_lock.flag &= ~(1 << ch);
	} else {
		spin_lock_irqsave(&ipc->read_lock.event.lock, flag);
		read_get_locked(ipc);
		ret =
		    wait_event_interruptible_timeout_locked(ipc->
							    read_lock.event,
							    ipc->read_lock.flag,
							    msecs_to_jiffies
							    (timeout));
		if (ret < 0)
			dev_warn(&ipc->pdev->dev,
				 "fails to get read ret:%d timeout:%d, flag:0x%x",
				 ret, timeout, ipc->read_lock.flag);
		read_put_unlocked(ipc);
		spin_unlock_irqrestore(&ipc->read_lock.event.lock, flag);

		if (ipc->read_lock.flag)
			goto search_channel;
		else
			goto fail_get_channel;
	}

	content = ipc_get_addr(IPC_REG_IPC_C2A, ch);
	ipc->recv_order.container[ch] = 0;
	ipc_update_channel_status(content, CS_CHUB_OWN);

	return mailbox_read_process(rx, content);

fail_get_channel:
	dev_err(&ipc->pdev->dev,
		"%s: fails to get channel flag:0x%x order:%lu\n",
		__func__, ipc->read_lock.flag, ipc->recv_order.order);
	contexthub_ipc_write_event(data, MAILBOX_EVT_CHUB_ALIVE);
	contexthub_ipc_write_event(data, MAILBOX_EVT_DUMP_STATUS);
	return -EINVAL;
}

int contexthub_ipc_write(void *data, uint8_t *tx, int length)
{
	struct ipc_content *content =
	    ipc_get_channel(IPC_REG_IPC_A2C, CS_IDLE, CS_AP_WRITE);

	if (!content) {
		pr_err("%s: fails to get channel.\n", __func__);
		ipc_print_channel();

		return -EINVAL;
	}
	content->size = length;
	memcpy_toio(content->buf, tx, length);

	DEBUG_PRINT(KERN_DEBUG, "->W%d\n", content->num);
	if (ipc_add_evt(IPC_EVT_A2C, content->num)) {
		contexthub_ipc_write_event(data, MAILBOX_EVT_CHUB_ALIVE);
		length = 0;
	}

	return length;
}

#ifdef CONFIG_NANOHUB
static int contexthub_nanohub_ipc_read(void *data, uint8_t *rx, int max_len,
				       int timeout)
{
	return contexthub_ipc_read(data, rx, timeout);
}

static int contexthub_nanohub_ipc_write(void *data, uint8_t *tx, int len,
					int timeout)
{
	return contexthub_ipc_write(data, tx, len);
}

static int contexthub_ipc_open(void *data)
{
	return 0;
}

static void contexthub_ipc_close(void *data)
{
	(void)data;
}

void nanohub_mailbox_comms_init(struct nanohub_comms *comms)
{
	comms->seq = 1;
	comms->timeout_write = 544;
	comms->timeout_ack = 272;
	comms->timeout_reply = 512;
	comms->open = contexthub_ipc_open;
	comms->close = contexthub_ipc_close;
	comms->write = contexthub_nanohub_ipc_write;
	comms->read = contexthub_nanohub_ipc_read;
}
#endif

static void check_rtc_time(void)
{
	struct rtc_device *chub_rtc = rtc_class_open(CONFIG_RTC_SYSTOHC_DEVICE);
	struct rtc_device *ap_rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	struct rtc_time chub_tm, ap_tm;
	time64_t chub_t, ap_t;

	rtc_read_time(ap_rtc, &chub_tm);
	rtc_read_time(chub_rtc, &ap_tm);

	chub_t = rtc_tm_sub(&chub_tm, &ap_tm);

	if (chub_t) {
		pr_info("nanohub %s: diff_time: %llu\n", __func__, chub_t);
		rtc_set_time(chub_rtc, &ap_tm);
	};

	chub_t = rtc_tm_to_time64(&chub_tm);
	ap_t = rtc_tm_to_time64(&ap_tm);
}

#define WAIT_TRY_CNT (3)
#define WAIT_TIMEOUT_MS (1000)

static int contexthub_wait_alive(struct host_data *nanohub)
{
	int trycnt = 0;
	struct contexthub_ipc_info *ipc = nanohub->pdata->mailbox_client;

	do {
		msleep(WAIT_TIMEOUT_MS);
		contexthub_ipc_write_event(nanohub, MAILBOX_EVT_CHUB_ALIVE);
		if (++trycnt > WAIT_TRY_CNT)
			break;
	} while ((atomic_read(&nanohub->pdata->chub_status) != CHUB_ST_RUN));

	if (atomic_read(&nanohub->pdata->chub_status) == CHUB_ST_RUN) {
		return 0;
	} else {
		dev_warn(&ipc->pdev->dev, "%s fails. contexthub status is %d\n",
			 __func__, atomic_read(&nanohub->pdata->chub_status));
		return -ETIMEDOUT;
	}
}

static int contexthub_ipc_reset(struct host_data *nanohub,
				enum mailbox_event event)
{
	struct contexthub_ipc_info *ipc = nanohub->pdata->mailbox_client;
	u32 val;

	/* clear ipc value */
	ipc_init();
	atomic_set(&nanohub->pdata->wakeup_chub, 1);
	atomic_set(&nanohub->pdata->irq1_apInt, 1);
	atomic_set(&ipc->read_lock.cnt, 0x0);
	ipc->read_lock.flag = 0;
	ipc->recv_order.order = 0;
	for (val = 0; val < IRQ_EVT_CH_MAX; val++)
		ipc->recv_order.container[val] = 0;

	ipc_hw_write_shared_reg(AP, ipc->os_load, SR_BOOT_MODE);
	ipc_set_chub_clk((u32) ipc->clkrate);

	switch (event) {
	case MAILBOX_EVT_POWER_ON:
#ifdef NEED_TO_RTC_SYNC
		check_rtc_time();
#endif
		if (atomic_read(&nanohub->pdata->chub_status) ==
		    CHUB_ST_NO_POWER) {
			atomic_set(&nanohub->pdata->chub_status,
				   CHUB_ST_POWER_ON);

			/* enable Dump GRP */
			IPC_HW_WRITE_DUMPGPR_CTRL(ipc->chub_dumpgrp, 0x1);

			/* pmu reset-release on CHUB */
			val =
			    __raw_readl(ipc->pmu_chub_reset +
					REG_CHUB_RESET_CHUB_OPTION);
			__raw_writel((val | CHUB_RESET_RELEASE_VALUE),
				     ipc->pmu_chub_reset +
				     REG_CHUB_RESET_CHUB_OPTION);

		} else {
			dev_warn(&ipc->pdev->dev,
				 "fails to contexthub power on. Status is %d\n",
				 atomic_read(&nanohub->pdata->chub_status));
		}
		break;
	case MAILBOX_EVT_SYSTEM_RESET:
#if defined(CONFIG_SOC_EXYNOS9810)
		val = __raw_readl(ipc->pmu_chub_cpu + REG_CHUB_CPU_OPTION);
		__raw_writel(val | ENABLE_SYSRESETREQ,
			     ipc->pmu_chub_cpu + REG_CHUB_CPU_OPTION);
#elif defined(CONFIG_SOC_EXYNOS9110)
/*
* NEED TO BE IMPLEMENTED
*/
#else
#error
#endif
		/* request systemresetreq to chub */
		ipc_hw_write_shared_reg(AP, ipc->os_load, SR_BOOT_MODE);
		ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_RESET);
		break;
	case MAILBOX_EVT_CORE_RESET:
		/* check chub cpu status */
		val = __raw_readl(ipc->pmu_chub_reset +
				  REG_CHUB_RESET_CHUB_CONFIGURATION);
		__raw_writel(val | (1 << 0),
			     ipc->pmu_chub_reset +
			     REG_CHUB_RESET_CHUB_CONFIGURATION);
		break;
	default:
		break;
	}

	return contexthub_wait_alive(nanohub);
}

int contexthub_ipc_write_event(struct host_data *data, enum mailbox_event event)
{
	struct contexthub_ipc_info *ipc = data->pdata->mailbox_client;
	u32 val;
	int ret = 0;

	switch (event) {
	case MAILBOX_EVT_INIT_IPC:
		ret = contexthub_ipc_init(data);
		break;
	case MAILBOX_EVT_DUMP_STATUS:
		chub_dbg_dump_status(ipc);
		break;
	case MAILBOX_EVT_POWER_ON:
		ipc_set_chub_bootmode(BOOTMODE_COLD);
		ret = contexthub_ipc_reset(data, event);
		break;
	case MAILBOX_EVT_CORE_RESET:
	case MAILBOX_EVT_SYSTEM_RESET:
		if (atomic_read(&data->pdata->chub_status) == CHUB_ST_SHUTDOWN) {
			ret = contexthub_ipc_reset(data, event);
			log_schedule_flush_all();
		} else {
			dev_err(&ipc->pdev->dev,
				"contexthub status isn't shutdown. fails to reset\n");
			ret = -EINVAL;
		}
		break;
	case MAILBOX_EVT_SHUTDOWN:
		/* assert */
		if (MAILBOX_EVT_RESET == MAILBOX_EVT_CORE_RESET) {
			ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_SHUTDOWN);
			msleep(100);	/* wait for shut down time */
#if defined(CONFIG_SOC_EXYNOS9810)
			val =
			    __raw_readl(ipc->pmu_chub_cpu +
					REG_CHUB_CPU_STATUS);
#elif defined(CONFIG_SOC_EXYNOS9110)
			val =
			    __raw_readl(ipc->pmu_chub_reset +
					REG_CHUB_CPU_STATUS);
#else
#error
#endif
			if (val & (1 << REG_CHUB_CPU_STATUS_BIT_STANDBYWFI)) {
				val = __raw_readl(ipc->pmu_chub_reset +
						  REG_CHUB_RESET_CHUB_CONFIGURATION);
				__raw_writel(val & ~(1 << 0),
					     ipc->pmu_chub_reset +
					     REG_CHUB_RESET_CHUB_CONFIGURATION);
			} else {
				dev_err(&ipc->pdev->dev,
					"fails to shutdown contexthub. cpu_status: 0x%x\n",
					val);
				return -EINVAL;
			}
		}
		atomic_set(&data->pdata->chub_status, CHUB_ST_SHUTDOWN);
		break;
	case MAILBOX_EVT_CHUB_ALIVE:
		ipc->chub_alive_lock.flag = 0;
		ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_CHUB_ALIVE);
		val = wait_event_timeout(ipc->chub_alive_lock.event,
					 ipc->chub_alive_lock.flag,
					 msecs_to_jiffies(WAIT_TIMEOUT_MS));

		if (ipc->chub_alive_lock.flag) {
			atomic_set(&data->pdata->chub_status, CHUB_ST_RUN);
			dev_info(&ipc->pdev->dev, "chub is alive");
		} else {
			dev_err(&ipc->pdev->dev,
				"chub isn't alive. should be reset\n");
			if (atomic_read(&data->pdata->chub_status) ==
			    CHUB_ST_RUN) {
				chub_dump_and_reset(ipc,
						    CHUB_ERR_CHUB_NO_RESPONSE);
				ipc->err_cnt[CHUB_ERR_CHUB_NO_RESPONSE]++;
				atomic_set(&data->pdata->chub_status,
					   CHUB_ST_NO_RESPONSE);
			}
			ret = -EINVAL;
		}
		break;
#ifdef CONFIG_NANOHUB_MAILBOX
	case MAILBOX_EVT_ERASE_SHARED:
		memset(ipc_get_base(IPC_REG_SHARED), 0,
		       ipc_get_offset(IPC_REG_SHARED));
		break;
	case MAILBOX_EVT_ENABLE_IRQ:
		/* if enable, mask from CHUB IRQ, else, unmask from CHUB IRQ */
		ipc_hw_unmask_irq(AP, IRQ_EVT_C2A_INT);
		ipc_hw_unmask_irq(AP, IRQ_EVT_C2A_INTCLR);
		break;
	case MAILBOX_EVT_DISABLE_IRQ:
		ipc_hw_mask_irq(AP, IRQ_EVT_C2A_INT);
		ipc_hw_mask_irq(AP, IRQ_EVT_C2A_INTCLR);
		break;
#endif
	default:
		break;
	}

	if ((int)event < IPC_DEBUG_UTC_MAX) {
		ipc->utc_run = event;
		if ((int)event == IPC_DEBUG_UTC_TIME_SYNC) {
			check_rtc_time();
#ifdef CONFIG_CONTEXTHUB_DEBUG
			/* log_flush enable when utc_run is set */
			schedule_work(&ipc->utc_work);
#else
			ipc_write_debug_event(AP, event);
			ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_DEBUG);
#endif
		}
		ipc_write_debug_event(AP, event);
		ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_DEBUG);
	}

	return ret;
}

#ifdef CONFIG_CONTEXTHUB_DEBUG
/* log_flush enable when utc_run is set */
static void handle_utc_work_func(struct work_struct *work)
{
	struct contexthub_ipc_info *ipc =
	    container_of(work, struct contexthub_ipc_info, utc_work);
	int trycnt = 0;

	while (ipc->utc_run) {
		msleep(20000);
		ipc_write_val(AP, sched_clock());
		ipc_write_debug_event(AP, ipc->utc_run);
		ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_DEBUG);
		if (!(++trycnt % 10))
			log_flush(ipc->fw_log);
	};
	dev_dbg(&ipc->pdev->dev, "%s is done with %d try\n", __func__, trycnt);
}
#endif

static void handle_debug_work_func(struct work_struct *work)
{
	struct contexthub_ipc_info *ipc =
	    container_of(work, struct contexthub_ipc_info, debug_work);
	enum ipc_debug_event event = ipc_read_debug_event(AP);

	dev_dbg(&ipc->pdev->dev,
		"%s is run with nanohub driver %d, fw %d error\n", __func__,
		ipc->chub_err, event);

	log_flush(ipc->fw_log);

	if (ipc->chub_err) {
		log_dump_all(ipc->chub_err);
		chub_dbg_dump_hw(ipc, ipc->chub_err);
		ipc->chub_err = 0;
		return;
	}

	switch (event) {
	case IPC_DEBUG_CHUB_FULL_LOG:
		dev_warn(&ipc->pdev->dev,
			 "Contexthub notified that logbuf is full\n");
		break;
	case IPC_DEBUG_CHUB_PRINT_LOG:
		break;
	case IPC_DEBUG_CHUB_FAULT:
		dev_warn(&ipc->pdev->dev, "Contexthub notified fault\n");
		ipc->err_cnt[CHUB_ERR_NANOHUB_FAULT]++;
		chub_dump_and_reset(ipc, CHUB_ERR_NANOHUB_FAULT);
		log_dump_all(CHUB_ERR_NANOHUB_FAULT);
		break;
	case IPC_DEBUG_CHUB_ASSERT:
		dev_warn(&ipc->pdev->dev, "Contexthub notified assert\n");
		ipc->err_cnt[CHUB_ERR_NANOHUB_ASSERT]++;
		chub_dbg_dump_hw(ipc, CHUB_ERR_NANOHUB_ASSERT);
		log_dump_all(CHUB_ERR_NANOHUB_ASSERT);
		break;
	case IPC_DEBUG_CHUB_ERROR:
		dev_warn(&ipc->pdev->dev, "Contexthub notified error\n");
		ipc->err_cnt[CHUB_ERR_NANOHUB_ERROR]++;
		contexthub_ipc_write_event(ipc->data, MAILBOX_EVT_DUMP_STATUS);
		log_dump_all(CHUB_ERR_NANOHUB_ERROR);
		break;
	default:
		break;
	}
}

static void handle_irq(struct contexthub_ipc_info *ipc, enum irq_evt_chub evt)
{
	struct ipc_content *content;

	switch (evt) {
	case IRQ_EVT_C2A_DEBUG:
		schedule_work(&ipc->debug_work);
		break;
	case IRQ_EVT_C2A_INT:
		if (atomic_read(&ipc->data->pdata->irq1_apInt)) {
			atomic_set(&ipc->data->pdata->irq1_apInt, 0);
#if defined(CONFIG_NANOHUB)
			nanohub_handle_irq1(ipc->data);
#elif defined(CONFIG_CONTEXTHUB_DRV)
			contexthub_handle_irq1(ipc->data);
#endif
		}
		break;
	case IRQ_EVT_C2A_INTCLR:
		atomic_set(&ipc->data->pdata->irq1_apInt, 1);
		break;
	default:
		if (evt < IRQ_EVT_CH_MAX) {
			int lock;

			content = ipc_get_addr(IPC_REG_IPC_C2A, evt);
			ipc_update_channel_status(content, CS_AP_RECV);

            /* reset order */
			if (!ipc->read_lock.flag)
				ipc->recv_order.order = 1;

			if (ipc->recv_order.container[evt])
				dev_warn(&ipc->pdev->dev,
					 "%s: invalid order container[%d] = %lu, status:%x\n",
					 __func__, evt,
					 ipc->recv_order.container[evt],
					 content->status);

			ipc->recv_order.container[evt] =
			    ++ipc->recv_order.order;
			ipc->read_lock.flag |= (1 << evt);

			DEBUG_PRINT(KERN_DEBUG, "<-R%d(%d)(%d)\n", evt,
				    content->size, ipc->recv_order.order);

			/* TODO: requered.. ? */
			spin_lock(&ipc->read_lock.event.lock);
			lock = read_is_locked(ipc);
			spin_unlock(&ipc->read_lock.event.lock);
			if (lock)
				wake_up_interruptible_sync(&ipc->
							   read_lock.event);
		} else {
			dev_warn(&ipc->pdev->dev, "%s: invalid %d event",
				 __func__, evt);
		}
		break;
	};
}

static irqreturn_t contexthub_irq_handler(int irq, void *data)
{
	struct contexthub_ipc_info *ipc = data;
	int start_index = ipc_hw_read_int_start_index(AP);
	unsigned int status = ipc_hw_read_int_status_reg(AP);
	struct ipc_evt_buf *cur_evt;
	enum CHUB_ERR_TYPE err = 0;
	enum irq_chub evt = 0;
	int irq_num = IRQ_EVT_CHUB_ALIVE + start_index;
	int intclr = 0;

	/* chub alive interrupt handle */
	irq_num = IRQ_EVT_CHUB_ALIVE + start_index;
	if (status & (1 << irq_num)) {
		status &= ~(1 << irq_num);
		ipc_hw_clear_int_pend_reg(AP, irq_num);
		/* set wakeup flag for chub_alive_lock */
		ipc->chub_alive_lock.flag = 1;
		wake_up(&ipc->chub_alive_lock.event);
	}

	/* chub intclr interrupt handle */
	irq_num = IRQ_EVT_C2A_INTCLR + start_index;
	if (status & (1 << irq_num)) {
		status &= ~(1 << irq_num);
		intclr = 1;
	}

	/* chub ipc event-queue interrupt handle */
	while (status) {
		cur_evt = ipc_get_evt(IPC_EVT_C2A);
		if (cur_evt) {
			evt = cur_evt->evt;
			irq_num = cur_evt->irq + start_index;

			/* check match evtq and hw interrupt pending */
			if (!(status & (1 << irq_num))) {
				err = CHUB_ERR_EVTQ_NO_HW_TRIGGER;
				break;
			}
		} else {
			err = CHUB_ERR_EVTQ_EMTPY;
			break;
		}

		handle_irq(ipc, evt);
		ipc_hw_clear_int_pend_reg(AP, irq_num);
		status &= ~(1 << irq_num);
	}

	if (intclr) {
		atomic_set(&ipc->data->pdata->irq1_apInt, 1);
		ipc_hw_clear_int_pend_reg(AP, IRQ_EVT_C2A_INTCLR + start_index);
	}

	if (err) {
		pr_err("inval irq(%d):evt:%d,irq:%d,status:0x%x(0x%x,0x%x)\n",
		       err, evt, irq_num, status,
		       ipc_hw_read_int_status_reg(AP),
		       ipc_hw_read_int_gen_reg(AP));
		ipc_print_evt(IPC_EVT_C2A);
		ipc->chub_err = err;
		ipc->err_cnt[err]++;
		schedule_work(&ipc->debug_work);
	}
	return IRQ_HANDLED;
}

static irqreturn_t contexthub_irq_wdt_handler(int irq, void *data)
{
	struct contexthub_ipc_info *ipc = data;

	dev_info(&ipc->pdev->dev, "context generated WDT timeout.\n");
	return IRQ_HANDLED;
}

static __init void contexhub_config_init(struct contexthub_ipc_info *chub)
{
	/* enable mailbox ipc */
	ipc_set_base(chub->sram);
	ipc_set_owner(AP, chub->mailbox, IPC_SRC);
}

#if defined(CONFIG_SOC_EXYNOS9110)
#define EXYNOS_CHUB (2)
#define EXYNOS_SET_CONN_TZPC (0)
extern int exynos_smc(unsigned long cmd, unsigned long arg1, unsigned long arg2,
		      unsigned long arg3);
#endif
static __init int contexthub_ipc_hw_init(struct platform_device *pdev,
					 struct contexthub_ipc_info *chub)
{
	int ret;
	int irq;
	int idx = 0;
	struct resource *res;
	const char *os;
	struct device *dev = &pdev->dev;
	struct device_node *node;
	unsigned int baaw_val[BAAW_VAL_MAX];
	int trycnt = 0;
	u32 val;

	node = dev->of_node;
	if (!node) {
		dev_err(dev, "driver doesn't support non-dt\n");
		return -ENODEV;
	}

	/* get os type from dt */
	os = of_get_property(node, "os-type", NULL);
	if (!strcmp(os, "none")) {
		dev_err(dev, "no use contexthub\n");
		return -ENODEV;
	} else if (!strcmp(os, "pass")) {
		chub->os_load = 0;
	} else {
		chub->os_load = 1;
		strcpy(chub->os_name, os);
	}
	pr_info("%s: %s\n", __func__, os);

	/* get mailbox interrupt */
	irq = irq_of_parse_and_map(node, 0);
	if (irq < 0) {
		dev_err(dev, "failed to get irq:%d\n", irq);
		return -EINVAL;
	}

	/* request irq handler */
	ret = devm_request_irq(dev, irq, contexthub_irq_handler,
			       0, dev_name(dev), chub);
	if (ret) {
		dev_err(dev, "failed to request irq:%d, ret:%d\n", irq, ret);
		return ret;
	}

	/* get wdt interrupt optionally */
	irq = irq_of_parse_and_map(node, 1);
	if (irq > 0) {
		/* request irq handler */
		ret = devm_request_irq(dev, irq,
				       contexthub_irq_wdt_handler, 0,
				       dev_name(dev), chub);
		if (ret) {
			dev_err(dev, "failed to request wdt irq:%d, ret:%d\n",
				irq, ret);
			return ret;
		}
	} else {
		dev_info(dev, "don't use wdt irq:%d\n", irq);
	}

	/* get mailbox SFR */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->mailbox = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->mailbox)) {
		dev_err(dev, "fails to get mailbox sfr\n");
		return PTR_ERR(chub->mailbox);
	}

	/* get SRAM base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->sram = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->sram)) {
		dev_err(dev, "fails to get sram\n");
		return PTR_ERR(chub->sram);
	}

	/* get chub gpr base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->chub_dumpgrp = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->chub_dumpgrp)) {
		dev_err(dev, "fails to get dumpgrp\n");
		return PTR_ERR(chub->chub_dumpgrp);
	}

	/* get pmu reset base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->pmu_chub_reset = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->pmu_chub_reset)) {
		dev_err(dev, "fails to get dumpgrp\n");
		return PTR_ERR(chub->pmu_chub_reset);
	}
#if defined(CONFIG_SOC_EXYNOS9810)
	/* get pmu reset enable base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->pmu_chub_cpu = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->pmu_chub_cpu)) {
		dev_err(dev, "fails to get pmu_chub_cpu\n");
		return PTR_ERR(chub->pmu_chub_cpu);
	}
#elif defined(CONFIG_SOC_EXYNOS9110)
	/* get pmu osc rco */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->pmu_osc_rco = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->pmu_osc_rco)) {
		dev_err(dev, "fails to get pmu_osc_rco\n");
		return PTR_ERR(chub->pmu_osc_rco);
	}

	/* get pmu rtc control */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->pmu_rtc_ctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->pmu_rtc_ctrl)) {
		dev_err(dev, "fails to get pmu_rtc_ctrl\n");
		return PTR_ERR(chub->pmu_rtc_ctrl);
	}

	/* get pmu chub control base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->pmu_chub_ctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->pmu_chub_ctrl)) {
		dev_err(dev, "fails to get pmu_chub_ctrl\n");
		return PTR_ERR(chub->pmu_chub_ctrl);
	}

	/* get pmu chub reset release status */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->pmu_chub_reset_stat = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->pmu_chub_reset_stat)) {
		dev_err(dev, "fails to get pmu_chub_reset_stat\n");
		return PTR_ERR(chub->pmu_chub_reset_stat);
	}
#endif

	/* get chub baaw base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, idx++);
	chub->chub_baaw = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->chub_baaw)) {
		pr_err("driver failed to get chub_baaw\n");
		chub->chub_baaw = 0;	/* it can be set on other-side (vts) */
	}

	/* pmu MUX Unset */
	val = __raw_readl(chub->pmu_osc_rco + REG_CTRL_REFCLK_PMU);
	__raw_writel((val & ~(0x1 << 4)),
		     chub->pmu_osc_rco + REG_CTRL_REFCLK_PMU);

	val = __raw_readl(chub->pmu_osc_rco + REG_CTRL_REFCLK_CHUB_VTS);
	__raw_writel((val & ~(0x1 << 4)),
		     chub->pmu_osc_rco + REG_CTRL_REFCLK_CHUB_VTS);

	/* CHUB Block Reset Release */
	val = __raw_readl(chub->pmu_chub_ctrl);
	__raw_writel((val | (0x1 << 9)), chub->pmu_chub_ctrl);

	/* Check Reset Sequence Status */
	do {
		msleep(WAIT_TIMEOUT_MS / 1000);
		val = __raw_readl(chub->pmu_chub_reset_stat);
		val = (val >> 12) & 0x7;
		if (++trycnt > WAIT_TRY_CNT)
			break;
	} while (val != 0x5);

	/* pmu MUX Set */
	val = __raw_readl(chub->pmu_osc_rco + REG_CTRL_REFCLK_PMU);
	__raw_writel((val | (0x1 << 4)),
		     chub->pmu_osc_rco + REG_CTRL_REFCLK_PMU);

	val = __raw_readl(chub->pmu_osc_rco + REG_CTRL_REFCLK_CHUB_VTS);
	__raw_writel((val | (0x1 << 4)),
		     chub->pmu_osc_rco + REG_CTRL_REFCLK_CHUB_VTS);

	ret = exynos_smc(SMC_CMD_CONN_IF,
			 (uint64_t)EXYNOS_CHUB << 32 |
			 EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret) {
		dev_err(dev, "%s: exynos_smc failed\n", __func__);
		return ret;
	}

	if (chub->chub_baaw) {
		for (idx = 0; idx < BAAW_VAL_MAX; idx++) {
			ret =
			    of_property_read_u32_index(node,
						       "baaw,baaw-p-apm-chub",
						       idx, &baaw_val[idx]);
			if (ret) {
				dev_err(dev,
					"fails to get baaw-p-apm-chub %d\n",
					idx);
				return -ENODEV;
			}
		}
	}

	/* BAAW-P-APM-CHUB for CHUB to access APM_CMGP */
	if (chub->chub_baaw) {
		/* baaw start */
		IPC_HW_WRITE_BAAW_CHUB0(chub->chub_baaw, baaw_val[0]);
		/* baaw end */
		IPC_HW_WRITE_BAAW_CHUB1(chub->chub_baaw, baaw_val[1]);
		/* baaw remap */
		IPC_HW_WRITE_BAAW_CHUB2(chub->chub_baaw, baaw_val[2]);
		/* baaw rw access enable */
		IPC_HW_WRITE_BAAW_CHUB3(chub->chub_baaw, baaw_val[3]);
	}

#if defined(CONFIG_SOC_EXYNOS9110)
	/* pmu rtc_control Set */
	val = __raw_readl(chub->pmu_rtc_ctrl);
	__raw_writel((val | (0x1 << 0)), chub->pmu_rtc_ctrl);

    /* Set CMU_CHUB CHUB_BUS as 49.152Mhz CLK_RCO_VTS in FW */
    chub->clkrate = 24576000 * 2;
	dev_info(dev, "%s clk selection of CMU_CHUB is %lu.\n", __func__, chub->clkrate);
#endif

	dev_info(dev, "%s with %lu clk is done.\n", __func__, chub->clkrate);
	return 0;
}

static int contexthub_ipc_probe(struct platform_device *pdev)
{
	struct contexthub_ipc_info *chub = NULL;
	int ret;
	int need_to_free = 0;
#ifdef CONFIG_NANOHUB
	struct iio_dev *iio_dev;
#endif

	chub = chub_dbg_get_memory(DBG_NANOHUB_DD_AREA);
	if (!chub) {
		chub =
		    devm_kzalloc(&pdev->dev, sizeof(struct contexthub_ipc_info),
				 GFP_KERNEL);
		need_to_free = 1;
	}
	if (IS_ERR(chub)) {
		dev_err(&pdev->dev, "%s failed to get ipc memory\n", __func__);
		return PTR_ERR(chub);
	}

	/* parse dt and hw init */
	ret = contexthub_ipc_hw_init(pdev, chub);
	if (ret) {
		dev_err(&pdev->dev, "%s failed to get init hw with ret %d\n",
			__func__, ret);
		goto err;
	}
#if defined(CONFIG_CONTEXTHUB_DRV)
	chub->data = contexthub_probe(&pdev->dev, chub);
	if (!chub->data) {
		dev_err(&pdev->dev, "%s failed to init host driver\n",
			__func__);
		ret = -ENODEV;
		goto err;
	}
#elif defined(CONFIG_NANOHUB)
	/* nanohub probe */
	iio_dev = nanohub_probe(&pdev->dev, NULL);
	if (IS_ERR(iio_dev)) {
		dev_err(&pdev->dev, "%s failed to init host driver\n",
			__func__);
		ret = PTR_ERR(iio_dev);
		goto err;
	}

	chub->data = iio_priv(iio_dev);
	nanohub_mailbox_comms_init(&chub->data->comms);

	/* set wakeup irq number on nanohub driver */
	chub->data->irq1 = IRQ_EVT_A2C_WAKEUP;
	chub->data->irq2 = 0;
#endif
	if (chub->data) {
		chub->data->pdata->mailbox_client = chub;
		chub->data->pdata->powermode_on = INIT_CHUB_VAL;
	}
	chub->pdev = pdev;
	chub->chub_err = 0;
	atomic_set(&chub->data->pdata->chub_status, CHUB_ST_NO_POWER);
	init_waitqueue_head(&chub->read_lock.event);
	init_waitqueue_head(&chub->chub_alive_lock.event);
	INIT_WORK(&chub->debug_work, handle_debug_work_func);
#ifdef CONFIG_CONTEXTHUB_DEBUG
    /* log_flush enable when utc_run is set */
	INIT_WORK(&chub->utc_work, handle_utc_work_func);
#endif
	contexhub_config_init(chub);

	dev_info(&pdev->dev, "%s is done\n", __func__);
	return 0;
err:
	if (chub) {
#ifdef CONFIG_CONTEXTHUB_DRV
		if (chub->data)
			contexthub_remove(&pdev->dev);
#endif
		if (need_to_free)
			devm_kfree(&pdev->dev, chub);
	}
	dev_err(&pdev->dev, "%s is fail with ret %d\n", __func__, ret);
	return ret;
}

static int contexthub_ipc_remove(struct platform_device *pdev)
{
#ifdef CONFIG_CONTEXTHUB_DRV
	contexthub_remove(&pdev->dev);
#endif

	return 0;
}

static const struct of_device_id contexthub_ipc_match[] = {
	{.compatible = "samsung,exynos-contexthub"},
	{},
};

static struct platform_driver samsung_contexthub_ipc_driver = {
	.probe = contexthub_ipc_probe,
	.remove = contexthub_ipc_remove,
	.driver = {
		   .name = "contexthub-ipc",
		   .owner = THIS_MODULE,
		   .of_match_table = contexthub_ipc_match,
		   },
};

int contexthub_mailbox_init(void)
{
	return platform_driver_register(&samsung_contexthub_ipc_driver);
}

void __exit contexthub_mailbox_cleanup(void)
{
	platform_driver_unregister(&samsung_contexthub_ipc_driver);
}

module_init(contexthub_mailbox_init);
module_exit(contexthub_mailbox_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Exynos contexthub mailbox Driver");
MODULE_AUTHOR("Boojin Kim <boojin.kim@samsung.com>");
