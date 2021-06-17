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

#include <linux/sched.h>
#include <mach/exynos-fimc-is-sensor.h>
#include <mach/exynos-fimc-is.h>
#include <media/exynos_mc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "fimc-is-dt.h"

#ifdef CONFIG_OF
static int board_rev = 0;
static int get_board_rev(struct device *dev)
{
	int ret = 0;
#if 0 // LSI CODE
	int board_rev_pin0, board_rev_pin1;
	struct device_node *np = dev->of_node;

	board_rev_pin0 = of_get_named_gpio(np, "gpios_board_rev", 0);
	if (!gpio_is_valid(board_rev_pin0)) {
		dev_err(dev, "failed to get main board_rev_pin0\n");
		ret = -EINVAL;
		goto p_err;
	}

	board_rev_pin1 = of_get_named_gpio(np, "gpios_board_rev", 1);
	if (!gpio_is_valid(board_rev_pin1)) {
		dev_err(dev, "failed to get main board_rev_pin1\n");
		ret = -EINVAL;
		goto p_err;
	}

	gpio_request_one(board_rev_pin0, GPIOF_IN, "BOARD_REV_PIN0");
	gpio_request_one(board_rev_pin1, GPIOF_IN, "BOARD_REV_PIN1");
	board_rev = __gpio_get_value(board_rev_pin0) << 0;
	board_rev |= __gpio_get_value(board_rev_pin1) << 1;

p_err:
#endif
	return ret;
}

static int parse_gate_info(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	int ret = 0;
	struct device_node *group_np = NULL;
	struct device_node *gate_info_np;
	struct property *prop;
	struct property *prop2;
	const __be32 *p;
	const char *s;
	u32 i = 0, u = 0;
	struct exynos_fimc_is_clk_gate_info *gate_info;

	/* get subip of fimc-is info */
	gate_info = kzalloc(sizeof(struct exynos_fimc_is_clk_gate_info), GFP_KERNEL);
	if (!gate_info) {
		printk(KERN_ERR "%s: no memory for fimc_is gate_info\n", __func__);
		return -EINVAL;
	}

	s = NULL;
	/* get gate register info */
	prop2 = of_find_property(np, "clk_gate_strs", NULL);
	of_property_for_each_u32(np, "clk_gate_enums", prop, p, u) {
		printk(KERN_INFO "int value: %d\n", u);
		s = of_prop_next_string(prop2, s);
		if (s != NULL) {
			printk(KERN_INFO "String value: %d-%s\n", u, s);
			gate_info->gate_str[u] = s;
		}
	}

	/* gate info */
	gate_info_np = of_find_node_by_name(np, "clk_gate_ctrl");
	if (!gate_info_np) {
		printk(KERN_ERR "%s: can't find fimc_is clk_gate_ctrl node\n", __func__);
		ret = -ENOENT;
		goto p_err;
	}
	i = 0;
	while ((group_np = of_get_next_child(gate_info_np, group_np))) {
		struct exynos_fimc_is_clk_gate_group *group =
				&gate_info->groups[i];
		of_property_for_each_u32(group_np, "mask_clk_on_org", prop, p, u) {
			printk(KERN_INFO "(%d) int1 value: %d\n", i, u);
			group->mask_clk_on_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_self_org", prop, p, u) {
			printk(KERN_INFO "(%d) int2 value: %d\n", i, u);
			group->mask_clk_off_self_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int3 value: %d\n", i, u);
			group->mask_clk_off_depend |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_cond_for_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int4 value: %d\n", i, u);
			group->mask_cond_for_depend |= (1 << u);
		}
		i++;
		printk(KERN_INFO "(%d) [0x%x , 0x%x, 0x%x, 0x%x\n", i,
			group->mask_clk_on_org,
			group->mask_clk_off_self_org,
			group->mask_clk_off_depend,
			group->mask_cond_for_depend
		);
	}

	pdata->gate_info = gate_info;
	pdata->gate_info->user_clk_gate = exynos_fimc_is_set_user_clk_gate;
	pdata->gate_info->clk_on_off = exynos_fimc_is_clk_gate;

	return 0;
p_err:
	kfree(gate_info);
	return ret;
}

static int parse_dvfs_data(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(np, "default_int", pdata->dvfs_data[FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "default_cam", pdata->dvfs_data[FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "default_mif", pdata->dvfs_data[FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "default_i2c", pdata->dvfs_data[FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_preview_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_preview_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_preview_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_preview_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_capture_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_capture_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_capture_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_capture_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_camcording_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_camcording_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_camcording_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_camcording_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_vt1_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_vt1_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_vt1_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_vt1_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_vt2_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_vt2_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_vt2_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_vt2_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_fhd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_fhd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_fhd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_fhd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_whd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_whd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_whd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_whd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_uhd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_uhd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_uhd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_uhd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_capture_int", pdata->dvfs_data[FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_capture_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_capture_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_capture_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_camcording_fhd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_camcording_fhd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_camcording_fhd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_camcording_fhd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_camcording_uhd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_camcording_uhd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_camcording_uhd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_camcording_uhd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_preview_int", pdata->dvfs_data[FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_preview_cam", pdata->dvfs_data[FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_preview_mif", pdata->dvfs_data[FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_preview_i2c", pdata->dvfs_data[FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_capture_int", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_capture_cam", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_capture_mif", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_capture_i2c", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_camcording_int", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_camcording_cam", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_camcording_mif", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_camcording_i2c", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "high_speed_fps_int", pdata->dvfs_data[FIMC_IS_SN_HIGH_SPEED_FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "high_speed_fps_cam", pdata->dvfs_data[FIMC_IS_SN_HIGH_SPEED_FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "high_speed_fps_mif", pdata->dvfs_data[FIMC_IS_SN_HIGH_SPEED_FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "high_speed_fps_i2c", pdata->dvfs_data[FIMC_IS_SN_HIGH_SPEED_FPS][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dis_enable_int", pdata->dvfs_data[FIMC_IS_SN_DIS_ENABLE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dis_enable_cam", pdata->dvfs_data[FIMC_IS_SN_DIS_ENABLE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dis_enable_mif", pdata->dvfs_data[FIMC_IS_SN_DIS_ENABLE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dis_enable_i2c", pdata->dvfs_data[FIMC_IS_SN_DIS_ENABLE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "max_int", pdata->dvfs_data[FIMC_IS_SN_MAX][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "max_cam", pdata->dvfs_data[FIMC_IS_SN_MAX][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "max_mif", pdata->dvfs_data[FIMC_IS_SN_MAX][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "max_i2c", pdata->dvfs_data[FIMC_IS_SN_MAX][FIMC_IS_DVFS_I2C]);

	return 0;
}

static int parse_subip_info(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	u32 temp;
	char *pprop;
	struct exynos_fimc_is_subip_info *subip_info;

	/* get subip of fimc-is info */
	subip_info = kzalloc(sizeof(struct exynos_fimc_is_subip_info), GFP_KERNEL);
	if (!subip_info) {
		printk(KERN_ERR "%s: no memory for fimc_is subip_info\n", __func__);
		return -EINVAL;
	}

	DT_READ_U32(np, "num_of_mcuctl", subip_info->_mcuctl.valid);
	DT_READ_U32(np, "num_of_3a0", subip_info->_3a0.valid);
	DT_READ_U32(np, "num_of_3a1", subip_info->_3a1.valid);
	DT_READ_U32(np, "num_of_isp", subip_info->_isp.valid);
	DT_READ_U32(np, "num_of_drc", subip_info->_drc.valid);
	DT_READ_U32(np, "num_of_scc", subip_info->_scc.valid);
	DT_READ_U32(np, "num_of_odc", subip_info->_odc.valid);
	DT_READ_U32(np, "num_of_dis", subip_info->_dis.valid);
	DT_READ_U32(np, "num_of_dnr", subip_info->_dnr.valid);
	DT_READ_U32(np, "num_of_scp", subip_info->_scp.valid);
	DT_READ_U32(np, "num_of_fd",  subip_info->_fd.valid);

	DT_READ_U32(np, "full_bypass_mcuctl", subip_info->_mcuctl.full_bypass);
	DT_READ_U32(np, "full_bypass_3a0", subip_info->_3a0.full_bypass);
	DT_READ_U32(np, "full_bypass_3a1", subip_info->_3a1.full_bypass);
	DT_READ_U32(np, "full_bypass_isp", subip_info->_isp.full_bypass);
	DT_READ_U32(np, "full_bypass_drc", subip_info->_drc.full_bypass);
	DT_READ_U32(np, "full_bypass_scc", subip_info->_scc.full_bypass);
	DT_READ_U32(np, "full_bypass_odc", subip_info->_odc.full_bypass);
	DT_READ_U32(np, "full_bypass_dis", subip_info->_dis.full_bypass);
	DT_READ_U32(np, "full_bypass_dnr", subip_info->_dnr.full_bypass);
	DT_READ_U32(np, "full_bypass_scp", subip_info->_scp.full_bypass);
	DT_READ_U32(np, "full_bypass_fd",  subip_info->_fd.full_bypass);

	DT_READ_U32(np, "version_mcuctl", subip_info->_mcuctl.version);
	DT_READ_U32(np, "version_3a0", subip_info->_3a0.version);
	DT_READ_U32(np, "version_3a1", subip_info->_3a1.version);
	DT_READ_U32(np, "version_isp", subip_info->_isp.version);
	DT_READ_U32(np, "version_drc", subip_info->_drc.version);
	DT_READ_U32(np, "version_scc", subip_info->_scc.version);
	DT_READ_U32(np, "version_odc", subip_info->_odc.version);
	DT_READ_U32(np, "version_dis", subip_info->_dis.version);
	DT_READ_U32(np, "version_dnr", subip_info->_dnr.version);
	DT_READ_U32(np, "version_scp", subip_info->_scp.version);
	DT_READ_U32(np, "version_fd",  subip_info->_fd.version);

	pdata->subip_info = subip_info;

	return 0;
}

struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	void *ret = NULL;
	struct exynos_platform_fimc_is *pdata;
	struct device_node *subip_info_np;
	struct device_node *dvfs_np;
	struct device_node *np = dev->of_node;
#ifdef CONFIG_COMPANION_USE
	int retVal = 0;
#endif

	if (!np)
		return ERR_PTR(-ENOENT);

	if (get_board_rev(dev) < 0)
		pr_warn("%s: Failed to get_board_rev\n", __func__);

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	pdata->clk_cfg = exynos_fimc_is_cfg_clk;
	pdata->clk_on = exynos_fimc_is_clk_on;
	pdata->clk_off = exynos_fimc_is_clk_off;
	pdata->print_clk = exynos_fimc_is_print_clk;
	pdata->print_cfg = exynos_fimc_is_print_cfg;
	pdata->print_pwr = exynos_fimc_is_print_pwr;

	dev->platform_data = pdata;

#ifdef CONFIG_COMPANION_USE
	retVal = of_property_read_u32(np,"companion_spi_channel",&pdata->companion_spi_channel);
	if (retVal) {
		err("spi_channel read is fail(%d)", retVal);
	}

	pdata->use_two_spi_line = of_property_read_bool(np,"use_two_spi_line");
#endif
	retVal = of_property_read_u32(np,"use_vision",&pdata->use_vision);
	if (retVal) {
		err("use_vision read is fail(%d)", retVal);
		pdata->use_vision = 0;
	}

	subip_info_np = of_find_node_by_name(np, "subip_info");
	if (!subip_info_np) {
		printk(KERN_ERR "%s: can't find fimc_is subip_info node\n", __func__);
		ret = ERR_PTR(-ENOENT);
		goto p_err;
	}
	parse_subip_info(pdata, subip_info_np);

	if (parse_gate_info(pdata, np) < 0)
		printk(KERN_ERR "%s: can't parse clock gate info node\n", __func__);

	dvfs_np = of_find_node_by_name(np, "fimc_is_dvfs");
	if (!dvfs_np) {
		printk(KERN_ERR "%s: can't find fimc_is_dvfs node\n", __func__);
		ret = ERR_PTR(-ENOENT);
		goto p_err;
	}
	parse_dvfs_data(pdata, dvfs_np);

	return pdata;
p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_sensor_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	u32 temp;
	char *pprop;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;
	int gpio_reset = 0, gpio_standby = 0;
#ifdef CONFIG_SOC_EXYNOS5422
	int gpios_cam_en = 0;
#endif
#if 0 /* LSI Patch */
	int gpio_cam_en = 0;
#endif
	int gpio_none = 0;
	const char *name;
	u32 id;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	if (get_board_rev(dev) < 0)
		pr_warn("%s: Failed to get_board_rev\n", __func__);

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_sensor_pins_cfg;
	pdata->iclk_cfg = exynos_fimc_is_sensor_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_sensor_iclk_on;
	pdata->iclk_off = exynos_fimc_is_sensor_iclk_off;
	pdata->mclk_on = exynos_fimc_is_sensor_mclk_on;
	pdata->mclk_off = exynos_fimc_is_sensor_mclk_off;

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "csi_ch", &pdata->csi_ch);
	if (ret) {
		err("csi_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "flite_ch", &pdata->flite_ch);
	if (ret) {
		err("flite_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "i2c_ch", &pdata->i2c_ch);
	if (ret) {
		err("i2c_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "i2c_addr", &pdata->i2c_addr);
	if (ret) {
		err("i2c_addr read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "is_bns", &pdata->is_bns);
	if (ret) {
		err("is_bns read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "id", &id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	DT_READ_U32(dnode, "flash_first_gpio",   pdata->flash_first_gpio );
	DT_READ_U32(dnode, "flash_second_gpio",  pdata->flash_second_gpio);

	ret = of_property_read_string(dnode, "sensor_name", &name);
	if (ret) {
		err("sensor_name read is fail(%d)", ret);
		goto p_err;
	}
	strcpy(pdata->sensor_name, name);

	ret = of_property_read_u32(dnode, "sensor_id", &pdata->sensor_id);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		ret = -EINVAL;
		goto p_err;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}
#ifdef CONFIG_SOC_EXYNOS5422
	gpios_cam_en = of_get_named_gpio(dnode, "gpios_cam_en", 0);
	if (!gpio_is_valid(gpios_cam_en)) {
		dev_err(dev, "failed to get main/front cam en gpio\n");
	} else {
		gpio_request_one(gpios_cam_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpios_cam_en);
	}
#endif

#if 0 /* LSI Patch */
	/* Optional Feature */
	gpio_comp_en = of_get_named_gpio(dnode, "gpios_comp_en", 0);
	if (!gpio_is_valid(gpio_comp_en))
	dev_err(dev, "failed to get main comp en gpio\n");

	gpio_comp_rst = of_get_named_gpio(dnode, "gpios_comp_reset", 0);
	if (!gpio_is_valid(gpio_comp_rst))
	dev_err(dev, "failed to get main comp reset gpio\n");

	gpio_standby = of_get_named_gpio(dnode, "gpio_standby", 0);
	if (!gpio_is_valid(gpio_standby))
		dev_err(dev, "failed to get gpio_standby\n");

	gpio_cam_en = of_get_named_gpio(dnode, "gpios_cam_en", 0);
	if (!gpio_is_valid(gpio_cam_en))
		dev_err(dev, "failed to get gpio_cam_en\n");

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_cam_en, 0, NULL, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_reset, 0, NULL, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "ch", PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpio_comp_en, 0, NULL, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_comp_rst, 0, NULL, PIN_RESET);
#if defined(CONFIG_SOC_EXYNOS5430)
	if ((id == SENSOR_POSITION_REAR) && (board_rev == 2)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_none, 0, "af", PIN_FUNCTION);
	}
#else
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_none, 0, "af", PIN_FUNCTION);
#endif
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, PIN_END);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_reset, 0, NULL, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_reset, 0, NULL, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_comp_rst, 0, NULL, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_comp_rst, 0, NULL, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_comp_en, 0, NULL, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_cam_en, 0, NULL, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, PIN_END);

	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0, gpio_cam_en, 0, NULL, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1, gpio_reset, 0, NULL, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2, gpio_standby, 0, NULL, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 3, gpio_none, 0, NULL, PIN_END);

	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0, gpio_reset, 0, NULL, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1, gpio_reset, 0, NULL, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2, gpio_standby, 0, NULL, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 4, gpio_cam_en, 0, NULL, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3, gpio_none, 0, NULL, PIN_END);

#else /* TN CODE */

	if (id == SENSOR_POSITION_FRONT) {
		gpio_standby = of_get_named_gpio(dnode, "gpio_standby", 0);
		if (!gpio_is_valid(gpio_standby)) {
			dev_err(dev, "failed to get gpio_standby\n");
		} else {
			gpio_request_one(gpio_standby, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
			gpio_free(gpio_standby);
		}
	}

#ifdef CONFIG_SOC_EXYNOS5422
	if (id == SENSOR_POSITION_REAR) {
		/* BACK CAMERA	- POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_none, 0, NULL, 0, PIN_END);

		/* BACK CAMERA	- POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, NULL, 0, PIN_END);
	} else {
		/* FRONT CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpios_cam_en, 0, NULL, 1000, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_none, 0, "ch", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* FRONT CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* VISION CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 3, gpios_cam_en, 0, NULL, 1000, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 4, gpio_standby, 0, NULL, 0, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 5, gpio_none, 0, "ch", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* VISION CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, 0, PIN_END);
	}
#else /*CONFIG_SOC_EXYNOS5430*/
	if (id == SENSOR_POSITION_REAR) {
		/* BACK CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_none, 0, NULL, 0, PIN_END);

		/* BACK CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, NULL, 0, PIN_END);
	} else {
		/* FRONT CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpio_none, 0, "VT_CAM_1.2V", 1000, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_none, 0, "ch", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* FRONT CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_none, 0, "VT_CAM_1.2V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* VISION CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 3, gpio_none, 0, "VT_CAM_1.2V", 1000, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 4, gpio_standby, 0, NULL, 0, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 5, gpio_none, 0, "ch", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* VISION CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3, gpio_none, 0, "VT_CAM_1.2V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, 0, PIN_END);
	}
#endif
#endif
	/* Xyref5430 board revision config */
	if ((id == SENSOR_POSITION_FRONT) && board_rev) {
		pdata->mclk_ch = 2;
		pdata->csi_ch = 2;
		pdata->flite_ch = 2;
		pdata->i2c_ch = 2;
	}

	if ((id == SENSOR_POSITION_REAR) && (board_rev == 2)) {
		pdata->i2c_ch = 0x0100;
		pdata->i2c_addr = 0x5A5A;
	}

	pdev->id = id;

	dev->platform_data = pdata;

	return ret;
p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_companion_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;
	int gpio_comp_en = 0, gpio_comp_rst = 0;
	int gpio_none = 0;
	int gpio_reset = 0;
#ifdef CONFIG_SOC_EXYNOS5422
	int gpios_cam_en = 0;
#endif

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_sensor_pins_cfg;
	pdata->iclk_cfg = exynos_fimc_is_companion_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_companion_iclk_on;
	pdata->iclk_off = exynos_fimc_is_companion_iclk_off;
	pdata->mclk_on = exynos_fimc_is_companion_mclk_on;
	pdata->mclk_off = exynos_fimc_is_companion_mclk_off;

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	gpio_comp_en = of_get_named_gpio(dnode, "gpios_comp_en", 0);
	if (!gpio_is_valid(gpio_comp_en)) {
		dev_err(dev, "failed to get main comp en gpio\n");
	} else {
		gpio_request_one(gpio_comp_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_comp_en);
	}

	gpio_comp_rst = of_get_named_gpio(dnode, "gpios_comp_reset", 0);
	if (!gpio_is_valid(gpio_comp_rst)) {
		dev_err(dev, "failed to get main comp reset gpio\n");
	} else {
		gpio_request_one(gpio_comp_rst, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_comp_rst);
	}

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		ret = -EINVAL;
		goto p_err;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}
#ifdef CONFIG_SOC_EXYNOS5422
	gpios_cam_en = of_get_named_gpio(dnode, "gpios_cam_en", 0);
	if (!gpio_is_valid(gpios_cam_en)) {
		dev_err(dev, "failed to get main/front cam en gpio\n");
	} else {
		gpio_request_one(gpios_cam_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpios_cam_en);
	}
#endif

#ifdef CONFIG_SOC_EXYNOS5422
	/* COMPANION - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_comp_en, 0, NULL, 150, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7, gpio_none, 0, "ch", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9, gpio_none, 0, "af", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 10, gpio_none, 0, NULL, 0, PIN_END);


	/* COMPANION - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_comp_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 8, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 9, gpio_none, 0, NULL, 0, PIN_END);
#else /*CONFIG_SOC_EXYNOS5430*/
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_none, 0, "CAM_SEN_A2.8V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_comp_en, 0, NULL, 150, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7, gpio_none, 0, "ch", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9, gpio_none, 0, NULL, 0, PIN_END);

	/* BACK CAMERA  - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_comp_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, "CAM_SEN_A2.8V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 8, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 9, gpio_none, 0, NULL, 0, PIN_END);
#endif

	dev->platform_data = pdata;

	return ret;
p_err:
	kfree(pdata);
	return ret;
}
#else
struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
