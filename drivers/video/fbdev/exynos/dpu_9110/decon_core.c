/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/console.h>
#include <linux/dma-buf.h>
#if defined(CONFIG_ION_EXYNOS)
#include <linux/exynos_ion.h>
#include <linux/ion.h>
#include <linux/exynos_iovmm.h>
#endif
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/bug.h>
#include <linux/of_address.h>
#include <linux/debugfs.h>
#include <linux/pinctrl/consumer.h>
#include <video/mipi_display.h>
#include <media/v4l2-subdev.h>
#ifdef CONFIG_EXYNOS9110_BTS
#include <soc/samsung/cal-if.h>
#endif
#include <dt-bindings/clock/exynos9110.h>

#include "decon.h"
#include "dsim.h"
#include "./panels/lcd_ctrl.h"
#include "../../../../dma-buf/sync_debug.h"
#include "dpp.h"
#ifdef CONFIG_DRM_DMA_SYNC
#include <linux/fdtable.h>
#include "tdm.h"
#endif
#include <linux/gpio.h>
#include <linux/power/disp_stat.h>
#ifdef DECON_TIMEOUT_RECOVERY
#include <linux/reboot.h>
#endif
int decon_log_level = 6;

#if defined(CONFIG_FRAME_DROP_DETECTOR)
unsigned long long frames_count = 0;
extern unsigned long long vsync_signal;
#endif

module_param(decon_log_level, int, 0644);
int dpu_bts_log_level = 6;
module_param(dpu_bts_log_level, int, 0644);
int win_update_log_level = 6;
module_param(win_update_log_level, int, 0644);
int decon_systrace_enable;

struct decon_device *decon_drvdata[MAX_DECON_CNT];
EXPORT_SYMBOL(decon_drvdata);

#if defined(CONFIG_EXYNOS_ITMON)
void __iomem *regs_dphy_iso;
void __iomem *regs_dphy_clk_0;
void __iomem *regs_dphy_clk_1;
void __iomem *regs_dphy_clk_2;
#endif

void tracing_mark_write(struct decon_device *decon, char id, char *str1, int value)
{
	char buf[DECON_TRACE_BUF_SIZE] = {0,};

	if (!decon->systrace.pid)
		return;

	switch (id) {
	case 'B': /* B : Begin */
		snprintf(buf, DECON_TRACE_BUF_SIZE, "B|%d|%s",
				decon->systrace.pid, str1);
		break;
	case 'E': /* E : End */
		strcpy(buf, "E");
		break;
	case 'C': /* C : Category */
		snprintf(buf, DECON_TRACE_BUF_SIZE,
				"C|%d|%s|%d", decon->systrace.pid, str1, value);
		break;
	default:
		decon_err("%s:argument fail\n", __func__);
		return;
	}
	trace_puts(buf);

}

static void decon_dump_using_dpp(struct decon_device *decon)
{
	int i;

	for (i = 0; i < MAX_DPP_SUBDEV; i++) {
		if (test_bit(i, &decon->prev_used_dpp)) {
			struct v4l2_subdev *sd = NULL;
			sd = decon->dpp_sd[i];
			BUG_ON(!sd);
			v4l2_subdev_call(sd, core, ioctl, DPP_DUMP, NULL);
		}
	}
}

static void decon_up_list_saved(void)
{
	int i;
	struct decon_device *decon;

	for (i = 0; i < MAX_DECON_CNT; i++) {
		decon = get_decon_drvdata(i);
		if (decon) {
			if (!list_empty(&decon->up.list) || !list_empty(&decon->up.saved_list)) {
				decon->up_list_saved = true;
#ifdef CONFIG_SYNC
				decon_info("\n=== DECON%d TIMELINE %d MAX %d ===\n",
						decon->id, decon->timeline->value,
						decon->timeline_max);
#endif
			} else {
				decon->up_list_saved = false;
			}
		}
	}
}

void decon_dump(struct decon_device *decon)
{
	int acquired = console_trylock();

	if (decon->state != DECON_STATE_ON) {
		decon_info("%s: DECON%d is disabled, state(%d)\n",
				__func__, decon->id, decon->state);
		/* This return block initial_display's decon_dump */
		return;
	}

	decon_info("\n=== DECON%d SFR DUMP ===\n", decon->id);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->res.regs, 0x620, false);

	decon_info("\n=== DECON%d SHADOW SFR DUMP ===\n", decon->id);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->res.regs + SHADOW_OFFSET, 0x304, false);

	decon_info("\n=== DECON0 WINDOW SFR DUMP ===\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->res.regs + 0x1000, 0x340, false);

	decon_info("\n=== DECON0 WINDOW SHADOW SFR DUMP ===\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->res.regs + SHADOW_OFFSET + 0x1000, 0x220, false);

	if (decon->dt.out_type == DECON_OUT_DSI)
		v4l2_subdev_call(decon->out_sd[0], core, ioctl,
				DSIM_IOC_DUMP, NULL);
	decon_dump_using_dpp(decon);

	if (acquired)
		console_unlock();
}


#if defined(CONFIG_DRM_DMA_SYNC)
static void decon_wait_for_framedone(struct decon_device *decon)
{
	int ret = 0;
	s64 time_ms = ktime_to_ms(ktime_get()) - ktime_to_ms(decon->trig_mask_timestamp);

	if (time_ms < MAX_FRM_DONE_WAIT) {
		DPU_EVENT_LOG(DPU_EVT_DECON_FRAMEDONE_WAIT, &decon->sd, ktime_set(0, 0));
		ret = wait_event_interruptible_timeout(decon->wait_frmdone,
			(decon->frame_done_cnt_target <= decon->frame_done_cnt_cur),
		msecs_to_jiffies(MAX_FRM_DONE_WAIT - time_ms));
	}

	if (decon->dbg_cnt)
		decon_info("%s:ms[%lld]ret[%d]c[%d]\n", "wait_for_frame", time_ms,
			ret, decon->dbg_cnt);
}
#endif


/* ---------- CHECK FUNCTIONS ----------- */
static void decon_win_config_to_regs_param
	(int transp_length, struct decon_win_config *win_config,
	 struct decon_window_regs *win_regs, enum decon_idma_type idma_type,
	 int idx)
{
	u8 alpha0 = 0, alpha1 = 0;

	win_regs->wincon = wincon(transp_length, alpha0, alpha1,
			win_config->plane_alpha, win_config->blending, idx);
	win_regs->start_pos = win_start_pos(win_config->dst.x, win_config->dst.y);
	win_regs->end_pos = win_end_pos(win_config->dst.x, win_config->dst.y,
			win_config->dst.w, win_config->dst.h);
	win_regs->pixel_count = (win_config->dst.w * win_config->dst.h);
	win_regs->whole_w = win_config->dst.f_w;
	win_regs->whole_h = win_config->dst.f_h;
	win_regs->offset_x = win_config->dst.x;
	win_regs->offset_y = win_config->dst.y;
	win_regs->type = idma_type;
	win_regs->plane_alpha = win_config->plane_alpha;
	win_regs->format = win_config->format;
	win_regs->blend = win_config->blending;

	decon_dbg("DMATYPE_%d@ SRC:(%d,%d) %dx%d  DST:(%d,%d) %dx%d\n",
			idma_type,
			win_config->src.x, win_config->src.y,
			win_config->src.f_w, win_config->src.f_h,
			win_config->dst.x, win_config->dst.y,
			win_config->dst.w, win_config->dst.h);
}

u32 wincon(u32 transp_len, u32 a0, u32 a1,
	int plane_alpha, enum decon_blending blending, int idx)
{
	u32 data = 0;

	data |= WIN_EN_F(idx);

	return data;
}

bool decon_validate_x_alignment(struct decon_device *decon, int x, u32 w,
		u32 bits_per_pixel)
{
	uint8_t pixel_alignment = 32 / bits_per_pixel;

	if (x % pixel_alignment) {
		decon_err("left x not aligned to %u-pixel(bpp = %u, x = %u)\n",
				pixel_alignment, bits_per_pixel, x);
		return 0;
	}
	if ((x + w) % pixel_alignment) {
		decon_err("right X not aligned to %u-pixel(bpp = %u, x = %u, w = %u)\n",
				pixel_alignment, bits_per_pixel, x, w);
		return 0;
	}

	return 1;
}

void decon_dpp_stop(struct decon_device *decon, bool do_reset)
{
	int i;
	bool rst = false;
	struct v4l2_subdev *sd;

	for (i = 0; i < MAX_DPP_SUBDEV; i++) {
		if (test_bit(i, &decon->prev_used_dpp) &&
				!test_bit(i, &decon->cur_using_dpp)) {
			sd = decon->dpp_sd[i];
			BUG_ON(!sd);
			if (test_bit(i, &decon->dpp_err_stat) || do_reset)
				rst = true;

			v4l2_subdev_call(sd, core, ioctl, DPP_STOP, (bool *)rst);

			clear_bit(i, &decon->prev_used_dpp);
			clear_bit(i, &decon->dpp_err_stat);
		}
	}
}

static void decon_free_dma_buf(struct decon_device *decon,
		struct decon_dma_buf_data *dma)
{
#if defined(CONFIG_ION_EXYNOS)
	if (!dma->dma_addr)
		return;
#ifdef CONFIG_SYNC
	if (dma->fence)
		fput(dma->fence->file);
#endif
	ion_iovmm_unmap(dma->attachment, dma->dma_addr);

	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);

	dma_buf_detach(dma->dma_buf, dma->attachment);
	dma_buf_put(dma->dma_buf);
	ion_free(decon->ion_client, dma->ion_handle);
	memset(dma, 0, sizeof(struct decon_dma_buf_data));
#endif
}

static void decon_set_black_window(struct decon_device *decon)
{
	struct decon_window_regs win_regs;
	struct decon_lcd *lcd = decon->lcd_info;

	memset(&win_regs, 0, sizeof(struct decon_window_regs));
	win_regs.wincon = wincon(0x8, 0xFF, 0xFF, 0xFF, DECON_BLENDING_NONE,
			decon->dt.dft_win);
	win_regs.start_pos = win_start_pos(0, 0);
	win_regs.end_pos = win_end_pos(0, 0, lcd->xres, lcd->yres);
	decon_info("xres %d yres %d win_start_pos %x win_end_pos %x\n",
			lcd->xres, lcd->yres, win_regs.start_pos,
			win_regs.end_pos);
	win_regs.colormap = 0x000000;
	win_regs.pixel_count = lcd->xres * lcd->yres;
	win_regs.whole_w = lcd->xres;
	win_regs.whole_h = lcd->yres;
	win_regs.offset_x = 0;
	win_regs.offset_y = 0;
	decon_info("pixel_count(%d), whole_w(%d), whole_h(%d), x(%d), y(%d)\n",
			win_regs.pixel_count, win_regs.whole_w,
			win_regs.whole_h, win_regs.offset_x,
			win_regs.offset_y);
	decon_reg_set_window_control(decon->id, decon->dt.dft_win,
			&win_regs, true);
	decon_reg_update_req_window(decon->id, decon->dt.dft_win);
}

int decon_tui_protection(bool tui_en)
{
	int ret = 0;
	int win_idx;
	struct decon_mode_info psr;
	struct decon_device *decon = decon_drvdata[0];
	unsigned long aclk_khz = 0;

	decon_info("%s:state %d: out_type %d:+\n", __func__,
				tui_en, decon->dt.out_type);
	if (tui_en) {
		mutex_lock(&decon->lock);
		decon_hiber_block_exit(decon);

		kthread_flush_worker(&decon->up.worker);

		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
#ifdef CONFIG_FB_WINDOW_UPDATE
		if (decon->win_up.enabled)
			dpu_set_win_update_config(decon, NULL);
#endif
		decon_to_psr_info(decon, &psr);
#if defined(CONFIG_SOC_EXYNOS9110)
		decon_reg_stop_tui(decon->id, decon->dt.out_idx[0], &psr);
#else
		decon_reg_stop_nreset(decon->id, &psr);
#endif

		decon->cur_using_dpp = 0;
		decon_dpp_stop(decon, false);

		/* after stopping decon, we can now update registers
		 * without considering per frame condition (8895) */
		for (win_idx = 0; win_idx < decon->dt.max_win; win_idx++)
			decon_reg_set_win_enable(decon->id, win_idx, false);
		decon_reg_all_win_shadow_update_req(decon->id);
		decon_reg_update_req_global(decon->id);
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);

		decon->state = DECON_STATE_TUI;
		aclk_khz = v4l2_subdev_call(decon->out_sd[0], core, ioctl,
				EXYNOS_DPU_GET_ACLK, NULL) / 1000U;
		decon_info("%s:DPU_ACLK(%ld khz)\n", __func__, aclk_khz);
#if defined(CONFIG_EXYNOS9110_BTS)
		decon_info("MIF(%lu), INT(%lu), DISP(%lu), total bw(%u, %u)\n",
				cal_dfs_get_rate(ACPM_DVFS_MIF),
				cal_dfs_get_rate(ACPM_DVFS_INT),
				cal_dfs_get_rate(ACPM_DVFS_DISP),
				decon->bts.prev_total_bw,
				decon->bts.total_bw);
#endif
		mutex_unlock(&decon->lock);
	} else {
		mutex_lock(&decon->lock);
		aclk_khz = v4l2_subdev_call(decon->out_sd[0], core, ioctl,
				EXYNOS_DPU_GET_ACLK, NULL) / 1000U;
		decon_info("%s:DPU_ACLK(%ld khz)\n", __func__, aclk_khz);
#if defined(CONFIG_EXYNOS9110_BTS)
		decon_info("MIF(%lu), INT(%lu), DISP(%lu), total bw(%u, %u)\n",
				cal_dfs_get_rate(ACPM_DVFS_MIF),
				cal_dfs_get_rate(ACPM_DVFS_INT),
				cal_dfs_get_rate(ACPM_DVFS_DISP),
				decon->bts.prev_total_bw,
				decon->bts.total_bw);
#endif
		decon->state = DECON_STATE_ON;
		decon_hiber_unblock(decon);
		mutex_unlock(&decon->lock);
	}
	decon_info("%s:state %d: out_type %d:-\n", __func__,
				tui_en, decon->dt.out_type);
	return ret;
}

/* ---------- FB_BLANK INTERFACE ----------- */
static int decon_enable(struct decon_device *decon)
{
	struct decon_mode_info psr;
	struct decon_param p;
	struct dsim_device *dsim;
	int ret = 0;

	decon_dbg("enable decon-%d\n", decon->id);

	mutex_lock(&decon->lock);

	if (!decon->id && (decon->dt.out_type == DECON_OUT_DSI) &&
				(decon->state == DECON_STATE_INIT)) {
		decon_info("decon%d init state\n", decon->id);
		decon->state = DECON_STATE_ON;
		goto err;
	}

	if (decon->state == DECON_STATE_ON) {
		decon_warn("decon%d already enabled\n", decon->id);
		goto err;
	}

	pm_stay_awake(decon->dev);
	dev_warn(decon->dev, "pm_stay_awake");

#ifdef CONFIG_EXYNOS9110_BTS
	decon->bts.ops->bts_acquire_bw(decon);
#endif

	if (decon->dt.psr_mode != DECON_VIDEO_MODE) {
		if (decon->res.pinctrl && decon->res.hw_te_on) {
			if (pinctrl_select_state(decon->res.pinctrl,
						decon->res.hw_te_on)) {
				decon_err("failed to turn on Decon_TE\n");
			}
		}
	}

	ret = v4l2_subdev_call(decon->out_sd[0], video, s_stream, 1);
	if (ret) {
		decon_err("starting stream failed for %s\n",
				decon->out_sd[0]->name);
	}

	decon_to_init_param(decon, &p);
	decon_reg_init(decon->id, decon->dt.out_idx[0], &p);

	decon_to_psr_info(decon, &psr);

	if (decon->dt.out_type == DECON_OUT_DSI) {
		if (psr.trig_mode == DECON_HW_TRIG) {
			decon_set_black_window(decon);
			/*
			 * Blender configuration must be set before DECON start.
			 * If DECON goes to start without window and
			 * blender configuration,
			 * DECON will go into abnormal state.
			 * DECON2(for DISPLAYPORT) start in winconfig
			 */
			decon_reg_start(decon->id, &psr);
			decon_reg_update_req_and_unmask(decon->id, &psr);
		}
		dsim = container_of(decon->out_sd[0], struct dsim_device, sd);
		call_panel_ops(dsim, displayon, dsim);
	}

	/*
	 * After turned on LCD, previous update region must be set as FULL size.
	 * DECON, DSIM and Panel are initialized as FULL size during UNBLANK
	 */
	DPU_FULL_RECT(&decon->win_up.prev_up_region, decon->lcd_info);

	if (!decon->id && !decon->eint_status) {
		enable_irq(decon->res.irq);
		decon->eint_status = 1;
	}

	decon->state = DECON_STATE_ON;
	decon_reg_set_int(decon->id, &psr, 1);

err:
#ifdef DECON_TIMEOUT_RECOVERY
	decon->recovery_working = 0;
#endif
	mutex_unlock(&decon->lock);
	return ret;
}

#if 0
int cmu_dpu_dump(void)
{
	void __iomem	*cmu_regs;
	void __iomem	*pmu_regs;

	decon_info("\n=== CMU_DPU0 SFR DUMP 0x12800100 ===\n");
	cmu_regs = ioremap(0x12800100, 0x10);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x0C, false);

	decon_info("\n=== CMU_DPU0 SFR DUMP 0x12800800 ===\n");
	cmu_regs = ioremap(0x12800800, 0x08);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x04, false);

	cmu_regs = ioremap(0x12800810, 0x10);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x08, false);

	decon_info("\n=== CMUdd_DPU0 SFR DUMP 0x12801800 ===\n");
	cmu_regs = ioremap(0x12801808, 0x08);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x04, false);

	decon_info("\n=== CMU_DPU0 SFR DUMP 0x12802000 ===\n");
	cmu_regs = ioremap(0x12802000, 0x74);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x70, false);

	cmu_regs = ioremap(0x1280207c, 0x100);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x94, false);

	decon_info("\n=== CMU_DPU0 SFR DUMP 0x12803000 ===\n");
	cmu_regs = ioremap(0x12803004, 0x10);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x0c, false);

	cmu_regs = ioremap(0x12803014, 0x2C);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x28, false);

	cmu_regs = ioremap(0x1280304c, 0x20);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x18, false);

	decon_info("\n=== PMU_DPU0 SFR DUMP 0x16484064 ===\n");
	pmu_regs = ioremap(0x16484064, 0x08);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			pmu_regs, 0x04, false);

	decon_info("\n=== PMU_DPU1 SFR DUMP 0x16484084 ===\n");
	pmu_regs = ioremap(0x16484084, 0x08);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			pmu_regs, 0x04, false);

	return 0;
}
#endif

#if defined(CONFIG_EXYNOS_ITMON)
static int out_sd_ioremap(void) {
	regs_dphy_iso = ioremap(0x11860730, 0x10);		/* PHY ISO reg in PMU */
	regs_dphy_clk_0 = ioremap(0x14980800, 0x10);	/* CMU_DISPAUD base + 0x0800 */
	regs_dphy_clk_1 = ioremap(0x14983030, 0x10);	/* CMU_DISPAUD base + 0x3030 */

	return 0;
}

static int out_sd_dump(void)
{
	decon_info("\n=== DEBUG register dump : DPHY iso ===\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			regs_dphy_iso, 0x04, false);

	decon_info("\n=== DEBUG register dump : DPHY clock ===\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			regs_dphy_clk_0, 0x04, false);

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			regs_dphy_clk_1, 0x04, false);

	return 0;
}
#endif

static int decon_disable(struct decon_device *decon)
{
	struct decon_mode_info psr;
	int ret = 0;

	if (decon->state == DECON_STATE_TUI) {
		decon_tui_protection(false);
	}

	mutex_lock(&decon->lock);

	if (decon->state == DECON_STATE_OFF) {
		decon_info("decon%d already disabled\n", decon->id);
		goto err;
	}

	kthread_flush_worker(&decon->up.worker);

	decon_to_psr_info(decon, &psr);
	decon_reg_set_int(decon->id, &psr, 0);

	if (!decon->id && (decon->vsync.irq_refcount <= 0) &&
			decon->eint_status) {
		disable_irq(decon->res.irq);
		decon->eint_status = 0;
	}

	ret = decon_reg_stop(decon->id, decon->dt.out_idx[0], &psr);
	if (ret < 0)
		decon_dump(decon);

	decon_reg_clear_int_all(decon->id);

	/* DMA protection disable must be happen on dpp domain is alive */
#if defined(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	decon_set_protected_content(decon, NULL);
#endif
	decon->cur_using_dpp = 0;
	decon_dpp_stop(decon, false);

#ifdef CONFIG_EXYNOS9110_BTS
	decon->bts.ops->bts_release_bw(decon);
#endif

	ret = v4l2_subdev_call(decon->out_sd[0], video, s_stream, 0);
	if (ret) {
		decon_err("failed to stop %s\n", decon->out_sd[0]->name);
	}

	pm_relax(decon->dev);
	dev_warn(decon->dev, "pm_relax");

	if (decon->dt.psr_mode != DECON_VIDEO_MODE) {
		if (decon->res.pinctrl && decon->res.hw_te_off) {
			if (pinctrl_select_state(decon->res.pinctrl,
						decon->res.hw_te_off)) {
				decon_err("failed to turn off Decon_TE\n");
			}
		}
	}

	decon->state = DECON_STATE_OFF;

err:
	mutex_unlock(&decon->lock);
	return ret;
}

static int decon_blank(int blank_mode, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	int ret = 0;

	decon_info("decon-%d %s mode: %dtype (0: DSI, 1: eDP, 2:DP, 3: WB)\n",
			decon->id,
			blank_mode == FB_BLANK_UNBLANK ? "UNBLANK" : (blank_mode == FB_BLANK_NORMAL ? "NORMAL" : "POWERDOWN"),
			decon->dt.out_type);
#if 0
	decon_info("%s[%d]state[%d]aod[%d]pm[%d %d %d]r[%d %d]\n", __func__,
		blank_mode, decon->state, decon->aod_state, pm_runtime_active(decon->dev),
		decon->dev->power.runtime_status, decon->dev->power.disable_depth,
		atomic_read(&decon->lpd_block_cnt), atomic_read(&decon->lpd_trig_cnt));
#endif

	decon_hiber_block_exit(decon);

	if (decon->aod_state != AOD_OFF) {
		decon_err("%s:invalid aod_state[%d]\n", __func__, decon->aod_state);
		ret = -EINVAL;
		goto bypass;
	}

	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
		DPU_EVENT_LOG(DPU_EVT_BLANK, &decon->sd, ktime_set(0, 0));
		decon->dpms = blank_mode;
		ret = decon_disable(decon);
		if (ret) {
			decon_err("failed to disable decon\n");
			goto blank_exit;
		}
		break;
	case FB_BLANK_NORMAL:
		ret = decon_enable(decon);
		if (ret) {
			decon_err("failed to enable decon\n");
			goto blank_exit;
		}
		decon->dpms = blank_mode;
		break;
	case FB_BLANK_UNBLANK:
		DPU_EVENT_LOG(DPU_EVT_UNBLANK, &decon->sd, ktime_set(0, 0));
		ret = decon_enable(decon);
		if (ret) {
			decon_err("failed to enable decon\n");
			goto blank_exit;
		}
		decon->dpms = blank_mode;
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	default:
		ret = -EINVAL;
	}

blank_exit:
	decon_hiber_unblock(decon);
	decon_info("%s -\n", __func__);
bypass:
#if 0
	decon_info("%s:dpms[%d]state[%d]aod[%d]pm[%d %d %d]r[%d %d]ret[%d]\n", __func__,
		decon->dpms, decon->state, decon->aod_state, pm_runtime_active(decon->dev),
		decon->dev->power.runtime_status, decon->dev->power.disable_depth,
		atomic_read(&decon->lpd_block_cnt), atomic_read(&decon->lpd_trig_cnt), ret);
#endif
	return ret;
}

/* ---------- FB_IOCTL INTERFACE ----------- */
static void decon_activate_vsync(struct decon_device *decon)
{
	int prev_refcount;

	mutex_lock(&decon->vsync.lock);

	prev_refcount = decon->vsync.irq_refcount++;
	if (!prev_refcount)
		DPU_EVENT_LOG(DPU_EVT_ACT_VSYNC, &decon->sd, ktime_set(0, 0));

	mutex_unlock(&decon->vsync.lock);
}

static void decon_deactivate_vsync(struct decon_device *decon)
{
	int new_refcount;

	mutex_lock(&decon->vsync.lock);

	new_refcount = --decon->vsync.irq_refcount;
	WARN_ON(new_refcount < 0);
	if (!new_refcount)
		DPU_EVENT_LOG(DPU_EVT_DEACT_VSYNC, &decon->sd, ktime_set(0, 0));

	mutex_unlock(&decon->vsync.lock);
}

#ifdef DECON_TIMEOUT_RECOVERY
extern void (*arm_pm_restart)(enum reboot_mode reboot_mode, const char *cmd);
#define	MAX_TIMEOUT_CNT	0
#define	MAX_RECOVERY_CNT	3
#define	MAX_TIMEOUT_PRD	VSYNC_TIMEOUT_MSEC * 10

extern void gpio_keys_send_fake_powerkey(int value);
void decon_vsync_timeout_recovery(struct decon_device *decon)
{
	int ret = 0;
	struct dsim_device *dsim;

	decon_info("%s[%d]\n", __func__, decon->recovery_cnt);

	if (decon->recovery_cnt >= MAX_RECOVERY_CNT) {
		decon_info("%s:device reset\n", __func__);
		arm_pm_restart(0, 0);
	}

	ret = v4l2_subdev_call(decon->out_sd[0], video, s_stream, false);
	if (ret) {
		decon_err("stopping stream failed for %s\n",
			decon->out_sd[0]->name);
	}

	msleep(100);

	ret = v4l2_subdev_call(decon->out_sd[0], video, s_stream, true);
	if (ret) {
		decon_err("stopping stream failed for %s\n",
			decon->out_sd[0]->name);
	}

	dsim = container_of(decon->out_sd[0], struct dsim_device, sd);
	dsim->decon_timeout_cnt++;
	call_panel_ops(dsim, displayon, dsim);

	gpio_keys_send_fake_powerkey(true);
	msleep(20);
	gpio_keys_send_fake_powerkey(false);
	decon->recovery_working = 0;
	decon->recovery_cnt++;
}

static void decon_check_max_vsync_timeout(struct decon_device *decon)
{
	s64 now_t = ktime_to_ms(ktime_get());

	if ((decon->last_timeout == 0) ||
	     ((now_t -decon->last_timeout) > MAX_TIMEOUT_PRD)) {
		decon->timeout_cnt = 1;
		decon_info("%s: reset timeout_cnt\n", __func__);
	} else {
		decon->timeout_cnt++;
		decon_info("%s: timeout_cnt[%d]\n", __func__, decon->timeout_cnt);
	}

	decon->last_timeout = now_t;

	if (decon->timeout_cnt > MAX_TIMEOUT_CNT && decon->recovery_working == 0) {
		decon->timeout_cnt = 0;
		decon->recovery_working = 1;
		decon_vsync_timeout_recovery(decon);
	} else
		decon_info("%s: recovery_working[%d], timeout_cnt[%d]\n",
			__func__, decon->recovery_working, decon->timeout_cnt);

	return;
}
#endif

int decon_wait_for_vsync(struct decon_device *decon, u32 timeout)
{
	ktime_t timestamp;
	struct decon_mode_info psr;
	int ret;

	decon_to_psr_info(decon, &psr);

	if (psr.trig_mode != DECON_HW_TRIG)
		return 0;

	timestamp = decon->vsync.timestamp;
	decon_activate_vsync(decon);

	if (timeout) {
		ret = wait_event_interruptible_timeout(decon->vsync.wait,
				!ktime_equal(timestamp,
						decon->vsync.timestamp),
				msecs_to_jiffies(timeout));
	} else {
		ret = wait_event_interruptible(decon->vsync.wait,
				!ktime_equal(timestamp,
						decon->vsync.timestamp));
	}

	decon_deactivate_vsync(decon);

	if (timeout && ret == 0) {
		if (decon->d.eint_pend) {
			decon_err("decon%d wait for vsync timeout(p:0x%x)\n",
				decon->id, readl(decon->d.eint_pend));
		} else {
			decon_err("decon%d wait for vsync timeout\n", decon->id);
		}
#ifdef DECON_TIMEOUT_RECOVERY
		decon_check_max_vsync_timeout(decon);
#endif
		return -ETIMEDOUT;
	}

	return 0;
}

static int decon_find_biggest_block_rect(struct decon_device *decon,
		int win_no, struct decon_win_config *win_config,
		struct decon_rect *block_rect, bool *enabled)
{
	struct decon_rect r1, r2, overlap_rect;
	unsigned int overlap_size = 0, blocking_size = 0;
	struct decon_win_config *config;
	int j;

	/* Get the rect in which we try to get the block region */
	config = &win_config[win_no];
	r1.left = config->dst.x;
	r1.top = config->dst.y;
	r1.right = r1.left + config->dst.w - 1;
	r1.bottom = r1.top + config->dst.h - 1;

	/* Find the biggest block region from overlays by the top windows */
	for (j = win_no + 1; j < MAX_DECON_WIN; j++) {
		config = &win_config[j];
		if (config->state != DECON_WIN_STATE_BUFFER)
			continue;

		/* If top window has plane alpha, blocking mode not appliable */
		if ((config->plane_alpha < 255) && (config->plane_alpha > 0))
			continue;

		if (is_decon_opaque_format(config->format)) {
			config->opaque_area.x = config->dst.x;
			config->opaque_area.y = config->dst.y;
			config->opaque_area.w = config->dst.w;
			config->opaque_area.h = config->dst.h;
		} else
			continue;

		r2.left = config->opaque_area.x;
		r2.top = config->opaque_area.y;
		r2.right = r2.left + config->opaque_area.w - 1;
		r2.bottom = r2.top + config->opaque_area.h - 1;
		/* overlaps or not */
		if (decon_intersect(&r1, &r2)) {
			decon_intersection(&r1, &r2, &overlap_rect);
			if (!is_decon_rect_differ(&r1, &overlap_rect)) {
				/* if overlaping area intersects the window
				 * completely then disable the window */
				win_config[win_no].state = DECON_WIN_STATE_DISABLED;
				return 1;
			}

			if (overlap_rect.right - overlap_rect.left + 1 <
					MIN_BLK_MODE_WIDTH ||
				overlap_rect.bottom - overlap_rect.top + 1 <
					MIN_BLK_MODE_HEIGHT)
				continue;

			overlap_size = (overlap_rect.right - overlap_rect.left) *
					(overlap_rect.bottom - overlap_rect.top);

			if (overlap_size > blocking_size) {
				memcpy(block_rect, &overlap_rect,
						sizeof(struct decon_rect));
				blocking_size =
					(block_rect->right - block_rect->left) *
					(block_rect->bottom - block_rect->top);
				*enabled = true;
			}
		}
	}

	return 0;
}

static int decon_set_win_blocking_mode(struct decon_device *decon,
		int win_no, struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
	struct decon_rect block_rect;
	bool enabled;
	int ret = 0;
	struct decon_win_config *config = &win_config[win_no];

	enabled = false;

	if (!IS_ENABLED(CONFIG_DECON_BLOCKING_MODE))
		return ret;

	if (config->state != DECON_WIN_STATE_BUFFER)
		return ret;

	if (config->compression)
		return ret;

	/* Blocking mode is supported only for RGB32 color formats */
	if (!is_rgb32(config->format))
		return ret;

	/* Blocking Mode is not supported if there is a rotation */
	if (config->dpp_parm.rot || is_scaling(config))
		return ret;

	/* Initialization */
	memset(&block_rect, 0, sizeof(struct decon_rect));

	/* Find the biggest block region from possible block regions
	 * 	Possible block regions
	 * 	- overlays by top windows
	 *
	 * returns :
	 * 	1  - corresponding window is blocked whole way,
	 * 	     meaning that the window could be disabled
	 *
	 * 	0, enabled = true  - blocking area has been found
	 * 	0, enabled = false - blocking area has not been found
	 */
	ret = decon_find_biggest_block_rect(decon, win_no, win_config,
						&block_rect, &enabled);
	if (ret)
		return ret;

	/* If there was a block region, set regs with results */
	if (enabled) {
		regs->block_rect[win_no].w = block_rect.right - block_rect.left + 1;
		regs->block_rect[win_no].h = block_rect.bottom - block_rect.top + 1;
		regs->block_rect[win_no].x = block_rect.left - config->dst.x;
		regs->block_rect[win_no].y = block_rect.top -  config->dst.y;
		decon_dbg("win-%d: block_rect[%d %d %d %d]\n", win_no,
			regs->block_rect[win_no].x, regs->block_rect[win_no].y,
			regs->block_rect[win_no].w, regs->block_rect[win_no].h);
		memcpy(&config->block_area, &regs->block_rect[win_no],
				sizeof(struct decon_win_rect));
	}

	return ret;
}

int decon_set_vsync_int(struct fb_info *info, bool active)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	bool prev_active = decon->vsync.active;

	decon->vsync.active = active;
	smp_wmb();

	if (active && !prev_active)
		decon_activate_vsync(decon);
	else if (!active && prev_active)
		decon_deactivate_vsync(decon);

	return 0;
}

#if defined(CONFIG_ION_EXYNOS)
static unsigned int decon_map_ion_handle(struct decon_device *decon,
		struct device *dev, struct decon_dma_buf_data *dma,
		struct ion_handle *ion_handle, struct dma_buf *buf, int win_no)
{
#ifdef CONFIG_SYNC
	dma->fence = NULL;
#endif
	dma->dma_buf = buf;

	dma->attachment = dma_buf_attach(dma->dma_buf, dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		decon_err("dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}

	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		decon_err("dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}

	/* This is DVA(Device Virtual Address) for setting base address SFR */
	dma->dma_addr = ion_iovmm_map(dma->attachment, 0,
			dma->dma_buf->size, DMA_TO_DEVICE, 0);
	if (!dma->dma_addr || IS_ERR_VALUE(dma->dma_addr)) {
		decon_err("iovmm_map() failed: %pa\n", &dma->dma_addr);
		goto err_iovmm_map;
	}

	exynos_ion_sync_dmabuf_for_device(dev, dma->dma_buf, dma->dma_buf->size,
			DMA_TO_DEVICE);

	dma->ion_handle = ion_handle;

	return dma->dma_buf->size;

err_iovmm_map:
	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);
err_buf_map_attachment:
	dma_buf_detach(dma->dma_buf, dma->attachment);
err_buf_map_attach:
	return 0;
}
#endif

static int decon_import_buffer(struct decon_device *decon, int idx,
		struct decon_win_config *config,
		struct decon_reg_data *regs)
{
#if defined(CONFIG_ION_EXYNOS)
	struct ion_handle *handle;
	struct dma_buf *buf;
	struct decon_dma_buf_data dma_buf_data[MAX_PLANE_CNT];
	struct dsim_device *dsim;
	struct device *dev = NULL;
	int ret = 0, i;
	size_t buf_size = 0;

	decon_dbg("%s +\n", __func__);

	DPU_EVENT_LOG(DPU_EVT_DECON_SET_BUFFER, &decon->sd, ktime_set(0, 0));

	regs->plane_cnt[idx] =
		dpu_get_plane_cnt(config->format, config->dpp_parm.hdr_std);

	for (i = 0; i < regs->plane_cnt[idx]; ++i) {
		handle = ion_import_dma_buf_fd(decon->ion_client,
				config->fd_idma[i]);
		if (IS_ERR(handle)) {
			decon_err("failed to import fd:%d\n", config->fd_idma[i]);
			ret = PTR_ERR(handle);
			goto fail;
		}

		buf = dma_buf_get(config->fd_idma[i]);
		if (IS_ERR_OR_NULL(buf)) {
			decon_err("failed to get dma_buf:%ld\n", PTR_ERR(buf));
			ret = PTR_ERR(buf);
			goto fail_buf;
		}
		if (decon->dt.out_type == DECON_OUT_DSI) {
			dsim = v4l2_get_subdevdata(decon->out_sd[0]);
			dev = dsim->dev;
		}
		buf_size = decon_map_ion_handle(decon, dev, &dma_buf_data[i],
				handle, buf, idx);
		if (!buf_size) {
			decon_err("failed to map buffer\n");
			ret = -ENOMEM;
			goto fail_map;
		}

		regs->dma_buf_data[idx][i] = dma_buf_data[i];
		/* DVA is passed to DPP parameters structure */
		config->dpp_parm.addr[i] = dma_buf_data[i].dma_addr;
	}

	decon_dbg("%s -\n", __func__);

	return ret;

fail_map:
	dma_buf_put(buf);
fail_buf:
	ion_free(decon->ion_client, handle);
fail:
	return ret;
#else
	return 0;
#endif
}

int decon_check_limitation(struct decon_device *decon, int idx,
		struct decon_win_config *config)
{
	if (config->format >= DECON_PIXEL_FORMAT_MAX) {
		decon_err("unknown pixel format %u\n", config->format);
		return -EINVAL;
	}

	switch (config->format) {
		/* RGB 10bit display */
		case DECON_PIXEL_FORMAT_ARGB_2101010:
		case DECON_PIXEL_FORMAT_ABGR_2101010:
		case DECON_PIXEL_FORMAT_RGBA_1010102:
		case DECON_PIXEL_FORMAT_BGRA_1010102:
		/* YUV 10bit display */
		case DECON_PIXEL_FORMAT_NV12M_P010:
		case DECON_PIXEL_FORMAT_NV21M_P010:
		case DECON_PIXEL_FORMAT_NV12M_S10B:
		case DECON_PIXEL_FORMAT_NV21M_S10B:
		/* YUV422 2P */
		case DECON_PIXEL_FORMAT_NV16:
		case DECON_PIXEL_FORMAT_NV61:
		/* YUV422 3P */
		case DECON_PIXEL_FORMAT_YVU422_3P:
		/* YUV420 3P */
		case DECON_PIXEL_FORMAT_YUV420:
		case DECON_PIXEL_FORMAT_YVU420:
		case DECON_PIXEL_FORMAT_YUV420M:
		case DECON_PIXEL_FORMAT_YVU420M:
			decon_err("not supported pixel format\n");
			return -EINVAL;
		default:
			break;
	}

	if (config->blending >= DECON_BLENDING_MAX) {
		decon_err("unknown blending %u\n", config->blending);
		return -EINVAL;
	}

	if ((config->plane_alpha < 0) || (config->plane_alpha > 0xff)) {
		decon_err("plane alpha value(%d) is out of range(0~255)\n",
				config->plane_alpha);
		return -EINVAL;
	}

	if (config->dst.w == 0 || config->dst.h == 0 ||
			config->dst.x < 0 || config->dst.y < 0) {
		decon_err("win[%d] size is abnormal (w:%d, h:%d, x:%d, y:%d)\n",
				idx, config->dst.w, config->dst.h,
				config->dst.x, config->dst.y);
		return -EINVAL;
	}

	if (config->dst.w < 16) {
		decon_err("window wide < 16pixels, width = %u)\n",
				config->dst.w);
		return -EINVAL;
	}

	if ((config->dst.w > config->dst.f_w) ||
			(config->dst.h > config->dst.f_h)) {
		decon_err("dst coordinate is out of range(%d %d %d %d %d %d)\n",
				config->dst.x, config->dst.w, config->dst.f_w,
				config->dst.y, config->dst.h, config->dst.f_h);
		return -EINVAL;
	}

	if (config->idma_type < IDMA_G0 || config->idma_type > IDMA_VG0) {
		decon_err("idma_type(%d) is wrong\n", config->idma_type);
		return -EINVAL;
	}

	if (config->compression) {
		decon_err("compression is not supported\n");
		return -EINVAL;
	}

	if (config->dpp_parm.rot > DPP_ROT_180) {
		decon_err("dpp_parm.rot(%d) is wrong\n", config->dpp_parm.rot);
		return -EINVAL;
	}

	if (config->dpp_parm.comp_src || config->dpp_parm.hdr_std
			|| config->dpp_parm.min_luminance || config->dpp_parm.max_luminance) {
		decon_err("dpp_parm.comp_src(%d)\n", config->dpp_parm.comp_src);
		decon_err("dpp_parm.hdr_std(%d)\n", config->dpp_parm.hdr_std);
		decon_err("dpp_parm.min_luminance(%d)\n", config->dpp_parm.min_luminance);
		decon_err("dpp_parm.max_luminance(%d)\n", config->dpp_parm.max_luminance);
		return -EINVAL;
	}

	return 0;
}

static int decon_set_win_buffer(struct decon_device *decon,
		struct decon_win_config *config,
		struct decon_reg_data *regs, int idx)
{
	int ret, i;
	u32 alpha_length;
#ifdef CONFIG_SYNC
	struct sync_file *fence = NULL;
#endif
	u32 config_size = 0;
	u32 alloc_size = 0;
	u32 byte_per_pixel = 4;

	ret = decon_check_limitation(decon, idx, config);
	if (ret)
		goto err;

	ret = decon_import_buffer(decon, idx, config, regs);
	if (ret)
		goto err;

#ifdef CONFIG_SYNC
	if (config->acq_fence >= 0) {
		/* fence is managed by buffer not plane */
		fence = sync_file_fdget(config->acq_fence);
		regs->dma_buf_data[idx][0].fence = fence;
		if (!fence) {
			decon_err("failed to import fence fd\n");
			ret = -EINVAL;
			goto err_fdget;
		}
		decon_dbg("acq_fence(%d), fence(%p)\n", config->acq_fence, fence);
	}
#endif

	/*
	 * To avoid SysMMU page fault due to small buffer allocation
	 * bpp = 12 : (NV12, NV21) check LUMA side for simplication
	 * bpp = 15 : (8+2_10bit)
	 * bpp = 16 : (RGB16 formats)
	 * bpp = 32 : (RGB32 formats)
	 */
	/* TODO : We should check also YUV format, because YUV has more than 2 palnes.
	 * Also bpp macro is not matched with this. In case of YUV format, each plane's
	 * bpp is needed.
	 */
	if (dpu_get_bpp(config->format) == 12) {
		byte_per_pixel = 1;
	} else if (dpu_get_bpp(config->format) == 15) {
		/* It should be 1.25 byte per pixel of Y plane.
		 * So 1 byte is used instead of floating point.
		 */
		byte_per_pixel = 1;
	} else if (dpu_get_bpp(config->format) == 16) {
		byte_per_pixel = 2;
	} else {
		byte_per_pixel = 4;
	}

	config_size = config->src.f_w * config->src.f_h * byte_per_pixel;
	alloc_size = (u32)(regs->dma_buf_data[idx][0].dma_buf->size);
	if (config_size > alloc_size) {
		decon_err("alloc buf size is less than required size ([w%d] alloc=%x : cfg=%x)\n",
				idx, alloc_size, config_size);
		ret = -EINVAL;
		goto err_fdget;
	}

	alpha_length = dpu_get_alpha_len(config->format);
	regs->protection[idx] = config->protection;
	decon_win_config_to_regs_param(alpha_length, config,
				&regs->win_regs[idx], config->idma_type, idx);

	return 0;

err_fdget:
	for (i = 0; i < regs->plane_cnt[idx]; ++i)
		decon_free_dma_buf(decon, &regs->dma_buf_data[idx][i]);
err:
	return ret;
}

void decon_reg_chmap_validate(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	unsigned short i, bitmap = 0;

	for (i = 0; i < decon->dt.max_win; i++) {
		if (!(regs->win_regs[i].wincon & WIN_EN_F(i)) ||
				(regs->win_regs[i].winmap_state))
			continue;

		if (bitmap & (1 << regs->dpp_config[i].idma_type)) {
			decon_warn("Channel-%d is mapped to multiple windows\n",
					regs->dpp_config[i].idma_type);
			regs->win_regs[i].wincon &= (~WIN_EN_F(i));
		}
		bitmap |= 1 << regs->dpp_config[i].idma_type;
	}
}

static void decon_check_used_dpp(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	decon->cur_using_dpp = 0;

	for (i = 0; i < decon->dt.max_win; i++) {
		struct decon_win *win = decon->win[i];
		if (!regs->win_regs[i].winmap_state)
			win->dpp_id = regs->dpp_config[i].idma_type;
		else
			win->dpp_id = 0xF;

		if ((regs->win_regs[i].wincon & WIN_EN_F(i)) &&
			(!regs->win_regs[i].winmap_state)) {
			set_bit(win->dpp_id, &decon->cur_using_dpp);
			set_bit(win->dpp_id, &decon->prev_used_dpp);
		}
	}
}

static int decon_set_dpp_config(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i, ret = 0, err_cnt = 0;
	struct v4l2_subdev *sd;
	struct decon_win *win;

	for (i = 0; i < decon->dt.max_win; i++) {
		win = decon->win[i];
		/*
		 * Although DPP number is set in cur_using_dpp, connected window
		 * can be disabled. If window related parameter has problem,
		 * requested window from user will be disabled because of
		 * error handling code.
		 */
		if (!test_bit(win->dpp_id, &decon->cur_using_dpp) ||
				!(regs->win_regs[i].wincon & WIN_EN_F(i)))
			continue;

		sd = decon->dpp_sd[win->dpp_id];
		ret = v4l2_subdev_call(sd, core, ioctl,
				DPP_WIN_CONFIG, &regs->dpp_config[i]);
		if (ret) {
			decon_err("failed to config (WIN%d : DPP%d)\n",
					i, win->dpp_id);
			regs->win_regs[i].wincon &= (~WIN_EN_F(i));
			decon_reg_set_win_enable(decon->id, i, false);
			if (regs->num_of_window != 0)
				regs->num_of_window--;
			clear_bit(win->dpp_id, &decon->cur_using_dpp);
			set_bit(win->dpp_id, &decon->dpp_err_stat);
			err_cnt++;
		}
	}

	return err_cnt;
}

static int __decon_update_regs(struct decon_device *decon, struct decon_reg_data *regs)
{
	int err_cnt = 0;
	unsigned short i, j;
	struct decon_mode_info psr;

	decon_to_psr_info(decon, &psr);
	/*
	 * Shadow update bit must be cleared before setting window configuration,
	 * If shadow update bit is not cleared, decon initial state or previous
	 * window configuration has problem.
	 */
	if (decon_reg_wait_for_update_timeout(decon->id, SHADOW_UPDATE_TIMEOUT) < 0) {
		decon_warn("decon SHADOW_UPDATE_TIMEOUT\n");
		return -ETIMEDOUT;
	}

	if (psr.trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_DISABLE);

	/* TODO: check and wait until the required IDMA is free */
	decon_reg_chmap_validate(decon, regs);

	/* apply window update configuration to DECON, DSIM and panel */
	dpu_set_win_update_config(decon, regs);

	for (i = 0; i < decon->dt.max_win; i++) {
		/* set decon registers for each window */
		decon_reg_set_window_control(decon->id, i, &regs->win_regs[i],
						regs->win_regs[i].winmap_state);

		/* backup cur dma_buf_data for freeing next update_handler_regs */
		for (j = 0; j < regs->plane_cnt[i]; ++j)
			decon->win[i]->dma_buf_data[j] = regs->dma_buf_data[i][j];
		decon->win[i]->plane_cnt = regs->plane_cnt[i];
	}

	err_cnt = decon_set_dpp_config(decon, regs);
	if (!regs->num_of_window) {
		decon_err("decon%d: num_of_window=0 during dpp_config(err_cnt:%d)\n",
			decon->id, err_cnt);
		return 0;
	}

#if defined(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	decon_set_protected_content(decon, regs);
#endif

	decon_reg_all_win_shadow_update_req(decon->id);
	decon_to_psr_info(decon, &psr);
	if (decon_reg_start(decon->id, &psr) < 0) {
		decon_up_list_saved();
#ifndef DECON_TIMEOUT_RECOVERY
		decon_dump(decon);
		BUG();
#endif
	}

	DPU_EVENT_LOG(DPU_EVT_TRIG_UNMASK, &decon->sd, ktime_set(0, 0));

	return 0;
}

void decon_wait_for_vstatus(struct decon_device *decon, u32 timeout)
{
	int ret;

	if (decon->id)
		return;

	decon_systrace(decon, 'C', "decon_frame_start", 1);
	ret = wait_event_interruptible_timeout(decon->wait_vstatus,
			(decon->frame_cnt_target <= decon->frame_cnt),
			msecs_to_jiffies(timeout));
	decon_systrace(decon, 'C', "decon_frame_start", 0);
	DPU_EVENT_LOG(DPU_EVT_DECON_FRAMESTART, &decon->sd, ktime_set(0, 0));
	if (!ret)
		decon_warn("%s:timeout\n", __func__);
}

static void __decon_update_clear(struct decon_device *decon, struct decon_reg_data *regs)
{
	unsigned short i, j;

	for (i = 0; i < decon->dt.max_win; i++) {
		for (j = 0; j < regs->plane_cnt[i]; ++j)
			decon->win[i]->dma_buf_data[j] = regs->dma_buf_data[i][j];

		decon->win[i]->plane_cnt = regs->plane_cnt[i];
	}

	return;
}

static void decon_acquire_old_bufs(struct decon_device *decon,
		struct decon_reg_data *regs,
		struct decon_dma_buf_data (*dma_bufs)[MAX_PLANE_CNT],
		int *plane_cnt)
{
	int i, j;

	for (i = 0; i < decon->dt.max_win; i++) {
		for (j = 0; j < MAX_PLANE_CNT; ++j)
			memset(&dma_bufs[i][j], 0, sizeof(struct decon_dma_buf_data));
		plane_cnt[i] = 0;
	}

	for (i = 0; i < decon->dt.max_win; i++) {
		plane_cnt[i] = decon->win[i]->plane_cnt;

		for (j = 0; j < plane_cnt[i]; ++j)
			dma_bufs[i][j] = decon->win[i]->dma_buf_data[j];
	}
}

static void decon_release_old_bufs(struct decon_device *decon,
		struct decon_reg_data *regs,
		struct decon_dma_buf_data (*dma_bufs)[MAX_PLANE_CNT],
		int *plane_cnt)
{
	int i, j;
	struct dsim_device *dsim;

	for (i = 0; i < decon->dt.max_win; i++)
		for (j = 0; j < plane_cnt[i]; ++j)
			decon_free_dma_buf(decon, &dma_bufs[i][j]);

	if (decon->dt.out_type == DECON_OUT_DSI) {
		if (decon->lcd_info->mode == DECON_VIDEO_MODE) {
			dsim = v4l2_get_subdevdata(decon->out_sd[0]);
			if (dsim->fb_reservation) {
				v4l2_subdev_call(decon->out_sd[0], core, ioctl,
						DSIM_IOC_FREE_FB_RES, NULL);
			}
		}
	}
}

#if defined(CONFIG_FRAME_DROP_DETECTOR)
void update_frames_and_vsync(unsigned int *frames, unsigned int *vsync)
{
	static unsigned long long last_frames = 0;
	static unsigned long long last_vsync = 0;
	int diff = 0;

	if (last_frames == 0) {
		last_frames = frames_count;
	} else {
		diff = frames_count - last_frames;
		last_frames = frames_count;
	}

	*frames = diff;

	if (last_vsync == 0) {
		last_vsync = vsync_signal;
	} else {
		diff = vsync_signal - last_vsync;
		last_vsync = vsync_signal;
	}

	*vsync = diff;
}
#endif
static void decon_update_regs(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	struct decon_dma_buf_data old_dma_bufs[decon->dt.max_win][MAX_PLANE_CNT];
	int old_plane_cnt[MAX_DECON_WIN];
	struct decon_mode_info psr;
	int i;

	if (!decon->systrace.pid)
		decon->systrace.pid = current->pid;

	decon_systrace(decon, 'B', "decon_update_regs", 0);

	decon_exit_hiber(decon);

	decon_acquire_old_bufs(decon, regs, old_dma_bufs, old_plane_cnt);

	decon_systrace(decon, 'C', "decon_fence_wait", 1);
	for (i = 0; i < decon->dt.max_win; i++) {
#ifdef CONFIG_SYNC
		if (regs->dma_buf_data[i][0].fence)
			decon_wait_fence(regs->dma_buf_data[i][0].fence);
#endif
#ifdef CONFIG_DRM_DMA_SYNC
		if (regs->dma_buf_data[i][0].dma_buf) {
			regs->dma_buf_data[i][0].fence = tdm_fence(decon->fence_dev,
				regs->dma_buf_data[i][0].dma_buf);
			if (!regs->dma_buf_data[i][0].fence)
				decon_err("%s:failed to get fence:win[%d]buf[%p]\n", __func__, i,
					regs->dma_buf_data[i][0].dma_buf);

			if (decon->dbg_cnt)
				decon_dbg("%s:win[%d]buf[%p]fence[%p]c[%d]\n", __func__, i,
					regs->dma_buf_data[i][0].dma_buf,
					regs->dma_buf_data[i][0].fence, decon->dbg_cnt);
	}
#endif
	}

	decon_systrace(decon, 'C', "decon_fence_wait", 0);

	decon_check_used_dpp(decon, regs);

#ifdef CONFIG_EXYNOS9110_BTS
	/* add calc and update bw : cur > prev */
	decon->bts.ops->bts_calc_bw(decon, regs);
	decon->bts.ops->bts_update_bw(decon, regs, 0);
#endif

	DPU_EVENT_LOG_WINCON(&decon->sd, regs);

	decon_to_psr_info(decon, &psr);
	if (regs->num_of_window) {
		if (__decon_update_regs(decon, regs) < 0) {
#ifdef DECON_TIMEOUT_RECOVERY
			decon_check_max_vsync_timeout(decon);
			goto out;
#else
			decon_dump(decon);
			BUG();
#endif
		}
		if (!regs->num_of_window) {
			__decon_update_clear(decon, regs);
			decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
			goto end;
		}
	} else {
		__decon_update_clear(decon, regs);
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		goto end;
	}

	decon->frame_cnt_target = decon->frame_cnt + 1;

	decon_systrace(decon, 'C', "decon_wait_vsync", 1);
	decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
	decon_systrace(decon, 'C', "decon_wait_vsync", 0);
#if defined(CONFIG_FRAME_DROP_DETECTOR)
	frames_count++;
#endif
	decon_wait_for_vstatus(decon, 50);
	if (decon_reg_wait_for_update_timeout(decon->id, SHADOW_UPDATE_TIMEOUT) < 0) {
#ifdef DECON_TIMEOUT_RECOVERY
		goto out;
#else
		decon_up_list_saved();
		decon_dump(decon);
		BUG();
#endif
	}
#ifdef CONFIG_DRM_TGM
	decon->trig_mask_timestamp =  ktime_get();
#endif
	decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_DISABLE);
	decon->frame_done_cnt_target = decon->frame_done_cnt_cur + 1;

end:
	DPU_EVENT_LOG(DPU_EVT_TRIG_MASK, &decon->sd, ktime_set(0, 0));

	decon_release_old_bufs(decon, regs, old_dma_bufs, old_plane_cnt);

#ifdef CONFIG_SYNC
	/* signal to acquire fence */
	decon_signal_fence(decon);
#endif
#ifdef CONFIG_DRM_DMA_SYNC
	{
		struct decon_fence_node *fence_node;
		int ret, wait = !completion_done(&decon->fence_comp);

		if (wait) {
			decon->dbg_cnt = 3;
			decon_dbg("%s:wait\n", __func__);
		}
		ret = wait_for_completion_timeout(
			&decon->fence_comp, msecs_to_jiffies(33));
		if (ret < 0) {
			decon_err("%s:failed to wait completion:ret[%d]\n", __func__, ret);
			ret = -ERESTARTSYS;
		} else if (!ret)
			decon_dbg("%s:timeout\n", __func__);

		if (ret <= 0) {
			decon_dbg("%s:send fence signal\n", __func__);
			goto err;
		}
		if (wait)
			decon_dbg("%s:wait_done[%d]\n", __func__, ret);

		fence_node = kzalloc(sizeof(*fence_node), GFP_KERNEL);
		if (!fence_node) {
			decon_err("%s:failed to alloc fence_node\n", __func__);
			goto err;
		}

		fence_node->regs = kzalloc(sizeof(*fence_node->regs), GFP_KERNEL);
		if (!fence_node->regs) {
			decon_err("%s:failed to alloc regs\n", __func__);
			kfree(fence_node);
			goto err;
		}

		reinit_completion(&decon->fence_comp);
		INIT_LIST_HEAD(&fence_node->list);
		memcpy(fence_node->regs, regs, sizeof(*regs));
		mutex_lock(&decon->fence_lock);

		if (decon->dbg_cnt)
			decon_info("%s:node[%p]c[%d]\n", __func__, fence_node,
				decon->dbg_cnt);

		list_add_tail(&fence_node->list, &decon->fence_list);
		mutex_unlock(&decon->fence_lock);

		if (!queue_work(decon->fence_wq, (struct work_struct *)
				decon->fence_work))
			decon_warn("%s:busy to queue_work.\n", __func__);
		goto out;
err:
		for (i = 0; i < decon->dt.max_win; i++) {
			if (regs->dma_buf_data[i][0].dma_buf) {
				decon_dbg("%s:win[%d]buf[%p]fence[%p]\n", __func__, i,
					regs->dma_buf_data[i][0].dma_buf,
					regs->dma_buf_data[i][0].fence);

				if (!regs->dma_buf_data[i][0].fence)
					decon_err("%s:failed to signal:win[%d]buf[%p]\n",
							__func__, i,
							regs->dma_buf_data[i][0].dma_buf);
				else
					tdm_fence_signal(decon->fence_dev,
							regs->dma_buf_data[i][0].fence);
			}
		}
	}
out:
#endif
	decon_systrace(decon, 'E', "decon_update_regs", 0);

	DPU_EVENT_LOG(DPU_EVT_FENCE_RELEASE, &decon->sd, ktime_set(0, 0));

#ifdef CONFIG_EXYNOS9110_BTS
	/* add update bw : cur < prev */
	decon->bts.ops->bts_update_bw(decon, regs, 1);
#endif

	decon_dpp_stop(decon, false);
}

static void decon_update_regs_handler(struct kthread_work *work)
{
	struct decon_update_regs *up =
			container_of(work, struct decon_update_regs, work);
	struct decon_device *decon =
			container_of(up, struct decon_device, up);

	struct decon_reg_data *data, *next;
	struct list_head saved_list;

	mutex_lock(&decon->up.lock);
	decon->up.saved_list = decon->up.list;
	saved_list = decon->up.list;
	if (!decon->up_list_saved)
		list_replace_init(&decon->up.list, &saved_list);
	else
		list_replace(&decon->up.list, &saved_list);
	mutex_unlock(&decon->up.lock);

	list_for_each_entry_safe(data, next, &saved_list, list) {
		decon_systrace(decon, 'C', "update_regs_list", 1);
		decon_update_regs(decon, data);
		decon_hiber_unblock(decon);
		if (!decon->up_list_saved) {
			list_del(&data->list);
			decon_systrace(decon, 'C',
					"update_regs_list", 0);
			kfree(data);
		}
	}
}
#ifdef CONFIG_SYNC
static int decon_get_active_win_count(struct decon_device *decon,
		struct decon_win_config_data *win_data)
{
	int i;
	int win_cnt = 0;
	struct decon_win_config *config;
	struct decon_win_config *win_config = win_data->config;

	for (i = 0; i < decon->dt.max_win; i++) {
		config = &win_config[i];

		switch (config->state) {
		case DECON_WIN_STATE_DISABLED:
			break;

		case DECON_WIN_STATE_COLOR:
		case DECON_WIN_STATE_BUFFER:
			win_cnt++;
			break;

		default:
			decon_warn("DECON:WARN:%s:unrecognized window state %u",
					__func__, config->state);
			break;
		}
	}

	return win_cnt;
}
#endif

void decon_set_full_size_win(struct decon_device *decon,
	struct decon_win_config *config)
{
	config->dst.x = 0;
	config->dst.y = 0;
	config->dst.w = decon->lcd_info->xres;
	config->dst.h = decon->lcd_info->yres;
	config->dst.f_w = decon->lcd_info->xres;
	config->dst.f_h = decon->lcd_info->yres;
}

static int decon_prepare_win_config(struct decon_device *decon,
		struct decon_win_config_data *win_data,
		struct decon_reg_data *regs)
{
	int ret = 0;
	int i;
	bool color_map;
	int enabled  = decon->dt.max_win;
	int state_changed = 0;
	struct decon_win_config *win_config = win_data->config;
	struct decon_win_config *config;
	struct decon_window_regs *win_regs;

	decon_dbg("%s +\n", __func__);
	for (i = 0; i < decon->dt.max_win && !ret; i++) {
		config = &win_config[i];
		win_regs = &regs->win_regs[i];
		color_map = true;

		switch (config->state) {
		case DECON_WIN_STATE_DISABLED:
			win_regs->wincon &= ~WIN_EN_F(i);
			enabled--;
			if (config->state != decon->win[i]->state)
				state_changed = 1;
			break;
		case DECON_WIN_STATE_COLOR:
			regs->num_of_window++;
			config->color |= (0xFF << 24);
			win_regs->colormap = config->color;
			/* decon_set_full_size_win(decon, config); */
			decon_win_config_to_regs_param(0, config, win_regs,
					config->idma_type, i);
			ret = 0;
			break;
		case DECON_WIN_STATE_BUFFER:
			if (decon_set_win_blocking_mode(decon, i, win_config, regs))
				break;

			regs->num_of_window++;
			ret = decon_set_win_buffer(decon, config, regs, i);
			if (!ret) {
				color_map = false;
			}
			break;
		default:
			win_regs->wincon &= ~WIN_EN_F(i);
			decon_warn("unrecognized window state %u",
					config->state);
			ret = -EINVAL;
			break;
		}
		win_regs->winmap_state = color_map;
		decon->win[i]->state = config->state;
	}

	if (!enabled &&
	      state_changed &&
	     (decon->aod_state == AOD_OFF)) {
		struct decon_lcd *lcd = decon->lcd_info;

		win_regs = &regs->win_regs[0];
		config = &win_config[i];

		win_regs->wincon = wincon(0x8, 0xFF, 0xFF, 0xFF, DECON_BLENDING_NONE, decon->dt.dft_win);
		win_regs->start_pos = win_start_pos(0, 0);
		win_regs->end_pos = win_end_pos(0, 0, lcd->xres, lcd->yres);
		win_regs->colormap = 0x000000;
		win_regs->pixel_count = lcd->xres * lcd->yres;
		win_regs->whole_w = lcd->xres;
		win_regs->whole_h = lcd->yres;
		win_regs->offset_x = 0;
		win_regs->offset_y = 0;

		decon_win_config_to_regs_param(0, config, win_regs, config->idma_type, 0);
		regs->num_of_window++;
		decon_info("%s : unset buffer\n", __func__);
	}

	for (i = 0; i < MAX_DPP_SUBDEV; i++) {
		memcpy(&regs->dpp_config[i], &win_config[i],
				sizeof(struct decon_win_config));
		regs->dpp_config[i].format = regs->dpp_config[i].format;
	}

	decon_dbg("%s -\n", __func__);

	return ret;
}

static int decon_set_win_config(struct decon_device *decon,
		struct decon_win_config_data *win_data)
{
#ifdef CONFIG_SYNC
	int num_of_window = 0;
#endif
	struct decon_reg_data *regs;
#ifdef CONFIG_SYNC
	struct sync_file *sync_file;
#endif
	int ret = 0;
	decon_dbg("%s +\n", __func__);

	mutex_lock(&decon->lock);

	if (decon->state == DECON_STATE_OFF ||
		decon->state == DECON_STATE_TUI) {

#ifdef CONFIG_SYNC
		win_data->retire_fence = decon_create_fence(decon, &sync_file);
		if (win_data->retire_fence < 0)
			goto err;
		fd_install(win_data->retire_fence, sync_file->file);
		decon_signal_fence(decon);
#endif
		goto err;
	}

	regs = kzalloc(sizeof(struct decon_reg_data), GFP_KERNEL);
	if (!regs) {
		decon_err("could not allocate decon_reg_data\n");
		ret = -ENOMEM;
		goto err;
	}

#ifdef CONFIG_SYNC
	num_of_window = decon_get_active_win_count(decon, win_data);
	if (num_of_window) {
		win_data->retire_fence = decon_create_fence(decon, &sync_file);
		if (win_data->retire_fence < 0)
			goto err_prepare;
	} else {
		decon->timeline_max++;
		win_data->retire_fence = -1;
	}
#endif
	dpu_prepare_win_update_config(decon, win_data, regs);

	ret = decon_prepare_win_config(decon, win_data, regs);
	if (ret)
		goto err_prepare;

	/*
	 * If dpu_prepare_win_update_config returns error, prev_up_region is
	 * updated but that partial size is not applied to HW in previous code.
	 * So, updating prev_up_region is moved here.
	 */
	memcpy(&decon->win_up.prev_up_region, &regs->up_region,
			sizeof(struct decon_rect));

#ifdef CONFIG_SYNC
	if (num_of_window) {
		fd_install(win_data->retire_fence, sync_file->file);
#if defined(CONFIG_DPU_2_0_RELEASE_FENCES)
		decon_create_release_fences(decon, win_data, sync_file);
#endif
	}
#endif
	decon_hiber_block(decon);

	mutex_lock(&decon->up.lock);
	list_add_tail(&regs->list, &decon->up.list);
	mutex_unlock(&decon->up.lock);
	kthread_queue_work(&decon->up.worker, &decon->up.work);

	mutex_unlock(&decon->lock);
	decon_systrace(decon, 'C', "decon_win_config", 0);

	decon_dbg("%s -\n", __func__);

	return ret;

err_prepare:
#ifdef CONFIG_SYNC
	if (win_data->retire_fence >= 0) {
		/* video mode should keep previous buffer object */
		if (decon->lcd_info->mode == DECON_MIPI_COMMAND_MODE)
			decon_signal_fence(decon);
		fput(sync_file->file);
		put_unused_fd(win_data->retire_fence);
	}
#endif
	kfree(regs);
#ifdef CONFIG_SYNC
	win_data->retire_fence = -1;
#endif
err:
	mutex_unlock(&decon->lock);
	return ret;
}

static int decon_get_hdr_capa(struct decon_device *decon,
		struct decon_hdr_capabilities *hdr_capa)
{
	int ret = 0;
	int k;

	decon_dbg("%s +\n", __func__);
	mutex_lock(&decon->lock);

	memset(hdr_capa, 0, sizeof(struct decon_hdr_capabilities));

	if (decon->dt.out_type == DECON_OUT_DSI) {
		for (k = 0; k < decon->lcd_info->dt_lcd_hdr.hdr_num; k++)
			hdr_capa->out_types[k] =
				decon->lcd_info->dt_lcd_hdr.hdr_type[k];
	}

	mutex_unlock(&decon->lock);
	decon_dbg("%s -\n", __func__);

	return ret;
}

static int decon_get_hdr_capa_info(struct decon_device *decon,
		struct decon_hdr_capabilities_info *hdr_capa_info)
{
	int ret = 0;

	decon_dbg("%s +\n", __func__);
	mutex_lock(&decon->lock);

	if (decon->dt.out_type == DECON_OUT_DSI) {
		hdr_capa_info->out_num =
			decon->lcd_info->dt_lcd_hdr.hdr_num;
		hdr_capa_info->max_luminance =
			decon->lcd_info->dt_lcd_hdr.hdr_max_luma;
		hdr_capa_info->max_average_luminance =
			decon->lcd_info->dt_lcd_hdr.hdr_max_avg_luma;
		hdr_capa_info->min_luminance =
			decon->lcd_info->dt_lcd_hdr.hdr_min_luma;
	} else
		memset(hdr_capa_info, 0, sizeof(struct decon_hdr_capabilities_info));

	mutex_unlock(&decon->lock);
	decon_dbg("%s -\n", __func__);

	return ret;

}

static int decon_set_metadata(struct decon_device *decon,
		struct decon_metadata *metadata)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	int ret = 0;

	mutex_lock(&decon->output_lock);

	if (!dsim) {
		decon_err("%s:failed to get dsim\n", __func__);
		ret = -ENODEV;
		goto err;
	}

	decon_info("%s:ops[%d]state[%d]\n", __func__, metadata->ops, decon->state);

	if (decon->dpms != FB_BLANK_UNBLANK) {
		decon_err("%s:invalid dpms[%d]\n", __func__, decon->dpms);
		goto err;
	}

	switch (metadata->ops) {
	case METADATA_OP_AOD_SET_INFO:
	{
		struct aod_config *aod_cfg = &metadata->aod_cfg;
		if (aod_cfg->req == AOD_SET_CONFIG && aod_cfg->mode == AOD_SCLK_ANALOG) {
			struct sclk_analog_cfg ana_cfg = aod_cfg->analog_cfg;
			int i;

			for (i = 0; i < SCLK_MAX_BUF; i++) {
				unsigned long fd, addr;
				struct ion_handle *handle;

				fd = ana_cfg.addr[i];
				handle = ion_import_dma_buf_fd(decon->ion_client, fd);
				if (IS_ERR(handle)) {
					decon_err("failed to import fd:%lx\n", fd);
					ret = PTR_ERR(handle);
					goto err;
				}
				addr = (unsigned long)ion_handle_kmap_get(handle);
				decon_dbg("%s address :%lx\n", __func__, addr);
				metadata->aod_cfg.analog_cfg.addr[i] = addr;
			}
		}
		ret = call_panel_ops(dsim, metadata_set, dsim, metadata);
		break;
	}
	case METADATA_OP_AOD_SET_STATE:
		decon_info("%s:aod[%d -> %d]\n", __func__, decon->aod_state, metadata->state);

		switch (metadata->state) {
		case AOD_ENTER:
			kthread_flush_worker(&decon->up.worker);
			call_panel_ops(dsim, aod_ctrl, dsim, 1);
			break;
		case AOD_UPDATE_REQ:
			break;
		case AOD_UPDATE_DONE:
			kthread_flush_worker(&decon->up.worker);
			break;
		case AOD_EXIT:
			call_panel_ops(dsim, aod_ctrl, dsim, 0);
			metadata->state = AOD_OFF;
			break;
		default:
			decon_err("%s:invalid state[%d]\n", __func__, metadata->state);
			ret = -EINVAL;
			goto err;
			break;
		}

		decon->aod_state = metadata->state;
		break;
	default:
		decon_err("%s:invalid ops[%d]\n", __func__, metadata->ops);
		ret = -EINVAL;
		break;
	}

err:
	decon_info("%s:ops[%d]state[%d]ret[%d]\n", __func__, metadata->ops, decon->state, ret);

	mutex_unlock(&decon->output_lock);

	return ret;
}

static int decon_set_aod_time(struct decon_device *decon, struct sclk_time_cfg_v2 *time_cfg)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	int ret = 0;

	decon_info("%s + HH[%d]:MM[%d]:SS[%d]:MS[%d] \n", __func__,
		time_cfg->hour, time_cfg->minute, time_cfg->second, time_cfg->millisecond);

	call_panel_ops(dsim, aod_time_set, dsim, time_cfg);

	return ret;
}


static int decon_set_aod_analog(struct decon_device *decon, struct sclk_analog_cfg_v2 *analog_cfg)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	struct ion_handle *hour_handle, *minute_handle, *second_handle;
	struct sclk_analog_hand *hand;
	unsigned long fd, addr;
	int ret = 0;

	decon_info("%s + \n", __func__);

	hand = &analog_cfg->hour;
	fd = hand->addr;
	decon_info("%s hour fd :%lx\n", __func__, fd);
	hour_handle = ion_import_dma_buf_fd(decon->ion_client, fd);
	if (IS_ERR(hour_handle)) {
		decon_err("failed to import fd:%lx\n", fd);
		ret = PTR_ERR(hour_handle);
		goto err;
	}
	addr = (unsigned long)ion_map_kernel(decon->ion_client, hour_handle);
	decon_info("%s hour address :%lx\n", __func__, addr);
	hand->addr = addr;

	hand = &analog_cfg->min;
	fd = hand->addr;
	decon_info("%s minute fd :%lx\n", __func__, fd);
	minute_handle = ion_import_dma_buf_fd(decon->ion_client, fd);
	if (IS_ERR(minute_handle)) {
		decon_err("failed to import fd:%lx\n", fd);
		ret = PTR_ERR(minute_handle);
		goto err;
	}
	addr = (unsigned long)ion_map_kernel(decon->ion_client, minute_handle);
	decon_info("%s minute address :%lx\n", __func__, addr);
	hand->addr = addr;

	hand = &analog_cfg->sec;
	fd = hand->addr;
	decon_info("%s second fd :%lx\n", __func__, fd);
	second_handle = ion_import_dma_buf_fd(decon->ion_client, fd);
	if (IS_ERR(second_handle)) {
		decon_err("failed to import fd:%lx\n", fd);
		ret = PTR_ERR(second_handle);
		goto err;
	}
	addr = (unsigned long)ion_map_kernel(decon->ion_client, second_handle);
	decon_info("%s second address :%lx\n", __func__, addr);
	hand->addr = addr;

	call_panel_ops(dsim, aod_analog_set, dsim, analog_cfg);

	ion_unmap_kernel(decon->ion_client, hour_handle);
	ion_free(decon->ion_client, hour_handle);
	ion_unmap_kernel(decon->ion_client, minute_handle);
	ion_free(decon->ion_client, minute_handle);
	ion_unmap_kernel(decon->ion_client, second_handle);
	ion_free(decon->ion_client, second_handle);

err:
	return ret;
}


static int decon_set_aod_digital(struct decon_device *decon, struct sclk_digital_cfg_v2 *digital_cfg)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	struct ion_handle *handle;
	unsigned long fd, addr;
	int ret = 0;

	decon_info("%s + \n", __func__);

	fd = digital_cfg->font_addr;
	decon_info("%s font fd :%lx\n", __func__, fd);
	handle = ion_import_dma_buf_fd(decon->ion_client, fd);
	if (IS_ERR(handle)) {
		decon_err("failed to import fd:%lx\n", fd);
		ret = PTR_ERR(handle);
		goto err;
	}
	addr = (unsigned long)ion_map_kernel(decon->ion_client, handle);
	decon_info("%s font address :%lx\n", __func__, addr);
	digital_cfg->font_addr = addr;

	call_panel_ops(dsim, aod_digital_set, dsim, digital_cfg);

	ion_unmap_kernel(decon->ion_client, handle);
	ion_free(decon->ion_client, handle);

err:
	return ret;
}


static int decon_set_aod_icon(struct decon_device *decon, struct sclk_icon_cfg_v2 *icon_cfg)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	struct ion_handle *handle;
	unsigned long fd, addr;
	int ret = 0;

	decon_info("%s + \n", __func__);

	if (icon_cfg->req == AOD_DATA_SET) {
		fd = icon_cfg->addr;
		decon_info("%s icon fd :%lx\n", __func__, fd);
		handle = ion_import_dma_buf_fd(decon->ion_client, fd);
		if (IS_ERR(handle)) {
			decon_err("failed to import fd:%lx\n", fd);
			ret = PTR_ERR(handle);
			goto err;
		}
		addr = (unsigned long)ion_map_kernel(decon->ion_client, handle);
		decon_info("%s icon address :%lx\n", __func__, addr);
		icon_cfg->addr = addr;
	}

	ret = call_panel_ops(dsim, aod_icon_set, dsim, icon_cfg);

	if (icon_cfg->req == AOD_DATA_SET) {
		ion_unmap_kernel(decon->ion_client, handle);
		ion_free(decon->ion_client, handle);
	}
err:
	return ret;
}


static struct sclk_move_cfg_v2 move_info;
static int decon_set_aod_move(struct decon_device *decon, struct sclk_move_cfg_v2 *move_cfg)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	int ret = 0;

	decon_info("%s + \n", __func__);
	switch (move_cfg->req) {
		case AOD_DATA_SET:
			move_info.h = move_cfg->h;
			move_info.w = move_cfg->w;
			move_info.clock_mask = move_cfg->clock_mask;
			move_info.blink_mask = move_cfg->blink_mask;
			move_info.icon_mask = move_cfg->icon_mask;
			break;
		case AOD_DATA_POS:
			move_info.x = move_cfg->x;
			move_info.y = move_cfg->y;
			break;
		case AOD_DATA_ENABLE:
			move_info.mask = move_cfg->mask;
			ret = call_panel_ops(dsim, aod_move_set, dsim, &move_info);
			break;
	}

	return ret;
}

static int decon_enter_aod(struct decon_device *decon, int para)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	int ret = 0;
	int watch_type;

	decon_info("%s + \n", __func__);

	switch (para) {
	case 0:
		watch_type = AOD_SCLK_ANALOG;
		decon->aod_state = AOD_ENTER;
		break;
	case 1:
		watch_type = AOD_SCLK_DIGITAL;
		decon->aod_state = AOD_ENTER;
		break;
	case 2:
		watch_type = AOD_HLPM;
		decon->aod_state = AOD_ENTER;
		break;
	default:
		watch_type = AOD_DISABLE;
		decon->aod_state = AOD_OFF;
		break;
	}

	ret = call_panel_ops(dsim, aod_mode_enter, dsim, watch_type);

	return ret;
}

static int decon_exit_aod(struct decon_device *decon, int para)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	int ret = 0;
	int watch_type;

	decon_info("%s + \n", __func__);

	decon->aod_state = AOD_OFF;

	switch (para) {
	case 0:
		watch_type = AOD_SCLK_ANALOG;
		break;
	case 1:
		watch_type = AOD_SCLK_DIGITAL;
		break;
	case 2:
		watch_type = AOD_HLPM;
		break;
	default:
		watch_type = AOD_DISABLE;
		break;
	}

	ret = call_panel_ops(dsim, aod_mode_exit, dsim, watch_type);

	return ret;
}


static int decon_afpc_set_compensation(struct decon_device *decon, struct afpc_compensation_v2 *comp)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	struct ion_handle *handle;
	unsigned long fd, addr;
	int ret = 0;

	decon_info("%s + \n", __func__);

	fd = comp->addr;
	decon_info("%s afpc compensation fd :%lx\n", __func__, fd);
	handle = ion_import_dma_buf_fd(decon->ion_client, fd);
	if (IS_ERR(handle)) {
		decon_err("failed to import fd:%lx\n", fd);
		ret = PTR_ERR(handle);
		goto err;
	}
	addr = (unsigned long)ion_map_kernel(decon->ion_client, handle);
	decon_info("%s afpc compensation address :%lx\n", __func__, addr);
	comp->addr = addr;

	ret = call_panel_ops(dsim, afpc_compensation_set, dsim, comp);

	ion_unmap_kernel(decon->ion_client, handle);
	ion_free(decon->ion_client, handle);

err:
	return ret;
}

static int decon_afpc_get_panel_id(struct decon_device *decon, struct afpc_panel_v2 *afpc_panel)
{
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	int ret = 0;

	ret = call_panel_ops(dsim, afpc_panel_get, dsim, afpc_panel);

	return ret;
}


static int decon_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	struct decon_lcd *lcd_info = decon->lcd_info;
	struct lcd_mres_info *mres_info = &lcd_info->dt_lcd_mres;
	struct decon_win_config_data win_data;
	struct decon_disp_info disp_info;
	struct decon_hdr_capabilities hdr_capa;
	struct decon_hdr_capabilities_info hdr_capa_info;
	struct decon_win_config_data __user *argp;
	struct decon_disp_info __user *argp_info;
	int ret = 0;
	u32 crtc;
	bool active;
	u32 crc_bit, crc_start;
	u32 crc_data[2];

	decon_hiber_block_exit(decon);
	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		if (get_user(crtc, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		if (crtc == 0)
			ret = decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		else
			ret = -ENODEV;

		break;

	case S3CFB_SET_VSYNC_INT:
		if (get_user(active, (bool __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_vsync_int(info, active);
		break;

	case S3CFB_WIN_CONFIG:
		argp = (struct decon_win_config_data __user *)arg;
		DPU_EVENT_LOG(DPU_EVT_WIN_CONFIG, &decon->sd, ktime_set(0, 0));
		decon_systrace(decon, 'C', "decon_win_config", 1);
		if (copy_from_user(&win_data,
				   (struct decon_win_config_data __user *)arg,
				   sizeof(struct decon_win_config_data))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_win_config(decon, &win_data);
		if (ret)
			break;
#ifdef CONFIG_SYNC
#if defined(CONFIG_DPU_2_0_RELEASE_FENCES)
		if (copy_to_user((void __user *)arg, &win_data, _IOC_SIZE(cmd))) {
			ret = -EFAULT;
			break;
		}
		break;
#else
		if (copy_to_user(&((struct decon_win_config_data __user *)arg)->retire_fence,
				 &win_data.retire_fence, sizeof(int))) {
			ret = -EFAULT;
			break;
		}
		break;
#endif
#else
		break;
#endif
	case S3CFB_GET_HDR_CAPABILITIES:
		ret = decon_get_hdr_capa(decon, &hdr_capa);
		if (ret)
			break;

		if (copy_to_user((struct decon_hdr_capabilities __user *)arg,
				&hdr_capa,
				sizeof(struct decon_hdr_capabilities))) {
			ret = -EFAULT;
			break;
		}
		break;

	case S3CFB_GET_HDR_CAPABILITIES_NUM:
		ret = decon_get_hdr_capa_info(decon, &hdr_capa_info);
		if (ret)
			break;

		if (copy_to_user((struct decon_hdr_capabilities_info __user *)arg,
				&hdr_capa_info,
				sizeof(struct decon_hdr_capabilities_info))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_DISP_INFO:
		argp_info = (struct decon_disp_info  __user *)arg;
		if (copy_from_user(&disp_info, argp_info,
				   sizeof(struct decon_disp_info))) {
			ret = -EFAULT;
			break;
		}

		if ((decon->ver == HWC_INIT) ||
				(decon->ver != disp_info.ver)) {
			decon->ver = disp_info.ver;
#ifdef CONFIG_SYNC
			if (decon->ver == HWC_2_0) {
				decon->timeline_max = 0;
				decon_info("decon is setting by HWC%d.0\n",
						decon->ver);
			} else {
				decon->timeline_max = 1;
			}
#endif
		}
		disp_info.psr_mode = decon->dt.psr_mode;
		disp_info.chip_ver = CHIP_VER;
		disp_info.mres_info = *mres_info;

		if (copy_to_user(argp_info,
				 &disp_info, sizeof(struct decon_disp_info))) {
			ret = -EFAULT;
			break;
		}
		break;

	case S3CFB_START_CRC:
		if (get_user(crc_start, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}
		mutex_lock(&decon->lock);
		if (decon->state != DECON_STATE_ON) {
			decon_err("DECON:WRN:%s:decon%d is not active:cmd=%d\n",
				__func__, decon->id, cmd);
			ret = -EIO;
			mutex_unlock(&decon->lock);
			break;
		}
		mutex_unlock(&decon->lock);
		decon_reg_set_start_crc(decon->id, crc_start);
		break;

	case S3CFB_SEL_CRC_BITS:
		if (get_user(crc_bit, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}
		mutex_lock(&decon->lock);
		if (decon->state != DECON_STATE_ON) {
			decon_err("DECON:WRN:%s:decon%d is not active:cmd=%d\n",
				__func__, decon->id, cmd);
			ret = -EIO;
			mutex_unlock(&decon->lock);
			break;
		}
		mutex_unlock(&decon->lock);
		decon_reg_set_select_crc_bits(decon->id, crc_bit);
		break;

	case S3CFB_GET_CRC_DATA:
		mutex_lock(&decon->lock);
		if (decon->state != DECON_STATE_ON) {
			decon_err("DECON:WRN:%s:decon%d is not active:cmd=%d\n",
				__func__, decon->id, cmd);
			ret = -EIO;
			mutex_unlock(&decon->lock);
			break;
		}
		mutex_unlock(&decon->lock);
		decon_reg_get_crc_data(decon->id, &crc_data[0], &crc_data[1]);
		if (copy_to_user((u32 __user *)arg, &crc_data[0], sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_DPU_DUMP:
		mutex_lock(&decon->lock);
		if (decon->state != DECON_STATE_ON) {
			decon_err("DECON:WRN:%s:decon%d is not active:cmd=%d\n",
				__func__, decon->id, cmd);
			ret = -EIO;
			mutex_unlock(&decon->lock);
			break;
		}
		mutex_unlock(&decon->lock);
		decon_dump(decon);
		break;
	case S3CFB_METADATA_SET: {
		struct decon_metadata metadata;
		if (copy_from_user(&metadata,
				   (struct decon_metadata __user *)arg,
				   sizeof(metadata))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_metadata(decon, &metadata);
		break;
	}
	case S3CFB_AOD_SET_TIME: {
		struct sclk_time_cfg_v2 time_cfg;
		if (copy_from_user(&time_cfg,
				   (struct sclk_time_cfg_v2 __user *)arg,
				   sizeof(time_cfg))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_aod_time(decon, &time_cfg);
		break;
	}
	case S3CFB_AOD_SET_ANALOG: {
		struct sclk_analog_cfg_v2 analog_cfg;
		if (copy_from_user(&analog_cfg,
				   (struct sclk_analog_cfg_v2 __user *)arg,
				   sizeof(analog_cfg))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_aod_analog(decon, &analog_cfg);
		break;
	}
	case S3CFB_AOD_SET_DIGITAL: {
		struct sclk_digital_cfg_v2 digital_cfg;
		if (copy_from_user(&digital_cfg,
				   (struct sclk_digital_cfg_v2 __user *)arg,
				   sizeof(digital_cfg))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_aod_digital(decon, &digital_cfg);
		break;
	}
	case S3CFB_AOD_SET_ICON: {
		struct sclk_icon_cfg_v2 icon_cfg;
		if (copy_from_user(&icon_cfg,
				   (struct sclk_icon_cfg_v2 __user *)arg,
				   sizeof(icon_cfg))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_aod_icon(decon, &icon_cfg);
		break;
	}
	case S3CFB_AOD_SET_MOVE: {
		struct sclk_move_cfg_v2 move_cfg;
		if (copy_from_user(&move_cfg,
				   (struct sclk_move_cfg_v2 __user *)arg,
				   sizeof(move_cfg))) {
			ret = -EFAULT;
			break;
		}
		ret = decon_set_aod_move(decon, &move_cfg);
		break;
	}
	case S3CFB_AOD_ENTER: {
		 unsigned int para;
		 if (get_user(para, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = decon_enter_aod(decon, para);
		if (!ret)
			disp_stat_aod_update(AOD_ENTER);
		break;
	}
	case S3CFB_AOD_EXIT: {
		 unsigned int para;
		 if (get_user(para, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = decon_exit_aod(decon, para);
		if (!ret)
			disp_stat_aod_update(AOD_EXIT);
		break;
	}
	case S3CFB_AFPC_GET_PANEL_ID: {
		struct afpc_panel_v2 afpc_panel;

		if (decon_afpc_get_panel_id(decon, &afpc_panel) < 0) {
			ret = -EFAULT;
			break;
		}
		if (copy_to_user((struct afpc_panel_v2 __user *)arg,
				 &afpc_panel, sizeof(afpc_panel))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case S3CFB_AFPC_SET_COMPENSATION: {
		struct afpc_compensation_v2 comp;
		if (copy_from_user(&comp,
				   (struct afpc_compensation_v2 __user *)arg,
				   sizeof(comp))) {
			ret = -EFAULT;
			break;
		}
		decon_afpc_set_compensation(decon, &comp);
		break;
	}
	default:
		ret = -ENOTTY;
	}

	decon_hiber_unblock(decon);
	return ret;
}

static ssize_t decon_fb_read(struct fb_info *info, char __user *buf,
		size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t decon_fb_write(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	return 0;
}

int decon_release(struct fb_info *info, int user)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;

	decon_info("%s + : %d\n", __func__, decon->id);

	if (decon->id && decon->dt.out_type == DECON_OUT_DSI) {
		decon_get_out_sd(decon);
		decon_info("output device of decon%d is changed to %s\n",
				decon->id, decon->out_sd[0]->name);
	}

	if (decon->dt.out_type == DECON_OUT_DSI) {
		decon_hiber_block_exit(decon);
		/* Unused DECON state is DECON_STATE_INIT */
		if (decon->state == DECON_STATE_ON)
			decon_disable(decon);
		decon_hiber_unblock(decon);
	}

	decon_info("%s - : %d\n", __func__, decon->id);

	return 0;
}

#ifdef CONFIG_COMPAT
static int decon_compat_ioctl(struct fb_info *info, unsigned int cmd,
		unsigned long arg)
{
	arg = (unsigned long) compat_ptr(arg);
	return decon_ioctl(info, cmd, arg);
}
#endif

/* ---------- FREAMBUFFER INTERFACE ----------- */
static struct fb_ops decon_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= decon_check_var,
	.fb_set_par	= decon_set_par,
	.fb_blank	= decon_blank,
	.fb_setcolreg	= decon_setcolreg,
	.fb_fillrect    = cfb_fillrect,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = decon_compat_ioctl,
#endif
	.fb_ioctl	= decon_ioctl,
	.fb_read	= decon_fb_read,
	.fb_write	= decon_fb_write,
	.fb_pan_display	= decon_pan_display,
	.fb_mmap	= decon_mmap,
	.fb_release	= decon_release,
};

/* ---------- POWER MANAGEMENT ----------- */
void decon_clocks_info(struct decon_device *decon)
{
}

void decon_put_clocks(struct decon_device *decon)
{
}

int decon_runtime_resume(struct device *dev)
{
	struct decon_device *decon = dev_get_drvdata(dev);

	decon_dbg("decon%d %s +\n", decon->id, __func__);
	clk_prepare_enable(decon->res.aclk);
/*
 * TODO :
 * There was an under-run issue when DECON suspend/resume
 * was operating while DP was operating.
 */

	DPU_EVENT_LOG(DPU_EVT_DECON_RESUME, &decon->sd, ktime_set(0, 0));
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

int decon_runtime_suspend(struct device *dev)
{
	struct decon_device *decon = dev_get_drvdata(dev);

	decon_dbg("decon%d %s +\n", decon->id, __func__);
	clk_disable_unprepare(decon->res.aclk);

/*
 * TODO :
 * There was an under-run issue when DECON suspend/resume
 * was operating while DP was operating.
 */

	DPU_EVENT_LOG(DPU_EVT_DECON_SUSPEND, &decon->sd, ktime_set(0, 0));
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

const struct dev_pm_ops decon_pm_ops = {
	.runtime_suspend = decon_runtime_suspend,
	.runtime_resume	 = decon_runtime_resume,
};

static int decon_register_subdevs(struct decon_device *decon)
{
	struct v4l2_device *v4l2_dev = &decon->v4l2_dev;
	int i, ret = 0;

	snprintf(v4l2_dev->name, sizeof(v4l2_dev->name), "%s",
			dev_name(decon->dev));
	ret = v4l2_device_register(decon->dev, &decon->v4l2_dev);
	if (ret) {
		decon_err("failed to register v4l2 device : %d\n", ret);
		return ret;
	}

	ret = dpu_get_sd_by_drvname(decon, DPP_MODULE_NAME);
	if (ret)
		return ret;

	ret = dpu_get_sd_by_drvname(decon, DSIM_MODULE_NAME);
	if (ret)
		return ret;

	if (!decon->id) {
		for (i = 0; i < MAX_DPP_SUBDEV; i++) {
			ret = v4l2_device_register_subdev(v4l2_dev,
					decon->dpp_sd[i]);
			if (ret) {
				decon_err("failed to register dpp%d sd\n", i);
				return ret;
			}
		}

		for (i = 0; i < MAX_DSIM_CNT; i++) {
			if (decon->dsim_sd[i] == NULL || i == 1)
				continue;

			ret = v4l2_device_register_subdev(v4l2_dev,
					decon->dsim_sd[i]);
			if (ret) {
				decon_err("failed to register dsim%d sd\n", i);
				return ret;
			}
		}
	}

	ret = v4l2_device_register_subdev_nodes(&decon->v4l2_dev);
	if (ret) {
		decon_err("failed to make nodes for subdev\n");
		return ret;
	}

	decon_dbg("Register V4L2 subdev nodes for DECON\n");

	if (decon->dt.out_type == DECON_OUT_DSI)
		ret = decon_get_out_sd(decon);

	return ret;
}

static void decon_unregister_subdevs(struct decon_device *decon)
{
	int i;

	if (!decon->id) {
		for (i = 0; i < MAX_DPP_SUBDEV; i++) {
			if (decon->dpp_sd[i] == NULL)
				continue;
			v4l2_device_unregister_subdev(decon->dpp_sd[i]);
		}

		for (i = 0; i < MAX_DSIM_CNT; i++) {
			if (decon->dsim_sd[i] == NULL || i == 1)
				continue;
			v4l2_device_unregister_subdev(decon->dsim_sd[i]);
		}

	}

	v4l2_device_unregister(&decon->v4l2_dev);
}

static void decon_release_windows(struct decon_win *win)
{
	if (win->fbinfo)
		framebuffer_release(win->fbinfo);
}

static int decon_fb_alloc_memory(struct decon_device *decon, struct decon_win *win)
{
	struct decon_lcd *lcd_info = decon->lcd_info;
	struct fb_info *fbi = win->fbinfo;
	struct dsim_device *dsim;
	struct device *dev = NULL;
	unsigned int real_size, virt_size, size;
	dma_addr_t map_dma;
#if defined(CONFIG_ION_EXYNOS)
	struct ion_handle *handle;
	struct dma_buf *buf;
	void *vaddr;
	unsigned int ret;
#endif

	decon_dbg("%s +\n", __func__);
	dev_info(decon->dev, "allocating memory for display\n");

	real_size = lcd_info->xres * lcd_info->yres;
	virt_size = lcd_info->xres * (lcd_info->yres * 2);

	dev_info(decon->dev, "real_size=%u (%u.%u), virt_size=%u (%u.%u)\n",
		real_size, lcd_info->xres, lcd_info->yres,
		virt_size, lcd_info->xres, lcd_info->yres * 2);

	size = (real_size > virt_size) ? real_size : virt_size;
	size *= DEFAULT_BPP / 8;

	fbi->fix.smem_len = size;
	size = PAGE_ALIGN(size);

	dev_info(decon->dev, "want %u bytes for window[%d]\n", size, win->idx);

#if defined(CONFIG_ION_EXYNOS)
	handle = ion_alloc(decon->ion_client, (size_t)size, 0,
					EXYNOS_ION_HEAP_SYSTEM_MASK, 0);
	if (IS_ERR(handle)) {
		dev_err(decon->dev, "failed to ion_alloc\n");
		return -ENOMEM;
	}

	buf = ion_share_dma_buf(decon->ion_client, handle);
	if (IS_ERR_OR_NULL(buf)) {
		dev_err(decon->dev, "ion_share_dma_buf() failed\n");
		goto err_share_dma_buf;
	}

	vaddr = ion_map_kernel(decon->ion_client, handle);

#if !defined(VADDR_COLORMAP)
	memset(vaddr, 0x00, size);
#else
	{
		int i;

		for (i = 0; i < size; i+=4) {
			memset(vaddr + i + 0, 0, 1);
			memset(vaddr + i + 1, 0, 1);
			memset(vaddr + i + 2, 0, 1);
			memset(vaddr + i + 3, 0xFF, 1);
		}
	}
#endif
	fbi->screen_base = vaddr;

#ifdef CONFIG_SYNC
	win->dma_buf_data[1].fence = NULL;
	win->dma_buf_data[2].fence = NULL;
#endif
	win->plane_cnt = 1;

	if (decon->dt.out_type == DECON_OUT_DSI) {
		dsim = v4l2_get_subdevdata(decon->out_sd[0]);
		dev = dsim->dev;
	}
	ret = decon_map_ion_handle(decon, dev, &win->dma_buf_data[0], handle,
			buf, win->idx);
	if (!ret)
		goto err_map;
	map_dma = win->dma_buf_data[0].dma_addr;

	dev_info(decon->dev, "alloated memory\n");
#else
	fbi->screen_base = dma_alloc_writecombine(decon->dev, size,
						  &map_dma, GFP_KERNEL);
	if (!fbi->screen_base)
		return -ENOMEM;

	dev_dbg(decon->dev, "mapped %x to %p\n",
		(unsigned int)map_dma, fbi->screen_base);

	memset(fbi->screen_base, 0x0, size);
#endif
	fbi->fix.smem_start = map_dma;

	dev_info(decon->dev, "fb start addr = 0x%x\n", (u32)fbi->fix.smem_start);

	decon_dbg("%s -\n", __func__);

	return 0;

#ifdef CONFIG_ION_EXYNOS
err_map:
	dma_buf_put(buf);
err_share_dma_buf:
	ion_free(decon->ion_client, handle);
	return -ENOMEM;
#endif
}

#if defined(CONFIG_FB_TEST)
static int decon_fb_test_alloc_memory(struct decon_device *decon, u32 size)
{
	struct fb_info *fbi = decon->win[decon->dt.dft_win]->fbinfo;
	struct decon_win *win = decon->win[decon->dt.dft_win];
	struct dsim_device *dsim;
	struct device *dev;
	dma_addr_t map_dma;
	struct ion_handle *handle;
	struct dma_buf *buf;
	void *vaddr;
	unsigned int ret;

	decon_dbg("%s +\n", __func__);
	dev_info(decon->dev, "allocating memory for fb test\n");

	size = PAGE_ALIGN(size);
	fbi->fix.smem_len = size;

	dev_info(decon->dev, "want %u bytes for window[%d]\n", size, win->idx);

	handle = ion_alloc(decon->ion_client, (size_t)size, 0,
					EXYNOS_ION_HEAP_SYSTEM_MASK, 0);
	if (IS_ERR(handle)) {
		dev_err(decon->dev, "failed to ion_alloc\n");
		return -ENOMEM;
	}

	buf = ion_share_dma_buf(decon->ion_client, handle);
	if (IS_ERR_OR_NULL(buf)) {
		dev_err(decon->dev, "ion_share_dma_buf() failed\n");
		goto err_share_dma_buf;
	}

	vaddr = ion_map_kernel(decon->ion_client, handle);

	memset(vaddr, 0x00, size);

	fbi->screen_base = vaddr;

	if (decon->dt.out_type == DECON_OUT_DSI) {
		dsim = v4l2_get_subdevdata(decon->out_sd[0]);
		dev = dsim->dev;
	}
	ret = decon_map_ion_handle(decon, dev, &win->fb_buf_data, handle,
			buf, win->idx);
	if (!ret)
		goto err_map;
	map_dma = win->fb_buf_data.dma_addr;

	dev_info(decon->dev, "alloated memory\n");
	fbi->fix.smem_start = map_dma;

	dev_info(decon->dev, "fb start addr = 0x%x\n", (u32)fbi->fix.smem_start);

	decon_dbg("%s -\n", __func__);

	return 0;

err_map:
	dma_buf_put(buf);
err_share_dma_buf:
	ion_free(decon->ion_client, handle);
	return -ENOMEM;
}
#endif

static int decon_acquire_window(struct decon_device *decon, int idx)
{
	struct decon_win *win;
	struct fb_info *fbinfo;
	struct fb_var_screeninfo *var;
	struct decon_lcd *lcd_info = decon->lcd_info;
	int ret, i;

	decon_dbg("acquire DECON window%d\n", idx);

	fbinfo = framebuffer_alloc(sizeof(struct decon_win), decon->dev);
	if (!fbinfo) {
		decon_err("failed to allocate framebuffer\n");
		return -ENOENT;
	}

	win = fbinfo->par;
	decon->win[idx] = win;
	var = &fbinfo->var;
	win->fbinfo = fbinfo;
	win->decon = decon;
	win->idx = idx;

	if (decon->dt.out_type == DECON_OUT_DSI) {
		win->videomode.left_margin = lcd_info->hbp;
		win->videomode.right_margin = lcd_info->hfp;
		win->videomode.upper_margin = lcd_info->vbp;
		win->videomode.lower_margin = lcd_info->vfp;
		win->videomode.hsync_len = lcd_info->hsa;
		win->videomode.vsync_len = lcd_info->vsa;
		win->videomode.xres = lcd_info->xres;
		win->videomode.yres = lcd_info->yres;
		// hack code
		win->videomode.pixclock = 128600;
		fb_videomode_to_var(&fbinfo->var, &win->videomode);
	}

	for (i = 0; i < MAX_PLANE_CNT; ++i)
		memset(&win->dma_buf_data[i], 0, sizeof(win->dma_buf_data[i]));

	if ((decon->dt.out_type == DECON_OUT_DSI)
			&& (idx == decon->dt.dft_win)) {
		ret = decon_fb_alloc_memory(decon, win);
		if (ret) {
			dev_err(decon->dev, "failed to alloc display memory\n");
			return ret;
		}
#if defined(CONFIG_FB_TEST)
		ret = decon_fb_test_alloc_memory(decon,
				win->fbinfo->fix.smem_len);
		if (ret) {
			dev_err(decon->dev, "failed to alloc test fb memory\n");
			return ret;
		}
#endif
	}

	fbinfo->fix.type	= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.accel	= FB_ACCEL_NONE;
	fbinfo->var.activate	= FB_ACTIVATE_NOW;
	fbinfo->var.vmode	= FB_VMODE_NONINTERLACED;
	fbinfo->var.bits_per_pixel = DEFAULT_BPP;
	fbinfo->var.width	= lcd_info->width;
	fbinfo->var.height	= lcd_info->height;
	fbinfo->var.yres_virtual = lcd_info->yres * 2;
	fbinfo->fbops		= &decon_fb_ops;
	fbinfo->flags		= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette  = &win->pseudo_palette;
	/* 'divide by 8' means converting bit to byte number */
	fbinfo->fix.line_length = fbinfo->var.xres * fbinfo->var.bits_per_pixel / 8;
	fbinfo->fix.ypanstep = 1;
	decon_info("default_win %d win_idx %d xres %d yres %d\n",
			decon->dt.dft_win, idx,
			fbinfo->var.xres, fbinfo->var.yres);

	ret = decon_check_var(&fbinfo->var, fbinfo);
	if (ret < 0) {
		dev_err(decon->dev, "check_var failed on initial video params\n");
		return ret;
	}

	decon_dbg("decon%d window[%d] create\n", decon->id, idx);
	return 0;
}

static int decon_acquire_windows(struct decon_device *decon)
{
	int i, ret;

	for (i = 0; i < decon->dt.max_win; i++) {
		ret = decon_acquire_window(decon, i);
		if (ret < 0) {
			decon_err("failed to create decon-int window[%d]\n", i);
			for (; i >= 0; i--)
				decon_release_windows(decon->win[i]);
			return ret;
		}
	}

	ret = register_framebuffer(decon->win[decon->dt.dft_win]->fbinfo);
	if (ret) {
		decon_err("failed to register framebuffer\n");
		return ret;
	}

	return 0;
}

static void decon_parse_dt(struct decon_device *decon)
{
	struct device_node *te_eint;
	struct device_node *cam_stat;
	struct device *dev = decon->dev;
	int ret;

	if (!dev->of_node) {
		decon_warn("no device tree information\n");
		return;
	}

	decon->id = of_alias_get_id(dev->of_node, "decon");
	of_property_read_u32(dev->of_node, "max_win",
			&decon->dt.max_win);
	of_property_read_u32(dev->of_node, "default_win",
			&decon->dt.dft_win);
	of_property_read_u32(dev->of_node, "default_idma",
			&decon->dt.dft_idma);
	/* video mode: 0, dp: 1 mipi command mode: 2 */
	of_property_read_u32(dev->of_node, "psr_mode",
			&decon->dt.psr_mode);
	/* H/W trigger: 0, S/W trigger: 1 */
	of_property_read_u32(dev->of_node, "trig_mode",
			&decon->dt.trig_mode);
	decon_info("decon-%s: max win%d, %s mode, %s trigger\n",
			(decon->id == 0) ? "f" : ((decon->id == 1) ? "s" : "t"),
			decon->dt.max_win,
			decon->dt.psr_mode ? "command" : "video",
			decon->dt.trig_mode ? "sw" : "hw");

	/* 0: DSI_MODE_SINGLE, 1: DSI_MODE_DUAL_DSI */
	of_property_read_u32(dev->of_node, "dsi_mode", &decon->dt.dsi_mode);
	decon_info("dsi mode(%d). 0: SINGLE 1: DUAL\n", decon->dt.dsi_mode);

	of_property_read_u32(dev->of_node, "out_type", &decon->dt.out_type);
	decon_info("out type(%d). 0: DSI 1: DISPLAYPORT 2: HDMI 3: WB\n",
			decon->dt.out_type);

	if (decon->dt.out_type == DECON_OUT_DSI) {
		ret = of_property_read_u32_index(dev->of_node, "out_idx", 0,
				&decon->dt.out_idx[0]);
		if (ret) {
			decon->dt.out_idx[0] = 0;
			decon_info("failed to parse out_idx[0].\n");
		}
		decon_info("out idx(%d). 0: DSI0 1: DSI1 2: DSI2\n",
				decon->dt.out_idx[0]);

		te_eint = of_get_child_by_name(decon->dev->of_node, "te_eint");
		if (!te_eint) {
			decon_info("No DT node for te_eint\n");
		} else {
			decon->d.eint_pend = of_iomap(te_eint, 0);
			if (!decon->d.eint_pend)
				decon_info("Failed to get te eint pend\n");
		}

		cam_stat = of_get_child_by_name(decon->dev->of_node, "cam-stat");
		if (!cam_stat) {
			decon_info("No DT node for cam_stat\n");
		} else {
			decon->hiber.cam_status = of_iomap(cam_stat, 0);
			if (!decon->hiber.cam_status)
				decon_info("Failed to get CAM0-STAT Reg\n");
		}
	}

}

static int decon_get_disp_ss_addr(struct decon_device *decon)
{
	if (of_have_populated_dt()) {
		struct device_node *nd;
#if defined(CONFIG_SOC_EXYNOS9110)
		nd = of_find_compatible_node(NULL, NULL,
				"samsung,exynos9-disp_ss");
#else
		nd = of_find_compatible_node(NULL, NULL,
				"samsung,exynos8-disp_ss");
#endif
		if (!nd) {
			decon_err("failed find compatible node(sysreg-disp)");
			return -ENODEV;
		}

		decon->res.ss_regs = of_iomap(nd, 0);
		if (!decon->res.ss_regs) {
			decon_err("Failed to get sysreg-disp address.");
			return -ENOMEM;
		}
	} else {
		decon_err("failed have populated device tree");
		return -EIO;
	}

	return 0;
}

static int decon_init_resources(struct decon_device *decon,
		struct platform_device *pdev, char *name)
{
	struct resource *res;
	int ret;

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	decon->res.regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(decon->res.regs)) {
		decon_err("failed to remap register region\n");
		ret = -ENOENT;
		goto err;
	}

	if (decon->dt.out_type == DECON_OUT_DSI) {
		decon_get_clocks(decon);
		ret = decon_register_irq(decon);
		if (ret)
			goto err;

		if (decon->dt.psr_mode != DECON_VIDEO_MODE) {
			ret = decon_register_ext_irq(decon);
			if (ret)
				goto err;
		}
	}
	else
		decon_err("not supported output type(%d)\n", decon->dt.out_type);

	/* mapping SYSTEM registers */
	ret = decon_get_disp_ss_addr(decon);
	if (ret)
		goto err;

#if defined(CONFIG_ION_EXYNOS)
	decon->ion_client = exynos_ion_client_create(name);
	if (IS_ERR(decon->ion_client)) {
		decon_err("failed to ion_client_create\n");
		ret = PTR_ERR(decon->ion_client);
		goto err_ion;
	}
#endif

	return 0;
#if defined(CONFIG_ION_EXYNOS)
err_ion:
	iounmap(decon->res.ss_regs);
#endif
err:
	return ret;
}

static void decon_destroy_update_thread(struct decon_device *decon)
{
	if (decon->up.thread)
		kthread_stop(decon->up.thread);
}

static int decon_create_update_thread(struct decon_device *decon, char *name)
{
	struct sched_param param;

	INIT_LIST_HEAD(&decon->up.list);
	INIT_LIST_HEAD(&decon->up.saved_list);
	decon->up_list_saved = false;
	kthread_init_worker(&decon->up.worker);
	decon->up.thread = kthread_run(kthread_worker_fn,
			&decon->up.worker, name);
	if (IS_ERR(decon->up.thread)) {
		decon->up.thread = NULL;
		decon_err("failed to run update_regs thread\n");
		return PTR_ERR(decon->up.thread);
	}
	param.sched_priority = 20;
	sched_setscheduler_nocheck(decon->up.thread, SCHED_FIFO, &param);
	kthread_init_work(&decon->up.work, decon_update_regs_handler);

	return 0;
}

#if defined(CONFIG_EXYNOS_ITMON)
static int decon_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct decon_device *decon;
	struct itmon_notifier *itmon_data = nb_data;

	decon = container_of(nb, struct decon_device, itmon_nb);

	decon_info("%s: DECON%d +\n", __func__, decon->id);

	if (decon->notified)
		return NOTIFY_DONE;

	if (IS_ERR_OR_NULL(itmon_data))
		return NOTIFY_DONE;

	/* port is master and dest is target */
	if ((itmon_data->port && (strncmp("DPU", itmon_data->port,
					sizeof("DPU") - 1) == 0)) ||
			(itmon_data->dest && (strncmp("DPU", itmon_data->dest,
					sizeof("DPU") - 1) == 0))) {
		decon_info("%s: port: %s, dest: %s, action: %lu\n", __func__,
				itmon_data->port, itmon_data->dest, action);
		out_sd_dump();
		decon_dump(decon);
		decon->notified = true;
		return NOTIFY_OK;
	}

	decon_info("%s -\n", __func__);

	return NOTIFY_DONE;
}
#endif

static int decon_initial_display(struct decon_device *decon, bool is_colormap)
{
	struct decon_param p;
	struct fb_info *fbinfo = decon->win[decon->dt.dft_win]->fbinfo;
	struct decon_window_regs win_regs;
	struct decon_win_config config;
	struct v4l2_subdev *sd = NULL;
	struct decon_mode_info psr;
	struct dsim_device *dsim;

	if (decon->id || (decon->dt.out_type != DECON_OUT_DSI)) {
		decon->state = DECON_STATE_OFF;
		decon_info("decon%d doesn't need to display\n", decon->id);
		return 0;
	}

	pm_stay_awake(decon->dev);
	dev_warn(decon->dev, "pm_stay_awake");

	if (decon->dt.psr_mode != DECON_VIDEO_MODE) {
		if (decon->res.pinctrl && decon->res.hw_te_on) {
			if (pinctrl_select_state(decon->res.pinctrl,
						decon->res.hw_te_on)) {
				decon_err("failed to turn on Decon_TE\n");
				return -EINVAL;
			}
		}
	}

	decon_to_init_param(decon, &p);

	if (decon_reg_init(decon->id, decon->dt.out_idx[0], &p) < 0)
		goto decon_init_done;

	memset(&win_regs, 0, sizeof(struct decon_window_regs));
	win_regs.wincon = wincon(0x8, 0xFF, 0xFF, 0xFF, DECON_BLENDING_NONE,
			decon->dt.dft_win);
	win_regs.start_pos = win_start_pos(0, 0);
	win_regs.end_pos = win_end_pos(0, 0, fbinfo->var.xres, fbinfo->var.yres);
	decon_dbg("xres %d yres %d win_start_pos %x win_end_pos %x\n",
			fbinfo->var.xres, fbinfo->var.yres, win_regs.start_pos,
			win_regs.end_pos);
	win_regs.colormap = 0x000000;
	win_regs.pixel_count = fbinfo->var.xres * fbinfo->var.yres;
	win_regs.whole_w = fbinfo->var.xres_virtual;
	win_regs.whole_h = fbinfo->var.yres_virtual;
	win_regs.offset_x = fbinfo->var.xoffset;
	win_regs.offset_y = fbinfo->var.yoffset;
	win_regs.type = decon->dt.dft_idma;
	decon_dbg("pixel_count(%d), whole_w(%d), whole_h(%d), x(%d), y(%d)\n",
			win_regs.pixel_count, win_regs.whole_w,
			win_regs.whole_h, win_regs.offset_x,
			win_regs.offset_y);
	decon_reg_set_window_control(decon->id, decon->dt.dft_win,
			&win_regs, is_colormap);

	set_bit(decon->dt.dft_idma, &decon->cur_using_dpp);
	set_bit(decon->dt.dft_idma, &decon->prev_used_dpp);
	memset(&config, 0, sizeof(struct decon_win_config));
	config.dpp_parm.addr[0] = fbinfo->fix.smem_start;
	config.format = DECON_PIXEL_FORMAT_BGRA_8888;
	config.src.w = fbinfo->var.xres;
	config.src.h = fbinfo->var.yres;
	config.src.f_w = fbinfo->var.xres;
	config.src.f_h = fbinfo->var.yres;
	config.dst.w = config.src.w;
	config.dst.h = config.src.h;
	config.dst.f_w = config.src.f_w;
	config.dst.f_h = config.src.f_h;
	sd = decon->dpp_sd[decon->dt.dft_idma];
	if (v4l2_subdev_call(sd, core, ioctl, DPP_WIN_CONFIG, &config)) {
		decon_err("Failed to config DPP-%d\n",
				decon->dt.dft_idma);
		clear_bit(decon->dt.dft_idma, &decon->cur_using_dpp);
		set_bit(decon->dt.dft_idma, &decon->dpp_err_stat);
	}
	decon_reg_update_req_window(decon->id, decon->dt.dft_win);

	decon_to_psr_info(decon, &psr);
	decon_reg_set_int(decon->id, &psr, 1);

	/* TODO:
	 * 1. If below code is called after turning on 1st LCD.
	 *    2nd LCD is not turned on
	 * 2. It needs small delay between decon start and LCD on
	 *    for avoiding garbage display when dual dsi mode is used. */

	dsim = container_of(decon->out_sd[0], struct dsim_device, sd);

#if defined(CONFIG_EXYNOS_DECON_LCD_S6E3AA2)
	/* Set DSIM Command LP mode */
	v4l2_subdev_call(decon->out_sd[0], core, ioctl,
			DSIM_IOC_SET_CMD_LPMODE, (unsigned long *)1);
#endif

	call_panel_ops(dsim, preinit, dsim);

#if defined(CONFIG_EXYNOS_DECON_LCD_S6E3AA2)
	/* Set DSIM Command HS mode */
	v4l2_subdev_call(decon->out_sd[0], core, ioctl,
			DSIM_IOC_SET_CMD_LPMODE, (unsigned long *)0);
#endif

	call_panel_ops(dsim, init, dsim);

	decon_reg_start(decon->id, &psr);
	call_panel_ops(dsim, displayon, dsim);

	decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
	if (decon_reg_wait_update_done_and_mask(decon->id, &psr,
				SHADOW_UPDATE_TIMEOUT) < 0)
		decon_err("%s: wait_for_update_timeout\n", __func__);

decon_init_done:

	decon->state = DECON_STATE_INIT;

	return 0;
}


#ifdef CONFIG_DRM_DMA_SYNC
static int decon_notifier_ctrl(struct notifier_block *this,
			unsigned long cmd, void *_data)
{
	struct decon_device *decon = container_of(this,
		struct decon_device, nb_ctrl);
	struct tdm_nb_event *event =
		(struct tdm_nb_event *)_data;
	int ret = NOTIFY_OK;

	decon_dbg("%s:cmd[%d]\n", __func__, (int)cmd);

	switch (cmd) {
	case TDM_NOTI_REGISTER:
		decon->fence_dev = event->data;
		break;
#ifdef CONFIG_DRM_TDM_DPMS_CTRL
	case TDM_NOTI_DPMS_CTRL: {
		struct fb_info *fbinfo;
		int *dpms = event->data;
		int blank;

		fbinfo = decon->win[decon->dt.dft_win]->fbinfo;

		if (!lock_fb_info(fbinfo)) {
			decon_err("%s:failed to lock_fb_info\n", __func__);
			return NOTIFY_BAD;
		}

		switch (*dpms) {
		case DRM_MODE_DPMS_ON:
			blank = FB_BLANK_UNBLANK;
			break;
		case DRM_MODE_DPMS_STANDBY:
			blank = FB_BLANK_NORMAL;
			break;
		case DRM_MODE_DPMS_OFF:
			blank = FB_BLANK_POWERDOWN;
			break;
		default:
			decon_err("invalid dpms[%d]\n", *dpms);
			unlock_fb_info(fbinfo);
			return NOTIFY_BAD;
		}

		if (fb_blank(fbinfo, blank))
			ret = NOTIFY_BAD;
		unlock_fb_info(fbinfo);
	}
		break;
#endif
	case TDM_NOTI_VSYNC_CTRL: {
		long int en = (long int)event->data;

		decon_dbg("VSYNC_CTRL[%ld]", en);

#if 0
		if (en)
			decon_lpd_block_exit(decon);
		else
			decon_lpd_unblock(decon);
#endif
		}
		break;
	default:
		break;
	}

	decon_dbg("%s:ret[0x%x]\n", __func__, ret);
	return ret;
}

static void decon_fence_work(struct work_struct *work)
{
	struct decon_fence_work *fence_work =
		(struct decon_fence_work *)work;
	struct decon_device *decon = fence_work->decon;
	struct decon_fence_node *fence_node;
	struct decon_reg_data *regs;
	int i;

	mutex_lock(&decon->fence_lock);

	if (list_empty(&decon->fence_list)) {
		decon_err("%s:failed to get fence_list\n", __func__);
		mutex_unlock(&decon->fence_lock);
		goto out;
	}

	fence_node = list_first_entry(&decon->fence_list,
		struct decon_fence_node, list);

	if (decon->dbg_cnt)
		decon_info("%s_in:node[%p]c[%d]\n", __func__, fence_node, decon->dbg_cnt);
	mutex_unlock(&decon->fence_lock);

	decon_wait_for_framedone(decon);
	regs = fence_node->regs;

	for (i = 0; i < decon->dt.max_win; i++) {
		if (regs->dma_buf_data[i][0].dma_buf) {

			if (decon->dbg_cnt)
				decon_info("%s:win[%d]buf[%p]fence[%p]c[%d]\n", __func__, i,
					regs->dma_buf_data[i][0].dma_buf,
					regs->dma_buf_data[i][0].fence, decon->dbg_cnt);

			if (!regs->dma_buf_data[i][0].fence)
				decon_err("%s:failed to send signal:win[%d]buf[%p]\n", __func__, i,
					regs->dma_buf_data[i][0].dma_buf);
			else
				tdm_fence_signal(decon->fence_dev, regs->dma_buf_data[i][0].fence);
		}
	}

	mutex_lock(&decon->fence_lock);
	list_del(&fence_node->list);

	if (decon->dbg_cnt) {
		decon_info("%s_out:node[%p]c[%d]\n", __func__, fence_node, decon->dbg_cnt);
		decon->dbg_cnt--;
	}
	mutex_unlock(&decon->fence_lock);
	kfree(fence_node->regs);
	kfree(fence_node);

out:
	complete_all(&decon->fence_comp);
	return;
}
#endif

extern int get_panel_id(void);

static struct decon_device *p_decon;

/* --------- DRIVER INITIALIZATION ---------- */
static int decon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct decon_device *decon;
	int ret = 0;
	char device_name[MAX_NAME_SIZE];
#ifdef CONFIG_DRM_DMA_SYNC
	struct decon_fence_work *fence_work;
#endif

	dev_info(dev, "%s start\n", __func__);
	p_decon = NULL;

	if (get_panel_id() == -1)
	{
		decon_err("No lcd attached!\n");
		return 0;
	}

	decon = devm_kzalloc(dev, sizeof(struct decon_device), GFP_KERNEL);
	if (!decon) {
		decon_err("no memory for decon device\n");
		ret = -ENOMEM;
		goto err;
	}
	p_decon = decon;
	decon->init_step = 1;

	decon->dev = dev;
	decon_parse_dt(decon);

	decon_drvdata[decon->id] = decon;

	spin_lock_init(&decon->slock);
	init_waitqueue_head(&decon->vsync.wait);
	init_waitqueue_head(&decon->wait_vstatus);
	init_waitqueue_head(&decon->wait_frmdone);
	mutex_init(&decon->vsync.lock);
	mutex_init(&decon->lock);
	mutex_init(&decon->pm_lock);
	mutex_init(&decon->up.lock);
	mutex_init(&decon->output_lock);

	decon_enter_shutdown_reset(decon);

	snprintf(device_name, MAX_NAME_SIZE, "decon%d", decon->id);
#ifdef CONFIG_SYNC
	decon_create_timeline(decon, device_name);
#endif

	/* systrace */
	decon_systrace_enable = 0;
	decon->systrace.pid = 0;

	ret = decon_init_resources(decon, pdev, device_name);
	if (ret) {
		decon->init_step = 2;
		goto err_res;
	}

	ret = decon_create_vsync_thread(decon);
	if (ret) {
		decon->init_step = 3;
		goto err_vsync;
	}

	ret = decon_create_psr_info(decon);
	if (ret) {
		decon->init_step = 4;
		goto err_psr;
	}

	ret = decon_get_pinctrl(decon);
	if (ret) {
		decon->init_step = 5;
		goto err_pinctrl;
	}

	ret = decon_create_debugfs(decon);
	if (ret) {
		decon->init_step = 6;
		goto err_pinctrl;
	}

#ifdef CONFIG_DECON_HIBER
	ret = decon_register_hiber_work(decon);
	if (ret) {
		decon->init_step = 7;
		goto err_pinctrl;
	}
#endif
	ret = decon_register_subdevs(decon);
	if (ret) {
		decon->init_step = 8;
		goto err_subdev;
	}

	ret = decon_acquire_windows(decon);
	if (ret) {
		decon->init_step = 9;
		goto err_win;
	}

	ret = decon_create_update_thread(decon, device_name);
	if (ret) {
		decon->init_step = 10;
		goto err_win;
	}

	dpu_init_win_update(decon);

#ifdef CONFIG_EXYNOS9110_BTS
	decon->bts.ops = &decon_bts_control;
	decon->bts.ops->bts_init(decon);
#endif

	platform_set_drvdata(pdev, decon);
	pm_runtime_enable(dev);

#ifdef CONFIG_DISPLAY_EARLY_DPMS
		device_set_early_complete(dev, EARLY_COMP_MASTER);
#endif
	/* prevent sleep enter during display(LCD, DP) on */
	ret = device_init_wakeup(decon->dev, true);
	if (ret) {
		dev_err(decon->dev, "failed to init wakeup device\n");
		decon->init_step = 11;
		goto err_display;
	}

#if defined(CONFIG_EXYNOS_ITMON)
	decon->itmon_nb.notifier_call = decon_itmon_notifier;
//	itmon_notifier_chain_register(&decon->itmon_nb);
	/* for DPHY debug */
	out_sd_ioremap();
#endif

	decon->dpms = FB_BLANK_POWERDOWN;
	ret = decon_initial_display(decon, false);
	if (ret) {
		decon->init_step = 12;
		goto err_display;
	}
#ifdef CONFIG_DRM_DMA_SYNC
	decon->nb_ctrl.notifier_call = decon_notifier_ctrl;
	if (tdm_nb_register(&decon->nb_ctrl))
		pr_err("could not register notify callback\n");

	decon->fence_wq= create_singlethread_workqueue("decon_fwq");
	if (!decon->fence_wq)
		pr_err("failed to create workq.\n");

	fence_work = kzalloc(sizeof(*fence_work), GFP_KERNEL);
	if (!fence_work) {
		pr_err("failed to alloc fence_work.\n");
		decon->init_step = 13;
		goto err_display;
	}

	fence_work->decon = decon;
	decon->fence_work = fence_work;
	INIT_WORK((struct work_struct *)decon->fence_work,
		decon_fence_work);

	init_completion(&decon->fence_comp);
	INIT_LIST_HEAD(&decon->fence_list);
	mutex_init(&decon->fence_lock);
	complete(&decon->fence_comp);
#endif

	tdm_set_irq(decon->res.irq);
	decon_info("decon%d registered successfully", decon->id);
	decon->init_step = 14;

	return 0;

err_display:
	decon_destroy_update_thread(decon);
err_win:
	decon_unregister_subdevs(decon);
	mutex_destroy(&decon->output_lock);
err_subdev:
	decon_destroy_debugfs(decon);
err_pinctrl:
	decon_destroy_psr_info(decon);
err_psr:
	decon_destroy_vsync_thread(decon);
err_vsync:
	iounmap(decon->res.ss_regs);
err_res:
	kfree(decon);
err:
	decon_err("decon probe fail");
	return ret;
}

static int decon_remove(struct platform_device *pdev)
{
	struct decon_device *decon = platform_get_drvdata(pdev);
	int i;

	if (!decon) {
		decon_warn("decon is not initialized\n");
		return 0;
	}

#ifdef CONFIG_EXYNOS9110_BTS
	decon->bts.ops->bts_deinit(decon);
#endif

#ifdef CONFIG_DRM_DMA_SYNC
	kfree(decon->fence_work);
	destroy_workqueue(decon->fence_wq);
	tdm_nb_unregister(&decon->nb_ctrl);
#endif
	pm_runtime_disable(&pdev->dev);
	decon_put_clocks(decon);
	unregister_framebuffer(decon->win[0]->fbinfo);

	if (decon->up.thread)
		kthread_stop(decon->up.thread);

	for (i = 0; i < decon->dt.max_win; i++)
		decon_release_windows(decon->win[i]);

	debugfs_remove_recursive(decon->d.debug_root);

	decon_info("remove sucessful\n");
	return 0;
}

static void decon_shutdown(struct platform_device *pdev)
{
	struct decon_device *decon;
	struct fb_info *fbinfo;

	decon = platform_get_drvdata(pdev);
	if (!decon) {
		decon_warn("decon is not initialized\n");
		return;
	}
	fbinfo = decon->win[decon->dt.dft_win]->fbinfo;

	decon_enter_shutdown(decon);

	if (!lock_fb_info(fbinfo)) {
		decon_warn("%s: fblock is failed\n", __func__);
		return;
	}

	decon_info("%s + state:%d\n", __func__, decon->state);
	DPU_EVENT_LOG(DPU_EVT_DECON_SHUTDOWN, &decon->sd, ktime_set(0, 0));

	decon_hiber_block_exit(decon);
	/* Unused DECON state is DECON_STATE_INIT */
	if (decon->state == DECON_STATE_ON)
		decon_disable(decon);

	unlock_fb_info(fbinfo);

	decon_info("%s -\n", __func__);
	return;
}

static const struct of_device_id decon_of_match[] = {
#if defined(CONFIG_SOC_EXYNOS9110)
	{ .compatible = "samsung,exynos9-decon" },
#else
	{ .compatible = "samsung,exynos8-decon" },
#endif
	{},
};
MODULE_DEVICE_TABLE(of, decon_of_match);

static struct platform_driver decon_driver __refdata = {
	.probe		= decon_probe,
	.remove		= decon_remove,
	.shutdown	= decon_shutdown,
	.driver = {
		.name	= DECON_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &decon_pm_ops,
		.of_match_table = of_match_ptr(decon_of_match),
		.suppress_bind_attrs = true,
	}
};

static int exynos_decon_register(void)
{
	platform_driver_register(&decon_driver);

	return 0;
}

static void exynos_decon_unregister(void)
{
	platform_driver_unregister(&decon_driver);
}
late_initcall(exynos_decon_register);
module_exit(exynos_decon_unregister);

MODULE_DESCRIPTION("Samsung EXYNOS DECON driver");
MODULE_LICENSE("GPL");
