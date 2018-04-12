/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/bug.h>
#include <linux/v4l2-mediabus.h>
#include <linux/gpio.h>
#include <linux/dma-iommu.h>
#include <linux/iommu.h>

#include <linux/of.h>
#include <linux/of_gpio.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-fan53555.h"

#include "sensor/fimc-is-device-6d1.h"
#include "sensor/fimc-is-device-imx240.h"
#include "fimc-is-sec-define.h"
#include "fimc-is-device-ois.h"
#include "fimc-is-device-af.h"

struct fimc_is_from_info *sysfs_finfo = NULL;
struct fimc_is_from_info *sysfs_pinfo = NULL;

struct class *camera_class = NULL;
struct device *camera_front_dev;
struct device *camera_rear_dev;
struct device *camera_ois_dev;

struct device *fimc_is_dev = NULL;
struct fimc_is_core *sysfs_core;

extern bool crc32_fw_check;
extern bool crc32_check;
extern bool crc32_check_factory;
extern bool fw_version_crc_check;
extern bool is_latest_cam_module;
extern bool is_final_cam_module;
extern bool is_right_prj_name;
extern bool crc32_c1_fw_check;
extern bool crc32_c1_check;
extern bool crc32_c1_check_factory;

extern int fimc_is_3a0_video_probe(void *data);
extern int fimc_is_3a1_video_probe(void *data);
extern int fimc_is_isp_video_probe(void *data);
extern int fimc_is_scc_video_probe(void *data);
extern int fimc_is_scp_video_probe(void *data);
extern int fimc_is_vdc_video_probe(void *data);
extern int fimc_is_vdo_video_probe(void *data);
extern int fimc_is_3a0c_video_probe(void *data);
extern int fimc_is_3a1c_video_probe(void *data);

/* sysfs global variable for debug */
struct fimc_is_sysfs_debug sysfs_debug;

int dma_alloc_coherent_at(struct device *dev, unsigned int size, void **kaddr, dma_addr_t dma_addr, gfp_t flags)
{
	struct iommu_domain *domain = iommu_get_domain_for_dev(dev);
	struct sg_table sgt;
	dma_addr_t alloc_dma_addr;
	int ret;

	*kaddr = dma_alloc_coherent(dev, size, &alloc_dma_addr, flags);

	dev_info(dev, "Allocated %d buffer at %pad\n", size, &alloc_dma_addr);

	/*
	 * HW requires firmware to be mapped at the begging of its address
	 * space. IOMMU_DMA glue layer uses allocator, which assigns
	 * virtual addresses from the end of defined address space.
	 * There is no direct way to enforce different IOVA address for the
	 * allocated buffer, so as a workaround, the firmware buffer will
	 * be mapped second time at the beggining of the address space.
	 */

	if (iommu_dma_reserve(dev, dma_addr, size))
		return -EBUSY;
	dev_info(dev, "Reserved %d bytes at %pad\n", size, &dma_addr);

	ret = dma_get_sgtable(dev, &sgt, *kaddr, alloc_dma_addr, size);

	if (iommu_map_sg(domain, dma_addr, sgt.sgl, sgt.nents,
			 IOMMU_READ | IOMMU_WRITE) != size) {
		ret = -ENOMEM;
	}

	dev_info(dev, "Remapped buffer to %pad address\n", &dma_addr);

	sg_free_table(&sgt);

	return ret;
}

static int fimc_is_ischain_allocmem(struct fimc_is_core *this)
{
	struct device *dev = &this->pdev->dev;
	int ret = 0;
	/* void *fw_cookie; */
	size_t fw_size =
#ifdef ENABLE_ODC
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
#endif
#ifdef ENABLE_VDIS
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
#endif
#ifdef ENABLE_TDNR
				SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF +
#endif
			FIMC_IS_A5_MEM_SIZE;

	fw_size = PAGE_ALIGN(fw_size);

	dbg_core("Allocating memory for FIMC-IS firmware.\n");

#if 0
	fw_cookie = vb2_ion_private_alloc(this->mem.alloc_ctx, fw_size, 1, 0);

	if (IS_ERR(fw_cookie)) {
		err("Allocating bitprocessor buffer failed");
		fw_cookie = NULL;
		ret = -ENOMEM;
		goto exit;
	}

	ret = vb2_ion_dma_address(fw_cookie, &this->minfo.dvaddr);
	if ((ret < 0) || (this->minfo.dvaddr  & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.");
		vb2_ion_private_free(fw_cookie);
		this->minfo.dvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto exit;
	}
	dbg_core("Device vaddr = %08x , size = %08x\n",
		this->minfo.dvaddr, FIMC_IS_A5_MEM_SIZE);

	this->minfo.kvaddr = vb2_ion_private_vaddr(fw_cookie);
	if (IS_ERR(this->minfo.kvaddr)) {
		err("Bitprocessor memory remap failed");
		vb2_ion_private_free(fw_cookie);
		this->minfo.kvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto exit;
	}

	vb2_ion_sync_for_device(fw_cookie, 0, fw_size, DMA_BIDIRECTIONAL);

exit:
	info("[COR] Device virtual for internal: %08x\n", this->minfo.kvaddr);
	this->minfo.fw_cookie = fw_cookie;
#endif

	this->minfo.dvaddr = 0x10000000;
	ret = dma_alloc_coherent_at(dev, fw_size,
				    &this->minfo.kvaddr, this->minfo.dvaddr,
				    GFP_KERNEL);
	if (ret)
		return -ENOMEM;

	/* memset((void *)this->minfo.kvaddr, 0, fw_size); */

	dev_info(dev, "FIMC-IS CPU memory base: %pad\n", &this->minfo.dvaddr);

	if (((u32)this->minfo.dvaddr) & FIMC_IS_FW_BASE_MASK) {
		dev_err(dev, "invalid firmware memory alignment: %pad\n",
			&this->minfo.dvaddr);
		dma_free_coherent(dev, fw_size, this->minfo.kvaddr,
				  this->minfo.dvaddr);
		return -EIO;
	}

	return 0;
}

static int fimc_is_ishcain_initmem(struct fimc_is_core *this)
{
	int ret = 0;
	u32 offset;

	dbg_core("fimc_is_init_mem - ION\n");

	ret = fimc_is_ischain_allocmem(this);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		ret = -ENOMEM;
		goto exit;
	}

	offset = FW_SHARED_OFFSET;
	this->minfo.dvaddr_fshared = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_fshared = this->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE;
	this->minfo.dvaddr_region = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_region = this->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE;
#ifdef ENABLE_ODC
	this->minfo.dvaddr_odc = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_odc = this->minfo.kvaddr + offset;
	offset += (SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
#else
	this->minfo.dvaddr_odc = 0;
	this->minfo.kvaddr_odc = 0;
#endif

#ifdef ENABLE_VDIS
	this->minfo.dvaddr_dis = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_dis = this->minfo.kvaddr + offset;
	offset += (SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
#else
	this->minfo.dvaddr_dis = 0;
	this->minfo.kvaddr_dis = 0;
#endif

#ifdef ENABLE_TDNR
	this->minfo.dvaddr_3dnr = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_3dnr = this->minfo.kvaddr + offset;
	offset += (SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF);
#else
	this->minfo.dvaddr_3dnr = 0;
	this->minfo.kvaddr_3dnr = 0;
#endif

	dbg_core("fimc_is_init_mem done\n");

exit:
	return ret;
}

static ssize_t camera_front_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_platform_fimc_is_sensor *sensor = dev_get_drvdata(dev);

	dev_info(dev, "%s: E", __func__);

	if (unlikely(!sensor)) {
		dev_err(dev, "%s: sensor null\n", __func__);
		return -EFAULT;
	}

	return sprintf(buf, "%d\n", sensor->sensor_id);
}

static ssize_t camera_rear_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_platform_fimc_is_sensor *sensor = dev_get_drvdata(dev);

	dev_info(dev, "%s: E", __func__);

	if (unlikely(!sensor)) {
		dev_err(dev, "%s: sensor null\n", __func__);
		return -EFAULT;
	}

	return sprintf(buf, "%d\n", sensor->sensor_id);
}

static DEVICE_ATTR(front_sensorid, S_IRUGO, camera_front_sensorid_show, NULL);
static DEVICE_ATTR(rear_sensorid, S_IRUGO, camera_rear_sensorid_show, NULL);

int fimc_is_get_sensor_data(struct device *dev, char *maker, char *name, int position)
{
	struct exynos_platform_fimc_is_sensor *sensor = dev_get_drvdata(dev);
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *device;
	int i;

	if (unlikely(!sensor)) {
		dev_err(dev, "%s: sensor null\n", __func__);
		return -EFAULT;
	}

	if (!fimc_is_dev) {
		dev_err(dev, "%s: fimc_is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	core = dev_get_drvdata(fimc_is_dev);
	if (!core) {
		dev_err(dev, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_REAR) {
		device = &core->sensor[position];
	} else {
		dev_err(dev, "%s: position value is wrong", __func__);
		return -EINVAL;
	}

	for (i = 0; i < atomic_read(&core->resourcemgr.rsccount_module); i++) {
		if (sensor->sensor_id == device->module_enum[i].id) {
			if (maker != NULL)
				sprintf(maker, "%s", device->module_enum[i].sensor_maker ?
						device->module_enum[i].sensor_maker : "UNKNOWN");
			if (name != NULL)
				sprintf(name, "%s", device->module_enum[i].sensor_name ?
						device->module_enum[i].sensor_name : "UNKNOWN");
			return 0;
		}
	}

	dev_err(dev, "%s: there's no matched sensor id", __func__);

	return -ENODEV;
}

static ssize_t camera_front_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_maker[50];
	char sensor_name[50];
	int ret;

	ret = fimc_is_get_sensor_data(dev, sensor_maker, sensor_name, SENSOR_POSITION_FRONT);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_FIMC_IS\n");
	else
		return sprintf(buf, "%s_%s_FIMC_IS\n", sensor_maker, sensor_name);
}

static ssize_t camera_front_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_name[50];
	int ret;

	ret = fimc_is_get_sensor_data(dev, NULL, sensor_name, SENSOR_POSITION_FRONT);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN UNKNOWN\n");
	else
		return sprintf(buf, "%s N\n", sensor_name);
}

static DEVICE_ATTR(front_camtype, S_IRUGO,
		camera_front_camtype_show, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO, camera_front_camfw_show, NULL);

static struct fimc_is_from_info *pinfo = NULL;
static struct fimc_is_from_info *finfo = NULL;

int read_from_firmware_version(void)
{
	char fw_name[100];
	char setf_name[100];
	char master_setf_name[100];
	char mode_setf_name[100];
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;

	fimc_is_sec_get_sysfs_pinfo(&pinfo);
	fimc_is_sec_get_sysfs_finfo(&finfo);

	if (!finfo->is_caldata_read) {
		if (finfo->bin_start_addr != 0x80000) {
			pr_debug("pm_runtime_suspended = %d\n",
			pm_runtime_suspended(is_dev));
			pm_runtime_get_sync(is_dev);

			fimc_is_sec_fw_sel(sysfs_core, is_dev, fw_name, setf_name, false);
			fimc_is_sec_concord_fw_sel(sysfs_core, is_dev, fw_name, master_setf_name, mode_setf_name);
			pm_runtime_put_sync(is_dev);
			pr_debug("pm_runtime_suspended = %d\n",
				pm_runtime_suspended(is_dev));
		}
	}
	return 0;
}

static ssize_t camera_rear_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_maker[50];
	char sensor_name[50];
	int ret;

	ret = fimc_is_get_sensor_data(dev, sensor_maker, sensor_name, SENSOR_POSITION_REAR);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_FIMC_IS\n");
	else
		return sprintf(buf, "%s_%s_FIMC_IS\n", sensor_maker, sensor_name);
}

static ssize_t camera_rear_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };
	char *loaded_fw;

	read_from_firmware_version();

	fimc_is_sec_get_loaded_fw(&loaded_fw);

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
		    && crc32_c1_fw_check && crc32_c1_check_factory
		) {
			return sprintf(buf, "%s %s\n", finfo->header_ver, loaded_fw);
		} else {
			strcpy(command_ack, "NG_");
			if (!crc32_fw_check)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
			if (!crc32_c1_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_c1_check_factory)
				strcat(command_ack, "CD1");
			if (finfo->header_ver[3] != 'L')
				strcat(command_ack, "_Q");
			return sprintf(buf, "%s %s\n", finfo->header_ver, command_ack);
		}
	} else {
		strcpy(command_ack, "NG_");
		strcat(command_ack, "FWCD");
		strcat(command_ack, "FW1CD1");
		return sprintf(buf, "%s %s\n", finfo->header_ver, command_ack);
	}
}

static ssize_t camera_rear_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };
	char *loaded_fw;

	read_from_firmware_version();

	fimc_is_sec_get_loaded_fw(&loaded_fw);

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
		    && crc32_c1_fw_check && crc32_c1_check_factory
		) {
			return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, loaded_fw);
		} else {
			strcpy(command_ack, "NG_");
			if (!crc32_fw_check)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
			if (!crc32_c1_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_c1_check_factory)
				strcat(command_ack, "CD1");
			if (finfo->header_ver[3] != 'L')
				strcat(command_ack, "_Q");
			return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, command_ack);
		}
	} else {
		strcpy(command_ack, "NG_");
		strcat(command_ack, "FWCD");
		strcat(command_ack, "FW1CD1");
		return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, command_ack);
	}
}

static ssize_t camera_rear_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version();

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
		    && crc32_c1_fw_check && crc32_c1_check_factory
		) {
			if (!is_latest_cam_module
				|| !is_right_prj_name
			) {
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_rear_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version();

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
		    && crc32_c1_fw_check && crc32_c1_check_factory
		) {
			if (!is_final_cam_module
				|| !is_right_prj_name
			) {
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_rear_companionfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *loaded_c1_fw;

	read_from_firmware_version();
	fimc_is_sec_get_loaded_c1_fw(&loaded_c1_fw);

	return sprintf(buf, "%s %s\n",
		finfo->concord_header_ver, loaded_c1_fw);
}

static ssize_t camera_rear_companionfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *loaded_c1_fw;

	read_from_firmware_version();
	fimc_is_sec_get_loaded_c1_fw(&loaded_c1_fw);

	return sprintf(buf, "%s %s %s\n",
		finfo->concord_header_ver, pinfo->concord_header_ver, loaded_c1_fw);
}

static ssize_t camera_rear_camfw_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = -EINVAL;

	if ((size == 1 || size == 2) && (buf[0] == 'F' || buf[0] == 'f')) {
		fimc_is_sec_set_force_caldata_dump(true);
		ret = size;
	} else {
		fimc_is_sec_set_force_caldata_dump(false);
	}
	return ret;
}

static ssize_t camera_rear_calcheck_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char rear_sensor[10] = {0, };
	char rear_companion[10] = {0, };

	read_from_firmware_version();

	if (crc32_check_factory)
		strcpy(rear_sensor, "Normal");
	else
		strcpy(rear_sensor, "Abnormal");

	if (crc32_c1_check_factory)
		strcpy(rear_companion, "Normal");
	else
		strcpy(rear_companion, "Abnormal");

	return sprintf(buf, "%s %s %s\n", rear_sensor, rear_companion, "Null");
}

static ssize_t camera_isp_core_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int sel;

	if (DCDC_VENDOR_NONE == sysfs_core->companion_dcdc.type)
		return sprintf(buf, "none\n");

	sel = fimc_is_power_binning(sysfs_core);
	return sprintf(buf, "%s\n", sysfs_core->companion_dcdc.get_vout_str(sel));
}

static ssize_t camera_ois_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)

{
	switch (buf[0]) {
	case '0':
		fimc_is_ois_gpio_off(sysfs_core->companion);
		break;
	case '1':
		fimc_is_ois_gpio_on(sysfs_core->companion);
		msleep(150);
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}

static ssize_t camera_ois_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result_total = 0;
	bool result_offset = 0, result_selftest = 0;
	int selftest_ret = 0;
	long raw_data_x = 0, raw_data_y = 0;

	fimc_is_ois_offset_test(sysfs_core, &raw_data_x, &raw_data_y);
	msleep(50);
	selftest_ret = fimc_is_ois_self_test(sysfs_core);

	if (selftest_ret == 0x0) {
		result_selftest = true;
	} else {
		result_selftest = false;
	}

	if (abs(raw_data_x) > 35000 || abs(raw_data_y) > 35000)  {
		result_offset = false;
	} else {
		result_offset = true;
	}

	if (result_offset && result_selftest) {
		result_total = 0;
	} else if (!result_offset && !result_selftest) {
		result_total = 3;
	} else if (!result_offset) {
		result_total = 1;
	} else if (!result_selftest) {
		result_total = 2;
	}

	if (raw_data_x < 0 && raw_data_y < 0) {
		return sprintf(buf, "%d,-%ld.%03ld,-%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else if (raw_data_x < 0) {
		return sprintf(buf, "%d,-%ld.%03ld,%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
			raw_data_y /1000, raw_data_y % 1000);
	} else if (raw_data_y < 0) {
		return sprintf(buf, "%d,%ld.%03ld,-%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else {
		return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

static ssize_t camera_ois_rawdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	long raw_data_x = 0, raw_data_y = 0;

	fimc_is_ois_get_offset_data(sysfs_core, &raw_data_x, &raw_data_y);

	if (raw_data_x < 0 && raw_data_y < 0) {
		return sprintf(buf, "-%ld.%03ld,-%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else if (raw_data_x < 0) {
		return sprintf(buf, "-%ld.%03ld,%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
			raw_data_y /1000, raw_data_y % 1000);
	} else if (raw_data_y < 0) {
		return sprintf(buf, "%ld.%03ld,-%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else {
		return sprintf(buf, "%ld.%03ld,%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

static ssize_t camera_ois_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;
	u8 checksum = 0, caldata = 0;

	if (!sysfs_core->running_rear_camera) {
		fimc_is_ois_gpio_on(sysfs_core->companion);
		msleep(150);
		if (!sysfs_core->ois_ver_read) {
			fimc_is_ois_check_fw(sysfs_core);
		}

		fimc_is_ois_fw_status(sysfs_core, &checksum, &caldata);

		if (!sysfs_core->running_rear_camera) {
			fimc_is_ois_gpio_off(sysfs_core->companion);
		}
	}

	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);

	if (checksum != 0x00) {
		return sprintf(buf, "%s %s\n", "NG_FW2", "NULL");
	} else if (caldata != 0x00) {
		return sprintf(buf, "%s %s\n", "NG_CD2", ois_pinfo->header_ver);
	} else {
		return sprintf(buf, "%s %s\n", ois_minfo->header_ver, ois_pinfo->header_ver);
	}
}

static ssize_t camera_ois_diff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result = 0;
	int x_diff = 0, y_diff = 0;

	result = fimc_is_ois_diff_test(sysfs_core, &x_diff, &y_diff);

	return sprintf(buf, "%d,%d,%d\n", result == true ? 0 : 1, x_diff, y_diff);
}

static ssize_t camera_ois_fw_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	fimc_is_ois_init_thread(sysfs_core);

	return sprintf(buf, "%s\n", "Ois update done.");
}

static ssize_t camera_ois_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;
	struct fimc_is_ois_info *ois_uinfo = NULL;
	struct fimc_is_ois_exif *ois_exif = NULL;

	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);
	fimc_is_ois_get_user_version(&ois_uinfo);
	fimc_is_ois_get_exif_data(&ois_exif);

	return sprintf(buf, "%s %s %s %d %d", ois_minfo->header_ver, ois_pinfo->header_ver,
		ois_uinfo->header_ver, ois_exif->error_data, ois_exif->status_data);
}

static DEVICE_ATTR(rear_camtype, S_IRUGO,
		camera_rear_camtype_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO,
		camera_rear_camfw_show, camera_rear_camfw_write);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO,
		camera_rear_camfw_full_show, NULL);
static DEVICE_ATTR(rear_companionfw, S_IRUGO,
		camera_rear_companionfw_show, NULL);
static DEVICE_ATTR(rear_companionfw_full, S_IRUGO,
		camera_rear_companionfw_full_show, NULL);
static DEVICE_ATTR(rear_calcheck, S_IRUGO,
		camera_rear_calcheck_show, NULL);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO,
		camera_rear_checkfw_user_show, NULL);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO,
		camera_rear_checkfw_factory_show, NULL);
static DEVICE_ATTR(isp_core, S_IRUGO,
		camera_isp_core_show, NULL);
static DEVICE_ATTR(selftest, S_IRUGO,
		camera_ois_selftest_show, NULL);
static DEVICE_ATTR(ois_power, S_IWUSR,
		NULL, camera_ois_power_store);
static DEVICE_ATTR(ois_rawdata, S_IRUGO,
		camera_ois_rawdata_show, NULL);
static DEVICE_ATTR(oisfw, S_IRUGO,
		camera_ois_version_show, NULL);
static DEVICE_ATTR(ois_diff, S_IRUGO,
		camera_ois_diff_show, NULL);
static DEVICE_ATTR(fw_update, S_IRUGO,
		camera_ois_fw_update_show, NULL);
static DEVICE_ATTR(ois_exif, S_IRUGO,
		camera_ois_exif_show, NULL);

static int fimc_is_suspend(struct device *dev)
{
	pr_debug("FIMC_IS Suspend\n");
	return 0;
}

static int fimc_is_resume(struct device *dev)
{
	pr_debug("FIMC_IS Resume\n");
	return 0;
}

static ssize_t show_clk_gate_mode(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.clk_gate_mode);
}

static ssize_t store_clk_gate_mode(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	return count;
}

static ssize_t show_en_clk_gate(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.en_clk_gate);
}

static ssize_t store_en_clk_gate(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	return count;
}

static ssize_t show_en_dvfs(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.en_dvfs);
}

static ssize_t store_en_dvfs(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(en_clk_gate, 0644, show_en_clk_gate, store_en_clk_gate);
static DEVICE_ATTR(clk_gate_mode, 0644, show_clk_gate_mode, store_clk_gate_mode);
static DEVICE_ATTR(en_dvfs, 0644, show_en_dvfs, store_en_dvfs);

static struct attribute *fimc_is_debug_entries[] = {
	&dev_attr_en_clk_gate.attr,
	&dev_attr_clk_gate_mode.attr,
	&dev_attr_en_dvfs.attr,
	NULL,
};
static struct attribute_group fimc_is_debug_attr_group = {
	.name	= "debug",
	.attrs	= fimc_is_debug_entries,
};

static int fimc_is_probe(struct platform_device *pdev)
{
	struct exynos_platform_fimc_is *pdata;
	struct resource *mem_res;
	struct resource *regs_res;
	struct fimc_is_core *core;
	int ret = -ENODEV;

	info("%s:start\n", __func__);

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		pdata = fimc_is_parse_dt(&pdev->dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	core = kzalloc(sizeof(struct fimc_is_core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	fimc_is_parse_children_dt(&pdev->dev, core);

	fimc_is_dev = &pdev->dev;
	dev_set_drvdata(fimc_is_dev, core);

	core->companion_spi_channel = pdata->companion_spi_channel;
	core->use_two_spi_line = pdata->use_two_spi_line;
	core->use_sensor_dynamic_voltage_mode = pdata->use_sensor_dynamic_voltage_mode;
	core->use_ois = pdata->use_ois;
	core->use_ois_hsi2c = pdata->use_ois_hsi2c;
	core->use_module_check = pdata->use_module_check;

	core->pdev = pdev;
	core->pdata = pdata;
	core->id = pdev->id;
	core->debug_cnt = 0;
	core->running_rear_camera = false;
	core->running_front_camera = false;

	device_init_wakeup(&pdev->dev, true);

	/* TEMPORARY HACK */
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(31);

	/* init mutex for spi read */
	mutex_init(&core->spi_lock);

	/* for mideaserver force down */
	atomic_set(&core->rsccount, 0);
	clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		goto p_err1;
	}

	regs_res = request_mem_region(mem_res->start, resource_size(mem_res),
					pdev->name);
	if (!regs_res) {
		dev_err(&pdev->dev, "Failed to request io memory region\n");
		goto p_err1;
	}

	core->regs_res = regs_res;
	core->regs =  ioremap_nocache(mem_res->start, resource_size(mem_res));
	if (!core->regs) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		goto p_err2;
	}

	core->irq = platform_get_irq(pdev, 0);
	if (core->irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto p_err3;
	}

	ret = fimc_is_mem_probe(&core->mem, core->pdev);
	if (ret) {
		err("fimc_is_mem_probe failed(%d)", ret);
		goto p_err3;
	}

	fimc_is_interface_probe(&core->interface,
		core->regs,
		core->irq,
		core);

	fimc_is_resource_probe(&core->resourcemgr, core);

	/* group initialization */
	fimc_is_groupmgr_probe(&core->groupmgr);

#if defined(CONFIG_CAMERA_SENSOR_6D1_OBJ)
	ret = sensor_6d1_probe(NULL, NULL);
	if (ret) {
		err("sensor_6d1_probe failed(%d)", ret);
		goto p_err3;
	}
#endif

#if defined(CONFIG_CAMERA_SENSOR_IMX240_OBJ)
	ret = sensor_imx240_probe(NULL, NULL);
	if (ret) {
		err("sensor_imx175_probe failed(%d)", ret);
		goto p_err3;
	}
#endif

	/* device entity - ischain0 */
	fimc_is_ischain_probe(&core->ischain[0],
		&core->interface,
		&core->resourcemgr,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		0,
		core->regs);

	/* device entity - ischain1 */
	fimc_is_ischain_probe(&core->ischain[1],
		&core->interface,
		&core->resourcemgr,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		1,
		core->regs);

	/* device entity - ischain2 */
	fimc_is_ischain_probe(&core->ischain[2],
		&core->interface,
		&core->resourcemgr,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		2,
		core->regs);

	ret = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register fimc-is v4l2 device\n");
		goto p_err4;
	}

	/* video entity - 3a0 */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a0))
		fimc_is_3a0_video_probe(core);

	/* video entity - 3a0 capture */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a0))
		fimc_is_3a0c_video_probe(core);

	/* video entity - 3a1 */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a1))
		fimc_is_3a1_video_probe(core);

	/* video entity - 3a1 capture */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a1))
		fimc_is_3a1c_video_probe(core);

	/* video entity - isp */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, isp))
		fimc_is_isp_video_probe(core);

	/*front video entity - scalerC */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, scc))
		fimc_is_scc_video_probe(core);

	/* back video entity - scalerP*/
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, scp))
		fimc_is_scp_video_probe(core);

	if (GET_FIMC_IS_NUM_OF_SUBIP(core, dis)) {
		/* vdis video entity - vdis capture*/
		fimc_is_vdc_video_probe(core);
		/* vdis video entity - vdis output*/
		fimc_is_vdo_video_probe(core);
	}

	platform_set_drvdata(pdev, core);

	ret = fimc_is_ishcain_initmem(core);
	if (ret) {
		err("fimc_is_ishcain_initmem failed(%d)", ret);
		goto p_err4;
	}


#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_attach_iommu(core->mem.alloc_ctx);
#endif

	pm_runtime_enable(&pdev->dev);

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class)) {
			pr_err("Failed to create class(camera)!\n");
			ret = PTR_ERR(camera_class);
			goto p_err5;
		}
	}

	camera_front_dev = device_create(camera_class, NULL, 0, NULL, "front");
	if (IS_ERR(camera_front_dev)) {
		printk(KERN_ERR "failed to create front device!\n");
	} else {
		if (device_create_file(camera_front_dev,
				&dev_attr_front_sensorid) < 0) {
			printk(KERN_ERR "failed to create front device file, %s\n",
					dev_attr_front_sensorid.attr.name);
		}

		if (device_create_file(camera_front_dev,
					&dev_attr_front_camtype)
				< 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camtype.attr.name);
		}
		if (device_create_file(camera_front_dev,
					&dev_attr_front_camfw) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camfw.attr.name);
		}
	}
	camera_rear_dev = device_create(camera_class, NULL, 1, NULL, "rear");
	if (IS_ERR(camera_rear_dev)) {
		printk(KERN_ERR "failed to create rear device!\n");
	} else {
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_sensorid) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_sensorid.attr.name);
		}

		if (device_create_file(camera_rear_dev, &dev_attr_rear_camtype)
				< 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_camtype.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_camfw) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_camfw.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_camfw_full) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_camfw_full.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_checkfw_user) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_checkfw_user.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_checkfw_factory) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_checkfw_factory.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_companionfw) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_companionfw.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_companionfw_full) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_companionfw_full.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_calcheck) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_calcheck.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_isp_core) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_isp_core.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_fw_update) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_fw_update.attr.name);
		}
	}

	camera_ois_dev = device_create(camera_class, NULL, 2, NULL, "ois");
	if (IS_ERR(camera_ois_dev)) {
		printk(KERN_ERR "failed to create ois device!\n");
	} else {
		if (device_create_file(camera_ois_dev,
					&dev_attr_selftest) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_selftest.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_ois_power) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_ois_power.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_ois_rawdata) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_ois_rawdata.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_oisfw) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_oisfw.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_ois_diff) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_ois_diff.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_ois_exif) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_ois_exif.attr.name);
		}
	}

	sysfs_core = core;

	dbg("%s : fimc_is_front_%d probe success\n", __func__, pdev->id);

	/* set sysfs for debuging */
	sysfs_debug.en_clk_gate = 0;
	sysfs_debug.en_dvfs = 1;

	sysfs_debug.en_clk_gate = 1;
	sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_FW;

	ret = sysfs_create_group(&core->pdev->dev.kobj, &fimc_is_debug_attr_group);


	info("%s:end\n", __func__);
	return 0;

p_err5:
	__pm_runtime_disable(&pdev->dev, false);
p_err4:
	v4l2_device_unregister(&core->v4l2_dev);
p_err3:
	iounmap(core->regs);
p_err2:
	release_mem_region(regs_res->start, resource_size(regs_res));
p_err1:
	kfree(core);
	return ret;
}

static int fimc_is_remove(struct platform_device *pdev)
{
	dbg("%s\n", __func__);

	if (camera_front_dev) {
		device_remove_file(camera_front_dev, &dev_attr_front_sensorid);
		device_remove_file(camera_front_dev, &dev_attr_front_camtype);
		device_remove_file(camera_front_dev, &dev_attr_front_camfw);
	}

	if (camera_rear_dev) {
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensorid);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camtype);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw_full);
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_user);
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_factory);
		device_remove_file(camera_rear_dev, &dev_attr_rear_companionfw);
		device_remove_file(camera_rear_dev, &dev_attr_rear_companionfw_full);
		device_remove_file(camera_rear_dev, &dev_attr_rear_calcheck);
		device_remove_file(camera_rear_dev, &dev_attr_isp_core);
		device_remove_file(camera_ois_dev, &dev_attr_fw_update);
	}

	if (camera_ois_dev) {
		device_remove_file(camera_ois_dev, &dev_attr_selftest);
		device_remove_file(camera_ois_dev, &dev_attr_ois_power);
		device_remove_file(camera_ois_dev, &dev_attr_ois_rawdata);
		device_remove_file(camera_ois_dev, &dev_attr_oisfw);
		device_remove_file(camera_ois_dev, &dev_attr_ois_diff);
		device_remove_file(camera_ois_dev, &dev_attr_ois_exif);
	}

	if (camera_class) {
		if (camera_front_dev)
			device_destroy(camera_class, camera_front_dev->devt);

		if (camera_rear_dev)
			device_destroy(camera_class, camera_rear_dev->devt);

		if (camera_ois_dev)
			device_destroy(camera_class, camera_ois_dev->devt);
	}

	class_destroy(camera_class);

	return 0;
}

static const struct dev_pm_ops fimc_is_pm_ops = {
	.suspend		= fimc_is_suspend,
	.resume			= fimc_is_resume,
	.runtime_suspend	= fimc_is_runtime_suspend,
	.runtime_resume		= fimc_is_runtime_resume,
};

static int of_fimc_is_spi_dt(struct device *dev, struct fimc_is_spi_gpio *spi_gpio, struct fimc_is_core *core)
{
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL,NULL,"samsung,fimc_is_spi1");
	if(np == NULL) {
		pr_err("compatible: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	ret = of_property_read_string(np, "fimc_is_spi_sclk", (const char **) &spi_gpio->spi_sclk);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	ret = of_property_read_string(np, "fimc_is_spi_ssn",(const char **) &spi_gpio->spi_ssn);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	ret = of_property_read_string(np, "fimc_is_spi_miso",(const char **) &spi_gpio->spi_miso);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	ret = of_property_read_string(np, "fimc_is_spi_mois",(const char **) &spi_gpio->spi_mois);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	pr_info("sclk = %s, ssn = %s, miso = %s, mois = %s spi_channel:(%d)\n", spi_gpio->spi_sclk, spi_gpio->spi_ssn, spi_gpio->spi_miso, spi_gpio->spi_mois,core->companion_spi_channel);

	return 0;
}

static int fimc_is_spi_probe(struct spi_device *spi)
{
	struct fimc_is_core *core;
	int ret = 0;

	BUG_ON(!fimc_is_dev);

	dbg_core("%s\n", __func__);

	core = dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}
	spi->mode = SPI_MODE_0;

	/* spi->bits_per_word = 16; */
	if (spi_setup(spi)) {
		pr_err("failed to setup spi for fimc_is_spi\n");
		return -EINVAL;
	}

	if (!strncmp(spi->modalias, "fimc_is_spi0", 12))
		core->spi0 = spi;

	if (!strncmp(spi->modalias, "fimc_is_spi1", 12)) {
		core->spi1 = spi;
		ret = of_fimc_is_spi_dt(&spi->dev,&core->spi_gpio, core);
		if (ret)
			pr_err("[%s] of_fimc_is_spi_dt parse dt failed\n", __func__);
	}

	return ret;
}

static int fimc_is_spi_remove(struct spi_device *spi)
{
	return 0;
}

static int fimc_is_fan53555_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_core *core;
	int ret = 0;
	struct device_node *np;
	int gpio_comp_en;

	BUG_ON(!fimc_is_dev);

	pr_info("%s start\n",__func__);

	core = dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	np = of_find_compatible_node(NULL, NULL, "samsung,fimc_is_fan53555");
	if(np == NULL) {
		pr_err("compatible: fail to read, fan_parse_dt\n");
		return -ENODEV;
	}

	gpio_comp_en = of_get_named_gpio(np, "comp_en", 0);
	if (!gpio_is_valid(gpio_comp_en))
		pr_err("failed to get comp en gpio\n");

	ret = gpio_request(gpio_comp_en,"COMP_EN");
	if (ret < 0 )
		pr_err("gpio_request_error(%d)\n",ret);

	gpio_direction_output(gpio_comp_en,1);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: SMBUS Byte Data not Supported\n", __func__);
		ret = -EIO;
		goto err;
	}

	core->companion_dcdc.client = client;
	core->companion_dcdc.type = DCDC_VENDOR_FAN53555;
	core->companion_dcdc.get_vout_val = fan53555_get_vout_val;
	core->companion_dcdc.get_vout_str = fan53555_get_vout_str;
	core->companion_dcdc.set_vout = fan53555_set_vsel0_vout;


	ret = i2c_smbus_write_byte_data(client, REG_VSEL0, VSEL0_INIT_VAL);
	if (ret < 0) {
		pr_err("%s: write error = %d , try again\n", __func__, ret);
		ret = i2c_smbus_write_byte_data(client, REG_VSEL0, VSEL0_INIT_VAL);
		if (ret < 0)
			pr_err("%s: write 2nd error = %d\n", __func__, ret);
	}

	ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
	if (ret < 0) {
		pr_err("%s: read error = %d , try again\n", __func__, ret);
		ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
		if (ret < 0)
			pr_err("%s: read 2nd error = %d\n", __func__, ret);
	}
	pr_err("[%s::%d]fan53555 [Read :: %x ,%x]\n\n", __func__, __LINE__, ret,VSEL0_INIT_VAL);

	gpio_direction_output(gpio_comp_en,0);
	gpio_free(gpio_comp_en);

	pr_info(" %s end\n",__func__);

	return 0;

err:
	gpio_direction_output(gpio_comp_en, 0);
	gpio_free(gpio_comp_en);

        return ret;
}

static int fimc_is_fan53555_remove(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id exynos_fimc_is_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is",
	},
	{
		.compatible = "samsung,fimc_is_spi0",
	},
	{
		.compatible = "samsung,fimc_is_spi1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_match);

static struct spi_driver fimc_is_spi0_driver = {
	.driver = {
		.name = "fimc_is_spi0",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = exynos_fimc_is_match,
	},
	.probe	= fimc_is_spi_probe,
	.remove	= fimc_is_spi_remove,
};
module_spi_driver(fimc_is_spi0_driver);

static struct of_device_id fan53555_dt_ids[] = {
        { .compatible = "samsung,fimc_is_fan53555",},
        {},
};
MODULE_DEVICE_TABLE(of, fan53555_dt_ids);

static const struct i2c_device_id fan53555_id[] = {
        {"fimc_is_fan53555", 0},
        {}
};
MODULE_DEVICE_TABLE(i2c, fan53555_id);

static struct i2c_driver fan53555_driver = {
        .driver = {
                .name = "fimc_is_fan53555",
                .owner  = THIS_MODULE,
                .of_match_table = fan53555_dt_ids,
        },
        .probe = fimc_is_fan53555_probe,
        .remove = fimc_is_fan53555_remove,
        .id_table = fan53555_id,
};
module_i2c_driver(fan53555_driver);

static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove		= fimc_is_remove,
	.driver = {
		.name	= FIMC_IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
		.of_match_table = exynos_fimc_is_match,
	}
};

module_platform_driver(fimc_is_driver);

MODULE_AUTHOR("Jiyoung Shin<idon.shin@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS2 driver");
MODULE_LICENSE("GPL");
