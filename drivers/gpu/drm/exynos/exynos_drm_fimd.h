/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Jinyoung Jeon <jy0.jeon@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _EXYNOS_DRM_FIMD_H_
#define _EXYNOS_DRM_FIMD_H_

#define FIMD_HIGH_FRAMERATE 60
#define FIMD_LOW_FRAMERATE 30

#define RUNTIME_OFF_PERIOD	600 /* ms */
#define WIDTH_LIMIT			16

/*
 * FIMD is stand for Fully Interactive Mobile Display and
 * as a display controller, it transfers contents drawn on memory
 * to a LCD Panel through Display Interfaces such as RGB or
 * CPU Interface.
 */

/* position control register for hardware window 0, 2 ~ 4.*/
#define VIDOSD_A(win)		(VIDOSD_BASE + 0x00 + (win) * 16)
#define VIDOSD_B(win)		(VIDOSD_BASE + 0x04 + (win) * 16)
/* size control register for hardware window 0. */
#define VIDOSD_C_SIZE_W0	(VIDOSD_BASE + 0x08)
/* alpha control register for hardware window 1 ~ 4. */
#define VIDOSD_C(win)		(VIDOSD_BASE + 0x18 + (win) * 16)
/* size control register for hardware window 1 ~ 4. */
#define VIDOSD_D(win)		(VIDOSD_BASE + 0x0C + (win) * 16)

#define VIDWx_BUF_START(win, buf)	(VIDW_BUF_START(buf) + (win) * 8)
#define VIDWx_BUF_END(win, buf)		(VIDW_BUF_END(buf) + (win) * 8)
#define VIDWx_BUF_SIZE(win, buf)	(VIDW_BUF_SIZE(buf) + (win) * 4)

#define VIDW0nADD2_DWORD_CHECK(pagewidth, offsize)	\
		((pagewidth + offsize) % 8)

/* color key control register for hardware window 1 ~ 4. */
#define WKEYCON0_BASE(x)		((WKEYCON0 + 0x140) + (x * 8))
/* color key value register for hardware window 1 ~ 4. */
#define WKEYCON1_BASE(x)		((WKEYCON1 + 0x140) + (x * 8))

/* i80 interface control register */
#define I80IFCONFAx(x)			(0x1B0 + x * 4)
#define I80IFCONFBx(x)			(0x1B8 + x * 4)
#define LCD_CS_SETUP(x)			(x << 16)
#define LCD_WR_SETUP(x)			(x << 12)
#define LCD_WR_ACT(x)			(x << 8)
#define LCD_WR_HOLD(x)			(x << 4)
#define LCD_WR_RS_POL(x)		(x << 2)
#define I80IFEN_ENABLE			(1 << 0)
/* Speifies color gain control register */
#define CGAINCON		(0x01C0)
/* Spedified gamma LUT data */
#define GAMMALUT_START	(0x037C)

/* FIMD has totally five hardware windows. */
#define WINDOWS_NR	5
#ifdef CONFIG_DRM_EXYNOS_DBG
#define FIMD_MAX_REG	128
#define FIMD_TUNE_REG_NUM	5
#endif

#define get_fimd_context(dev)	platform_get_drvdata(to_platform_device(dev))

#ifdef CONFIG_MDNIE_SUPPORT
#define STOP_TIMEOUT	20
static struct s5p_fimd_ext_device *fimd_lite_dev, *mdnie;
static struct s5p_fimd_dynamic_refresh *fimd_refresh;
#endif

/* memory type definitions. */
enum fimd_shadowcon {
	FIMD_SC_UNPROTECT	= 0 << 0,
	FIMD_SC_PROTECT	= 1 << 0,
	FIMD_SC_BYPASS_CH_CTRL	= 1 << 1,
	FIMD_SC_CH_DISABLE	= 0 << 2,
	FIMD_SC_CH_ENABLE	= 1 << 2,
};

#define FIMD_AGING_LOG
#ifdef FIMD_AGING_LOG
#define FIMD_LOG_INFO_MAX 128

enum fimd_log_state {
	FIMD_LOG_DPMS,
	FIMD_LOG_WIN_COMMIT,
	FIMD_LOG_ISR,
	FIMD_LOG_SET_LPM,
	FIMD_LOG_UNSET_LPM,
	FIMD_LOG_MAX,
};

struct fimd_debug_log {
	unsigned long long time;
	pid_t pid;
};

static atomic_t fimd_log_idx[FIMD_LOG_MAX] = {
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1) };
static struct fimd_debug_log fimd_log_info[FIMD_LOG_MAX][FIMD_LOG_INFO_MAX];
#endif

struct fimd_driver_data {
	unsigned int timing_base;
	unsigned int lcdblk_off;
	unsigned int lcdblk_shift;

	unsigned int has_shadowcon:1;
	unsigned int has_clksel:1;
	unsigned int has_limited_fmt:1;
	bool	use_vclk;
};

#ifdef CONFIG_DRM_EXYNOS_DBG
struct reg_info_t {
	char *name;
	unsigned int start;
	unsigned int end;
};

enum {
	TUNE_ENABLE = 0,
	COLOR_GAIN,
	HUE,
	GAMMA_64,
	GAMMA_16,
};

struct reg_info_t fimd_tune_info[5] = {
	{.name = "VIDCON3", .start = 0x000C, .end = 0x000C},
	{.name = "GAIN", .start = 0x01C0, .end = 0x01C0},
	{.name = "HUE", .start = 0x1EC, .end = 0x020C},
	{.name = "GAMMA_64", .start = 0x037C,  .end = 0x03FC},
	{.name = "GAMMA_16", .start = 0x037C,  .end = 0x03FC},
};
#endif

static struct fimd_driver_data s3c64xx_fimd_driver_data = {
	.timing_base = 0x0,
	.has_clksel = 1,
	.has_limited_fmt = 1,
};

static struct fimd_driver_data exynos3_fimd_driver_data = {
	.timing_base = 0x20000,
	.lcdblk_off = 0x210,
	.lcdblk_shift = 10,
	.has_shadowcon = 1,
};

static struct fimd_driver_data exynos4_fimd_driver_data = {
	.timing_base = 0x0,
	.lcdblk_off = 0x210,
	.lcdblk_shift = 10,
	.has_shadowcon = 1,
};

static struct fimd_driver_data exynos5_fimd_driver_data = {
	.timing_base = 0x20000,
	.lcdblk_off = 0x210,
	.lcdblk_shift = 24,
	.has_shadowcon = 1,
};

struct fimd_win_data {
	unsigned int		offset_x;
	unsigned int		offset_y;
	unsigned int		ovl_width;
	unsigned int		ovl_height;
	unsigned int		fb_width;
	unsigned int		fb_height;
	unsigned int		bpp;
	unsigned int		refresh;
	unsigned int		pixel_format;
	dma_addr_t		base_dma_addr;
	dma_addr_t		dma_addr;
	unsigned int		buf_offsize;
	unsigned int		line_size;	/* bytes */
	unsigned int		local_path;
	bool			enabled;
	bool			resume;
};

struct fimd_lpm_work {
	struct work_struct work;
	struct fimd_context *ctx;
	bool enable_lpm;
	bool set_vblank;
};

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
struct fimd_secure_work {
	struct work_struct work;
	struct fimd_context *ctx;
	bool secure_mode;
};
#endif

struct fimd_context {
	struct exynos_drm_subdrv	subdrv;
	struct device			*dev;
	struct drm_device		*drm_dev;
	struct device			*smies_device;
	int				irq;
	struct drm_crtc			*crtc;
	struct clk			*bus_clk;
	struct clk			*lcd_clk;
	struct resource			*regs_res;
	void __iomem			*regs;
	struct fimd_win_data		win_data[WINDOWS_NR];
	unsigned int			clkdiv;
	unsigned int			default_win;
	u32				vidcon0;
	u32				vidcon1;
	u32				vidcon3;
	u32				*reg_gamma;
	u32				reg_gain;
	u32				i80ifcon;
	bool				i80_if;
	bool				iommu_on;
	bool				mdnie_enabled;
	int				pipe;
	struct mutex			lock;
	wait_queue_head_t		wait_vsync_queue;
	atomic_t			wait_vsync_event;
	wait_queue_head_t		wait_done_queue;
	atomic_t			wait_done_event;
	bool				no_trigger;
	atomic_t			win_update;
	int				dpms;
	atomic_t			smies_active;
	atomic_t			gamma_on;
	struct completion	lpm_comp;
	struct exynos_drm_panel_info *panel;
	struct notifier_block	nb_ctrl;
	struct pm_qos_request	pm_ovly_qos;
	struct fimd_driver_data *driver_data;
	struct platform_device		*disp_bus_pdev;
	struct dispfreq_device *dfd;
	struct workqueue_struct	*fimd_wq;
	struct fimd_lpm_work *lpm_work;
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
	struct fimd_secure_work *secure_work;
	struct completion	secure_comp;
	bool				secure_mode;
#endif
	atomic_t			do_apply;
	atomic_t			partial_requested;
	spinlock_t			win_updated_lock;
	bool				pm_gating_on;
	bool				bl_on;
	int	(*smies_on)(struct device *smies);
	int	(*smies_off)(struct device *smies);
	int	(*smies_mode)(struct device *smies, int mode);
	int				dbg_cnt;
#ifdef CONFIG_SLEEP_MONITOR
	int				act_cnt;
#endif
};

static int fimd_runtime_get_sync(struct device *dev);
static int fimd_runtime_put_sync(struct device *dev);
static void fimd_update_gamma(struct device *dev, bool enable);
static int fimd_iommu_ctrl(struct fimd_context *ctx, bool on);
static void fimd_win_enable(struct device *dev, int zpos);
static void fimd_win_disable(struct device *dev, int zpos);
static int fimd_wait_for_done(struct device *dev);
static void fimd_wait_for_vblank(struct device *dev);
static void fimd_clear_win(struct fimd_context *ctx, int win);
static void fimd_stop_trigger(struct device *dev,	bool stop);
#endif /* _EXYNOS_DRM_FIMD_H_ */
