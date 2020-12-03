/*
 * linux/drivers/video/omap2/dss/dispc.c
 *
 * Copyright (C) 2009 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * Some code and ideas taken from drivers/video/omap/ driver
 * by Imre Deak.
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

#define DSS_SUBSYS_NAME "DISPC"

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include <plat/sram.h>
#include <plat/clock.h>

#include <plat/display.h>

#include "dss.h"
#include "gammatable.h"
#include <mach/tiler.h>
//#include"../../../media/video/tiler/tiler.h"

#ifndef CONFIG_ARCH_OMAP4
       	/* DSS */
       	#define DSS_BASE                0x48050000
       	/* DISPLAY CONTROLLER */
       	#define DISPC_BASE              0x48050400
		#define DISPC_SZ_REGS			SZ_1K
#else
       	/* DSS */
	#define DSS_BASE                0x58000000
       	/* DISPLAY CONTROLLER */
	#define DISPC_BASE              0x58001000
	#define DISPC_SZ_REGS		0x814
#endif


#define MAX_FLICKER_FILTER	2

#ifdef LCD_REFRESH_CHANGE
void reduce_refresh_rate(int enable);
#endif



struct dispc_reg { u16 idx; };

#define DISPC_REG(idx)			((const struct dispc_reg) { idx })

/* DISPC common */
#define DISPC_REVISION			DISPC_REG(0x0000)
#define DISPC_SYSCONFIG			DISPC_REG(0x0010)
#define DISPC_SYSSTATUS			DISPC_REG(0x0014)
#define DISPC_IRQSTATUS			DISPC_REG(0x0018)
#define DISPC_IRQENABLE			DISPC_REG(0x001C)
#define DISPC_CONTROL			DISPC_REG(0x0040)
	/* DISPC_CONTROL1 in OMAP4 */
#define DISPC_CONFIG			DISPC_REG(0x0044)
	/* DISPC_CONFIG1 in OMAP4 */
#define DISPC_CAPABLE			DISPC_REG(0x0048)
#define DISPC_DEFAULT_COLOR0		DISPC_REG(0x004C)
#define DISPC_DEFAULT_COLOR1		DISPC_REG(0x0050)
#define DISPC_TRANS_COLOR0		DISPC_REG(0x0054)
#define DISPC_TRANS_COLOR1		DISPC_REG(0x0058)
#define DISPC_LINE_STATUS		DISPC_REG(0x005C)
#define DISPC_LINE_NUMBER		DISPC_REG(0x0060)
#define DISPC_TIMING_H			DISPC_REG(0x0064)
	/* DISPC_TIMING_H1 in OMAP4 */
#define DISPC_TIMING_V			DISPC_REG(0x0068)
	/* DISPC_TIMING_V1 in OMAP4 */
#define DISPC_POL_FREQ			DISPC_REG(0x006C)
	/* DISPC_POL_FREQ1 in OMAP4 */

#ifndef CONFIG_ARCH_OMAP4
#define DISPC_DIVISOR			DISPC_REG(0x0070)
#else
#define DISPC_DIVISOR			DISPC_REG(0x0804)
#define DISPC_DIVISOR1			DISPC_REG(0x0070)
#endif

#define DISPC_GLOBAL_ALPHA		DISPC_REG(0x0074)
#define DISPC_SIZE_DIG			DISPC_REG(0x0078)
	/* DISPC_SIZE_TV in OMAP4 */
#define DISPC_SIZE_LCD			DISPC_REG(0x007C)
	/* DISPC_SIZE_LCD1 in OMAP4 */

#ifdef CONFIG_ARCH_OMAP4
#define DISPC_GLOBAL_BUFFER		DISPC_REG(0x0800)
#endif

/* DISPC GFX plane */
#define DISPC_GFX_BA0			DISPC_REG(0x0080)
#define DISPC_GFX_BA1			DISPC_REG(0x0084)
#define DISPC_GFX_POSITION		DISPC_REG(0x0088)
#define DISPC_GFX_SIZE			DISPC_REG(0x008C)
#define DISPC_GFX_ATTRIBUTES		DISPC_REG(0x00A0)
#define DISPC_GFX_FIFO_THRESHOLD	DISPC_REG(0x00A4)
	/* DISPC_GFX_BUF_THRESHOLD in OMAP4 */
#define DISPC_GFX_FIFO_SIZE_STATUS	DISPC_REG(0x00A8)
	/* DISPC_GFX_BUF_SIZE_STATUS in OMAP4 */
#define DISPC_GFX_ROW_INC		DISPC_REG(0x00AC)
#define DISPC_GFX_PIXEL_INC		DISPC_REG(0x00B0)
#define DISPC_GFX_WINDOW_SKIP	DISPC_REG(0x00B4)
#define DISPC_GFX_TABLE_BA		DISPC_REG(0x00B8)

#define DISPC_DATA_CYCLE1		DISPC_REG(0x01D4)
	/* DISPC_DATA1_CYCLE1 in OMAP4 */
#define DISPC_DATA_CYCLE2		DISPC_REG(0x01D8)
	/* DISPC_DATA1_CYCLE2 in OMAP4 */
#define DISPC_DATA_CYCLE3		DISPC_REG(0x01DC)
	/* DISPC_DATA1_CYCLE3 in OMAP4 */

#define DISPC_CPR_COEF_R		DISPC_REG(0x0220)
	/* DISPC_CPR1_COEFF_R in OMAP4 */
#define DISPC_CPR_COEF_G		DISPC_REG(0x0224)
	/* DISPC_CPR1_COEFF_G in OMAP4 */
#define DISPC_CPR_COEF_B		DISPC_REG(0x0228)
	/* DISPC_CPR1_COEFF_B in OMAP4 */

#define DISPC_GFX_PRELOAD		DISPC_REG(0x022C)

/* DISPC Video plane, n = 0 for VID1 and n = 1 for VID2 */
#define DISPC_VID_REG(n, idx)		DISPC_REG(0x00BC + (n)*0x90 + idx)

#define DISPC_VID_BA0(n)		DISPC_VID_REG(n, 0x0000)
#define DISPC_VID_BA1(n)		DISPC_VID_REG(n, 0x0004)

#define DISPC_VID_POSITION(n)		DISPC_VID_REG(n, 0x0008)
#define DISPC_VID_SIZE(n)		DISPC_VID_REG(n, 0x000C)
#define DISPC_VID_ATTRIBUTES(n)		DISPC_VID_REG(n, 0x0010)
#define DISPC_VID_FIFO_THRESHOLD(n)	DISPC_VID_REG(n, 0x0014)
#define DISPC_VID_FIFO_SIZE_STATUS(n)	DISPC_VID_REG(n, 0x0018)
#define DISPC_VID_ROW_INC(n)		DISPC_VID_REG(n, 0x001C)
#define DISPC_VID_PIXEL_INC(n)		DISPC_VID_REG(n, 0x0020)
#define DISPC_VID_FIR(n)		DISPC_VID_REG(n, 0x0024)
#define DISPC_VID_PICTURE_SIZE(n)	DISPC_VID_REG(n, 0x0028)
#define DISPC_VID_ACCU0(n)		DISPC_VID_REG(n, 0x002C)
#define DISPC_VID_ACCU1(n)		DISPC_VID_REG(n, 0x0030)

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_FIR_COEF_H(n, i)	DISPC_REG(0x00F0 + (n)*0x90 + (i)*0x8)
/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_FIR_COEF_HV(n, i)	DISPC_REG(0x00F4 + (n)*0x90 + (i)*0x8)
/* coef index i = {0, 1, 2, 3, 4} */
#define DISPC_VID_CONV_COEF(n, i)	DISPC_REG(0x0130 + (n)*0x90 + (i)*0x4)
/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_FIR_COEF_V(n, i)	DISPC_REG(0x01E0 + (n)*0x20 + (i)*0x4)

#define DISPC_VID_PRELOAD(n)		DISPC_REG(0x230 + (n)*0x04)

#define DISPC_CONTROL2			DISPC_REG(0x0238)
#ifdef CONFIG_ARCH_OMAP4


/******** registers related to VID3 and WB pipelines ****/
/* DISPC Video plane, n = 0 for VID3, n = 1 for WB _VID_V3_WB_ */
#define DISPC_VID_V3_WB_REG(n, idx) DISPC_REG(0x0300 + (n)*0x200 + idx)

#define DISPC_VID_V3_WB_ACCU0(n)		DISPC_VID_V3_WB_REG(n, 0x0000)
#define DISPC_VID_V3_WB_ACCU1(n)		DISPC_VID_V3_WB_REG(n, 0x0004)

#define DISPC_VID_V3_WB_BA0(n)		DISPC_VID_V3_WB_REG(n, 0x0008)
#define DISPC_VID_V3_WB_BA1(n)		DISPC_VID_V3_WB_REG(n, 0x000C)

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_V3_WB_FIR_COEF_H(n, i) DISPC_REG(0x0310+(n)*0x200+(i)*0x8)

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_V3_WB_FIR_COEF_HV(n, i) DISPC_REG(0x0314+(n)*0x200+(i)*0x8)

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_V3_WB_FIR_COEF_V(n, i) DISPC_REG(0x0350+(n)*0x200+(i)*0x4)

#define DISPC_VID_V3_WB_ATTRIBUTES(n)		DISPC_VID_V3_WB_REG(n, 0x0070)

/* coef index i = {0, 1, 2, 3, 4} */
#define DISPC_VID_V3_WB_CONV_COEF(n, i)	DISPC_REG(0x0374 + (n)*0x200 + (i)*0x4)

#define DISPC_VID_V3_WB_BUF_SIZE_STATUS(n)	DISPC_VID_V3_WB_REG(n, 0x0088)
#define DISPC_VID_V3_WB_BUF_THRESHOLD(n)	DISPC_VID_V3_WB_REG(n, 0x008C)
#define DISPC_VID_V3_WB_FIR(n)		DISPC_VID_V3_WB_REG(n, 0x0090)
#define DISPC_VID_V3_WB_PICTURE_SIZE(n)	DISPC_VID_V3_WB_REG(n, 0x0094)
#define DISPC_VID_V3_WB_PIXEL_INC(n)		DISPC_VID_V3_WB_REG(n, 0x0098)

#define DISPC_VID_VID3_POSITION		DISPC_REG(0x039C)
#define DISPC_VID_VID3_PRELOAD		DISPC_REG(0x03A0)

#define DISPC_VID_V3_WB_ROW_INC(n)		DISPC_VID_V3_WB_REG(n, 0x00A4)
#define DISPC_VID_V3_WB_SIZE(n)		DISPC_VID_V3_WB_REG(n, 0x00A8)

#define DISPC_VID_V3_WB_FIR2(n)		DISPC_REG(0x0724 + (n)*0x6C)
					/* n=0: VID3, n=1: WB*/

#define DISPC_VID_V3_WB_ACCU2_0(n)	DISPC_REG(0x0728 + (n)*0x6C)
					/* n=0: VID3, n=1: WB*/
#define DISPC_VID_V3_WB_ACCU2_1(n)	DISPC_REG(0x072C + (n)*0x6C)
					/* n=0: VID3, n=1: WB*/

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7}  n=0: VID3, n=1: WB */
#define DISPC_VID_V3_WB_FIR_COEF_H2(n, i) DISPC_REG(0x0730+(n)*0x6C+(i)*0x8)

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_V3_WB_FIR_COEF_HV2(n, i) DISPC_REG(0x0734+(n)*0x6C+(i)*0x8)

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_V3_WB_FIR_COEF_V2(n, i) DISPC_REG(0x0770+(n)*0x6C+(i)*0x4)


/*********End Vid3 and WB Registers ***************/

/********** OMAP4 new global registers  **/
#define DISPC_DEFAULT_COLOR2		DISPC_REG(0x03AC)
#define DISPC_TRANS_COLOR2			DISPC_REG(0x03B0)
#define DISPC_CPR2_COEF_B			DISPC_REG(0x03B4)
#define DISPC_CPR2_COEF_G			DISPC_REG(0x03B8)
#define DISPC_CPR2_COEF_R			DISPC_REG(0x03BC)
#define DISPC_DATA2_CYCLE1			DISPC_REG(0x03C0)
#define DISPC_DATA2_CYCLE2			DISPC_REG(0x03C4)
#define DISPC_DATA2_CYCLE3			DISPC_REG(0x03C8)
#define DISPC_SIZE_LCD2				DISPC_REG(0x03CC)
#define DISPC_TIMING_H2				DISPC_REG(0x0400)
#define DISPC_TIMING_V2				DISPC_REG(0x0404)
#define DISPC_POL_FREQ2				DISPC_REG(0x0408)
#define DISPC_DIVISOR2				DISPC_REG(0x040C)



/* DISPC Video plane,
		n = 0 for VID1
		n = 1 for VID2
		and n = 2 for VID3,
		n = 3 for WB*/

#define DISPC_VID_OMAP4_REG(n, idx) DISPC_REG(0x0600 + (n)*0x04 + idx)

#define DISPC_VID_BA_UV0(n)		DISPC_VID_OMAP4_REG((n)*2, 0x0000)
#define DISPC_VID_BA_UV1(n)		DISPC_VID_OMAP4_REG((n)*2, 0x0004)

#define DISPC_CONFIG2			DISPC_REG(0x0620)

#define DISPC_VID_ATTRIBUTES2(n)	DISPC_VID_OMAP4_REG(n, 0x0024)
					/* n = {0,1,2,3} */
#define DISPC_GAMMA_TABLE(n)		DISPC_VID_OMAP4_REG(n, 0x0030)
					/* n = {0,1,2,3} */

/* VID1/VID2 specific new registers */
#define DISPC_VID_FIR2(n)		DISPC_REG(0x063C + (n)*0x6C)
					/* n=0: VID1, n=1: VID2*/

#define DISPC_VID_ACCU2_0(n)		DISPC_REG(0x0640 + (n)*0x6C)
					/* n=0: VID1, n=1: VID2*/
#define DISPC_VID_ACCU2_1(n)		DISPC_REG(0x0644 + (n)*0x6C)
					/* n=0: VID1, n=1: VID2*/

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7}  n=0: VID1, n=1: VID2 */
#define DISPC_VID_FIR_COEF_H2(n, i)	DISPC_REG(0x0648 + (n)*0x6C + (i)*0x8)

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_FIR_COEF_HV2(n, i)	DISPC_REG(0x064C + (n)*0x6C + (i)*0x8)

/* coef index i = {0, 1, 2, 3, 4, 5, 6, 7} */
#define DISPC_VID_FIR_COEF_V2(n, i)	DISPC_REG(0x0688 + (n)*0x6C + (i)*0x4)

/*end of VID1/VID2 specific new registers*/


#endif

/*#Enable sync after LCD comes up! SPC_IRQ_SYNC_LOST | \
                                        DISPC_IRQ_VID1_FIFO_UNDERFLOW | \
                                      DISPC_IRQ_VID2_FIFO_UNDERFLOW | \

*/
#if 1 /* klaatu - workaround of SYNC_LOST_DIGIT, disabling TV problem */
#define DISPC_IRQ_MASK_ERROR            (DISPC_IRQ_GFX_FIFO_UNDERFLOW | \
                                         DISPC_IRQ_OCP_ERR | \
                                         DISPC_IRQ_VID1_FIFO_UNDERFLOW | \
                                         DISPC_IRQ_VID2_FIFO_UNDERFLOW | \
                                         DISPC_IRQ_SYNC_LOST )
#else
#define DISPC_IRQ_MASK_ERROR            (DISPC_IRQ_GFX_FIFO_UNDERFLOW | \
                                         DISPC_IRQ_OCP_ERR | \
                                         DISPC_IRQ_VID1_FIFO_UNDERFLOW | \
                                         DISPC_IRQ_VID2_FIFO_UNDERFLOW | \
                                         DISPC_IRQ_SYNC_LOST | \
                                         DISPC_IRQ_SYNC_LOST_DIGIT)
#endif

#define DISPC_MAX_NR_ISRS		8

struct omap_dispc_isr_data {
	omap_dispc_isr_t	isr;
	void			*arg;
	u32			mask;
};

#define REG_GET(idx, start, end) \
	FLD_GET(dispc_read_reg(idx), start, end)

#define REG_FLD_MOD(idx, val, start, end)				\
	dispc_write_reg(idx, FLD_MOD(dispc_read_reg(idx), val, start, end))

static const struct dispc_reg dispc_reg_att[] = { DISPC_GFX_ATTRIBUTES,
	DISPC_VID_ATTRIBUTES(0),
	DISPC_VID_ATTRIBUTES(1)
#ifdef CONFIG_ARCH_OMAP4
	, DISPC_VID_V3_WB_ATTRIBUTES(0)/* VID 3 pipeline */
#endif

};
unsigned int vid_in_use;
EXPORT_SYMBOL(vid_in_use);


static struct {
	void __iomem    *base;

	u32	fifo_size[3];

	spinlock_t irq_lock;
	u32 irq_error_mask;
	struct omap_dispc_isr_data registered_isr[DISPC_MAX_NR_ISRS];
	u32 error_irqs;
	struct work_struct error_work;

	u32		ctx[DISPC_SZ_REGS / sizeof(u32)];
} dispc;
extern void __iomem  *dispc_base;

static void _omap_dispc_set_irqs(void);
static int dispc_is_vdma_req(u8 rotation, enum omap_color_mode color_mode);

static inline void dispc_write_reg(const struct dispc_reg idx, u32 val)
{
	__raw_writel(val, dispc.base + idx.idx);
}


static inline u32 dispc_read_reg(const struct dispc_reg idx)
{
        return __raw_readl(dispc.base + idx.idx);
}

/*
static inline u32 dispc_read_reg(const struct dispc_reg idx)
{
	return __raw_readl(dispc.base + idx.idx);
}
*/
static inline u8 calc_tiler_orientation(u8 rotation, u8 mir)
{
	static u8 orientation;
	switch (rotation) {
	case 0:
		orientation = (mir ? 0x2 : 0x0);
		break;
	case 1:
		orientation = (mir ? 0x7 : 0x6);
		break;
	case 2:
		orientation = (mir ? 0x1 : 0x3);
		break;
	case 3:
		orientation = (mir ? 0x4 : 0x5);
		break;
	}
	return orientation;
}

#define SR(reg) \
	dispc.ctx[(DISPC_##reg).idx / sizeof(u32)] = dispc_read_reg(DISPC_##reg)
#define RR(reg) \
	dispc_write_reg(DISPC_##reg, dispc.ctx[(DISPC_##reg).idx / sizeof(u32)])

void dispc_save_context(void)
{
	if (cpu_is_omap24xx())
		return;

	SR(SYSCONFIG);
	SR(IRQENABLE);
	SR(CONTROL);
	SR(CONFIG);
	SR(DEFAULT_COLOR0);
	SR(DEFAULT_COLOR1);
	SR(TRANS_COLOR0);
	SR(TRANS_COLOR1);
	SR(LINE_NUMBER);
	SR(TIMING_H);
	SR(TIMING_V);
	SR(POL_FREQ);
	SR(DIVISOR);
	SR(GLOBAL_ALPHA);
	SR(SIZE_DIG);
	SR(SIZE_LCD);

	SR(GFX_BA0);
	SR(GFX_BA1);
	SR(GFX_POSITION);
	SR(GFX_SIZE);
	SR(GFX_ATTRIBUTES);
	SR(GFX_FIFO_THRESHOLD);
	SR(GFX_ROW_INC);
	SR(GFX_PIXEL_INC);
	SR(GFX_WINDOW_SKIP);
	SR(GFX_TABLE_BA);

	SR(DATA_CYCLE1);
	SR(DATA_CYCLE2);
	SR(DATA_CYCLE3);

	SR(CPR_COEF_R);
	SR(CPR_COEF_G);
	SR(CPR_COEF_B);

	SR(GFX_PRELOAD);

#ifdef CONFIG_ARCH_OMAP4
/* TODO: find out what needs to be saved */
#endif

	/* VID1 */
	SR(VID_BA0(0));
	SR(VID_BA1(0));
	SR(VID_POSITION(0));
	SR(VID_SIZE(0));
	SR(VID_ATTRIBUTES(0));
	SR(VID_FIFO_THRESHOLD(0));
	SR(VID_ROW_INC(0));
	SR(VID_PIXEL_INC(0));
	SR(VID_FIR(0));
	SR(VID_PICTURE_SIZE(0));
	SR(VID_ACCU0(0));
	SR(VID_ACCU1(0));

	SR(VID_FIR_COEF_H(0, 0));
	SR(VID_FIR_COEF_H(0, 1));
	SR(VID_FIR_COEF_H(0, 2));
	SR(VID_FIR_COEF_H(0, 3));
	SR(VID_FIR_COEF_H(0, 4));
	SR(VID_FIR_COEF_H(0, 5));
	SR(VID_FIR_COEF_H(0, 6));
	SR(VID_FIR_COEF_H(0, 7));

	SR(VID_FIR_COEF_HV(0, 0));
	SR(VID_FIR_COEF_HV(0, 1));
	SR(VID_FIR_COEF_HV(0, 2));
	SR(VID_FIR_COEF_HV(0, 3));
	SR(VID_FIR_COEF_HV(0, 4));
	SR(VID_FIR_COEF_HV(0, 5));
	SR(VID_FIR_COEF_HV(0, 6));
	SR(VID_FIR_COEF_HV(0, 7));

	SR(VID_CONV_COEF(0, 0));
	SR(VID_CONV_COEF(0, 1));
	SR(VID_CONV_COEF(0, 2));
	SR(VID_CONV_COEF(0, 3));
	SR(VID_CONV_COEF(0, 4));

	SR(VID_FIR_COEF_V(0, 0));
	SR(VID_FIR_COEF_V(0, 1));
	SR(VID_FIR_COEF_V(0, 2));
	SR(VID_FIR_COEF_V(0, 3));
	SR(VID_FIR_COEF_V(0, 4));
	SR(VID_FIR_COEF_V(0, 5));
	SR(VID_FIR_COEF_V(0, 6));
	SR(VID_FIR_COEF_V(0, 7));

	SR(VID_PRELOAD(0));

	/* VID2 */
	SR(VID_BA0(1));
	SR(VID_BA1(1));
	SR(VID_POSITION(1));
	SR(VID_SIZE(1));
	SR(VID_ATTRIBUTES(1));
	SR(VID_FIFO_THRESHOLD(1));
	SR(VID_ROW_INC(1));
	SR(VID_PIXEL_INC(1));
	SR(VID_FIR(1));
	SR(VID_PICTURE_SIZE(1));
	SR(VID_ACCU0(1));
	SR(VID_ACCU1(1));

	SR(VID_FIR_COEF_H(1, 0));
	SR(VID_FIR_COEF_H(1, 1));
	SR(VID_FIR_COEF_H(1, 2));
	SR(VID_FIR_COEF_H(1, 3));
	SR(VID_FIR_COEF_H(1, 4));
	SR(VID_FIR_COEF_H(1, 5));
	SR(VID_FIR_COEF_H(1, 6));
	SR(VID_FIR_COEF_H(1, 7));

	SR(VID_FIR_COEF_HV(1, 0));
	SR(VID_FIR_COEF_HV(1, 1));
	SR(VID_FIR_COEF_HV(1, 2));
	SR(VID_FIR_COEF_HV(1, 3));
	SR(VID_FIR_COEF_HV(1, 4));
	SR(VID_FIR_COEF_HV(1, 5));
	SR(VID_FIR_COEF_HV(1, 6));
	SR(VID_FIR_COEF_HV(1, 7));

	SR(VID_CONV_COEF(1, 0));
	SR(VID_CONV_COEF(1, 1));
	SR(VID_CONV_COEF(1, 2));
	SR(VID_CONV_COEF(1, 3));
	SR(VID_CONV_COEF(1, 4));

	SR(VID_FIR_COEF_V(1, 0));
	SR(VID_FIR_COEF_V(1, 1));
	SR(VID_FIR_COEF_V(1, 2));
	SR(VID_FIR_COEF_V(1, 3));
	SR(VID_FIR_COEF_V(1, 4));
	SR(VID_FIR_COEF_V(1, 5));
	SR(VID_FIR_COEF_V(1, 6));
	SR(VID_FIR_COEF_V(1, 7));

	SR(VID_PRELOAD(1));
}

void dispc_restore_context(void)
{
	RR(SYSCONFIG);
	RR(IRQENABLE);
	/*RR(CONTROL);*/
	RR(CONFIG);
	RR(DEFAULT_COLOR0);
	RR(DEFAULT_COLOR1);
	RR(TRANS_COLOR0);
	RR(TRANS_COLOR1);
	RR(LINE_NUMBER);
	RR(TIMING_H);
	RR(TIMING_V);
	RR(POL_FREQ);
	RR(DIVISOR);
	RR(GLOBAL_ALPHA);
	RR(SIZE_DIG);
	RR(SIZE_LCD);

#ifdef CONFIG_ARCH_OMAP4
	/* TODO: find out what needs to be saved/restored */
#endif

	RR(GFX_BA0);
	RR(GFX_BA1);
	RR(GFX_POSITION);
	RR(GFX_SIZE);
	RR(GFX_ATTRIBUTES);
	RR(GFX_FIFO_THRESHOLD);
	RR(GFX_ROW_INC);
	RR(GFX_PIXEL_INC);
	RR(GFX_WINDOW_SKIP);
	RR(GFX_TABLE_BA);

	RR(DATA_CYCLE1);
	RR(DATA_CYCLE2);
	RR(DATA_CYCLE3);

	RR(CPR_COEF_R);
	RR(CPR_COEF_G);
	RR(CPR_COEF_B);

	RR(GFX_PRELOAD);

	/* VID1 */
	RR(VID_BA0(0));
	RR(VID_BA1(0));
	RR(VID_POSITION(0));
	RR(VID_SIZE(0));
	RR(VID_ATTRIBUTES(0));
	RR(VID_FIFO_THRESHOLD(0));
	RR(VID_ROW_INC(0));
	RR(VID_PIXEL_INC(0));
	RR(VID_FIR(0));
	RR(VID_PICTURE_SIZE(0));
	RR(VID_ACCU0(0));
	RR(VID_ACCU1(0));

	RR(VID_FIR_COEF_H(0, 0));
	RR(VID_FIR_COEF_H(0, 1));
	RR(VID_FIR_COEF_H(0, 2));
	RR(VID_FIR_COEF_H(0, 3));
	RR(VID_FIR_COEF_H(0, 4));
	RR(VID_FIR_COEF_H(0, 5));
	RR(VID_FIR_COEF_H(0, 6));
	RR(VID_FIR_COEF_H(0, 7));

	RR(VID_FIR_COEF_HV(0, 0));
	RR(VID_FIR_COEF_HV(0, 1));
	RR(VID_FIR_COEF_HV(0, 2));
	RR(VID_FIR_COEF_HV(0, 3));
	RR(VID_FIR_COEF_HV(0, 4));
	RR(VID_FIR_COEF_HV(0, 5));
	RR(VID_FIR_COEF_HV(0, 6));
	RR(VID_FIR_COEF_HV(0, 7));

	RR(VID_CONV_COEF(0, 0));
	RR(VID_CONV_COEF(0, 1));
	RR(VID_CONV_COEF(0, 2));
	RR(VID_CONV_COEF(0, 3));
	RR(VID_CONV_COEF(0, 4));

	RR(VID_FIR_COEF_V(0, 0));
	RR(VID_FIR_COEF_V(0, 1));
	RR(VID_FIR_COEF_V(0, 2));
	RR(VID_FIR_COEF_V(0, 3));
	RR(VID_FIR_COEF_V(0, 4));
	RR(VID_FIR_COEF_V(0, 5));
	RR(VID_FIR_COEF_V(0, 6));
	RR(VID_FIR_COEF_V(0, 7));

	RR(VID_PRELOAD(0));

	/* VID2 */
	RR(VID_BA0(1));
	RR(VID_BA1(1));
	RR(VID_POSITION(1));
	RR(VID_SIZE(1));
	RR(VID_ATTRIBUTES(1));
	RR(VID_FIFO_THRESHOLD(1));
	RR(VID_ROW_INC(1));
	RR(VID_PIXEL_INC(1));
	RR(VID_FIR(1));
	RR(VID_PICTURE_SIZE(1));
	RR(VID_ACCU0(1));
	RR(VID_ACCU1(1));

	RR(VID_FIR_COEF_H(1, 0));
	RR(VID_FIR_COEF_H(1, 1));
	RR(VID_FIR_COEF_H(1, 2));
	RR(VID_FIR_COEF_H(1, 3));
	RR(VID_FIR_COEF_H(1, 4));
	RR(VID_FIR_COEF_H(1, 5));
	RR(VID_FIR_COEF_H(1, 6));
	RR(VID_FIR_COEF_H(1, 7));

	RR(VID_FIR_COEF_HV(1, 0));
	RR(VID_FIR_COEF_HV(1, 1));
	RR(VID_FIR_COEF_HV(1, 2));
	RR(VID_FIR_COEF_HV(1, 3));
	RR(VID_FIR_COEF_HV(1, 4));
	RR(VID_FIR_COEF_HV(1, 5));
	RR(VID_FIR_COEF_HV(1, 6));
	RR(VID_FIR_COEF_HV(1, 7));

	RR(VID_CONV_COEF(1, 0));
	RR(VID_CONV_COEF(1, 1));
	RR(VID_CONV_COEF(1, 2));
	RR(VID_CONV_COEF(1, 3));
	RR(VID_CONV_COEF(1, 4));

	RR(VID_FIR_COEF_V(1, 0));
	RR(VID_FIR_COEF_V(1, 1));
	RR(VID_FIR_COEF_V(1, 2));
	RR(VID_FIR_COEF_V(1, 3));
	RR(VID_FIR_COEF_V(1, 4));
	RR(VID_FIR_COEF_V(1, 5));
	RR(VID_FIR_COEF_V(1, 6));
	RR(VID_FIR_COEF_V(1, 7));

	RR(VID_PRELOAD(1));

	/* enable last, because LCD & DIGIT enable are here */
	RR(CONTROL);
}

#undef SR
#undef RR

static inline void enable_clocks(bool enable)
{
	if (enable)
		dss_clk_enable(DSS_CLK_ICK | DSS_CLK_FCK1);
	else
		dss_clk_disable(DSS_CLK_ICK | DSS_CLK_FCK1);
}

bool dispc_go_busy(enum omap_channel channel)
{
	int bit;
#ifdef CONFIG_ARCH_OMAP4
	if (channel != OMAP_DSS_CHANNEL_DIGIT)
#else
	if (channel == OMAP_DSS_CHANNEL_LCD)
#endif
		bit = 5; /* GOLCD */
	else
		bit = 6; /* GODIGIT */

#ifdef CONFIG_ARCH_OMAP4
	if (channel == OMAP_DSS_CHANNEL_LCD2)
		return REG_GET(DISPC_CONTROL2, bit, bit) == 1;
	else
#endif
		return REG_GET(DISPC_CONTROL, bit, bit) == 1;
}

static void _dispc_go(struct dispc_reg ctrl, int bit_en, int bit_go,
		      char *channel)
{
	enable_clocks(1);
	if (REG_GET(ctrl, bit_en, bit_en) == 0)
		goto end;

	if (REG_GET(ctrl, bit_go, bit_go) == 1) {
		DSSDBG("GO bit not down for %s\n", channel);
		goto end;
	}

	DSSDBG("GO %s\n", channel);

	REG_FLD_MOD(ctrl, 1, bit_go, bit_go);
end:
	enable_clocks(0);
}

void dispc_go(enum omap_channel channel)
{
	if (channel == OMAP_DSS_CHANNEL_LCD2)
		_dispc_go(DISPC_CONTROL2, 0, 5, "LCD2");
	else if (channel == OMAP_DSS_CHANNEL_LCD)
		_dispc_go(DISPC_CONTROL, 0, 5, "LCD");
	else if (channel == OMAP_DSS_CHANNEL_DIGIT)
		_dispc_go(DISPC_CONTROL, 1, 6, "DIGIT");
}

static void _dispc_write_firh_reg(enum omap_plane plane, int reg, u32 value)
{
	BUG_ON(plane == OMAP_DSS_GFX);

#ifdef CONFIG_ARCH_OMAP4
	if ((OMAP_DSS_VIDEO1 == plane) || (OMAP_DSS_VIDEO2 == plane))
#endif
		dispc_write_reg(DISPC_VID_FIR_COEF_H(plane-1, reg), value);
#ifdef CONFIG_ARCH_OMAP4
	else if (OMAP_DSS_VIDEO3 == plane)
		dispc_write_reg(DISPC_VID_V3_WB_FIR_COEF_H(0, reg), value);
#endif

}

static void _dispc_write_firhv_reg(enum omap_plane plane, int reg, u32 value)
{
	BUG_ON(plane == OMAP_DSS_GFX);

#ifdef CONFIG_ARCH_OMAP4
	if ((OMAP_DSS_VIDEO1 == plane) || (OMAP_DSS_VIDEO2 == plane))
#endif
		dispc_write_reg(DISPC_VID_FIR_COEF_HV(plane-1, reg), value);
#ifdef CONFIG_ARCH_OMAP4
	else if (OMAP_DSS_VIDEO3 == plane)
		dispc_write_reg(DISPC_VID_V3_WB_FIR_COEF_HV(0, reg), value);
#endif
}

static void _dispc_write_firv_reg(enum omap_plane plane, int reg, u32 value)
{
	BUG_ON(plane == OMAP_DSS_GFX);

#ifdef CONFIG_ARCH_OMAP4
	if ((OMAP_DSS_VIDEO1 == plane) || (OMAP_DSS_VIDEO2 == plane))
#endif

		dispc_write_reg(DISPC_VID_FIR_COEF_V(plane-1, reg), value);
#ifdef CONFIG_ARCH_OMAP4
	else if (OMAP_DSS_VIDEO3 == plane)
		dispc_write_reg(DISPC_VID_V3_WB_FIR_COEF_V(0, reg), value);
#endif
}

#ifdef CONFIG_ARCH_OMAP4
static void _dispc_write_firh2_reg(enum omap_plane plane, int reg, u32 value)
{
	BUG_ON(plane == OMAP_DSS_GFX);

	if ((OMAP_DSS_VIDEO1 == plane) || (OMAP_DSS_VIDEO2 == plane))
		dispc_write_reg(DISPC_VID_FIR_COEF_H2(plane-1, reg), value);
	else if (OMAP_DSS_VIDEO3 == plane)
		dispc_write_reg(DISPC_VID_V3_WB_FIR_COEF_H2(0, reg), value);

}

static void _dispc_write_firhv2_reg(enum omap_plane plane, int reg, u32 value)
{
	BUG_ON(plane == OMAP_DSS_GFX);

	if ((OMAP_DSS_VIDEO1 == plane) || (OMAP_DSS_VIDEO2 == plane))
		dispc_write_reg(DISPC_VID_FIR_COEF_HV2(plane-1, reg), value);
	else if (OMAP_DSS_VIDEO3 == plane)
		dispc_write_reg(DISPC_VID_V3_WB_FIR_COEF_HV2(0, reg), value);
}

static void _dispc_write_firv2_reg(enum omap_plane plane, int reg, u32 value)
{
	BUG_ON(plane == OMAP_DSS_GFX);

	if ((OMAP_DSS_VIDEO1 == plane) || (OMAP_DSS_VIDEO2 == plane))
		dispc_write_reg(DISPC_VID_FIR_COEF_V2(plane-1, reg), value);
	else if (OMAP_DSS_VIDEO3 == plane)
		dispc_write_reg(DISPC_VID_V3_WB_FIR_COEF_V2(0, reg), value);
}
#endif

static void _dispc_set_scale_coef(enum omap_plane plane, const s8 *hfir,
				  const s8 *vfir, int three_taps)
{
	int i;
	for (i = 0; i < 8; i++, hfir++, vfir++) {
		u32 h, hv, v;
		h = ((hfir[0] & 0xFF) | ((hfir[8] << 8) & 0xFF00) |
		     ((hfir[16] << 16) & 0xFF0000) |
		     ((hfir[24] << 24) & 0xFF000000));
		hv = ((hfir[32] & 0xFF) | ((vfir[8] << 8) & 0xFF00) |
		      ((vfir[16] << 16) & 0xFF0000) |
		      ((vfir[24] << 24) & 0xFF000000));
		v = ((vfir[0] & 0xFF) | ((vfir[32] << 8) & 0xFF00));

		_dispc_write_firh_reg(plane, i, h);
		_dispc_write_firhv_reg(plane, i, hv);
		_dispc_write_firv_reg(plane, i, v);
	}
}

static void _dispc_setup_color_conv_coef(void)
{
	const struct color_conv_coef {
		int  ry,  rcr,  rcb,   gy,  gcr,  gcb,   by,  bcr,  bcb;
		int  full_range;
	}  ctbl_bt601_5 = {
		298,  409,    0,  298, -208, -100,  298,    0,  517, 0,
	};

	const struct color_conv_coef *ct;

#define CVAL(x, y) (FLD_VAL(x, 26, 16) | FLD_VAL(y, 10, 0))

	ct = &ctbl_bt601_5;

	dispc_write_reg(DISPC_VID_CONV_COEF(0, 0), CVAL(ct->rcr, ct->ry));
	dispc_write_reg(DISPC_VID_CONV_COEF(0, 1), CVAL(ct->gy,	 ct->rcb));
	dispc_write_reg(DISPC_VID_CONV_COEF(0, 2), CVAL(ct->gcb, ct->gcr));
	dispc_write_reg(DISPC_VID_CONV_COEF(0, 3), CVAL(ct->bcr, ct->by));
	dispc_write_reg(DISPC_VID_CONV_COEF(0, 4), CVAL(0,       ct->bcb));

	dispc_write_reg(DISPC_VID_CONV_COEF(1, 0), CVAL(ct->rcr, ct->ry));
	dispc_write_reg(DISPC_VID_CONV_COEF(1, 1), CVAL(ct->gy,	 ct->rcb));
	dispc_write_reg(DISPC_VID_CONV_COEF(1, 2), CVAL(ct->gcb, ct->gcr));
	dispc_write_reg(DISPC_VID_CONV_COEF(1, 3), CVAL(ct->bcr, ct->by));
	dispc_write_reg(DISPC_VID_CONV_COEF(1, 4), CVAL(0,       ct->bcb));

#ifdef CONFIG_ARCH_OMAP4
	dispc_write_reg(DISPC_VID_V3_WB_CONV_COEF(0, 0),
			CVAL(ct->rcr, ct->ry));
	dispc_write_reg(DISPC_VID_V3_WB_CONV_COEF(0, 1),
			CVAL(ct->gy,  ct->rcb));
	dispc_write_reg(DISPC_VID_V3_WB_CONV_COEF(0, 2),
			CVAL(ct->gcb, ct->gcr));
	dispc_write_reg(DISPC_VID_V3_WB_CONV_COEF(0, 3),
			CVAL(ct->bcr, ct->by));
	dispc_write_reg(DISPC_VID_V3_WB_CONV_COEF(0, 4),
			CVAL(0,	ct->bcb));

	REG_FLD_MOD(DISPC_VID_V3_WB_ATTRIBUTES(0), ct->full_range, 11, 11);
#endif

#undef CVAL

	REG_FLD_MOD(DISPC_VID_ATTRIBUTES(0), ct->full_range, 11, 11);
	REG_FLD_MOD(DISPC_VID_ATTRIBUTES(1), ct->full_range, 11, 11);
}


static void _dispc_set_plane_ba0(enum omap_plane plane, u32 paddr)
{
	const struct dispc_reg ba0_reg[] = { DISPC_GFX_BA0,
		DISPC_VID_BA0(0),
		DISPC_VID_BA0(1)
#ifdef CONFIG_ARCH_OMAP4
		, DISPC_VID_V3_WB_BA0(0) /* VID 3 pipeline*/
#endif
	};

	dispc_write_reg(ba0_reg[plane], paddr);
}

static void _dispc_set_plane_ba1(enum omap_plane plane, u32 paddr)
{
	const struct dispc_reg ba1_reg[] = { DISPC_GFX_BA1,
				      DISPC_VID_BA1(0),
				      DISPC_VID_BA1(1)
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_VID_V3_WB_BA1(0) /* VID 3 pipeline*/
#endif

	};

	dispc_write_reg(ba1_reg[plane], paddr);
}

#ifdef CONFIG_ARCH_OMAP4
static void _dispc_set_plane_ba_uv0(enum omap_plane plane, u32 paddr)
{
	const struct dispc_reg ba_uv0_reg[] = { DISPC_VID_BA_UV0(0),
				DISPC_VID_BA_UV0(1),
				DISPC_VID_BA_UV0(2) /* VID 3 pipeline*/
	};

	BUG_ON(plane == OMAP_DSS_GFX);

	dispc_write_reg(ba_uv0_reg[plane - 1], paddr);
	/* plane - 1 => no UV_BA for GFX*/

}

static void _dispc_set_plane_ba_uv1(enum omap_plane plane, u32 paddr)
{
	const struct dispc_reg ba_uv1_reg[] = { DISPC_VID_BA_UV1(0),
				DISPC_VID_BA_UV1(1),
				DISPC_VID_BA_UV1(2)
	};

	BUG_ON(plane == OMAP_DSS_GFX);

	dispc_write_reg(ba_uv1_reg[plane - 1], paddr);
	/* plane - 1 => no UV_BA for GFX*/
}

#endif

static void _dispc_set_plane_pos(enum omap_plane plane, int x, int y)
{
	const struct dispc_reg pos_reg[] = { DISPC_GFX_POSITION,
				      DISPC_VID_POSITION(0),
				      DISPC_VID_POSITION(1)
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_VID_VID3_POSITION /* VID 3 pipeline*/
#endif

	};

	u32 val = FLD_VAL(y, 26, 16) | FLD_VAL(x, 10, 0);
	dispc_write_reg(pos_reg[plane], val);
}

static void _dispc_set_pic_size(enum omap_plane plane, int width, int height)
{
	const struct dispc_reg siz_reg[] = { DISPC_GFX_SIZE,
				      DISPC_VID_PICTURE_SIZE(0),
				      DISPC_VID_PICTURE_SIZE(1)
#ifdef CONFIG_ARCH_OMAP4
			, DISPC_VID_V3_WB_PICTURE_SIZE(0) /* VID 3 pipeline*/
#endif

	};
	u32 val = FLD_VAL(height - 1, 26, 16) | FLD_VAL(width - 1, 10, 0);
	dispc_write_reg(siz_reg[plane], val);
}

static void _dispc_set_vid_size(enum omap_plane plane, int width, int height)
{
	u32 val;
	const struct dispc_reg vsi_reg[] = { DISPC_VID_SIZE(0),
				      DISPC_VID_SIZE(1)
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_VID_V3_WB_SIZE(0) /* VID 3 pipeline*/
#endif

	};

	BUG_ON(plane == OMAP_DSS_GFX);

	val = FLD_VAL(height - 1, 26, 16) | FLD_VAL(width - 1, 10, 0);
	dispc_write_reg(vsi_reg[plane-1], val);
}

static void _dispc_set_alpha_blend_attrs(enum omap_plane plane, bool enable)
{
       REG_FLD_MOD(dispc_reg_att[plane], enable ? 1 : 0, 28, 28);
}

static void _dispc_setup_global_alpha(enum omap_plane plane, u8 global_alpha)
{

#ifndef CONFIG_ARCH_OMAP4      /* all pipelines have alpha coeff in OMAP4 */
	BUG_ON(plane == OMAP_DSS_VIDEO1);
#endif
	if (cpu_is_omap24xx())
		return;

	if (plane == OMAP_DSS_GFX)
		REG_FLD_MOD(DISPC_GLOBAL_ALPHA, global_alpha, 7, 0);
	else if (plane == OMAP_DSS_VIDEO2)
		REG_FLD_MOD(DISPC_GLOBAL_ALPHA, global_alpha, 23, 16);
#ifdef CONFIG_ARCH_OMAP4
	else if (plane == OMAP_DSS_VIDEO1)
		REG_FLD_MOD(DISPC_GLOBAL_ALPHA, global_alpha, 15, 8);
	else if (plane == OMAP_DSS_VIDEO3)
		REG_FLD_MOD(DISPC_GLOBAL_ALPHA, global_alpha, 31, 24);
#endif

}

static void _dispc_set_pix_inc(enum omap_plane plane, s32 inc)
{
	const struct dispc_reg ri_reg[] = { DISPC_GFX_PIXEL_INC,
				     DISPC_VID_PIXEL_INC(0),
				     DISPC_VID_PIXEL_INC(1)
#ifdef CONFIG_ARCH_OMAP4
			, DISPC_VID_V3_WB_PIXEL_INC(0) /* VID 3 pipeline*/
#endif

	};

	dispc_write_reg(ri_reg[plane], inc);
}

static void _dispc_set_row_inc(enum omap_plane plane, s32 inc)
{
	const struct dispc_reg ri_reg[] = { DISPC_GFX_ROW_INC,
				     DISPC_VID_ROW_INC(0),
				     DISPC_VID_ROW_INC(1)
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_VID_V3_WB_ROW_INC(0) /* VID 3 pipeline*/
#endif

	};

	dispc_write_reg(ri_reg[plane], inc);
}

static void _dispc_set_color_mode(enum omap_plane plane,
		enum omap_color_mode color_mode)
{
	u32 m = 0;

#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_GFX == plane) {
#endif
		switch (color_mode) {
		case OMAP_DSS_COLOR_CLUT1:
			m = 0x0; break;
		case OMAP_DSS_COLOR_CLUT2:
			m = 0x1; break;
		case OMAP_DSS_COLOR_CLUT4:
			m = 0x2; break;
		case OMAP_DSS_COLOR_CLUT8:
			m = 0x3; break;
		case OMAP_DSS_COLOR_RGB12U:
			m = 0x4; break;
		case OMAP_DSS_COLOR_ARGB16:
			m = 0x5; break;
		case OMAP_DSS_COLOR_RGB16:
			m = 0x6; break;
		case OMAP_DSS_COLOR_RGB24U:
			m = 0x8; break;
		case OMAP_DSS_COLOR_RGB24P:
			m = 0x9; break;
		case OMAP_DSS_COLOR_YUV2:
			m = 0xa; break;
		case OMAP_DSS_COLOR_UYVY:
			m = 0xb; break;
		case OMAP_DSS_COLOR_ARGB32:
			m = 0xc; break;
		case OMAP_DSS_COLOR_RGBA32:
			m = 0xd; break;
		case OMAP_DSS_COLOR_RGBX32:
			m = 0xe; break;

		default:
			BUG(); break;
		}
#ifdef CONFIG_ARCH_OMAP4
	} else {
		switch (color_mode) {
		case OMAP_DSS_COLOR_NV12:
			m = 0x0; break;
		case OMAP_DSS_COLOR_RGB12U:
			m = 0x1; break;
		case OMAP_DSS_COLOR_RGBA12:
			m = 0x2; break;
		case OMAP_DSS_COLOR_XRGB12:
			m = 0x4; break;
		case OMAP_DSS_COLOR_ARGB16:
			m = 0x5; break;
		case OMAP_DSS_COLOR_RGB16:
			m = 0x6; break;
		case OMAP_DSS_COLOR_ARGB16_1555:
			m = 0x7; break;
		case OMAP_DSS_COLOR_RGB24U:
			m = 0x8; break;
		case OMAP_DSS_COLOR_RGB24P:
			m = 0x9; break;
		case OMAP_DSS_COLOR_YUV2:
			m = 0xA; break;
		case OMAP_DSS_COLOR_UYVY:
			m = 0xB; break;
		case OMAP_DSS_COLOR_ARGB32:
			m = 0xC; break;
		case OMAP_DSS_COLOR_RGBA32:
			m = 0xD; break;
		case OMAP_DSS_COLOR_RGBX24_32_ALGN:
			m = 0xE; break;
		case OMAP_DSS_COLOR_XRGB15:
			m = 0xF; break;
		default:
			BUG(); break;
		}
	}
#endif
	REG_FLD_MOD(dispc_reg_att[plane], m, 4, 1);
}

static void _dispc_set_channel_out(enum omap_plane plane,
		enum omap_channel channel)
{
	int shift;
	u32 val;
#ifdef CONFIG_ARCH_OMAP4
	int chan = 0, chan2 = 0;
#endif

	switch (plane) {
	case OMAP_DSS_GFX:
		shift = 8;
		break;
	case OMAP_DSS_VIDEO1:
	case OMAP_DSS_VIDEO2:
#ifdef CONFIG_ARCH_OMAP4
	case OMAP_DSS_VIDEO3: /* VID 3 pipeline*/
#endif
		shift = 16;
		break;
	default:
		BUG();
		return;
	}

	val = dispc_read_reg(dispc_reg_att[plane]);
#ifdef CONFIG_ARCH_OMAP4
	switch (channel) {
	case OMAP_DSS_CHANNEL_LCD:
		chan = 0; chan2 = 0; break;
	case OMAP_DSS_CHANNEL_DIGIT:
		chan = 1; chan2 = 0; break;
	case OMAP_DSS_CHANNEL_LCD2:
		chan = 0; chan2 = 1; break;
	}
	val = FLD_MOD(val, chan, shift, shift);
	val = FLD_MOD(val, chan2, 31, 30);
#else
	val = FLD_MOD(val, channel, shift, shift);
#endif
	dispc_write_reg(dispc_reg_att[plane], val);
}

void dispc_set_burst_size(enum omap_plane plane,
		enum omap_burst_size burst_size)
{
	int shift;
	u32 val;

	enable_clocks(1);

	switch (plane) {
	case OMAP_DSS_GFX:
		shift = 6;
		break;
	case OMAP_DSS_VIDEO1:
	case OMAP_DSS_VIDEO2:
#ifdef CONFIG_ARCH_OMAP4
	case OMAP_DSS_VIDEO3: /* VID 3 pipeline*/
#endif
		shift = 14;
		break;
	default:
		BUG();
		return;
	}

	val = dispc_read_reg(dispc_reg_att[plane]);
	val = FLD_MOD(val, burst_size, shift+1, shift);
	dispc_write_reg(dispc_reg_att[plane], val);

	enable_clocks(0);
}

#ifdef CONFIG_ARCH_OMAP4
void dispc_set_zorder(enum omap_plane plane,
					enum omap_overlay_zorder zorder)
{
	u32 val;

	val = dispc_read_reg(dispc_reg_att[plane]);
	val = FLD_MOD(val, zorder, 27, 26);
	dispc_write_reg(dispc_reg_att[plane], val);

}
void dispc_enable_zorder(enum omap_plane plane, bool enable)
{
	u32 val;

	val = dispc_read_reg(dispc_reg_att[plane]);
	val = FLD_MOD(val, enable, 25, 25);
	dispc_write_reg(dispc_reg_att[plane], val);

}
/* this routine is a collection of some fine tuned settings from SiVal test, needs to be revisited on SDC */
void dispc_enable_preload(enum omap_plane plane, bool enable)
{
	u32 val;
	int x, y;

	/* enable preload */
	val = dispc_read_reg(dispc_reg_att[plane]);
	val = FLD_MOD(val, enable, 19, 19);
	dispc_write_reg(dispc_reg_att[plane], val);

	/* DMA preload values */
	dispc_write_reg(DISPC_VID_PRELOAD(0), 0x100);

	/* clk divisor for DISPC_CORE_CLK */
	x = 1; /* 1 for 1080P 2 for others */
	y = 1;
	val = FLD_VAL(x, 23, 16) | FLD_VAL(y, 0, 0);
	dispc_write_reg(DISPC_DIVISOR, val);
}
void dispc_set_idle_mode(void)
{
	u32 l;

	l = dispc_read_reg(DISPC_SYSCONFIG);
	l = FLD_MOD(l, 1, 13, 12);	/* MIDLEMODE: smart standby */
	l = FLD_MOD(l, 1, 4, 3);	/* SIDLEMODE: smart idle */
	l = FLD_MOD(l, 0, 2, 2);	/* ENWAKEUP */
	l = FLD_MOD(l, 0, 0, 0);	/* AUTOIDLE */
	dispc_write_reg(DISPC_SYSCONFIG, l);

}
void dispc_enable_gamma_table(bool enable)
{
	REG_FLD_MOD(DISPC_CONFIG, enable, 9, 9);
}


#endif

static void _dispc_set_vid_color_conv(enum omap_plane plane, bool enable)
{
	u32 val;

	BUG_ON(plane == OMAP_DSS_GFX);

	val = dispc_read_reg(dispc_reg_att[plane]);
	val = FLD_MOD(val, enable, 9, 9);
	dispc_write_reg(dispc_reg_att[plane], val);
}

void dispc_enable_replication(enum omap_plane plane, bool enable)
{
	int bit;

	if (plane == OMAP_DSS_GFX)
		bit = 5;
	else
		bit = 10;

	enable_clocks(1);
	REG_FLD_MOD(dispc_reg_att[plane], enable, bit, bit);
	enable_clocks(0);
}

void dispc_set_lcd_size(enum omap_channel channel, u16 width, u16 height)
{
	u32 val;
	BUG_ON((width > (1 << 11)) || (height > (1 << 11)));
	val = FLD_VAL(height - 1, 26, 16) | FLD_VAL(width - 1, 10, 0);
	enable_clocks(1);
#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel)
		dispc_write_reg(DISPC_SIZE_LCD2, val);
	else
#endif
		dispc_write_reg(DISPC_SIZE_LCD, val);
	enable_clocks(0);
}

void dispc_set_digit_size(u16 width, u16 height)
{
	u32 val;
	BUG_ON((width > (1 << 11)) || (height > (1 << 11)));
	val = FLD_VAL(height - 1, 26, 16) | FLD_VAL(width - 1, 10, 0);
	enable_clocks(1);
	dispc_write_reg(DISPC_SIZE_DIG, val);
	enable_clocks(0);
}


static void dispc_read_plane_fifo_sizes(void)
{
	const struct dispc_reg fsz_reg[] = { DISPC_GFX_FIFO_SIZE_STATUS,
				      DISPC_VID_FIFO_SIZE_STATUS(0),
				      DISPC_VID_FIFO_SIZE_STATUS(1)
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_VID_V3_WB_BUF_SIZE_STATUS(0)
					/* VID 3 pipeline*/
#endif

	};
	u32 size;
	int plane;

	enable_clocks(1);

	for (plane = 0; plane < ARRAY_SIZE(dispc.fifo_size); ++plane) {
		if (cpu_is_omap24xx())
			size = FLD_GET(dispc_read_reg(fsz_reg[plane]), 8, 0);
		else if (cpu_is_omap34xx())
			size = FLD_GET(dispc_read_reg(fsz_reg[plane]), 10, 0);
		else if (cpu_is_omap44xx())
			size = FLD_GET(dispc_read_reg(fsz_reg[plane]), 15, 0);
		else
			BUG();

		dispc.fifo_size[plane] = size;
	}

	enable_clocks(0);
}

u32 dispc_get_plane_fifo_size(enum omap_plane plane)
{
	return dispc.fifo_size[plane];
}

void dispc_setup_plane_fifo(enum omap_plane plane, u32 low, u32 high)
{
	const struct dispc_reg ftrs_reg[] = { DISPC_GFX_FIFO_THRESHOLD,
				       DISPC_VID_FIFO_THRESHOLD(0),
				       DISPC_VID_FIFO_THRESHOLD(1)
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_VID_V3_WB_BUF_THRESHOLD(0)
				/* VID 3 pipeline*/
#endif

	};
	enable_clocks(1);

	DSSDBG("fifo(%d) low/high old %u/%u, new %u/%u\n",
			plane,
#ifndef CONFIG_ARCH_OMAP4
			REG_GET(ftrs_reg[plane], 11, 0),
			REG_GET(ftrs_reg[plane], 27, 16),
#else
			REG_GET(ftrs_reg[plane], 15, 0),
			REG_GET(ftrs_reg[plane], 31, 16),
#endif
			low, high);

	if (cpu_is_omap24xx())
		dispc_write_reg(ftrs_reg[plane],
			FLD_VAL(high, 24, 16) | FLD_VAL(low, 8, 0));
	else if (cpu_is_omap34xx())
               	dispc_write_reg(ftrs_reg[plane],
                     	FLD_VAL(high, 27, 16) | FLD_VAL(low, 11, 0));
       	else /* cpu is omap44xx */
               	dispc_write_reg(ftrs_reg[plane],
                       	FLD_VAL(high, 31, 16) | FLD_VAL(low, 15, 0));

	enable_clocks(0);
}

void dispc_enable_fifomerge(bool enable)
{
	enable_clocks(1);

	DSSDBG("FIFO merge %s\n", enable ? "enabled" : "disabled");
	REG_FLD_MOD(DISPC_CONFIG, enable ? 1 : 0, 14, 14);

	enable_clocks(0);
}

static void _dispc_set_fir(enum omap_plane plane, int hinc, int vinc)
{
	u32 val;
	const struct dispc_reg fir_reg[] = { DISPC_VID_FIR(0),
				      DISPC_VID_FIR(1)
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_VID_V3_WB_FIR(0)
				/* VID 3 pipeline*/
#endif

	};

	BUG_ON(plane == OMAP_DSS_GFX);

	if (cpu_is_omap24xx())
		val = FLD_VAL(vinc, 27, 16) | FLD_VAL(hinc, 11, 0);
	else
		val = FLD_VAL(vinc, 28, 16) | FLD_VAL(hinc, 12, 0);
	dispc_write_reg(fir_reg[plane-1], val);
}

static void _dispc_set_vid_accu0(enum omap_plane plane, int haccu, int vaccu)
{
	u32 val;
	const struct dispc_reg ac0_reg[] = { DISPC_VID_ACCU0(0),
				      DISPC_VID_ACCU0(1)
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_VID_V3_WB_ACCU0(0)
				/* VID 3 pipeline*/
#endif

	};

	BUG_ON(plane == OMAP_DSS_GFX);
	if (cpu_is_omap44xx())
		val = FLD_VAL(vaccu, 26, 16) | FLD_VAL(haccu, 10, 0);
	else
		val = FLD_VAL(vaccu, 25, 16) | FLD_VAL(haccu, 9, 0);
	dispc_write_reg(ac0_reg[plane-1], val);
}

static void _dispc_set_vid_accu1(enum omap_plane plane, int haccu, int vaccu)
{
	u32 val;
	const struct dispc_reg ac1_reg[] = { DISPC_VID_ACCU1(0),
				      DISPC_VID_ACCU1(1)
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_VID_V3_WB_ACCU1(0) /* VID 3 pipeline*/
#endif

	};

	BUG_ON(plane == OMAP_DSS_GFX);
	if (cpu_is_omap44xx())
		val = FLD_VAL(vaccu, 26, 16) | FLD_VAL(haccu, 10, 0);
	else
		val = FLD_VAL(vaccu, 25, 16) | FLD_VAL(haccu, 9, 0);
	dispc_write_reg(ac1_reg[plane-1], val);
}

#ifdef CONFIG_ARCH_OMAP4
static void _dispc_set_fir2(enum omap_plane plane, int hinc, int vinc)
{
	u32 val;
	const struct dispc_reg fir_reg[] = { DISPC_VID_FIR2(0),
						DISPC_VID_FIR2(1),
						DISPC_VID_V3_WB_FIR2(0)
				/* VID 3 pipeline*/
	};

	BUG_ON(plane == OMAP_DSS_GFX);

	val = FLD_VAL(vinc, 28, 16) | FLD_VAL(hinc, 12, 0);

	dispc_write_reg(fir_reg[plane-1], val);
}

static void _dispc_set_vid_accu2_0(enum omap_plane plane, int haccu, int vaccu)
{
	u32 val;
	const struct dispc_reg ac0_reg[] = { DISPC_VID_ACCU2_0(0),
					DISPC_VID_ACCU2_0(1),
					DISPC_VID_V3_WB_ACCU2_0(0)
	};

	BUG_ON(plane == OMAP_DSS_GFX);

	val = FLD_VAL(vaccu, 26, 16) | FLD_VAL(haccu, 10, 0);
	dispc_write_reg(ac0_reg[plane-1], val);
}

static void _dispc_set_vid_accu2_1(enum omap_plane plane, int haccu, int vaccu)
{
	u32 val;
	const struct dispc_reg ac1_reg[] = { DISPC_VID_ACCU2_1(0),
					DISPC_VID_ACCU2_1(1),
					DISPC_VID_V3_WB_ACCU2_1(0)
	};

	BUG_ON(plane == OMAP_DSS_GFX);

	val = FLD_VAL(vaccu, 26, 16) | FLD_VAL(haccu, 10, 0);
	dispc_write_reg(ac1_reg[plane-1], val);
}

#endif

static const s8 fir5_zero[] = {
	 0,    0,    0,    0,    0,    0,    0,    0,
	 0,    0,    0,    0,    0,    0,    0,    0,
	 0,    0,    0,    0,    0,    0,    0,    0,
	 0,    0,    0,    0,    0,    0,    0,    0,
	 0,    0,    0,    0,    0,    0,    0,    0,
};
static const s8 fir3_m8[] = {
	 0,    0,    0,    0,    0,    0,    0,    0,
	 0,    2,    5,    7,    64,   32,   12,   3,
	 128,  123,  111,  89,   64,   89,   111,  123,
	 0,    3,    12,   32,   0,    7,    5,    2,
	 0,    0,    0,    0,    0,    0,    0,    0,
};
static const s8 fir5_m8[] = {
	 17,   18,   15,   9,   -18,  -6,    5,    13,
	-20,  -27,  -30,  -27,   81,   47,   17,  -4,
	 134,  127,  121,  105,  81,   105,  121,  127,
	-20,  -4,    17,   47,  -18,  -27,  -30,  -27,
	 17,   14,   5,   -6,    2,    9,    15,   19,
};
static const s8 fir5_m8b[] = {
	 0,    0,   -1,   -2,   -9,   -5,   -2,   -1,
	 0,   -8,   -11,  -11,   73,   51,   30,   13,
	 128,  124,  112,  95,   73,   95,   112,  124,
	 0,    13,   30,   51,  -9,   -11,  -11,  -8,
	 0,   -1,   -2,   -5,    0,   -2,   -1,    0,
};
static const s8 fir5_m9[] = {
	 8,    14,   17,   17,  -26,  -18,  -9,    1,
	-8,   -21,  -27,  -30,   83,   56,   30,   8,
	 128,  126,  117,  103,  83,   103,  117,  126,
	-8,    8,    30,   56,  -26,  -30,  -27,  -21,
	 8,    1,   -9,   -18,   14,   17,   17,   14,
};
static const s8 fir5_m10[] = {
	-2,    5,    11,   15,  -28,  -24,  -18,  -10,
	 2,   -12,  -22,  -27,   83,   62,   41,   20,
	 128,  125,  116,  102,  83,   102,  116,  125,
	 2,    20,   41,   62,  -28,  -27,  -22,  -12,
	-2,   -10,  -18,  -24,   18,   15,   11,   5,
};
static const s8 fir5_m11[] = {
	-12,  -4,    3,    9,   -26,  -27,  -24,  -19,
	 12,  -3,   -15,  -22,   83,   67,   49,   30,
	 128,  124,  115,  101,  83,   101,  115,  124,
	 12,   30,   49,   67,  -26,  -22,  -15,  -3,
	-12,  -19,  -24,  -27,   14,   9,    3,   -4,
};
static const s8 fir5_m12[] = {
	-19,  -12,  -6,    1,   -21,  -25,  -26,  -24,
	 21,   6,   -7,   -16,   82,   70,   55,   38,
	 124,  120,  112,  98,   82,   98,   112,  120,
	 21,   38,   55,   70,  -21,  -16,  -7,    6,
	-19,  -24,  -26,  -25,   6,    1,   -6,   -12,
};
static const s8 fir5_m13[] = {
	-22,  -18,  -12,  -6,   -17,  -22,  -25,  -25,
	 27,   13,   0,   -10,   81,   71,   58,   43,
	 118,  115,  107,  95,   81,   95,   107,  115,
	 27,   43,   58,   71,  -17,  -10,   0,    13,
	-22,  -25,  -25,  -22,   0,   -6,   -12,  -18,
};
static const s8 fir5_m14[] = {
	-23,  -20,  -16,  -11,  -11,  -18,  -22,  -24,
	 32,   18,   6,   -4,    78,   70,   59,   46,
	 110,  108,  101,  91,   78,   91,   101,  108,
	 32,   46,   59,   70,  -11,  -4,    6,    18,
	-23,  -24,  -22,  -18,  -6,   -11,  -16,  -20,
};
static const s8 fir3_m16[] = {
	 0,    0,    0,    0,    0,    0,    0,    0,
	 36,   31,   27,   23,   55,   50,   45,   40,
	 56,   57,   56,   55,   55,   55,   56,   57,
	 36,   40,   45,   50,   18,   23,   27,   31,
	 0,    0,    0,    0,    0,    0,    0,    0,
};
static const s8 fir5_m16[] = {
	-20,  -21,  -19,  -17,  -2,   -9,   -14,  -18,
	 37,   26,   15,   6,    73,   66,   58,   48,
	 94,   93,   88,   82,   73,   82,   88,   93,
	 37,   48,   58,   66,  -2,    6,    15,   26,
	-20,  -18,  -14,  -9,   -14,  -17,  -19,  -21,
};
static const s8 fir5_m19[] = {
	-12,  -14,  -16,  -16,   8,    1,   -4,   -9,
	 38,   31,   22,   15,   64,   59,   53,   47,
	 76,   72,   73,   69,   64,   69,   73,   72,
	 38,   47,   53,   59,   8,    15,   22,   31,
	-12,  -8,   -4,    1,   -16,  -16,  -16,  -13,
};
static const s8 fir5_m22[] = {
	-6,   -8,   -11,  -13,   13,   8,    3,   -2,
	 37,   32,   25,   19,   58,   53,   48,   44,
	 66,   61,   63,   61,   58,   61,   63,   61,
	 37,   44,   48,   53,   13,   19,   25,   32,
	-6,   -1,    3,    8,   -14,  -13,  -11,  -7,
};
static const s8 fir5_m26[] = {
	 1,   -2,   -5,   -8,    18,   13,   8,    4,
	 36,   31,   27,   22,   51,   48,   44,   40,
	 54,   55,   54,   53,   51,   53,   54,   55,
	 36,   40,   44,   48,   18,   22,   27,   31,
	 1,    4,    8,    13,  -10,  -8,   -5,   -2,
};
static const s8 fir5_m32[] = {
	 7,    4,    1,   -1,    21,   17,   14,   10,
	 34,   31,   27,   24,   45,   42,   39,   37,
	 46,   46,   46,   46,   45,   46,   46,   46,
	 34,   37,   39,   42,   21,   24,   28,   31,
	 7,    10,   14,   17,  -4,   -1,    1,    4,
};

static const s8 coeff_mvals[] = {
	8, 8, 8, 9, 10, 11, 12, 13, 14, 16, 19, 22, 26, 32, 32, 32
};
static const s8 *get_scaling_coef(int orig_size, int out_size,
			    int orig_ilaced, int out_ilaced,
			    int three_tap,
			    int flicker_filter)
{
	int i = 0;
	int rem_ratio = 0;
	int mval = 8; /* default */
	int default_mval = mval;

	/* ranges from 2 to 32 */
	int two_m = 16 * orig_size / out_size;


	if (orig_size > 4 * out_size)
		return fir5_zero;

	if (out_size > 8 * orig_size)
		return three_tap ? fir3_m8 : fir5_m8;

	/* interlaced output needs at least M = 16 */
	if (out_ilaced) {
		if (two_m < 32)
			two_m = 32;
	}

	if (three_tap)
		return two_m < 24 ? fir3_m8 : fir3_m16;

	if (cpu_is_omap44xx())
		return orig_size < out_size ? fir5_m8b :
			two_m < 17 ? fir5_m8 :
			two_m < 19 ? fir5_m9 :
			two_m < 21 ? fir5_m10 :
			two_m < 23 ? fir5_m11 :
			two_m < 25 ? fir5_m12 :
			two_m < 27 ? fir5_m13 :
			two_m < 30 ? fir5_m14 :
			two_m < 35 ? fir5_m16 :
			two_m < 41 ? fir5_m19 :
			two_m < 48 ? fir5_m22 :
			two_m < 58 ? fir5_m26 :
			fir5_m32;
	else {
		rem_ratio =  out_size / orig_size ;
		if (rem_ratio > 1) {
			mval = 8;
			DSSDBG("Coefficient class mval = %d \n", mval);
		} else
			mval = two_m / 2;

		/* if flicker filter is ON then calculate
		 * the corresponding vertical coeff table
		*/

		if (flicker_filter != 0) {
			default_mval = 	mval == 8  ? 8 :
					mval == 9  ? 9 :
					mval == 10 ? 10 :
					mval == 11 ? 11 :
					mval == 12 ? 12 :
					mval == 13 ? 13 :
					mval == 14 ? 14 :
					mval < 17  ? 16 :
					mval < 20  ? 19 :
					mval < 23  ? 22 :
					mval < 27  ? 26 :
					mval <= 32 ? 32 :
					8;

			for (i = MAX_FLICKER_FILTER;
			      i < (sizeof(coeff_mvals) - MAX_FLICKER_FILTER);
			      i++)
				if (coeff_mvals[i] == default_mval)
					break;

			/*get the requested frequency coeff table*/
			mval = coeff_mvals[i + flicker_filter];
		}

		DSSDBG("<%s> Applying flicker_filter [%d], mval [%d]\n",
		       __func__, flicker_filter, mval);

		return mval == 8   ? fir5_m8 :
			mval == 9  ? fir5_m9 :
			mval == 10 ? fir5_m10 :
			mval == 11 ? fir5_m11 :
			mval == 12 ? fir5_m12 :
			mval == 13 ? fir5_m13 :
			mval == 14 ? fir5_m14 :
			mval < 17  ? fir5_m16 :
			mval < 20  ? fir5_m19 :
			mval < 23  ? fir5_m22 :
			mval < 27  ? fir5_m26 :
			mval <= 32 ? fir5_m32 :
			fir5_m8;

	}
}
static void _dispc_set_vdma_attrs(enum omap_plane plane, bool enable)
{
	REG_FLD_MOD(dispc_reg_att[plane], enable ? 1 : 0, 20, 20);
}
static void _dispc_set_scaling(enum omap_plane plane,
		u16 orig_width, u16 orig_height,
		u16 out_width, u16 out_height,
		bool ilace, bool three_taps,
		bool fieldmode, int scale_x, int scale_y,
		bool vdma,
		int flicker_filter)
{
	int fir_hinc;
	int fir_vinc;
	int accu0 = 0;
	int accu1 = 0;
	u32 l;
	const s8 *hfir, *vfir;

	BUG_ON(plane == OMAP_DSS_GFX);

	if (scale_x || vdma) {
		fir_hinc = 1024 * (orig_width - 1) / (out_width - 1);
		if (fir_hinc > 4095)
			fir_hinc = 4095;
		hfir = get_scaling_coef(orig_width, out_width, 0, 0, 0,
					flicker_filter);
	} else {
		fir_hinc = 0;
		hfir = fir5_zero;
	}

	if (scale_y || vdma) {
		fir_vinc = 1024 * (orig_height - 1) / (out_height - 1);
		if (fir_vinc > 4095)
			fir_vinc = 4095;
		vfir = get_scaling_coef(orig_height, out_height, 0, 0,
					three_taps, flicker_filter);
	} else {
		fir_vinc = 0;
		vfir = fir5_zero;
	}

	_dispc_set_scale_coef(plane, hfir, vfir, three_taps);
	_dispc_set_fir(plane, fir_hinc, fir_vinc);

	l = dispc_read_reg(dispc_reg_att[plane]);
	/* setting attrib register for scaling */
#ifndef CONFIG_ARCH_OMAP4
	l &= ~((0x0f << 5) | (0x1 << 21));
	l |= out_width > orig_width ? 0 : (1 << 7);
	l |= out_height > orig_height ? 0 : (1 << 8);
#else
    l &= ~((0x03 << 5) | (0x1 << 21));
#endif
	l |= fir_hinc ? (1 << 5) : 0;
	l |= fir_vinc ? (1 << 6) : 0;

	l |= three_taps ? 0 : (1 << 21);
	l |= vdma ? (1 << 22) : 0;

	dispc_write_reg(dispc_reg_att[plane], l);

	/*
	 * field 0 = even field = bottom field
	 * field 1 = odd field = top field
	 */
	if (ilace && !fieldmode) {
		accu1 = 0;
		accu0 = (fir_vinc / 2) & 0x3ff;
		if (accu0 >= 1024/2) {
			accu1 = 1024/2;
			accu0 -= accu1;
		}
	}

	_dispc_set_vid_accu0(plane, 0, accu0);
	_dispc_set_vid_accu1(plane, 0, accu1);
}

#ifdef CONFIG_ARCH_OMAP4
static void _dispc_set_scaling_uv(enum omap_plane plane,
		u16 orig_width, u16 orig_height,
		u16 out_width, u16 out_height,
		bool ilace, bool three_taps,
		bool fieldmode, int scale_x, int scale_y)
{
	int i;
	int fir_hinc, fir_vinc;
	int accu0, accu1, accuh;
	const s8 *hfir, *vfir;

	if (scale_x) {
		fir_hinc = 1024 * (orig_width - 1) / (out_width - 1);
		if (fir_hinc > 4095)
			fir_hinc = 4095;
		hfir = get_scaling_coef(orig_width, out_width, 0, 0, 0, 0);
	} else {
		fir_hinc = 0;
		hfir = fir5_zero;
		}

	if (scale_y) {
		fir_vinc = 1024 * (orig_height - 0) / (out_height - 0);
		if (fir_vinc > 4095)
			fir_vinc = 4095;
		vfir = get_scaling_coef(orig_height, out_height, 0,
					ilace, three_taps, 0);
	} else {
		fir_vinc = 0;
		vfir = fir5_zero;
		}

	for (i = 0; i < 8; i++, hfir++, vfir++) {
		u32 h, hv, v;
		h = ((hfir[0] & 0xFF) | ((hfir[8] << 8) & 0xFF00) |
		     ((hfir[16] << 16) & 0xFF0000) |
		     ((hfir[24] << 24) & 0xFF000000));
		hv = ((hfir[32] & 0xFF) | ((vfir[8] << 8) & 0xFF00) |
		      ((vfir[16] << 16) & 0xFF0000) |
		      ((vfir[24] << 24) & 0xFF000000));
		v = ((vfir[0] & 0xFF) | ((vfir[32] << 8) & 0xFF00));

		_dispc_write_firh2_reg(plane, i, h);
		_dispc_write_firhv2_reg(plane, i, hv);
		_dispc_write_firv2_reg(plane, i, v);
	}

	/* set chroma resampling */
	REG_FLD_MOD(DISPC_VID_ATTRIBUTES2(plane - 1),
		(fir_hinc || fir_vinc) ? 1 : 0, 8, 8);

	/* set H scaling */
	REG_FLD_MOD(dispc_reg_att[plane], fir_hinc ? 1 : 0, 6, 6);

	/* set V scaling */
	REG_FLD_MOD(dispc_reg_att[plane], fir_vinc ? 1 : 0, 5, 5);

	_dispc_set_fir2(plane, fir_hinc, fir_vinc);

	if (ilace) {
		accu0 = (-3 * fir_vinc / 4) % 1024;
		accu1 = (-fir_vinc / 4) % 1024;
	} else {
		accu0 = accu1 = (-fir_vinc / 2) % 1024;
	}
	accuh = (-fir_hinc / 2) % 1024;

	_dispc_set_vid_accu2_0(plane, 0x80, 0);
	_dispc_set_vid_accu2_1(plane, 0x80, 0);
	/* _dispc_set_vid_accu2_0(plane, accuh, accu0);
	   _dispc_set_vid_accu2_1(plane, accuh, accu1); */
}
#endif

static void _dispc_set_rotation_attrs(enum omap_plane plane, u8 rotation,
		bool mirroring, enum omap_color_mode color_mode, bool vdma)
{
#ifndef CONFIG_ARCH_OMAP4
	if (color_mode == OMAP_DSS_COLOR_YUV2 ||
			color_mode == OMAP_DSS_COLOR_UYVY) {
		int vidrot = 0;

		if (mirroring) {
			switch (rotation) {
			case OMAP_DSS_ROT_0:
				vidrot = 2;
				break;
			case OMAP_DSS_ROT_90:
				vidrot = 3;
				break;
			case OMAP_DSS_ROT_180:
				vidrot = 0;
				break;
			case OMAP_DSS_ROT_270:
				vidrot = 1;
				break;
			}
		} else {
			switch (rotation) {
			case OMAP_DSS_ROT_0:
				vidrot = 0;
				break;
			case OMAP_DSS_ROT_90:
				vidrot = 3;
				break;
			case OMAP_DSS_ROT_180:
				vidrot = 2;
				break;
			case OMAP_DSS_ROT_270:
				vidrot = 1;
				break;
			}
		}

		REG_FLD_MOD(dispc_reg_att[plane], vidrot, 13, 12);

		if (!vdma &&
			(rotation == OMAP_DSS_ROT_90 ||
				rotation == OMAP_DSS_ROT_270))
			REG_FLD_MOD(dispc_reg_att[plane], 0x1, 18, 18);
		else
			REG_FLD_MOD(dispc_reg_att[plane], 0x0, 18, 18);
	} else {
		REG_FLD_MOD(dispc_reg_att[plane], 0, 13, 12);
		REG_FLD_MOD(dispc_reg_att[plane], 0, 18, 18);
	}
#else
	if (plane != OMAP_DSS_GFX) {
		if (color_mode == OMAP_DSS_COLOR_NV12) {
			/* DOUBLESTRIDE : 0 for 90-, 270-; 1 for 0- and 180- */
			if (rotation == 1 || rotation == 3)
				REG_FLD_MOD(dispc_reg_att[plane], 0x0, 22, 22);
			else
				REG_FLD_MOD(dispc_reg_att[plane], 0x1, 22, 22);
		}
	}

	/* Set the rotation value for pipeline */
/*	REG_FLD_MOD(dispc_reg_att[plane], rotation, 13, 12); */
#endif
}

static int color_mode_to_bpp(enum omap_color_mode color_mode)
{
	switch (color_mode) {
	case OMAP_DSS_COLOR_CLUT1:
		return 1;
	case OMAP_DSS_COLOR_CLUT2:
		return 2;
	case OMAP_DSS_COLOR_CLUT4:
		return 4;
	case OMAP_DSS_COLOR_CLUT8:
		return 8;
	case OMAP_DSS_COLOR_RGB12U:
	case OMAP_DSS_COLOR_RGB16:
	case OMAP_DSS_COLOR_ARGB16:
	case OMAP_DSS_COLOR_YUV2:
	case OMAP_DSS_COLOR_UYVY:
		return 16;
	case OMAP_DSS_COLOR_RGB24P:
		return 24;
	case OMAP_DSS_COLOR_RGB24U:
	case OMAP_DSS_COLOR_ARGB32:
	case OMAP_DSS_COLOR_RGBA32:
	case OMAP_DSS_COLOR_RGBX32:
		return 32;
	default:
		BUG();
	}
}

static s32 pixinc(int pixels, u8 ps)
{
	if (pixels == 1)
		return 1;
	else if (pixels > 1)
		return 1 + (pixels - 1) * ps;
	else if (pixels < 0)
		return 1 - (-pixels + 1) * ps;
	else
		BUG();
}

static void calc_tiler_row_rotation(u8 rotation,
		u16 width, u16 height,
		enum omap_color_mode color_mode,
		s32 *row_inc)
{
	u8 ps = 1;
	DSSDBG("calc_tiler_rot(%d): %dx%d\n", rotation, width, height);

	switch (color_mode) {
	case OMAP_DSS_COLOR_RGB16:
	case OMAP_DSS_COLOR_ARGB16:

	case OMAP_DSS_COLOR_YUV2:
	case OMAP_DSS_COLOR_UYVY:
		ps = 2;
		break;

	case OMAP_DSS_COLOR_RGB24P:
	case OMAP_DSS_COLOR_RGB24U:
	case OMAP_DSS_COLOR_ARGB32:
	case OMAP_DSS_COLOR_RGBA32:
	case OMAP_DSS_COLOR_RGBX32:
		ps = 4;
		break;
#ifdef CONFIG_ARCH_OMAP4
	case OMAP_DSS_COLOR_NV12:
		ps = 1;
		break;
#endif
	default:
		BUG();
		return;
	}

	switch (rotation) {
	case 0:
	case 2:
		if (1 == ps)
			*row_inc = 16384 + 1 - (width);
		else
			*row_inc = 32768 + 1 - (width * ps);
		break;

	case 1:
	case 3:
		if (4 == ps)
			*row_inc = 16384 + 1 - (width * ps);
		else
			*row_inc = 8192 + 1 - (width * ps);
		break;

	default:
		BUG();
		return;
	}

	DSSDBG(
		" colormode: %d, rotation: %d, ps: %d, width: %d,"
		" height: %d, row_inc:%d\n",
		color_mode, rotation, ps, width, height, *row_inc);

	return;
}

static void calc_vrfb_rotation_offset(u8 rotation, bool mirror,
		u16 screen_width,
		u16 width, u16 height,
		enum omap_color_mode color_mode, bool fieldmode,
		unsigned int field_offset,
		unsigned *offset0, unsigned *offset1,
		s32 *row_inc, s32 *pix_inc)
{
	u8 ps;

	/* FIXME CLUT formats */
	switch (color_mode) {
	case OMAP_DSS_COLOR_CLUT1:
	case OMAP_DSS_COLOR_CLUT2:
	case OMAP_DSS_COLOR_CLUT4:
	case OMAP_DSS_COLOR_CLUT8:
		BUG();
		return;
	case OMAP_DSS_COLOR_YUV2:
	case OMAP_DSS_COLOR_UYVY:
		ps = 4;
		break;
	default:
		ps = color_mode_to_bpp(color_mode) / 8;
		break;
	}

	DSSDBG("calc_rot(%d): scrw %d, %dx%d\n", rotation, screen_width,
			width, height);

	/*
	 * field 0 = even field = bottom field
	 * field 1 = odd field = top field
	 */
	switch (rotation + mirror * 4) {
	case OMAP_DSS_ROT_0:
	case OMAP_DSS_ROT_180:
		/*
		 * If the pixel format is YUV or UYVY divide the width
		 * of the image by 2 for 0 and 180 degree rotation.
		 */
		if (color_mode == OMAP_DSS_COLOR_YUV2 ||
			color_mode == OMAP_DSS_COLOR_UYVY)
			width = width >> 1;
	case OMAP_DSS_ROT_90:
	case OMAP_DSS_ROT_270:
		*offset1 = 0;
		if (field_offset)
			*offset0 = field_offset * screen_width * ps;
		else
			*offset0 = 0;

		*row_inc = pixinc(1 + (screen_width - width) +
				(fieldmode ? screen_width : 0),
				ps);
		*pix_inc = pixinc(1, ps);
		break;

	case OMAP_DSS_ROT_0 + 4:
	case OMAP_DSS_ROT_180 + 4:
		/* If the pixel format is YUV or UYVY divide the width
		 * of the image by 2  for 0 degree and 180 degree
		 */
		if (color_mode == OMAP_DSS_COLOR_YUV2 ||
			color_mode == OMAP_DSS_COLOR_UYVY)
			width = width >> 1;
	case OMAP_DSS_ROT_90 + 4:
	case OMAP_DSS_ROT_270 + 4:
		*offset1 = 0;
		if (field_offset)
			*offset0 = field_offset * screen_width * ps;
		else
			*offset0 = 0;
		*row_inc = pixinc(1 - (screen_width + width) -
				(fieldmode ? screen_width : 0),
				ps);
		*pix_inc = pixinc(1, ps);
		break;

	default:
		BUG();
	}
}

static void calc_dma_rotation_offset(u8 rotation, bool mirror,
		u16 screen_width,
		u16 width, u16 height,
		enum omap_color_mode color_mode, bool fieldmode,
		unsigned int field_offset,
		unsigned *offset0, unsigned *offset1,
		s32 *row_inc, s32 *pix_inc)
{
	u8 ps;
	u16 fbw, fbh;

	/* FIXME CLUT formats */
	switch (color_mode) {
	case OMAP_DSS_COLOR_CLUT1:
	case OMAP_DSS_COLOR_CLUT2:
	case OMAP_DSS_COLOR_CLUT4:
	case OMAP_DSS_COLOR_CLUT8:
		BUG();
		return;
	default:
		ps = color_mode_to_bpp(color_mode) / 8;
		break;
	}

	DSSDBG("calc_rot(%d): scrw %d, %dx%d\n", rotation, screen_width,
			width, height);

	/* width & height are overlay sizes, convert to fb sizes */

	if (rotation == OMAP_DSS_ROT_0 || rotation == OMAP_DSS_ROT_180) {
		fbw = width;
		fbh = height;
	} else {
		fbw = height;
		fbh = width;
	}

	/*
	 * field 0 = even field = bottom field
	 * field 1 = odd field = top field
	 */
	switch (rotation + mirror * 4) {
	case OMAP_DSS_ROT_0:
		*offset1 = 0;
		if (field_offset)
			*offset0 = *offset1 + field_offset * screen_width * ps;
		else
			*offset0 = *offset1;
		*row_inc = pixinc(1 + (screen_width - fbw) +
				(fieldmode ? screen_width : 0),
				ps);
		*pix_inc = pixinc(1, ps);
		break;
	case OMAP_DSS_ROT_90:
		*offset1 = screen_width * (fbh - 1) * ps;
		if (field_offset)
			*offset0 = *offset1 + field_offset * ps;
		else
			*offset0 = *offset1;
		*row_inc = pixinc(screen_width * (fbh - 1) + 1 +
				(fieldmode ? 1 : 0), ps);
		*pix_inc = pixinc(-screen_width, ps);
		break;
	case OMAP_DSS_ROT_180:
		*offset1 = (screen_width * (fbh - 1) + fbw - 1) * ps;
		if (field_offset)
			*offset0 = *offset1 - field_offset * screen_width * ps;
		else
			*offset0 = *offset1;
		*row_inc = pixinc(-1 -
				(screen_width - fbw) -
				(fieldmode ? screen_width : 0),
				ps);
		*pix_inc = pixinc(-1, ps);
		break;
	case OMAP_DSS_ROT_270:
		*offset1 = (fbw - 1) * ps;
		if (field_offset)
			*offset0 = *offset1 - field_offset * ps;
		else
			*offset0 = *offset1;
		*row_inc = pixinc(-screen_width * (fbh - 1) - 1 -
				(fieldmode ? 1 : 0), ps);
		*pix_inc = pixinc(screen_width, ps);
		break;

	/* mirroring */
	case OMAP_DSS_ROT_0 + 4:
		*offset1 = (fbw - 1) * ps;
		if (field_offset)
			*offset0 = *offset1 + field_offset * screen_width * ps;
		else
			*offset0 = *offset1;
		*row_inc = pixinc(screen_width * 2 - 1 +
				(fieldmode ? screen_width : 0),
				ps);
		*pix_inc = pixinc(-1, ps);
		break;

	case OMAP_DSS_ROT_90 + 4:
		*offset1 = 0;
		if (field_offset)
			*offset0 = *offset1 + field_offset * ps;
		else
			*offset0 = *offset1;
		*row_inc = pixinc(-screen_width * (fbh - 1) + 1 +
				(fieldmode ? 1 : 0),
				ps);
		*pix_inc = pixinc(screen_width, ps);
		break;

	case OMAP_DSS_ROT_180 + 4:
		*offset1 = screen_width * (fbh - 1) * ps;
		if (field_offset)
			*offset0 = *offset1 - field_offset * screen_width * ps;
		else
			*offset0 = *offset1;
		*row_inc = pixinc(1 - screen_width * 2 -
				(fieldmode ? screen_width : 0),
				ps);
		*pix_inc = pixinc(1, ps);
		break;

	case OMAP_DSS_ROT_270 + 4:
		*offset1 = (screen_width * (fbh - 1) + fbw - 1) * ps;
		if (field_offset)
			*offset0 = *offset1 - field_offset * ps;
		else
			*offset0 = *offset1;
		*row_inc = pixinc(screen_width * (fbh - 1) - 1 -
				(fieldmode ? 1 : 0),
				ps);
		*pix_inc = pixinc(-screen_width, ps);
		break;

	default:
		BUG();
	}
}

static unsigned long calc_fclk_five_taps(enum omap_channel channel,
		u16 width, u16 height, u16 out_width, u16 out_height,
		enum omap_color_mode color_mode)
{
	u32 fclk = 0;
	/* FIXME venc pclk? */
	u64 tmp, pclk = dispc_pclk_rate(channel);

	if (height > out_height) {
		/* FIXME get real display PPL */
		unsigned int ppl = 800;

		tmp = pclk * height * out_width;
		do_div(tmp, 2 * out_height * ppl);
		fclk = tmp;

		if (height > 2 * out_height && ppl != out_width) {
			tmp = pclk * (height - 2 * out_height) * out_width;
			do_div(tmp, 2 * out_height * (ppl - out_width));
			fclk = max(fclk, (u32) tmp);
		}
	}

	if (width > out_width) {
		tmp = pclk * width;
		do_div(tmp, out_width);
		fclk = max(fclk, (u32) tmp);

		if (color_mode == OMAP_DSS_COLOR_RGB24U)
			fclk <<= 1;
	}

	return fclk;
}

static unsigned long calc_fclk(enum omap_channel channel, u16 width,
		u16 height, u16 out_width, u16 out_height)
{
	unsigned int hf, vf;

	/*
	 * FIXME how to determine the 'A' factor
	 * for the no downscaling case ?
	 */

	if (width > 3 * out_width)
		hf = 4;
	else if (width > 2 * out_width)
		hf = 3;
	else if (width > out_width)
		hf = 2;
	else
		hf = 1;

	if (height > out_height)
		vf = 2;
	else
		vf = 1;

	/* FIXME venc pclk? */
	return dispc_pclk_rate(channel) * vf * hf;
}

static int dispc_is_vdma_req(u8 rotation, enum omap_color_mode color_mode)
{
	if (cpu_is_omap3630())
		if ((color_mode == OMAP_DSS_COLOR_YUV2) ||
			(color_mode == OMAP_DSS_COLOR_UYVY))
			if ((rotation == OMAP_DSS_ROT_90 ||
				rotation == OMAP_DSS_ROT_270))
				return true;
	return false;
}
void dispc_set_channel_out(enum omap_plane plane, enum omap_channel channel_out)
{
	enable_clocks(1);
	_dispc_set_channel_out(plane, channel_out);
	enable_clocks(0);
}



static int _dispc_setup_plane(enum omap_plane plane,
		u32 paddr, u16 screen_width,
		u16 pos_x, u16 pos_y,
		u16 width, u16 height,
		u16 out_width, u16 out_height,
		enum omap_color_mode color_mode,
		bool ilace,
		enum omap_dss_rotation_type rotation_type,
		u8 rotation, int mirror,
		u8 global_alpha, enum omap_channel channel,
		u8 pre_alpha_mult, u32 puv_addr,
		int flicker_filter)
{

#ifdef CONFIG_ARCH_OMAP4
	int maxdownscale = 4;
#else
	int maxdownscale = cpu_is_omap34xx() ? 4 : 2;
#endif
	bool three_taps = 0;
	bool fieldmode = 0;
	int cconv = 0;
	unsigned offset0, offset1;
	s32 row_inc;
	s32 pix_inc;
	u16 frame_height = height;
	unsigned int field_offset = 0;
	bool vdma;

	u8 orientation = 0;
	struct tiler_view_orient orient;
	unsigned long  mir_x = 0, mir_y = 0;
	unsigned long tiler_width = 0, tiler_height = 0;
	
	void __iomem *reg = NULL;

	if (paddr == 0)
		return -EINVAL;

	if (ilace && (height == out_height) && flicker_filter == 0)
		fieldmode = 1;

	if (ilace) {
		if (fieldmode)
			height /= 2;
		pos_y /= 2;
		out_height /= 2;

		DSSDBG("adjusting for ilace: height %d, pos_y %d, "
				"out_height %d\n",
				height, pos_y, out_height);
	}

	if (plane == OMAP_DSS_GFX) {
		if (width != out_width || height != out_height)
			return -EINVAL;

		switch (color_mode) {
		case OMAP_DSS_COLOR_CLUT8:
		case OMAP_DSS_COLOR_ARGB16:
		case OMAP_DSS_COLOR_ARGB32:
		case OMAP_DSS_COLOR_RGBA32:
		case OMAP_DSS_COLOR_RGBX32:
			if (cpu_is_omap24xx())
				return -EINVAL;
			/* fall through */
		case OMAP_DSS_COLOR_RGB12U:
		case OMAP_DSS_COLOR_RGB16:
		case OMAP_DSS_COLOR_RGB24P:
		case OMAP_DSS_COLOR_RGB24U:
			break;

		default:
			return -EINVAL;
		}
	} else {
		/* video plane */

		unsigned long fclk = 0;
// Patch for OMAPS00227718
/*		if (out_width < width / maxdownscale ||
		   out_width > width * 8)
*/
		if (out_width < width / maxdownscale)
//Patch for OMAPS00227718
			return -EINVAL;

//Patch for OMAPS00227718
/*		if (out_height < height / maxdownscale ||
		   out_height > height * 8)
*/
	 if (out_height < height / maxdownscale)
//Patch for OMAPS00227718
			return -EINVAL;

		switch (color_mode) {
		case OMAP_DSS_COLOR_RGBX32:
		case OMAP_DSS_COLOR_RGB12U:
			if (cpu_is_omap24xx())
				return -EINVAL;
			/* fall through */
		case OMAP_DSS_COLOR_RGB16:
		case OMAP_DSS_COLOR_RGB24P:
		case OMAP_DSS_COLOR_RGB24U:
			break;

		case OMAP_DSS_COLOR_ARGB16:
		case OMAP_DSS_COLOR_ARGB32:
		case OMAP_DSS_COLOR_RGBA32:
#ifdef CONFIG_ARCH_OMAP4
		case OMAP_DSS_COLOR_RGBA12:
		case OMAP_DSS_COLOR_XRGB12:
		case OMAP_DSS_COLOR_ARGB16_1555:
		case OMAP_DSS_COLOR_RGBX24_32_ALGN:
		case OMAP_DSS_COLOR_XRGB15:
#else
			if (cpu_is_omap24xx())
				return -EINVAL;
			if (plane == OMAP_DSS_VIDEO1)
				return -EINVAL;
#endif
			break;

#ifdef CONFIG_ARCH_OMAP4
		case OMAP_DSS_COLOR_NV12:
#endif
		case OMAP_DSS_COLOR_YUV2:
		case OMAP_DSS_COLOR_UYVY:
			cconv = 1;
			break;

		default:
			return -EINVAL;
		}

		vdma = dispc_is_vdma_req(rotation, color_mode);
		if (vdma)
			three_taps = false;
		/* Must use 3-tap filter */
		if (cpu_is_omap44xx())
		three_taps = width > 1280;
		else
			three_taps = width > 1024;

		/* Should use 3-tap filter for upscaling, but HDMI gets
		   out of sync if using 3-tap */
		/* if (out_height > height)
			three_taps = 1; */

		if (three_taps) {
			fclk = calc_fclk(channel, width, height,
					out_width, out_height);

			/* Try 5-tap filter if 3-tap fclk is too high*/
			if (cpu_is_omap34xx() && height > out_height &&
					fclk > dispc_fclk_rate()) {
				printk(KERN_ERR
					"Should use 5 tap but cannot\n");
			}
		} else {
			fclk = calc_fclk_five_taps(channel, width, height,
				out_width, out_height, color_mode);
		}

#ifndef CONFIG_ARCH_OMAP4
		if (width > (1024 << three_taps))
			return -EINVAL;
#endif

		DSSDBG("required fclk rate = %lu Hz\n", fclk);
		DSSDBG("current fclk rate = %lu Hz\n", dispc_fclk_rate());

		if (fclk > dispc_fclk_rate()) {
			DSSERR("failed to set up scaling, "
					"required fclk rate = %lu Hz, "
					"current fclk rate = %lu Hz\n",
					fclk, dispc_fclk_rate());
			return -EINVAL;
		}
	}

	if (ilace && !fieldmode) {
		/*
		 * when downscaling the bottom field may have to start several
		 * source lines below the top field. Unfortunately ACCUI
		 * registers will only hold the fractional part of the offset
		 * so the integer part must be added to the base address of the
		 * bottom field.
		 */
		if (!height || height == out_height)
			field_offset = 0;
		else
			field_offset = height / out_height / 2;
	}

	/* Fields are independent but interleaved in memory. */
	if (fieldmode)
		field_offset = 1;
	if (cpu_is_omap44xx()) {
	pix_inc = 0x1;
	offset0 = 0x0;
	offset1 = 0x0;
	/* check if tiler address; else set row_inc = 1*/
	if ((paddr >= 0x60000000) && (paddr <= 0x7fffffff)) {
		calc_tiler_row_rotation(rotation, width, frame_height,
						color_mode, &row_inc);
			orientation = calc_tiler_orientation(rotation,
							(u8)mirror);
		/* get rotated top-left coordinate
				(if rotation is applied before mirroring) */
		memset(&orient, 0, sizeof(orient));
		tiler_rotate_view(&orient, rotation * 90);

		if (mirror) {
			/* Horizontal mirroring */
			if (rotation == 1 || rotation == 3)
				mir_x = 1;
			else
				mir_y = 1;
		} else {
			mir_x = 0;
			mir_y = 0;
		}
		orient.x_invert ^= mir_x;
		orient.y_invert ^= mir_y;

		if (orient.rotate_90 & 1) {
			tiler_height = width;
			tiler_width = height;
		} else {
			tiler_height = height;
			tiler_width = width;
		}

			paddr = tiler_reorient_topleft(
					tiler_get_natural_addr((void *) paddr),
					orient, tiler_width, tiler_height);


			if (puv_addr)
				puv_addr = tiler_reorient_topleft(
					tiler_get_natural_addr(
					(void *) puv_addr), orient,
					tiler_width/2, tiler_height/2);

			DSSDBG("rotated addresses: 0x%0x, 0x%0x\n",
					paddr, puv_addr);

			/* set BURSTTYPE if rotation is non-zero */
			REG_FLD_MOD(dispc_reg_att[plane], 0x1, 29, 29);

		} else {
			row_inc = 0x1;
		}
	} else {
		if (rotation_type == OMAP_DSS_ROT_DMA)
			calc_dma_rotation_offset(rotation, mirror,
				screen_width, width, frame_height, color_mode,
				fieldmode, field_offset,
				&offset0, &offset1, &row_inc, &pix_inc);
		else
			calc_vrfb_rotation_offset(rotation, mirror,
				screen_width, width, frame_height, color_mode,
				fieldmode, field_offset,
				&offset0, &offset1, &row_inc, &pix_inc);
	}

	DSSDBG("offset0 %u, offset1 %u, row_inc %d, pix_inc %d\n",
			offset0, offset1, row_inc, pix_inc);

	_dispc_set_color_mode(plane, color_mode);

	_dispc_set_plane_ba0(plane, paddr + offset0);
	_dispc_set_plane_ba1(plane, paddr + offset1);
#ifdef CONFIG_ARCH_OMAP4
	/* TODO: check the offset calculations? */
	if (OMAP_DSS_COLOR_NV12 == color_mode) {
		_dispc_set_plane_ba_uv0(plane, puv_addr + offset0);
		_dispc_set_plane_ba_uv1(plane, puv_addr + offset1);
	}
#endif
	_dispc_set_row_inc(plane, row_inc);
	_dispc_set_pix_inc(plane, pix_inc);

	DSSDBG("%d,%d %dx%d -> %dx%d\n", pos_x, pos_y, width, height,
			out_width, out_height);

	_dispc_set_plane_pos(plane, pos_x, pos_y);

	_dispc_set_pic_size(plane, width, height);

	if (plane != OMAP_DSS_GFX) {
		int scale_x = width != out_width;
		int scale_y = height != out_height;
#ifdef CONFIG_ARCH_OMAP4
		u16 out_ch_height = out_height;
		u16 out_ch_width = out_width;
		u16 ch_height = height;
		u16 ch_width = width;
		int scale_uv = 0;

		/* account for chroma decimation */
		switch (color_mode) {
		case OMAP_DSS_COLOR_NV12:
			ch_height >>= 1; /* Y downsampled by 2 */
		case OMAP_DSS_COLOR_YUV2:
		case OMAP_DSS_COLOR_UYVY:
			ch_width >>= 1; /* X downsampled by 2 */
			/* must use FIR for YUV422 if rotated */
			if (color_mode != OMAP_DSS_COLOR_NV12 && rotation % 4)
				scale_x = scale_y = 1;
			scale_uv = 1;
			break;
		default:
			/* no UV scaling for RGB formats for now */
			break;
			}

		if (out_ch_width != ch_width)
			scale_x = true;
		if (out_ch_height != ch_height)
			scale_y = true;
		/* set up UV scaling */
		_dispc_set_scaling_uv(plane, ch_width, ch_height,
			out_ch_width, out_ch_height, ilace,
			three_taps, fieldmode, scale_uv && scale_x,
			scale_uv && scale_y);
		if (!scale_uv || (!scale_x && !scale_y))
			/* :TRICKY: set chroma resampling for RGB formats */
			REG_FLD_MOD(DISPC_VID_ATTRIBUTES2(plane - 1), 0, 8, 8);
#endif
		_dispc_set_scaling(plane, width, height,
					out_width, out_height,
					ilace, three_taps, fieldmode,
					scale_x, scale_y, vdma,
					flicker_filter);
		_dispc_set_vdma_attrs(plane, vdma);
		_dispc_set_vid_size(plane, out_width, out_height);
		_dispc_set_vid_color_conv(plane, cconv);
	}

	_dispc_set_rotation_attrs(plane, rotation, mirror, color_mode, vdma);

	if (cpu_is_omap3630() && (plane != OMAP_DSS_VIDEO1))
		_dispc_set_alpha_blend_attrs(plane, pre_alpha_mult);

#ifndef CONFIG_ARCH_OMAP4
	if (plane != OMAP_DSS_VIDEO1)
#endif
		_dispc_setup_global_alpha(plane, global_alpha);

	pix_inc = dispc_read_reg(dispc_reg_att[plane]);
	DSSDBG("vid[%d] attributes = %x\n", plane, pix_inc);

	return 0;
}

static void _dispc_enable_plane(enum omap_plane plane, bool enable)
{
	REG_FLD_MOD(dispc_reg_att[plane], enable ? 1 : 0, 0, 0);
	if (!enable) { /* clear out resizer related bits */
		REG_FLD_MOD(dispc_reg_att[plane], 0x00, 6, 5);
		REG_FLD_MOD(dispc_reg_att[plane], 0x00, 21, 21);
		REG_FLD_MOD(dispc_reg_att[plane], 0x00, 22, 22);
	}
}

static void dispc_disable_isr(void *data, u32 mask)
{
	struct completion *compl = data;
	complete(compl);
}

static void _enable_lcd_out(enum omap_channel channel, bool enable)
{
#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel)
		REG_FLD_MOD(DISPC_CONTROL2, enable ? 1 : 0, 0, 0);
	else
#endif
		REG_FLD_MOD(DISPC_CONTROL, enable ? 1 : 0, 0, 0);
}

void dispc_enable_lcd_out(enum omap_channel channel, bool enable)
{
	struct completion frame_done_completion;
	bool is_on;
	int r;
	int irq;

	enable_clocks(1);

	/* When we disable LCD output, we need to wait until frame is done.
	 * Otherwise the DSS is still working, and turning off the clocks
	 * prevents DSS from going to OFF mode */
#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel) {
		is_on = REG_GET(DISPC_CONTROL2, 0, 0);
		irq = DISPC_IRQ_FRAMEDONE2;
	} else
#endif
	{
		is_on = REG_GET(DISPC_CONTROL, 0, 0);
		irq = DISPC_IRQ_FRAMEDONE;
	}

	if (!enable && is_on) {
		init_completion(&frame_done_completion);

		r = omap_dispc_register_isr(dispc_disable_isr,
				&frame_done_completion,
				irq);

		if (r)
			DSSERR("failed to register FRAMEDONE isr\n");
	}

	_enable_lcd_out(channel, enable);

	if (!enable && is_on) {
		if (!wait_for_completion_timeout(&frame_done_completion,
					msecs_to_jiffies(100)))
			DSSERR("timeout waiting for FRAME DONE\n");

		r = omap_dispc_unregister_isr(dispc_disable_isr,
				&frame_done_completion,
				irq);

		if (r)
			DSSERR("failed to unregister FRAMEDONE isr\n");
	}

	enable_clocks(0);
}

static void _enable_digit_out(bool enable)
{
	REG_FLD_MOD(DISPC_CONTROL, enable ? 1 : 0, 1, 1);
}

void dispc_enable_digit_out(bool enable)
{
	struct completion frame_done_completion;
	int r;

	enable_clocks(1);

	if (REG_GET(DISPC_CONTROL, 1, 1) == enable) {
		enable_clocks(0);
		return;
	}

	if (enable) {
		unsigned long flags;
		/* When we enable digit output, we'll get an extra digit
		 * sync lost interrupt, that we need to ignore */
		spin_lock_irqsave(&dispc.irq_lock, flags);
		dispc.irq_error_mask &= ~DISPC_IRQ_SYNC_LOST_DIGIT;
		_omap_dispc_set_irqs();
		spin_unlock_irqrestore(&dispc.irq_lock, flags);
	}

	/* When we disable digit output, we need to wait until fields are done.
	 * Otherwise the DSS is still working, and turning off the clocks
	 * prevents DSS from going to OFF mode. And when enabling, we need to
	 * wait for the extra sync losts */
	init_completion(&frame_done_completion);

	r = omap_dispc_register_isr(dispc_disable_isr, &frame_done_completion,
			DISPC_IRQ_EVSYNC_EVEN | DISPC_IRQ_EVSYNC_ODD);
	if (r)
		DSSERR("failed to register EVSYNC isr\n");

	_enable_digit_out(enable);

	/* XXX I understand from TRM that we should only wait for the
	 * current field to complete. But it seems we have to wait
	 * for both fields */
	if (!cpu_is_omap44xx()) {
	if (!wait_for_completion_timeout(&frame_done_completion,
				msecs_to_jiffies(100)))
		DSSERR("timeout waiting for EVSYNC\n");

	if (!wait_for_completion_timeout(&frame_done_completion,
				msecs_to_jiffies(100)))
		DSSERR("timeout waiting for EVSYNC\n");
	}

	r = omap_dispc_unregister_isr(dispc_disable_isr,
			&frame_done_completion,
			DISPC_IRQ_EVSYNC_EVEN | DISPC_IRQ_EVSYNC_ODD);
	if (r)
		DSSERR("failed to unregister EVSYNC isr\n");

	if (enable) {
		unsigned long flags;
		spin_lock_irqsave(&dispc.irq_lock, flags);
		dispc.irq_error_mask = DISPC_IRQ_MASK_ERROR;
		dispc_write_reg(DISPC_IRQSTATUS, DISPC_IRQ_SYNC_LOST_DIGIT);
		_omap_dispc_set_irqs();
		spin_unlock_irqrestore(&dispc.irq_lock, flags);
	}

	enable_clocks(0);
}

void dispc_lcd_enable_signal_polarity(bool act_high)
{
	enable_clocks(1);
	REG_FLD_MOD(DISPC_CONTROL, act_high ? 1 : 0, 29, 29);
	enable_clocks(0);
}

void dispc_lcd_enable_signal(bool enable)
{
	enable_clocks(1);
	REG_FLD_MOD(DISPC_CONTROL, enable ? 1 : 0, 28, 28);
	enable_clocks(0);
}

void dispc_pck_free_enable(bool enable)
{
	enable_clocks(1);
	REG_FLD_MOD(DISPC_CONTROL, enable ? 1 : 0, 27, 27);
	enable_clocks(0);
}

void dispc_enable_fifohandcheck(bool enable)
{
	enable_clocks(1);
	REG_FLD_MOD(DISPC_CONFIG, enable ? 1 : 0, 16, 16);
	enable_clocks(0);
}


void dispc_set_lcd_display_type(enum omap_channel channel,
				enum omap_lcd_display_type type)
{
	int mode;

	switch (type) {
	case OMAP_DSS_LCD_DISPLAY_STN:
		mode = 0;
		break;

	case OMAP_DSS_LCD_DISPLAY_TFT:
		mode = 1;
		break;

	default:
		BUG();
		return;
	}

	enable_clocks(1);
#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel)
		REG_FLD_MOD(DISPC_CONTROL2, mode, 3, 3);
	else
#endif
		REG_FLD_MOD(DISPC_CONTROL, mode, 3, 3);
	enable_clocks(0);
}

void dispc_set_loadmode(enum omap_dss_load_mode mode)
{
	enable_clocks(1);
	REG_FLD_MOD(DISPC_CONFIG, mode, 2, 1);
	enable_clocks(0);
}


void dispc_set_default_color(enum omap_channel channel, u32 color)
{
	const struct dispc_reg def_reg[] = { DISPC_DEFAULT_COLOR0,
				DISPC_DEFAULT_COLOR1
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_DEFAULT_COLOR2
#endif
	};

	enable_clocks(1);
	dispc_write_reg(def_reg[channel], color);
	enable_clocks(0);
}

u32 dispc_get_default_color(enum omap_channel channel)
{
	const struct dispc_reg def_reg[] = { DISPC_DEFAULT_COLOR0,
				DISPC_DEFAULT_COLOR1
#ifdef CONFIG_ARCH_OMAP4
				, DISPC_DEFAULT_COLOR2
#endif
};

	u32 l;

#ifndef CONFIG_ARCH_OMAP4
	BUG_ON(channel != OMAP_DSS_CHANNEL_DIGIT &&
	       channel != OMAP_DSS_CHANNEL_LCD);
#endif

	enable_clocks(1);
	l = dispc_read_reg(def_reg[channel]);
	enable_clocks(0);

	return l;
}

void dispc_set_trans_key(enum omap_channel ch,
		enum omap_dss_trans_key_type type,
		u32 trans_key)
{
	const struct dispc_reg tr_reg[] = {
		DISPC_TRANS_COLOR0, DISPC_TRANS_COLOR1
#ifdef CONFIG_ARCH_OMAP4
		, DISPC_TRANS_COLOR2
#endif

	};

	enable_clocks(1);
#ifdef CONFIG_ARCH_OMAP4
	if (ch == OMAP_DSS_CHANNEL_LCD2)
		REG_FLD_MOD(DISPC_CONFIG2, type, 11, 11);
	else
#endif
	if (ch == OMAP_DSS_CHANNEL_LCD)
		REG_FLD_MOD(DISPC_CONFIG, type, 11, 11);
	else /* OMAP_DSS_CHANNEL_DIGIT */
		REG_FLD_MOD(DISPC_CONFIG, type, 13, 13);

	dispc_write_reg(tr_reg[ch], trans_key);
	enable_clocks(0);
}

void dispc_get_trans_key(enum omap_channel ch,
		enum omap_dss_trans_key_type *type,
		u32 *trans_key)
{
	const struct dispc_reg tr_reg[] = {
		DISPC_TRANS_COLOR0, DISPC_TRANS_COLOR1
#ifdef CONFIG_ARCH_OMAP4
		, DISPC_TRANS_COLOR2
#endif

	};

	enable_clocks(1);
	if (type) {
#ifdef CONFIG_ARCH_OMAP4
		if (ch == OMAP_DSS_CHANNEL_LCD2)
			*type = REG_GET(DISPC_CONFIG2, 11, 11);
		else
#endif
		if (ch == OMAP_DSS_CHANNEL_LCD)
			*type = REG_GET(DISPC_CONFIG, 11, 11);
		else if (ch == OMAP_DSS_CHANNEL_DIGIT)
			*type = REG_GET(DISPC_CONFIG, 13, 13);
		else
			BUG();
	}

	if (trans_key)
		*trans_key = dispc_read_reg(tr_reg[ch]);
	enable_clocks(0);
}

void dispc_enable_trans_key(enum omap_channel ch, bool enable)
{
	enable_clocks(1);
#ifdef CONFIG_ARCH_OMAP4
	if (ch == OMAP_DSS_CHANNEL_LCD2)
		REG_FLD_MOD(DISPC_CONFIG2, enable, 10, 10);
	else
#endif
	if (ch == OMAP_DSS_CHANNEL_LCD)
		REG_FLD_MOD(DISPC_CONFIG, enable, 10, 10);
	else /* OMAP_DSS_CHANNEL_DIGIT */
		REG_FLD_MOD(DISPC_CONFIG, enable, 12, 12);
	enable_clocks(0);
}
void dispc_enable_alpha_blending(enum omap_channel ch, bool enable)
{
	if (cpu_is_omap24xx())
		return;

	enable_clocks(1);
	if (ch == OMAP_DSS_CHANNEL_LCD)
		REG_FLD_MOD(DISPC_CONFIG, enable, 18, 18);
	else /* OMAP_DSS_CHANNEL_DIGIT */
		REG_FLD_MOD(DISPC_CONFIG, enable, 19, 19);
	enable_clocks(0);
}
bool dispc_alpha_blending_enabled(enum omap_channel ch)
{
	bool enabled;

	if (cpu_is_omap24xx())
		return false;

	enable_clocks(1);
	if (ch == OMAP_DSS_CHANNEL_LCD)
		enabled = REG_GET(DISPC_CONFIG, 18, 18);
	else if (ch == OMAP_DSS_CHANNEL_DIGIT)
		enabled = REG_GET(DISPC_CONFIG, 18, 18);
	else
		BUG();
	enable_clocks(0);

	return enabled;

}


bool dispc_trans_key_enabled(enum omap_channel ch)
{
	bool enabled;
#ifdef CONFIG_ARCH_OMAP4
	BUG_ON(ch == OMAP_DSS_CHANNEL_LCD2);
#endif
	enable_clocks(1);
#ifdef CONFIG_ARCH_OMAP4
	if (ch == OMAP_DSS_CHANNEL_LCD2)
		enabled = REG_GET(DISPC_CONFIG2, 10, 10);
	else
#endif
	if (ch == OMAP_DSS_CHANNEL_LCD)
		enabled = REG_GET(DISPC_CONFIG, 10, 10);
	else if (ch == OMAP_DSS_CHANNEL_DIGIT)
		enabled = REG_GET(DISPC_CONFIG, 12, 12);
	else
		BUG();
	enable_clocks(0);

	return enabled;
}

int dispc_enable_gamma(enum omap_channel ch, u8 gamma)
{
#ifdef CONFIG_ARCH_OMAP4
	u32 i, temp, channel;
	static int enabled;

	channel = ch == OMAP_DSS_CHANNEL_LCD ? 0 :
		 ch == OMAP_DSS_CHANNEL_LCD2 ? 1 : 2;

	enable_clocks(1);

	if (gamma > NO_OF_GAMMA_TABLES)
		return -EINVAL;

	if (gamma) {
		u8 *tablePtr = gammaTablePtr[gamma - 1];

		for (i = 0; i < GAMMA_TBL_SZ; i++) {
			temp =  tablePtr[i];
			temp =  (i<<24)|(temp|(temp<<8)|(temp<<16));
			dispc_write_reg(DISPC_GAMMA_TABLE(channel), temp);
		}
	}
	enabled = enabled & ~(1 << channel) | (gamma ? (1 << channel) : 0);
	REG_FLD_MOD(DISPC_CONFIG, (enabled & 1), 3, 3);
	REG_FLD_MOD(DISPC_CONFIG, !!(enabled & 6), 9, 9);

	return 0;
#endif
}
void dispc_enable_dither(bool enable)
{
	enable_clocks(1);

	REG_FLD_MOD(DISPC_CONTROL, enable, 7, 7);

	enable_clocks(0);
}

void dispc_set_dither_mode(enum omap_dss_dither_mode mode)
{
	enable_clocks(1);

	REG_FLD_MOD(DISPC_CONTROL, mode, 31, 30);

	enable_clocks(0);
}

int dispc_set_tft_data_lines(enum omap_channel channel, u8 data_lines)
{
	int code;

	switch (data_lines) {
	case 12:
		code = 0;
		break;
	case 16:
		code = 1;
		break;
	case 18:
		code = 2;
		break;
	case 24:
		code = 3;
		break;
	default:
		return -EINVAL;
	}

	enable_clocks(1);
#ifdef CONFIG_ARCH_OMAP4
	if (channel == OMAP_DSS_CHANNEL_LCD2)
		REG_FLD_MOD(DISPC_CONTROL2, code, 9, 8);
	else
#endif
		REG_FLD_MOD(DISPC_CONTROL, code, 9, 8);
	enable_clocks(0);
	return 0;
}

void dispc_set_parallel_interface_mode(enum omap_channel channel,
				enum omap_parallel_interface_mode mode)
{
	u32 l;
	int stallmode;
	int gpout0 = 1;
	int gpout1;

	switch (mode) {
	case OMAP_DSS_PARALLELMODE_BYPASS:
		stallmode = 0;
		gpout1 = 1;
		break;

	case OMAP_DSS_PARALLELMODE_RFBI:
		stallmode = 1;
		gpout1 = 0;
		break;

	case OMAP_DSS_PARALLELMODE_DSI:
		stallmode = 1;
		gpout1 = 1;
		break;

	default:
		BUG();
		return;
	}

	enable_clocks(1);
#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel) {
		l = dispc_read_reg(DISPC_CONTROL2);

		printk(KERN_INFO "OMAP DISPCONTROL read (stallmode)%d\n",
		FLD_GET(l, 11, 11));

		l = FLD_MOD(l, stallmode, 11, 11);

		dispc_write_reg(DISPC_CONTROL2, l);

	} else
#endif
	{
		l = dispc_read_reg(DISPC_CONTROL);

		printk(KERN_INFO "OMAP DISPCONTROL read (stallmode)%d\n",
			FLD_GET(l, 11, 11));
		printk(KERN_INFO "OMAP DISPCONTROL read (gpout)%d\n",
			FLD_GET(l, 15, 15));
		printk(KERN_INFO "OMAP DISPCONTROL read (stallmode)%d\n",
			FLD_GET(l, 16, 16));


		l = FLD_MOD(l, stallmode, 11, 11);
		l = FLD_MOD(l, gpout0, 15, 15);
		l = FLD_MOD(l, gpout1, 16, 16);

		dispc_write_reg(DISPC_CONTROL, l);
	}
	enable_clocks(0);
}

static bool _dispc_lcd_timings_ok(int hsw, int hfp, int hbp,
					int vsw, int vfp, int vbp)
{
	if (cpu_is_omap24xx() || omap_rev() < OMAP3430_REV_ES3_0) {
		if (hsw < 1 || hsw > 64 ||
				hfp < 1 || hfp > 256 ||
				hbp < 1 || hbp > 256 ||
				vsw < 1 || vsw > 64 ||
				vfp < 0 || vfp > 255 ||
				vbp < 0 || vbp > 255)
			return false;
	} else {
		if (hsw < 1 || hsw > 256 ||
				hfp < 1 || hfp > 4096 ||
				hbp < 1 || hbp > 4096 ||
				vsw < 1 || vsw > 256 ||
				vfp < 0 || vfp > 4095 ||
				vbp < 0 || vbp > 4095)
			return false;
	}

	return true;
}

bool dispc_lcd_timings_ok(struct omap_video_timings *timings)
{

	return _dispc_lcd_timings_ok(timings->hsw, timings->hfp,
			timings->hbp, timings->vsw,
			timings->vfp, timings->vbp);
}

static void _dispc_set_lcd_timings(enum omap_channel channel, int hsw,
				int hfp, int hbp, int vsw, int vfp, int vbp)
{
	u32 timing_h, timing_v;

	if (cpu_is_omap24xx() || omap_rev() < OMAP3430_REV_ES3_0) {
		timing_h = FLD_VAL(hsw-1, 5, 0) | FLD_VAL(hfp-1, 15, 8) |
					FLD_VAL(hbp-1, 27, 20);

		timing_v = FLD_VAL(vsw-1, 5, 0) | FLD_VAL(vfp, 15, 8) |
					FLD_VAL(vbp, 27, 20);
	} else {

		timing_h = FLD_VAL(hsw-1, 7, 0) | FLD_VAL(hfp-1, 19, 8) |
					FLD_VAL(hbp-1, 31, 20);

		timing_v = FLD_VAL(vsw-1, 7, 0) | FLD_VAL(vfp, 19, 8) |
					FLD_VAL(vbp, 31, 20);
	}
	enable_clocks(1);

#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel) {
		dispc_write_reg(DISPC_TIMING_H2, timing_h);
		dispc_write_reg(DISPC_TIMING_V2, timing_v);
	} else
#endif
	{
		dispc_write_reg(DISPC_TIMING_H, timing_h);
		dispc_write_reg(DISPC_TIMING_V, timing_v);
	}
	enable_clocks(0);
}


void dispc_set_lcd_timings(enum omap_channel channel,
				struct omap_video_timings *timings)
{
	unsigned xtot, ytot;
	unsigned long ht, vt;

	if (!_dispc_lcd_timings_ok(timings->hsw, timings->hfp,
				timings->hbp, timings->vsw,
				timings->vfp, timings->vbp))
		BUG();

	_dispc_set_lcd_timings(channel, timings->hsw, timings->hfp,
				timings->hbp, timings->vsw, timings->vfp,
				timings->vbp);

	dispc_set_lcd_size(channel, timings->x_res, timings->y_res);

	xtot = timings->x_res + timings->hfp + timings->hsw + timings->hbp;
	ytot = timings->y_res + timings->vfp + timings->vsw + timings->vbp;

	ht = (timings->pixel_clock * 1000) / xtot;
	vt = (timings->pixel_clock * 1000) / xtot / ytot;

	DSSDBG("channel %u xres %u yres %u\n", channel, timings->x_res,
							timings->y_res);
	DSSDBG("pck %u\n", timings->pixel_clock);
	DSSDBG("hsw %d hfp %d hbp %d vsw %d vfp %d vbp %d\n",
			timings->hsw, timings->hfp, timings->hbp,
			timings->vsw, timings->vfp, timings->vbp);

	DSSDBG("hsync %luHz, vsync %luHz\n", ht, vt);
}

void dispc_set_lcd_divisor(enum omap_channel channel, u16 lck_div,
								u16 pck_div)
{
	BUG_ON(lck_div < 1);
	BUG_ON(pck_div < 2);

	enable_clocks(1);
#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel)
		dispc_write_reg(DISPC_DIVISOR2,
			FLD_VAL(lck_div, 23, 16) | FLD_VAL(pck_div, 7, 0));
	else
		dispc_write_reg(DISPC_DIVISOR1,
			FLD_VAL(lck_div, 23, 16) | FLD_VAL(pck_div, 7, 0));
#else
		dispc_write_reg(DISPC_DIVISOR,
			FLD_VAL(lck_div, 23, 16) | FLD_VAL(pck_div, 7, 0));
#endif
	enable_clocks(0);
}

static void dispc_get_lcd_divisor(enum omap_channel channel,
					int *lck_div, int *pck_div)
{
	u32 l;

#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel)
		l = dispc_read_reg(DISPC_DIVISOR2);
	else
		l = dispc_read_reg(DISPC_DIVISOR1);
#else
		l = dispc_read_reg(DISPC_DIVISOR);
#endif
	*lck_div = FLD_GET(l, 23, 16);
	*pck_div = FLD_GET(l, 7, 0);
}
/* TODO: Check with Senthil on handling of clocks */
unsigned long dispc_fclk_rate(void)
{
	unsigned long r = 0;

	if (dss_get_dispc_clk_source() == 0)
		r = dss_clk_get_rate(DSS_CLK_FCK1);
	else
#ifdef CONFIG_OMAP2_DSS_DSI
		r = dsi_get_dsi1_pll_rate(0);
#else
	BUG();
#endif
	return r;
}

unsigned long dispc_lclk_rate(enum omap_channel channel)
{
	int lcd;
	unsigned long r;
	u32 l;
#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel)
		l = dispc_read_reg(DISPC_DIVISOR2);
	else
		l = dispc_read_reg(DISPC_DIVISOR1);
#else
	l = dispc_read_reg(DISPC_DIVISOR);
#endif
	lcd = FLD_GET(l, 23, 16);

	r = dispc_fclk_rate();

	return r / lcd;
}

unsigned long dispc_pclk_rate(enum omap_channel channel)
{
	int lcd, pcd;
	unsigned long r;
	u32 l;
#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2 == channel)
		l = dispc_read_reg(DISPC_DIVISOR2);
	else
		l = dispc_read_reg(DISPC_DIVISOR1);
#else
	l = dispc_read_reg(DISPC_DIVISOR);
#endif
	lcd = FLD_GET(l, 23, 16);
	pcd = FLD_GET(l, 7, 0);

	r = dispc_fclk_rate();

	return r / lcd / pcd;
}

void dispc_dump_clocks(struct seq_file *s)
{
	int lcd, pcd;

	enable_clocks(1);

	dispc_get_lcd_divisor(OMAP_DSS_CHANNEL_LCD, &lcd, &pcd);

	seq_printf(s, "- DISPC -\n");

	seq_printf(s, "dispc fclk source = %s\n",
			dss_get_dispc_clk_source() == 0 ?
			"dss1_alwon_fclk" : "dsi1_pll_fclk");

	seq_printf(s, "fck\t\t%-16lu\n", dispc_fclk_rate());
	seq_printf(s, "lck\t\t%-16lulck div\t%u\n", dispc_lclk_rate(OMAP_DSS_CHANNEL_LCD), lcd);
	seq_printf(s, "pck\t\t%-16lupck div\t%u\n", dispc_pclk_rate(OMAP_DSS_CHANNEL_LCD), pcd);

#ifdef CONFIG_ARCH_OMAP4
        dispc_get_lcd_divisor(OMAP_DSS_CHANNEL_LCD2, &lcd, &pcd);

        seq_printf(s, "- DISPC - LCD 2\n");

        seq_printf(s, "dispc fclk source = %s\n",
                        dss_get_dispc_clk_source() == 0 ?
                        "dss1_alwon_fclk" : "dsi1_pll_fclk");

        seq_printf(s, "fck\t\t%-16lu\n", dispc_fclk_rate());
        seq_printf(s, "lck\t\t%-16lulck div\t%u\n", dispc_lclk_rate(OMAP_DSS_CHANNEL_LCD), lcd);
        seq_printf(s, "pck\t\t%-16lupck div\t%u\n", dispc_pclk_rate(OMAP_DSS_CHANNEL_LCD2), pcd);
#endif

	enable_clocks(0);
}

void dispc_dump_regs(struct seq_file *s)
{
#define DUMPREG(r) seq_printf(s, "%-35s %08x\n", #r, dispc_read_reg(r))


	dss_clk_enable(DSS_CLK_ICK | DSS_CLK_FCK1);

	DUMPREG(DISPC_REVISION);
	DUMPREG(DISPC_SYSCONFIG);
	DUMPREG(DISPC_SYSSTATUS);
	DUMPREG(DISPC_IRQSTATUS);
	DUMPREG(DISPC_IRQENABLE);
	DUMPREG(DISPC_CONTROL);
	DUMPREG(DISPC_CONFIG);
	DUMPREG(DISPC_CAPABLE);
	DUMPREG(DISPC_DEFAULT_COLOR0);
	DUMPREG(DISPC_DEFAULT_COLOR1);
	DUMPREG(DISPC_TRANS_COLOR0);
	DUMPREG(DISPC_TRANS_COLOR1);
	DUMPREG(DISPC_LINE_STATUS);
	DUMPREG(DISPC_LINE_NUMBER);
	DUMPREG(DISPC_TIMING_H);
	DUMPREG(DISPC_TIMING_V);
	DUMPREG(DISPC_POL_FREQ);
#ifdef CONFIG_ARCH_OMAP4
	DUMPREG(DISPC_DIVISOR1);
#else
	DUMPREG(DISPC_DIVISOR);
#endif
	DUMPREG(DISPC_GLOBAL_ALPHA);
	DUMPREG(DISPC_SIZE_DIG);
	DUMPREG(DISPC_SIZE_LCD);

	DUMPREG(DISPC_GFX_BA0);
	DUMPREG(DISPC_GFX_BA1);
	DUMPREG(DISPC_GFX_POSITION);
	DUMPREG(DISPC_GFX_SIZE);
	DUMPREG(DISPC_GFX_ATTRIBUTES);
	DUMPREG(DISPC_GFX_FIFO_THRESHOLD);
	DUMPREG(DISPC_GFX_FIFO_SIZE_STATUS);
	DUMPREG(DISPC_GFX_ROW_INC);
	DUMPREG(DISPC_GFX_PIXEL_INC);
	DUMPREG(DISPC_GFX_WINDOW_SKIP);
	DUMPREG(DISPC_GFX_TABLE_BA);

	DUMPREG(DISPC_DATA_CYCLE1);
	DUMPREG(DISPC_DATA_CYCLE2);
	DUMPREG(DISPC_DATA_CYCLE3);

	DUMPREG(DISPC_CPR_COEF_R);
	DUMPREG(DISPC_CPR_COEF_G);
	DUMPREG(DISPC_CPR_COEF_B);

	DUMPREG(DISPC_GFX_PRELOAD);

	DUMPREG(DISPC_VID_BA0(0));
	DUMPREG(DISPC_VID_BA1(0));
	DUMPREG(DISPC_VID_POSITION(0));
	DUMPREG(DISPC_VID_SIZE(0));
	DUMPREG(DISPC_VID_ATTRIBUTES(0));
	DUMPREG(DISPC_VID_FIFO_THRESHOLD(0));
	DUMPREG(DISPC_VID_FIFO_SIZE_STATUS(0));
	DUMPREG(DISPC_VID_ROW_INC(0));
	DUMPREG(DISPC_VID_PIXEL_INC(0));
	DUMPREG(DISPC_VID_FIR(0));
	DUMPREG(DISPC_VID_PICTURE_SIZE(0));
	DUMPREG(DISPC_VID_ACCU0(0));
	DUMPREG(DISPC_VID_ACCU1(0));

	DUMPREG(DISPC_VID_BA0(1));
	DUMPREG(DISPC_VID_BA1(1));
	DUMPREG(DISPC_VID_POSITION(1));
	DUMPREG(DISPC_VID_SIZE(1));
	DUMPREG(DISPC_VID_ATTRIBUTES(1));
	DUMPREG(DISPC_VID_FIFO_THRESHOLD(1));
	DUMPREG(DISPC_VID_FIFO_SIZE_STATUS(1));
	DUMPREG(DISPC_VID_ROW_INC(1));
	DUMPREG(DISPC_VID_PIXEL_INC(1));
	DUMPREG(DISPC_VID_FIR(1));
	DUMPREG(DISPC_VID_PICTURE_SIZE(1));
	DUMPREG(DISPC_VID_ACCU0(1));
	DUMPREG(DISPC_VID_ACCU1(1));

	DUMPREG(DISPC_VID_FIR_COEF_H(0, 0));
	DUMPREG(DISPC_VID_FIR_COEF_H(0, 1));
	DUMPREG(DISPC_VID_FIR_COEF_H(0, 2));
	DUMPREG(DISPC_VID_FIR_COEF_H(0, 3));
	DUMPREG(DISPC_VID_FIR_COEF_H(0, 4));
	DUMPREG(DISPC_VID_FIR_COEF_H(0, 5));
	DUMPREG(DISPC_VID_FIR_COEF_H(0, 6));
	DUMPREG(DISPC_VID_FIR_COEF_H(0, 7));
	DUMPREG(DISPC_VID_FIR_COEF_HV(0, 0));
	DUMPREG(DISPC_VID_FIR_COEF_HV(0, 1));
	DUMPREG(DISPC_VID_FIR_COEF_HV(0, 2));
	DUMPREG(DISPC_VID_FIR_COEF_HV(0, 3));
	DUMPREG(DISPC_VID_FIR_COEF_HV(0, 4));
	DUMPREG(DISPC_VID_FIR_COEF_HV(0, 5));
	DUMPREG(DISPC_VID_FIR_COEF_HV(0, 6));
	DUMPREG(DISPC_VID_FIR_COEF_HV(0, 7));
	DUMPREG(DISPC_VID_CONV_COEF(0, 0));
	DUMPREG(DISPC_VID_CONV_COEF(0, 1));
	DUMPREG(DISPC_VID_CONV_COEF(0, 2));
	DUMPREG(DISPC_VID_CONV_COEF(0, 3));
	DUMPREG(DISPC_VID_CONV_COEF(0, 4));
	DUMPREG(DISPC_VID_FIR_COEF_V(0, 0));
	DUMPREG(DISPC_VID_FIR_COEF_V(0, 1));
	DUMPREG(DISPC_VID_FIR_COEF_V(0, 2));
	DUMPREG(DISPC_VID_FIR_COEF_V(0, 3));
	DUMPREG(DISPC_VID_FIR_COEF_V(0, 4));
	DUMPREG(DISPC_VID_FIR_COEF_V(0, 5));
	DUMPREG(DISPC_VID_FIR_COEF_V(0, 6));
	DUMPREG(DISPC_VID_FIR_COEF_V(0, 7));

	DUMPREG(DISPC_VID_FIR_COEF_H(1, 0));
	DUMPREG(DISPC_VID_FIR_COEF_H(1, 1));
	DUMPREG(DISPC_VID_FIR_COEF_H(1, 2));
	DUMPREG(DISPC_VID_FIR_COEF_H(1, 3));
	DUMPREG(DISPC_VID_FIR_COEF_H(1, 4));
	DUMPREG(DISPC_VID_FIR_COEF_H(1, 5));
	DUMPREG(DISPC_VID_FIR_COEF_H(1, 6));
	DUMPREG(DISPC_VID_FIR_COEF_H(1, 7));
	DUMPREG(DISPC_VID_FIR_COEF_HV(1, 0));
	DUMPREG(DISPC_VID_FIR_COEF_HV(1, 1));
	DUMPREG(DISPC_VID_FIR_COEF_HV(1, 2));
	DUMPREG(DISPC_VID_FIR_COEF_HV(1, 3));
	DUMPREG(DISPC_VID_FIR_COEF_HV(1, 4));
	DUMPREG(DISPC_VID_FIR_COEF_HV(1, 5));
	DUMPREG(DISPC_VID_FIR_COEF_HV(1, 6));
	DUMPREG(DISPC_VID_FIR_COEF_HV(1, 7));
	DUMPREG(DISPC_VID_CONV_COEF(1, 0));
	DUMPREG(DISPC_VID_CONV_COEF(1, 1));
	DUMPREG(DISPC_VID_CONV_COEF(1, 2));
	DUMPREG(DISPC_VID_CONV_COEF(1, 3));
	DUMPREG(DISPC_VID_CONV_COEF(1, 4));
	DUMPREG(DISPC_VID_FIR_COEF_V(1, 0));
	DUMPREG(DISPC_VID_FIR_COEF_V(1, 1));
	DUMPREG(DISPC_VID_FIR_COEF_V(1, 2));
	DUMPREG(DISPC_VID_FIR_COEF_V(1, 3));
	DUMPREG(DISPC_VID_FIR_COEF_V(1, 4));
	DUMPREG(DISPC_VID_FIR_COEF_V(1, 5));
	DUMPREG(DISPC_VID_FIR_COEF_V(1, 6));
	DUMPREG(DISPC_VID_FIR_COEF_V(1, 7));

	DUMPREG(DISPC_VID_PRELOAD(0));
	DUMPREG(DISPC_VID_PRELOAD(1));
/* TODO: OMAP4: add new registers here */

	dss_clk_disable(DSS_CLK_ICK | DSS_CLK_FCK1);
#undef DUMPREG
}

static void _dispc_set_pol_freq(enum omap_channel channel, bool onoff,
				bool rf, bool ieo, bool ipc, bool ihs,
				bool ivs, u8 acbi, u8 acb)
{
	u32 l = 0;

	DSSDBG("onoff %d rf %d ieo %d ipc %d ihs %d ivs %d acbi %d acb %d\n",
			onoff, rf, ieo, ipc, ihs, ivs, acbi, acb);
/*
	if (cpu_is_omap3630()) {
		l = 0x00033028;
	} else 
*/
	{
		l |= FLD_VAL(onoff, 17, 17);
		l |= FLD_VAL(rf, 16, 16);
		l |= FLD_VAL(ieo, 15, 15);
		l |= FLD_VAL(ipc, 14, 14);
		l |= FLD_VAL(ihs, 13, 13);
		l |= FLD_VAL(ivs, 12, 12);
		l |= FLD_VAL(acbi, 11, 8);
		l |= FLD_VAL(acb, 7, 0);
	}


	enable_clocks(1);
#ifdef CONFIG_ARCH_OMAP4
	if (OMAP_DSS_CHANNEL_LCD2)
		dispc_write_reg(DISPC_POL_FREQ2, l);
	else
#endif
		dispc_write_reg(DISPC_POL_FREQ, l);
	enable_clocks(0);
}

void dispc_set_pol_freq(enum omap_channel ch, enum omap_panel_config config,
							u8 acbi, u8 acb)
{
	_dispc_set_pol_freq(ch, (config & OMAP_DSS_LCD_ONOFF) != 0,
			(config & OMAP_DSS_LCD_RF) != 0,
			(config & OMAP_DSS_LCD_IEO) != 0,
			(config & OMAP_DSS_LCD_IPC) != 0,
			(config & OMAP_DSS_LCD_IHS) != 0,
			(config & OMAP_DSS_LCD_IVS) != 0,
			acbi, acb);
}

/* with fck as input clock rate, find dispc dividers that produce req_pck */
void dispc_find_clk_divs(bool is_tft, unsigned long req_pck, unsigned long fck,
		struct dispc_clock_info *cinfo)
{
	u16 pcd_min = is_tft ? 2 : 3;
	unsigned long best_pck;
	u16 best_ld, cur_ld;
	u16 best_pd, cur_pd;

	best_pck = 0;
	best_ld = 0;
	best_pd = 0;

	for (cur_ld = 1; cur_ld <= 255; ++cur_ld) {
		unsigned long lck = fck / cur_ld;

		for (cur_pd = pcd_min; cur_pd <= 255; ++cur_pd) {
			unsigned long pck = lck / cur_pd;
			long old_delta = abs(best_pck - req_pck);
			long new_delta = abs(pck - req_pck);

			if (best_pck == 0 || new_delta < old_delta) {
				best_pck = pck;
				best_ld = cur_ld;
				best_pd = cur_pd;

				if (pck == req_pck)
					goto found;
			}

			if (pck < req_pck)
				break;
		}

		if (lck / pcd_min < req_pck)
			break;
	}

found:
	cinfo->lck_div = best_ld;
	cinfo->pck_div = best_pd;
	cinfo->lck = fck / cinfo->lck_div;
	cinfo->pck = cinfo->lck / cinfo->pck_div;
}

/* calculate clock rates using dividers in cinfo */
int dispc_calc_clock_rates(unsigned long dispc_fclk_rate,
		struct dispc_clock_info *cinfo)
{
	if (cinfo->lck_div > 255 || cinfo->lck_div == 0)
		return -EINVAL;
	if (cinfo->pck_div < 2 || cinfo->pck_div > 255)
		return -EINVAL;

	cinfo->lck = dispc_fclk_rate / cinfo->lck_div;
	cinfo->pck = cinfo->lck / cinfo->pck_div;

	return 0;
}

int dispc_set_clock_div(enum omap_channel channel,
		struct dispc_clock_info *cinfo)
{
	DSSDBG("lck = %lu (%u)\n", cinfo->lck, cinfo->lck_div);
	DSSDBG("pck = %lu (%u)\n", cinfo->pck, cinfo->pck_div);


	dispc_set_lcd_divisor(channel, cinfo->lck_div,
							cinfo->pck_div);


	return 0;
}

int dispc_get_clock_div(struct dispc_clock_info *cinfo)
{
	unsigned long fck;

	fck = dispc_fclk_rate();

#ifdef CONFIG_ARCH_OMAP4
	cinfo->lck_div = REG_GET(DISPC_DIVISOR1, 23, 16);
	cinfo->pck_div = REG_GET(DISPC_DIVISOR1, 7, 0);
#else
	cinfo->lck_div = REG_GET(DISPC_DIVISOR, 23, 16);
	cinfo->pck_div = REG_GET(DISPC_DIVISOR, 7, 0);
#endif
	cinfo->lck = fck / cinfo->lck_div;
	cinfo->pck = cinfo->lck / cinfo->pck_div;

	return 0;
}

/* dispc.irq_lock has to be locked by the caller */
static void _omap_dispc_set_irqs(void)
{
	u32 mask;
	u32 old_mask;
	int i;
	struct omap_dispc_isr_data *isr_data;

	mask = dispc.irq_error_mask;

	for (i = 0; i < DISPC_MAX_NR_ISRS; i++) {
		isr_data = &dispc.registered_isr[i];

		if (isr_data->isr == NULL)
			continue;

		mask |= isr_data->mask;
	}

	enable_clocks(1);

	old_mask = dispc_read_reg(DISPC_IRQENABLE);
	/* clear the irqstatus for newly enabled irqs */
	dispc_write_reg(DISPC_IRQSTATUS, (mask ^ old_mask) & mask);

	dispc_write_reg(DISPC_IRQENABLE, mask);

	enable_clocks(0);
}

int omap_dispc_register_isr(omap_dispc_isr_t isr, void *arg, u32 mask)
{
	int i;
	int ret;
	unsigned long flags;
	struct omap_dispc_isr_data *isr_data;

	if (isr == NULL)
		return -EINVAL;

	spin_lock_irqsave(&dispc.irq_lock, flags);

	/* check for duplicate entry */
	for (i = 0; i < DISPC_MAX_NR_ISRS; i++) {
		isr_data = &dispc.registered_isr[i];
		if (isr_data->isr == isr && isr_data->arg == arg &&
				isr_data->mask == mask) {
			ret = -EINVAL;
			goto err;
		}
	}

	isr_data = NULL;
	ret = -EBUSY;

	for (i = 0; i < DISPC_MAX_NR_ISRS; i++) {
		isr_data = &dispc.registered_isr[i];

		if (isr_data->isr != NULL)
			continue;

		isr_data->isr = isr;
		isr_data->arg = arg;
		isr_data->mask = mask;
		ret = 0;

		break;
	}

	_omap_dispc_set_irqs();

	spin_unlock_irqrestore(&dispc.irq_lock, flags);

	return 0;
err:
	spin_unlock_irqrestore(&dispc.irq_lock, flags);

	return ret;
}
EXPORT_SYMBOL(omap_dispc_register_isr);

int omap_dispc_unregister_isr(omap_dispc_isr_t isr, void *arg, u32 mask)
{
	int i;
	unsigned long flags;
	int ret = -EINVAL;
	struct omap_dispc_isr_data *isr_data;

	spin_lock_irqsave(&dispc.irq_lock, flags);

	for (i = 0; i < DISPC_MAX_NR_ISRS; i++) {
		isr_data = &dispc.registered_isr[i];
		if (isr_data->isr != isr || isr_data->arg != arg ||
				isr_data->mask != mask)
			continue;

		/* found the correct isr */

		isr_data->isr = NULL;
		isr_data->arg = NULL;
		isr_data->mask = 0;

		ret = 0;
		break;
	}

	if (ret == 0)
		_omap_dispc_set_irqs();

	spin_unlock_irqrestore(&dispc.irq_lock, flags);

	return ret;
}
EXPORT_SYMBOL(omap_dispc_unregister_isr);

#ifdef DEBUG
static void print_irq_status(u32 status)
{
	if ((status & dispc.irq_error_mask) == 0)
		return;

	printk(KERN_DEBUG "DISPC IRQ: 0x%x: ", status);

#define PIS(x) \
	if (status & DISPC_IRQ_##x) \
		printk(#x " ");
	PIS(GFX_FIFO_UNDERFLOW);
	PIS(OCP_ERR);
	PIS(VID1_FIFO_UNDERFLOW);
	PIS(VID2_FIFO_UNDERFLOW);
	PIS(SYNC_LOST);
	PIS(SYNC_LOST_DIGIT);
#undef PIS

	printk("\n");
}
#endif

/* Called from dss.c. Note that we don't touch clocks here,
 * but we presume they are on because we got an IRQ. However,
 * an irq handler may turn the clocks off, so we may not have
 * clock later in the function. */
void dispc_irq_handler(void)
{
	int i;
	u32 irqstatus;
	u32 handledirqs = 0;
	u32 unhandled_errors;
	struct omap_dispc_isr_data *isr_data;
	struct omap_dispc_isr_data registered_isr[DISPC_MAX_NR_ISRS];

	spin_lock(&dispc.irq_lock);

	irqstatus = dispc_read_reg(DISPC_IRQSTATUS);

#ifdef DEBUG
	if (dss_debug)
		print_irq_status(irqstatus);
#endif
	/* Ack the interrupt. Do it here before clocks are possibly turned
	 * off */
	dispc_write_reg(DISPC_IRQSTATUS, irqstatus);
	/* flush posted write */
	dispc_read_reg(DISPC_IRQSTATUS);

	/* flushed posted write */
	dispc_read_reg(DISPC_IRQSTATUS);

	/* make a copy and unlock, so that isrs can unregister
	 * themselves */
	memcpy(registered_isr, dispc.registered_isr,
			sizeof(registered_isr));

	spin_unlock(&dispc.irq_lock);

	for (i = 0; i < DISPC_MAX_NR_ISRS; i++) {
		isr_data = &registered_isr[i];

		if (!isr_data->isr)
			continue;

		if (isr_data->mask & irqstatus) {
			isr_data->isr(isr_data->arg, irqstatus);
			handledirqs |= isr_data->mask;

			if (!cpu_is_omap44xx() &&
			    (isr_data->mask & irqstatus & DISPC_IRQ_VSYNC))
				if (dispc_go_busy(OMAP_DSS_CHANNEL_LCD))
					break;
			
		}
	}

	spin_lock(&dispc.irq_lock);

	unhandled_errors = irqstatus & ~handledirqs & dispc.irq_error_mask;

	if (unhandled_errors) {
		dispc.error_irqs |= unhandled_errors;

		dispc.irq_error_mask &= ~unhandled_errors;
		_omap_dispc_set_irqs();

		schedule_work(&dispc.error_work);
	}

	spin_unlock(&dispc.irq_lock);
}

static void dispc_error_worker(struct work_struct *work)
{
	int i;
	u32 errors;
	unsigned long flags;
#if 1//TI HS.Yoon 20100520 for recovering OCP_ERR
        u32 regval;
#endif



	spin_lock_irqsave(&dispc.irq_lock, flags);
	errors = dispc.error_irqs;
	dispc.error_irqs = 0;
	spin_unlock_irqrestore(&dispc.irq_lock, flags);

	if (errors & DISPC_IRQ_GFX_FIFO_UNDERFLOW) {
/*SV //HS mode just report the error dont close the pipeline */		
		DSSERR("GFX_FIFO_UNDERFLOW, but dont disable GFX\n");
#if 0
		for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
			struct omap_overlay *ovl;
			ovl = omap_dss_get_overlay(i);

			if (!(ovl->caps & OMAP_DSS_OVL_CAP_DISPC))
				continue;

			if (ovl->id == 0) {
				dispc_enable_plane(ovl->id, 0);
				dispc_go(ovl->manager->id);
				mdelay(50);
				break;
			}
		}
#endif
	}

	if (errors & DISPC_IRQ_VID1_FIFO_UNDERFLOW) {
		DSSERR("VID1_FIFO_UNDERFLOW, disabling VID1\n");
		for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
			struct omap_overlay *ovl;
			ovl = omap_dss_get_overlay(i);

			if (!(ovl->caps & OMAP_DSS_OVL_CAP_DISPC))
				continue;

			if (ovl->id == 1) {
				dispc_enable_plane(ovl->id, 0);
				dispc_go(ovl->manager->id);
				mdelay(50);
				break;
			}
		}
	}

	if (errors & DISPC_IRQ_VID2_FIFO_UNDERFLOW) {
		DSSERR("VID2_FIFO_UNDERFLOW, disabling VID2\n");
		for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
			struct omap_overlay *ovl;
			ovl = omap_dss_get_overlay(i);

			if (!(ovl->caps & OMAP_DSS_OVL_CAP_DISPC))
				continue;

			if (ovl->id == 2) {
				dispc_enable_plane(ovl->id, 0);
				dispc_go(ovl->manager->id);
				mdelay(50);
				break;
			}
		}
	}
#ifdef CONFIG_ARCH_OMAP4
	if (errors & DISPC_IRQ_VID3_FIFO_UNDERFLOW) {
		DSSERR("VID3_FIFO_UNDERFLOW, disabling VID2\n");
		for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
			struct omap_overlay *ovl;
			ovl = omap_dss_get_overlay(i);

			if (!(ovl->caps & OMAP_DSS_OVL_CAP_DISPC))
				continue;

			if (ovl->id == 3) {
				dispc_enable_plane(ovl->id, 0);
				dispc_go(ovl->manager->id);
				mdelay(50);
				break;
			}
		}
	}
#endif

#ifdef CONFIG_ARCH_OMAP4
	if (errors & DISPC_IRQ_SYNC_LOST_2) {
		struct omap_overlay_manager *manager = NULL;
		bool enable = false;

		DSSERR("SYNC_LOST for LCD2, disabling LCD2\n");

		for (i = 0; i < omap_dss_get_num_overlay_managers(); ++i) {
			struct omap_overlay_manager *mgr;
			mgr = omap_dss_get_overlay_manager(i);
			if (mgr == NULL)
				break;
			if (mgr->id == OMAP_DSS_CHANNEL_LCD2) {
				manager = mgr;
				enable = mgr->device->state ==
						OMAP_DSS_DISPLAY_ACTIVE;
				mgr->device->disable(mgr->device);
				break;
			}
		}

		if (manager) {
			for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
				struct omap_overlay *ovl;
				ovl = omap_dss_get_overlay(i);

				if (!(ovl->caps & OMAP_DSS_OVL_CAP_DISPC))
					continue;

				if (ovl->id != 0 && ovl->manager == manager)
					dispc_enable_plane(ovl->id, 0);
			}

			dispc_go(manager->id);
			mdelay(50);
#ifndef __WORKAROUND_DSS_ERR_ABORT
                        if (enable && manager->device != 0)
                                manager->device->enable(manager->device);
#else

			if (enable)
				manager->device->enable(manager->device);
#endif
		}
	}
#endif

	if (errors & DISPC_IRQ_SYNC_LOST) {
		struct omap_overlay_manager *manager = NULL;
		bool enable = false;

		DSSERR("SYNC_LOST, disabling LCD\n");

		for (i = 0; i < omap_dss_get_num_overlay_managers(); ++i) {
			struct omap_overlay_manager *mgr;
			mgr = omap_dss_get_overlay_manager(i);

			if (mgr == NULL)
				break;

			if (mgr->id == OMAP_DSS_CHANNEL_LCD) {
				manager = mgr;
				enable = mgr->device->state ==
						OMAP_DSS_DISPLAY_ACTIVE;
				mgr->device->disable(mgr->device);
				break;
			}
		}

		if (manager) {
			for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
				struct omap_overlay *ovl;
				ovl = omap_dss_get_overlay(i);

				if (!(ovl->caps & OMAP_DSS_OVL_CAP_DISPC))
					continue;

				if (ovl->id != 0 && ovl->manager == manager)
					dispc_enable_plane(ovl->id, 0);
			}

			dispc_go(manager->id);
			mdelay(50);
			if (enable)
				manager->device->enable(manager->device);
		}
	}

	if (errors & DISPC_IRQ_SYNC_LOST_DIGIT) {
#if 0
		struct omap_overlay_manager *manager = NULL;
		bool enable = false;
#endif

		DSSERR("SYNC_LOST_DIGIT\n");
	/*
		DSSERR("SYNC_LOST_DIGIT, disabling TV\n");

		for (i = 0; i < omap_dss_get_num_overlay_managers(); ++i) {
			struct omap_overlay_manager *mgr;
			mgr = omap_dss_get_overlay_manager(i);
			if (mgr == NULL)
				break;

			if (mgr->id == OMAP_DSS_CHANNEL_DIGIT) {
				manager = mgr;
				enable = mgr->device->state ==
						OMAP_DSS_DISPLAY_ACTIVE;
				mgr->device->disable(mgr->device);
				break;
			}
		}

		if (manager) {
			for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
				struct omap_overlay *ovl;
				ovl = omap_dss_get_overlay(i);

				if (!(ovl->caps & OMAP_DSS_OVL_CAP_DISPC))
					continue;

				if (ovl->id != 0 && ovl->manager == manager)
					dispc_enable_plane(ovl->id, 0);
			}

			dispc_go(manager->id);
			mdelay(50);
			if (enable)
				manager->device->enable(manager->device);
		}
		*/
	}

	if (errors & DISPC_IRQ_OCP_ERR) {
		DSSERR("OCP_ERR\n");
/*
#if 1//TI HS.Yoon 20100520 for recovering OCP_ERR
		regval = dispc_read_reg(DISPC_CONFIG);
		printk("[OCP_ERR] DISPC_CONFIG = %x\n", regval);
		if( (regval & 0x6) != 0x4 )  
		REG_FLD_MOD(DISPC_CONFIG, 0x2, 1, 2);


#else
*/


		for (i = 0; i < omap_dss_get_num_overlay_managers(); ++i) {
			struct omap_overlay_manager *mgr;
			mgr = omap_dss_get_overlay_manager(i);
			
			if (!mgr){
				pr_err("Error obtaining overlay manager!\n");
				BUG_ON(1);
			}
/*
#ifndef __WORKAROUND_DSS_ERR_ABORT
			if ( (mgr->caps & OMAP_DSS_OVL_CAP_DISPC) && mgr->device != 0 )
				mgr->device->disable(mgr->device);

#else
*/


			if (mgr->caps & OMAP_DSS_OVL_CAP_DISPC)
				mgr->device->disable(mgr->device);
		}
//#endif
	}
//#endif

	spin_lock_irqsave(&dispc.irq_lock, flags);
	dispc.irq_error_mask |= errors;
	_omap_dispc_set_irqs();
	spin_unlock_irqrestore(&dispc.irq_lock, flags);
}

int omap_dispc_wait_for_irq_timeout(u32 irqmask, unsigned long timeout)
{
	void dispc_irq_wait_handler(void *data, u32 mask)
	{
		complete((struct completion *)data);
	}

	int r;
	DECLARE_COMPLETION_ONSTACK(completion);

	r = omap_dispc_register_isr(dispc_irq_wait_handler, &completion,
			irqmask);

	if (r)
		return r;

	timeout = wait_for_completion_timeout(&completion, timeout);

	omap_dispc_unregister_isr(dispc_irq_wait_handler, &completion, irqmask);

	if (timeout == 0)
		return -ETIMEDOUT;

	if (timeout == -ERESTARTSYS)
		return -ERESTARTSYS;

	return 0;
}

int omap_dispc_wait_for_irq_interruptible_timeout(u32 irqmask,
		unsigned long timeout)
{
	void dispc_irq_wait_handler(void *data, u32 mask)
	{
		complete((struct completion *)data);
	}

	int r;
	DECLARE_COMPLETION_ONSTACK(completion);

	r = omap_dispc_register_isr(dispc_irq_wait_handler, &completion,
			irqmask);

	if (r)
		return r;

	timeout = wait_for_completion_interruptible_timeout(&completion,
			timeout);

	omap_dispc_unregister_isr(dispc_irq_wait_handler, &completion, irqmask);

	if (timeout == 0)
		return -ETIMEDOUT;

	if (timeout == -ERESTARTSYS)
		return -ERESTARTSYS;

	return 0;
}

#ifdef CONFIG_OMAP2_DSS_FAKE_VSYNC
void dispc_fake_vsync_irq(int disp_id)
{
	u32 irqstatus;
	int i;

	local_irq_disable();
	switch (disp_id) {
	case 0:
		irqstatus = DISPC_IRQ_VSYNC;
		break;
	case 1:
		irqstatus = DISPC_IRQ_VSYNC2;
		break;
	default:
		DSSERR("Invalid display id for fake vsync\n");
		local_irq_enable();
		return;
	}

	for (i = 0; i < DISPC_MAX_NR_ISRS; i++) {
		struct omap_dispc_isr_data *isr_data;
		isr_data = &dispc.registered_isr[i];

		if (!isr_data->isr)
			continue;

		if (isr_data->mask & irqstatus)
			isr_data->isr(isr_data->arg, irqstatus);
	}

	local_irq_enable();
}
#endif

static void _omap_dispc_initialize_irq(void)
{
	unsigned long flags;

	spin_lock_irqsave(&dispc.irq_lock, flags);

	memset(dispc.registered_isr, 0, sizeof(dispc.registered_isr));

	dispc.irq_error_mask = DISPC_IRQ_MASK_ERROR;

	/* there's SYNC_LOST_DIGIT waiting after enabling the DSS,
	 * so clear it */
	dispc_write_reg(DISPC_IRQSTATUS, dispc_read_reg(DISPC_IRQSTATUS));

	_omap_dispc_set_irqs();

	spin_unlock_irqrestore(&dispc.irq_lock, flags);
}

void dispc_enable_sidle(void)
{
	REG_FLD_MOD(DISPC_SYSCONFIG, 2, 4, 3);	/* SIDLEMODE: smart idle */
}

void dispc_disable_sidle(void)
{
	REG_FLD_MOD(DISPC_SYSCONFIG, 1, 4, 3);	/* SIDLEMODE: no idle */
}

static void _omap_dispc_initial_config(void)
{
	u32 l;

	l = dispc_read_reg(DISPC_SYSCONFIG);
	l = FLD_MOD(l, 2, 13, 12);	/* MIDLEMODE: smart standby */
	l = FLD_MOD(l, 2, 4, 3);	/* SIDLEMODE: smart idle */
	l = FLD_MOD(l, 1, 2, 2);	/* ENWAKEUP */
	l = FLD_MOD(l, 1, 0, 0);	/* AUTOIDLE */
	dispc_write_reg(DISPC_SYSCONFIG, l);

	if (!cpu_is_omap44xx()) {
		/* FUNCGATED: changed bitfield in OMAP4 */
		REG_FLD_MOD(DISPC_CONFIG, 1, 9, 9);
	}

	/* L3 firewall setting: enable access to OCM RAM */
	/* XXX this should be somewhere in plat-omap */
	if (cpu_is_omap24xx())
		__raw_writel(0x402000b0, OMAP2_L3_IO_ADDRESS(0x680050a0));

	_dispc_setup_color_conv_coef();

	dispc_set_loadmode(OMAP_DSS_LOAD_FRAME_ONLY);

	dispc_read_plane_fifo_sizes();
}

int dispc_init(void)
{
	u32 rev;

	spin_lock_init(&dispc.irq_lock);

	INIT_WORK(&dispc.error_work, dispc_error_worker);

	dispc_base = dispc.base = ioremap(DISPC_BASE, DISPC_SZ_REGS);
	if (!dispc.base) {
		DSSERR("can't ioremap DISPC\n");
		return -ENOMEM;
	}

	enable_clocks(1);

	_omap_dispc_initial_config();

	_omap_dispc_initialize_irq();

	dispc_save_context();

	rev = dispc_read_reg(DISPC_REVISION);
	printk(KERN_INFO "OMAP DISPC rev %d.%d\n",
	       FLD_GET(rev, 7, 4), FLD_GET(rev, 3, 0));
		dispc_write_reg(DISPC_LINE_NUMBER, 0x95);	//swin.kim add 
	
#ifndef CONFIG_FB_OMAP_BOOTLOADER_INIT


	enable_clocks(0);
#endif


	return 0;
}

void dispc_exit(void)
{
	iounmap(dispc.base);
}

int dispc_enable_plane(enum omap_plane plane, bool enable)
{
	DSSDBG("dispc_enable_plane %d, %d\n", plane, enable);

	if(plane == OMAP_DSS_VIDEO1) {
#if 0
	printk("Reducing the refresh rate\n");
//Switch to lower refresh rate
	timing_h = dispc_read_reg(DISPC_TIMING_H);
        timing_h = FLD_MOD(timing_h, (38-1), 7, 0);
	dispc_write_reg(DISPC_TIMING_H, timing_h);
	reduced_rate = 1;
#endif
	if(enable) {
		vid_in_use = 1;	
	}
	else {
		vid_in_use = 0;
	}

	}	









	enable_clocks(1);
	_dispc_enable_plane(plane, enable);
	enable_clocks(0);

	return 0;
}

int dispc_setup_plane(enum omap_plane plane,
		       u32 paddr, u16 screen_width,
		       u16 pos_x, u16 pos_y,
		       u16 width, u16 height,
		       u16 out_width, u16 out_height,
		       enum omap_color_mode color_mode,
		       bool ilace,
		       enum omap_dss_rotation_type rotation_type,
		       u8 rotation, bool mirror, u8 global_alpha,
		       enum omap_channel channel,
		       u8 pre_alpha_mult, u32 puv_addr,
		       int flicker_filter)
{
	int r = 0;

	DSSDBG("dispc_setup_plane %d, pa %x, sw %d, %d,%d, %dx%d -> "
	       "%dx%d, ilace %d, cmode %x, rot %d, mir %d "
	       "flicker_filter %d\n",
	       plane, paddr, screen_width, pos_x, pos_y,
	       width, height,
	       out_width, out_height,
	       ilace, color_mode,
	       rotation, mirror,
	       flicker_filter);

	enable_clocks(1);

	r = _dispc_setup_plane(plane,
			   paddr, screen_width,
			   pos_x, pos_y,
			   width, height,
			   out_width, out_height,
			   color_mode, ilace,
			   rotation_type,
			   rotation, mirror,
			   global_alpha, channel,
			   pre_alpha_mult, puv_addr,
			   flicker_filter);

	enable_clocks(0);

	return r;
}
void test_out(bool enable)
{
	REG_FLD_MOD(DISPC_CONTROL, enable ? 1 : 0, 0, 0);
	REG_FLD_MOD(DISPC_CONTROL, enable ? 1 : 0, 1, 1);
}


#ifdef VID_SYNC_LOST
void modify_lcd_timing(int enable)
{
	u32 regval;
	enable_clocks(1);
	if(enable) {
//Reduce the refresh rate
		printk("Reducing the refresh rate\n");
		regval = omap_readl(0x48050464);
	        regval &= ~0xFF;
		regval |= 0x25;		//HSW = 38
        	omap_writel(regval,0x48050464);
	}
	else {
		printk("Refresh rate restored\n");
		regval = omap_readl(0x48050464);
                regval &= ~0xFF;
                regval |= 0x02;         //HSW = 3
                omap_writel(regval,0x48050464);
	}
	dispc_go(0);
	enable_clocks(0);
		
}
EXPORT_SYMBOL(modify_lcd_timing);
#endif	

#ifdef LCD_REFRESH_CHANGE
void reduce_refresh_rate(int enable)
{
        u32 regval;
        enable_clocks(1);
        if(enable) {
//Reduce the refresh rate
//                printk("Reducing the refresh rate\n");
                regval = omap_readl(0x48050464);
                regval &= ~0xFF;
                regval |= 0x3F;         //HSW = 64
                omap_writel(regval,0x48050464);
        }
        else {
//Increase the Refresh Rate
//                printk("Refresh rate restored\n");
                regval = omap_readl(0x48050464);
                regval &= ~0xFF;
                regval |= 0x02;         //HSW = 3
                omap_writel(regval,0x48050464);
        }
        dispc_go(0);
        enable_clocks(0);
	return;

}
EXPORT_SYMBOL(reduce_refresh_rate);
#endif



