/*
 * linux/drivers/video/omap2/dss/dsi.c
 *
 * Copyright (C) 2009 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DSS_SUBSYS_NAME "DSI"

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/i2c/twl.h>

#include <plat/display.h>
#include <plat/clock.h>

#include "dss.h"

/*#define VERBOSE_IRQ*/
#define DSI_CATCH_MISSING_TE

#ifndef CONFIG_ARCH_OMAP4
#define DSI_BASE		0x4804FC00
#else
#define DSI_BASE		0x58004000
#define DSI2_BASE		0x58005000
#endif

struct dsi_reg { u16 idx; };
struct dss_reg { u16 idx; };
extern void __iomem  *dss_base;

#define DSI_REG(idx)		((const struct dsi_reg) { idx })
#define DSS_REG(idx)		((const struct dss_reg) { idx })

#define DSI_SZ_REGS		SZ_1K
/* DSI Protocol Engine */
#define DSS_CONTROL 			DSS_REG(0x0040)

#define DSI_REVISION			DSI_REG(0x0000)
#define DSI_SYSCONFIG			DSI_REG(0x0010)
#define DSI_SYSSTATUS			DSI_REG(0x0014)
#define DSI_IRQSTATUS			DSI_REG(0x0018)
#define DSI_IRQENABLE			DSI_REG(0x001C)
#define DSI_CTRL			DSI_REG(0x0040)
#define DSI_GNQ					DSI_REG(0x0044)
#define DSI_COMPLEXIO_CFG1		DSI_REG(0x0048)
#define DSI_COMPLEXIO_IRQ_STATUS	DSI_REG(0x004C)
#define DSI_COMPLEXIO_IRQ_ENABLE	DSI_REG(0x0050)
#define DSI_CLK_CTRL			DSI_REG(0x0054)
#define DSI_TIMING1			DSI_REG(0x0058)
#define DSI_TIMING2			DSI_REG(0x005C)
#define DSI_VM_TIMING1			DSI_REG(0x0060)
#define DSI_VM_TIMING2			DSI_REG(0x0064)
#define DSI_VM_TIMING3			DSI_REG(0x0068)
#define DSI_CLK_TIMING			DSI_REG(0x006C)
#define DSI_TX_FIFO_VC_SIZE		DSI_REG(0x0070)
#define DSI_RX_FIFO_VC_SIZE		DSI_REG(0x0074)
#define DSI_COMPLEXIO_CFG2		DSI_REG(0x0078)
#define DSI_RX_FIFO_VC_FULLNESS		DSI_REG(0x007C)
#define DSI_VM_TIMING4			DSI_REG(0x0080)
#define DSI_TX_FIFO_VC_EMPTINESS	DSI_REG(0x0084)
#define DSI_VM_TIMING5			DSI_REG(0x0088)
#define DSI_VM_TIMING6			DSI_REG(0x008C)
#define DSI_VM_TIMING7			DSI_REG(0x0090)
#define DSI_STOPCLK_TIMING		DSI_REG(0x0094)
#ifdef CONFIG_ARCH_OMAP4
#define DSI_CTRL2						DSI_REG(0x0098)
#define DSI_VM_TIMING8					DSI_REG(0x009C)

#define DSI_TE_HSYNC_WIDTH(n)			DSI_REG(0x00A0 + (n *0xC))
#define DSI_TE_VSYNC_WIDTH(n)			DSI_REG(0x00A4 + (n *0xC))

#define DSI_TE_HSYNC_NUMBER(n)			DSI_REG(0x00A8 + (n *0xC))
#endif
#define DSI_VC_CTRL(n)			DSI_REG(0x0100 + (n * 0x20))
#define DSI_VC_TE(n)			DSI_REG(0x0104 + (n * 0x20))
#define DSI_VC_LONG_PACKET_HEADER(n)	DSI_REG(0x0108 + (n * 0x20))
#define DSI_VC_LONG_PACKET_PAYLOAD(n)	DSI_REG(0x010C + (n * 0x20))
#define DSI_VC_SHORT_PACKET_HEADER(n)	DSI_REG(0x0110 + (n * 0x20))
#define DSI_VC_IRQSTATUS(n)		DSI_REG(0x0118 + (n * 0x20))
#define DSI_VC_IRQENABLE(n)		DSI_REG(0x011C + (n * 0x20))

/* DSIPHY_SCP */

#define DSI_DSIPHY_CFG0			DSI_REG(0x200 + 0x0000)
#define DSI_DSIPHY_CFG1			DSI_REG(0x200 + 0x0004)
#define DSI_DSIPHY_CFG2			DSI_REG(0x200 + 0x0008)
#define DSI_DSIPHY_CFG5			DSI_REG(0x200 + 0x0014)

#ifdef CONFIG_ARCH_OMAP4
#define DSI_DSIPHY_CFG12			DSI_REG(0x200 + 0x0030)
#define DSI_DSIPHY_CFG14			DSI_REG(0x200 + 0x0038)
#define DSI_DSIPHY_CFG8			DSI_REG(0x200 + 0x0020)
#define DSI_DSIPHY_CFG9			DSI_REG(0x200 + 0x0024)
#endif
/* DSI_PLL_CTRL_SCP */

#define DSI_PLL_CONTROL			DSI_REG(0x300 + 0x0000)
#define DSI_PLL_STATUS			DSI_REG(0x300 + 0x0004)
#define DSI_PLL_GO			DSI_REG(0x300 + 0x0008)
#define DSI_PLL_CONFIGURATION1		DSI_REG(0x300 + 0x000C)
#define DSI_PLL_CONFIGURATION2		DSI_REG(0x300 + 0x0010)
#ifdef CONFIG_ARCH_OMAP4
#define DSI_PLL_CONFIGURATION3		DSI_REG(0x300 + 0x0014)
#define DSI_SSC_CONFIGURATION1		DSI_REG(0x300 + 0x0018)
#define DSI_SSC_CONFIGURATION2		DSI_REG(0x300 + 0x001C)
#define DSI_SSC_CONFIGURATION4		DSI_REG(0x300 + 0x0020)
#endif

#define REG_GET(no, idx, start, end) \
	FLD_GET(dsi_read_reg(no, idx), start, end)

#define REG_FLD_MOD(no, idx, val, start, end) \
	dsi_write_reg(no, idx, FLD_MOD(dsi_read_reg(no, idx), val, start, end))


#define DSICLKCTRLLPCLKDIVISOR    		1<<0| 1<<1| 1<<2 | 1<<3 | 1<<4 | 1<<5 | 1<<6 | 1<<7 | 1<<8 | 1<<9 | 1<<10 | 1<<11 | 1<<12
/* OMAP4: check this! */

/* Global interrupts */
#define DSI_IRQ_VC0		(1 << 0)
#define DSI_IRQ_VC1		(1 << 1)
#define DSI_IRQ_VC2		(1 << 2)
#define DSI_IRQ_VC3		(1 << 3)
#define DSI_IRQ_WAKEUP		(1 << 4)
#define DSI_IRQ_RESYNC		(1 << 5)
#define DSI_IRQ_PLL_LOCK	(1 << 7)
#define DSI_IRQ_PLL_UNLOCK	(1 << 8)
#define DSI_IRQ_PLL_RECALL	(1 << 9)
#define DSI_IRQ_COMPLEXIO_ERR	(1 << 10)
#define DSI_IRQ_HS_TX_TIMEOUT	(1 << 14)
#define DSI_IRQ_LP_RX_TIMEOUT	(1 << 15)
#define DSI_IRQ_TE_TRIGGER	(1 << 16)
#define DSI_IRQ_ACK_TRIGGER	(1 << 17)
#define DSI_IRQ_SYNC_LOST	(1 << 18)
#define DSI_IRQ_LDO_POWER_GOOD	(1 << 19)
#define DSI_IRQ_TA_TIMEOUT	(1 << 20)
#define DSI_IRQ_ERROR_MASK \
	(DSI_IRQ_HS_TX_TIMEOUT | DSI_IRQ_LP_RX_TIMEOUT | DSI_IRQ_SYNC_LOST | \
	DSI_IRQ_TA_TIMEOUT)
#define DSI_IRQ_CHANNEL_MASK	0xf

/* Virtual channel interrupts */
#define DSI_VC_IRQ_CS		(1 << 0)
#define DSI_VC_IRQ_ECC_CORR	(1 << 1)
#define DSI_VC_IRQ_PACKET_SENT	(1 << 2)
#define DSI_VC_IRQ_FIFO_TX_OVF	(1 << 3)
#define DSI_VC_IRQ_FIFO_RX_OVF	(1 << 4)
#define DSI_VC_IRQ_BTA		(1 << 5)
#define DSI_VC_IRQ_ECC_NO_CORR	(1 << 6)
#define DSI_VC_IRQ_FIFO_TX_UDF	(1 << 7)
#define DSI_VC_IRQ_PP_BUSY_CHANGE (1 << 8)
#define DSI_VC_IRQ_ERROR_MASK \
	(DSI_VC_IRQ_CS | DSI_VC_IRQ_ECC_CORR | DSI_VC_IRQ_FIFO_TX_OVF | \
	DSI_VC_IRQ_FIFO_RX_OVF | DSI_VC_IRQ_ECC_NO_CORR | \
	DSI_VC_IRQ_FIFO_TX_UDF)

/* ComplexIO interrupts */
#define DSI_CIO_IRQ_ERRSYNCESC1		(1 << 0)
#define DSI_CIO_IRQ_ERRSYNCESC2		(1 << 1)
#define DSI_CIO_IRQ_ERRSYNCESC3		(1 << 2)
#define DSI_CIO_IRQ_ERRESC1		(1 << 5)
#define DSI_CIO_IRQ_ERRESC2		(1 << 6)
#define DSI_CIO_IRQ_ERRESC3		(1 << 7)
#define DSI_CIO_IRQ_ERRCONTROL1		(1 << 10)
#define DSI_CIO_IRQ_ERRCONTROL2		(1 << 11)
#define DSI_CIO_IRQ_ERRCONTROL3		(1 << 12)
#define DSI_CIO_IRQ_STATEULPS1		(1 << 15)
#define DSI_CIO_IRQ_STATEULPS2		(1 << 16)
#define DSI_CIO_IRQ_STATEULPS3		(1 << 17)
#define DSI_CIO_IRQ_ERRCONTENTIONLP0_1	(1 << 20)
#define DSI_CIO_IRQ_ERRCONTENTIONLP1_1	(1 << 21)
#define DSI_CIO_IRQ_ERRCONTENTIONLP0_2	(1 << 22)
#define DSI_CIO_IRQ_ERRCONTENTIONLP1_2	(1 << 23)
#define DSI_CIO_IRQ_ERRCONTENTIONLP0_3	(1 << 24)
#define DSI_CIO_IRQ_ERRCONTENTIONLP1_3	(1 << 25)
#define DSI_CIO_IRQ_ULPSACTIVENOT_ALL0	(1 << 30)
#define DSI_CIO_IRQ_ULPSACTIVENOT_ALL1	(1 << 31)

#define DSI_DT_DCS_SHORT_WRITE_0	0x05
#define DSI_DT_DCS_SHORT_WRITE_1	0x15
#define DSI_DT_DCS_READ			0x06
#define DSI_DT_SET_MAX_RET_PKG_SIZE	0x37
#define DSI_DT_NULL_PACKET		0x09
#define DSI_DT_DCS_LONG_WRITE		0x39

#define DSI_DT_RX_ACK_WITH_ERR		0x02
#define DSI_DT_RX_DCS_LONG_READ		0x1c
#define DSI_DT_RX_SHORT_READ_1		0x21
#define DSI_DT_RX_SHORT_READ_2		0x22

#define FINT_MAX 2100000
#define FINT_MIN 750000
#define REGN_MAX (1 << 7)
#define REGM_MAX ((1 << 11) - 1)
#define REGM3_MAX (1 << 4)
#define REGM4_MAX (1 << 4)
#define LP_DIV_MAX ((1 << 13) - 1)
#define PWM2ON		0x3
#define PWM2OFF		0x4
#define TOGGLE3		0x92
#define PWDNSTATUS2	0x4


extern void __iomem  *dss_base;
extern void __iomem  *dispc_base;
void __iomem  *gpio_base;
void __iomem  *dsi_base;
#ifdef CONFIG_ARCH_OMAP4
void __iomem  *dsi2_base;
EXPORT_SYMBOL(dsi2_base);
#endif

enum fifo_size {
	DSI_FIFO_SIZE_0		= 0,
	DSI_FIFO_SIZE_32	= 1,
	DSI_FIFO_SIZE_64	= 2,
	DSI_FIFO_SIZE_96	= 3,
	DSI_FIFO_SIZE_128	= 4,
};

enum dsi_vc_mode {
	DSI_VC_MODE_L4 = 0,
	DSI_VC_MODE_VP,
};

struct dsi_update_region {
	bool dirty;
	u16 x, y, w, h;
	struct omap_dss_device *device;
};

static struct dsi_struct
{
	void __iomem	*base;

	struct dsi_clock_info current_cinfo;

	struct regulator *vdds_dsi_reg;

	struct {
		enum dsi_vc_mode mode;
		struct omap_dss_device *dssdev;
		enum fifo_size fifo_size;
		int dest_per;	/* destination peripheral 0-3 */
	} vc[4];

	struct mutex lock;
	struct mutex bus_lock;

	unsigned pll_locked;

	struct completion bta_completion;

	struct task_struct *thread;
	wait_queue_head_t waitqueue;

	spinlock_t update_lock;
	bool framedone_received;
	struct dsi_update_region update_region;
	struct dsi_update_region active_update_region;
	struct completion update_completion;

	enum omap_dss_update_mode user_update_mode;
	enum omap_dss_update_mode update_mode;
	bool te_enabled;
	bool use_ext_te;

#ifdef DSI_CATCH_MISSING_TE
	struct timer_list te_timer;
#endif

	unsigned long cache_req_pck;
	unsigned long cache_clk_freq;
	struct dsi_clock_info cache_cinfo;

	u32		errors;
	spinlock_t	errors_lock;
#ifdef DEBUG
	ktime_t perf_setup_time;
	ktime_t perf_start_time;
	ktime_t perf_start_time_auto;
	int perf_measure_frames;
#endif
	int debug_read;
	int debug_write;
} dsi_1, dsi_2;

#if 0
static unsigned int dsi_perf;
module_param_named(dsi_perf, dsi_perf, bool, 0644);
#endif


static inline void dsi_write_reg(enum dsi lcd_ix,
	const struct dsi_reg idx, u32 val)
{
	(lcd_ix == dsi1) ? __raw_writel(val, dsi_1.base + idx.idx) :
		__raw_writel(val, dsi_2.base + idx.idx);
}

static inline u32 dsi_read_reg(enum dsi lcd_ix, const struct dsi_reg idx)
{
	if (lcd_ix == dsi1)
		return __raw_readl(dsi_1.base + idx.idx);
	else
		return __raw_readl(dsi_2.base + idx.idx);
}

static inline void dss_write_reg(const struct dss_reg idx, u32 val)
{
	__raw_writel(val, dss_base + idx.idx);
}

static inline u32 dss_read_reg(const struct dss_reg idx)
{
	return __raw_readl(dss_base + idx.idx);
}

static void modify_dss_register(const struct dss_reg idx, u32 clearmask,
                                                u32 setmask)
{
	unsigned int val;
		val = dss_read_reg(idx);
		val &= ~(clearmask);
		val |= (setmask);
		dss_write_reg(idx,val);

}


void dsi_save_context(void)
{
}

void dsi_restore_context(void)
{
}

void dsi_bus_lock(enum dsi lcd_ix)
{
	(lcd_ix == dsi1) ? mutex_lock(&dsi_1.bus_lock) :
		mutex_lock(&dsi_2.bus_lock);
}
EXPORT_SYMBOL(dsi_bus_lock);

void dsi_bus_unlock(enum dsi lcd_ix)
{
	(lcd_ix == dsi1) ? mutex_unlock(&dsi_1.bus_lock) :
		mutex_unlock(&dsi_2.bus_lock);
}
EXPORT_SYMBOL(dsi_bus_unlock);

static inline int wait_for_bit_change_delay(enum dsi lcd_ix,
		const struct dsi_reg idx, int bitnum, int value, int delay)
{
	int t = 100000;

	while (REG_GET(lcd_ix, idx, bitnum, bitnum) != value) {
		udelay(delay);
		if (--t == 0)
			return !value;
	}

	return value;
}

static inline int wait_for_bit_change(enum dsi lcd_ix,
	const struct dsi_reg idx, int bitnum, int value)
{
	int t = 100000;

	while (REG_GET(lcd_ix, idx, bitnum, bitnum) != value) {
		if (--t == 0)
			return !value;
	}

	return value;
}

#if 0
static void dsi_perf_mark_setup(enum dsi lcd_ix)
{
	if (lcd_ix == dsi1)
		dsi_1.perf_setup_time = ktime_get();
	else
		dsi_2.perf_setup_time = ktime_get();
}

static void dsi_perf_mark_start(enum dsi lcd_ix)
{
	if (lcd_ix == dsi1)
		dsi_1.perf_start_time = ktime_get();
	else
		dsi_2.perf_start_time = ktime_get();
}

static void dsi_perf_mark_start_auto(enum dsi lcd_ix)
{
if (lcd_ix == dsi1) {
	dsi_1.perf_measure_frames = 0;
	dsi_1.perf_start_time_auto = ktime_get();
	} else {
		dsi_2.perf_measure_frames = 0;
		dsi_2.perf_start_time_auto = ktime_get();
	}
}
static void dsi_perf_show(enum dsi lcd_ix, const char *name)
{
	ktime_t t, setup_time, trans_time;
	u32 total_bytes;
	u32 setup_us, trans_us, total_us;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	if (!dsi_perf)
		return;

	if (p_dsi->update_mode == OMAP_DSS_UPDATE_DISABLED)
		return;

	t = ktime_get();

	setup_time = ktime_sub(p_dsi->perf_start_time, p_dsi->perf_setup_time);
	setup_us = (u32)ktime_to_us(setup_time);
	if (setup_us == 0)
		setup_us = 1;

	trans_time = ktime_sub(t, p_dsi->perf_start_time);
	trans_us = (u32)ktime_to_us(trans_time);
	if (trans_us == 0)
		trans_us = 1;

	total_us = setup_us + trans_us;

	total_bytes = p_dsi->active_update_region.w *
		p_dsi->active_update_region.h *
		p_dsi->active_update_region.device->ctrl.pixel_size / 8;

	if (p_dsi->update_mode == OMAP_DSS_UPDATE_AUTO) {
		static u32 s_total_trans_us, s_total_setup_us;
		static u32 s_min_trans_us = 0xffffffff, s_min_setup_us;
		static u32 s_max_trans_us, s_max_setup_us;
		const int numframes = 100;
		ktime_t total_time_auto;
		u32 total_time_auto_us;

		p_dsi->perf_measure_frames++;

		if (setup_us < s_min_setup_us)
			s_min_setup_us = setup_us;

		if (setup_us > s_max_setup_us)
			s_max_setup_us = setup_us;

		s_total_setup_us += setup_us;

		if (trans_us < s_min_trans_us)
			s_min_trans_us = trans_us;

		if (trans_us > s_max_trans_us)
			s_max_trans_us = trans_us;

		s_total_trans_us += trans_us;

		if (p_dsi->perf_measure_frames < numframes)
			return;

		total_time_auto = ktime_sub(t, p_dsi->perf_start_time_auto);
		total_time_auto_us = (u32)ktime_to_us(total_time_auto);

		printk(KERN_INFO "DSI(%s): %u fps, setup %u/%u/%u, "
				"trans %u/%u/%u\n",
				name,
				1000 * 1000 * numframes / total_time_auto_us,
				s_min_setup_us,
				s_max_setup_us,
				s_total_setup_us / numframes,
				s_min_trans_us,
				s_max_trans_us,
				s_total_trans_us / numframes);

		s_total_setup_us = 0;
		s_min_setup_us = 0xffffffff;
		s_max_setup_us = 0;
		s_total_trans_us = 0;
		s_min_trans_us = 0xffffffff;
		s_max_trans_us = 0;
		dsi_perf_mark_start_auto(lcd_ix);
	} else {
		printk(KERN_INFO "DSI(%s): %u us + %u us = %u us (%uHz), "
				"%u bytes, %u kbytes/sec\n",
				name,
				setup_us,
				trans_us,
				total_us,
				1000*1000 / total_us,
				total_bytes,
				total_bytes * 1000 / total_us);
	}
}
#else
#define dsi_perf_mark_setup(lcd_ix)
#define dsi_perf_mark_start(lcd_ix)
#define dsi_perf_mark_start_auto(lcd_ix)
#define dsi_perf_show(lcd_ix, x)
#endif

static void print_irq_status(u32 status)
{
#ifndef VERBOSE_IRQ
	if ((status & ~DSI_IRQ_CHANNEL_MASK) == 0)
		return;
#endif
	printk(KERN_DEBUG "DSI IRQ: 0x%x: ", status);

#define PIS(x) \
	if (status & DSI_IRQ_##x) \
		printk(#x " ");
#ifdef VERBOSE_IRQ
	PIS(VC0);
	PIS(VC1);
	PIS(VC2);
	PIS(VC3);
#endif
	PIS(WAKEUP);
	PIS(RESYNC);
	PIS(PLL_LOCK);
	PIS(PLL_UNLOCK);
	PIS(PLL_RECALL);
	PIS(COMPLEXIO_ERR);
	PIS(HS_TX_TIMEOUT);
	PIS(LP_RX_TIMEOUT);
	PIS(TE_TRIGGER);
	PIS(ACK_TRIGGER);
	PIS(SYNC_LOST);
	PIS(LDO_POWER_GOOD);
	PIS(TA_TIMEOUT);
#undef PIS

	printk("\n");
}

static void print_irq_status_vc(int channel, u32 status)
{
#ifndef VERBOSE_IRQ
	if ((status & ~DSI_VC_IRQ_PACKET_SENT) == 0)
		return;
#endif
	printk(KERN_DEBUG "DSI VC(%d) IRQ 0x%x: ", channel, status);

#define PIS(x) \
	if (status & DSI_VC_IRQ_##x) \
		printk(#x " ");
	PIS(CS);
	PIS(ECC_CORR);
#ifdef VERBOSE_IRQ
	PIS(PACKET_SENT);
#endif
	PIS(FIFO_TX_OVF);
	PIS(FIFO_RX_OVF);
	PIS(BTA);
	PIS(ECC_NO_CORR);
	PIS(FIFO_TX_UDF);
	PIS(PP_BUSY_CHANGE);
#undef PIS
	printk("\n");
}

static void print_irq_status_cio(u32 status)
{
	printk(KERN_DEBUG "DSI CIO IRQ 0x%x: ", status);

#define PIS(x) \
	if (status & DSI_CIO_IRQ_##x) \
		printk(#x " ");
	PIS(ERRSYNCESC1);
	PIS(ERRSYNCESC2);
	PIS(ERRSYNCESC3);
	PIS(ERRESC1);
	PIS(ERRESC2);
	PIS(ERRESC3);
	PIS(ERRCONTROL1);
	PIS(ERRCONTROL2);
	PIS(ERRCONTROL3);
	PIS(STATEULPS1);
	PIS(STATEULPS2);
	PIS(STATEULPS3);
	PIS(ERRCONTENTIONLP0_1);
	PIS(ERRCONTENTIONLP1_1);
	PIS(ERRCONTENTIONLP0_2);
	PIS(ERRCONTENTIONLP1_2);
	PIS(ERRCONTENTIONLP0_3);
	PIS(ERRCONTENTIONLP1_3);
	PIS(ULPSACTIVENOT_ALL0);
	PIS(ULPSACTIVENOT_ALL1);
#undef PIS

	printk("\n");
}

static int debug_irq;

/*dsi irq line needs to be activated on omap4*/
void dsi_irq_handler(enum dsi lcd_ix)
{
	u32 irqstatus, vcstatus, ciostatus;
	int i;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	irqstatus = dsi_read_reg(lcd_ix, DSI_IRQSTATUS);

	if (irqstatus & DSI_IRQ_ERROR_MASK) {
		DSSERR("DSI error, irqstatus %x\n", irqstatus);
		print_irq_status(irqstatus);
		spin_lock(&(p_dsi->errors_lock));
		p_dsi->errors |= irqstatus & DSI_IRQ_ERROR_MASK;
		spin_unlock(&(p_dsi->errors_lock));
	} else if (debug_irq) {
		print_irq_status(irqstatus);
	}

#ifdef DSI_CATCH_MISSING_TE
	if (irqstatus & DSI_IRQ_TE_TRIGGER)
		del_timer(&(p_dsi->te_timer));
#endif

	for (i = 0; i < 4; ++i) {
		if ((irqstatus & (1<<i)) == 0)
			continue;

		vcstatus = dsi_read_reg(lcd_ix, DSI_VC_IRQSTATUS(i));

		if (vcstatus & DSI_VC_IRQ_BTA)
			complete(&(p_dsi->bta_completion));

		if (vcstatus & DSI_VC_IRQ_ERROR_MASK) {
			DSSERR("DSI VC(%d) error, vc irqstatus %x\n",
				       i, vcstatus);
			print_irq_status_vc(i, vcstatus);
		} else if (debug_irq) {
			print_irq_status_vc(i, vcstatus);
		}

		dsi_write_reg(lcd_ix, DSI_VC_IRQSTATUS(i), vcstatus);
		/* flush posted write */
		dsi_read_reg(lcd_ix, DSI_VC_IRQSTATUS(i));
	}

	if (irqstatus & DSI_IRQ_COMPLEXIO_ERR) {
		ciostatus = dsi_read_reg(lcd_ix, DSI_COMPLEXIO_IRQ_STATUS);

		dsi_write_reg(lcd_ix, DSI_COMPLEXIO_IRQ_STATUS, ciostatus);
		/* flush posted write */
		dsi_read_reg(lcd_ix, DSI_COMPLEXIO_IRQ_STATUS);

		DSSERR("DSI CIO error, cio irqstatus %x\n", ciostatus);
		print_irq_status_cio(ciostatus);
	}

	dsi_write_reg(lcd_ix, DSI_IRQSTATUS, irqstatus & ~DSI_IRQ_CHANNEL_MASK);
	/* flush posted write */
	dsi_read_reg(lcd_ix, DSI_IRQSTATUS);
}


static void _dsi_initialize_irq(enum dsi lcd_ix)
{
	u32 l;
	int i;

	/* disable all interrupts */
	dsi_write_reg(lcd_ix, DSI_IRQENABLE, 0);
	for (i = 0; i < 4; ++i)
		dsi_write_reg(lcd_ix, DSI_VC_IRQENABLE(i), 0);
	dsi_write_reg(lcd_ix, DSI_COMPLEXIO_IRQ_ENABLE, 0);

	/* clear interrupt status */
	l = dsi_read_reg(lcd_ix, DSI_IRQSTATUS);
	dsi_write_reg(lcd_ix, DSI_IRQSTATUS, l & ~DSI_IRQ_CHANNEL_MASK);

	for (i = 0; i < 4; ++i) {
		l = dsi_read_reg(lcd_ix, DSI_VC_IRQSTATUS(i));
		dsi_write_reg(lcd_ix, DSI_VC_IRQSTATUS(i), l);
	}

	l = dsi_read_reg(lcd_ix, DSI_COMPLEXIO_IRQ_STATUS);
	dsi_write_reg(lcd_ix, DSI_COMPLEXIO_IRQ_STATUS, l);

	/* enable error irqs */
	l = DSI_IRQ_ERROR_MASK;
#ifdef DSI_CATCH_MISSING_TE
	l |= DSI_IRQ_TE_TRIGGER;
#endif
	dsi_write_reg(lcd_ix, DSI_IRQENABLE, l);

	l = DSI_VC_IRQ_ERROR_MASK;
	for (i = 0; i < 4; ++i)
		dsi_write_reg(lcd_ix, DSI_VC_IRQENABLE(i), l);

	/* XXX zonda responds incorrectly, causing control error:
	   Exit from LP-ESC mode to LP11 uses wrong transition states on the
	   data lines LP0 and LN0. */
	   if (!cpu_is_omap44xx())
		dsi_write_reg(lcd_ix, DSI_COMPLEXIO_IRQ_ENABLE,
			-1 & (~DSI_CIO_IRQ_ERRCONTROL2));
}

static u32 dsi_get_errors(enum dsi lcd_ix)
{
	unsigned long flags;
	u32 e;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	spin_lock_irqsave(&(p_dsi->errors_lock), flags);
	e = p_dsi->errors;
	p_dsi->errors = 0;
	spin_unlock_irqrestore(&(p_dsi->errors_lock), flags);
	return e;
}

static void dsi_vc_enable_bta_irq(enum dsi lcd_ix, int channel)
{
	u32 l;

	dsi_write_reg(lcd_ix, DSI_VC_IRQSTATUS(channel), DSI_VC_IRQ_BTA);

	l = dsi_read_reg(lcd_ix, DSI_VC_IRQENABLE(channel));
	l |= DSI_VC_IRQ_BTA;
	dsi_write_reg(lcd_ix, DSI_VC_IRQENABLE(channel), l);
}

static void dsi_vc_disable_bta_irq(enum dsi lcd_ix, int channel)
{
	u32 l;

	l = dsi_read_reg(lcd_ix, DSI_VC_IRQENABLE(channel));
	l &= ~DSI_VC_IRQ_BTA;
	dsi_write_reg(lcd_ix, DSI_VC_IRQENABLE(channel), l);
}

/* DSI func clock. this could also be DSI2_PLL_FCLK */
static inline void enable_clocks(bool enable)
{
	if (enable)
		dss_clk_enable(DSS_CLK_ICK | DSS_CLK_FCK1);
	else
		dss_clk_disable(DSS_CLK_ICK | DSS_CLK_FCK1);
}

/* source clock for DSI PLL. this could also be PCLKFREE */
static inline void dsi_enable_pll_clock(enum dsi lcd_ix, bool enable)
{
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	if (enable)
		dss_clk_enable(DSS_CLK_FCK2);
	else
		dss_clk_disable(DSS_CLK_FCK2);

	if (enable && p_dsi->pll_locked) {
		if (wait_for_bit_change(lcd_ix, DSI_PLL_STATUS, 1, 1) != 1)
			DSSERR("cannot lock PLL when enabling clocks\n");
	}
}

#if 0
static void _dsi_print_reset_status(enum dsi lcd_ix)
{
	u32 l;

	if (!dss_debug)
		return;

	/* A dummy read using the SCP interface to any DSIPHY register is
	 * required after DSIPHY reset to complete the reset of the DSI complex
	 * I/O. */
	l = dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG5);

	printk(KERN_DEBUG "DSI resets: ");

	l = dsi_read_reg(lcd_ix, DSI_PLL_STATUS);
	printk("PLL (%d) ", FLD_GET(l, 0, 0));

	l = dsi_read_reg(lcd_ix, DSI_COMPLEXIO_CFG1);
	printk("CIO (%d) ", FLD_GET(l, 29, 29));

	l = dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG5);
	printk("PHY (%x, %d, %d, %d)\n",
			FLD_GET(l, 28, 26),
			FLD_GET(l, 29, 29),
			FLD_GET(l, 30, 30),
			FLD_GET(l, 31, 31));

}
#else
#define _dsi_print_reset_status(lcd_ix)
#endif

static inline int dsi_if_enable(enum dsi lcd_ix, bool enable)
{
	DSSDBG("dsi_if_enable(%d, %d)\n", lcd_ix, enable);

	enable = enable ? 1 : 0;
	REG_FLD_MOD(lcd_ix, DSI_CTRL, enable, 0, 0); /* IF_EN */

	if (wait_for_bit_change(lcd_ix, DSI_CTRL, 0, enable) != enable) {
			DSSERR("Failed to set dsi_if_enable to %d\n", enable);
			return -EIO;
	}

	return 0;
}

unsigned long dsi_get_dsi1_pll_rate(void)
{
	return dsi_1.current_cinfo.dsi1_pll_fclk;
}

static unsigned long dsi_get_dsi2_pll_rate(void)
{
	return dsi_2.current_cinfo.dsi2_pll_fclk;
}

static unsigned long dsi_get_txbyteclkhs(enum dsi lcd_ix)
{
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	return p_dsi->current_cinfo.clkin4ddr / 16;
}

static unsigned long dsi_fclk_rate(void)
{
	unsigned long r;

	if (dss_get_dsi_clk_source() == 0) {
		/* DSI FCLK source is DSS1_ALWON_FCK, which is dss1_fck */
		r = dss_clk_get_rate(DSS_CLK_FCK1);
	} else {
		/* DSI FCLK source is DSI2_PLL_FCLK */
		r = dsi_get_dsi2_pll_rate();
	}

	return r;
}

static int dsi_set_lp_clk_divisor(struct omap_dss_device *dssdev)
{
	unsigned long dsi_fclk;
	unsigned lp_clk_div;
	unsigned long lp_clk;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	lp_clk_div = dssdev->phy.dsi.div.lp_clk_div;

	if (lp_clk_div == 0 || lp_clk_div > LP_DIV_MAX)
		return -EINVAL;

	dsi_fclk = dsi_fclk_rate();

	lp_clk = dsi_fclk / 2 / lp_clk_div;

	DSSDBG("LP_CLK_DIV %u, LP_CLK %lu\n", lp_clk_div, lp_clk);
	p_dsi->current_cinfo.lp_clk = lp_clk;
	p_dsi->current_cinfo.lp_clk_div = lp_clk_div;

	/* TODO: chnage the values in board file to get n=6 here */
	REG_FLD_MOD(lcd_ix, DSI_CLK_CTRL,
	(cpu_is_omap44xx()) ? 6 : lp_clk_div, 12, 0);	/* LP_CLK_DIVISOR */

	REG_FLD_MOD(lcd_ix, DSI_CLK_CTRL, 1,
			21, 21);		/* LP_RX_SYNCHRO_ENABLE */

	return 0;
}


enum dsi_pll_power_state {
	DSI_PLL_POWER_OFF	= 0x0,
	DSI_PLL_POWER_ON_HSCLK	= 0x1,
	DSI_PLL_POWER_ON_ALL	= 0x2,
	DSI_PLL_POWER_ON_DIV	= 0x3,
};

static int dsi_pll_power(enum dsi lcd_ix, enum dsi_pll_power_state state)
{
	int t = 0;

	REG_FLD_MOD(lcd_ix, DSI_CLK_CTRL, state, 31, 30);	/* PLL_PWR_CMD */

	/* PLL_PWR_STATUS */
	while (FLD_GET(dsi_read_reg(lcd_ix, DSI_CLK_CTRL), 29, 28) != state) {
		udelay(1);
		if (t++ > 1000) {
			DSSERR("Failed to set DSI PLL power mode to %d\n",
					state);
			return -ENODEV;
		}
	}

	return 0;
}

/* calculate clock rates using dividers in cinfo */
static int dsi_calc_clock_rates(struct dsi_clock_info *cinfo)
{
	if (cinfo->regn == 0 || cinfo->regn > REGN_MAX)
		return -EINVAL;

	if (cinfo->regm == 0 || cinfo->regm > REGM_MAX)
		return -EINVAL;

	if (cinfo->regm3 > REGM3_MAX)
		return -EINVAL;

	if (cinfo->regm4 > REGM4_MAX)
		return -EINVAL;

	if (cinfo->use_dss2_fck) {
		cinfo->clkin = dss_clk_get_rate(DSS_CLK_FCK2);
		/* XXX it is unclear if highfreq should be used
		 * with DSS2_FCK source also */
		cinfo->highfreq = 0;
	} else {
		/* TODO: Add support for LCD2 */
		cinfo->clkin = dispc_pclk_rate(OMAP_DSS_CHANNEL_LCD);

		if (cinfo->clkin < 32000000)
			cinfo->highfreq = 0;
		else
			cinfo->highfreq = 1;
	}

	cinfo->fint = cinfo->clkin / (cinfo->regn * (cinfo->highfreq ? 2 : 1));

	if (cinfo->fint > FINT_MAX || cinfo->fint < FINT_MIN)
		return -EINVAL;

	cinfo->clkin4ddr = 2 * cinfo->regm * cinfo->fint;

	if (cinfo->clkin4ddr > 1800 * 1000 * 1000)
		return -EINVAL;

	if (cinfo->regm3 > 0)
		cinfo->dsi1_pll_fclk = cinfo->clkin4ddr / cinfo->regm3;
	else
		cinfo->dsi1_pll_fclk = 0;

	if (cinfo->regm4 > 0)
		cinfo->dsi2_pll_fclk = cinfo->clkin4ddr / cinfo->regm4;
	else
		cinfo->dsi2_pll_fclk = 0;

	return 0;
}

int dsi_pll_calc_clock_div_pck(enum dsi lcd_ix, bool is_tft, unsigned long req_pck,
		struct dsi_clock_info *dsi_cinfo,
		struct dispc_clock_info *dispc_cinfo)
{
	struct dsi_clock_info cur, best;
	struct dispc_clock_info best_dispc;
	int min_fck_per_pck;
	int match = 0;
	unsigned long dss_clk_fck2;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	dss_clk_fck2 = dss_clk_get_rate(DSS_CLK_FCK2);

	if (req_pck == p_dsi->cache_req_pck &&
			p_dsi->cache_cinfo.clkin == dss_clk_fck2) {
		DSSDBG("DSI clock info found from cache\n");
		*dsi_cinfo = p_dsi->cache_cinfo;
		dispc_find_clk_divs(is_tft, req_pck, dsi_cinfo->dsi1_pll_fclk,
				dispc_cinfo);
		return 0;
	}

	min_fck_per_pck = CONFIG_OMAP2_DSS_MIN_FCK_PER_PCK;

	if (min_fck_per_pck &&
		req_pck * min_fck_per_pck > DISPC_MAX_FCK) {
		DSSERR("Requested pixel clock not possible with the current "
				"OMAP2_DSS_MIN_FCK_PER_PCK setting. Turning "
				"the constraint off.\n");
		min_fck_per_pck = 0;
	}

	DSSDBG("dsi_pll_calc\n");

retry:
	memset(&best, 0, sizeof(best));
	memset(&best_dispc, 0, sizeof(best_dispc));

	memset(&cur, 0, sizeof(cur));
	cur.clkin = dss_clk_fck2;
	cur.use_dss2_fck = 1;
	cur.highfreq = 0;

	/* no highfreq: 0.75MHz < Fint = clkin / regn < 2.1MHz */
	/* highfreq: 0.75MHz < Fint = clkin / (2*regn) < 2.1MHz */
	/* To reduce PLL lock time, keep Fint high (around 2 MHz) */
	for (cur.regn = 1; cur.regn < REGN_MAX; ++cur.regn) {
		if (cur.highfreq == 0)
			cur.fint = cur.clkin / cur.regn;
		else
			cur.fint = cur.clkin / (2 * cur.regn);

		if (cur.fint > FINT_MAX || cur.fint < FINT_MIN)
			continue;

		/* DSIPHY(MHz) = (2 * regm / regn) * (clkin / (highfreq + 1)) */
		for (cur.regm = 1; cur.regm < REGM_MAX; ++cur.regm) {
			unsigned long a, b;

			a = 2 * cur.regm * (cur.clkin/1000);
			b = cur.regn * (cur.highfreq + 1);
			cur.clkin4ddr = a / b * 1000;

			if (cur.clkin4ddr > 1800 * 1000 * 1000)
				break;

			/* DSI1_PLL_FCLK(MHz) = DSIPHY(MHz) / regm3  < 173MHz */
			for (cur.regm3 = 1; cur.regm3 < REGM3_MAX;
					++cur.regm3) {
				struct dispc_clock_info cur_dispc;
				cur.dsi1_pll_fclk = cur.clkin4ddr / cur.regm3;

				/* this will narrow down the search a bit,
				 * but still give pixclocks below what was
				 * requested */
				if (cur.dsi1_pll_fclk  < req_pck)
					break;

				if (cur.dsi1_pll_fclk > DISPC_MAX_FCK)
					continue;

				if (min_fck_per_pck &&
					cur.dsi1_pll_fclk <
						req_pck * min_fck_per_pck)
					continue;

				match = 1;

				dispc_find_clk_divs(is_tft, req_pck,
						cur.dsi1_pll_fclk,
						&cur_dispc);

				if (abs(cur_dispc.pck - req_pck) <
						abs(best_dispc.pck - req_pck)) {
					best = cur;
					best_dispc = cur_dispc;

					if (cur_dispc.pck == req_pck)
						goto found;
				}
			}
		}
	}
found:
	if (!match) {
		if (min_fck_per_pck) {
			DSSERR("Could not find suitable clock settings.\n"
					"Turning FCK/PCK constraint off and"
					"trying again.\n");
			min_fck_per_pck = 0;
			goto retry;
		}

		DSSERR("Could not find suitable clock settings.\n");

		return -EINVAL;
	}

	/* DSI2_PLL_FCLK (regm4) is not used. Set it to something sane. */
	best.regm4 = best.clkin4ddr / 48000000;
	if (best.regm4 > REGM4_MAX)
		best.regm4 = REGM4_MAX;
	else if (best.regm4 == 0)
		best.regm4 = 1;
	best.dsi2_pll_fclk = best.clkin4ddr / best.regm4;

	if (dsi_cinfo)
		*dsi_cinfo = best;
	if (dispc_cinfo)
		*dispc_cinfo = best_dispc;

	p_dsi->cache_req_pck = req_pck;
	p_dsi->cache_clk_freq = 0;
	p_dsi->cache_cinfo = best;

	return 0;
}

int dsi_pll_set_clock_div(enum dsi lcd_ix, struct dsi_clock_info *cinfo)
{
	int r = 0;
	u32 l;
	int f;
	struct dsi_struct *p_dsi;
	DSSDBGF();
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	p_dsi->current_cinfo.fint = cinfo->fint;
	p_dsi->current_cinfo.clkin4ddr = cinfo->clkin4ddr;
	p_dsi->current_cinfo.dsi1_pll_fclk = cinfo->dsi1_pll_fclk;
	p_dsi->current_cinfo.dsi2_pll_fclk = cinfo->dsi2_pll_fclk;

	p_dsi->current_cinfo.regn = cinfo->regn;
	p_dsi->current_cinfo.regm = cinfo->regm;
	p_dsi->current_cinfo.regm3 = cinfo->regm3;
	p_dsi->current_cinfo.regm4 = cinfo->regm4;

	DSSDBG("DSI Fint %ld\n", cinfo->fint);

	DSSDBG("clkin (%s) rate %ld, highfreq %d\n",
			cinfo->use_dss2_fck ? "dss2_fck" : "pclkfree",
			cinfo->clkin,
			cinfo->highfreq);

	/* DSIPHY == CLKIN4DDR */
	DSSDBG("CLKIN4DDR = 2 * %d / %d * %lu / %d = %lu\n",
			cinfo->regm,
			cinfo->regn,
			cinfo->clkin,
			cinfo->highfreq + 1,
			cinfo->clkin4ddr);

	DSSDBG("Data rate on 1 DSI lane %ld Mbps\n",
			cinfo->clkin4ddr / 1000 / 1000 / 2);

	DSSDBG("Clock lane freq %ld Hz\n", cinfo->clkin4ddr / 4);

	DSSDBG("regm3 = %d, dsi1_pll_fclk = %lu\n",
			cinfo->regm3, cinfo->dsi1_pll_fclk);
	DSSDBG("regm4 = %d, dsi2_pll_fclk = %lu\n",
			cinfo->regm4, cinfo->dsi2_pll_fclk);

	REG_FLD_MOD(lcd_ix, DSI_PLL_CONTROL, 0, 0, 0); /* DSI_PLL_AUTOMODE = manual */

if (!cpu_is_omap44xx()) {
	l = dsi_read_reg(lcd_ix, DSI_PLL_CONFIGURATION1);
	l = FLD_MOD(l, 1, 0, 0);		/* DSI_PLL_STOPMODE */
	l = FLD_MOD(l, cinfo->regn - 1, 7, 1);	/* DSI_PLL_REGN */
	l = FLD_MOD(l, cinfo->regm, 18, 8);	/* DSI_PLL_REGM */
	l = FLD_MOD(l, cinfo->regm3 > 0 ? cinfo->regm3 - 1 : 0,
			22, 19);		/* DSI_CLOCK_DIV */
	l = FLD_MOD(l, cinfo->regm4 > 0 ? cinfo->regm4 - 1 : 0,
			26, 23);		/* DSIPROTO_CLOCK_DIV */
	dsi_write_reg(lcd_ix, DSI_PLL_CONFIGURATION1, l);

	BUG_ON(cinfo->fint < 750000 || cinfo->fint > 2100000);
	if (cinfo->fint < 1000000)
		f = 0x3;
	else if (cinfo->fint < 1250000)
		f = 0x4;
	else if (cinfo->fint < 1500000)
		f = 0x5;
	else if (cinfo->fint < 1750000)
		f = 0x6;
	else
		f = 0x7;

	l = dsi_read_reg(lcd_ix, DSI_PLL_CONFIGURATION2);
	l = FLD_MOD(l, f, 4, 1);		/* DSI_PLL_FREQSEL */
	l = FLD_MOD(l, cinfo->use_dss2_fck ? 0 : 1,
			11, 11);		/* DSI_PLL_CLKSEL */
	l = FLD_MOD(l, cinfo->highfreq,
			12, 12);		/* DSI_PLL_HIGHFREQ */
	l = FLD_MOD(l, 1, 13, 13);		/* DSI_PLL_REFEN */
	l = FLD_MOD(l, 0, 14, 14);		/* DSIPHY_CLKINEN */
	l = FLD_MOD(l, 1, 20, 20);		/* DSI_HSDIVBYPASS */
	dsi_write_reg(lcd_ix, DSI_PLL_CONFIGURATION2, l);

	} else {

	l = dsi_read_reg(lcd_ix, DSI_PLL_CONFIGURATION1);
	l = FLD_MOD(l, 1, 0, 0);					/* DSI_PLL_STOPMODE */
	l = FLD_MOD(l, cinfo->regn - 1, 8, 1);		/* DSI_PLL_REGN */
	l = FLD_MOD(l, cinfo->regm, 20, 9);			/* DSI_PLL_REGM */
	l = FLD_MOD(l, cinfo->regm3 - 1, 25, 21);	/* DSI_CLOCK_DIV */
	l = FLD_MOD(l, cinfo->regm4 - 1, 30, 26);/* DSIPROTO_CLOCK_DIV */
	dsi_write_reg(lcd_ix, DSI_PLL_CONFIGURATION1, l);

	l = dsi_read_reg(lcd_ix, DSI_PLL_CONFIGURATION2);
	l = FLD_MOD(l, cinfo->use_dss2_fck ? 0 : 1, 11, 11);/* DSI_PLL_CLKSEL */
	l = FLD_MOD(l, cinfo->highfreq, 12, 12);	/* DSI_PLL_HIGHFREQ */
	l = FLD_MOD(l, 1, 13, 13);		/* DSI_PLL_REFEN */
	l = FLD_MOD(l, 0, 14, 14);		/* DSIPHY_CLKINEN */
	l = FLD_MOD(l, 1, 20, 20);		/* DSI_HSDIVBYPASS */
	l = FLD_MOD(l, 3, 22, 21);		/*DSI_REF_SEL*/
	dsi_write_reg(lcd_ix, DSI_PLL_CONFIGURATION2, l);

	}

	REG_FLD_MOD(lcd_ix, DSI_PLL_GO, 1, 0, 0); /* DSI_PLL_GO */

	if (wait_for_bit_change(lcd_ix, DSI_PLL_GO, 0, 0) != 0) {
		DSSERR("dsi pll go bit not going down.\n");
		r = -EIO;
		goto err;
	}

	if (wait_for_bit_change(lcd_ix, DSI_PLL_STATUS, 1, 1) != 1) {
		DSSERR("cannot lock PLL\n");
		r = -EIO;
		goto err;
	}
	printk(KERN_DEBUG "\n PLL is locked ");
	printk(KERN_DEBUG "\n DSI_PLL_STATUS = 0x%X ", dsi_read_reg(lcd_ix, DSI_PLL_STATUS));
	p_dsi->pll_locked = 1;

	l = dsi_read_reg(lcd_ix, DSI_PLL_CONFIGURATION2);
	l = FLD_MOD(l, 0, 0, 0);	/* DSI_PLL_IDLE */
	l = FLD_MOD(l, 0, 5, 5);	/* DSI_PLL_PLLLPMODE */
	l = FLD_MOD(l, 0, 6, 6);	/* DSI_PLL_LOWCURRSTBY */
	l = FLD_MOD(l, 0, 7, 7);	/* DSI_PLL_TIGHTPHASELOCK */
	l = FLD_MOD(l, 0, 8, 8);	/* DSI_PLL_DRIFTGUARDEN */
	l = FLD_MOD(l, 0, 10, 9);	/* DSI_PLL_LOCKSEL */
	l = FLD_MOD(l, 1, 13, 13);	/* DSI_PLL_REFEN */
	l = FLD_MOD(l, 1, 14, 14);	/* DSIPHY_CLKINEN */
	l = FLD_MOD(l, 0, 15, 15);	/* DSI_BYPASSEN */
	l = FLD_MOD(l, 1, 16, 16);	/* DSS_CLOCK_EN */
	l = FLD_MOD(l, 0, 17, 17);	/* DSS_CLOCK_PWDN */
	l = FLD_MOD(l, 1, 18, 18);	/* DSI_PROTO_CLOCK_EN */
	l = FLD_MOD(l, 0, 19, 19);	/* DSI_PROTO_CLOCK_PWDN */
	l = FLD_MOD(l, 0, 20, 20);	/* DSI_HSDIVBYPASS */
	dsi_write_reg(lcd_ix, DSI_PLL_CONFIGURATION2, l);

	DSSDBG("PLL config done\n");
err:
	return r;
}

int dsi_pll_init(enum dsi lcd_ix, struct omap_dss_device *dssdev,
		bool enable_hsclk, bool enable_hsdiv)
{
	int r = 0;
	enum dsi_pll_power_state pwstate;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	DSSDBG("PLL init\n");

	enable_clocks(1);
	dsi_enable_pll_clock(lcd_ix, 1);

if (!cpu_is_omap44xx()) {

	r = regulator_enable(p_dsi->vdds_dsi_reg);
	if (r)
		goto err0;
}

	/* XXX PLL does not come out of reset without this... */

	if (!cpu_is_omap44xx())
		dispc_pck_free_enable(1);


	if (wait_for_bit_change(lcd_ix, DSI_PLL_STATUS, 0, 1) != 1) {
		DSSERR("PLL not coming out of reset.\n");
		r = -ENODEV;
		goto err1;
	}

	/* XXX ... but if left on, we get problems when planes do not
	 * fill the whole display. No idea about this */
	if (!cpu_is_omap44xx()) {

	dispc_pck_free_enable(0);

	if (enable_hsclk && enable_hsdiv)
		pwstate = DSI_PLL_POWER_ON_ALL;
	else if (enable_hsclk)
		pwstate = DSI_PLL_POWER_ON_HSCLK;
	else if (enable_hsdiv)
		pwstate = DSI_PLL_POWER_ON_DIV;
	else
		pwstate = DSI_PLL_POWER_OFF;

	} else
		pwstate = DSI_PLL_POWER_ON_ALL;

	r = dsi_pll_power(lcd_ix, pwstate);

	if (r)
		goto err1;

	DSSDBG("PLL init done\n");

	return 0;

err1:

if (!cpu_is_omap44xx())
	regulator_disable(p_dsi->vdds_dsi_reg);

err0:
	enable_clocks(0);
	dsi_enable_pll_clock(lcd_ix, 0);
	return r;
}

void dsi_pll_uninit(enum dsi lcd_ix)
{
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	enable_clocks(0);
	dsi_enable_pll_clock(lcd_ix, 0);

	p_dsi->pll_locked = 0;
	dsi_pll_power(lcd_ix, DSI_PLL_POWER_OFF);

if (!cpu_is_omap44xx())
	regulator_disable(p_dsi->vdds_dsi_reg);

	DSSDBG("PLL uninit done\n");
}

void dsi_dump_clocks(enum dsi lcd_ix, struct seq_file *s)
{
	int clksel;
	struct dsi_struct *p_dsi;
	struct dsi_clock_info *cinfo;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	cinfo = &p_dsi->current_cinfo;

	enable_clocks(1);

	clksel = REG_GET(lcd_ix, DSI_PLL_CONFIGURATION2, 11, 11);

	seq_printf(s,	"- DSI PLL -\n");

	seq_printf(s,	"dsi pll source = %s\n",
			clksel == 0 ?
			"dss2_alwon_fclk" : "pclkfree");

	seq_printf(s,	"Fint\t\t%-16luregn %u\n", cinfo->fint, cinfo->regn);

	seq_printf(s,	"CLKIN4DDR\t%-16luregm %u\n",
			cinfo->clkin4ddr, cinfo->regm);

	seq_printf(s,	"dsi1_pll_fck\t%-16luregm3 %u\t(%s)\n",
			cinfo->dsi1_pll_fclk,
			cinfo->regm3,
			dss_get_dispc_clk_source() == 0 ? "off" : "on");

	seq_printf(s,	"dsi2_pll_fck\t%-16luregm4 %u\t(%s)\n",
			cinfo->dsi2_pll_fclk,
			cinfo->regm4,
			dss_get_dsi_clk_source() == 0 ? "off" : "on");

	seq_printf(s,	"- DSI -\n");

	seq_printf(s,	"dsi fclk source = %s\n",
			dss_get_dsi_clk_source() == 0 ?
			"dss1_alwon_fclk" : "dsi2_pll_fclk");

	seq_printf(s,	"DSI_FCLK\t%lu\n", dsi_fclk_rate());

	seq_printf(s,	"DDR_CLK\t\t%lu\n",
			cinfo->clkin4ddr / 4);

	seq_printf(s,	"TxByteClkHS\t%lu\n", dsi_get_txbyteclkhs(lcd_ix));

	seq_printf(s,	"LP_CLK\t\t%lu\n", cinfo->lp_clk);

	seq_printf(s,	"VP_CLK\t\t%lu\n"
			"VP_PCLK\t\t%lu\n",
			dispc_lclk_rate(OMAP_DSS_CHANNEL_LCD),
			dispc_pclk_rate(OMAP_DSS_CHANNEL_LCD));

	enable_clocks(0);
}

void dsi_dump_regs(enum dsi lcd_ix, struct seq_file *s)
{
#define DUMPREG(n, r) seq_printf(s, "%-35s %08x\n", #r, dsi_read_reg(n, r))


	dss_clk_enable(DSS_CLK_ICK | DSS_CLK_FCK1);

	DUMPREG(lcd_ix, DSI_REVISION);
	DUMPREG(lcd_ix, DSI_SYSCONFIG);
	DUMPREG(lcd_ix, DSI_SYSSTATUS);
	DUMPREG(lcd_ix, DSI_IRQSTATUS);
	DUMPREG(lcd_ix, DSI_IRQENABLE);
	DUMPREG(lcd_ix, DSI_CTRL);
	DUMPREG(lcd_ix, DSI_COMPLEXIO_CFG1);
	DUMPREG(lcd_ix, DSI_COMPLEXIO_IRQ_STATUS);
	DUMPREG(lcd_ix, DSI_COMPLEXIO_IRQ_ENABLE);
	DUMPREG(lcd_ix, DSI_CLK_CTRL);
	DUMPREG(lcd_ix, DSI_TIMING1);
	DUMPREG(lcd_ix, DSI_TIMING2);
	DUMPREG(lcd_ix, DSI_VM_TIMING1);
	DUMPREG(lcd_ix, DSI_VM_TIMING2);
	DUMPREG(lcd_ix, DSI_VM_TIMING3);
	DUMPREG(lcd_ix, DSI_CLK_TIMING);
	DUMPREG(lcd_ix, DSI_TX_FIFO_VC_SIZE);
	DUMPREG(lcd_ix, DSI_RX_FIFO_VC_SIZE);
	DUMPREG(lcd_ix, DSI_COMPLEXIO_CFG2);
	DUMPREG(lcd_ix, DSI_RX_FIFO_VC_FULLNESS);
	DUMPREG(lcd_ix, DSI_VM_TIMING4);
	DUMPREG(lcd_ix, DSI_TX_FIFO_VC_EMPTINESS);
	DUMPREG(lcd_ix, DSI_VM_TIMING5);
	DUMPREG(lcd_ix, DSI_VM_TIMING6);
	DUMPREG(lcd_ix, DSI_VM_TIMING7);
	DUMPREG(lcd_ix, DSI_STOPCLK_TIMING);

	DUMPREG(lcd_ix, DSI_VC_CTRL(0));
	DUMPREG(lcd_ix, DSI_VC_TE(0));
	DUMPREG(lcd_ix, DSI_VC_LONG_PACKET_HEADER(0));
	DUMPREG(lcd_ix, DSI_VC_LONG_PACKET_PAYLOAD(0));
	DUMPREG(lcd_ix, DSI_VC_SHORT_PACKET_HEADER(0));
	DUMPREG(lcd_ix, DSI_VC_IRQSTATUS(0));
	DUMPREG(lcd_ix, DSI_VC_IRQENABLE(0));

	DUMPREG(lcd_ix, DSI_VC_CTRL(1));
	DUMPREG(lcd_ix, DSI_VC_TE(1));
	DUMPREG(lcd_ix, DSI_VC_LONG_PACKET_HEADER(1));
	DUMPREG(lcd_ix, DSI_VC_LONG_PACKET_PAYLOAD(1));
	DUMPREG(lcd_ix, DSI_VC_SHORT_PACKET_HEADER(1));
	DUMPREG(lcd_ix, DSI_VC_IRQSTATUS(1));
	DUMPREG(lcd_ix, DSI_VC_IRQENABLE(1));

	DUMPREG(lcd_ix, DSI_VC_CTRL(2));
	DUMPREG(lcd_ix, DSI_VC_TE(2));
	DUMPREG(lcd_ix, DSI_VC_LONG_PACKET_HEADER(2));
	DUMPREG(lcd_ix, DSI_VC_LONG_PACKET_PAYLOAD(2));
	DUMPREG(lcd_ix, DSI_VC_SHORT_PACKET_HEADER(2));
	DUMPREG(lcd_ix, DSI_VC_IRQSTATUS(2));
	DUMPREG(lcd_ix, DSI_VC_IRQENABLE(2));

	DUMPREG(lcd_ix, DSI_VC_CTRL(3));
	DUMPREG(lcd_ix, DSI_VC_TE(3));
	DUMPREG(lcd_ix, DSI_VC_LONG_PACKET_HEADER(3));
	DUMPREG(lcd_ix, DSI_VC_LONG_PACKET_PAYLOAD(3));
	DUMPREG(lcd_ix, DSI_VC_SHORT_PACKET_HEADER(3));
	DUMPREG(lcd_ix, DSI_VC_IRQSTATUS(3));
	DUMPREG(lcd_ix, DSI_VC_IRQENABLE(3));

	DUMPREG(lcd_ix, DSI_DSIPHY_CFG0);
	DUMPREG(lcd_ix, DSI_DSIPHY_CFG1);
	DUMPREG(lcd_ix, DSI_DSIPHY_CFG2);
	DUMPREG(lcd_ix, DSI_DSIPHY_CFG5);

	DUMPREG(lcd_ix, DSI_PLL_CONTROL);
	DUMPREG(lcd_ix, DSI_PLL_STATUS);
	DUMPREG(lcd_ix, DSI_PLL_GO);
	DUMPREG(lcd_ix, DSI_PLL_CONFIGURATION1);
	DUMPREG(lcd_ix, DSI_PLL_CONFIGURATION2);

	dss_clk_disable(DSS_CLK_ICK | DSS_CLK_FCK1);
#undef DUMPREG
}

enum dsi_complexio_power_state {
	DSI_COMPLEXIO_POWER_OFF		= 0x0,
	DSI_COMPLEXIO_POWER_ON		= 0x1,
	DSI_COMPLEXIO_POWER_ULPS	= 0x2,
};

static int dsi_complexio_power(enum dsi lcd_ix, enum dsi_complexio_power_state state)
{
	int t = 0;

	/* PWR_CMD */
	REG_FLD_MOD(lcd_ix, DSI_COMPLEXIO_CFG1, state, 28, 27);

if (cpu_is_omap44xx())
	/*bit 30 has to be set to 1 to GO in omap4*/
	REG_FLD_MOD(lcd_ix, DSI_COMPLEXIO_CFG1, 1, 30, 30);

	/* PWR_STATUS */
	while (FLD_GET(dsi_read_reg(lcd_ix, DSI_COMPLEXIO_CFG1), 26, 25) != state) {
		udelay(1);
		if (t++ > 10000) {
			DSSERR("failed to set complexio power state to "
					"%d\n", state);
			return -ENODEV;
		}
	}

	return 0;
}

static void dsi_complexio_config(struct omap_dss_device *dssdev)
{
	u32 r;
	enum dsi lcd_ix;

	int clk_lane   = dssdev->phy.dsi.clk_lane;
	int data1_lane = dssdev->phy.dsi.data1_lane;
	int data2_lane = dssdev->phy.dsi.data2_lane;
	int clk_pol    = dssdev->phy.dsi.clk_pol;
	int data1_pol  = dssdev->phy.dsi.data1_pol;
	int data2_pol  = dssdev->phy.dsi.data2_pol;

	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	r = dsi_read_reg(lcd_ix, DSI_COMPLEXIO_CFG1);
	r = FLD_MOD(r, clk_lane, 2, 0);
	r = FLD_MOD(r, clk_pol, 3, 3);
	r = FLD_MOD(r, data1_lane, 6, 4);
	r = FLD_MOD(r, data1_pol, 7, 7);
	r = FLD_MOD(r, data2_lane, 10, 8);
	r = FLD_MOD(r, data2_pol, 11, 11);
	dsi_write_reg(lcd_ix, DSI_COMPLEXIO_CFG1, r);

	/* The configuration of the DSI complex I/O (number of data lanes,
	   position, differential order) should not be changed while
	   DSS.DSI_CLK_CRTRL[20] LP_CLK_ENABLE bit is set to 1. In order for
	   the hardware to take into account a new configuration of the complex
	   I/O (done in DSS.DSI_COMPLEXIO_CFG1 register), it is recommended to
	   follow this sequence: First set the DSS.DSI_CTRL[0] IF_EN bit to 1,
	   then reset the DSS.DSI_CTRL[0] IF_EN to 0, then set
	   DSS.DSI_CLK_CTRL[20] LP_CLK_ENABLE to 1 and finally set again the
	   DSS.DSI_CTRL[0] IF_EN bit to 1. If the sequence is not followed, the
	   DSI complex I/O configuration is unknown. */

	/*
	REG_FLD_MOD(DSI_CTRL, 1, 0, 0);
	REG_FLD_MOD(DSI_CTRL, 0, 0, 0);
	REG_FLD_MOD(DSI_CLK_CTRL, 1, 20, 20);
	REG_FLD_MOD(DSI_CTRL, 1, 0, 0);
	*/
}

static inline unsigned ns2ddr(enum dsi lcd_ix, unsigned ns)
{
	unsigned long ddr_clk;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	/* convert time in ns to ddr ticks, rounding up */
	ddr_clk = p_dsi->current_cinfo.clkin4ddr / 4;
	return (ns * (ddr_clk/1000/1000) + 999) / 1000;
}

static inline unsigned ddr2ns(enum dsi lcd_ix, unsigned ddr)
{
	struct dsi_struct *p_dsi;
	unsigned long ddr_clk;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	ddr_clk = p_dsi->current_cinfo.clkin4ddr / 4;
	return ddr * 1000 * 1000 / (ddr_clk / 1000);
}

static void dsi_complexio_timings(enum dsi lcd_ix)
{
	u32 r;
	u32 ths_prepare, ths_prepare_ths_zero, ths_trail, ths_exit;
	u32 tlpx_half, tclk_trail, tclk_zero;
	u32 tclk_prepare;
	unsigned int val;
	/* calculate timings */

	/* 1 * DDR_CLK = 2 * UI */

	/* min 40ns + 4*UI	max 85ns + 6*UI */
	ths_prepare = ns2ddr(lcd_ix, 70) + 2;

	/* min 145ns + 10*UI */
	ths_prepare_ths_zero = ns2ddr(lcd_ix, 175) + 2;

	/* min max(8*UI, 60ns+4*UI) */
	ths_trail = ns2ddr(lcd_ix, 60) + 5;

	/* min 100ns */
	ths_exit = ns2ddr(lcd_ix, 145);

	/* tlpx min 50n */
	tlpx_half = ns2ddr(lcd_ix, 25);

	/* min 60ns */
	tclk_trail = ns2ddr(lcd_ix, 60) + 2;

	/* min 38ns, max 95ns */
	tclk_prepare = ns2ddr(lcd_ix, 65);

	/* min tclk-prepare + tclk-zero = 300ns */
	tclk_zero = ns2ddr(lcd_ix, 260);

	DSSDBG("ths_prepare %u (%uns), ths_prepare_ths_zero %u (%uns)\n",
		ths_prepare, ddr2ns(lcd_ix, ths_prepare),
		ths_prepare_ths_zero, ddr2ns(lcd_ix, ths_prepare_ths_zero));
	DSSDBG("ths_trail %u (%uns), ths_exit %u (%uns)\n",
			ths_trail, ddr2ns(lcd_ix, ths_trail),
			ths_exit, ddr2ns(lcd_ix, ths_exit));

	DSSDBG("tlpx_half %u (%uns), tclk_trail %u (%uns), "
			"tclk_zero %u (%uns)\n",
			tlpx_half, ddr2ns(lcd_ix, tlpx_half),
			tclk_trail, ddr2ns(lcd_ix, tclk_trail),
			tclk_zero, ddr2ns(lcd_ix, tclk_zero));
	DSSDBG("tclk_prepare %u (%uns)\n",
			tclk_prepare, ddr2ns(lcd_ix, tclk_prepare));

	/* program timings */
	if (!cpu_is_omap44xx()) {
	r = dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG0);
	r = FLD_MOD(r, ths_prepare, 31, 24);
	r = FLD_MOD(r, ths_prepare_ths_zero, 23, 16);
	r = FLD_MOD(r, ths_trail, 15, 8);
	r = FLD_MOD(r, ths_exit, 7, 0);
	dsi_write_reg(lcd_ix, DSI_DSIPHY_CFG0, r);

	r = dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG1);
	r = FLD_MOD(r, tlpx_half, 22, 16);
	r = FLD_MOD(r, tclk_trail, 15, 8);
	r = FLD_MOD(r, tclk_zero, 7, 0);
	dsi_write_reg(lcd_ix, DSI_DSIPHY_CFG1, r);

	r = dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG2);
	r = FLD_MOD(r, tclk_prepare, 7, 0);
	dsi_write_reg(lcd_ix, DSI_DSIPHY_CFG2, r);
		} else {
	/*Timing Configurations
	 Configure the DSI PHY timing parameters*/
	val = ((9<<24)|
		   (20<<16)|
		   (6<<8)|
		   (15<<0));
	dsi_write_reg(lcd_ix, DSI_DSIPHY_CFG0,val);

	val = dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG1);
	val = val & ~(0x007FFFFF);
	val = ((2<<29)|
	       (0<<27)|
	       (2<<24)|
	       (3<<16)|
	       (6<<8)|
	       (26<<0) );
	dsi_write_reg(lcd_ix, DSI_DSIPHY_CFG1,val);

	val = dsi_read_reg(lcd_ix,DSI_DSIPHY_CFG2);
	/* this is required to preserve the reset data */
	val = val & ~(0x000000FF);
	val = val | (7 << 0);
	dsi_write_reg(lcd_ix, DSI_DSIPHY_CFG2,val);
			}
}


static int dsi_complexio_init(enum dsi lcd_ix, struct omap_dss_device *dssdev)
{
	int r = 0;

	void __iomem *phymux_base = NULL;

	unsigned int dsimux = 0xFFFFFFFF;

	DSSDBG("dsi_complexio_init\n");

	if (cpu_is_omap44xx()) {
	phymux_base = ioremap(0x4A100000, 0x1000);

	/* CIO_CLK_ICG, enable L3 clk to CIO */
	REG_FLD_MOD(lcd_ix, DSI_CLK_CTRL, 1, 14, 14);

	/* Turning on DSI PHY Mux*/
	__raw_writel(dsimux, phymux_base+0x618);
	dsimux = __raw_readl(phymux_base+0x618);
	/* A dummy read using the SCP interface to any DSIPHY register is
	 * required after DSIPHY reset to complete the reset of the DSI complex
	 * I/O. */
	}
	if (!cpu_is_omap44xx()) {

		dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG5);

	if (wait_for_bit_change(lcd_ix, DSI_DSIPHY_CFG5, 30, 1) != 1) {
		DSSERR("ComplexIO PHY not coming out of reset.\n");
		r = -ENODEV;
		goto err;
		}
	}


	dsi_complexio_config(dssdev);

if (cpu_is_omap44xx()) {
			/* clear the events*/
		dsi_write_reg(lcd_ix, DSI_COMPLEXIO_IRQ_STATUS, 0xFFFFFFFF);
			/* enable complexio interrupt events*/
		dsi_write_reg(lcd_ix, DSI_COMPLEXIO_IRQ_ENABLE, 0x0);
	}

	r = dsi_complexio_power(lcd_ix, DSI_COMPLEXIO_POWER_ON);

	if (r)
		goto err;

	if (wait_for_bit_change(lcd_ix, DSI_COMPLEXIO_CFG1, 29, 1) != 1) {
		DSSERR("ComplexIO not coming out of reset.\n");
		r = -ENODEV;
		goto err;
	}

if (!cpu_is_omap44xx()) {
		/*unknown issue with omap4*/
	if (wait_for_bit_change(lcd_ix, DSI_COMPLEXIO_CFG1, 21, 1) != 1) {
		DSSERR("ComplexIO LDO power down.\n");
		r = -ENODEV;
		goto err;
	}
		}

	dsi_complexio_timings(lcd_ix);

	/*
	   The configuration of the DSI complex I/O (number of data lanes,
	   position, differential order) should not be changed while
	   DSS.DSI_CLK_CRTRL[20] LP_CLK_ENABLE bit is set to 1. For the
	   hardware to recognize a new configuration of the complex I/O (done
	   in DSS.DSI_COMPLEXIO_CFG1 register), it is recommended to follow
	   this sequence: First set the DSS.DSI_CTRL[0] IF_EN bit to 1, next
	   reset the DSS.DSI_CTRL[0] IF_EN to 0, then set DSS.DSI_CLK_CTRL[20]
	   LP_CLK_ENABLE to 1, and finally, set again the DSS.DSI_CTRL[0] IF_EN
	   bit to 1. If the sequence is not followed, the DSi complex I/O
	   configuration is undetermined.
	   */


	dsi_if_enable(lcd_ix, 1);
	dsi_if_enable(lcd_ix, 0);
	REG_FLD_MOD(lcd_ix, DSI_CLK_CTRL, 1, 20, 20); /* LP_CLK_ENABLE done before now*/
	dsi_if_enable(lcd_ix, 1);
	dsi_if_enable(lcd_ix, 0);

	DSSDBG("CIO init done\n");
err:
	return r;
}

static void dsi_complexio_uninit(enum dsi lcd_ix)
{
	dsi_complexio_power(lcd_ix, DSI_COMPLEXIO_POWER_OFF);
}

static int _dsi_wait_reset(enum dsi lcd_ix)
{
	int i = 0;

	while (REG_GET(lcd_ix, DSI_SYSSTATUS, 0, 0) == 0) {
		if (i++ > 50) {
			DSSERR("soft reset failed\n");
			return -ENODEV;
		}
		udelay(1);
	}

	return 0;
}

static int _dsi_reset(enum dsi lcd_ix)
{
	/* Soft reset */
	REG_FLD_MOD(lcd_ix, DSI_SYSCONFIG, 1, 1, 1);
	return _dsi_wait_reset(lcd_ix);
}

static void dsi_reset_tx_fifo(enum dsi lcd_ix, int channel)
{
	u32 mask;
	u32 l;

	/* set fifosize of the channel to 0, then return the old size */
	l = dsi_read_reg(lcd_ix, DSI_TX_FIFO_VC_SIZE);

	mask = FLD_MASK((8 * channel) + 7, (8 * channel) + 4);
	dsi_write_reg(lcd_ix, DSI_TX_FIFO_VC_SIZE, l & ~mask);

	dsi_write_reg(lcd_ix, DSI_TX_FIFO_VC_SIZE, l);
}

static void dsi_config_tx_fifo(enum dsi lcd_ix, enum fifo_size size1, enum fifo_size size2,
		enum fifo_size size3, enum fifo_size size4)
{
	u32 r = 0;
	int add = 0;
	int i;
	unsigned int val;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	p_dsi->vc[0].fifo_size = size1;
	p_dsi->vc[1].fifo_size = size2;
	p_dsi->vc[2].fifo_size = size3;
	p_dsi->vc[3].fifo_size = size4;

if (!cpu_is_omap44xx()) {

	for (i = 0; i < 4; i++) {
		u8 v;
		int size = p_dsi->vc[i].fifo_size;

		if (add + size > 4) {
			DSSERR("Illegal FIFO configuration\n");
			BUG();
		}

		v = FLD_VAL(add, 2, 0) | FLD_VAL(size, 7, 4);
		r |= v << (8 * i);
		/*DSSDBG("TX FIFO vc %d: size %d, add %d\n", i, size, add); */
		add += size;
	}

	dsi_write_reg(lcd_ix, DSI_TX_FIFO_VC_SIZE, r);
		} else {
			val = (0<<28) | 			/* VC3 Not used*/
				(0<<24) | 			/* VC3 Not used*/
				(0<<20) |			/* VC2 Not used*/
				(0<<16) | 			/* VC2 Not used*/
				(4<<12) | 			/* VC1 FIFO length*/
				(0<<8) | 				/* VC1 FIFO start address*/
				(4<<4) | 				/* VC0 FIFO length*/
				(0<<0); 				/* VC0 FIFO start address*/
				dsi_write_reg(lcd_ix, DSI_TX_FIFO_VC_SIZE, val);

		}
}

static void dsi_config_rx_fifo(enum dsi lcd_ix, enum fifo_size size1, enum fifo_size size2,
		enum fifo_size size3, enum fifo_size size4)
{
	u32 r = 0;
	int add = 0;
	int i;
	unsigned int val;
	struct dsi_struct *p_dsi;

	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	p_dsi->vc[0].fifo_size = size1;
	p_dsi->vc[1].fifo_size = size2;
	p_dsi->vc[2].fifo_size = size3;
	p_dsi->vc[3].fifo_size = size4;

if (!cpu_is_omap44xx()) {

	for (i = 0; i < 4; i++) {
		u8 v;
		int size = p_dsi->vc[i].fifo_size;

		if (add + size > 4) {
			DSSERR("Illegal FIFO configuration\n");
			BUG();
		}

		v = FLD_VAL(add, 2, 0) | FLD_VAL(size, 7, 4);
		r |= v << (8 * i);
		/*DSSDBG("RX FIFO vc %d: size %d, add %d\n", i, size, add); */
		add += size;
	}

		dsi_write_reg(lcd_ix, DSI_RX_FIFO_VC_SIZE, r);
	} else {

		val = (0<<28) | 			/* VC3 Not used*/
			(0<<24) | 			/* VC3 Not used*/
			(0<<20) | 			/* VC2 Not used*/
			(0<<16) | 			/* VC2 Not used*/
			(1<<12) | 			/* VC1 FIFO length*/
			(0<<8) | 				/* VC1 FIFO start address*/
			(1<<4) | 				/* VC0 Not used for BTA*/
			(0<<0);				/* VC0 Not used for BTA*/
			dsi_write_reg(lcd_ix, DSI_RX_FIFO_VC_SIZE, val);

	}
}

static int dsi_force_tx_stop_mode_io(enum dsi lcd_ix)
{
	u32 r;

	r = dsi_read_reg(lcd_ix, DSI_TIMING1);
	r = FLD_MOD(r, 1, 15, 15);	/* FORCE_TX_STOP_MODE_IO */
	dsi_write_reg(lcd_ix, DSI_TIMING1, r);

	if (wait_for_bit_change(lcd_ix, DSI_TIMING1, 15, 0) != 0) {
		DSSERR("TX_STOP bit not going down\n");
		return -EIO;
	}

	return 0;
}

static void dsi_vc_print_status(enum dsi lcd_ix, int channel)
{
	u32 r;

	r = dsi_read_reg(lcd_ix, DSI_VC_CTRL(channel));
	DSSDBG("vc %d: TX_FIFO_NOT_EMPTY %d, BTA_EN %d, VC_BUSY %d, "
			"TX_FIFO_FULL %d, RX_FIFO_NOT_EMPTY %d, ",
			channel,
			FLD_GET(r, 5, 5),
			FLD_GET(r, 6, 6),
			FLD_GET(r, 15, 15),
			FLD_GET(r, 16, 16),
			FLD_GET(r, 20, 20));

	r = dsi_read_reg(lcd_ix, DSI_TX_FIFO_VC_EMPTINESS);
	DSSDBG("EMPTINESS %d\n", (r >> (8 * channel)) & 0xff);
}

static int dsi_vc_enable(enum dsi lcd_ix, int channel, bool enable)
{

/*	if (dsi.update_mode != OMAP_DSS_UPDATE_AUTO)
	DSSDBG("dsi_vc_enable channel %d, enable %d\n",
			channel, enable);*/

	enable = enable ? 1 : 0;

	REG_FLD_MOD(lcd_ix, DSI_VC_CTRL(channel), enable, 0, 0);

	if (wait_for_bit_change(lcd_ix, DSI_VC_CTRL(channel), 0, enable) != enable) {
			DSSERR("Failed to set dsi_vc_enable to %d\n", enable);
			return -EIO;
	}

	return 0;
}

static void dsi_vc_initial_config(enum dsi lcd_ix, int channel)
{
	u32 r;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	DSSDBGF("%d", channel);

	r = dsi_read_reg(lcd_ix, DSI_VC_CTRL(channel));

	if (FLD_GET(r, 15, 15)) /* VC_BUSY */
		DSSERR("VC(%d) busy when trying to configure it!\n",
				channel);

	r = FLD_MOD(r, 0, 1, 1); /* SOURCE, 0 = L4 */
	r = FLD_MOD(r, 0, 2, 2); /* BTA_SHORT_EN  */
	r = FLD_MOD(r, 0, 3, 3); /* BTA_LONG_EN */
	r = FLD_MOD(r, 0, 4, 4); /* MODE, 0 = command */
	r = FLD_MOD(r, 1, 7, 7); /* CS_TX_EN */
	r = FLD_MOD(r, 1, 8, 8); /* ECC_TX_EN */
	r = FLD_MOD(r, 0, 9, 9); /* MODE_SPEED, high speed on/off */
	if (cpu_is_omap44xx()) {
		r = FLD_MOD(r, 3, 11, 10);  /*sv5*/
		r = FLD_MOD(r, 3, 18, 17);  /*sv5*/
		}
	r = FLD_MOD(r, 4, 29, 27); /* DMA_RX_REQ_NB = no dma */
	r = FLD_MOD(r, 4, 23, 21); /* DMA_TX_REQ_NB = no dma */

	dsi_write_reg(lcd_ix, DSI_VC_CTRL(channel), r);
	p_dsi->vc[channel].mode = DSI_VC_MODE_L4;
}

static void dsi_vc_config_l4(enum dsi lcd_ix, int channel)
{
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	if (p_dsi->vc[channel].mode == DSI_VC_MODE_L4)
		return;

	DSSDBGF("%d", channel);

	dsi_vc_enable(lcd_ix, channel, 0);

	if (REG_GET(lcd_ix, DSI_VC_CTRL(channel), 15, 15)) /* VC_BUSY */
		DSSERR("vc(%d) busy when trying to config for L4\n", channel);

	REG_FLD_MOD(lcd_ix, DSI_VC_CTRL(channel), 0, 1, 1); /* SOURCE, 0 = L4 */

	dsi_vc_enable(lcd_ix, channel, 1);

	p_dsi->vc[channel].mode = DSI_VC_MODE_L4;
}

static void dsi_vc_config_vp(enum dsi lcd_ix, int channel)
{
	u32 r;

	DSSDBG("dsi_vc_config_vp\n");

	r = dsi_read_reg(lcd_ix, DSI_VC_CTRL(channel));

	r = FLD_MOD(r, 1, 1, 1); /* SOURCE, 1 = video port */
	r = FLD_MOD(r, 0, 2, 2); /* BTA_SHORT_EN */
	r = FLD_MOD(r, 0, 3, 3); /* BTA_LONG_EN */
	r = FLD_MOD(r, 0, 4, 4); /* MODE, 0 = command */
	r = FLD_MOD(r, 1, 7, 7); /* CS_TX_EN */
	r = FLD_MOD(r, 1, 8, 8); /* ECC_TX_EN */
	r = FLD_MOD(r, 1, 9, 9); /* MODE_SPEED, high speed on/off */

	r = FLD_MOD(r, 4, 29, 27); /* DMA_RX_REQ_NB = no dma */
	r = FLD_MOD(r, 4, 23, 21); /* DMA_TX_REQ_NB = no dma */

if (cpu_is_omap44xx()) {
	r = FLD_MOD(r, 1, 12, 12);	/*RGB565_ORDER*/
	r = FLD_MOD(r, 1, 30, 30);	/* DCS_CMD_ENABLE*/
	r = FLD_MOD(r, 0, 31, 31);	/* DCS_CMD_CODE*/
	}

	dsi_write_reg(lcd_ix, DSI_VC_CTRL(channel), r);
}


static void dsi_vc_enable_hs(enum dsi lcd_ix, int channel, bool enable)
{
	DSSDBG("dsi_vc_enable_hs(%d, %d)\n", channel, enable);

	dsi_vc_enable(lcd_ix, channel, 0);
	dsi_if_enable(lcd_ix, 0);

	REG_FLD_MOD(lcd_ix, DSI_VC_CTRL(channel), enable, 9, 9);

	dsi_vc_enable(lcd_ix, channel, 1);
	dsi_if_enable(lcd_ix, 1);

	dsi_force_tx_stop_mode_io(lcd_ix);
}

static void dsi_vc_flush_long_data(enum dsi lcd_ix, int channel)
{
	while (REG_GET(lcd_ix, DSI_VC_CTRL(channel), 20, 20)) {
		u32 val;
		val = dsi_read_reg(lcd_ix, DSI_VC_SHORT_PACKET_HEADER(channel));
		DSSDBG("\t\tb1 %#02x b2 %#02x b3 %#02x b4 %#02x\n",
				(val >> 0) & 0xff,
				(val >> 8) & 0xff,
				(val >> 16) & 0xff,
				(val >> 24) & 0xff);
	}
}

static void dsi_show_rx_ack_with_err(u16 err)
{
	DSSERR("\tACK with ERROR (%#x):\n", err);
	if (err & (1 << 0))
		DSSERR("\t\tSoT Error\n");
	if (err & (1 << 1))
		DSSERR("\t\tSoT Sync Error\n");
	if (err & (1 << 2))
		DSSERR("\t\tEoT Sync Error\n");
	if (err & (1 << 3))
		DSSERR("\t\tEscape Mode Entry Command Error\n");
	if (err & (1 << 4))
		DSSERR("\t\tLP Transmit Sync Error\n");
	if (err & (1 << 5))
		DSSERR("\t\tHS Receive Timeout Error\n");
	if (err & (1 << 6))
		DSSERR("\t\tFalse Control Error\n");
	if (err & (1 << 7))
		DSSERR("\t\t(reserved7)\n");
	if (err & (1 << 8))
		DSSERR("\t\tECC Error, single-bit (corrected)\n");
	if (err & (1 << 9))
		DSSERR("\t\tECC Error, multi-bit (not corrected)\n");
	if (err & (1 << 10))
		DSSERR("\t\tChecksum Error\n");
	if (err & (1 << 11))
		DSSERR("\t\tData type not recognized\n");
	if (err & (1 << 12))
		DSSERR("\t\tInvalid VC ID\n");
	if (err & (1 << 13))
		DSSERR("\t\tInvalid Transmission Length\n");
	if (err & (1 << 14))
		DSSERR("\t\t(reserved14)\n");
	if (err & (1 << 15))
		DSSERR("\t\tDSI Protocol Violation\n");
}

static u16 dsi_vc_flush_receive_data(enum dsi lcd_ix, int channel)
{
	/* RX_FIFO_NOT_EMPTY */
	while (REG_GET(lcd_ix, DSI_VC_CTRL(channel), 20, 20)) {
		u32 val;
		u8 dt;
		val = dsi_read_reg(lcd_ix, DSI_VC_SHORT_PACKET_HEADER(channel));
		DSSDBG("\trawval %#08x\n", val);
		dt = FLD_GET(val, 5, 0);
		if (dt == DSI_DT_RX_ACK_WITH_ERR) {
			u16 err = FLD_GET(val, 23, 8);
			dsi_show_rx_ack_with_err(err);
		} else if (dt == DSI_DT_RX_SHORT_READ_1) {
			DSSDBG("\tDCS short response, 1 byte: %#x\n",
					FLD_GET(val, 23, 8));
		} else if (dt == DSI_DT_RX_SHORT_READ_2) {
			DSSDBG("\tDCS short response, 2 byte: %#x\n",
					FLD_GET(val, 23, 8));
		} else if (dt == DSI_DT_RX_DCS_LONG_READ) {
			DSSDBG("\tDCS long response, len %d\n",
					FLD_GET(val, 23, 8));
			dsi_vc_flush_long_data(lcd_ix, channel);
		} else {
			DSSERR("\tunknown datatype 0x%02x\n", dt);
		}
	}
	return 0;
}

static int dsi_vc_send_bta(enum dsi lcd_ix, int channel)
{
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	if (p_dsi->update_mode != OMAP_DSS_UPDATE_AUTO &&
		(p_dsi->debug_write || p_dsi->debug_read))
		DSSDBG("dsi_vc_send_bta %d\n", channel);

	WARN_ON(!mutex_is_locked(&(p_dsi->bus_lock)));
	/* RX_FIFO_NOT_EMPTY */
	if (REG_GET(lcd_ix, DSI_VC_CTRL(channel), 20, 20)) {
		DSSERR("rx fifo not empty when sending BTA, dumping data:\n");
		dsi_vc_flush_receive_data(lcd_ix, channel);
	}

	REG_FLD_MOD(lcd_ix, DSI_VC_CTRL(channel), 1, 6, 6); /* BTA_EN */

	return 0;
}

int dsi_vc_send_bta_sync(enum dsi lcd_ix, int channel)
{
	int r = 0;
	u32 err;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	INIT_COMPLETION(p_dsi->bta_completion);

	dsi_vc_enable_bta_irq(lcd_ix, channel);

	r = dsi_vc_send_bta(lcd_ix, channel);
	if (r)
		goto err;

	if (wait_for_completion_timeout(&(p_dsi->bta_completion),
				msecs_to_jiffies(500)) == 0) {
		DSSERR("Failed to receive BTA\n");
		r = -EIO;
		goto err;
	}

	err = dsi_get_errors(lcd_ix);
	if (err) {
		DSSERR("Error while sending BTA: %x\n", err);
		r = -EIO;
		goto err;
	}
err:
	dsi_vc_disable_bta_irq(lcd_ix, channel);

	return r;
}
EXPORT_SYMBOL(dsi_vc_send_bta_sync);

static inline void dsi_vc_write_long_header(enum dsi lcd_ix, int channel,
	u8 data_type, u16 len, u8 ecc)
{
	u32 val;
	u8 data_id;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	WARN_ON(!mutex_is_locked(&(p_dsi->bus_lock)));
	ecc = 0; /*sv5*/

	/*data_id = data_type | channel << 6; */
	data_id = data_type | p_dsi->vc[channel].dest_per << 6;

	val = FLD_VAL(data_id, 7, 0) | FLD_VAL(len, 23, 8) |
		FLD_VAL(ecc, 31, 24);

	dsi_write_reg(lcd_ix, DSI_VC_LONG_PACKET_HEADER(channel), val);
}

static inline void dsi_vc_write_long_payload(enum dsi lcd_ix, int channel,
		u8 b1, u8 b2, u8 b3, u8 b4)
{
	u32 val;

	val = b4 << 24 | b3 << 16 | b2 << 8  | b1 << 0;
	dsi_write_reg(lcd_ix, DSI_VC_LONG_PACKET_PAYLOAD(channel), val);
}

static int dsi_vc_send_long(enum dsi lcd_ix,
	int channel, u8 data_type, u8 *data, u16 len, u8 ecc)
{
	int i;
	u8 *p;
	int r = 0;
	u8 b1, b2, b3, b4;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	ecc = 0; /*sv5*/

	if (p_dsi->debug_write)
		DSSDBG("dsi_vc_send_long, %d bytes\n", len);


/*sv HS mode*/
	printk(KERN_DEBUG "we need not come here for send long");
	/* len + header */
	if (p_dsi->vc[channel].fifo_size * 32 * 4 < len + 4) {
		DSSERR("unable to send long packet: packet too long.\n");
		return -EINVAL;
	}

	dsi_vc_config_l4(lcd_ix, channel);

	mdelay(2+1);
	dsi_vc_write_long_header(lcd_ix, channel, data_type, len, ecc);



	p = data;
	for (i = 0; i < len >> 2; i++) {
		if (p_dsi->debug_write)
			DSSDBG("\tsending full packet %d\n", i);
		/*dsi_vc_print_status(0); */

		b1 = *p++;
		b2 = *p++;
		b3 = *p++;
		b4 = *p++;

		mdelay(2+1);
		dsi_vc_write_long_payload(lcd_ix, channel, b1, b2, b3, b4);
	}

	i = len % 4;
	if (i) {
		b1 = 0; b2 = 0; b3 = 0;

		if (p_dsi->debug_write)
			DSSDBG("\tsending remainder bytes %d\n", i);

		switch (i) {
		case 3:
			b1 = *p++;
			b2 = *p++;
			b3 = *p++;
			break;
		case 2:
			b1 = *p++;
			b2 = *p++;
			break;
		case 1:
			b1 = *p++;
			break;
		}

		mdelay(2+1);
		dsi_vc_write_long_payload(lcd_ix, channel, b1, b2, b3, 0);
	}

	return r;
}



int send_short_packet(enum dsi lcd_ix, u8 data_type, u8 vc, u8 data0,
 u8 data1, bool mode, bool ecc)
{
	u32 val, header = 0, count = 10000;

	/* Configure the Virtual Channel */
	dsi_vc_enable(lcd_ix, vc, 0);
	/* speed selection (HS or LPS) */
	val = dsi_read_reg(lcd_ix, DSI_VC_CTRL(vc));
	if (mode == 1) {/*HS MODE*/
		val = val | (1<<9);
		}
	else if (mode == 0) {/*LP MODE*/
		val = val & ~(1<<9);
		}
	dsi_write_reg(lcd_ix, DSI_VC_CTRL(vc), val);
	/*TODO: can be do the below step before itself, do we need to disable
	the DSI interface before configuring the 	 * VCs */
	/*	enable_omap_dsi_interface();	*/
	dsi_vc_enable(lcd_ix, vc, 1);
	/* Send Short packet */
	header = (0<<24)|
			(data1<<16)|
			(data0<<8)|
			(0<<6) |
			(data_type<<0);
	dsi_write_reg(lcd_ix, DSI_VC_SHORT_PACKET_HEADER(0), header);

	printk(KERN_DEBUG "Header = 0x%x", header);

	do {
		val = dsi_read_reg(lcd_ix, DSI_VC_IRQSTATUS(vc));
	} while ((!(val & 0x00000004)) && --count);
	if (count) {
		printk(KERN_DEBUG "Short packet  success!!! \n\r");
		/*TODO: this need to be cross check,
		whether we need to reset the bit */
		dsi_write_reg(lcd_ix, DSI_VC_IRQSTATUS(vc), 0x00000004);
		return 0;
		}
	else	{
		printk(KERN_ERR "Failed to send Short packet !!! \n\r");
		return -1;
		}
}


static int dsi_vc_send_short(enum dsi lcd_ix, int channel, u8 data_type,
	u16 data, u8 ecc)
{
	u32 r;
	u8 data_id;
	u32 val, u, count;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	ecc = 0; /*sv5*/

	WARN_ON(!mutex_is_locked(&(p_dsi->bus_lock)));

	if (p_dsi->debug_write)
		DSSDBG("dsi_vc_send_short(ch%d, dt %#x, b1 %#x, b2 %#x)\n",
				channel,
				data_type, data & 0xff, (data >> 8) & 0xff);

	dsi_vc_config_l4(lcd_ix, channel);

	data_id = data_type | 0 << 6;

	r = (data_id << 0) | (data << 8) | (0 << 16) | (ecc << 24);

	mdelay(2);

	dsi_write_reg(lcd_ix, DSI_VC_SHORT_PACKET_HEADER(channel), r);

	count = 10000;

	do {
		val = dsi_read_reg(lcd_ix, DSI_VC_IRQSTATUS(channel));
		for (u = 0; u < 100000; u++)
			;
	} while ((!(val & 0x4)) && (--count))
		;

	if (count) {
		dsi_write_reg(lcd_ix, DSI_VC_IRQSTATUS(channel), val);
		printk(KERN_DEBUG "short Packet  success");
		return 0;
	} else {
		printk("short Packet sent fail");
	}


	return 0;
}

int dsi_vc_send_null(enum dsi lcd_ix, int channel)
{
	u8 nullpkg[] = {0, 0, 0, 0};
	return dsi_vc_send_long(lcd_ix, 0, DSI_DT_NULL_PACKET, nullpkg, 4, 0);
}
EXPORT_SYMBOL(dsi_vc_send_null);

int dsi_vc_dcs_write_nosync(enum dsi lcd_ix, int channel, u8 *data, int len)
{
	int r = 0;

	BUG_ON(len == 0);

	if (len == 1) {
		r = dsi_vc_send_short(lcd_ix, channel, DSI_DT_DCS_SHORT_WRITE_0,
				data[0], 0);
	} else if (len == 2) {
		r = dsi_vc_send_short(lcd_ix, channel, DSI_DT_DCS_SHORT_WRITE_1,
				data[0] | (data[1] << 8), 0);
	} else {
		/* 0x39 = DCS Long Write */
		r = dsi_vc_send_long(lcd_ix, channel, DSI_DT_DCS_LONG_WRITE,
				data, len, 0);
	}

	return r;
}

EXPORT_SYMBOL(dsi_vc_dcs_write_nosync);

int dsi_vc_dcs_write(enum dsi lcd_ix, int channel, u8 *data, int len)
{
	int r = 0;

	r = dsi_vc_dcs_write_nosync(lcd_ix, channel, data, len);

	if (!cpu_is_omap44xx())/*will be removed after bta works*/
		r = dsi_vc_send_bta_sync(lcd_ix, channel);

	return r;
}

EXPORT_SYMBOL(dsi_vc_dcs_write);

int dsi_vc_dcs_read(enum dsi lcd_ix, int channel, u8 dcs_cmd,
	u8 *buf, int buflen)
{
	u32 val;
	u8 dt;
	int r;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	if (p_dsi->debug_read)
		DSSDBG("dsi_vc_dcs_read(ch%d, dcs_cmd %u)\n", channel, dcs_cmd);

	r = dsi_vc_send_short(lcd_ix, channel, DSI_DT_DCS_READ, dcs_cmd, 0);
	if (r)
		return r;

	r = dsi_vc_send_bta_sync(lcd_ix, channel);
	if (r)
		return r;

	/* RX_FIFO_NOT_EMPTY */
	if (REG_GET(lcd_ix, DSI_VC_CTRL(channel), 20, 20) == 0) {
		DSSERR("RX fifo empty when trying to read.\n");
		return -EIO;
	}

	val = dsi_read_reg(lcd_ix, DSI_VC_SHORT_PACKET_HEADER(channel));
	if (p_dsi->debug_read)
		DSSDBG("\theader: %08x\n", val);
	dt = FLD_GET(val, 5, 0);
	if (dt == DSI_DT_RX_ACK_WITH_ERR) {
		u16 err = FLD_GET(val, 23, 8);
		dsi_show_rx_ack_with_err(err);
		return -EIO;

	} else if (dt == DSI_DT_RX_SHORT_READ_1) {
		u8 data = FLD_GET(val, 15, 8);
		if (p_dsi->debug_read)
			DSSDBG("\tDCS short response, 1 byte: %02x\n", data);

		if (buflen < 1)
			return -EIO;

		buf[0] = data;

		return 1;
	} else if (dt == DSI_DT_RX_SHORT_READ_2) {
		u16 data = FLD_GET(val, 23, 8);
		if (p_dsi->debug_read)
			DSSDBG("\tDCS short response, 2 byte: %04x\n", data);

		if (buflen < 2)
			return -EIO;

		buf[0] = data & 0xff;
		buf[1] = (data >> 8) & 0xff;

		return 2;
	} else if (dt == DSI_DT_RX_DCS_LONG_READ) {
		int w;
		int len = FLD_GET(val, 23, 8);
		if (p_dsi->debug_read)
			DSSDBG("\tDCS long response, len %d\n", len);

		if (len > buflen)
			return -EIO;

		/* two byte checksum ends the packet, not included in len */
		for (w = 0; w < len + 2;) {
			int b;
			val = dsi_read_reg(lcd_ix,
				DSI_VC_SHORT_PACKET_HEADER(channel));
			if (p_dsi->debug_read)
				DSSDBG("\t\t%02x %02x %02x %02x\n",
						(val >> 0) & 0xff,
						(val >> 8) & 0xff,
						(val >> 16) & 0xff,
						(val >> 24) & 0xff);

			for (b = 0; b < 4; ++b) {
				if (w < len)
					buf[w] = (val >> (b * 8)) & 0xff;
				/* we discard the 2 byte checksum */
				++w;
			}
		}

		return len;

	} else {
		DSSERR("\tunknown datatype 0x%02x\n", dt);
		return -EIO;
	}
}
EXPORT_SYMBOL(dsi_vc_dcs_read);


int dsi_vc_set_max_rx_packet_size(enum dsi lcd_ix, int channel, u16 len)
{
	int r;
	r = dsi_vc_send_short(lcd_ix, channel, DSI_DT_SET_MAX_RET_PKG_SIZE,
			len, 0);

	if (r)
		return r;

	r = dsi_vc_send_bta_sync(lcd_ix, channel);

	return r;
}
EXPORT_SYMBOL(dsi_vc_set_max_rx_packet_size);

static void dsi_set_lp_rx_timeout(enum dsi lcd_ix, unsigned long ns)
{
	u32 r;
	unsigned x4 = 0, x16 = 0;
	unsigned long fck = 0;
	unsigned long ticks = 0;

	/* ticks in DSI_FCK */
	if (!cpu_is_omap44xx()) {

	fck = dsi_fclk_rate();
	ticks = (fck / 1000 / 1000) * ns / 1000;
	x4 = 0;
	x16 = 0;

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / 4;
		x4 = 1;
		x16 = 0;
	}

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / 16;
		x4 = 0;
		x16 = 1;
	}

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / (4 * 16);
		x4 = 1;
		x16 = 1;
	}

	if (ticks > 0x1fff) {
		DSSWARN("LP_TX_TO over limit, setting it to max\n");
		ticks = 0x1fff;
		x4 = 1;
		x16 = 1;
	}
		}

	r = dsi_read_reg(lcd_ix, DSI_TIMING2);

	if (!cpu_is_omap44xx()) {
		r = FLD_MOD(r, 1, 15, 15);	/* LP_RX_TO */
		r = FLD_MOD(r, x16, 14, 14);	/* LP_RX_TO_X16 */
		r = FLD_MOD(r, x4, 13, 13);	/* LP_RX_TO_X4 */
		r = FLD_MOD(r, ticks, 12, 0);	/* LP_RX_COUNTER */
		} else
			r = FLD_MOD(r, 0, 15, 15);	/* LP_RX_TO */

	dsi_write_reg(lcd_ix, DSI_TIMING2, r);

	if (!cpu_is_omap44xx())	/*will be removed after proper clock calculations*/
		DSSDBG("LP_RX_TO %lu ns (%#lx ticks%s%s)\n",
			(ticks * (x16 ? 16 : 1) * (x4 ? 4 : 1) * 1000) /
			(fck / 1000 / 1000),
			ticks, x4 ? " x4" : "", x16 ? " x16" : "");
}

static void dsi_set_ta_timeout(enum dsi lcd_ix, unsigned long ns)
{
	u32 r;
	unsigned x8 = 0, x16 = 0;
	unsigned long fck = 0;
	unsigned long ticks = 0;

	/* ticks in DSI_FCK */
	if (!cpu_is_omap44xx()) {
	fck = dsi_fclk_rate();
	ticks = (fck / 1000 / 1000) * ns / 1000;
	x8 = 0;
	x16 = 0;

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / 8;
		x8 = 1;
		x16 = 0;
	}

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / 16;
		x8 = 0;
		x16 = 1;
	}

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / (8 * 16);
		x8 = 1;
		x16 = 1;
	}

	if (ticks > 0x1fff) {
		DSSWARN("TA_TO over limit, setting it to max\n");
		ticks = 0x1fff;
		x8 = 1;
		x16 = 1;
	}
		}

	r = dsi_read_reg(lcd_ix, DSI_TIMING1);

	r = FLD_MOD(r, (cpu_is_omap44xx()) ? 0 : 1,
		31, 31);/* TA_TO : 0x0 turn around is off*/
	r = FLD_MOD(r, (cpu_is_omap44xx()) ? 0 : x16,
		30, 30);	/* TA_TO_X16 */
	r = FLD_MOD(r, (cpu_is_omap44xx()) ? 0 : x8,
		29, 29);	/* TA_TO_X8 */
	r = FLD_MOD(r, (cpu_is_omap44xx()) ? 0 : ticks,
		28, 16);	/* TA_TO_COUNTER */

	dsi_write_reg(lcd_ix, DSI_TIMING1, r);


	if (!cpu_is_omap44xx())	/*will be removed after proper clock calculations*/
		DSSDBG("TA_TO %lu ns (%#lx ticks%s%s)\n",
			(ticks * (x16 ? 16 : 1) * (x8 ? 8 : 1) * 1000) /
			(fck / 1000 / 1000),
			ticks, x8 ? " x8" : "", x16 ? " x16" : "");
}

static void dsi_set_stop_state_counter(enum dsi lcd_ix, unsigned long ns)
{
	u32 r;
	unsigned x4 = 0, x16 = 0;
	unsigned long fck = 0;
	unsigned long ticks = 0;

	/* ticks in DSI_FCK */
	if (!cpu_is_omap44xx()) {
	fck = dsi_fclk_rate();
	ticks = (fck / 1000 / 1000) * ns / 1000;
	x4 = 0;
	x16 = 0;

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / 4;
		x4 = 1;
		x16 = 0;
	}

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / 16;
		x4 = 0;
		x16 = 1;
	}

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / (4 * 16);
		x4 = 1;
		x16 = 1;
	}

	if (ticks > 0x1fff) {
		DSSWARN("STOP_STATE_COUNTER_IO over limit, "
				"setting it to max\n");
		ticks = 0x1fff;
		x4 = 1;
		x16 = 1;
	}
		}

	r = dsi_read_reg(lcd_ix, DSI_TIMING1);
	r = FLD_MOD(r, 1, 15, 15);	/* FORCE_TX_STOP_MODE_IO */
	r = FLD_MOD(r, (cpu_is_omap44xx()) ? 1 : x16,
		14, 14);	/* STOP_STATE_X16_IO */
	r = FLD_MOD(r, (cpu_is_omap44xx()) ? 0 : x4,
		13, 13);	/* STOP_STATE_X4_IO */
	r = FLD_MOD(r, (cpu_is_omap44xx()) ? 0x999 : ticks,
		12, 0);/* STOP_STATE_COUNTER_IO */
	dsi_write_reg(lcd_ix, DSI_TIMING1, r);

	if (!cpu_is_omap44xx())	/*will be removed after proper clock calculations*/
		DSSDBG("STOP_STATE_COUNTER %lu ns (%#lx ticks%s%s)\n",
			(ticks * (x16 ? 16 : 1) * (x4 ? 4 : 1) * 1000) /
			(fck / 1000 / 1000),
			ticks, x4 ? " x4" : "", x16 ? " x16" : "");
}

static void dsi_set_hs_tx_timeout(enum dsi lcd_ix, unsigned long ns)
{
	u32 r;
	unsigned x4 = 0, x16 = 0;
	unsigned long fck = 0;
	unsigned long ticks = 0;

	/* ticks in TxByteClkHS */
if (!cpu_is_omap44xx()) {

	fck = dsi_get_txbyteclkhs(lcd_ix);
	ticks = (fck / 1000 / 1000) * ns / 1000;
	x4 = 0;
	x16 = 0;

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / 4;
		x4 = 1;
		x16 = 0;
	}

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / 16;
		x4 = 0;
		x16 = 1;
	}

	if (ticks > 0x1fff) {
		ticks = (fck / 1000 / 1000) * ns / 1000 / (4 * 16);
		x4 = 1;
		x16 = 1;
	}

	if (ticks > 0x1fff) {
		DSSWARN("HS_TX_TO over limit, setting it to max\n");
		ticks = 0x1fff;
		x4 = 1;
		x16 = 1;
	}
		}

	r = dsi_read_reg(lcd_ix, DSI_TIMING2);

	if (!cpu_is_omap44xx()) {
		r = FLD_MOD(r, 1, 31, 31);	/* HS_TX_TO */
		r = FLD_MOD(r, x16, 30, 30);	/* HS_TX_TO_X16 */
		r = FLD_MOD(r, x4, 29, 29);	/* HS_TX_TO_X8 (4 really) */
		r = FLD_MOD(r, ticks, 28, 16);	/* HS_TX_TO_COUNTER */
	} else
		r = FLD_MOD(r, 0, 31, 31);	/* HS_TX_TO */

	dsi_write_reg(lcd_ix, DSI_TIMING2, r);

	if (!cpu_is_omap44xx())
		DSSDBG("HS_TX_TO %lu ns (%#lx ticks%s%s)\n",
			(ticks * (x16 ? 16 : 1) * (x4 ? 4 : 1) * 1000) /
			(fck / 1000 / 1000),
			ticks, x4 ? " x4" : "", x16 ? " x16" : "");
}
static int dsi_proto_config(struct omap_dss_device *dssdev)
{
	u32 r;
	int buswidth = 0;
	int div;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;

	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	dsi_config_tx_fifo(lcd_ix, DSI_FIFO_SIZE_128,
			DSI_FIFO_SIZE_0,
			DSI_FIFO_SIZE_0,
			DSI_FIFO_SIZE_0);

	dsi_config_rx_fifo(lcd_ix, DSI_FIFO_SIZE_128,
			DSI_FIFO_SIZE_0,
			DSI_FIFO_SIZE_0,
			DSI_FIFO_SIZE_0);

	/* XXX what values for the timeouts? */
	dsi_set_stop_state_counter(lcd_ix, 1000);
	dsi_set_ta_timeout(lcd_ix, 6400000);
	dsi_set_lp_rx_timeout(lcd_ix, 48000);
	dsi_set_hs_tx_timeout(lcd_ix, 1000000);

	switch (dssdev->ctrl.pixel_size) {
	case 16:
		buswidth = 0;
		break;
	case 18:
		buswidth = 1;
		break;
	case 24:
		buswidth = 2;
		break;
	default:
		BUG();
	}

if (!cpu_is_omap44xx()) {

	r = dsi_read_reg(lcd_ix, DSI_CTRL);
	r = FLD_MOD(r, 1, 1, 1);	/* CS_RX_EN */
	r = FLD_MOD(r, 1, 2, 2);	/* ECC_RX_EN */
	r = FLD_MOD(r, 1, 3, 3);	/* TX_FIFO_ARBITRATION */
	r = FLD_MOD(r, 1, 4, 4);	/* VP_CLK_RATIO, always 1, see errata*/
	r = FLD_MOD(r, buswidth, 7, 6); /* VP_DATA_BUS_WIDTH */
	r = FLD_MOD(r, 0, 8, 8);	/* VP_CLK_POL */
	r = FLD_MOD(r, 2, 13, 12);	/* LINE_BUFFER, 2 lines */
	r = FLD_MOD(r, 1, 14, 14);	/* TRIGGER_RESET_MODE */
	r = FLD_MOD(r, 1, 19, 19);	/* EOT_ENABLE */
	r = FLD_MOD(r, 1, 24, 24);	/* DCS_CMD_ENABLE */
	r = FLD_MOD(r, 0, 25, 25);	/* DCS_CMD_CODE, 1=start, 0=continue */

	dsi_write_reg(lcd_ix, DSI_CTRL, r);

} else {

	dsi_write_reg(lcd_ix, DSI_VM_TIMING1, 0x02004006);
	dsi_write_reg(lcd_ix, DSI_VM_TIMING2, 0x04010001);
	dsi_write_reg(lcd_ix, DSI_VM_TIMING3, 0x036F01E0);
	dsi_write_reg(lcd_ix, DSI_VM_TIMING4, 0x00487296);
	dsi_write_reg(lcd_ix, DSI_VM_TIMING5, 0x0082DF3B);
	dsi_write_reg(lcd_ix, DSI_VM_TIMING6, 0x7A6731D1);

	div = 1; /*sv3*/
	r = dsi_read_reg(lcd_ix, DSI_CTRL);
	r = FLD_MOD(r, 0, 1, 1);	/* CS_RX_EN */
	r = FLD_MOD(r, 0, 2, 2);	/* ECC_RX_EN */
	r = FLD_MOD(r, 1, 3, 3);	/* TX_FIFO_ARBITRATION */
	r = FLD_MOD(r, 1, 11, 11); 	/*VP_VSYNC_POL */
	r = FLD_MOD(r, 1, 9, 9); 	/*VP_DE_POL */
	r = FLD_MOD(r, div == 2 ? 0 : 1, 4, 4);	/* VP_CLK_RATIO */
	r = FLD_MOD(r, buswidth, 7, 6); /* VP_DATA_BUS_WIDTH */
	r = FLD_MOD(r, 0, 8, 8);	/* VP_CLK_POL */
	r = FLD_MOD(r, 2, 13, 12);	/* LINE_BUFFER, 2 lines */
	r = FLD_MOD(r, 1, 14, 14);	/* TRIGGER_RESET_MODE */
	dsi_write_reg(lcd_ix, DSI_CTRL,r);

		}

	dsi_vc_initial_config(lcd_ix, 0);
	dsi_vc_config_vp(lcd_ix, 1);

	/* set all vc targets to peripheral 0 */
	p_dsi->vc[0].dest_per = 0;
	p_dsi->vc[1].dest_per = 0;
	p_dsi->vc[2].dest_per = 0;
	p_dsi->vc[3].dest_per = 0;

	return 0;
}

static void dsi_proto_timings(struct omap_dss_device *dssdev)
{
	unsigned tlpx, tclk_zero, tclk_prepare, tclk_trail;
	unsigned tclk_pre, tclk_post;
	unsigned ths_prepare, ths_prepare_ths_zero, ths_zero;
	unsigned ths_trail, ths_exit;
	unsigned ddr_clk_pre, ddr_clk_post;
	unsigned enter_hs_mode_lat, exit_hs_mode_lat;
	unsigned ths_eot;
	u32 r;
	unsigned int val;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;

	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

if (!cpu_is_omap44xx()) {

	r = dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG0);
	ths_prepare = FLD_GET(r, 31, 24);
	ths_prepare_ths_zero = FLD_GET(r, 23, 16);
	ths_zero = ths_prepare_ths_zero - ths_prepare;
	ths_trail = FLD_GET(r, 15, 8);
	ths_exit = FLD_GET(r, 7, 0);

	r = dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG1);
	tlpx = FLD_GET(r, 22, 16) * 2;
	tclk_trail = FLD_GET(r, 15, 8);
	tclk_zero = FLD_GET(r, 7, 0);

	r = dsi_read_reg(lcd_ix, DSI_DSIPHY_CFG2);
	tclk_prepare = FLD_GET(r, 7, 0);

	/* min 8*UI */
	tclk_pre = 20;
	/* min 60ns + 52*UI */
	tclk_post = ns2ddr(lcd_ix, 60) + 26;

	/* ths_eot is 2 for 2 datalanes and 4 for 1 datalane */
	if (dssdev->phy.dsi.data1_lane != 0 &&
			dssdev->phy.dsi.data2_lane != 0)
		ths_eot = 2;
	else
		ths_eot = 4;

	ddr_clk_pre = DIV_ROUND_UP(tclk_pre + tlpx + tclk_zero + tclk_prepare,
			4);
	ddr_clk_post = DIV_ROUND_UP(tclk_post + ths_trail, 4) + ths_eot;

	BUG_ON(ddr_clk_pre == 0 || ddr_clk_pre > 255);
	BUG_ON(ddr_clk_post == 0 || ddr_clk_post > 255);

	ddr_clk_pre = 0xA; 	/*sv3*/
	ddr_clk_post = 0x9;	/*sv3*/
	r = dsi_read_reg(lcd_ix, DSI_CLK_TIMING);
	r = FLD_MOD(r, ddr_clk_pre, 15, 8);
	r = FLD_MOD(r, ddr_clk_post, 7, 0);
	dsi_write_reg(lcd_ix, DSI_CLK_TIMING, r);

	DSSDBG("ddr_clk_pre %u, ddr_clk_post %u\n",
			ddr_clk_pre,
			ddr_clk_post);

	enter_hs_mode_lat = 1 + DIV_ROUND_UP(tlpx, 4) +
		DIV_ROUND_UP(ths_prepare, 4) +
		DIV_ROUND_UP(ths_zero + 3, 4);

	exit_hs_mode_lat = DIV_ROUND_UP(ths_trail + ths_exit, 4) + 1 + ths_eot;
	enter_hs_mode_lat = 7; /*sv3*/
	exit_hs_mode_lat = 9; /*sv3*/
	r = FLD_VAL(enter_hs_mode_lat, 31, 16) |
		FLD_VAL(exit_hs_mode_lat, 15, 0);
	dsi_write_reg(lcd_ix, DSI_VM_TIMING7, r);

	DSSDBG("enter_hs_mode_lat %u, exit_hs_mode_lat %u\n",
			enter_hs_mode_lat, exit_hs_mode_lat);
} else {
	dsi_write_reg(lcd_ix, DSI_VM_TIMING7, 0x00090007);

	/* config DDR CLK timer*/
	val = dsi_read_reg(lcd_ix, DSI_CLK_TIMING);
	val = (10 << 8) |
		  (9 << 0);
	dsi_write_reg(lcd_ix, DSI_CLK_TIMING, val);
	}
}


#define DSI_DECL_VARS \
	int __dsi_cb = 0; u32 __dsi_cv = 0;

#define DSI_FLUSH(no, ch) \
	if (__dsi_cb > 0) { \
		/*DSSDBG("sending long packet %#010x\n", __dsi_cv);*/ \
		dsi_write_reg(no, DSI_VC_LONG_PACKET_PAYLOAD(ch), __dsi_cv); \
		__dsi_cb = __dsi_cv = 0; \
	}

#define DSI_PUSH(no, ch, data) \
	do { \
		__dsi_cv |= (data) << (__dsi_cb * 8); \
		/*DSSDBG("cv = %#010x, cb = %d\n", __dsi_cv, __dsi_cb);*/ \
		if (++__dsi_cb > 3) \
			DSI_FLUSH(no, ch); \
	} while (0)

static int dsi_update_screen_l4(enum dsi lcd_ix,
			struct omap_dss_device *dssdev, int x, int y, int w, int h)
{
	/* Note: supports only 24bit colors in 32bit container */
	int first = 1;
	int fifo_stalls = 0;
	int max_dsi_packet_size;
	int max_data_per_packet;
	int max_pixels_per_packet;
	int pixels_left;
	int bytespp = dssdev->ctrl.pixel_size / 8;
	int scr_width;
	u32 __iomem *data;
	int start_offset;
	int horiz_inc;
	int current_x;
	struct omap_overlay *ovl;
			struct dsi_struct *p_dsi;
			p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	debug_irq = 0;

	DSSDBG("dsi_update_screen_l4 (%d,%d %dx%d)\n",
			x, y, w, h);

	ovl = dssdev->manager->overlays[0];

	if (ovl->info.color_mode != OMAP_DSS_COLOR_RGB24U)
		return -EINVAL;

	if (dssdev->ctrl.pixel_size != 24)
		return -EINVAL;

	scr_width = ovl->info.screen_width;
	data = ovl->info.vaddr;

	start_offset = scr_width * y + x;
	horiz_inc = scr_width - w;
	current_x = x;

	/* We need header(4) + DCSCMD(1) + pixels(numpix*bytespp) bytes
	 * in fifo */

	/* When using CPU, max long packet size is TX buffer size */
			max_dsi_packet_size = p_dsi->vc[0].fifo_size * 32 * 4;

	/* we seem to get better perf if we divide the tx fifo to half,
	   and while the other half is being sent, we fill the other half
	   max_dsi_packet_size /= 2; */

	max_data_per_packet = max_dsi_packet_size - 4 - 1;

	max_pixels_per_packet = max_data_per_packet / bytespp;

	DSSDBG("max_pixels_per_packet %d\n", max_pixels_per_packet);

	pixels_left = w * h;

	DSSDBG("total pixels %d\n", pixels_left);

	data += start_offset;

	while (pixels_left > 0) {
		/* 0x2c = write_memory_start */
		/* 0x3c = write_memory_continue */
		u8 dcs_cmd = first ? 0x2c : 0x3c;
		int pixels;
		DSI_DECL_VARS;
		first = 0;

#if 1
				/* using fifo not empty */
				/* TX_FIFO_NOT_EMPTY */
				while (FLD_GET(dsi_read_reg(lcd_ix, DSI_VC_CTRL(0)), 5, 5)) {
					udelay(1);
					fifo_stalls++;
					if (fifo_stalls > 0xfffff) {
						DSSERR("fifo stalls overflow, pixels left %d\n",
								pixels_left);
						dsi_if_enable(lcd_ix, 0);
						return -EIO;
					}
				}
#endif
		pixels = min(max_pixels_per_packet, pixels_left);

		pixels_left -= pixels;

				dsi_vc_write_long_header(lcd_ix, 0, DSI_DT_DCS_LONG_WRITE,
				1 + pixels * bytespp, 0);

				DSI_PUSH(lcd_ix, 0, dcs_cmd);

		while (pixels-- > 0) {
			u32 pix = __raw_readl(data++);

					DSI_PUSH(lcd_ix, 0, (pix >> 16) & 0xff);
					DSI_PUSH(lcd_ix, 0, (pix >> 8) & 0xff);
					DSI_PUSH(lcd_ix, 0, (pix >> 0) & 0xff);

			current_x++;
			if (current_x == x+w) {
				current_x = x;
				data += horiz_inc;
			}
		}

				DSI_FLUSH(lcd_ix, 0);
	}

	return 0;
}

static void dsi_update_screen_dispc(enum dsi lcd_ix,
	struct omap_dss_device *dssdev, u16 x, u16 y,
	u16 w, u16 h)
{
	unsigned bytespp;
	unsigned bytespl;
	unsigned bytespf;
	unsigned total_len;
	unsigned packet_payload;
	unsigned packet_len;
	u32 l;
	bool use_te_trigger;
	const unsigned channel = 0+1;  /*HS mode sv*/
	/* line buffer is 1024 x 24bits */
	/* XXX: for some reason using full buffer size causes considerable TX
	 * slowdown with update sizes that fill the whole buffer */
	const unsigned line_buf_size = 1023 * 3;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	use_te_trigger = p_dsi->te_enabled && !p_dsi->use_ext_te;
	if (use_te_trigger)
		use_te_trigger = 0; /*sv HS mode*/

	if (p_dsi->update_mode != OMAP_DSS_UPDATE_AUTO)
		DSSDBG("dsi_update_screen_dispc(%d,%d %dx%d)\n",
				x, y, w, h);

	bytespp	= dssdev->ctrl.pixel_size / 8;
	bytespl = w * bytespp;
	bytespf = bytespl * h;

	/* NOTE: packet_payload has to be equal to N * bytespl, where N is
	 * number of lines in a packet.  See errata about VP_CLK_RATIO */

	if (bytespf < line_buf_size)
		packet_payload = bytespf;
	else
		packet_payload = (line_buf_size) / bytespl * bytespl;

	packet_len = packet_payload + 1;	/* 1 byte for DCS cmd */
	total_len = (bytespf / packet_payload) * packet_len;

	if (bytespf % packet_payload)
		total_len += (bytespf % packet_payload) + 1;

	if (0)
		dsi_vc_print_status(lcd_ix, 1);

	l = FLD_VAL(total_len, 23, 0); /* TE_SIZE */
	dsi_write_reg(lcd_ix, DSI_VC_TE(channel), l);

	dsi_vc_write_long_header(lcd_ix, channel, DSI_DT_DCS_LONG_WRITE,
		packet_len, 0);

	if (use_te_trigger)
		l = FLD_MOD(l, 1, 30, 30); /* TE_EN */
	else
		l = FLD_MOD(l, 1, 31, 31); /* TE_START */
	dsi_write_reg(lcd_ix, DSI_VC_TE(channel), l);

	/* We put SIDLEMODE to no-idle for the duration of the transfer,
	 * because DSS interrupts are not capable of waking up the CPU and the
	 * framedone interrupt could be delayed for quite a long time. I think
	 * the same goes for any DSS interrupts, but for some reason I have not
	 * seen the problem anywhere else than here.
	 */
	dispc_disable_sidle();

	dss_start_update(dssdev);

	if (use_te_trigger) {
		/* disable LP_RX_TO, so that we can receive TE.  Time to wait
		 * for TE is longer than the timer allows */
		REG_FLD_MOD(lcd_ix, DSI_TIMING2, 0, 15, 15); /* LP_RX_TO */

		dsi_vc_send_bta(lcd_ix, channel);

#ifdef DSI_CATCH_MISSING_TE
		mod_timer(&(p_dsi->te_timer), jiffies + msecs_to_jiffies(250));
#endif
	}
}

#ifdef DSI_CATCH_MISSING_TE
static void dsi_te_timeout(unsigned long arg)
{
	DSSERR("TE not received for 250ms!\n");
}
#endif

static void dsi_framedone_irq_callback(void *data, u32 mask)
{
	/* Note: We get FRAMEDONE when DISPC has finished sending pixels and
	 * turns itself off. However, DSI still has the pixels in its buffers,
	 * and is sending the data.
	 */

	/* SIDLEMODE back to smart-idle */
	dispc_enable_sidle();
	udelay(100);
	dsi_1.framedone_received = true;
	wake_up(&dsi_1.waitqueue);
}

#ifdef CONFIG_ARCH_OMAP4
static void dsi2_framedone_irq_callback(void *data, u32 mask)
{
	/* Note: We get FRAMEDONE when DISPC has finished sending pixels and
	 * turns itself off. However, DSI still has the pixels in its buffers,
	 * and is sending the data.
	 */
	/* SIDLEMODE back to smart-idle */
	dispc_enable_sidle();
	udelay(100);
	dsi_2.framedone_received = true;
	wake_up(&dsi_2.waitqueue);
}
#endif

static void dsi_set_update_region(enum dsi lcd_ix,
	struct omap_dss_device *dssdev, u16 x, u16 y,
	u16 w, u16 h)
{
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	spin_lock(&(p_dsi->update_lock));
	if (p_dsi->update_region.dirty) {
		p_dsi->update_region.x = min(x, p_dsi->update_region.x);
		p_dsi->update_region.y = min(y, p_dsi->update_region.y);
		p_dsi->update_region.w = max(w, p_dsi->update_region.w);
		p_dsi->update_region.h = max(h, p_dsi->update_region.h);
	} else {
		p_dsi->update_region.x = x;
		p_dsi->update_region.y = y;
		p_dsi->update_region.w = w;
		p_dsi->update_region.h = h;
	}

	p_dsi->update_region.device = dssdev;
	p_dsi->update_region.dirty = true;

	spin_unlock(&(p_dsi->update_lock));

}

static int dsi_set_update_mode(enum dsi lcd_ix, struct omap_dss_device *dssdev,
		enum omap_dss_update_mode mode)
{
	int r = 0;
	int i;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	WARN_ON(!mutex_is_locked(&(p_dsi->bus_lock)));

	if (p_dsi->update_mode != mode) {
		p_dsi->update_mode = mode;

	/*sv HS mode set the GFX threshold there properly before apply*/
		/* Mark the overlays dirty, and do apply(), so that we get the
		 * overlays configured properly after update mode change. */
		for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
			struct omap_overlay *ovl;
			ovl = omap_dss_get_overlay(i);
			if (ovl->manager == dssdev->manager)
				ovl->info_dirty = true;
		}

		r = dssdev->manager->apply(dssdev->manager);

		if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE &&
				mode == OMAP_DSS_UPDATE_AUTO) {

			u16 w, h;

			DSSDBG("starting auto update\n");

			dssdev->get_resolution(dssdev, &w, &h);

			dsi_set_update_region(lcd_ix, dssdev, 0, 0, w, h);

			dsi_perf_mark_start_auto(lcd_ix);

			wake_up(&(p_dsi->waitqueue));
		}
	}

	return r;
}

static int dsi_set_te(struct omap_dss_device *dssdev, bool enable)
{
	int r;
	r = dssdev->driver->enable_te(dssdev, enable);
	/* XXX for some reason, DSI TE breaks if we don't wait here.
	 * Panel bug? Needs more studying */
	msleep(100);
	return r;
}

static void dsi_handle_framedone(enum dsi lcd_ix)
{
	const int channel = 0;
	bool use_te_trigger;
	struct dsi_struct *p_dsi;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	use_te_trigger = p_dsi->te_enabled && !p_dsi->use_ext_te;
	if(use_te_trigger)
		use_te_trigger = 0; /*sv HS mode*/

	if (p_dsi->update_mode != OMAP_DSS_UPDATE_AUTO)
		DSSDBG("FRAMEDONE\n");

	if (use_te_trigger) {
		/* enable LP_RX_TO again after the TE */
		REG_FLD_MOD(lcd_ix, DSI_TIMING2, 1, 15, 15); /* LP_RX_TO */
	}

	/* Send BTA after the frame. We need this for the TE to work, as TE
	 * trigger is only sent for BTAs without preceding packet. Thus we need
	 * to BTA after the pixel packets so that next BTA will cause TE
	 * trigger.
	 *
	 * This is not needed when TE is not in use, but we do it anyway to
	 * make sure that the transfer has been completed. It would be more
	 * optimal, but more complex, to wait only just before starting next
	 * transfer. */
#if 0
	r = dsi_vc_send_bta_sync(channel);
	if (r)
		DSSERR("BTA after framedone failed\n");
#endif
	/* RX_FIFO_NOT_EMPTY */
	if (REG_GET(lcd_ix, DSI_VC_CTRL(channel), 20, 20)) {
		DSSERR("Received error during frame transfer:\n");
		dsi_vc_flush_receive_data(lcd_ix, 0);
	}

#ifdef CONFIG_OMAP2_DSS_FAKE_VSYNC
	dispc_fake_vsync_irq(lcd_ix);
#endif
}

static int dsi_update_thread(void *data)
{
	unsigned long timeout;
	struct omap_dss_device *device;
	u16 x, y, w, h;
	enum dsi lcd_ix = dsi1;
	while (1) {
		bool sched;

		wait_event_interruptible(dsi_1.waitqueue,
				dsi_1.update_mode == OMAP_DSS_UPDATE_AUTO ||
				(dsi_1.update_mode == OMAP_DSS_UPDATE_MANUAL &&
				 dsi_1.update_region.dirty == true) ||
				kthread_should_stop());

		if (kthread_should_stop())
			break;

		dsi_bus_lock(lcd_ix);

		if (dsi_1.update_mode == OMAP_DSS_UPDATE_DISABLED ||
			kthread_should_stop()) {
			dsi_bus_unlock(lcd_ix);
			break;
		}

		dsi_perf_mark_setup(lcd_ix);

		if (dsi_1.update_region.dirty) {
			spin_lock(&dsi_1.update_lock);
			dsi_1.active_update_region = dsi_1.update_region;
			dsi_1.update_region.dirty = false;
			spin_unlock(&dsi_1.update_lock);
		}

		device = dsi_1.active_update_region.device;
		x = dsi_1.active_update_region.x;
		y = dsi_1.active_update_region.y;
		w = dsi_1.active_update_region.w;
		h = dsi_1.active_update_region.h;

		if (device->manager->caps & OMAP_DSS_OVL_MGR_CAP_DISPC) {

			if (dsi_1.update_mode == OMAP_DSS_UPDATE_MANUAL)
				dss_setup_partial_planes(device,
						&x, &y, &w, &h);

			dispc_set_lcd_size(OMAP_DSS_CHANNEL_LCD, w, h);
			/* TODO: Correct this while adding support for LCD2 */
		}

		if (dsi_1.active_update_region.dirty) {
			dsi_1.active_update_region.dirty = false;
			/* XXX TODO we don't need to send the coords, if they
			 * are the same that are already programmed to the
			 * panel. That should speed up manual update a bit */
			device->driver->setup_update(device, x, y, w, h);
		}

		dsi_perf_mark_start(lcd_ix);

		if (device->manager->caps & OMAP_DSS_OVL_MGR_CAP_DISPC) {
			/*HS mode dsi_vc_config_vp(1);Video mode use channel1
			Since we have already configured the VC Ctrl
			of Video channel */

			if (dsi_1.te_enabled && dsi_1.use_ext_te)
				device->driver->wait_for_te(device);

			dsi_1.framedone_received = false;

			dsi_update_screen_dispc(lcd_ix, device, x, y, w, h);

			/* wait for framedone */
			timeout = msecs_to_jiffies(1000);
			timeout = wait_event_timeout(dsi_1.waitqueue,
					dsi_1.framedone_received == true,
					timeout);

			if (timeout == 0) {
				DSSERR("framedone timeout\n");
				DSSERR("failed update %d,%d %dx%d\n",
						x, y, w, h);

				dispc_enable_sidle();
				/* TODO: update for LCD2 support */
				dispc_enable_lcd_out(OMAP_DSS_CHANNEL_LCD, 0);

				dsi_reset_tx_fifo(lcd_ix, 0);
			} else {
				dsi_handle_framedone(lcd_ix);
				dsi_perf_show(lcd_ix, "DISPC");
			}
		} else {
			dsi_update_screen_l4(lcd_ix, device, x, y, w, h);
			dsi_perf_show(lcd_ix, "L4");
		}

		sched = atomic_read(&dsi_1.bus_lock.count) < 0;

		complete_all(&dsi_1.update_completion);

		dsi_bus_unlock(lcd_ix);

		/* XXX We need to give others chance to get the bus lock. Is
		 * there a better way for this? */
		if (dsi_1.update_mode == OMAP_DSS_UPDATE_AUTO && sched)
			schedule_timeout_interruptible(1);
	}

	DSSDBG("update thread exiting\n");

	return 0;
}

static int dsi2_update_thread(void *data)
 {
	unsigned long timeout;
	struct omap_dss_device *device;
	u16 x, y, w, h;
	enum dsi lcd_ix = dsi2;
	while (1) {
		bool sched;

		wait_event_interruptible(dsi_2.waitqueue,
				dsi_2.update_mode == OMAP_DSS_UPDATE_AUTO ||
				(dsi_2.update_mode == OMAP_DSS_UPDATE_MANUAL &&
				 dsi_2.update_region.dirty == true) ||
				kthread_should_stop());

		if (kthread_should_stop())
			break;

		dsi_bus_lock(lcd_ix);

		if (dsi_2.update_mode == OMAP_DSS_UPDATE_DISABLED ||
			kthread_should_stop()) {
			dsi_bus_unlock(lcd_ix);
			break;
		}

		dsi_perf_mark_setup(lcd_ix);

		if (dsi_2.update_region.dirty) {
			spin_lock(&dsi_2.update_lock);
			dsi_2.active_update_region = dsi_2.update_region;
			dsi_2.update_region.dirty = false;
			spin_unlock(&dsi_2.update_lock);
		}

		device = dsi_2.active_update_region.device;
		x = dsi_2.active_update_region.x;
		y = dsi_2.active_update_region.y;
		w = dsi_2.active_update_region.w;
		h = dsi_2.active_update_region.h;

		if (device->manager->caps & OMAP_DSS_OVL_MGR_CAP_DISPC) {

			if (dsi_2.update_mode == OMAP_DSS_UPDATE_MANUAL) {
				dss_setup_partial_planes(device,
						&x, &y, &w, &h);

				/* XXX there seems to be a bug in this driver
				 * or OMAP hardware.  Some updates with certain
				 * widths and x coordinates fail. These widths
				 * are always odd, so "fix" it here for now */
				if (w & 1) {
					u16 dw, dh;
					device->get_resolution(device,
						&dw, &dh);
					if (x + w == dw)
						x &= ~1;
					++w;
					dss_setup_partial_planes(device,
							&x, &y, &w, &h);
					}

		}

			dispc_set_lcd_size(DEFAULT_CHANNEL, w, h);
			/* TODO: Correct this while adding support for LCD2 */
		}

		if (dsi_2.active_update_region.dirty) {
			dsi_2.active_update_region.dirty = false;
			/* XXX TODO we don't need to send the coords, if they
			 * are the same that are already programmed to the
			 * panel. That should speed up manual update a bit */
			device->driver->setup_update(device, x, y, w, h);
		}

		dsi_perf_mark_start(lcd_ix);

		if (device->manager->caps & OMAP_DSS_OVL_MGR_CAP_DISPC) {
			/*sv HS mode dsi2_vc_config_vp(0+1);Video mode
			use channel1.Since we already configured VC Ctrl of
			Video channel */

			if (dsi_2.te_enabled && dsi_2.use_ext_te)
				device->driver->wait_for_te(device);

			dsi_2.framedone_received = false;
			dsi_update_screen_dispc(lcd_ix, device, x, y, w, h);

			/* wait for framedone */
			timeout = msecs_to_jiffies(1000);
			timeout = wait_event_timeout(dsi_2.waitqueue,
					dsi_2.framedone_received == true,
					timeout);

			if (timeout == 0) {
				/*DSSERR("dsi2 framedone timeout\n");*/ /*svov3*/
				printk(KERN_DEBUG "DSS_CONTROL = 0x%x",
					__raw_readl(dss_base + 0x40));
				/*svov3 DSSERR("failed update %d,%d %dx%d\n",
						svov3 x, y, w, h);*/

				dispc_enable_sidle();
				/* TODO: update for LCD2 support */
				dispc_enable_lcd_out(DEFAULT_CHANNEL, 0);
			} else {
				dsi_handle_framedone(lcd_ix);
				dsi_perf_show(lcd_ix, "DISPC");
			}
		} else {
			dsi_update_screen_l4(lcd_ix, device, x, y, w, h);
			dsi_perf_show(lcd_ix, "L4");
		}

		sched = atomic_read(&dsi_2.bus_lock.count) < 0;

		complete_all(&dsi_2.update_completion);

		dsi_bus_unlock(lcd_ix);

		/* XXX We need to give others chance to get the bus lock. Is
		 * there a better way for this? */
		 if (dsi_2.update_mode == OMAP_DSS_UPDATE_AUTO && sched)
			schedule_timeout_interruptible(1);
	}

	DSSDBG("update thread exiting\n");

	return 0;
 }


/* Display funcs */

static int dsi_display_init_dispc(struct omap_dss_device *dssdev)
{
	int r = 0;
	enum dsi lcd_ix;
	struct omap_video_timings timings = {
		.hsw		= 4+1,
		.hfp		= 4+1,
		.hbp		= 4+1,
		.vsw		= 0+1, /*before writing to the register it subtracts 1*/
		.vfp		= 0,
		.vbp		= 1,
		.x_res	= 864,
		.y_res	= 480,
	};

	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	if (lcd_ix == dsi1)
	r = omap_dispc_register_isr(dsi_framedone_irq_callback, NULL,
			DISPC_IRQ_FRAMEDONE);
#ifdef CONFIG_ARCH_OMAP4
	else
		r = omap_dispc_register_isr(dsi2_framedone_irq_callback, NULL,
	DISPC_IRQ_FRAMEDONE2);
#endif
	if (r) {
		DSSERR("can't get FRAMEDONE irq\n");
		return r;
	}

	if (lcd_ix == dsi1) {
		dispc_set_lcd_display_type(OMAP_DSS_CHANNEL_LCD,
		OMAP_DSS_LCD_DISPLAY_TFT);

		dispc_set_parallel_interface_mode(OMAP_DSS_CHANNEL_LCD,
					OMAP_DSS_PARALLELMODE_DSI);

		dispc_set_tft_data_lines(OMAP_DSS_CHANNEL_LCD,
			dssdev->ctrl.pixel_size);
	} else {
		dispc_set_lcd_display_type(DEFAULT_CHANNEL,
			OMAP_DSS_LCD_DISPLAY_TFT);

		dispc_set_parallel_interface_mode(DEFAULT_CHANNEL,
					OMAP_DSS_PARALLELMODE_DSI);
		dispc_set_tft_data_lines(DEFAULT_CHANNEL,
			dssdev->ctrl.pixel_size);
		}


	if (lcd_ix == dsi1)
		dispc_set_lcd_timings(OMAP_DSS_CHANNEL_LCD, &timings);
	else
		dispc_set_lcd_timings(DEFAULT_CHANNEL, &timings);


	if (lcd_ix == dsi1) {
		dispc_set_pol_freq(OMAP_DSS_CHANNEL_LCD, dssdev->panel.config,
		dssdev->panel.acbi, dssdev->panel.acb);

		dispc_set_lcd_divisor(OMAP_DSS_CHANNEL_LCD, 1/*lck_div*/,
							0x6/*pck_div*/);
		} else {
			dispc_set_pol_freq(DEFAULT_CHANNEL,
				dssdev->panel.config, dssdev->panel.acbi,
				dssdev->panel.acb);

			dispc_set_lcd_divisor(DEFAULT_CHANNEL,
				1/*lck_div*/, 0x6/*pck_div*/);
		}

	return 0;
}

static void dsi_display_uninit_dispc(struct omap_dss_device *dssdev)
{
	enum dsi lcd_ix;
	int i;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	if (lcd_ix == dsi1) {
		i = 0;
		omap_dispc_unregister_isr(dsi_framedone_irq_callback, NULL,
			DISPC_IRQ_FRAMEDONE);
#ifdef CONFIG_ARCH_OMAP4
	} else {
		i = 1;
		omap_dispc_unregister_isr(dsi2_framedone_irq_callback, NULL,
			DISPC_IRQ_FRAMEDONE2);
#endif
	}

}

static int dsi_configure_dsi_clocks(struct omap_dss_device *dssdev)
{
	struct dsi_clock_info cinfo;
	int r;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	/* we always use DSS2_FCK as input clock */
	cinfo.use_dss2_fck = true;
	cinfo.regn  = dssdev->phy.dsi.div.regn;
	cinfo.regm  = dssdev->phy.dsi.div.regm;
	cinfo.regm3 = dssdev->phy.dsi.div.regm3;
	cinfo.regm4 = dssdev->phy.dsi.div.regm4;

	if (!cpu_is_omap44xx()) {
	r = dsi_calc_clock_rates(&cinfo);
	if (r)
		return r;
		}

	if (cpu_is_omap44xx()) {/*TODO: shouldn't be needed once we have corrected dsi_configure_dsi_clocks ? */
	cinfo.regn = 19;
	cinfo.regm = 102;
	cinfo.regm3 = 4;
	cinfo.regm4 = 4;
	cinfo.use_dss2_fck = true;
	cinfo.highfreq = 0;
	}
	r = dsi_pll_set_clock_div(lcd_ix, &cinfo);
	if (r) {
		DSSERR("Failed to set dsi clocks\n");
		return r;
	}

	return 0;
}

static int dsi_configure_dispc_clocks(struct omap_dss_device *dssdev)
{
	struct dispc_clock_info dispc_cinfo;
	int r;
	unsigned long long fck;

	fck = dsi_get_dsi1_pll_rate();

	dispc_cinfo.lck_div = dssdev->phy.dsi.div.lck_div;
	dispc_cinfo.pck_div = dssdev->phy.dsi.div.pck_div;
	if (!cpu_is_omap44xx()) {
	r = dispc_calc_clock_rates(fck, &dispc_cinfo);
	if (r) {
		DSSERR("Failed to calc dispc clocks\n");
		return r;
	}
		}
	r = dispc_set_clock_div(&dispc_cinfo);
	if (r) {
		DSSERR("Failed to set dispc clocks\n");
		return r;
	}

	return 0;
}

static int dsi_display_init_dsi(struct omap_dss_device *dssdev)
{
	int r;
	int val;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	_dsi_print_reset_status(lcd_ix);

if (cpu_is_omap44xx()) {

	/*Make sure the Virtual channels are disabled before
		configuration*/
	val = dsi_read_reg(lcd_ix, DSI_VC_CTRL(0));
	val = val & ~(1<<0);
	dsi_write_reg(lcd_ix, DSI_VC_CTRL(0), val);

	val = dsi_read_reg(lcd_ix, DSI_VC_CTRL(0+1));
	val = val & ~(1<<0);
	dsi_write_reg(lcd_ix, DSI_VC_CTRL(0+1), val);

	/*Setup the clock control*/
	val = dsi_read_reg(lcd_ix, DSI_CLK_CTRL);
	val = val & ~(DSICLKCTRLLPCLKDIVISOR);

	val = val |
	    (1<<18) |
	    (0<<16) |
	    (0<<15) |  /* 0 -> NULL packet generation disabled */
	    (1<<14) |
	    (1<<13) |
	    (6<<0); /* 50/7 => 7MHz */

	dsi_write_reg(lcd_ix, DSI_CLK_CTRL, val);
}

	r = dsi_pll_init(lcd_ix, dssdev, true, true);
	if (r)
		goto err0;

	r = dsi_configure_dsi_clocks(dssdev); /* TODO: should be dependent on clocks? */
	if (r)
		goto err1;

	dss_select_clk_source_dsi(lcd_ix, true, true);

	DSSDBG("PLL OK\n");

	r = dsi_configure_dispc_clocks(dssdev);
	if (r)
		goto err2;

	if (cpu_is_omap44xx()) {

	if (lcd_ix == 0)
		modify_dss_register(DSS_CONTROL, 0x00000000, 0x003);
	else
		modify_dss_register(DSS_CONTROL, 0x00000000, (1<<12 | 1<<10));

	}

	r = dsi_complexio_init(lcd_ix, dssdev);
	if (r)
		goto err2;

	_dsi_print_reset_status(lcd_ix);

	dsi_proto_timings(dssdev);
	dsi_set_lp_clk_divisor(dssdev);

	if (1)
		_dsi_print_reset_status(lcd_ix);

	r = dsi_proto_config(dssdev);
	if (r)
		goto err3;

	/* enable interface */
	dsi_vc_enable(lcd_ix, 0, 1);
	dsi_vc_enable(lcd_ix, 1, 1);
	dsi_if_enable(lcd_ix, 1);
	dsi_force_tx_stop_mode_io(lcd_ix);

#ifdef CONFIG_ARCH_OMAP4
	val = 0;

	/* Register 12*/
	val = val | (0x58 << 0);
	dsi_write_reg(lcd_ix, DSI_DSIPHY_CFG12, val);

	/* Register 14*/
	val = 0;
	val = val | (1 << 31) | 1<<11 | (0x54 << 23) |
			1<<19 | 1<<18 | (0x7 << 14);
	dsi_write_reg(lcd_ix, DSI_DSIPHY_CFG14, val);

	/* Register 8*/
	val = 0;
	val = val | (1 << 11) | (16 << 6) | 1<<5 | (0xE << 0);
	dsi_write_reg(lcd_ix, DSI_DSIPHY_CFG8, val);
#endif

	/* Around 100ms delay for TAAL to stabilize*/

	mdelay(200);

	if (dssdev->driver->enable) {
		r = dssdev->driver->enable(dssdev);
		if (r)
			goto err4;
	}

	/* enable high-speed after initial config */
	/*dsi_vc_enable_hs(lcd_ix, 0, 1);*/

	return 0;
err4:
	dsi_if_enable(lcd_ix, 0);
err3:
	dsi_complexio_uninit(lcd_ix);
err2:
	dss_select_clk_source_dsi(lcd_ix, false, false);
err1:
	dsi_pll_uninit(lcd_ix);
err0:
	return r;
}

static void dsi_display_uninit_dsi(struct omap_dss_device *dssdev)
{
	enum dsi lcd_ix;

	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	if (dssdev->driver->disable)
		dssdev->driver->disable(dssdev);

	dss_select_clk_source_dsi(lcd_ix, false, false);
	dsi_complexio_uninit(lcd_ix);
	dsi_pll_uninit(lcd_ix);
}

static int dsi_core_init(enum dsi lcd_ix)
{

	REG_FLD_MOD(lcd_ix, DSI_SYSCONFIG, 0, 0, 0);

	/* ENWAKEUP */
	REG_FLD_MOD(lcd_ix, DSI_SYSCONFIG, 1, 2, 2);

	/* SIDLEMODE smart-idle */
	REG_FLD_MOD(lcd_ix, DSI_SYSCONFIG, 2, 4, 3);

	_dsi_initialize_irq(lcd_ix);

	return 0;
}

#define GPIO_OE		0x134
#define GPIO_DATAOUT	0x13C
#define OMAP24XX_GPIO_CLEARDATAOUT	0x190
#define OMAP24XX_GPIO_SETDATAOUT	0x194

static int dsi_display_enable(struct omap_dss_device *dssdev)
{
	int r = 0;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	int val = 0;

	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	DSSDBG("dsi_display_enable\n");

	mutex_lock(&(p_dsi->lock));
	dsi_bus_lock(lcd_ix);

	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err0;
	}

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		DSSERR("dssdev already enabled\n");
		r = -EINVAL;
		goto err1;
	}

	enable_clocks(1);
	dsi_enable_pll_clock(lcd_ix, 1);

	dsi_core_init(lcd_ix);

if (cpu_is_omap44xx())
	omap_writel(0x00030007  , 0x4A307100);  /*DSS_PWR_DSS_DSS_CTRL*/


	r = _dsi_reset(lcd_ix);
	if (r)
		goto err2;

	if (cpu_is_omap44xx() && (lcd_ix == dsi1)) {

		gpio_base=ioremap(0x48059000,0x1000);
		val = __raw_readl(gpio_base+GPIO_OE);
		val &= ~0x140;
		__raw_writel(val, gpio_base+GPIO_OE);
		mdelay(120);

		/* To output signal high */
		val = __raw_readl(gpio_base+OMAP24XX_GPIO_SETDATAOUT);
		val |= 0x140;
		__raw_writel(val, gpio_base+OMAP24XX_GPIO_SETDATAOUT);
		mdelay(120);

		val = __raw_readl(gpio_base+OMAP24XX_GPIO_CLEARDATAOUT);
		val |= 0x140;
		__raw_writel(val, gpio_base+OMAP24XX_GPIO_CLEARDATAOUT);
		mdelay(120);

		val = __raw_readl(gpio_base+OMAP24XX_GPIO_SETDATAOUT);
		val |= 0x140;
		__raw_writel(val, gpio_base+OMAP24XX_GPIO_SETDATAOUT);

		mdelay(120);
		printk(KERN_DEBUG "GPIO reset done ");
	}
	r = dsi_display_init_dispc(dssdev);
	if (r)
		goto err2;

	r = dsi_display_init_dsi(dssdev);
	if (r)
		goto err3;

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	p_dsi->use_ext_te = dssdev->phy.dsi.ext_te;

	r = dsi_set_te(dssdev, p_dsi->te_enabled);
	if (r)
		goto err4;

	if (cpu_is_omap44xx()) {
		if (lcd_ix == dsi2)
			p_dsi->update_mode = OMAP_DSS_UPDATE_AUTO;
	}
	p_dsi->update_mode = OMAP_DSS_UPDATE_DISABLED;
	p_dsi->user_update_mode = OMAP_DSS_UPDATE_AUTO;

	dsi_set_update_mode(lcd_ix, dssdev, p_dsi->user_update_mode);

	dsi_bus_unlock(lcd_ix);
	mutex_unlock(&(p_dsi->lock));

	return 0;

err4:
	dsi_display_uninit_dsi(dssdev);
err3:
	dsi_display_uninit_dispc(dssdev);
err2:
	enable_clocks(0);
	dsi_enable_pll_clock(lcd_ix, 0);
err1:
	omap_dss_stop_device(dssdev);
err0:
	dsi_bus_unlock(lcd_ix);
	mutex_unlock(&(p_dsi->lock));
	DSSDBG("dsi_display_enable FAILED\n");
	return r;
}

static void dsi_display_disable(struct omap_dss_device *dssdev)
{
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	DSSDBG("dsi_display_disable\n");

	mutex_lock(&(p_dsi->lock));
	dsi_bus_lock(lcd_ix);

	if (dssdev->state == OMAP_DSS_DISPLAY_DISABLED ||
			dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED)
		goto end;

	p_dsi->update_mode = OMAP_DSS_UPDATE_DISABLED;
	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;

	dsi_display_uninit_dispc(dssdev);

	dsi_display_uninit_dsi(dssdev);

	enable_clocks(0);
	dsi_enable_pll_clock(lcd_ix, 0);

	omap_dss_stop_device(dssdev);
end:
	dsi_bus_unlock(lcd_ix);
	mutex_unlock(&(p_dsi->lock));
}

static int dsi_display_suspend(struct omap_dss_device *dssdev)
{
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	DSSDBG("dsi_display_suspend\n");

	mutex_lock(&(p_dsi->lock));
	dsi_bus_lock(lcd_ix);

	if (dssdev->state == OMAP_DSS_DISPLAY_DISABLED ||
			dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED)
		goto end;

	p_dsi->update_mode = OMAP_DSS_UPDATE_DISABLED;
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	dsi_display_uninit_dispc(dssdev);

	dsi_display_uninit_dsi(dssdev);

	enable_clocks(0);
	dsi_enable_pll_clock(lcd_ix, 0);
end:
	dsi_bus_unlock(lcd_ix);
	mutex_unlock(&(p_dsi->lock));

	return 0;
}

static int dsi_display_resume(struct omap_dss_device *dssdev)
{
	int r;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	DSSDBG("dsi_display_resume\n");

	mutex_lock(&(p_dsi->lock));
	dsi_bus_lock(lcd_ix);

	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		DSSERR("dssdev not suspended\n");
		r = -EINVAL;
		goto err0;
	}

	enable_clocks(1);
	dsi_enable_pll_clock(lcd_ix, 1);

	r = _dsi_reset(lcd_ix);
	if (r)
		goto err1;

	dsi_core_init(lcd_ix);

	r = dsi_display_init_dispc(dssdev);
	if (r)
		goto err1;

	r = dsi_display_init_dsi(dssdev);
	if (r)
		goto err2;

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	r = dsi_set_te(dssdev, p_dsi->te_enabled);
	if (r)
		goto err2;

	dsi_set_update_mode(lcd_ix, dssdev, p_dsi->user_update_mode);

	dsi_bus_unlock(lcd_ix);
	mutex_unlock(&(p_dsi->lock));

	return 0;

err2:
	dsi_display_uninit_dispc(dssdev);
err1:
	enable_clocks(0);
	dsi_enable_pll_clock(lcd_ix, 0);
err0:
	dsi_bus_unlock(lcd_ix);
	mutex_unlock(&(p_dsi->lock));
	DSSDBG("dsi_display_resume FAILED\n");
	return r;

}

static int dsi_display_update(struct omap_dss_device *dssdev,
			u16 x, u16 y, u16 w, u16 h)
{
	int r = 0;
	u16 dw, dh;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;

	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	printk(KERN_DEBUG "dsi_display_update lcd_ix=%d\n", lcd_ix);

	DSSDBG("dsi_display_update(%d,%d %dx%d)\n", x, y, w, h);

	mutex_lock(&(p_dsi->lock));

	if (p_dsi->update_mode != OMAP_DSS_UPDATE_MANUAL)
		goto end;

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		goto end;

	dssdev->get_resolution(dssdev, &dw, &dh);

	if  (x > dw || y > dh)
		goto end;

	if (x + w > dw)
		w = dw - x;

	if (y + h > dh)
		h = dh - y;

	if (w == 0 || h == 0)
		goto end;

	if (w == 1) {
		r = -EINVAL;
		goto end;
	}

	dsi_set_update_region(lcd_ix, dssdev, x, y, w, h);

	wake_up(&(p_dsi->waitqueue));

end:
	mutex_unlock(&(p_dsi->lock));

	return r;
}

static int dsi_display_sync(struct omap_dss_device *dssdev)
{
	bool wait;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	DSSDBG("dsi_display_sync()\n");

	mutex_lock(&(p_dsi->lock));
	dsi_bus_lock(lcd_ix);

	if (p_dsi->update_mode == OMAP_DSS_UPDATE_MANUAL &&
			p_dsi->update_region.dirty) {
		INIT_COMPLETION(p_dsi->update_completion);
		wait = true;
	} else {
		wait = false;
	}

	dsi_bus_unlock(lcd_ix);
	mutex_unlock(&(p_dsi->lock));

	if (wait)
		wait_for_completion_interruptible(&(p_dsi->update_completion));

	DSSDBG("dsi_display_sync() done\n");
	return 0;
}

static int dsi_display_set_update_mode(struct omap_dss_device *dssdev,
		enum omap_dss_update_mode mode)
{
	int r = 0;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	DSSDBGF("%d", mode);

	mutex_lock(&(p_dsi->lock));
	dsi_bus_lock(lcd_ix);

	if (p_dsi->update_mode != mode) {
		p_dsi->user_update_mode = mode;
		p_dsi->update_mode = mode;
		r = dsi_set_update_mode(lcd_ix, dssdev, mode);
	}
	dsi_bus_unlock(lcd_ix);
	mutex_unlock(&(p_dsi->lock));

	return r;
}

static enum omap_dss_update_mode dsi_display_get_update_mode(
		struct omap_dss_device *dssdev)
{
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	return p_dsi->update_mode;
}


static int dsi_display_enable_te(struct omap_dss_device *dssdev, bool enable)
{
	int r = 0;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	DSSDBGF("%d", enable);

	if (!dssdev->driver->enable_te)
		return -ENOENT;

	dsi_bus_lock(lcd_ix);

	p_dsi->te_enabled = enable;

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		goto end;

	r = dsi_set_te(dssdev, enable);
end:
	dsi_bus_unlock(lcd_ix);

	return r;
}

static int dsi_display_get_te(struct omap_dss_device *dssdev)
{
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	return p_dsi->te_enabled;
}

static int dsi_display_set_rotate(struct omap_dss_device *dssdev, u8 rotate)
{
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	DSSDBGF("%d", rotate);

	if (!dssdev->driver->set_rotate || !dssdev->driver->get_rotate)
		return -EINVAL;

	dsi_bus_lock(lcd_ix);
	dssdev->driver->set_rotate(dssdev, rotate);
	if (p_dsi->update_mode == OMAP_DSS_UPDATE_AUTO) {
		u16 w, h;
		/* the display dimensions may have changed, so set a new
		 * update region */
		dssdev->get_resolution(dssdev, &w, &h);
		dsi_set_update_region(lcd_ix, dssdev, 0, 0, w, h);
	}
	dsi_bus_unlock(lcd_ix);

	return 0;
}

static u8 dsi_display_get_rotate(struct omap_dss_device *dssdev)
{
	if (!dssdev->driver->set_rotate || !dssdev->driver->get_rotate)
		return 0;

	return dssdev->driver->get_rotate(dssdev);
}

static int dsi_display_set_mirror(struct omap_dss_device *dssdev, bool mirror)
{
	enum dsi lcd_ix;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	DSSDBGF("%d", mirror);

	if (!dssdev->driver->set_mirror || !dssdev->driver->get_mirror)
		return -EINVAL;

	dsi_bus_lock(lcd_ix);
	dssdev->driver->set_mirror(dssdev, mirror);
	dsi_bus_unlock(lcd_ix);

	return 0;
}

static bool dsi_display_get_mirror(struct omap_dss_device *dssdev)
{
	if (!dssdev->driver->set_mirror || !dssdev->driver->get_mirror)
		return 0;

	return dssdev->driver->get_mirror(dssdev);
}

static int dsi_display_run_test(struct omap_dss_device *dssdev, int test_num)
{
	int r = 0;
	enum dsi lcd_ix;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return -EIO;

	DSSDBGF("%d", test_num);

	dsi_bus_lock(lcd_ix);

	/* run test first in low speed mode */
	dsi_vc_enable_hs(lcd_ix, 0, 0);

	if (dssdev->driver->run_test) {
		r = dssdev->driver->run_test(dssdev, test_num);
		if (r)
			goto end;
	}

	/* then in high speed */
	dsi_vc_enable_hs(lcd_ix, 0, 1);

	if (dssdev->driver->run_test) {
		r = dssdev->driver->run_test(dssdev, test_num);
		if (r)
			goto end;
	}

end:
	dsi_vc_enable_hs(lcd_ix, 0, 1);

	dsi_bus_unlock(lcd_ix);

	return r;
}

static int dsi_display_memory_read(struct omap_dss_device *dssdev,
		void *buf, size_t size,
		u16 x, u16 y, u16 w, u16 h)
{
	int r;
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;
	DSSDBGF("");

	if (!dssdev->driver->memory_read)
		return -EINVAL;

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return -EIO;

	dsi_bus_lock(lcd_ix);

	r = dssdev->driver->memory_read(dssdev, buf, size,
			x, y, w, h);

	/* Memory read usually changes the update area. This will
	 * force the next update to re-set the update area */
	p_dsi->active_update_region.dirty = true;

	dsi_bus_unlock(lcd_ix);

	return r;
}

void dsi_get_overlay_fifo_thresholds(enum omap_plane plane,
		u32 fifo_size, enum omap_burst_size *burst_size,
		u32 *fifo_low, u32 *fifo_high)
{
	unsigned burst_size_bytes;
	if (!cpu_is_omap44xx()) {
		*burst_size = OMAP_DSS_BURST_16x32;
		burst_size_bytes = 16 * 32 / 8;
		*fifo_high = fifo_size - burst_size_bytes;
		*fifo_low = fifo_size - burst_size_bytes * 8;
	} else {
		*burst_size = OMAP_DSS_BURST_4x32; /* OMAP4: same as 2x128*/
		burst_size_bytes = 2 * 128 / 8;
		*fifo_high = 1020; /* check SV comment*/
		*fifo_low = 956;
			}
}

int dsi_init_display(struct omap_dss_device *dssdev)
{
	enum dsi lcd_ix;
	struct dsi_struct *p_dsi;
	lcd_ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? dsi1 : dsi2;
	p_dsi = (lcd_ix == dsi1) ? &dsi_1 : &dsi_2;

	DSSDBG("DSI init\n");

	dssdev->enable = dsi_display_enable;
	dssdev->disable = dsi_display_disable;
	dssdev->suspend = dsi_display_suspend;
	dssdev->resume = dsi_display_resume;
	dssdev->update = dsi_display_update;
	dssdev->sync = dsi_display_sync;
	dssdev->set_update_mode = dsi_display_set_update_mode;
	dssdev->get_update_mode = dsi_display_get_update_mode;
	dssdev->enable_te = dsi_display_enable_te;
	dssdev->get_te = dsi_display_get_te;

	dssdev->get_rotate = dsi_display_get_rotate;
	dssdev->set_rotate = dsi_display_set_rotate;

	dssdev->get_mirror = dsi_display_get_mirror;
	dssdev->set_mirror = dsi_display_set_mirror;

	dssdev->run_test = dsi_display_run_test;
	dssdev->memory_read = dsi_display_memory_read;

	/* XXX these should be figured out dynamically */
	dssdev->caps = OMAP_DSS_DISPLAY_CAP_MANUAL_UPDATE |
		OMAP_DSS_DISPLAY_CAP_TEAR_ELIM;

	p_dsi->vc[0].dssdev = dssdev;
	p_dsi->vc[1].dssdev = dssdev;

	return 0;
}

int dsi_init(struct platform_device *pdev)
{
	int ret;
	u32 rev;
	int r;
	enum dsi lcd_ix = dsi1;

	struct sched_param param = {
		.sched_priority = MAX_USER_RT_PRIO-1
	};

	spin_lock_init(&dsi_1.errors_lock);
	dsi_1.errors = 0;

	/* XXX fail properly */

	init_completion(&dsi_1.bta_completion);
	init_completion(&dsi_1.update_completion);

	dsi_1.thread = kthread_create(dsi_update_thread, NULL, "dsi");
	if (IS_ERR(dsi_1.thread)) {
		DSSERR("cannot create kthread\n");
		return PTR_ERR(dsi_1.thread);
		goto err0;
	}
	sched_setscheduler(dsi_1.thread, SCHED_FIFO, &param);

	init_waitqueue_head(&dsi_1.waitqueue);
	spin_lock_init(&dsi_1.update_lock);

	mutex_init(&dsi_1.lock);
	mutex_init(&dsi_1.bus_lock);

#ifdef DSI_CATCH_MISSING_TE
	init_timer(&dsi_1.te_timer);
	dsi_1.te_timer.function = dsi_te_timeout;
	dsi_1.te_timer.data = 0;
#endif

	dsi_1.update_mode = OMAP_DSS_UPDATE_DISABLED;
	dsi_1.user_update_mode = OMAP_DSS_UPDATE_DISABLED;

	dsi_base = dsi_1.base = ioremap(DSI_BASE, 2000);

	if (!dsi_1.base) {
		DSSERR("can't ioremap DSI\n");
		r = -ENOMEM;
		goto err1;
	}

if (!cpu_is_omap44xx()) {

	dsi_1.vdds_dsi_reg = regulator_get(&pdev->dev, "vdds_dsi");
	if (IS_ERR(dsi_1.vdds_dsi_reg)) {
		DSSERR("can't get VDDS_DSI regulator\n");
		r = PTR_ERR(dsi_1.vdds_dsi_reg);
		goto err2;
	}
		} else {
	ret = twl_i2c_write_u8(TWL_MODULE_PWM, 0xFF, PWM2ON);
	ret = twl_i2c_write_u8(TWL_MODULE_PWM, 0x7F, PWM2OFF);
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x30, TOGGLE3);
		}

	enable_clocks(1);

	rev = dsi_read_reg(lcd_ix, DSI_REVISION);
	printk(KERN_INFO "OMAP DSI rev %d.%d\n",
	       FLD_GET(rev, 7, 4), FLD_GET(rev, 3, 0));

	enable_clocks(0);

	wake_up_process(dsi_1.thread);

	return 0;
err2:
	iounmap(dsi_1.base);
err1:
	kthread_stop(dsi_1.thread);
err0:
	return r;
}

void dsi_exit(void)
{
	kthread_stop(dsi_1.thread);

#ifndef CONFIG_ARCH_OMAP4
	regulator_put(dsi_1.vdds_dsi_reg);
#endif

	iounmap(dsi_1.base);

	DSSDBG("omap_dsi_exit\n");
}

int dsi2_init(struct platform_device *pdev)
{
	u32 rev;
	int r;
	enum dsi lcd_ix = dsi2;

	struct sched_param param = {
		.sched_priority = MAX_USER_RT_PRIO-1
	};

	spin_lock_init(&dsi_2.errors_lock);
	dsi_2.errors = 0;

	init_completion(&dsi_2.bta_completion);
	init_completion(&dsi_2.update_completion);

	dsi_2.thread = kthread_create(dsi2_update_thread, NULL, "dsi2");
	if (IS_ERR(dsi_2.thread)) {
		DSSERR("cannot create kthread\n");
		return PTR_ERR(dsi_2.thread);
		goto err0;
	}
	sched_setscheduler(dsi_2.thread, SCHED_FIFO, &param);

	init_waitqueue_head(&dsi_2.waitqueue);
	spin_lock_init(&dsi_2.update_lock);

	mutex_init(&dsi_2.lock);
	mutex_init(&dsi_2.bus_lock);

#ifdef DSI_CATCH_MISSING_TE
	init_timer(&dsi_2.te_timer);
	dsi_2.te_timer.function = dsi_te_timeout;
	dsi_2.te_timer.data = 0;
#endif

	dsi_2.update_mode = OMAP_DSS_UPDATE_DISABLED;
	dsi_2.user_update_mode = OMAP_DSS_UPDATE_DISABLED;

#ifdef CONFIG_ARCH_OMAP4
	dsi2_base = dsi_2.base = ioremap(DSI2_BASE, 2000);
	if (!dsi_2.base) {
		DSSERR("can't ioremap DSI\n");
		r = -ENOMEM;
		goto err1;
	}
#endif

#ifdef CONFIG_ARCH_OMAP4
#if 0
	dsi_2.vdds_dsi_reg = regulator_get(&pdev->dev, "vdds_dsi");
	if (IS_ERR(dsi_2.vdds_dsi_reg)) {
		DSSERR("can't get VDDS_DSI regulator\n");
		r = PTR_ERR(dsi_2.vdds_dsi_reg);
		goto err2;
	}
#endif
#endif

	enable_clocks(1);

	rev = dsi_read_reg(lcd_ix, DSI_REVISION);
	printk(KERN_INFO "OMAP DSI2 rev %d.%d\n",
	       FLD_GET(rev, 7, 4), FLD_GET(rev, 3, 0));

	enable_clocks(0);

	wake_up_process(dsi_2.thread);

	return 0;
#ifdef CONFIG_ARCH_OMAP4
err2:
	iounmap(dsi_2.base);
err1:
	kthread_stop(dsi_2.thread);
#endif
err0:
	return r;
}

void dsi2_exit(void)
{
	kthread_stop(dsi_2.thread);

	/*regulator_put(dsi.vdds_dsi_reg);*/

	iounmap(dsi_2.base);

	DSSDBG("omap_dsi2_exit\n");
}
