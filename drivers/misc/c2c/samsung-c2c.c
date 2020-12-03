/*
 * Samsung C2C driver
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Kisang Lee <kisang80.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/cma.h>
#include <linux/sysfs.h>
#ifdef CONFIG_C2C_IPC_ENABLE
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#endif
#include <asm/mach-types.h>
#include <linux/mm.h>

#include <mach/c2c.h>
#include <mach/regs-c2c.h>
#include <mach/regs-pmu.h>
#include <mach/regs-pmu5.h>
#include <mach/pmu.h>
#include <plat/cpu.h>

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <mach/dma.h>

#include <mach/c2c.h>
#include "samsung-c2c.h"

#define SRC 0
#define DST 1

struct c2c_dma_thread {
	struct task_struct *task;
	dma_addr_t buff_virt[2];
	dma_addr_t buff_phys[2];
	enum s3c2410_dma_buffresult res;
	int size;
	int is_running;
	struct s3c2410_dma_client cl;
	struct completion xfer_cmplt;
};

static struct c2c_state_control c2c_con;

static inline void c2c_writel(u32 val, int reg)
{
	writel(val, c2c_con.ap_sscm_addr + reg);
}

static inline void c2c_writew(u16 val, int reg)
{
	writew(val, c2c_con.ap_sscm_addr + reg);
}

static inline void c2c_writeb(u8 val, int reg)
{
	writeb(val, c2c_con.ap_sscm_addr + reg);
}

static inline u32 c2c_readl(int reg)
{
	return readl(c2c_con.ap_sscm_addr + reg);
}

static inline u16 c2c_readw(int reg)
{
	return readw(c2c_con.ap_sscm_addr + reg);
}

static inline u8 c2c_readb(int reg)
{
	return readb(c2c_con.ap_sscm_addr + reg);
}

static inline void c2c_writel_cp(u32 val, int reg)
{
	writel(val, c2c_con.cp_sscm_addr + reg);
}

static inline void c2c_writew_cp(u16 val, int reg)
{
	writew(val, c2c_con.cp_sscm_addr + reg);
}

static inline void c2c_writeb_cp(u8 val, int reg)
{
	writeb(val, c2c_con.cp_sscm_addr + reg);
}

static inline u32 c2c_readl_cp(int reg)
{
	return readl(c2c_con.cp_sscm_addr + reg);
}

static inline u16 c2c_readw_cp(int reg)
{
	return readw(c2c_con.cp_sscm_addr + reg);
}

static inline u8 c2c_readb_cp(int reg)
{
	return readb(c2c_con.cp_sscm_addr + reg);
}

static inline enum c2c_set_clear c2c_get_clock_gating(void)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);
	if (sysreg & (1 << C2C_SYSREG_CG))
		return C2C_SET;
	else
		return C2C_CLEAR;
}

static inline void c2c_set_clock_gating(enum c2c_set_clear val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	if (val == C2C_SET)
		sysreg |= (1 << C2C_SYSREG_CG);
	else
		sysreg &= ~(1 << C2C_SYSREG_CG);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static inline enum c2c_set_clear c2c_get_memdone(void)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);
	if (sysreg & (1 << C2C_SYSREG_MD))
		return C2C_SET;
	else
		return C2C_CLEAR;
}

static inline void c2c_set_memdone(enum c2c_set_clear val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	if (val == C2C_SET)
		sysreg |= (1 << C2C_SYSREG_MD);
	else
		sysreg &= ~(1 << C2C_SYSREG_MD);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static inline enum c2c_set_clear c2c_get_master_on(void)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);
	if (sysreg & (1 << C2C_SYSREG_MO))
		return C2C_SET;
	else
		return C2C_CLEAR;
}

static inline void c2c_set_master_on(enum c2c_set_clear val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	if (val == C2C_SET)
		sysreg |= (1 << C2C_SYSREG_MO);
	else
		sysreg &= ~(1 << C2C_SYSREG_MO);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static inline u32 c2c_get_func_clk(void)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= (0x3ff << C2C_SYSREG_FCLK);

	return sysreg >> C2C_SYSREG_FCLK;
}

static inline void c2c_set_func_clk(u32 val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= ~(0x3ff << C2C_SYSREG_FCLK);
	sysreg |= (val << C2C_SYSREG_FCLK);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static inline u32 c2c_get_tx_buswidth(void)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= (0x3 << C2C_SYSREG_TXW);

	return sysreg >> C2C_SYSREG_TXW;
}

static inline void c2c_set_tx_buswidth(u32 val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= ~(0x3 << C2C_SYSREG_TXW);
	sysreg |= (val << C2C_SYSREG_TXW);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static inline u32 c2c_get_rx_buswidth(void)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= (0x3 << C2C_SYSREG_RXW);

	return sysreg >> C2C_SYSREG_RXW;
}

static inline void c2c_set_rx_buswidth(u32 val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= ~(0x3 << C2C_SYSREG_RXW);
	sysreg |= (val << C2C_SYSREG_RXW);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static inline enum c2c_set_clear c2c_get_reset(void)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);
	if (sysreg & (1 << C2C_SYSREG_RST))
		return C2C_SET;
	else
		return C2C_CLEAR;
}

static inline void c2c_set_reset(enum c2c_set_clear val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	if (val == C2C_SET)
		sysreg |= (1 << C2C_SYSREG_RST);
	else
		sysreg &= ~(1 << C2C_SYSREG_RST);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static inline void c2c_set_rtrst(enum c2c_set_clear val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	if (val == C2C_SET)
		sysreg |= (1 << C2C_SYSREG_RTRST);
	else
		sysreg &= ~(1 << C2C_SYSREG_RTRST);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static inline u32 c2c_get_base_addr(void)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= (0x3ff << C2C_SYSREG_BASE_ADDR);

	return sysreg >> C2C_SYSREG_BASE_ADDR;
}

static inline void c2c_set_base_addr(u32 val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= ~(0x3ff << C2C_SYSREG_BASE_ADDR);
	sysreg |= (val << C2C_SYSREG_BASE_ADDR);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static inline u32 c2c_get_shdmem_size(void)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= (0x7 << C2C_SYSREG_DRAM_SIZE);

	return sysreg >> C2C_SYSREG_DRAM_SIZE;
}

static inline void c2c_set_shdmem_size(u32 val)
{
	u32 sysreg = readl(c2c_con.c2c_sysreg);

	sysreg &= ~(0x7 << C2C_SYSREG_DRAM_SIZE);
	sysreg |= (val << C2C_SYSREG_DRAM_SIZE);

	writel(sysreg, c2c_con.c2c_sysreg);
}

static unsigned int xfer_size = 1024 * 1024;	/* 1 MB */
static unsigned short burst_size = 8;
static unsigned int test_time_sec = 60 * 60;	/* default test time : 1 hour */
struct c2c_dma_thread *c2c_dma_test_thread;

void c2c_dma_cb(struct s3c2410_dma_chan *chan, void *buf_id, int size,
		enum s3c2410_dma_buffresult res)
{
	struct c2c_dma_thread *thread = buf_id;

	thread->res = res;
	thread->size = size;

	complete(&thread->xfer_cmplt);
}

static int free_c2c_dma_test(void)
{
	s3c2410_dma_free(DMACH_MTOM_0, &c2c_dma_test_thread->cl);

	if (c2c_dma_test_thread->buff_virt[DST] != 0) {
		dma_free_coherent(NULL, xfer_size,
			(void *)c2c_dma_test_thread->buff_virt[DST],
			c2c_dma_test_thread->buff_phys[DST]);
	}

	if (c2c_dma_test_thread->buff_virt[SRC] != 0) {
		dma_free_coherent(NULL, xfer_size,
			(void *)c2c_dma_test_thread->buff_virt[SRC],
			c2c_dma_test_thread->buff_phys[SRC]);
	}

	if (c2c_dma_test_thread != NULL)
		kfree(c2c_dma_test_thread);

	return 0;
}

static void init_dma_srcbuf(u32 *buf, unsigned int size)
{
	unsigned int i;
	for (i = 0; i < (size >> 2); i++)
		buf[i] = i;
}

static int dma_test_func(void *data)
{
	struct c2c_dma_thread *thread = data;
	unsigned long time_out = jiffies + test_time_sec * HZ;
	int val = 0;
	c2c_dma_test_thread->is_running = 1;

	pr_err("[C2C DMA Test] Start DMA test thread\n");
	while (!kthread_should_stop() && time_before(jiffies, time_out)) {
		s3c2410_dma_devconfig(DMACH_MTOM_0, S3C_DMA_MEM2MEM,
					thread->buff_phys[SRC]);
		s3c2410_dma_enqueue(DMACH_MTOM_0, (void *)thread,
					thread->buff_phys[DST], xfer_size);
		s3c2410_dma_ctrl(DMACH_MTOM_0, S3C2410_DMAOP_START);

		val = wait_for_completion_timeout(&thread->xfer_cmplt,
						msecs_to_jiffies(5*1000));
		if (!val) {
			dma_addr_t src, dst;
			s3c2410_dma_getposition(DMACH_MTOM_0, &src, &dst);
			pr_err("[C2C DMA Test] Transmission timeout! "
				"(src:%x, dst:%x)\n", src, dst);
			break;
		}

		if (thread->res != S3C2410_RES_OK &&
		    thread->size != xfer_size) {
			pr_err("[C2C DMA Test] DMA transmission fail\n");
			break;
		}

		val = memcmp((void *)thread->buff_virt[SRC],
			(void *)thread->buff_virt[DST], xfer_size);
		if (val != 0) {
			pr_err("[C2C DMA Test] src != dst (err %d)\n", val);
			break;
		}

		/* Buffer initialization */
		init_dma_srcbuf((u32 *)thread->buff_virt[SRC], xfer_size);
		memset((void *)thread->buff_virt[DST], 0, xfer_size);
		pr_err(".");
	}

	c2c_dma_test_thread->is_running = 0;
	free_c2c_dma_test();

	return 0;
}

static int init_dma_test(void)
{
	struct c2c_dma_thread *thread;
	int ret;

	pr_err("[C2C DMA Test] DMA busy test initialization\n");

	thread = kzalloc(sizeof(struct c2c_dma_thread), GFP_KERNEL);
	if (!thread)
		pr_err("[C2C DMA Test] DMA test thread allocation fail!\n");
	c2c_dma_test_thread = thread;

	thread->buff_virt[SRC] = 0;
	thread->buff_phys[SRC] = 0;
	thread->buff_virt[DST] = 0;
	thread->buff_phys[DST] = 0;

	thread->buff_virt[SRC] = (dma_addr_t)dma_alloc_coherent(NULL, xfer_size,
			&thread->buff_phys[SRC], GFP_KERNEL);
	if (!thread->buff_virt[SRC]) {
		pr_err("[C2C DMA Test] Source buffer allocation fail!\n");
		return -EBUSY;
	}

	thread->buff_virt[DST] = (dma_addr_t)dma_alloc_coherent(NULL, xfer_size,
			&thread->buff_phys[DST], GFP_KERNEL);
	if (!thread->buff_virt[DST]) {
		pr_err("[C2C DMA Test] Destination buffer allocation fail!\n");
		return -EBUSY;
	}
	pr_err("[C2C DMA Test] Buffer allocation success\n");
	pr_err("[C2C DMA Test] Src : %x, Dst : %x", thread->buff_phys[SRC],
			thread->buff_phys[DST]);

	/* Buffer initialization */
	init_dma_srcbuf((u32 *)thread->buff_virt[SRC], xfer_size);
	memset((void *)thread->buff_virt[DST], 0, xfer_size);

	thread->cl.name = (char *)thread;
	init_completion(&thread->xfer_cmplt);

	ret = s3c2410_dma_request(DMACH_MTOM_0, &thread->cl, NULL);
	if (ret) {
		pr_err("[C2C DMA Test] DMA request fail\n");
		return -EBUSY;
	}

	s3c2410_dma_set_buffdone_fn(DMACH_MTOM_0, c2c_dma_cb);

	ret = s3c2410_dma_config(DMACH_MTOM_0, burst_size);
	if (ret) {
		pr_err("[C2C DMA Test] DMA config fail\n");
		return -EBUSY;
	}

	thread->task = kthread_run(dma_test_func, thread, "c2c-m2m-test");
	if (IS_ERR(thread->task)) {
		pr_err("[C2C DMA Test] Thread running fail\n");
		return -EBUSY;
	}

	return 0;
}

void (*exynos_c2c_request_pwr_mode)(enum c2c_pwr_mode mode);

void c2c_reset_ops(void)
{
	/* This function will be only used for EVT0 or EVT0.1 */
	u32 set_clk = 0;

	if (c2c_con.opp_mode == C2C_OPP100)
		set_clk = c2c_con.clk_opp100;
	else if (c2c_con.opp_mode == C2C_OPP50)
		set_clk = c2c_con.clk_opp50;
	else if (c2c_con.opp_mode == C2C_OPP25)
		set_clk = c2c_con.clk_opp25;

	pr_err("[C2C] c2c_reset_ops()\n");
	clk_set_rate(c2c_con.c2c_sclk, (set_clk + 1) * MHZ);
	c2c_set_func_clk(set_clk);

	/* First phase - C2C block reset */
	c2c_set_reset(C2C_CLEAR);
	c2c_set_reset(C2C_SET);
	/* Second phase - Clear clock gating */
	c2c_set_clock_gating(C2C_CLEAR);
	/* Third phase - Retention reg */
	c2c_writel(c2c_con.retention_reg, EXYNOS_C2C_IRQ_EN_SET1);
	c2c_writel(set_clk, EXYNOS_C2C_FCLK_FREQ);
	c2c_writel(set_clk, EXYNOS_C2C_RX_MAX_FREQ);
	/* Last phase - Set clock gating */
	c2c_set_clock_gating(C2C_SET);
}

static ssize_t c2c_ctrl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	c2c_set_clock_gating(C2C_CLEAR);

	ret = sprintf(buf, "C2C State");
	ret += sprintf(&buf[ret], "SysReg : 0x%x\n",
			readl(c2c_con.c2c_sysreg));
	ret += sprintf(&buf[ret], "Port Config : 0x%x\n",
			c2c_readl(EXYNOS_C2C_PORTCONFIG));
	ret += sprintf(&buf[ret], "FCLK_FREQ : %d\n",
			c2c_readl(EXYNOS_C2C_FCLK_FREQ));
	ret += sprintf(&buf[ret], "RX_MAX_FREQ : %d\n",
			c2c_readl(EXYNOS_C2C_RX_MAX_FREQ));
	ret += sprintf(&buf[ret], "IRQ_EN_SET1 : 0x%x\n",
			c2c_readl(EXYNOS_C2C_IRQ_EN_SET1));
	ret += sprintf(&buf[ret], "IRQ_RAW_STAT1 : 0x%x\n",
			c2c_readl(EXYNOS_C2C_IRQ_RAW_STAT1));
	ret += sprintf(&buf[ret], "Get C2C sclk rate : %ld\n",
			clk_get_rate(c2c_con.c2c_sclk));
	ret += sprintf(&buf[ret], "Get C2C aclk rate : %ld\n",
			clk_get_rate(c2c_con.c2c_aclk));

	ret += sprintf(&buf[ret], "EXYNOS_C2C_GENO_INT : 0x%x\n",
			c2c_readl(EXYNOS_C2C_GENO_INT));
	ret += sprintf(&buf[ret], "EXYNOS_C2C_GENO_LEVEL : 0x%x\n",
			c2c_readl(EXYNOS_C2C_GENO_LEVEL));
	ret += sprintf(&buf[ret], "STANDBY_IN (0:linked 1: Dislinked) : %d\n",
			c2c_readl(EXYNOS_C2C_STANDBY_IN));

	c2c_set_clock_gating(C2C_SET);

	return ret;
}

static ssize_t c2c_ctrl_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ops_num, opp_val, req_clk, ret;
	sscanf(buf, "%d", &ops_num);

	switch (ops_num) {
	case 1:
		c2c_reset_ops();
		break;

	case 2:
	case 3:
	case 4:
		opp_val = ops_num - 1;
		req_clk = 0;
		pr_err("[C2C] Set current OPP mode (%d)\n", opp_val);

		if (opp_val == C2C_OPP100)
			req_clk = c2c_con.clk_opp100;
		else if (opp_val == C2C_OPP50)
			req_clk = c2c_con.clk_opp50;
		else if (opp_val == C2C_OPP25)
			req_clk = c2c_con.clk_opp25;

		if (opp_val == 0 || req_clk == 1) {
			pr_err("[C2C] ERR! undefined OPP mode\n");
		} else {
			c2c_set_clock_gating(C2C_CLEAR);
			if (c2c_con.opp_mode < opp_val) {
				/* increase case */
				clk_set_rate(c2c_con.c2c_sclk,
					(req_clk + 1) * MHZ);
				c2c_writel(req_clk, EXYNOS_C2C_FCLK_FREQ);
				c2c_set_func_clk(req_clk);
				c2c_writel(req_clk, EXYNOS_C2C_RX_MAX_FREQ);
			} else if (c2c_con.opp_mode > opp_val) {
				/* decrease case */
				c2c_writel(req_clk, EXYNOS_C2C_RX_MAX_FREQ);
				clk_set_rate(c2c_con.c2c_sclk,
					(req_clk + 1) * MHZ);
				c2c_writel(req_clk, EXYNOS_C2C_FCLK_FREQ);
				c2c_set_func_clk(req_clk);
			} else{
				pr_err("[C2C] same OPP mode\n");
			}
			c2c_con.opp_mode = opp_val;
			c2c_set_clock_gating(C2C_SET);
		}

		pr_err("[C2C] sclk rate %ld\n", clk_get_rate(c2c_con.c2c_sclk));
		pr_err("[C2C] aclk rate %ld\n", clk_get_rate(c2c_con.c2c_aclk));
		break;

	case 5:
		ret = init_dma_test();
		if (ret != 0)
			free_c2c_dma_test();
		break;

	case 6:
		if (c2c_dma_test_thread->is_running == 1)
			kthread_stop(c2c_dma_test_thread->task);
		break;

	default:
		pr_err("[C2C] Wrong C2C operation number\n");
		pr_err("[C2C] ---C2C Operation Number---\n");
		pr_err("[C2C] 1. C2C Reset\n");
		pr_err("[C2C] 2. Set OPP25\n");
		pr_err("[C2C] 3. Set OPP50\n");
		pr_err("[C2C] 4. Set OPP100\n");
		pr_err("[C2C] 5. Start mem busy for 1hour\n");
		pr_err("[C2C] 6. Stop mem busy\n");
	}

	return count;
}

static DEVICE_ATTR(c2c_ctrl, 0644, c2c_ctrl_show, c2c_ctrl_store);

int c2c_open(struct inode *inode, struct file *filp)
{
	pr_err("[C2C] %s\n", __func__);
	return 0;
}

int c2c_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int err;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long pfn = __phys_to_pfn(C2C_SHAREDMEM_BASE + offset);

	if ((offset + size) > C2C_SHAREDMEM_SIZE) {
		pr_err("[C2C] ERR! (offset + size) > C2C_SHAREDMEM_SIZE\n");
		return -EINVAL;
	}

	/* I/O memory must be noncacheable. */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= (VM_IO | VM_RESERVED);

	err = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
	if (err) {
		pr_err("[C2C] ERR! remap_pfn_range fail\n");
		return -EAGAIN;
	}

	pr_err("[C2C] %s: vm_start 0x%08X\n", __func__, (u32)vma->vm_start);
	pr_err("[C2C] %s: vm_end   0x%08X\n", __func__, (u32)vma->vm_end);
	pr_err("[C2C] %s: vm_flags 0x%08X\n", __func__, (u32)vma->vm_flags);
	return 0;
}

static long c2c_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	c2c_reset_ops();
	return 0;
}

static int c2c_release(struct inode *inode, struct file *filp)
{
	pr_err("[C2C] %s\n", __func__);
	return 0;
}

static const struct file_operations c2c_fops = {
	.owner = THIS_MODULE,
	.open = c2c_open,
	.release = c2c_release,
	.mmap = c2c_mmap,
	.unlocked_ioctl = c2c_ioctl,
};

static struct miscdevice char_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= C2C_DEV_NAME,
	.fops	= &c2c_fops
};

static int c2c_set_sharedmem(enum c2c_shrdmem_size size, u32 addr)
{
	pr_err("[C2C] %s: base 0x%08X, size %d MB\n",
		__func__, addr, (1 << (2 + size)));

	/* Set DRAM Base Addr & Size */
	c2c_set_shdmem_size(size);
	c2c_set_base_addr((addr >> 22));

	return 0;
}

static void c2c_set_interrupt(u32 genio_num, enum c2c_interrupt set_int)
{
	u32 cur_int_reg, cur_lev_reg;

	cur_int_reg = c2c_readl(EXYNOS_C2C_GENO_INT);
	cur_lev_reg = c2c_readl(EXYNOS_C2C_GENO_LEVEL);

	switch (set_int) {
	case C2C_INT_TOGGLE:
		cur_int_reg &= ~(0x1 << genio_num);
		c2c_writel(cur_int_reg, EXYNOS_C2C_GENO_INT);
		break;
	case C2C_INT_HIGH:
		cur_int_reg |= (0x1 << genio_num);
		cur_lev_reg |= (0x1 << genio_num);
		c2c_writel(cur_int_reg, EXYNOS_C2C_GENO_INT);
		c2c_writel(cur_lev_reg, EXYNOS_C2C_GENO_LEVEL);
		break;
	case C2C_INT_LOW:
		cur_int_reg |= (0x1 << genio_num);
		cur_lev_reg &= ~(0x1 << genio_num);
		c2c_writel(cur_int_reg, EXYNOS_C2C_GENO_INT);
		c2c_writel(cur_lev_reg, EXYNOS_C2C_GENO_LEVEL);
		break;
	}
}

static irqreturn_t c2c_sscm0_irq(int irq, void *data)
{
	/* TODO : This function will be used other type boards */
	return IRQ_HANDLED;
}

static irqreturn_t c2c_sscm1_irq(int irq, void *data)
{
	/* TODO : It is just temporary code. It will be modified. */
	u32 raw_irq, latency_val, opp_val, req_clk;

	raw_irq = c2c_readl(EXYNOS_C2C_IRQ_EN_STAT1);
	pr_err("[C2C] %s: interrupt 0x%08X\n", __func__, raw_irq);

#ifdef CONFIG_C2C_IPC_ENABLE
	if ((raw_irq >> C2C_GENIO_MBOX_INT) & 1) {
		if (c2c_con.hd.handler)
			c2c_con.hd.handler(c2c_con.hd.data);

	} else {
		c2c_writel(c2c_readl(EXYNOS_C2C_IRQ_RAW_STAT1),
			EXYNOS_C2C_IRQ_EN_STAT1);
		return IRQ_HANDLED;
	}
#endif

	if ((raw_irq >> C2C_GENIO_OPP_INT) & 1) {
		/* OPP Change
		----------------------------------------------------
		    OPP mode GENI/O bit definition[29:27]
		    OPP100 GENI/O[29:28] : 1 1
		    OPP50 GENI/O[29:28] : 1 0
		    OPP25 GENI/O[29:28] : 0 1
		    GENI[27] is only used for making interrupt.
		*/
		opp_val = (c2c_readl(EXYNOS_C2C_GENO_STATUS) >> 28) & 3;
		req_clk = 0;
		pr_err("[C2C] OPP interrupt occured (%d)\n", opp_val);

		if (opp_val == C2C_OPP100)
			req_clk = c2c_con.clk_opp100;
		else if (opp_val == C2C_OPP50)
			req_clk = c2c_con.clk_opp50;
		else if (opp_val == C2C_OPP25)
			req_clk = c2c_con.clk_opp25;

		if (opp_val == 0 || req_clk == 1) {
			pr_err("[C2C] ERR! undefined OPP mode\n");
		} else {
			if (c2c_con.opp_mode < opp_val) {
				/* increase case */
				clk_set_rate(c2c_con.c2c_sclk,
					(req_clk + 1) * MHZ);
				c2c_writel(req_clk, EXYNOS_C2C_FCLK_FREQ);
				c2c_set_func_clk(req_clk);
				c2c_writel(req_clk, EXYNOS_C2C_RX_MAX_FREQ);
			} else if (c2c_con.opp_mode > opp_val) {
				/* decrease case */
				c2c_writel(req_clk, EXYNOS_C2C_RX_MAX_FREQ);
				clk_set_rate(c2c_con.c2c_sclk,
					(req_clk + 1) * MHZ);
				c2c_writel(req_clk, EXYNOS_C2C_FCLK_FREQ);
				c2c_set_func_clk(req_clk);
			} else{
				pr_err("[C2C] same OPP mode\n");
			}
			c2c_con.opp_mode = opp_val;
		}

		/* Interrupt Clear */
		c2c_writel((0x1 << C2C_GENIO_OPP_INT), EXYNOS_C2C_IRQ_EN_STAT1);
	}

	/* Memory I/F latency change */
	if ((raw_irq >> C2C_GENIO_LATENCY_INT) & 1) {
		latency_val = (c2c_readl(EXYNOS_C2C_GENO_STATUS) >> 30) & 3;
		switch (latency_val) {
		case 3:
			pr_err("[C2C] Set Min latency\n");
			if (exynos_c2c_request_pwr_mode != NULL)
				exynos_c2c_request_pwr_mode(MIN_LATENCY);
			break;
		case 1:
			pr_err("[C2C] Set Short latency\n");
			if (exynos_c2c_request_pwr_mode != NULL)
				exynos_c2c_request_pwr_mode(SHORT_LATENCY);
			break;
		case 0:
			pr_err("[C2C] Set Max latency\n");
			if (exynos_c2c_request_pwr_mode != NULL)
				exynos_c2c_request_pwr_mode(MAX_LATENCY);
			break;
		}
		/* Interrupt Clear */
		c2c_writel((0x1 << C2C_GENIO_LATENCY_INT),
			EXYNOS_C2C_IRQ_EN_STAT1);
	}

	c2c_writel(c2c_readl(EXYNOS_C2C_IRQ_RAW_STAT1),
		EXYNOS_C2C_IRQ_EN_STAT1);

	return IRQ_HANDLED;
}

static void set_c2c_device(struct platform_device *pdev)
{
	struct exynos_c2c_platdata *pdata = pdev->dev.platform_data;
	u32 default_clk;
	int i;

	c2c_con.c2c_sysreg = pdata->c2c_sysreg;
	c2c_con.rx_width = pdata->rx_width;
	c2c_con.tx_width = pdata->tx_width;
	c2c_con.clk_opp100 = pdata->clk_opp100;
	c2c_con.clk_opp50 = pdata->clk_opp50;
	c2c_con.clk_opp25 = pdata->clk_opp25;
	c2c_con.opp_mode = pdata->default_opp_mode;
#ifdef CONFIG_C2C_IPC_ENABLE
	c2c_con.shd_pages = NULL;
	c2c_con.hd.data = NULL;
	c2c_con.hd.handler = NULL;
#endif
	c2c_con.c2c_sclk = clk_get(&pdev->dev, "sclk_c2c");
	c2c_con.c2c_aclk = clk_get(&pdev->dev, "aclk_c2c");

	if (soc_is_exynos4212())
		exynos_c2c_request_pwr_mode = exynos4_c2c_request_pwr_mode;
	else if (soc_is_exynos4412()) {
		exynos_c2c_request_pwr_mode = exynos4_c2c_request_pwr_mode;
		if (samsung_rev() >= EXYNOS4412_REV_1_0)
			writel(C2C_SYSREG_DEFAULT, c2c_con.c2c_sysreg);
	} else if (soc_is_exynos5250())
		exynos_c2c_request_pwr_mode = NULL;

	/* Set clock to default mode */
	if (c2c_con.opp_mode == C2C_OPP100)
		default_clk = c2c_con.clk_opp100;
	else if (c2c_con.opp_mode == C2C_OPP50)
		default_clk = c2c_con.clk_opp50;
	else if (c2c_con.opp_mode == C2C_OPP25)
		default_clk = c2c_con.clk_opp25;
	else {
		pr_err("[C2C] Default OPP mode is not selected.\n");
		c2c_con.opp_mode = C2C_OPP50;
		default_clk = c2c_con.clk_opp50;
	}

	clk_set_rate(c2c_con.c2c_sclk, (default_clk + 1)  * MHZ);
	clk_set_rate(c2c_con.c2c_aclk, ((default_clk / 2) + 1) * MHZ);

	pr_err("[C2C] Get C2C sclk rate : %ld\n",
				clk_get_rate(c2c_con.c2c_sclk));
	pr_err("[C2C] Get C2C aclk rate : %ld\n",
				clk_get_rate(c2c_con.c2c_aclk));
	if (pdata->setup_gpio)
		pdata->setup_gpio(pdata->rx_width, pdata->tx_width);

	c2c_set_sharedmem(pdata->shdmem_size, pdata->shdmem_addr);

	/* Set SYSREG to memdone */
	c2c_set_memdone(C2C_SET);
	c2c_set_clock_gating(C2C_CLEAR);

	/* Set C2C clock register to OPP50 */
	c2c_writel(default_clk, EXYNOS_C2C_FCLK_FREQ);
	c2c_writel(default_clk, EXYNOS_C2C_RX_MAX_FREQ);
	c2c_set_func_clk(default_clk);

	/* Set C2C buswidth */
	c2c_writel(((pdata->rx_width << 4) | (pdata->tx_width)),
					EXYNOS_C2C_PORTCONFIG);
	c2c_set_tx_buswidth(pdata->tx_width);
	c2c_set_rx_buswidth(pdata->rx_width);

	/* Enable all of GENI/O Interrupt */
	c2c_writel((0x1 << C2C_GENIO_MBOX_INT), EXYNOS_C2C_IRQ_EN_SET1);
	c2c_con.retention_reg |= (0x1 << C2C_GENIO_MBOX_INT);

	c2c_writel((0x1 << C2C_GENIO_MBOX_EXT_INT), EXYNOS_C2C_IRQ_EN_SET1);
	c2c_con.retention_reg |= (0x1 << C2C_GENIO_MBOX_EXT_INT);

	if (exynos_c2c_request_pwr_mode != NULL)
		exynos_c2c_request_pwr_mode(MAX_LATENCY);

	for (i = 0; i < 32; i++)
		c2c_set_interrupt(i, C2C_INT_HIGH);

	pr_err("[C2C] REG.PORT_CONFIG: 0x%08X\n",
		c2c_readl(EXYNOS_C2C_PORTCONFIG));
	pr_err("[C2C] REG.FCLK_FREQ  : 0x%08X\n",
		c2c_readl(EXYNOS_C2C_FCLK_FREQ));
	pr_err("[C2C] REG.RX_MAX_FREQ: 0x%08X\n",
		c2c_readl(EXYNOS_C2C_RX_MAX_FREQ));
	pr_err("[C2C] REG.IRQ_EN_SET1: 0x%08X\n",
		c2c_readl(EXYNOS_C2C_IRQ_EN_SET1));

	c2c_set_clock_gating(C2C_SET);
}

#ifdef CONFIG_C2C_IPC_ENABLE
void __iomem *c2c_request_cp_region(unsigned int cp_addr,
		unsigned int size)
{
	dma_addr_t phy_cpmem;

	phy_cpmem = cma_alloc(c2c_con.c2c_dev, "c2c_shdmem", size, 0);
	if (IS_ERR_VALUE(phy_cpmem)) {
		pr_err("[C2C] ERR! cma_alloc fail\n");
		return NULL;
	}

	return phys_to_virt(phy_cpmem);
}
EXPORT_SYMBOL(c2c_request_cp_region);

void c2c_release_cp_region(void *rgn)
{
	dma_addr_t phy_cpmem;

	phy_cpmem = virt_to_phys(rgn);

	cma_free(phy_cpmem);
}
EXPORT_SYMBOL(c2c_release_cp_region);

void __iomem *c2c_request_sh_region(unsigned int sh_addr, unsigned int size)
{
	int i;
	struct page **pages;
	unsigned int num_pages = (size >> PAGE_SHIFT);
	void *pv;

	pages = kmalloc(num_pages * sizeof(*pages), GFP_KERNEL);
	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(sh_addr);
		sh_addr += PAGE_SIZE;
	}

	c2c_con.shd_pages = (void *)pages;

	pv = vmap(pages, num_pages, VM_MAP, pgprot_noncached(PAGE_KERNEL));

	return (void __iomem *)pv;
}
EXPORT_SYMBOL(c2c_request_sh_region);

void c2c_release_sh_region(void *rgn)
{
	vunmap(rgn);
	kfree(c2c_con.shd_pages);
	c2c_con.shd_pages = NULL;
}
EXPORT_SYMBOL(c2c_release_sh_region);

int c2c_register_handler(void (*handler)(void *), void *data)
{
	if (!handler)
		return -EINVAL;

	c2c_con.hd.data = data;
	c2c_con.hd.handler = handler;
	return 0;
}
EXPORT_SYMBOL(c2c_register_handler);

int c2c_unregister_handler(void (*handler)(void *))
{
	if (!handler || (c2c_con.hd.handler != handler))
		return -EINVAL;

	c2c_con.hd.data = NULL;
	c2c_con.hd.handler = NULL;
	return 0;
}
EXPORT_SYMBOL(c2c_unregister_handler);

void c2c_send_interrupt(u32 cmd)
{
	pr_err("[C2C] %s: int_val 0x%08X\n", __func__, cmd);

	if (c2c_read_link()) {
		pr_err("[C2C] %s: c2c_read_link fail\n", __func__);
		return;
	}

	c2c_writel(cmd,	EXYNOS_C2C_GENI_CONTROL);
	c2c_writel(0x0,	EXYNOS_C2C_GENI_CONTROL);
}
EXPORT_SYMBOL(c2c_send_interrupt);

void c2c_reset_interrupt(void)
{
	c2c_writel(c2c_readl(EXYNOS_C2C_IRQ_RAW_STAT1),
		EXYNOS_C2C_IRQ_EN_STAT1);
}
EXPORT_SYMBOL(c2c_reset_interrupt);

u32 c2c_read_interrupt(void)
{
	if (c2c_read_link())
		return 0;

	return c2c_readl(EXYNOS_C2C_IRQ_RAW_STAT1);
}
EXPORT_SYMBOL(c2c_read_interrupt);

u32 c2c_read_link(void)
{
	u32 linkStat;
	c2c_set_clock_gating(C2C_CLEAR);
	linkStat = c2c_readl(EXYNOS_C2C_STANDBY_IN);
	c2c_set_clock_gating(C2C_SET);
	return linkStat;
}
EXPORT_SYMBOL(c2c_read_link);
#endif /*CONFIG_C2C_IPC_ENABLE*/

static int __devinit samsung_c2c_probe(struct platform_device *pdev)
{
	struct exynos_c2c_platdata *pdata = pdev->dev.platform_data;
	struct resource *res = NULL;
	struct resource *res1 = NULL;
	int sscm_irq0, sscm_irq1;
	int err = 0;

	c2c_con.c2c_dev = &pdev->dev;

	/* resource for AP's SSCM region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no memory resource defined(AP's SSCM)\n");
		return -ENOENT;
	}
	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!res) {
		dev_err(&pdev->dev, "failded to request memory resource(AP)\n");
		return -ENOENT;
	}
	pdata->ap_sscm_addr = ioremap(res->start, resource_size(res));
	if (!pdata->ap_sscm_addr) {
		dev_err(&pdev->dev, "failded to request memory resource(AP)\n");
		goto release_ap_sscm;
	}
	c2c_con.ap_sscm_addr = pdata->ap_sscm_addr;

	/* resource for CP's SSCM region */
	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res1) {
		dev_err(&pdev->dev, "no memory resource defined(AP's SSCM)\n");
		goto unmap_ap_sscm;
	}
	res1 = request_mem_region(res1->start, resource_size(res1), pdev->name);
	if (!res1) {
		dev_err(&pdev->dev, "failded to request memory resource(AP)\n");
		goto unmap_ap_sscm;
	}
	pdata->cp_sscm_addr = ioremap(res1->start, resource_size(res1));
	if (!pdata->cp_sscm_addr) {
		dev_err(&pdev->dev, "failded to request memory resource(CP)\n");
		goto release_cp_sscm;
	}
	c2c_con.cp_sscm_addr = pdata->cp_sscm_addr;

	/* Request IRQ */
	sscm_irq0 = platform_get_irq(pdev, 0);
	if (sscm_irq0 < 0) {
		dev_err(&pdev->dev, "no irq specified\n");
		goto unmap_cp_sscm;
	}
	err = request_irq(sscm_irq0, c2c_sscm0_irq, 0, pdev->name, pdev);
	if (err) {
		dev_err(&pdev->dev, "Can't request SSCM0 IRQ\n");
		goto unmap_cp_sscm;
	}
	/* SSCM0 irq will be only used for master(CP) device */
	disable_irq(sscm_irq0);

	sscm_irq1 = platform_get_irq(pdev, 1);
	if (sscm_irq1 < 0) {
		dev_err(&pdev->dev, "no irq specified\n");
		goto release_sscm_irq0;
	}

	err = request_irq(sscm_irq1, c2c_sscm1_irq, IRQF_NO_SUSPEND,
			pdev->name, pdev);
	if (err) {
		dev_err(&pdev->dev, "Can't request SSCM1 IRQ\n");
		goto release_sscm_irq0;
	}

	err = misc_register(&char_dev);
	if (err) {
		dev_err(&pdev->dev, "Can't register chrdev!\n");
		goto release_sscm_irq0;
	}

	set_c2c_device(pdev);

	/* Create sysfs file for C2C debug */
	err = device_create_file(&pdev->dev, &dev_attr_c2c_ctrl);
	if (err) {
		dev_err(&pdev->dev, "Failed to create sysfs for C2C\n");
		goto release_sscm_irq1;
	}

	return 0;

release_sscm_irq1:
	free_irq(sscm_irq1, pdev);

release_sscm_irq0:
	free_irq(sscm_irq0, pdev);

unmap_cp_sscm:
	iounmap(pdata->cp_sscm_addr);

release_cp_sscm:
	release_mem_region(res1->start, resource_size(res1));

unmap_ap_sscm:
	iounmap(pdata->ap_sscm_addr);

release_ap_sscm:
	release_mem_region(res->start, resource_size(res));

	return err;
}

static int __devexit samsung_c2c_remove(struct platform_device *pdev)
{
	/* TODO */
	return 0;
}

#ifdef CONFIG_PM
static int samsung_c2c_suspend(struct platform_device *dev, pm_message_t pm)
{
	/* TODO */
	return 0;
}


static int samsung_c2c_resume(struct platform_device *dev)
{
	/* TODO */
	return 0;
}
#else
#define samsung_c2c_suspend NULL
#define samsung_c2c_resume NULL
#endif

static struct platform_driver samsung_c2c_driver = {
	.probe		= samsung_c2c_probe,
	.remove		= __devexit_p(samsung_c2c_remove),
	.suspend	= samsung_c2c_suspend,
	.resume		= samsung_c2c_resume,
	.driver		= {
		.name	= "samsung-c2c",
		.owner	= THIS_MODULE,
	},
};

static int __init samsung_c2c_init(void)
{
	return platform_driver_register(&samsung_c2c_driver);
}
module_init(samsung_c2c_init);

static void __exit samsung_c2c_exit(void)
{
	platform_driver_unregister(&samsung_c2c_driver);
}
module_exit(samsung_c2c_exit);

MODULE_DESCRIPTION("Samsung C2C driver");
MODULE_AUTHOR("Kisang Lee <kisang80.lee@samsung.com>");
MODULE_LICENSE("GPL");

