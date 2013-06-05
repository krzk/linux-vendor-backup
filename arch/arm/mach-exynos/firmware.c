/*
 * Copyright (C) 2012 Samsung Electronics.
 * Kyungmin Park <kyungmin.park@samsung.com>
 * Tomasz Figa <t.figa@samsung.com>
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <asm/firmware.h>
#include <asm/hardware/cache-l2x0.h>

#include <mach/map.h>

#include "smc.h"

#define EXYNOS_SLEEP_MAGIC	0x00000BAD

static int exynos_do_idle(void)
{
	exynos_smc(SMC_CMD_SLEEP, 0, 0, 0);
	return 0;
}

static int exynos_cpu_boot(int cpu)
{
	exynos_smc(SMC_CMD_CPU1BOOT, cpu, 0, 0);
	return 0;
}

static int exynos_set_cpu_boot_addr(int cpu, unsigned long boot_addr)
{
	void __iomem *boot_reg = S5P_VA_SYSRAM_NS + 0x1c + 4*cpu;

	__raw_writel(boot_addr, boot_reg);
	return 0;
}

static int exynos_l2x0_init(void)
{
	exynos_smc(SMC_CMD_L2X0INVALL, 0, 0, 0);
	exynos_smc(SMC_CMD_L2X0CTRL, 1, 0, 0);
	return 0;
}

static int exynos_l2x0_resume(void)
{
	exynos_smc(SMC_CMD_L2X0SETUP1, l2x0_saved_regs.tag_latency,
		   l2x0_saved_regs.data_latency, l2x0_saved_regs.prefetch_ctrl);
	exynos_smc(SMC_CMD_L2X0SETUP2, 0x3, 0x7C470001, 0xC200FFFF);
	exynos_smc(SMC_CMD_L2X0INVALL, 0, 0, 0);
	exynos_smc(SMC_CMD_L2X0CTRL, 1, 0, 0);

	return 0;
}

static int exynos_suspend(unsigned long resume_addr)
{
	writel(EXYNOS_SLEEP_MAGIC, S5P_VA_SYSRAM_NS + 0xC);
	writel(resume_addr, S5P_VA_SYSRAM_NS + 0x8);
	exynos_smc(SMC_CMD_SLEEP, 0, 0, 0);

	return 0;
}

static int exynos_resume(void)
{
	writel(0, S5P_VA_SYSRAM_NS + 0xC);

	return 0;
}

static int exynos_c15resume(u32 *regs)
{
	exynos_smc(SMC_CMD_C15RESUME, regs[0], regs[1], 0);

	return 0;
}

static const struct firmware_ops exynos_firmware_ops = {
	.do_idle		= exynos_do_idle,
	.set_cpu_boot_addr	= exynos_set_cpu_boot_addr,
	.cpu_boot		= exynos_cpu_boot,
	.l2x0_init	= exynos_l2x0_init,
	.l2x0_resume	= exynos_l2x0_resume,
	.suspend	= exynos_suspend,
	.resume		= exynos_resume,
	.c15resume	= exynos_c15resume,
};

void __init exynos_firmware_init(void)
{
	if (of_have_populated_dt()) {
		struct device_node *nd;
		const __be32 *addr;

		nd = of_find_compatible_node(NULL, NULL,
						"samsung,secure-firmware");
		if (!nd)
			return;

		addr = of_get_address(nd, 0, NULL, NULL);
		if (!addr) {
			pr_err("%s: No address specified.\n", __func__);
			return;
		}
	}

	pr_info("Running under secure firmware.\n");

	register_firmware_ops(&exynos_firmware_ops);
}
