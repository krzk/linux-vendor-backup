/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk-provider.h>

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ion_exynos.h>
#include <linux/dma-direct.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/smc.h>
#include <linux/soc/samsung/exynos-soc.h>

#include "npu-log.h"
#include "npu-config.h"
#include "npu-common.h"
#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"
#include "npu-system-soc.h"
#ifdef CONFIG_NPU_USE_MBR
#include "dsp-npu.h"
#endif

/* EVT0 DSP secure SFR */
#define DSP_CPU_INITVTOR		(0x17943000)

#define DSP_CPU_WAIT			(0x1794300c)
#define DSP_CPU_RELEASE			(0x17943040)


#define DNC_CPU0_CONFIGURATION	(0x15863400)

#define SFR_APB					(0x020)
#define DRCG_EN					(0x104)
#define MEM_CLK					(0x108)
#define LH_DRCG_EN				(0x40C)


#define OFFSET_END			0xFFFFFFFF
#define TIMEOUT_ITERATION		100

/* Initialzation steps for system_resume */
enum npu_system_resume_soc_steps {
	NPU_SYS_RESUME_SOC_CPU_ON,
	NPU_SYS_RESUME_SOC_COMPLETED
};

#ifdef ENABLE_PWR_ON
static int npu_pwr_on(struct npu_system *system);
#endif

static int npu_cpu_on(struct npu_system *system);
static int npu_cpu_off(struct npu_system *system);
static int npu_memory_alloc_from_heap(struct platform_device *pdev, struct npu_memory_buffer *buffer, dma_addr_t daddr, const char *heapname);
static void npu_memory_free_from_heap(struct npu_memory_buffer *buffer);
#ifdef CONFIG_NPU_HARDWARE
static int init_iomem_area(struct npu_system *system);
#endif

int npu_system_soc_probe(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;

	BUG_ON(!system);
#ifdef CONFIG_NPU_HARDWARE
	npu_dbg("system soc probe: ioremap areas\n");
	ret = init_iomem_area(system);
	if (ret) {
		probe_err("fail(%d) in init_iomem_area\n", ret);
		goto p_err;
	}

	return 0;
p_err:
#elif defined(CONFIG_NPU_LOOPBACK)
	system->tcu_sram.vaddr = kmalloc(0x80000, GFP_KERNEL);
	system->idp_sram.vaddr = kmalloc(0x100000, GFP_KERNEL);
	system->sfr_npu[0].vaddr = kmalloc(0x100000, GFP_KERNEL);
	system->sfr_npu[1].vaddr = kmalloc(0xf0000, GFP_KERNEL);
	system->pmu_npu.vaddr = kmalloc(0x100, GFP_KERNEL);
	system->pmu_npu_cpu.vaddr = kmalloc(0x100, GFP_KERNEL);
	//system->baaw_npu.vaddr = kmalloc(0x100, GFP_KERNEL);
	system->mbox_sfr.vaddr = kmalloc(0x17c, GFP_KERNEL);
	system->pwm_npu.vaddr = kmalloc(0x10000, GFP_KERNEL);

#endif

	return ret;
}

int npu_system_soc_release(struct npu_system *system, struct platform_device *pdev)
{
	return 0;
}

static inline void print_iomem_area(const char *pr_name, const struct npu_iomem_area *mem_area)
{
	if (mem_area->vaddr)
		npu_info(KERN_CONT "(%8s) Phy(0x%08x)-(0x%08llx) Virt(%pK) Size(%llu)\n",
			pr_name, mem_area->paddr, mem_area->paddr + mem_area->size,
			mem_area->vaddr, mem_area->size);
}

static void print_all_iomem_area(const struct npu_system *system)
{
	int i;
	char buf[256];
#ifdef CONFIG_NPU_HARDWARE
	npu_dbg("start in IOMEM mapping\n");
	print_iomem_area("TCUSRAM", &system->tcu_sram);
	print_iomem_area("IDPSRAM", &system->idp_sram);
	print_iomem_area("SFR_DNC", &system->sfr_dnc);
	for (i = 0; i < 2; i++) {
		sprintf(buf, "SFR_NPUC%d", i);
		print_iomem_area(buf, &system->sfr_npuc[i]);
	}
	for (i = 0; i < 4; i++) {
		sprintf(buf, "SFR_NPU%d", i);
		print_iomem_area(buf, &system->sfr_npu[i]);
	}
	print_iomem_area("PMU_NPU", &system->pmu_npu);
	print_iomem_area("PMU_NCPU", &system->pmu_npu_cpu);
	print_iomem_area("MBOX_SFR", &system->mbox_sfr);
	print_iomem_area("PWM_NPU", &system->pwm_npu);
#endif
	npu_dbg("end in IOMEM mapping\n");

}

int npu_system_soc_resume(struct npu_system *system, u32 mode)
{
	int ret = 0;
#ifdef CONFIG_NPU_LOOPBACK
	return ret;
#endif
	BUG_ON(!system);

	/* sys reg enables */
	/* NPU0 */
	npu_set_sfr(0x10A20000 + 0x600, (0x1 << 8) | (0x1 << 0), (0x1 << 8) | (0x1 << 0));//NPUDUNIT_DSU_CLK_GATE
	npu_set_sfr(0x10A20000 + 0x604, (0x7 << 8) | (0x1 << 0), (0x7 << 8) | (0x1 << 0));//NPUDUNIT_DU_CLK_GATE
	npu_set_sfr(0x10A20000 + 0x608, (0x1 << 8) | (0x1 << 0), (0x1 << 8) | (0x1 << 0));//NPUDUNIT_MAA_CLK_GATE

	//NPUDUNIT_NFU_CLK_GATE
	npu_set_sfr(0x10A20000 + 0x60C, (0x3 << 14) | (0xF << 8) | (0x1 << 0), (0x3 << 14) | (0xF << 8) | (0x1 << 0));
	npu_set_sfr(0x10A20000 + 0x610, (0x3 << 8) | (0x1 << 0), (0x3 << 8) | (0x1 << 0));//NPUDUNIT_DRU_CLK_GATE
	npu_set_sfr(0x10A20000 + 0x614, (0x1 << 8) | (0x1 << 0), (0x1 << 8) | (0x1 << 0));//NPUDUNIT_SRAM_CLK_GATE

	/* NPU1 */
	npu_set_sfr(0x10B20000 + 0x600, (0x1 << 8) | (0x1 << 0), (0x1 << 8) | (0x1 << 0));//NPUDUNIT_DSU_CLK_GATE
	npu_set_sfr(0x10B20000 + 0x604, (0x7 << 8) | (0x1 << 0), (0x7 << 8) | (0x1 << 0));//NPUDUNIT_DU_CLK_GATE
	npu_set_sfr(0x10B20000 + 0x608, (0x1 << 8) | (0x1 << 0), (0x1 << 8) | (0x1 << 0));//NPUDUNIT_MAA_CLK_GATE

	//NPUDUNIT_NFU_CLK_GATE
	npu_set_sfr(0x10B20000 + 0x60C, (0x3 << 14) | (0xF << 8) | (0x1 << 0), (0x3 << 14) | (0xF << 8) | (0x1 << 0));
	npu_set_sfr(0x10B20000 + 0x610, (0x3 << 8) | (0x1 << 0), (0x3 << 8) | (0x1 << 0));//NPUDUNIT_DRU_CLK_GATE
	npu_set_sfr(0x10B20000 + 0x614, (0x1 << 8) | (0x1 << 0), (0x1 << 8) | (0x1 << 0));//NPUDUNIT_SRAM_CLK_GATE

	/* Clear resume steps */
	system->resume_soc_steps = 0;

	print_all_iomem_area(system);
#ifdef ENABLE_PWR_ON
	ret = npu_pwr_on(system);
	if (ret) {
		npu_err("fail(%d) in npu_pwr_on\n", ret);
		goto p_err;
	}
#endif

	ret = npu_cpu_on(system);
	if (ret) {
		npu_err("fail(%d) in npu_cpu_on\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_SOC_CPU_ON, &system->resume_soc_steps);
	set_bit(NPU_SYS_RESUME_SOC_COMPLETED, &system->resume_soc_steps);

	return ret;
p_err:
	npu_err("Failure detected[%d].\n", ret);
	return ret;
}

int npu_system_soc_suspend(struct npu_system *system)
{
	int ret = 0;
#ifdef CONFIG_NPU_LOOPBACK
	return ret;
#endif
	BUG_ON(!system);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC_COMPLETED, &system->resume_soc_steps, NULL, ;);
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC_CPU_ON, &system->resume_soc_steps, "Turn NPU cpu off", {
		ret = npu_cpu_off(system);
		if (ret)
			npu_err("fail(%d) in npu_cpu_off\n", ret);
	});

	if (system->resume_soc_steps != 0)
		npu_warn("Missing clean-up steps [%lu] found.\n", system->resume_soc_steps);

	/* Function itself never be failed, even thought there was some error */
	return ret;
}

int npu_hwacg(struct npu_system *system, bool on)
{
	npu_dbg("hwacg %s\n", on ? "on" : "off");

	return 0;
}

static int npu_cpu_on(struct npu_system *system)
{
	int ret = 0;

	const struct reg_set_map_2 cpu_on_regs[] = {
		/* NPU0_CM7_CFG1, SysTick Callibration, use external OSC, 26MHz */
#ifdef FORCE_HWACG_DISABLE
		{&system->sfr_npu[0],	0x800,	0x00,	0x31000000},	/* NPU0_CMU_NPU0_CONTROLLER_OPTION */
		{&system->sfr_npu[1],	0x800,	0x00,	0x31000000},	/* NPU1_CMU_NPU1_CONTROLLER_OPTION */
#endif
		//{&system->pmu_npu_cpu,	0x00,	0x01,	0x01},	/* NPU0_CPU_CONFIGURATION */
#ifdef FORCE_WDT_DISABLE
		//{&system->pmu_npu_cpu,	0x20,	0x00,	0x02},	/* NPU0_CPU_OUT */
#else
		//{&system->pmu_npu_cpu,  0x20,   0x02,   0x02},  /* NPU0_CPU_OUT */
#endif
	};

	ret = npu_set_hw_reg_2(cpu_on_regs, ARRAY_SIZE(cpu_on_regs), 0);
	if (ret) {
		npu_err("Failed to write registers on cpu_on_regs array (%d)\n", ret);
		goto err_exit;
	}

#ifdef CONFIG_NPU_USE_MBR
	npu_info("use Master Boot Record\n");
	npu_info("ask to jump to  0x%x\n", (u32)system->fw_npu_memory_buffer->daddr);
	ret = dsp_npu_release(true, system->fw_npu_memory_buffer->daddr);
	if (ret) {
		npu_err("Failed to release CPU_SS : %d\n", ret);
		goto err_exit;
	}
#else

	//changed one for 0x50000000
	const unsigned char early_boot_code[] = {0x00, 0x00, 0x00, 0xe3, 0x00, 0x00, 0x45, 0xe3, 0x10, 0xff,
												0x2f, 0xe1, 0x00, 0xf0, 0x20, 0xe3};

	memcpy(system->fw_npu_early_boot_buffer->vaddr, early_boot_code, sizeof(early_boot_code));

	//Secure setting
	npu_set_sfr(0x16400000 + 0x1000c, system->fw_npu_early_boot_buffer->daddr, 0xFFFF0000);
	npu_set_sfr(0x16400000 + 0x10004, 0x1, 0x1);
	npu_set_sfr(0x16400000 + 0x10040, 0x0, 0x6);
	npu_set_sfr(0x16400000 + 0x10040, 0x6, 0x6);

	npu_info("start in npu_cpu_on\n");
#endif

	npu_info("complete in npu_cpu_on\n");
	return 0;
err_exit:
	npu_info("error(%d) in npu_cpu_on\n", ret);
	return ret;
}

int npu_soc_core_on(struct npu_system *system, int core)
{
	int ret = 0;
	const struct reg_set_map_2 core0_on_regs[] = {
#ifdef FORCE_HWACG_DISABLE
		{&system->sfr_npu[0],   0x800,  0x0,    0x31000000},	/* NPU00_CMU_NPU00_CONTROLLER_OPTION */
		{&system->sfr_npu[1],   0x800,  0x0,    0x31000000},	/* NPU01_CMU_NPU01_CONTROLLER_OPTION */
#endif
	};
	const struct reg_set_map_2 core1_on_regs[] = {
#ifdef FORCE_HWACG_DISABLE
		//{&system->sfr_npu[2],   0x800,  0x0,    0x31000000},	/* NPU10_CMU_NPU10_CONTROLLER_OPTION */
		//{&system->sfr_npu[3],   0x800,  0x0,    0x31000000},	/* NPU11_CMU_NPU11_CONTROLLER_OPTION */
#endif
	};
	const struct reg_set_map_2 *core_on_regs[] = {
		core0_on_regs,
		core1_on_regs
	};
	const int core_on_regs_len[] = {
		ARRAY_SIZE(core0_on_regs),
		ARRAY_SIZE(core1_on_regs)
	};

	BUG_ON(!system);

	npu_info("start %s for core %d (%d)\n", __func__, core, core_on_regs_len[core]);
	ret = npu_set_hw_reg_2(core_on_regs[core], core_on_regs_len[core], 0);
	if (ret) {
		npu_err("Failed to write registers on core_on_regs array (%d)\n", ret);
		goto err_exit;
	}

	npu_info("complete %s for core %d\n", __func__, core);
	return 0;
err_exit:
	npu_info("error(%d) in %s\n", ret, __func__);
	return ret;
}

static int npu_cpu_off(struct npu_system *system)
{
	int ret;
	const struct reg_set_map_2 cpu_off_regs[] = {
#ifdef FORCE_HWACG_DISABLE
		{&system->sfr_npu[0],	0x800,	0x31000000,		0x31000000},	/* NPU00_CMU_NPU00_CONTROLLER_OPTION */
		{&system->sfr_npu[1],	0x800,	0x31000000,		0x31000000},	/* NPU01_CMU_NPU01_CONTROLLER_OPTION */
#endif
	};

#ifdef CONFIG_NPU_USE_MBR
	ret = dsp_npu_release(false, 0);
	if (ret) {
		npu_err("Failed to release CPU_SS : %d\n", ret);
		goto err_exit;
	}
#else

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	npu_info("start in npu_cpu_off\n");
	npu_set_sfr(0x16400000 + 0x10040, 0x0, 0x6);
#endif

	ret = npu_set_hw_reg_2(cpu_off_regs, ARRAY_SIZE(cpu_off_regs), 0);
	if (ret) {
		npu_err("fail(%d) in npu_set_hw_reg(cpu_on_regs)\n", ret);
		goto err_exit;
	}

	npu_info("complete in npu_cpu_off\n");
	return 0;
err_exit:
	npu_info("error(%d) in npu_cpu_off\n", ret);
	return ret;
}

int npu_soc_core_off(struct npu_system *system, int core)
{
	int ret;
	const struct reg_set_map_2 core0_off_regs[] = {
#ifdef FORCE_HWACG_DISABLE
		{&system->sfr_npu[0],   0x800,  0x31000000,    0x31000000},	/* NPU00_CMU_NPU00_CONTROLLER_OPTION */
		{&system->sfr_npu[1],   0x800,  0x31000000,    0x31000000},	/* NPU01_CMU_NPU01_CONTROLLER_OPTION */
#endif
	};
	const struct reg_set_map_2 core1_off_regs[] = {
#ifdef FORCE_HWACG_DISABLE
		//{&system->sfr_npu[2],   0x800,  0x31000000,    0x31000000},	/* NPU10_CMU_NPU10_CONTROLLER_OPTION */
		//{&system->sfr_npu[3],   0x800,  0x31000000,    0x31000000},	/* NPU11_CMU_NPU11_CONTROLLER_OPTION */
#endif
	};
	const struct reg_set_map_2 *core_off_regs[] = {
		core0_off_regs,
		core1_off_regs
	};
	const int core_off_regs_len[] = {
		ARRAY_SIZE(core0_off_regs),
		ARRAY_SIZE(core1_off_regs)
	};

	BUG_ON(!system);

	npu_info("start %s for core %d (%d)\n", __func__, core, core_off_regs_len[core]);
	ret = npu_set_hw_reg_2(core_off_regs[core], core_off_regs_len[core], 0);
	if (ret) {
		npu_err("Failed to write registers on core_off_regs array (%d)\n", ret);
		goto err_exit;
	}
	npu_info("complete %s for core %d\n", __func__, core);
	return 0;
err_exit:
	npu_info("error(%d) in %s\n", ret, __func__);
	return ret;
}

#ifdef ENABLE_PWR_ON
static __attribute__((unused)) int npu_pwr_on(struct npu_system *system)
{
	int ret = 0;
	int check_pass;
	u32 v;
	size_t i;
	int j;
	void __iomem *reg_addr;

	const static struct reg_set_map pwr_on_regs[] = {
		{0x00,	0x01,	0x01},		/* NPU0_CONFIGURATION */
		{0x80,	0x01,	0x01},		/* NPU1_CONFIGURATION */
	};
	const static struct reg_set_map pwr_status_regs[] = {
		{0x04,	0x01,	0x01},		/* NPU0_STATUS */
		{0x84,	0x01,	0x01},		/* NPU1_STATUS */
	};

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	npu_info("start in npu_pwr_on\n");

	npu_dbg("set power-on NPU in npu_pwr_on\n");
	ret = npu_set_hw_reg(&system->pmu_npu, pwr_on_regs, ARRAY_SIZE(pwr_on_regs), 0);
	if (ret) {
		npu_err("fail(%d) in npu_set_hw_reg(pwr_on_regs)\n", ret);
		goto err_exit;
	}

	/* Check status */
	npu_dbg("check status in npu_pwr_on\n");
	for (j = 0, check_pass = 0; j < TIMEOUT_ITERATION && !check_pass; j++) {
		check_pass = 1;
		for (i = 0; i < ARRAY_SIZE(pwr_status_regs); ++i) {
			// Check status flag
			reg_addr = system->pmu_npu.vaddr + pwr_status_regs[i].offset;
			v = readl(reg_addr);
			if ((v & pwr_status_regs[i].mask) != pwr_status_regs[i].val)
				check_pass = 0;

			npu_dbg("get status NPU[%zu]=0x%08x\n", i, v);
		}
		mdelay(10);
	}

	/* Timeout check */
	if (j >= TIMEOUT_ITERATION) {
		ret = -ETIMEDOUT;
		goto err_exit;
	}



	//directly write
	npu_set_sfr(0x10e60000 + 0x2480, 0x1, 0x1);//NPU0_CONFIGURATION
	npu_set_sfr(0x10e60000 + 0x2500, 0x1, 0x1);//NPU1_CONFIGURATION
	npu_set_sfr(0x10e60000 + 0x2580, 0x1, 0x1);//DNC_CONFIGURATION
	npu_set_sfr(0x10e60000 + 0x2680, 0x1, 0x1);//DSP0_CONFIGURATION
	npu_set_sfr(0x10e60000 + 0x2700, 0x1, 0x1);//DSP1_CONFIGURATION
	npu_set_sfr(0x10e60000 + 0x2f80, 0x1, 0x1);//DNC_CPU0_CONFIGURATION
	npu_set_sfr(0x10e60000 + 0x3000, 0x1, 0x1);//DNC_CPU1_CONFIGURATION
	npu_info("complete in npu_pwr_on\n");
	return 0;

err_exit:
	npu_info("error(%d) in npu_pwr_on\n", ret);
	return ret;

}
#endif

#ifdef CONFIG_NPU_HARDWARE
struct iomem_reg_t {
	u32 dummy;
	u32 start;
	u32 size;
};

static inline void set_mailbox_channel(struct npu_system *system, int ch)
{
	BUG_ON(ch > 1);
	probe_info("mailbox channel : %d\n", ch);
	system->mbox_sfr.vaddr = system->sfr_mbox[ch].vaddr;
	system->mbox_sfr.paddr = system->sfr_mbox[ch].paddr;
	system->mbox_sfr.size = system->sfr_mbox[ch].size;
}

static inline void set_max_npu_core(struct npu_system *system, u32 num)
{
	BUG_ON(num < 0);
	probe_info("Max number of npu core : %d\n", num);
	system->max_npu_core = num;
}

static inline int get_iomem_data_index(const struct npu_iomem_init_data data[], const char *name)
{
	int i;

	for (i = 0; data[i].name != NULL; i++) {
		if (!strcmp(data[i].name, name))
			return i;
	}
	return -1;
}

static struct npu_memory_buffer *flush_memory_buffer;
static int init_iomem_area(struct npu_system *system)
{
	int ret = 0;
	int i, k, di;
	void __iomem *iomem;
	struct device *dev;
	int iomem_count;
	struct iomem_reg_t *iomem_data;
	const char **iomem_name;
	struct npu_iomem_area *id;
	struct npu_memory_buffer **bd;

	const struct npu_iomem_init_data init_data[] = {
		{NULL,		"TCUSRAM",	(void *)&system->tcu_sram},
		{NULL,		"IDPSRAM",	(void *)&system->idp_sram},
		{NULL,		"SFR_DNC",	(void *)&system->sfr_dnc},
		{NULL,		"SFR_NPUC0",	(void *)&system->sfr_npuc[0]},
		{NULL,		"SFR_NPUC1",	(void *)&system->sfr_npuc[1]},
		{NULL,		"SFR_NPU0", (void *)&system->sfr_npu[0]},
		{NULL,		"SFR_NPU1", (void *)&system->sfr_npu[1]},
		{NULL,		"SFR_NPU2", (void *)&system->sfr_npu[2]},
		{NULL,		"SFR_NPU3", (void *)&system->sfr_npu[3]},
		{NULL,		"PMU",		(void *)&system->pmu_npu},
		{NULL,		"PMUCPU",	(void *)&system->pmu_npu_cpu},
		{NULL,		"MAILBOX0", (void *)&system->sfr_mbox[0]},
		{NULL,		"MAILBOX1", (void *)&system->sfr_mbox[1]},
		{NULL,		"PWM",		(void *)&system->pwm_npu},
		{"npu_fw",	"FW_DRAM",	(void *)&system->fw_npu_memory_buffer},
		{"npu_fw",	"FW_UNITTEST",	(void *)&system->fw_npu_unittest_buffer},
		{"npu_fw",	"FW_LOG",	(void *)&system->fw_npu_log_buffer},
		{NULL,		NULL,		NULL}
	};

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	probe_trace("start in iomem area.\n");

	dev = &(system->pdev->dev);
	iomem_count = of_property_count_strings(
			dev->of_node, "samsung,npumem-names");
	if (IS_ERR_VALUE((unsigned long)iomem_count)) {
		probe_err("invalid iomem list in %s node", dev->of_node->name);
		ret = -EINVAL;
		goto err_exit;
	}

	iomem_name = (const char **)devm_kmalloc(dev,
				(iomem_count + 1) * sizeof(const char *),
				GFP_KERNEL);
	if (!iomem_name) {
		probe_err("failed to alloc for iomem names");
		ret = -ENOMEM;
		goto err_exit;
	}

	for (i = 0; i < iomem_count; i++) {
		ret = of_property_read_string_index(dev->of_node,
				"samsung,npumem-names", i, &iomem_name[i]);
		if (ret) {
			probe_err("failed to read iomem name %d from %s node\n",
					i, dev->of_node->name);
			goto err_exit;
		}
	}
	iomem_name[iomem_count] = NULL;

	iomem_data = (struct iomem_reg_t *)devm_kmalloc(dev,
			iomem_count * sizeof(struct iomem_reg_t), GFP_KERNEL);
	if (!iomem_data) {
		probe_err("failed to alloc for iomem data");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = of_property_read_u32_array(dev->of_node, "samsung,npumem-address", (u32 *)iomem_data,
			iomem_count * sizeof(struct iomem_reg_t) / sizeof(u32));
	if (ret) {
		probe_err("failed to get iomem data");
		goto err_exit;
	}

	for (i = 0; iomem_name[i] != NULL; i++) {
		di = get_iomem_data_index(init_data, iomem_name[i]);
		if (di < 0) {
			probe_err("invalid npumem names : %s\n", iomem_name[i]);
			continue;
		}

		if (init_data[di].heapname) {
			bd = (struct npu_memory_buffer **)init_data[di].area_info;
			*bd = kcalloc(1, sizeof(struct npu_memory_buffer), GFP_KERNEL);
			(*bd)->size = (iomem_data + i)->size;
			ret = npu_memory_alloc_from_heap(system->pdev, *bd,
					(iomem_data + i)->start, init_data[di].heapname);
			if (ret) {
				for (k = 0; init_data[k].name != NULL; k++) {
					if (init_data[k].heapname) {
						bd = (struct npu_memory_buffer **)init_data[k].area_info;
						if (*bd) {
							if ((*bd)->vaddr)
								npu_memory_free_from_heap(*bd);
							kfree(*bd);
							*bd = NULL;
						}
					}
				}
				probe_err("buffer allocation from %s heap failed w/ err: %d\n",
						init_data[di].heapname, ret);
				ret = -EFAULT;
				goto err_exit;
			}
		} else {
			id = (struct npu_iomem_area *)init_data[di].area_info;
			id->paddr = (iomem_data + i)->start;
			id->size = (iomem_data + i)->size;
			iomem = devm_ioremap_nocache(&(system->pdev->dev),
				(iomem_data + i)->start, (iomem_data + i)->size);
			if (IS_ERR_OR_NULL(iomem)) {
				probe_err("fail(%pK) in devm_ioremap_nocache(0x%08x, %u)\n",
					iomem, id->paddr, (u32)id->size);
				ret = -EFAULT;
				goto err_exit;
			}
			id->vaddr = iomem;
			probe_info("%s : Paddr[%08x], [%08x] => Mapped @[%pK], Length = %llu\n",
					iomem_name[i], (iomem_data + i)->start, (iomem_data + i)->size,
					id->vaddr, id->size);
		}
	}

	set_mailbox_channel(system, 0);
	set_max_npu_core(system, 1);
	flush_memory_buffer = system->fw_npu_memory_buffer;

	probe_trace("complete in init_iomem_area\n");
	return 0;
err_exit:
	probe_err("error(%d) in init_iomem_area\n", ret);
	return ret;
}

void npu_memory_sync_for_cpu(void)
{
	dma_sync_single_for_cpu(
			flush_memory_buffer->attachment->dev,
			(dma_addr_t)(sg_dma_address(flush_memory_buffer->sgt->sgl) + NPU_MAILBOX_BASE),
			(size_t)NPU_MAILBOX_SIZE,
			DMA_FROM_DEVICE);
}

void npu_memory_sync_for_device(void)
{
	dma_sync_single_for_device(
			flush_memory_buffer->attachment->dev,
			(dma_addr_t)(sg_dma_address(flush_memory_buffer->sgt->sgl) + NPU_MAILBOX_BASE),
			(size_t)NPU_MAILBOX_SIZE,
			DMA_TO_DEVICE);
}

static int npu_memory_alloc_from_heap(struct platform_device *pdev, struct npu_memory_buffer *buffer,
				dma_addr_t daddr, const char *heapname)
{
	int ret = 0;
	bool complete_suc = false;

	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct scatterlist *sg;
	struct sg_table *sgt;
	phys_addr_t phys_addr;
	void *vaddr;
	int i;
	int flag;

	size_t size;

	BUG_ON(!buffer);

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
	INIT_LIST_HEAD(&buffer->list);

	flag = 0;

	size = buffer->size;

	dma_buf = ion_alloc_dmabuf(heapname, size, flag);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is fail(%p)\n", dma_buf);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma_buf = dma_buf;

	attachment = dma_buf_attach(dma_buf, &pdev->dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		npu_err("dma_buf_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->attachment = attachment;

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		npu_err("dma_buf_map_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->sgt = sgt;

	for_each_sg(buffer->sgt->sgl, sg, sgt->nents, i) {
		phys_addr = dma_to_phys(&pdev->dev, sg->dma_address);
	}
	buffer->paddr = phys_addr;

	if (!daddr) {
		daddr = ion_iovmm_map(attachment, 0, size, DMA_BIDIRECTIONAL, 0);
		if (IS_ERR_VALUE(daddr)) {
			npu_err("fail(err %pad) in ion_iovmm_map\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}
	} else {
		struct iommu_domain *domain = iommu_get_domain_for_dev(&pdev->dev);

		ret = iommu_map(domain, daddr, phys_addr, size, 0);
		if (ret) {
			npu_err("fail(err %pad) in iommu_map\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}
	}
	buffer->daddr = daddr;

	vaddr = dma_buf_vmap(dma_buf);
	if (IS_ERR(vaddr)) {
		npu_err("fail(err %p) in dma_buf_vmap\n", vaddr);
		ret = -EFAULT;
		goto p_err;
	}
	buffer->vaddr = vaddr;

	complete_suc = true;

	npu_dbg("buffer[%p], paddr[%llx], vaddr[%p], daddr[%llx], sgt[%p], attachment[%p]\n",
		buffer, buffer->paddr, buffer->vaddr, buffer->daddr, buffer->sgt, buffer->attachment);


p_err:
	if (complete_suc != true) {
		npu_memory_free_from_heap(buffer);
	}
	return ret;
}

static void npu_memory_free_from_heap(struct npu_memory_buffer *buffer)
{
	if (buffer->vaddr) {
		dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
	}
	if (buffer->daddr) {
		ion_iovmm_unmap(buffer->attachment, buffer->daddr);
	}
	if (buffer->sgt) {
		dma_buf_unmap_attachment(buffer->attachment, buffer->sgt, DMA_BIDIRECTIONAL);
	}
	if (buffer->attachment) {
		dma_buf_detach(buffer->dma_buf, buffer->attachment);
	}
	if (buffer->dma_buf) {
		dma_buf_put(buffer->dma_buf);
	}

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
}

void npu_soc_status_report(struct npu_system *system)
{
	BUG_ON(!system);

	npu_info("CA5 PC : CPU0 0x%x, CPU1 0x%x\n",
		readl(system->sfr_dnc.vaddr + 0x100cc),
		readl(system->sfr_dnc.vaddr + 0x100d0));
	npu_info("DSPC DBG STATUS : INTR 0x%x, DINTR 0x%x\n",
		readl(system->sfr_dnc.vaddr + 0x110c),
		readl(system->sfr_dnc.vaddr + 0x1144));
}

u32 npu_get_hw_info(void)
{
	union npu_hw_info hw_info;

	memset(&hw_info, 0, sizeof(hw_info));

	BUG_ON(exynos_soc_info.product_id != EXYNOS9630_SOC_ID);
	hw_info.fields.product_id = 0x9630;

	/* DCache disable before EVT 1.0 */
	hw_info.fields.dcache_en = (exynos_soc_info.main_rev >= 1) ? 1 : 0;

	npu_info("HW Info = %08x\n", hw_info.value);

	return hw_info.value;
}

#endif

