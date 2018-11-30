/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include <linux/math64.h>
#include <linux/ioctl.h>
#include <linux/compat.h>
#include <linux/types.h>
#include <linux/delay.h>
#include "ssp.h"

#define BATCH_IOCTL_MAGIC		0xFC
#define SENSORHUB_DUMP_FILE_PATH "/var/log/sensor_reg_dump"

struct batch_config {
	int64_t timeout;
	int64_t delay;
	int flag;
};

/*************************************************************************/
/* SSP data delay function                                              */
/*************************************************************************/

int get_msdelay(int64_t dDelayRate)
{
	return div_s64(dDelayRate, 1000000);
}

static void enable_sensor(struct ssp_data *data,
	int iSensorType, int64_t dNewDelay)
{
	u8 uBuf[9];
	u64 uNewEnable = 0;
	s32 maxBatchReportLatency = 0;
	s8 batchOptions = 0;
	int64_t dTempDelay = data->adDelayBuf[iSensorType];
	s32 dMsDelay = get_msdelay(dNewDelay);
	int ret = 0;

	data->adDelayBuf[iSensorType] = dNewDelay;
	maxBatchReportLatency = data->batchLatencyBuf[iSensorType];
	batchOptions = data->batchOptBuf[iSensorType];

	switch (data->aiCheckStatus[iSensorType]) {
	case ADD_SENSOR_STATE:
		ssp_dbg("[SSP]: %s - add %u, New = %lldns\n",
			 __func__, 1 << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;

		ret = send_instruction(data, ADD_SENSOR,
				iSensorType, uBuf, 9);
		pr_info("[SSP], delay %d, timeout %d, flag=%d, ret%d",
			dMsDelay, maxBatchReportLatency, uBuf[8], ret);
		if (ret <= 0) {
			uNewEnable =
				(u64)atomic64_read(&data->aSensorEnable)
				& (~(u64)(1 << iSensorType));
			atomic64_set(&data->aSensorEnable, uNewEnable);

			data->aiCheckStatus[iSensorType] = NO_SENSOR_STATE;
			break;
		}

		data->aiCheckStatus[iSensorType] = RUNNING_SENSOR_STATE;

		break;
	case RUNNING_SENSOR_STATE:
		if (get_msdelay(dTempDelay)
			== get_msdelay(data->adDelayBuf[iSensorType]))
			break;

		ssp_dbg("[SSP]: %s - Change %u, New = %lldns\n",
			__func__, 1 << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;
		send_instruction(data, CHANGE_DELAY, iSensorType, uBuf, 9);

		break;
	default:
		data->aiCheckStatus[iSensorType] = ADD_SENSOR_STATE;
	}
}

static void change_sensor_delay(struct ssp_data *data,
	int iSensorType, int64_t dNewDelay)
{
	u8 uBuf[9];
	s32 maxBatchReportLatency = 0;
	s8 batchOptions = 0;
	int64_t dTempDelay = data->adDelayBuf[iSensorType];
	s32 dMsDelay = get_msdelay(dNewDelay);

	data->adDelayBuf[iSensorType] = dNewDelay;
	data->batchLatencyBuf[iSensorType] = maxBatchReportLatency;
	data->batchOptBuf[iSensorType] = batchOptions;

	switch (data->aiCheckStatus[iSensorType]) {
	case RUNNING_SENSOR_STATE:
		if (get_msdelay(dTempDelay)
			== get_msdelay(data->adDelayBuf[iSensorType]))
			break;

		ssp_dbg("[SSP]: %s - Change %u, New = %lldns\n",
			__func__, 1 << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;
		send_instruction(data, CHANGE_DELAY, iSensorType, uBuf, 9);

		break;
	default:
		break;
	}
}

/*************************************************************************/
/* SSP data enable function                                              */
/*************************************************************************/

static int ssp_remove_sensor(struct ssp_data *data,
	unsigned int uChangedSensor, u64 uNewEnable)
{
	u8 uBuf[4];
	int64_t dSensorDelay = data->adDelayBuf[uChangedSensor];

	ssp_dbg("[SSP]: %s - remove sensor = %lld, current state = %lld\n",
		__func__, (u64)(1 << uChangedSensor), uNewEnable);

	data->batchLatencyBuf[uChangedSensor] = 0;
	data->batchOptBuf[uChangedSensor] = 0;

	if (uChangedSensor == ORIENTATION_SENSOR) {
		if (!(atomic64_read(&data->aSensorEnable)
			& (1 << ACCELEROMETER_SENSOR))) {
			uChangedSensor = ACCELEROMETER_SENSOR;
		} else {
			change_sensor_delay(data, ACCELEROMETER_SENSOR,
				data->adDelayBuf[ACCELEROMETER_SENSOR]);
			return 0;
		}
	} else if (uChangedSensor == ACCELEROMETER_SENSOR) {
		if (atomic64_read(&data->aSensorEnable)
			& (1 << ORIENTATION_SENSOR)) {
			change_sensor_delay(data, ORIENTATION_SENSOR,
				data->adDelayBuf[ORIENTATION_SENSOR]);
			return 0;
		}
	}

	if (!data->bSspShutdown)
		if (atomic64_read(&data->aSensorEnable) & (1 << uChangedSensor)) {
			s32 dMsDelay = get_msdelay(dSensorDelay);
			memcpy(&uBuf[0], &dMsDelay, 4);

			send_instruction(data, REMOVE_SENSOR,
				uChangedSensor, uBuf, 4);
		}
	data->aiCheckStatus[uChangedSensor] = NO_SENSOR_STATE;

	return 0;
}

/*************************************************************************/
/* ssp Sysfs                                                             */
/*************************************************************************/

static ssize_t show_enable_irq(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - %d\n", __func__, !data->bSspShutdown);

	return snprintf(buf, PAGE_SIZE, "%d\n", !data->bSspShutdown);
}

static ssize_t set_enable_irq(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u8 dTemp;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtou8(buf, 10, &dTemp) < 0)
		return -1;

	pr_info("[SSP] %s - %d start\n", __func__, dTemp);
	if (dTemp) {
		reset_mcu(data);
		enable_debug_timer(data);
	} else if (!dTemp) {
		disable_debug_timer(data);
		ssp_enable(data, 0);
	} else
		pr_err("[SSP] %s - invalid value\n", __func__);
	pr_info("[SSP] %s - %d end\n", __func__, dTemp);
	return size;
}

static ssize_t show_sensors_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - cur_enable = %llu\n", __func__,
		(u64)(atomic64_read(&data->aSensorEnable)));

	return snprintf(buf, PAGE_SIZE, "%llu\n",
		(u64)(atomic64_read(&data->aSensorEnable)));
}

static ssize_t set_sensors_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dTemp;
	u64 uNewEnable = 0;
	unsigned int uChangedSensor = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dTemp) < 0)
		return -EINVAL;

	uNewEnable = (u64)dTemp;
	ssp_dbg("[SSP]: %s - new_enable = %llu, old_enable = %lu\n", __func__,
		 uNewEnable, atomic64_read(&data->aSensorEnable));

	if ((uNewEnable != atomic64_read(&data->aSensorEnable)) &&
		!(data->uSensorState &
		(uNewEnable - atomic64_read(&data->aSensorEnable)))) {
		pr_info("[SSP] %s - %llu is not connected(sensor state: 0x%llx)\n",
			__func__,
			uNewEnable - atomic64_read(&data->aSensorEnable),
			data->uSensorState);
		return -EINVAL;
	}

	if (uNewEnable == atomic64_read(&data->aSensorEnable))
		return size;

	for (uChangedSensor = 0; uChangedSensor < SENSOR_MAX;
		uChangedSensor++) {
		if ((atomic64_read(&data->aSensorEnable) & (1 << uChangedSensor))
		!= (uNewEnable & (1 << uChangedSensor))) {

			if (!(uNewEnable & (1 << uChangedSensor))) {
				data->reportedData[uChangedSensor] = false;

				ssp_remove_sensor(data, uChangedSensor,
					uNewEnable); /* disable */
			} else { /* Change to ADD_SENSOR_STATE from KitKat */
				data->aiCheckStatus[uChangedSensor]
					= ADD_SENSOR_STATE;

				enable_sensor(data, uChangedSensor,
					data->adDelayBuf[uChangedSensor]);
			}
		}
	}
	atomic64_set(&data->aSensorEnable, uNewEnable);

	return size;
}

static ssize_t set_cal_data(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dTemp;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dTemp) < 0)
		return -EINVAL;

	if (unlikely(data->bSspShutdown)) {
		pr_err("[SSP]%s, stop sending cal data(shutdown)", __func__);
		return -EBUSY;
	}

	if (dTemp == 1) {
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
		accel_open_calibration(data);
		set_accel_cal(data);
#endif

#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
		gyro_open_calibration(data);
		set_gyro_cal(data);
#endif

#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
		pressure_open_calibration(data);
		set_pressure_cal(data);
#endif

#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
		hrm_open_calibration(data);
		set_hrm_calibration(data);
#endif
	}

	return size;
}

static ssize_t wom_cmd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", (int)data->bWOMode);
}

static ssize_t wom_cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_sensorhub_data *hub_data;
	struct sensorhub_wom wom;
	int val;

	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	pr_info("[SSP]: %s: Forcely send wom_cmd (%d)\n",
			__func__, val);

	wom.size = 0;
	wom.addr = 0;

	hub_data = data->hub_data;

	switch (val) {
#ifdef CONFIG_SENSORS_BBD_WOM_SUPPORT
	case CHANGE_NORMAL_MODE:
	case CHANGE_WATCHONLY_MODE:
	case INT_WOM_IMAGE_USE:
	case INST_WOM_START:
		wom.cmd_type = (char)val;
		break;

	case 40:
		wom.cmd_type = INST_WOM_START;
		wom.size = 1;
		break;
	case 41:
		wom.cmd_type = INT_WOM_IMAGE_USE;
		val = handle_wom_image_cmd(&wom, hub_data);
		if (val < 0) {
			pr_err("[SSP]: %s: WOM_IMAGE handle failed(%d)\n",
				__func__, val);
			return val;
		}

		pr_info("[SSP]: %s: WOM_IMAGE handle done\n", __func__);
		mdelay(500);

		wom.cmd_type = INST_WOM_START;
		break;
#endif

	default:
		pr_err("[SSP]: %s: Unsupported wom_cmd (%d)\n",
			__func__, val);
		return -EINVAL;
	}

	val = handle_wom_image_cmd(&wom, hub_data);
	if (val < 0) {
		pr_err("[SSP]: %s: wom_cmd handle failed(%d)\n",
			__func__, val);
		return val;
	}

	pr_info("[SSP]: %s: [%d] send wom_cmd done\n",
		__func__, (int)wom.cmd_type);

	return size;
}

static ssize_t ssp_cmd_ctl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", (int)data->uCmdCtl);
}

static ssize_t ssp_cmd_ctl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u8 old_val;
	int val;

	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	old_val = data->uCmdCtl;
	data->uCmdCtl = (u8)val;
	pr_info("[SSP] %s : SSP cmd ctl update by %u (from %u)\n",
		__func__, data->uCmdCtl, old_val);

	return size;
}

static ssize_t hub_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%llu\n", (u64)data->ulHubStateBit);
}

static ssize_t hub_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	unsigned long long UserAck;
	unsigned long userbit, oldstate;
	int bit;

	if (kstrtoull(buf, 10, &UserAck) < 0)
		return -EINVAL;

	pr_info("[SSP] %s : UserAck triggered by 0x%llx\n",
		__func__, UserAck);
	userbit = (unsigned long)UserAck;

	oldstate = data->ulHubStateBit;

	for_each_set_bit(bit, &userbit, EVT2FW_HUB_STATE_MAX_BITS_SZ)
		clear_bit(bit, &data->ulHubStateBit);

	pr_info("[SSP] %s : HubStateBit update by 0x%lx (from 0x%lx)\n",
		__func__, data->ulHubStateBit, oldstate);

	return size;
}

static ssize_t sensorhub_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iRet=0;
	u8 buffer[SENSORHUB_DUMP_SIZE];
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	struct ssp_data *data = dev_get_drvdata(dev);

	//Read dump from sensorhub
	iRet = sensorhub_dump_read(data, buffer);
	if(iRet != SUCCESS)
	{
		pr_err("[SSP]: %s - dump read error\n", __func__);
		return 0;
	}

	//Store to file
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	cal_filp = filp_open(SENSORHUB_DUMP_FILE_PATH,
			O_CREAT | O_TRUNC | O_RDWR, 0660);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		pr_err("[SSP]: %s - Can't open sensorhub dump file, %d\n", __func__, iRet);
		return -EIO;
	}

	iRet = vfs_write(cal_filp, (char *)buffer, sizeof(buffer), &cal_filp->f_pos);
	if (iRet != sizeof(buffer)) {
		pr_err("[SSP]: %s - Can't write sensorhub dump to file\n", __func__);
		return -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_err("[SSP]: %s - Success\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
		buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9],
		buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15], buffer[16], buffer[17], buffer[18], buffer[19],
		buffer[20], buffer[21], buffer[22], buffer[23], buffer[24], buffer[25], buffer[26], buffer[27], buffer[28], buffer[29],
		buffer[30], buffer[31], buffer[32], buffer[33], buffer[34], buffer[35], buffer[36], buffer[37], buffer[38], buffer[39],
		buffer[40], buffer[41], buffer[42], buffer[43], buffer[44], buffer[45], buffer[46], buffer[47], buffer[48], buffer[49],
		buffer[50], buffer[51], buffer[52], buffer[53], buffer[54], buffer[55], buffer[56], buffer[57], buffer[58], buffer[59],
		buffer[60], buffer[61], buffer[62], buffer[63], buffer[64], buffer[65], buffer[66], buffer[67], buffer[68], buffer[69],
		buffer[70], buffer[71], buffer[72], buffer[73], buffer[74], buffer[75], buffer[76], buffer[77], buffer[78], buffer[79],
		buffer[80], buffer[81], buffer[82], buffer[83], buffer[84], buffer[85], buffer[86], buffer[87], buffer[88], buffer[89],
		buffer[90], buffer[91], buffer[92], buffer[93], buffer[94], buffer[95], buffer[96], buffer[97], buffer[98], buffer[99],
		buffer[100], buffer[101], buffer[102], buffer[103], buffer[104], buffer[105], buffer[106], buffer[107], buffer[108], buffer[109],
		buffer[110], buffer[111], buffer[112], buffer[113], buffer[114], buffer[115], buffer[116], buffer[117], buffer[118], buffer[119],
		buffer[120], buffer[121], buffer[122], buffer[123], buffer[124], buffer[125], buffer[126], buffer[127], buffer[128], buffer[129],
		buffer[130], buffer[131], buffer[132], buffer[133], buffer[134], buffer[135], buffer[136], buffer[137], buffer[138], buffer[139],
		buffer[140], buffer[141], buffer[142], buffer[143], buffer[144], buffer[145], buffer[146], buffer[147], buffer[148], buffer[149],
		buffer[150], buffer[151], buffer[152], buffer[153], buffer[154], buffer[155], buffer[156], buffer[157], buffer[158], buffer[159],
		buffer[160], buffer[161], buffer[162], buffer[163], buffer[164], buffer[165], buffer[166], buffer[167], buffer[168], buffer[169],
		buffer[170], buffer[171], buffer[172], buffer[173], buffer[174], buffer[175], buffer[176], buffer[177], buffer[178], buffer[179],
		buffer[180], buffer[181], buffer[182], buffer[183], buffer[184], buffer[185], buffer[186], buffer[187], buffer[188], buffer[189],
		buffer[190], buffer[191], buffer[192], buffer[193], buffer[194], buffer[195], buffer[196], buffer[197], buffer[198], buffer[199],
		buffer[200], buffer[201], buffer[202], buffer[203], buffer[204], buffer[205], buffer[206], buffer[207], buffer[208], buffer[209],
		buffer[210], buffer[211], buffer[212], buffer[213], buffer[214], buffer[215], buffer[216], buffer[217], buffer[218], buffer[219],
		buffer[220], buffer[221], buffer[222], buffer[223], buffer[224], buffer[225], buffer[226], buffer[227], buffer[228], buffer[229],
		buffer[230], buffer[231], buffer[232], buffer[233], buffer[234], buffer[235], buffer[236], buffer[237], buffer[238], buffer[239],
		buffer[240], buffer[241], buffer[242], buffer[243], buffer[244], buffer[245], buffer[246], buffer[247], buffer[248], buffer[249],
		buffer[250], buffer[251], buffer[252], buffer[253], buffer[254], buffer[255], buffer[256], buffer[257], buffer[258], buffer[259],
		buffer[260], buffer[261], buffer[262], buffer[263], buffer[264], buffer[265], buffer[266], buffer[267], buffer[268], buffer[269],
		buffer[270], buffer[271], buffer[272], buffer[273], buffer[274], buffer[275], buffer[276], buffer[277], buffer[278], buffer[279],
		buffer[280], buffer[281], buffer[282], buffer[283], buffer[284], buffer[285], buffer[286], buffer[287], buffer[288], buffer[289],
		buffer[290], buffer[291], buffer[292], buffer[293], buffer[294], buffer[295], buffer[296], buffer[297], buffer[298], buffer[299]);
}

static ssize_t sensorhub_dump_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t show_acc_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[ACCELEROMETER_SENSOR]);
}

static ssize_t set_acc_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	if ((atomic64_read(&data->aSensorEnable) & (1 << ORIENTATION_SENSOR)) &&
		(data->adDelayBuf[ORIENTATION_SENSOR] < dNewDelay))
		data->adDelayBuf[ACCELEROMETER_SENSOR] = dNewDelay;
	else
		change_sensor_delay(data, ACCELEROMETER_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_gyro_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GYROSCOPE_SENSOR]);
}

static ssize_t set_gyro_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GYROSCOPE_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_uncalib_gyro_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GYRO_UNCALIB_SENSOR]);
}

static ssize_t set_uncalib_gyro_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GYRO_UNCALIB_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_mag_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GEOMAGNETIC_SENSOR]);
}

static ssize_t set_mag_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GEOMAGNETIC_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_uncal_mag_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GEOMAGNETIC_UNCALIB_SENSOR]);
}

static ssize_t set_uncal_mag_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GEOMAGNETIC_UNCALIB_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_pressure_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[PRESSURE_SENSOR]);
}

static ssize_t set_pressure_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, PRESSURE_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_light_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[LIGHT_SENSOR]);
}

static ssize_t set_light_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, LIGHT_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_temp_humi_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[TEMPERATURE_HUMIDITY_SENSOR]);
}

static ssize_t set_temp_humi_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, TEMPERATURE_HUMIDITY_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_rot_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[ROTATION_VECTOR]);
}

static ssize_t set_rot_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, ROTATION_VECTOR, dNewDelay);

	return size;
}

static ssize_t show_game_rot_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GAME_ROTATION_VECTOR]);
}

static ssize_t set_game_rot_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GAME_ROTATION_VECTOR, dNewDelay);

	return size;
}

static ssize_t show_hrm_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[HRM_RAW_SENSOR]);
}

static ssize_t set_hrm_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, HRM_RAW_SENSOR, dNewDelay);
	change_sensor_delay(data, HRM_RAW_FAC_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_hrm_lib_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[HRM_LIB_SENSOR]);
}

static ssize_t set_hrm_lib_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, HRM_LIB_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_front_hrm_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[FRONT_HRM_RAW_SENSOR]);
}

static ssize_t set_front_hrm_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, FRONT_HRM_RAW_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_uv_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[UV_SENSOR]);
}

static ssize_t set_uv_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, UV_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_gsr_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n", sdev->adDelayBuf[GSR_SENSOR]);
}

static ssize_t set_gsr_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(sdev, GSR_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_ecg_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n", sdev->adDelayBuf[ECG_SENSOR]);
}

static ssize_t set_ecg_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(sdev, ECG_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_grip_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		sdev->adDelayBuf[GRIP_SENSOR]);
}

static ssize_t set_grip_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(sdev, GRIP_SENSOR, dNewDelay);

	return size;
}

static DEVICE_ATTR(mcu_rev, S_IRUGO, mcu_revision_show, NULL);
static DEVICE_ATTR(mcu_name, S_IRUGO, mcu_model_name_show, NULL);
static DEVICE_ATTR(mcu_update, S_IRUGO, mcu_update_kernel_bin_show, NULL);
static DEVICE_ATTR(mcu_update2, S_IRUGO,
	mcu_update_kernel_crashed_bin_show, NULL);
static DEVICE_ATTR(mcu_update_ums, S_IRUGO, mcu_update_ums_bin_show, NULL);
static DEVICE_ATTR(mcu_reset, S_IRUGO, mcu_reset_show, NULL);
static DEVICE_ATTR(mcu_ready, S_IRUGO, mcu_ready_show, NULL);
static DEVICE_ATTR(mcu_dump, S_IRUGO, mcu_dump_show, NULL);
static DEVICE_ATTR(mcu_sensorstate, S_IRUGO, mcu_sensor_state, NULL);
static DEVICE_ATTR(mcu_test, S_IRUGO | S_IWUSR | S_IWGRP,
	mcu_factorytest_show, mcu_factorytest_store);
static DEVICE_ATTR(mcu_sleep_test, S_IRUGO | S_IWUSR | S_IWGRP,
	mcu_sleep_factorytest_show, mcu_sleep_factorytest_store);
static DEVICE_ATTR(fota_rotate_info, S_IRUGO, mcu_fota_rotate_status_show, NULL);


static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	show_sensors_enable, set_sensors_enable);
static DEVICE_ATTR(enable_irq, S_IRUGO | S_IWUSR | S_IWGRP,
	show_enable_irq, set_enable_irq);
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
static DEVICE_ATTR(accel_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_acc_delay, set_acc_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
static DEVICE_ATTR(gyro_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gyro_delay, set_gyro_delay);
static DEVICE_ATTR(uncal_gyro_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncalib_gyro_delay, set_uncalib_gyro_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
static DEVICE_ATTR(mag_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_mag_delay, set_mag_delay);
static DEVICE_ATTR(uncal_mag_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncal_mag_delay, set_uncal_mag_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
static DEVICE_ATTR(pressure_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_pressure_delay, set_pressure_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
static DEVICE_ATTR(light_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_light_delay, set_light_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
static DEVICE_ATTR(temp_humid_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_temp_humi_delay, set_temp_humi_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
static DEVICE_ATTR(rot_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_rot_delay, set_rot_delay);
static DEVICE_ATTR(game_rot_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_game_rot_delay, set_game_rot_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
static DEVICE_ATTR(hrm_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_delay, set_hrm_delay);
static DEVICE_ATTR(hrm_lib_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_lib_delay, set_hrm_lib_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
static DEVICE_ATTR(front_hrm_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_front_hrm_delay, set_front_hrm_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
static DEVICE_ATTR(uv_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uv_delay, set_uv_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
static DEVICE_ATTR(gsr_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gsr_delay, set_gsr_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
static DEVICE_ATTR(ecg_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_ecg_delay, set_ecg_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
static DEVICE_ATTR(grip_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_grip_delay, set_grip_delay);
#endif

static DEVICE_ATTR(set_cal_data, S_IWUSR | S_IWGRP,
	NULL, set_cal_data);
static DEVICE_ATTR(hub_state, 0664, hub_state_show, hub_state_store);
static DEVICE_ATTR(ssp_cmd_ctl, 0664, ssp_cmd_ctl_show, ssp_cmd_ctl_store);
static DEVICE_ATTR(wom_cmd, 0664, wom_cmd_show, wom_cmd_store);
static DEVICE_ATTR(sensor_reg_dump, 0664, sensorhub_dump_show, sensorhub_dump_store);

static struct device_attribute *mcu_attrs[] = {
	&dev_attr_enable,
	&dev_attr_mcu_rev,
	&dev_attr_mcu_name,
	&dev_attr_mcu_test,
	&dev_attr_mcu_reset,
	&dev_attr_mcu_ready,
	&dev_attr_mcu_sensorstate,
	&dev_attr_mcu_dump,
	&dev_attr_mcu_update,
	&dev_attr_mcu_update2,
	&dev_attr_mcu_update_ums,
	&dev_attr_mcu_sleep_test,
	&dev_attr_fota_rotate_info,
	&dev_attr_enable_irq,
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
	&dev_attr_accel_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
	&dev_attr_gyro_poll_delay,
	&dev_attr_uncal_gyro_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
	&dev_attr_mag_poll_delay,
	&dev_attr_uncal_mag_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
	&dev_attr_pressure_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
	&dev_attr_light_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
	&dev_attr_temp_humid_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
	&dev_attr_rot_poll_delay,
	&dev_attr_game_rot_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
	&dev_attr_hrm_poll_delay,
	&dev_attr_hrm_lib_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
	&dev_attr_front_hrm_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
	&dev_attr_uv_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
	&dev_attr_gsr_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
	&dev_attr_ecg_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
	&dev_attr_grip_poll_delay,
#endif
	&dev_attr_set_cal_data,
	&dev_attr_hub_state,
	&dev_attr_ssp_cmd_ctl,
	&dev_attr_wom_cmd,
	&dev_attr_sensor_reg_dump,
	NULL,
};

static struct device_attribute dev_attr_input_accel_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_acc_delay, set_acc_delay);
static struct device_attribute dev_attr_input_gyro_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gyro_delay, set_gyro_delay);
static struct device_attribute dev_attr_input_uncalib_gyro_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncalib_gyro_delay, set_uncalib_gyro_delay);
static struct device_attribute dev_attr_input_mag_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_mag_delay, set_mag_delay);
static struct device_attribute dev_attr_input_uncal_mag_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncal_mag_delay, set_uncal_mag_delay);
static struct device_attribute dev_attr_input_pressure_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_pressure_delay, set_pressure_delay);
static struct device_attribute dev_attr_input_light_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_light_delay, set_light_delay);
static struct device_attribute dev_attr_input_temp_humi_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_temp_humi_delay, set_temp_humi_delay);
static struct device_attribute dev_attr_input_rot_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_rot_delay, set_rot_delay);
static struct device_attribute dev_attr_input_game_rot_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_game_rot_delay, set_game_rot_delay);
static struct device_attribute dev_attr_input_hrm_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_delay, set_hrm_delay);
static struct device_attribute dev_attr_input_hrm_lib_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_lib_delay, set_hrm_lib_delay);
static struct device_attribute dev_attr_input_front_hrm_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_front_hrm_delay, set_front_hrm_delay);
static struct device_attribute dev_attr_input_uv_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uv_delay, set_uv_delay);
static struct device_attribute dev_attr_input_gsr_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gsr_delay, set_gsr_delay);
static struct device_attribute dev_attr_input_ecg_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_ecg_delay, set_ecg_delay);
static struct device_attribute dev_attr_input_grip_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_grip_delay, set_grip_delay);

static void initialize_mcu_factorytest(struct ssp_data *data)
{
	sensors_register(data->mcu_device, data, mcu_attrs, "ssp_sensor");
}

static void remove_mcu_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->mcu_device, mcu_attrs);
}

static void remove_input_poll_delay_sysfs(struct ssp_data *data)
{
	if (data->grip_input_dev)
		device_remove_file(&data->grip_input_dev->dev,
			&dev_attr_input_grip_poll_delay);
	if (data->ecg_input_dev)
		device_remove_file(&data->ecg_input_dev->dev,
			&dev_attr_input_ecg_poll_delay);
	if (data->gsr_input_dev)
		device_remove_file(&data->gsr_input_dev->dev,
			&dev_attr_input_gsr_poll_delay);
	if (data->uv_input_dev)
		device_remove_file(&data->uv_input_dev->dev,
			&dev_attr_input_uv_poll_delay);
	if (data->front_hrm_raw_input_dev)
		device_remove_file(&data->front_hrm_raw_input_dev->dev,
			&dev_attr_input_front_hrm_poll_delay);
	if (data->hrm_raw_input_dev)
		device_remove_file(&data->hrm_raw_input_dev->dev,
			&dev_attr_input_hrm_poll_delay);
	if (data->hrm_lib_input_dev)
		device_remove_file(&data->hrm_lib_input_dev->dev,
			&dev_attr_input_hrm_lib_poll_delay);
	if (data->game_rot_input_dev)
		device_remove_file(&data->game_rot_input_dev->dev,
			&dev_attr_input_game_rot_poll_delay);
	if (data->rot_input_dev)
		device_remove_file(&data->rot_input_dev->dev,
			&dev_attr_input_game_rot_poll_delay);
	if (data->temp_humi_input_dev)
		device_remove_file(&data->temp_humi_input_dev->dev,
			&dev_attr_input_temp_humi_poll_delay);
	if (data->light_input_dev)
		device_remove_file(&data->light_input_dev->dev,
			&dev_attr_input_light_poll_delay);
	if (data->pressure_input_dev)
		device_remove_file(&data->pressure_input_dev->dev,
			&dev_attr_input_pressure_poll_delay);
	if (data->uncalib_gyro_input_dev)
		device_remove_file(&data->uncalib_gyro_input_dev->dev,
			&dev_attr_input_uncalib_gyro_poll_delay);
	if (data->uncal_mag_input_dev)
		device_remove_file(&data->uncal_mag_input_dev->dev,
			&dev_attr_input_uncal_mag_poll_delay);
	if (data->mag_input_dev)
		device_remove_file(&data->mag_input_dev->dev,
			&dev_attr_input_mag_poll_delay);
	if (data->gyro_input_dev)
		device_remove_file(&data->gyro_input_dev->dev,
			&dev_attr_input_gyro_poll_delay);
	if (data->acc_input_dev)
		device_remove_file(&data->acc_input_dev->dev,
			&dev_attr_input_accel_poll_delay);
}

static int initialize_input_poll_delay_sysfs(struct ssp_data *data)
{
	int iRet;

	if (data->acc_input_dev) {
		iRet = device_create_file(&data->acc_input_dev->dev,
					&dev_attr_input_accel_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->gyro_input_dev) {
		iRet = device_create_file(&data->gyro_input_dev->dev,
					&dev_attr_input_gyro_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->mag_input_dev) {
		iRet = device_create_file(&data->mag_input_dev->dev,
					&dev_attr_input_mag_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->uncal_mag_input_dev) {
		iRet = device_create_file(&data->uncal_mag_input_dev->dev,
					&dev_attr_input_uncal_mag_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->uncalib_gyro_input_dev) {
		iRet = device_create_file(&data->uncalib_gyro_input_dev->dev,
				&dev_attr_input_uncalib_gyro_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->pressure_input_dev) {
		iRet = device_create_file(&data->pressure_input_dev->dev,
					&dev_attr_input_pressure_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->light_input_dev) {
		iRet = device_create_file(&data->light_input_dev->dev,
				&dev_attr_input_light_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->temp_humi_input_dev) {
		iRet = device_create_file(&data->temp_humi_input_dev->dev,
				&dev_attr_input_temp_humi_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->rot_input_dev) {
		iRet = device_create_file(&data->rot_input_dev->dev,
			&dev_attr_input_rot_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->game_rot_input_dev) {
		iRet = device_create_file(&data->game_rot_input_dev->dev,
			&dev_attr_input_game_rot_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->hrm_raw_input_dev) {
		iRet = device_create_file(&data->hrm_raw_input_dev->dev,
					&dev_attr_input_hrm_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->hrm_lib_input_dev) {
		iRet = device_create_file(&data->hrm_lib_input_dev->dev,
					&dev_attr_input_hrm_lib_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->front_hrm_raw_input_dev) {
		iRet = device_create_file(&data->front_hrm_raw_input_dev->dev,
					&dev_attr_input_front_hrm_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->uv_input_dev) {
		iRet = device_create_file(&data->uv_input_dev->dev,
					&dev_attr_input_uv_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->gsr_input_dev) {
		iRet = device_create_file(&data->gsr_input_dev->dev,
				&dev_attr_input_gsr_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->ecg_input_dev) {
		iRet = device_create_file(&data->ecg_input_dev->dev,
				&dev_attr_input_ecg_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->grip_input_dev) {
		iRet = device_create_file(&data->grip_input_dev->dev,
				&dev_attr_input_grip_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	return SUCCESS;

err_create_poll_delay:
	remove_input_poll_delay_sysfs(data);

	return ERROR;
}

static long ssp_batch_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct ssp_data *data
		= container_of(file->private_data,
			struct ssp_data, batch_io_device);

	struct batch_config batch;

	void __user *argp = (void __user *)arg;
	int retries = 2;
	int ret = 0;
	int sensor_type, ms_delay;
	int timeout_ms = 0;
	u8 uBuf[9];

	sensor_type = (cmd & 0xFF);

	if ((cmd >> 8 & 0xFF) != BATCH_IOCTL_MAGIC) {
		pr_err("[SSP] Invalid BATCH CMD %x\n", cmd);
		return -EINVAL;
	}

	while (retries--) {
		ret = copy_from_user(&batch, argp, sizeof(batch));
		if (likely(!ret))
			break;
	}
	if (unlikely(ret)) {
		pr_err("[SSP] batch ioctl err(%d)\n", ret);
		return -EINVAL;
	}

	pr_info("[SSP] batch ioctl [arg] delay:%lld, timeout:%lld, flag:0x%0x\n",
		batch.delay, batch.timeout, batch.flag);

	ms_delay = get_msdelay(batch.delay);
	timeout_ms = div_s64(batch.timeout, 1000000);
	memcpy(&uBuf[0], &ms_delay, 4);
	memcpy(&uBuf[4], &timeout_ms, 4);
	uBuf[8] = batch.flag;

	/* For arg input checking */
	if ((batch.timeout < 0) || (batch.delay <= 0)) {
		pr_err("[SSP] Invalid batch values!!\n");
		return -EINVAL;
	} else if ((batch.timeout != 0) && (timeout_ms == 0)) {
		pr_err("[SSP] Invalid batch timeout range!!\n");
		return -EINVAL;
	} else if ((batch.delay != 0) && (ms_delay == 0)) {
		pr_err("[SSP] Invalid batch delay range!!\n");
		return -EINVAL;
	}

	if (batch.timeout) { /* add or dry */

		if(!(batch.flag & SENSORS_BATCH_DRY_RUN)) { /* real batch, NOT DRY, change delay */
			ret = 1;
			/* if sensor is not running state, enable will be called.
			   MCU return fail when receive chage delay inst during NO_SENSOR STATE */
			if (data->aiCheckStatus[sensor_type] == RUNNING_SENSOR_STATE) {
				ret = send_instruction_sync(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			}
			if (ret > 0) { // ret 1 is success
				data->batchOptBuf[sensor_type] = (u8)batch.flag;
				data->batchLatencyBuf[sensor_type] = timeout_ms;
				data->adDelayBuf[sensor_type] = batch.delay;
			}
		} else { /* real batch, DRY RUN */
			ret = send_instruction_sync(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			if (ret > 0) { // ret 1 is success
				data->batchOptBuf[sensor_type] = (u8)batch.flag;
				data->batchLatencyBuf[sensor_type] = timeout_ms;
				data->adDelayBuf[sensor_type] = batch.delay;
			}
		}
	} else { /* remove batch or normal change delay, remove or add will be called. */

		if (!(batch.flag & SENSORS_BATCH_DRY_RUN)) { /* no batch, NOT DRY, change delay */
			data->batchOptBuf[sensor_type] = 0;
			data->batchLatencyBuf[sensor_type] = 0;
			data->adDelayBuf[sensor_type] = batch.delay;
			if (data->aiCheckStatus[sensor_type] == RUNNING_SENSOR_STATE) {
				send_instruction(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			}
		}
	}

	pr_info("[SSP] batch %d: delay %lld(ms_delay:%d), timeout %lld(timeout_ms:%d), flag %d, ret %d \n",
		sensor_type, batch.delay, ms_delay, batch.timeout, timeout_ms, batch.flag, ret);
	if (!batch.timeout)
		return 0;
	if (ret <= 0)
		return -EINVAL;
	else
		return 0;
}

#ifdef CONFIG_COMPAT
static long ssp_batch_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return ssp_batch_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static struct file_operations ssp_batch_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.unlocked_ioctl = ssp_batch_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ssp_batch_compat_ioctl,
#endif
};

int initialize_sysfs(struct ssp_data *data)
{
	initialize_factorytest(data);

	initialize_mcu_factorytest(data);
	if (initialize_input_poll_delay_sysfs(data) != SUCCESS)
		goto err_init_input_sysfs;

	data->batch_io_device.minor = MISC_DYNAMIC_MINOR;
	data->batch_io_device.name = "batch_io";
	data->batch_io_device.fops = &ssp_batch_fops;
	if (misc_register(&data->batch_io_device))
		goto err_batch_io_dev;

	return SUCCESS;

err_batch_io_dev:
	remove_input_poll_delay_sysfs(data);
err_init_input_sysfs:
	remove_mcu_factorytest(data);
	remove_factorytest(data);

	return ERROR;
}

void remove_sysfs(struct ssp_data *data)
{
	misc_deregister(&data->batch_io_device);

	remove_input_poll_delay_sysfs(data);

	remove_factorytest(data);

	remove_mcu_factorytest(data);

	destroy_sensor_class();
}
