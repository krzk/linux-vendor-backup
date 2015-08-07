/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <mach/devfreq.h>
#include <mach/bts.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/exynos5-mipiphy.h>

#include "fimc-is-config.h"
#include "fimc-is-type.h"
#include "fimc-is-regs.h"
#include "fimc-is-core.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define PM_QOS_CAM_THROUGHPUT	PM_QOS_RESERVED
#endif
extern struct pm_qos_request exynos_isp_qos_cpu_min;
extern struct pm_qos_request exynos_isp_qos_cpu_max;
extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;
extern struct pm_qos_request exynos_isp_qos_disp;
extern struct pm_qos_request max_cpu_qos;

int fimc_is_runtime_suspend_post(struct device *dev)
{
	int ret = 0;
	u32 timeout;

	timeout = 2000;
	while ((readl(PMUREG_ISP_STATUS) & 0x1) && timeout) {
		timeout--;
		usleep_range(1000, 1000);
	}
	if (timeout == 0)
		err("ISP power down failed(0x%08x)\n",
			readl(PMUREG_ISP_STATUS));

	timeout = 1000;
	while ((readl(PMUREG_CAM0_STATUS) & 0x1) && timeout) {
		timeout--;
		usleep_range(1000, 1000);
	}
	if (timeout == 0)
		err("CAM0 power down failed(0x%08x)\n",
			readl(PMUREG_CAM0_STATUS));

	timeout = 2000;
	while ((readl(PMUREG_CAM1_STATUS) & 0x1) && timeout) {
		timeout--;
		usleep_range(1000, 1000);
	}
	if (timeout == 0)
		err("CAM1 power down failed(CAM1:0x%08x, A5:0x%08x)\n",
			readl(PMUREG_CAM1_STATUS), readl(PMUREG_ISP_ARM_STATUS));


	return ret;
}

int fimc_is_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(pdev);

	BUG_ON(!core);
	BUG_ON(!core->pdata);
	BUG_ON(!core->pdata->clk_off);

	pr_info("FIMC_IS runtime suspend in\n");

	/* EGL Release */
	pm_qos_update_request(&max_cpu_qos, PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);
	pm_qos_remove_request(&max_cpu_qos);


	if (CALL_POPS(core, clk_off, pdev) < 0)
		warn("clk_off is fail\n");

	pr_info("FIMC_IS runtime suspend out\n");

	pm_relax(dev);
	return 0;
}

int fimc_is_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(pdev);

	pm_stay_awake(dev);
	pr_info("FIMC_IS runtime resume in\n");

	/* EGL Lock */
	pm_qos_add_request(&max_cpu_qos, PM_QOS_CPU_FREQ_MAX, 1700000);

	/* HACK: DVFS lock sequence is change.
	 * DVFS level should be locked after power on.
	 */

	/* Low clock setting */
	if (CALL_POPS(core, clk_cfg, core->pdev) < 0) {
		err("clk_cfg is fail\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* Clock on */
	if (CALL_POPS(core, clk_on, core->pdev) < 0) {
		err("clk_on is fail\n");
		ret = -EINVAL;
		goto p_err;
	}


	pr_info("FIMC-IS runtime resume out\n");

	return 0;

p_err:
	pm_relax(dev);
	return ret;
}
