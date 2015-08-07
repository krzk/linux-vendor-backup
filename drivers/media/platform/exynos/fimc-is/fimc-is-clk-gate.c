/*
 * Samsung Exynos SoC series FIMC-IS driver
 *
 * exynos fimc-is core functions
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-clk-gate.h"

int fimc_is_clk_gate_init(struct fimc_is_core *core)
{
	struct fimc_is_clk_gate_ctrl *gate_ctrl;

	pr_info("%s\n",	__func__);

	if (!core) {
		err("core is NULL\n");
		return -EINVAL;
	}

	gate_ctrl = &core->resourcemgr.clk_gate_ctrl;
	memset(gate_ctrl, 0x0, sizeof(struct fimc_is_clk_gate_ctrl));

	/* init spin_lock for clock gating */
	spin_lock_init(&gate_ctrl->lock);

	/* ISSR53 is clock gating debugging region.
	 * High means clock on state.
	 * To prevent telling A5 wrong clock off state,
	 * clock on state should be set before clock off is set.
	 */
	writel(0xFFFFFFFF, core->ischain[0].interface->regs + ISSR53);

	return 0;
}

int fimc_is_clk_gate_lock_set(struct fimc_is_core *core, u32 instance, u32 is_start)
{
	spin_lock(&core->resourcemgr.clk_gate_ctrl.lock);
	core->resourcemgr.clk_gate_ctrl.msk_lock_by_ischain[instance] = is_start;
	spin_unlock(&core->resourcemgr.clk_gate_ctrl.lock);
	return 0;
}

#if 0
/* This function may be used when clk_enable api will be faster than now */
int fimc_is_clk_gate_reg_set(struct fimc_is_core *core,
		bool is_on, const char* gate_str, u32 clk_gate_id,
		struct exynos_fimc_is_clk_gate_info *gate_info)
{
	struct platform_device *pdev = core->pdev;
	if (is_on) {
		if (gate_info->clk_on(pdev, gate_str) < 0) {
			pr_err("%s: could not enable %s\n", __func__, gate_str);
			return -EINVAL;
		}
	} else {
		if (gate_info->clk_off(pdev, gate_str) < 0) {
			pr_err("%s: could not disable %s\n", __func__, gate_str);
			return -EINVAL;
		}
	}
	return 0;
}
#endif
inline bool fimc_is_group_otf(struct fimc_is_device_ischain *device, int group_id)
{
	struct fimc_is_group *group;

	switch (group_id) {
	case GROUP_ID_3A0:
	case GROUP_ID_3A1:
		group = &device->group_3aa;
		break;
	case GROUP_ID_ISP:
		group = &device->group_isp;
		break;
	case GROUP_ID_DIS:
		group = &device->group_dis;
		break;
	default:
		group = NULL;
		pr_err("%s unresolved group id %d", __func__,  group_id);
		return false;
	}

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		return true;
	else
		return false;
}

