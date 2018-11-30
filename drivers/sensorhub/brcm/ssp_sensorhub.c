/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "ssp_sensorhub.h"
#ifdef CONFIG_DISPLAY_EARLY_DPMS
#include <drm/tgm_drm.h>
#endif
#ifdef CONFIG_SENSORHUB_STAT
#include <linux/sensorhub_stat.h>
#endif
#ifdef CONFIG_KNOX_GEARPAY
#include <linux/knox_gearlock.h>
int strap_status = -1;
#endif
#ifdef CONFIG_COPR_SUPPORT
extern int convert_to_candela(int brightnes);
#endif

static void ssp_sensorhub_log(const char *func_name,
				const char *data, int length)
{
	char buf[6];
	char *log_str;
	int log_size;
	int i;

	if (likely(length <= BIG_DATA_SIZE))
		log_size = length;
	else
		log_size = PRINT_TRUNCATE * 2 + 1;

	log_size = sizeof(buf) * log_size + 1;
	log_str = kzalloc(log_size, GFP_ATOMIC);
	if (unlikely(!log_str)) {
		sensorhub_err("allocate memory for data log err");
		return;
	}

	for (i = 0; i < length; i++) {
		if (length < BIG_DATA_SIZE ||
			i < PRINT_TRUNCATE || i >= length - PRINT_TRUNCATE) {
			snprintf(buf, sizeof(buf), "%d", (signed char)data[i]);
			strlcat(log_str, buf, log_size);

			if (i < length - 1)
				strlcat(log_str, ", ", log_size);
		}
		if (length > BIG_DATA_SIZE && i == PRINT_TRUNCATE)
			strlcat(log_str, "..., ", log_size);
	}

	pr_info("[SSP]: %s - %s (%d)\n", func_name, log_str, length);
	kfree(log_str);
}

static int ssp_sensorhub_send_big_data(struct ssp_sensorhub_data *hub_data,
					const char *buf, int count, u8 ext_cmd)
{
	int length = count - 3;
	int ret = 0;

	/* only support voice service for the moment */
  if (!ext_cmd && (buf[1] != TYPE_WAKE_UP_VOICE_SERVICE)) {
    sensorhub_err("not voice service(%d)", buf[1]);
    return -EINVAL;
  }

  /* am or grammer data? */
  if (!ext_cmd && (buf[2] != TYPE_WAKE_UP_VOICE_SOUND_SOURCE_AM) &&
    (buf[2] != TYPE_WAKE_UP_VOICE_SOUND_SOURCE_GRAMMER)) {
    sensorhub_err("voice data type err(%d)", buf[2]);
    return -EINVAL;
  }

  mutex_lock(&hub_data->big_events_lock);

  if (ext_cmd) {
    hub_data->big_send_events.library_data = (char *)buf;
    hub_data->big_send_events.library_length = count;
  } else {
    hub_data->big_send_events.library_data
      = kzalloc(length * sizeof(char), GFP_KERNEL);
    if (unlikely(!hub_data->big_send_events.library_data)) {
      mutex_unlock(&hub_data->big_events_lock);
      sensorhub_err("allocate memory for big library err");
      return -ENOMEM;
    }

    memcpy(hub_data->big_send_events.library_data, buf+3, length);
    hub_data->big_send_events.library_length = length;
  }

  /* trigger big data transfer */
  if (ext_cmd)
    ret = set_big_data_start(hub_data->ssp_data, ext_cmd, count);
  else
    ret = set_big_data_start(hub_data->ssp_data, buf[2]+1, length);

  if (ret != SUCCESS) {
    sensorhub_err("set_big_data_start err(%d)", ret);
    goto exit;
  }
  mutex_unlock(&hub_data->big_events_lock);

  /* wait until write operation is done */
  ret = wait_for_completion_timeout(&hub_data->big_write_done,
    (2 * COMPLETION_TIMEOUT));

  mutex_lock(&hub_data->big_events_lock);

  if (unlikely(!ret)) {
    sensorhub_err("wait timed out");
    ret = -EBUSY;
  } else if (unlikely(ret < 0)) {
    sensorhub_err("wait for completion err(%d)", ret);
    ret = -EIO;
  }

exit:
  if (!ext_cmd)
    kfree(hub_data->big_send_events.library_data);
  hub_data->big_send_events.library_data = NULL;
  hub_data->big_send_events.library_length = 0;
  mutex_unlock(&hub_data->big_events_lock);

  return !ret ? ret + 1 : ret;

}

static int ssp_sensorhub_send_cmd(struct ssp_sensorhub_data *hub_data,
					const char *buf, int count)
{
	int ret = 0;

	if (buf[2] < MSG2SSP_AP_STATUS_WAKEUP ||
		buf[2] > MSG2SSP_AP_STATUS_POW_DISCONNECTED) {
		sensorhub_err("MSG2SSP_INST_LIB_NOTI err(%d)", buf[2]);
		return -EINVAL;
	}

	ret = ssp_send_cmd(hub_data->ssp_data, buf[2], 0);

	if (buf[2] == MSG2SSP_AP_STATUS_WAKEUP ||
		buf[2] == MSG2SSP_AP_STATUS_SLEEP)
		hub_data->ssp_data->uLastAPState = buf[2];

	if (buf[2] == MSG2SSP_AP_STATUS_SUSPEND ||
		buf[2] == MSG2SSP_AP_STATUS_RESUME)
		hub_data->ssp_data->uLastResumeState = buf[2];

	return ret;
}

static int ssp_sensorhub_send_instruction(struct ssp_sensorhub_data *hub_data,
					const char *buf, int count)
{
	unsigned char instruction = buf[0];

	if (buf[0] == MSG2SSP_INST_LIBRARY_REMOVE) {
#ifdef CONFIG_SLEEP_MONITOR
		hub_data->ssp_data->service_mask = hub_data->ssp_data->service_mask & (~(1ULL<<buf[1]));
#endif
		instruction = REMOVE_LIBRARY;
	} else if (buf[0] == MSG2SSP_INST_LIBRARY_ADD) {
#ifdef CONFIG_SLEEP_MONITOR
		hub_data->ssp_data->service_mask = hub_data->ssp_data->service_mask | 1ULL<<buf[1];
#endif
		instruction = ADD_LIBRARY;
	} else if (instruction > MSG2SSP_MAX_INST_VALUE) {
		sensorhub_err("Invalid INST CMD value : 0x%x !!", instruction);
		return -EINVAL;
	}

	return send_instruction(hub_data->ssp_data, instruction,
		(unsigned char)buf[1], (unsigned char *)(buf+2), count-2);
}

static ssize_t ssp_sensorhub_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	struct ssp_sensorhub_data *hub_data
		= container_of(file->private_data,
			struct ssp_sensorhub_data, sensorhub_device);
	int ret = 0;
	char *buffer;

	//if (!hub_data) {
	//	sensorhub_err("hub data err(null)");
	//	return -EINVAL;
	//}

	if (!hub_data->ssp_data) {
		sensorhub_err("ssp data err(null)");
		return -EINVAL;
	}

	if (unlikely(hub_data->ssp_data->bSspShutdown)) {
		sensorhub_err("stop sending library data(shutdown)");
		return -EBUSY;
	}

	if (hub_data->ssp_data->uCmdCtl &&
		((hub_data->ssp_data->iHubState
				!= EVT2FW_HUB_STATE_READY_DONE)	||
			test_bit(EVT2FW_HUB_STATE_READY_DONE,
				&hub_data->ssp_data->ulHubStateBit))) {
		sensorhub_err("We didn't get User ACK or not yet ready!!\n");
		return -EPERM;
	}

	if (unlikely(count < 2)) {
		sensorhub_err("library data length err(%ld)", count);
		return -EINVAL;
	}

	buffer = kzalloc(count * sizeof(char), GFP_KERNEL);
	if (unlikely(!buffer)) {
		sensorhub_err("allocate memory for kernel buffer err\n");
		return -ENOMEM;
	}

	ret = copy_from_user(buffer, buf, count);
	if (unlikely(ret)) {
		sensorhub_err("memcpy for kernel buffer err\n");
		ret = -EFAULT;
		goto sensorhub_write_exit;
	}

#ifdef CONFIG_COPR_SUPPORT
	if (buffer[0] == 0xc1 && buffer[1] == 0x17 && buffer[2] == 0x32)
		convert_candela((unsigned char *)(buffer+3));
#endif

	ssp_sensorhub_log(__func__, buffer, count);

	if (buffer[0] == MSG2SSP_INST_LIB_DATA && count >= BIG_DATA_SIZE)
		ret = ssp_sensorhub_send_big_data(hub_data, buffer, count, 0);
	else if (buffer[0] == MSG2SSP_INST_LIB_NOTI)
		ret = ssp_sensorhub_send_cmd(hub_data, buffer, count);
	else
		ret = ssp_sensorhub_send_instruction(hub_data, buffer, count);

	if (unlikely(ret <= 0)) {
		sensorhub_err("send library data err(%d)", ret);
		/* i2c transfer fail */
		if (ret == ERROR)
			ret = -EIO;
		/* i2c transfer done but no ack from MCU */
		else if (ret == FAIL)
			ret = -EAGAIN;

		goto sensorhub_write_exit;
	}

	ret = count;

sensorhub_write_exit:
	kfree(buffer);
	return ret;
}

static ssize_t ssp_sensorhub_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	struct ssp_sensorhub_data *hub_data
		= container_of(file->private_data,
			struct ssp_sensorhub_data, sensorhub_device);
	struct sensorhub_event *event;
	int retries = MAX_DATA_COPY_TRY;
	int length = 0;
	int ret = 0;

	//if (!hub_data)
	//	return -EINVAL;

	spin_lock_bh(&hub_data->sensorhub_lock);
	if (unlikely(kfifo_is_empty(&hub_data->fifo))) {
		sensorhub_info("no library data");
		goto err;
	}

	/* first in first out */
	ret = kfifo_out_peek(&hub_data->fifo, &event, sizeof(void *));
	if (unlikely(!ret)) {
		sensorhub_err("kfifo out peek err(%d)", ret);
		ret = EIO;
		goto err;
	}

	length = event->library_length;

	while (retries--) {
		ret = copy_to_user(buf,
			event->library_data, event->library_length);
		if (likely(!ret))
			break;
	}
	if (unlikely(ret)) {
		sensorhub_err("read library data err(%d)", ret);
		goto err;
	}

/*
	ssp_sensorhub_log(__func__,
		event->library_data, event->library_length);
*/

	/* remove first event from the list */
	ret = kfifo_out(&hub_data->fifo, &event, sizeof(void *));
	if (unlikely(ret != sizeof(void *))) {
		sensorhub_err("kfifo out err(%d)", ret);
		ret = EIO;
		goto err;
	}

	complete(&hub_data->read_done);
	spin_unlock_bh(&hub_data->sensorhub_lock);

	return length;

err:
	spin_unlock_bh(&hub_data->sensorhub_lock);
	return ret ? -ret : 0;
}

int handle_wom_image_cmd(struct sensorhub_wom *wom,
				struct ssp_sensorhub_data *hub_data)
{
	int ret = 0;
//	int remain_sz;
	struct ssp_data *data = hub_data->ssp_data;
//	struct wom_image *wimage = &hub_data->wimage;
//	struct tdm_get_buffer_info get_tdm_info = {0, };
//	struct wom_mcu_cmd wom_cmd;

	switch (wom->cmd_type) {
	case CHANGE_NORMAL_MODE:
		sensorhub_info("Forcely reset trigger for NORMAL\n");
		data->bWOMode = false;
		data->bSspReady = false;
		ssp_enable(data, false);
		clean_pending_list(data);
		bbd_mcu_reset();
		break;

	case CHANGE_WATCHONLY_MODE:
		sensorhub_info("Forcely reset trigger for WOM\n");
		data->bWOMode = true;
		data->bSspReady = false;
		ssp_enable(data, false);
		clean_pending_list(data);
		bbd_mcu_reset();
		break;
#if 0
	case INT_WOM_IMAGE_USE:
		ret = tdm_client_send_event(TDM_CLIENT_CMD_GET_BUF_INFO,
				(void *)&get_tdm_info);
		if (ret) {
			sensorhub_err("Failed to get tdm info(%d)!!\n", ret);
			ret = -EIO;
			break;
		}

		sensorhub_info("[tdm] mode:%d, buf_cont:%u\n",
			get_tdm_info.mode, get_tdm_info.bufs_count);

		wom_cmd.ext_val[0] = get_tdm_info.mode;

		for (i = 0; i < get_tdm_info.bufs_count; i++) {
			sensorhub_info("[tdm][%dth] prop:%x, sz:%u, addr:%p\n",
				i, get_tdm_info.bufs_prop[i],
					get_tdm_info.bufs_size[i],
						get_tdm_info.bufs_addr[i]);

			wom_cmd.ext_val_sz = 1;

			wom_cmd.ext_val[1] = get_tdm_info.bufs_prop[i];
			wom_cmd.ext_val_sz++;

			memcpy(&wom_cmd.ext_val[2],
				&get_tdm_info.bufs_size[i], 4);
			wom_cmd.ext_val_sz += 4;

			ret = send_instruction_sync(data, EXT_CMD,
				MSG2SSP_INST_WOM_IMG_INFO, wom_cmd.ext_val,
					wom_cmd.ext_val_sz);

			if (ret) {
				sensorhub_err("failed to send WOM_IMG_INFO(%d/%d)\n",
					i, ret);
				break;
			}

			ret = ssp_sensorhub_send_big_data(hub_data,
				get_tdm_info.bufs_addr[i],
				get_tdm_info.bufs_size[i],
				TYPE_WOM_IMAGE_TYPE_BACKGROUND + i);
			if (ret < 0) {
				sensorhub_err("failed send_big_data type(%d/%d)\n",
					i, ret);
				break;
			}
		}
		break;

	case EXT_WOM_IMAGE_INFO:
		if (wom->size <= 0) {
			sensorhub_err("wrong size infom(%d)\n", wom->size);
			ret = -EINVAL;
			break;
		}

		wimage->buf = vzalloc(wom->size * sizeof(char));
		if (!wimage->buf) {
			ret = -ENOMEM;
			break;
		}

		wimage->offset_idx = 0;
		wimage->total_size = wom->size;
		break;

	case EXT_WOM_IMAGE_DATA:
		remain_sz = wimage->total_size - wimage->offset_idx;
		if (remain_sz < wom->size) {
			sensorhub_err("Size mis-match avail_sz:%d, curr:%d\n",
				remain_sz, wom->size);
			ret = -EINVAL;
			break;
		}

		ret = copy_from_user(wimage->buf,
			(void __user *)wom->addr, wom->size);
		if (unlikely(ret)) {
			sensorhub_err("failed memcpy from user\n");
			ret = -EFAULT;
		} else {
			wimage->offset_idx += wom->size;
		}
		break;

	case EXT_WOM_IMAGE_SEND:
		if (!wimage->total_size || (wimage->total_size !=
				wimage->offset_idx)) {
			sensorhub_err("Not yet complete tsz:%d, idx:%d\n",
				wimage->total_size, wimage->offset_idx);
			ret = -EAGAIN;
			break;
		}

		/* We assue that TYPE_WOM_IMAGE_TYPE_BACKGROUND command
		 * will be received by 'wom->size' value from TizenFW
		 */
		ret = ssp_sensorhub_send_big_data(hub_data, wimage->buf,
				wimage->total_size, (u8)wom->size);
		if (ret < 0)
			sensorhub_err("failed send_big_data(%d/0x%x)\n",
				ret, wom->cmd_type);

		vfree(wimage->buf);
		wimage->offset_idx = 0;
		wimage->total_size = 0;
		break;

	case INST_WOM_START:
		if (wom->size != 0)
			wom_cmd.ext_val[0] = 0;
		else
			wom_cmd.ext_val[0] = 1;

		wom_cmd.ext_val_sz = 1;

		memcpy(&wom_cmd.ext_val[1], "WOMSTART", 8);
		wom_cmd.ext_val_sz += 8;

		ret = send_instruction_sync(data, EXT_CMD,
			MSG2SSP_INST_WOM_START,	wom_cmd.ext_val,
				wom_cmd.ext_val_sz);

		/* ret value is Sensrhub's real return value.
		 * maybe "ret == 1" is the success-return-value.
		 */
		if (ret == ERROR) {
			sensorhub_info("failed send_inst_sync for starting!!\n");
			ret = -EIO;
		} else {
			sensorhub_info("WOM start cmd done (0x%x)!!\n", ret);
			ret = 0;
		}
		break;
#endif

	default:
		sensorhub_err("Unsupport cmd(0x%x)\n", wom->cmd_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}


static long ssp_sensorhub_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct ssp_sensorhub_data *hub_data
		= container_of(file->private_data,
			struct ssp_sensorhub_data, sensorhub_device);
	void __user *argp;
  struct sensorhub_wom wom;
	int retries = MAX_DATA_COPY_TRY;
	int length = 0;
	int ret = 0;

	//if (!hub_data)
	//	return -EINVAL;

	pr_info("[SSP]: %s - rev_cmd : 0x%0x, def_cmd : 0x%0lx\n",
		__func__, cmd, IOCTL_READ_BIG_CONTEXT_DATA);

	switch (cmd) {
	case IOCTL_READ_BIG_CONTEXT_DATA:
    argp = (void __user *)arg;
		mutex_lock(&hub_data->big_events_lock);
		length = hub_data->big_events.library_length;
		if (unlikely(!hub_data->big_events.library_data
			|| !hub_data->big_events.library_length)) {
			sensorhub_info("no big library data");
			mutex_unlock(&hub_data->big_events_lock);
			return 0;
		}

		while (retries--) {
			ret = copy_to_user(argp,
				hub_data->big_events.library_data,
				hub_data->big_events.library_length);
			if (likely(!ret))
				break;
		}
		if (unlikely(ret)) {
			sensorhub_err("read big library data err(%d)\n", ret);
			mutex_unlock(&hub_data->big_events_lock);
			return -EFAULT;
		}

		ssp_sensorhub_log(__func__,
			hub_data->big_events.library_data,
			hub_data->big_events.library_length);

		hub_data->is_big_event = false;
		kfree(hub_data->big_events.library_data);
		hub_data->big_events.library_data = NULL;
		hub_data->big_events.library_length = 0;
		complete(&hub_data->big_read_done);
		mutex_unlock(&hub_data->big_events_lock);
		break;

	case IOCTL_SSP_WOM_CMD:
		ret = copy_from_user(&wom, (void __user *)arg, sizeof(wom));
		if (unlikely(ret)) {
			sensorhub_err("get SSP_WOM_CMD failed\n");
			length = -EFAULT;
			break;
		}

		ret = handle_wom_image_cmd(&wom, hub_data);
		if (ret)
			length = ret;
		break;

	default:
		sensorhub_err("ioctl cmd err(%d)\n", cmd);
		return -EINVAL;
	}

	return length;
}

#ifdef CONFIG_COMPAT
static long ssp_sensorhub_compat_ioctl(struct file *file,
						unsigned int cmd, unsigned long arg)
{
	return ssp_sensorhub_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations ssp_sensorhub_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = ssp_sensorhub_write,
	.read = ssp_sensorhub_read,
	.unlocked_ioctl = ssp_sensorhub_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ssp_sensorhub_compat_ioctl,
#endif
};

void ssp_sensorhub_report_notice(struct ssp_data *ssp_data, char notice)
{
	struct ssp_sensorhub_data *hub_data = ssp_data->hub_data;

	input_report_rel(hub_data->sensorhub_input_dev, NOTICE, notice);
	input_sync(hub_data->sensorhub_input_dev);

	if (notice == MSG2SSP_AP_STATUS_WAKEUP)
		sensorhub_info("wake up");
	else if (notice == MSG2SSP_AP_STATUS_SLEEP)
		sensorhub_info("sleep");
	else if (notice == MSG2SSP_AP_STATUS_RESET)
		sensorhub_info("reset");
	else if (notice == MSG2SSP_HUB_STATUS_NOTI)
		sensorhub_info("Hub state 0x%lx\n", ssp_data->ulHubStateBit);
	else
		sensorhub_err("invalid notice(0x%x)", notice);
}

static void ssp_sensorhub_report_library(struct ssp_sensorhub_data *hub_data)
{
	input_report_rel(hub_data->sensorhub_input_dev, DATA, DATA);
	input_sync(hub_data->sensorhub_input_dev);
	wake_lock_timeout(&hub_data->sensorhub_wake_lock, WAKE_LOCK_TIMEOUT);
}

static void ssp_sensorhub_report_big_library(
			struct ssp_sensorhub_data *hub_data)
{
	input_report_rel(hub_data->sensorhub_input_dev, BIG_DATA, BIG_DATA);
	input_sync(hub_data->sensorhub_input_dev);
	wake_lock_timeout(&hub_data->sensorhub_wake_lock, WAKE_LOCK_TIMEOUT);
}

static int ssp_sensorhub_list(struct ssp_sensorhub_data *hub_data,
				char *dataframe, int length)
{
	struct sensorhub_event *event;
	int ret = 0;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	struct display_early_dpms_nb_event dpms_event;
#endif

	if (unlikely(length <= 0 || length >= PAGE_SIZE)) {
		sensorhub_err("library length err(%d)", length);
		return -EINVAL;
	}

#ifdef CONFIG_DISPLAY_EARLY_DPMS
	if ((length > 2) && (dataframe[2] == 19)) {
		dpms_event.id = DISPLAY_EARLY_DPMS_ID_PRIMARY;
		dpms_event.data = (void *)true;

		display_early_dpms_nb_send_event(DISPLAY_EARLY_DPMS_MODE_SET,
			(void *)&dpms_event);
		pr_info("[SSP]%s, wristup event received set ealry dpms mode \n",
			__func__);
	}
#endif
#ifdef CONFIG_KNOX_GEARPAY
	if ((length > 3) && (dataframe[0] == 0x01) && (dataframe[1] == 0x01)
			&& (dataframe[2] == 0x36)
			&& (dataframe[3] != strap_status)) {
		pr_info("[SSP]%s, STRAP = %d \n", __func__, dataframe[3]);
		strap_status = dataframe[3];
		if (dataframe[3] == 0x01) {
			exynos_smc_gearpay(STRAP, 1);
		}
		else if (dataframe[3] == 0x02) {
			exynos_smc_gearpay(STRAP, 0);
		}
	}
#endif
#ifdef CONFIG_SENSORHUB_STAT
	sensorhub_stat_rcv(dataframe, length);
#endif

	/* Does not handle GPS logging data, throw them all out
	 * They were already handled in sensrohub_stat_rcv()
	 */
	if ((length > 2) && (dataframe[0] == 0x01)
			&& (dataframe[1] == 0x05) && (dataframe[2] == 0x2f)) {
		pr_debug("[SSP] GPS logging data received\n");
		return 0;
	}

	if ((length > 2) && (dataframe[0] == 0x01) && (dataframe[1] == 0x01)
			&& (dataframe[2] == 0x3e)) {
		if (dataframe[3] <= 0) {
			pr_err("[SSP]: MSG From MCU - invalid debug length(%d/%d)\n",
				dataframe[3], length);
			return dataframe[3] ? dataframe[3] : ERROR;
		}
		pr_info("[SSP]: MSG From MCU - %s\n", dataframe + 4);
	}

	ssp_sensorhub_log(__func__, dataframe, length);

	/* overwrite new event if list is full */
	if (unlikely(kfifo_is_full(&hub_data->fifo))) {
		ret = kfifo_out(&hub_data->fifo, &event, sizeof(void *));
		if (unlikely(ret != sizeof(void *))) {
			sensorhub_err("kfifo out err(%d)", ret);
			return -EIO;
		}
		sensorhub_info("overwrite event");
	}

	/* allocate memory for new event */
	kfree(hub_data->events[hub_data->event_number].library_data);
	hub_data->events[hub_data->event_number].library_data
		= kzalloc(length * sizeof(char), GFP_ATOMIC);
	if (unlikely(!hub_data->events[hub_data->event_number].library_data)) {
		sensorhub_err("allocate memory for library err");
		return -ENOMEM;
	}

	/* copy new event into memory */
	memcpy(hub_data->events[hub_data->event_number].library_data,
		dataframe, length);
	hub_data->events[hub_data->event_number].library_length = length;

	/* add new event into the end of list */
	event = &hub_data->events[hub_data->event_number];
	ret = kfifo_in(&hub_data->fifo, &event, sizeof(void *));
	if (unlikely(ret != sizeof(void *))) {
		sensorhub_err("kfifo in err(%d)", ret);
		return -EIO;
	}

	/* not to overflow max list capacity */
	if (hub_data->event_number++ >= LIST_SIZE - 1)
		hub_data->event_number = 0;

	return kfifo_len(&hub_data->fifo) / sizeof(void *);
}

int ssp_sensorhub_handle_data(struct ssp_data *ssp_data, char *dataframe,
				int start, int end)
{
	struct ssp_sensorhub_data *hub_data = ssp_data->hub_data;
	int ret = 0;

	/* add new sensorhub event into list */
	spin_lock_bh(&hub_data->sensorhub_lock);
	ret = ssp_sensorhub_list(hub_data, dataframe+start, end-start);
	spin_unlock_bh(&hub_data->sensorhub_lock);

	if (ret < 0)
		sensorhub_err("sensorhub list err(%d)", ret);
	else if (ret > 0)
		wake_up(&hub_data->sensorhub_wq);

	return ret;
}

static int ssp_sensorhub_thread(void *arg)
{
	struct ssp_sensorhub_data *hub_data = (struct ssp_sensorhub_data *)arg;
	int ret = 0;

	while (likely(!kthread_should_stop())) {
		/* run thread if list is not empty */
		wait_event_interruptible(hub_data->sensorhub_wq,
				kthread_should_stop() ||
				!kfifo_is_empty(&hub_data->fifo) ||
				hub_data->is_big_event);

		/* exit thread if kthread should stop */
		if (unlikely(kthread_should_stop())) {
			sensorhub_info("kthread_stop()");
			break;
		}

		if (likely(!kfifo_is_empty(&hub_data->fifo))) {
			/* report sensorhub event to user */
			ssp_sensorhub_report_library(hub_data);
			/* wait until transfer finished */
			ret = wait_for_completion_timeout(
				&hub_data->read_done, COMPLETION_TIMEOUT);
			if (unlikely(!ret))
				sensorhub_err("wait for read timed out");
			else if (unlikely(ret < 0))
				sensorhub_err("read completion err(%d)", ret);
		}

		if (unlikely(hub_data->is_big_event)) {
			/* report big sensorhub event to user */
			ssp_sensorhub_report_big_library(hub_data);
			/* wait until transfer finished */
			ret = wait_for_completion_timeout(
				&hub_data->big_read_done, COMPLETION_TIMEOUT);
			if (unlikely(!ret))
				sensorhub_err("wait for big read timed out");
			else if (unlikely(ret < 0))
				sensorhub_err("big read completion err(%d)",
					ret);
		}
	}

	return 0;
}

void ssp_read_big_library_task(struct work_struct *work)
{
	struct ssp_big *big = container_of(work, struct ssp_big, work);
	struct ssp_sensorhub_data *hub_data = big->data->hub_data;
	struct ssp_msg *msg;
	int buf_len, residue, ret = 0, index = 0, pos = 0;

	mutex_lock(&hub_data->big_events_lock);
	if (hub_data->big_events.library_data) {
		sensorhub_info("!!WARNING big data didn't get from FW\n");
		ssp_sensorhub_log(__func__,
			hub_data->big_events.library_data,
			hub_data->big_events.library_length);
		kfree(hub_data->big_events.library_data);
	}

	residue = big->length;
	hub_data->big_events.library_length = big->length;
	hub_data->big_events.library_data = kzalloc(big->length, GFP_KERNEL);

	while (residue > 0) {
		buf_len = residue > DATA_PACKET_SIZE
			? DATA_PACKET_SIZE : residue;

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MSG2SSP_AP_GET_BIG_DATA;
		msg->length = buf_len;
		msg->options = AP2HUB_READ | (index++ << SSP_INDEX);
		msg->data = big->addr;
		msg->buffer = hub_data->big_events.library_data + pos;
		msg->free_buffer = 0;

		ret = ssp_spi_sync(big->data, msg, 1000);
		if (ret != SUCCESS) {
			sensorhub_err("read big data err(%d)", ret);
			break;
		}

		pos += buf_len;
		residue -= buf_len;

		sensorhub_info("read big data (%5d / %5d)", pos, big->length);
	}

	hub_data->is_big_event = true;
	wake_up(&hub_data->sensorhub_wq);
	kfree(big);
	mutex_unlock(&hub_data->big_events_lock);
}

void ssp_send_big_library_task(struct work_struct *work)
{
  struct ssp_big *big = container_of(work, struct ssp_big, work);
  struct ssp_sensorhub_data *hub_data = big->data->hub_data;
  struct ssp_msg *msg;
  int buf_len, residue, ret = 0, index = 0, pos = 0;
  int timeout = 1000;

  mutex_lock(&hub_data->big_events_lock);
  if (unlikely(!hub_data->big_send_events.library_data)) {
    mutex_unlock(&hub_data->big_events_lock);
    sensorhub_info("!!WARNING no big data for Sensorhub\n");
    kfree(big);
    return;
  }

  if (big->bigType == BIG_TYPE_WOM)
    big->length = hub_data->big_send_events.library_length;

  residue = big->length;

  while (residue > 0) {
    buf_len = residue > DATA_PACKET_SIZE
      ? DATA_PACKET_SIZE : residue;

    msg = kzalloc(sizeof(*msg), GFP_KERNEL);
    msg->cmd = MSG2SSP_AP_SET_BIG_DATA;
    msg->length = buf_len;
    msg->options = AP2HUB_WRITE | (index++ << SSP_INDEX);

    if (big->bigType == BIG_TYPE_WOM) {
      msg->data = 0;
      timeout = 2000;
    } else
      msg->data = big->addr;

    msg->buffer = hub_data->big_send_events.library_data + pos;
    msg->free_buffer = 0;

    ret = ssp_spi_sync(big->data, msg, timeout);
    if (ret != SUCCESS) {
      sensorhub_err("send big data err(%d)", ret);
      break;
    }

    pos += buf_len;
    residue -= buf_len;

    sensorhub_info("send big data(%u) (%5d / %5d)",
      big->bigType, pos, big->length);
  }
  mutex_unlock(&hub_data->big_events_lock);

  complete(&hub_data->big_write_done);
  kfree(big);
}


#ifdef CONFIG_SENSORS_SSP_VOICE
void ssp_pcm_dump_task(struct work_struct *work)
{
	struct ssp_big *big = container_of(work, struct ssp_big, work);
	struct ssp_sensorhub_data *hub_data = big->data->hub_data;
	struct ssp_msg *msg;
	int buf_len, residue = big->length, ret = 0, index = 0;

	mm_segment_t old_fs;
	struct file *voice_filp;
	char pcm_path[BIN_PATH_SIZE+1];
	char *buff;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	snprintf(pcm_path, BIN_PATH_SIZE,
		"/data/voice%d.pcm", hub_data->pcm_cnt);
	voice_filp = filp_open(pcm_path, O_RDWR | O_CREAT | O_APPEND, 0660);
	if (IS_ERR(voice_filp)) {
		sensorhub_err("open pcm dump file err");
		goto exit;
	}

	buf_len = big->length > DATA_PACKET_SIZE
			? DATA_PACKET_SIZE : big->length;
	buff = kzalloc(buf_len, GFP_KERNEL);

	while (residue > 0) {
		buf_len = residue > DATA_PACKET_SIZE
			? DATA_PACKET_SIZE : residue;

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MSG2SSP_AP_GET_BIG_DATA;
		msg->length = buf_len;
		msg->options = AP2HUB_READ | (index++ << SSP_INDEX);
		msg->data = big->addr;
		msg->buffer = buff;
		msg->free_buffer = 0;

		ret = ssp_spi_sync(big->data, msg, 1000);
		if (ret != SUCCESS) {
			sensorhub_err("receive pcm dump err(%d)", ret);
			break;
		}

		ret = vfs_write(voice_filp, (char __user *)buff, buf_len, &voice_filp->f_pos);

		if (ret < 0) {
			sensorhub_err("write pcm dump to file err(%d)", ret);
			break;
		}

		residue -= buf_len;
		sensorhub_info("write pcm dump...");
	}

	filp_close(voice_filp, current->files);
	kfree(buff);

exit:
	set_fs(old_fs);
	kfree(big);
}

int ssp_sensorhub_pcm_dump(struct ssp_sensorhub_data *hub_data)
{
	hub_data->pcm_cnt++;
	return set_big_data_start(hub_data->ssp_data, BIG_TYPE_VOICE_PCM, 0);
}
#endif

int ssp_sensorhub_initialize(struct ssp_data *ssp_data)
{
	struct ssp_sensorhub_data *hub_data;
	int ret;

	/* allocate memory for sensorhub data */
	hub_data = kzalloc(sizeof(*hub_data), GFP_KERNEL);
	if (!hub_data) {
		sensorhub_err("allocate memory for sensorhub data err");
		ret = -ENOMEM;
		goto exit;
	}
	hub_data->ssp_data = ssp_data;
	ssp_data->hub_data = hub_data;

	/* init wakelock, list, waitqueue, completion and spinlock */
	wake_lock_init(&hub_data->sensorhub_wake_lock, WAKE_LOCK_SUSPEND,
			"ssp_sensorhub_wake_lock");
	init_waitqueue_head(&hub_data->sensorhub_wq);
	init_completion(&hub_data->read_done);
	init_completion(&hub_data->big_read_done);
	init_completion(&hub_data->big_write_done);
	spin_lock_init(&hub_data->sensorhub_lock);
	mutex_init(&hub_data->big_events_lock);

	/* allocate sensorhub input device */
	hub_data->sensorhub_input_dev = input_allocate_device();
	if (!hub_data->sensorhub_input_dev) {
		sensorhub_err("allocate sensorhub input device err");
		ret = -ENOMEM;
		goto err_input_allocate_device_sensorhub;
	}

	/* set sensorhub input device */
	input_set_drvdata(hub_data->sensorhub_input_dev, hub_data);
	hub_data->sensorhub_input_dev->name = "ssp_context";
	input_set_capability(hub_data->sensorhub_input_dev, EV_REL, DATA);
	input_set_capability(hub_data->sensorhub_input_dev, EV_REL, BIG_DATA);
	input_set_capability(hub_data->sensorhub_input_dev, EV_REL, NOTICE);

	/* register sensorhub input device */
	ret = input_register_device(hub_data->sensorhub_input_dev);
	if (ret < 0) {
		sensorhub_err("register sensorhub input device err(%d)", ret);
		input_free_device(hub_data->sensorhub_input_dev);
		goto err_input_register_device_sensorhub;
	}

	/* register sensorhub misc device */
	hub_data->sensorhub_device.minor = MISC_DYNAMIC_MINOR;
	hub_data->sensorhub_device.name = "ssp_sensorhub";
	hub_data->sensorhub_device.fops = &ssp_sensorhub_fops;

	ret = misc_register(&hub_data->sensorhub_device);
	if (ret < 0) {
		sensorhub_err("register sensorhub misc device err(%d)", ret);
		goto err_misc_register;
	}

	/* allocate fifo */
	ret = kfifo_alloc(&hub_data->fifo,
		sizeof(void *) * LIST_SIZE, GFP_KERNEL);
	if (ret) {
		sensorhub_err("kfifo allocate err(%d)", ret);
		goto err_kfifo_alloc;
	}

	/* create and run sensorhub thread */
	hub_data->sensorhub_task = kthread_run(ssp_sensorhub_thread,
				(void *)hub_data, "ssp_sensorhub_thread");
	if (IS_ERR(hub_data->sensorhub_task)) {
		ret = PTR_ERR(hub_data->sensorhub_task);
		goto err_kthread_run;
	}

	return 0;

err_kthread_run:
	kfifo_free(&hub_data->fifo);
err_kfifo_alloc:
	misc_deregister(&hub_data->sensorhub_device);
err_misc_register:
	input_unregister_device(hub_data->sensorhub_input_dev);
err_input_register_device_sensorhub:
err_input_allocate_device_sensorhub:
	complete_all(&hub_data->big_write_done);
	complete_all(&hub_data->big_read_done);
	complete_all(&hub_data->read_done);
	wake_lock_destroy(&hub_data->sensorhub_wake_lock);
	kfree(hub_data);
exit:
	return ret;
}

void ssp_sensorhub_remove(struct ssp_data *ssp_data)
{
	struct ssp_sensorhub_data *hub_data = ssp_data->hub_data;

	kthread_stop(hub_data->sensorhub_task);
	kfifo_free(&hub_data->fifo);
	misc_deregister(&hub_data->sensorhub_device);
	input_unregister_device(hub_data->sensorhub_input_dev);
	complete_all(&hub_data->big_write_done);
	mutex_destroy(&hub_data->big_events_lock);
	complete_all(&hub_data->big_read_done);
	complete_all(&hub_data->read_done);
	wake_lock_destroy(&hub_data->sensorhub_wake_lock);
	kfree(hub_data);
}

#ifdef CONFIG_COPR_SUPPORT
void convert_candela(char *buf)
{
	int candela = 0;

	candela = convert_to_candela(buf[0]);

	if (candela != 0) {
		buf[0] = candela & 0xff;
		buf[1] = (candela & 0xff00) >> 8;
	}
}
#endif

MODULE_DESCRIPTION("Seamless Sensor Platform(SSP) sensorhub driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
