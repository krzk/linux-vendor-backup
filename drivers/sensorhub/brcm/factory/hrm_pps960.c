/*
 * Copyright (c)2013 Maxim Integrated Products, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "../ssp.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define VENDOR		"TI"
#define CHIP_ID		"PPS960"
#define MODULE_NAME "AFE4410"

#define EOL_DATA_FILE_PATH "/csa/sensor/hrm_eol_data"

#define EOL_DATA_SIZE 24

static ssize_t hrm_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t hrm_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t hrm_module_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MODULE_NAME);
}

static ssize_t hrm_led_g_ir_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  int iRet = 0;

  int32_t buffer[2] = {0, };
  int32_t g_led=0, ir=0;
  struct ssp_data *data = dev_get_drvdata(dev);
  struct ssp_msg *msg;

  msg = kzalloc(sizeof(*msg), GFP_KERNEL);
  if (msg == NULL) {
    pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
      __func__);
    goto exit;
  }
  msg->cmd = HRM_G_IR_LED;
  msg->length = 8;
  msg->options = AP2HUB_READ;
  msg->buffer = (char *)&buffer;
  msg->free_buffer = 0;

  iRet = ssp_spi_sync(data, msg, 10000);
  if (iRet != SUCCESS) {
    pr_err("[SSP] %s - hrm green led Timeout!!\n", __func__);
    goto exit;
  }

  g_led = buffer[0];
  ir = buffer[1];

  if ((g_led>2050000)&&(ir>2050000))
  {
    iRet = 1;
  }
  else
  {
    iRet = 0;
  }

  ssp_dbg("[SSP] : %s, %d, %d Ret:%d\n", __func__, g_led, ir, iRet);


exit:
  return sprintf(buf, "%d\n", iRet);
}

static ssize_t hrm_led_b_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  int iRet = 0;

  int32_t buffer[2] = {0, };
  int32_t b_led=0, r_led=0;
  struct ssp_data *data = dev_get_drvdata(dev);
  struct ssp_msg *msg;

  msg = kzalloc(sizeof(*msg), GFP_KERNEL);
  if (msg == NULL) {
    pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
      __func__);
    goto exit;
  }
  msg->cmd = HRM_B_R_LED;
  msg->length = 8;
  msg->options = AP2HUB_READ;
  msg->buffer = (char *)&buffer;
  msg->free_buffer = 0;

  iRet = ssp_spi_sync(data, msg, 10000);
  if (iRet != SUCCESS) {
    pr_err("[SSP] %s - hrm blue & red led Timeout!!\n", __func__);
    goto exit;
  }

  b_led = buffer[0];
  r_led = buffer[1];

  if ((b_led>2050000)&&(r_led>2050000))
  {
    iRet = 1;
  }
  else
  {
    iRet = 0;
  }

  ssp_dbg("[SSP] : %s, %d, %d Ret:%d\n", __func__, b_led, r_led, iRet);


exit:
  return sprintf(buf, "%d\n", iRet);
}

static ssize_t hrm_noise_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  int iRet = 0;

  int32_t buffer[3] = {0, };
  int32_t pd1=0, pd2=0, pd3=0;
  struct ssp_data *data = dev_get_drvdata(dev);
  struct ssp_msg *msg;

  msg = kzalloc(sizeof(*msg), GFP_KERNEL);
  if (msg == NULL) {
    pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
      __func__);
    goto exit;
  }
  msg->cmd = HRM_NOISE;
  msg->length = 12;
  msg->options = AP2HUB_READ;
  msg->buffer = (char *)&buffer;
  msg->free_buffer = 0;

  iRet = ssp_spi_sync(data, msg, 10000);
  if (iRet != SUCCESS) {
    pr_err("[SSP] %s - hrm noise Timeout!!\n", __func__);
    goto exit;
  }

  pd1 = buffer[0];
  pd2 = buffer[1];
  pd3 = buffer[2];

  if ((pd1>2050000)&&(pd2>2050000)&&(pd3>2050000))
  {
    iRet = 1;
  }
  else
  {
    iRet = 0;
  }

  ssp_dbg("[SSP] : %s, %d, %d, %d Ret:%d\n", __func__, pd1, pd2, pd3, iRet);

exit:
  return sprintf(buf, "%d %d %d %d\n", iRet, pd1, pd2, pd3);
}

static int hrm_open_eol_data(struct ssp_data *data)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;
	u32 eol_data[EOL_DATA_SIZE];

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(EOL_DATA_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		pr_info("[SSP] %s : file open fail %d\n", __func__, iRet);
		memset(data->hrmcal, 0x00, sizeof(data->hrmcal));

		return iRet;
	}

	iRet = vfs_read(cal_filp, (char *)&eol_data, EOL_DATA_SIZE * sizeof(u32), &cal_filp->f_pos);

	if (iRet != EOL_DATA_SIZE  * sizeof(u32))
		iRet = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_info("[SSP] %s: [%d %d %d %d %d %d %d %d]\n", __func__,
		eol_data[0], eol_data[1], eol_data[2], eol_data[3],
		eol_data[4], eol_data[5], eol_data[6], eol_data[7]);
	pr_info("[SSP] %s: [%d %d %d %d %d %d %d %d]\n", __func__,
		eol_data[8], eol_data[9], eol_data[10], eol_data[11],
		eol_data[12], eol_data[13], eol_data[14], eol_data[15]);
	pr_info("[SSP] %s: [%d %d %d %d %d %d %d %d]\n", __func__,
		eol_data[16], eol_data[17], eol_data[18], eol_data[19],
		eol_data[20], eol_data[21], eol_data[22], eol_data[23]);

	memcpy(data->hrmcal, eol_data, sizeof(eol_data));

	return iRet;
}

static int save_hrm_eol_data(struct ssp_data *data)
{
	int iRet = 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(EOL_DATA_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP]: %s - Can't open osc_reg_value file\n", __func__);
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		return -EIO;
	}

	iRet = vfs_write(cal_filp, (char *)&data->buf[HRM_RAW_FAC_SENSOR], EOL_DATA_SIZE * sizeof(u32), &cal_filp->f_pos);

	if (iRet != EOL_DATA_SIZE * sizeof(u32)) {
		pr_err("[SSP]: %s - Can't write hrm osc_reg_value to file\n",
			__func__);
		iRet = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return iRet;
}


int pps960_set_hrm_calibration(struct ssp_data *data)
{
	int i, iRet = 0;
	struct ssp_msg *msg;

	if (!(data->uSensorState & (1 << HRM_RAW_SENSOR))) {
		for (i = 0; i < EOL_DATA_SIZE; i++)
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[i] = 0;

		pr_info("[SSP]: %s - Skip this function!!!"\
			", hrm sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return iRet;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n", __func__);
		return -ENOMEM;
	}
	msg->cmd = MSG2SSP_AP_MCU_SET_HRM_OSC_REG;
	msg->length = 4 * EOL_DATA_SIZE;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char *) kzalloc(4 * EOL_DATA_SIZE, GFP_KERNEL);

	msg->free_buffer = 1;
	memcpy(msg->buffer,
		&data->hrmcal[0], 4 * EOL_DATA_SIZE);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	pr_info("[SSP] %s: Set hrm cal data %d, %d, %d, %d, %d, %d, %d, %d",
		__func__, data->hrmcal[0], data->hrmcal[1], data->hrmcal[2],
		data->hrmcal[3], data->hrmcal[4], data->hrmcal[5],
		data->hrmcal[6], data->hrmcal[7]);
	pr_info("[SSP] %s: %d, %d, %d, %d, %d, %d, %d, %d\n", __func__,
		data->hrmcal[8], data->hrmcal[9], data->hrmcal[10],
		data->hrmcal[11], data->hrmcal[12], data->hrmcal[13],
		data->hrmcal[14], data->hrmcal[15]);
	pr_info("[SSP] %s: %d, %d, %d, %d, %d, %d, %d, %d\n", __func__,
		data->hrmcal[16], data->hrmcal[17], data->hrmcal[18],
		data->hrmcal[19], data->hrmcal[20], data->hrmcal[21],data->hrmcal[22], data->hrmcal[23]);

	return iRet;
}

int pps960_hrm_open_calibration(struct ssp_data *data)
{
	int iRet = 0;

	hrm_open_eol_data(data);

	return iRet;
}

static ssize_t hrm_lib_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char buffer[10]  = {0, };
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n", __func__);
		goto exit;
	}
	msg->cmd = HRM_LIB_VERSION_INFO;
	msg->length = HRM_LIB_VERSION_INFO_LENGTH;
	msg->options = AP2HUB_READ;
	msg->buffer = (char*)&buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 3000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s - hrm lib version Timeout!!\n", __func__);
		goto exit;
	}

	return sprintf(buf, "%x %x %x %x %x %x %x %x %x %x\n",
			buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
			buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
exit:
	return sprintf(buf, "%d\n", iRet);
}

static ssize_t hrm_eol_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	ssize_t count;

	mutex_lock(&data->sysfs_op_mtx);
	memcpy(data->hrmcal, data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data,
		sizeof(data->hrmcal));

	save_hrm_eol_data(data);
	set_hrm_calibration(data);

	count = snprintf(buf, PAGE_SIZE,
		"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[0],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[1],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[2],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[3],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[4],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[5],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[6],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[7],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[8],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[9],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[10],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[11],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[12],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[13],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[14],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[15],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[16],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[17],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[18],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[19],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[20],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[21],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[22],
			data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data[23]);
	mutex_unlock(&data->sysfs_op_mtx);

	return count;
}

static ssize_t hrm_eol_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	mutex_lock(&data->sysfs_op_mtx);
	if (dEnable)
		atomic_set(&data->eol_enable, 1);
	else
		atomic_set(&data->eol_enable, 0);
	mutex_unlock(&data->sysfs_op_mtx);

	return size;
}

static ssize_t hrm_raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		data->buf[HRM_RAW_SENSOR].ch_a_sum,
		data->buf[HRM_RAW_SENSOR].ch_a_x1,
		data->buf[HRM_RAW_SENSOR].ch_a_x2,
		data->buf[HRM_RAW_SENSOR].ch_a_y1,
		data->buf[HRM_RAW_SENSOR].ch_a_y2,
		data->buf[HRM_RAW_SENSOR].ch_b_sum,
		data->buf[HRM_RAW_SENSOR].ch_b_x1,
		data->buf[HRM_RAW_SENSOR].ch_b_x2,
		data->buf[HRM_RAW_SENSOR].ch_b_y1,
		data->buf[HRM_RAW_SENSOR].ch_b_y2);
}

static ssize_t hrm_lib_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[HRM_LIB_SENSOR].hr,
		data->buf[HRM_LIB_SENSOR].rri,
		data->buf[HRM_LIB_SENSOR].snr);
}

static ssize_t hrm_eol_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;
	u32 eol_data[EOL_DATA_SIZE];

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(EOL_DATA_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);

		memset(eol_data, 0, sizeof(eol_data));
		goto exit;
	}

	iRet = vfs_read(cal_filp, (char *)&eol_data, EOL_DATA_SIZE * sizeof(u32), &cal_filp->f_pos);

	if (iRet != EOL_DATA_SIZE * sizeof(u32))
		iRet = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

exit:
	return snprintf(buf, PAGE_SIZE, "%d %d %d %d "\
		"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		eol_data[0], eol_data[1], eol_data[2], eol_data[3],
		eol_data[4], eol_data[5], eol_data[6], eol_data[7],
		eol_data[8], eol_data[9], eol_data[10], eol_data[11],
		eol_data[12], eol_data[13], eol_data[14], eol_data[15],
		eol_data[16], eol_data[17], eol_data[18], eol_data[19],
		eol_data[20], eol_data[21], eol_data[22], eol_data[23]);
}

static ssize_t hrm_ir_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int32_t buffer	= 0;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n", __func__);
		goto exit;
	}
	msg->cmd = HRM_IR_LEVEL_THRESHOLD;
	msg->length = HRM_IR_LEVEL_THRESHOLD_LENGTH;
	msg->options = AP2HUB_READ;
	msg->buffer = (char*)&buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 3000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s - hrm ir threshold Timeout!!\n", __func__);
		goto exit;
	}

	pr_info("[SSP] %s - %d\n", __func__, buffer);

	return sprintf(buf, "%d\n", buffer);
exit:
	return sprintf(buf, "%d\n", iRet);
}

static ssize_t hrm_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iRet = 1;

	int32_t buffer[6]	= {0, };
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		goto exit;
	}
	msg->cmd = HRM_FACTORY;
	msg->length = 24;
	msg->options = AP2HUB_READ;
	msg->buffer = (char *)&buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 3000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s - hrm ir threshold Timeout!!\n", __func__);
		goto exit;
	}
	pr_info("[SSP] %s- gain=0x%x ambgain=0x%x ir=%d red=%d green=%d amb=%d\n",
		__func__, buffer[0], buffer[1], buffer[2],
		buffer[3], buffer[4], buffer[5]);

	if ((buffer[0] == 0x8005 && buffer[1] == 0x0005)
		&& (buffer[2] > 0 && buffer[2] < 4194304)
		&& (buffer[3] > 0 && buffer[3] < 4194304)
		&& (buffer[4] > 0 && buffer[4] < 4194304)
		&& (buffer[5] > 0 && buffer[5] < 4194304)) {
		if (buffer[2] == 0 && buffer[3] == 0 &&
			buffer[4] == 0 && buffer[4] == 0)
			iRet = 0;
		else
			iRet = 1;
	} else
		iRet = 0;

exit:
	return sprintf(buf, "%d\n", iRet);
}

static DEVICE_ATTR(name, S_IRUGO, hrm_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, hrm_vendor_show, NULL);
static DEVICE_ATTR(hrm_lib_ver, S_IRUGO, hrm_lib_version_show, NULL);
static DEVICE_ATTR(hrm_eol, S_IRUGO | S_IWUSR | S_IWGRP, hrm_eol_show, hrm_eol_store);
static DEVICE_ATTR(hrm_raw, S_IRUGO, hrm_raw_data_read, NULL);
static DEVICE_ATTR(hrm_lib, S_IRUGO, hrm_lib_data_read, NULL);
static DEVICE_ATTR(hrm_eol_data, S_IRUGO, hrm_eol_data_show, NULL);
static DEVICE_ATTR(hrm_ir_threshold, S_IRUGO, hrm_ir_threshold_show, NULL);
static DEVICE_ATTR(selftest, S_IRUGO, hrm_selftest_show, NULL);
static DEVICE_ATTR(module, S_IRUGO, hrm_module_show, NULL);
static DEVICE_ATTR(hrm_led_g_ir, S_IRUGO, hrm_led_g_ir_show, NULL);
static DEVICE_ATTR(hrm_led_b_r, S_IRUGO, hrm_led_b_r_show, NULL);
static DEVICE_ATTR(hrm_noise, S_IRUGO, hrm_noise_show, NULL);

static struct device_attribute *hrm_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_hrm_lib_ver,
	&dev_attr_hrm_eol,
	&dev_attr_hrm_raw,
	&dev_attr_hrm_lib,
	&dev_attr_hrm_eol_data,
	&dev_attr_hrm_ir_threshold,
	&dev_attr_selftest,
	&dev_attr_module,
	&dev_attr_hrm_led_g_ir,
	&dev_attr_hrm_led_b_r,
	&dev_attr_hrm_noise,
	NULL,
};

void initialize_pps960_hrm_factorytest(struct ssp_data *data)
{
	sensors_register(data->hrm_device, data, hrm_attrs,
		"hrm_sensor");
}

void remove_pps960_hrm_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->hrm_device, hrm_attrs);
}
