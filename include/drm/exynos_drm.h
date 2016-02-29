/* exynos_drm.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _EXYNOS_DRM_H_
#define _EXYNOS_DRM_H_

#include "drm.h"
#include <linux/fb.h>

/**
 * User-desired buffer creation information structure.
 *
 * @size: user-desired memory allocation size.
 *	- this size value would be page-aligned internally.
 * @flags: user request for setting memory type or cache attributes.
 * @handle: returned a handle to created gem object.
 *	- this handle will be set by gem module of kernel side.
 */
struct drm_exynos_gem_create {
	uint64_t size;
	unsigned int flags;
	unsigned int handle;
};

/**
 * A structure for getting buffer offset.
 *
 * @handle: a pointer to gem object created.
 * @pad: just padding to be 64-bit aligned.
 * @offset: relatived offset value of the memory region allocated.
 *	- this value should be set by user.
 */
struct drm_exynos_gem_map_off {
	unsigned int handle;
	unsigned int pad;
	uint64_t offset;
};

/**
 * A structure for mapping buffer.
 *
 * @handle: a handle to gem object created.
 * @pad: just padding to be 64-bit aligned.
 * @size: memory size to be mapped.
 * @mapped: having user virtual address mmaped.
 *	- this variable would be filled by exynos gem module
 *	of kernel side with user virtual address which is allocated
 *	by do_mmap().
 */
struct drm_exynos_gem_mmap {
	unsigned int handle;
	unsigned int pad;
	uint64_t size;
	uint64_t mapped;
};

/**
 * A structure to gem information.
 *
 * @handle: a handle to gem object created.
 * @flags: flag value including memory type and cache attribute and
 *	this value would be set by driver.
 * @size: size to memory region allocated by gem and this size would
 *	be set by driver.
 */
struct drm_exynos_gem_info {
	unsigned int handle;
	unsigned int flags;
	uint64_t size;
};

/**
 * A structure for user connection request of virtual display.
 *
 * @connection: indicate whether doing connetion or not by user.
 * @extensions: if this value is 1 then the vidi driver would need additional
 *	128bytes edid data.
 * @edid: the edid data pointer from user side.
 */
struct drm_exynos_vidi_connection {
	unsigned int connection;
	unsigned int extensions;
	uint64_t edid;
};

/* Indicate exynos specific vblank flags */
enum e_drm_exynos_vblank {
	_DRM_VBLANK_EXYNOS_VIDI	= 2,
};

/* indicate cache units. */
enum e_drm_exynos_gem_cache_sel {
	EXYNOS_DRM_L1_CACHE		= 1 << 0,
	EXYNOS_DRM_L2_CACHE		= 1 << 1,
	EXYNOS_DRM_ALL_CORES		= 1 << 2,
	EXYNOS_DRM_ALL_CACHES		= EXYNOS_DRM_L1_CACHE |
						EXYNOS_DRM_L2_CACHE,
	EXYNOS_DRM_ALL_CACHES_CORES	= EXYNOS_DRM_L1_CACHE |
						EXYNOS_DRM_L2_CACHE |
						EXYNOS_DRM_ALL_CORES,
	EXYNOS_DRM_CACHE_SEL_MASK	= EXYNOS_DRM_ALL_CACHES_CORES
};

/* indicate cache operation types. */
enum e_drm_exynos_gem_cache_op {
	EXYNOS_DRM_CACHE_INV_ALL	= 1 << 3,
	EXYNOS_DRM_CACHE_INV_RANGE	= 1 << 4,
	EXYNOS_DRM_CACHE_CLN_ALL	= 1 << 5,
	EXYNOS_DRM_CACHE_CLN_RANGE	= 1 << 6,
	EXYNOS_DRM_CACHE_FSH_ALL	= EXYNOS_DRM_CACHE_INV_ALL |
						EXYNOS_DRM_CACHE_CLN_ALL,
	EXYNOS_DRM_CACHE_FSH_RANGE	= EXYNOS_DRM_CACHE_INV_RANGE |
						EXYNOS_DRM_CACHE_CLN_RANGE,
	EXYNOS_DRM_CACHE_OP_MASK	= EXYNOS_DRM_CACHE_FSH_ALL |
						EXYNOS_DRM_CACHE_FSH_RANGE
};

/* memory type definitions. */
enum e_drm_exynos_gem_mem_type {
	/* Physically Continuous memory and used as default. */
	EXYNOS_BO_CONTIG	= 0 << 0,
	/* Physically Non-Continuous memory. */
	EXYNOS_BO_NONCONTIG	= 1 << 0,
	/* non-cachable mapping and used as default. */
	EXYNOS_BO_NONCACHABLE	= 0 << 1,
	/* cachable mapping. */
	EXYNOS_BO_CACHABLE	= 1 << 1,
	/* write-combine mapping. */
	EXYNOS_BO_WC		= 1 << 2,
	EXYNOS_BO_MASK		= EXYNOS_BO_NONCONTIG | EXYNOS_BO_CACHABLE |
					EXYNOS_BO_WC
};

/**
 * A structure for cache operation.
 *
 * @usr_addr: user space address.
 *	P.S. it SHOULD BE user space.
 * @size: buffer size for cache operation.
 * @flags: select cache unit and cache operation.
 * @gem_handle: a handle to a gem object.
 *	this gem handle is needed for cache range operation to L2 cache.
 */
struct drm_exynos_gem_cache_op {
	uint64_t usr_addr;
	unsigned int size;
	unsigned int flags;
	unsigned int gem_handle;
};

struct drm_exynos_g2d_get_ver {
	__u32	major;
	__u32	minor;
};

struct drm_exynos_g2d_cmd {
	unsigned long offset;
	unsigned long data;
};

enum drm_exynos_g2d_buf_type {
	G2D_BUF_USERPTR = 1 << 31,
};

enum drm_exynos_g2d_event_type {
	G2D_EVENT_NOT,
	G2D_EVENT_NONSTOP,
	G2D_EVENT_STOP,		/* not yet */
};

struct drm_exynos_g2d_userptr {
	unsigned long userptr;
	unsigned long size;
};

struct drm_exynos_g2d_set_cmdlist {
	__u64					cmd;
	__u64					cmd_gem;
	__u32					cmd_nr;
	__u32					cmd_gem_nr;

	/* for g2d event */
	__u64					event_type;
	__u64					user_data;
};

struct drm_exynos_g2d_exec {
	__u32					async;
	__u32					reserved;
};

enum drm_exynos_g2d_lock_mode_type {
	G2D_LOCK_MODE_NORMAL,
	G2D_LOCK_MODE_SECURE
};

/**
 * A structure for lock mode operation.
 *
 * @flags: select lock mode operation.
 */
struct drm_exynos_g2d_lock_mode {
	__u32	flags;
	__u32	reserved;
};

/* define of color range */
enum drm_exynos_color_range {
	COLOR_RANGE_LIMITED,
	COLOR_RANGE_FULL,
};

enum drm_exynos_ops_id {
	EXYNOS_DRM_OPS_SRC,
	EXYNOS_DRM_OPS_DST,
	EXYNOS_DRM_OPS_MAX,
};

struct drm_exynos_sz {
	__u32	hsize;
	__u32	vsize;
};

struct drm_exynos_pos {
	__u32	x;
	__u32	y;
	__u32	w;
	__u32	h;
};

enum drm_exynos_flip {
	EXYNOS_DRM_FLIP_NONE = (0 << 0),
	EXYNOS_DRM_FLIP_VERTICAL = (1 << 0),
	EXYNOS_DRM_FLIP_HORIZONTAL = (1 << 1),
	EXYNOS_DRM_FLIP_BOTH = EXYNOS_DRM_FLIP_VERTICAL |
			EXYNOS_DRM_FLIP_HORIZONTAL,
};

enum drm_exynos_degree {
	EXYNOS_DRM_DEGREE_0,
	EXYNOS_DRM_DEGREE_90,
	EXYNOS_DRM_DEGREE_180,
	EXYNOS_DRM_DEGREE_270,
};

enum drm_exynos_planer {
	EXYNOS_DRM_PLANAR_Y,
	EXYNOS_DRM_PLANAR_CB,
	EXYNOS_DRM_PLANAR_CR,
	EXYNOS_DRM_PLANAR_MAX,
};

enum drm_exynos_ipp_cmd {
	IPP_CMD_NONE,
	IPP_CMD_M2M,
	IPP_CMD_WB,
	IPP_CMD_OUTPUT,
	IPP_CMD_MAX,
};

/**
 * A structure for ipp capability.
 *
 * @writeback: flag of writeback supporting.
 * @flip: flag of flip supporting.
 * @degree: flag of degree information.
 * @csc: flag of csc supporting.
 * @crop: flag of crop supporting.
 * @scale: flag of scale supporting.
 * @blending: flag of blending supporting.
 * @dithering: flag of dithering supporting.
 * @colorfill: flag of colorfill supporting.
 * @refresh_min: min hz of refresh.
 * @refresh_max: max hz of refresh.
 * @crop_min: crop min resolution.
 * @crop_max: crop max resolution.
 * @scale_min: scale min resolution.
 * @scale_max: scale max resolution.
 */
struct drm_exynos_ipp_capability {
	__u32	writeback;
	__u32	flip;
	__u32	degree;
	__u32	csc;
	__u32	crop;
	__u32	scale;
	__u32	blending;
	__u32	dithering;
	__u32	colorfill;
	__u32	refresh_min;
	__u32	refresh_max;
	struct drm_exynos_sz	crop_min;
	struct drm_exynos_sz	crop_max;
	struct drm_exynos_sz	scale_min;
	struct drm_exynos_sz	scale_max;
};

/**
 * A structure for ipp supported property list.
 *
 * @version: version of this structure.
 * @num_drv: quantity of ipp drivers.
 * @ipp_id: id of ipp driver.
 * @link_count: link count of current ipp driver.
 * @cur_cmd: current command mode.
 * @capability: capability of ipp driver.
 */

struct drm_exynos_ipp_prop_list {
	__u32	version;
	__u32	num_drv;
	__u32	ipp_id;
	__u32	link_count;
	enum drm_exynos_ipp_cmd	cur_cmd;
	struct drm_exynos_ipp_capability capability;
};

/**
 * A structure for ipp config.
 *
 * @ops_id: property of operation directions.
 * @flip: property of mirror, flip.
 * @degree: property of rotation degree.
 * @fmt: property of image format.
 * @sz: property of image size.
 * @pos: property of image position(src-cropped,dst-scaler).
 */
struct drm_exynos_ipp_config {
	enum drm_exynos_ops_id ops_id;
	enum drm_exynos_flip	flip;
	enum drm_exynos_degree	degree;
	__u32	fmt;
	struct drm_exynos_sz	sz;
	struct drm_exynos_pos	pos;
};

/* define of blending operation */
enum drm_exynos_ipp_blending {
	IPP_BLENDING_NO,
	/* [0, 0] */
	IPP_BLENDING_CLR,
	/* [Sa, Sc] */
	IPP_BLENDING_SRC,
	/* [Da, Dc] */
	IPP_BLENDING_DST,
	/* [Sa + (1 - Sa)*Da, Rc = Sc + (1 - Sa)*Dc] */
	IPP_BLENDING_SRC_OVER,
	/* [Sa + (1 - Sa)*Da, Rc = Dc + (1 - Da)*Sc] */
	IPP_BLENDING_DST_OVER,
	/* [Sa * Da, Sc * Da] */
	IPP_BLENDING_SRC_IN,
	/* [Sa * Da, Sa * Dc] */
	IPP_BLENDING_DST_IN,
	/* [Sa * (1 - Da), Sc * (1 - Da)] */
	IPP_BLENDING_SRC_OUT,
	/* [Da * (1 - Sa), Dc * (1 - Sa)] */
	IPP_BLENDING_DST_OUT,
	/* [Da, Sc * Da + (1 - Sa) * Dc] */
	IPP_BLENDING_SRC_ATOP,
	/* [Sa, Sc * (1 - Da) + Sa * Dc ] */
	IPP_BLENDING_DST_ATOP,
	/* [-(Sa * Da), Sc * (1 - Da) + (1 - Sa) * Dc] */
	IPP_BLENDING_XOR,
	/* [Sa + Da - Sa*Da, Sc*(1 - Da) + Dc*(1 - Sa) + min(Sc, Dc)] */
	IPP_BLENDING_DARKEN,
	/* [Sa + Da - Sa*Da, Sc*(1 - Da) + Dc*(1 - Sa) + max(Sc, Dc)] */
	IPP_BLENDING_LIGHTEN,
	/* [Sa * Da, Sc * Dc] */
	IPP_BLENDING_MULTIPLY,
	/* [Sa + Da - Sa * Da, Sc + Dc - Sc * Dc] */
	IPP_BLENDING_SCREEN,
	/* Saturate(S + D) */
	IPP_BLENDING_ADD,
	/* Max */
	IPP_BLENDING_MAX,
};

/* define of dithering operation */
enum drm_exynos_ipp_dithering {
	IPP_DITHERING_NO,
	IPP_DITHERING_8BIT,
	IPP_DITHERING_6BIT,
	IPP_DITHERING_5BIT,
	IPP_DITHERING_4BIT,
	IPP_DITHERING_MAX,
};

/* define of ipp operation type */
enum drm_exynos_ipp_type {
	IPP_SYNC_WORK = 0x0,
	IPP_EVENT_DRIVEN = 0x1,
	IPP_TYPE_MAX = 0x2,
};

/**
 * A structure for ipp property.
 *
 * @config: source, destination config.
 * @cmd: definition of command.
 * @ipp_id: id of ipp driver.
 * @prop_id: id of property.
 * @refresh_rate: refresh rate.
 * @protect: protection enable flag.
 * @range: color space converting range.
 * @type: definition of operation type.
 * @blending: blending opeation config.
 * @dithering: dithering opeation config.
 * @color_fill: color fill value.
 */
struct drm_exynos_ipp_property {
	struct drm_exynos_ipp_config config[EXYNOS_DRM_OPS_MAX];
	enum drm_exynos_ipp_cmd	cmd;
	__u32	ipp_id;
	__u32	prop_id;
	__u32	refresh_rate;
	__u32	protect;
	enum drm_exynos_color_range range;
	enum drm_exynos_ipp_type	type;
	enum drm_exynos_ipp_blending blending;
	enum drm_exynos_ipp_dithering	dithering;
	__u32	color_fill;
};

enum drm_exynos_ipp_buf_type {
	IPP_BUF_ENQUEUE,
	IPP_BUF_DEQUEUE,
};

/**
 * A structure for ipp buffer operations.
 *
 * @ops_id: operation directions.
 * @buf_type: definition of buffer.
 * @prop_id: id of property.
 * @buf_id: id of buffer.
 * @handle: Y, Cb, Cr each planar handle.
 * @user_data: user data.
 */
struct drm_exynos_ipp_queue_buf {
	enum drm_exynos_ops_id	ops_id;
	enum drm_exynos_ipp_buf_type	buf_type;
	__u32	prop_id;
	__u32	buf_id;
	__u32	handle[EXYNOS_DRM_PLANAR_MAX];
	__u32	reserved;
	__u64	user_data;
};

enum drm_exynos_ipp_ctrl {
	IPP_CTRL_PLAY,
	IPP_CTRL_STOP,
	IPP_CTRL_PAUSE,
	IPP_CTRL_RESUME,
	IPP_CTRL_MAX,
};

/**
 * A structure for ipp start/stop operations.
 *
 * @prop_id: id of property.
 * @ctrl: definition of control.
 */
struct drm_exynos_ipp_cmd_ctrl {
	__u32	prop_id;
	enum drm_exynos_ipp_ctrl	ctrl;
};

/* type of hdmi audio */
enum drm_exynos_hdmi_type {
	HDMI_TYPE_I2S,
	HDMI_TYPE_SPDIF,
	HDMI_TYPE_MAX,
};

/* codec of hdmi audio */
enum drm_exynos_hdmi_codec {
	HDMI_CODEC_PCM,
	HDMI_CODEC_AC3,
	HDMI_CODEC_MP3,
	HDMI_CODEC_WMA,
	HDMI_CODEC_MAX,
};

/**
 * A structure for hdmi audio enable.
 *
 * @type: audio type list.
 * @codec: audio codec list.
 * @enable: enable or disable audio.
 * @sampling_freq: sampling frequency.
	e.g 32000,44100,48000,88200,
	96000,176400,192000.
 * @channel: channel mode.
	e.g stereo-2,5.1 channel-6.
 * @bits_per_sample: bit per sample.
	e.g 16,20,24.
 */
struct drm_exynos_hdmi_audio {
	enum drm_exynos_hdmi_type	type;
	enum drm_exynos_hdmi_codec	codec;
	__u32	enable;
	__u32	sampling_freq;
	__u32	channel;
	__u32	bits_per_sample;
};

#define DRM_EXYNOS_GEM_CREATE		0x00
#define DRM_EXYNOS_GEM_MAP_OFFSET	0x01
#define DRM_EXYNOS_GEM_MMAP		0x02
#define DRM_EXYNOS_GEM_GET		0x04
#define DRM_EXYNOS_VIDI_CONNECTION	0x07

/* temporary ioctl command. */
#define DRM_EXYNOS_GEM_CACHE_OP		0x12

/* G2D */
#define DRM_EXYNOS_G2D_GET_VER		0x20
#define DRM_EXYNOS_G2D_SET_CMDLIST	0x21
#define DRM_EXYNOS_G2D_EXEC		0x22
#define DRM_EXYNOS_G2D_LOCK_MODE	0x23

/* IPP - Image Post Processing */
#define DRM_EXYNOS_IPP_GET_PROPERTY	0x30
#define DRM_EXYNOS_IPP_SET_PROPERTY	0x31
#define DRM_EXYNOS_IPP_QUEUE_BUF	0x32
#define DRM_EXYNOS_IPP_CMD_CTRL	0x33

/* HDMI - Audio */
#define DRM_EXYNOS_HDMI_AUDIO		0x40

#define DRM_EXYNOS_DPMS		0x50

#define DRM_IOCTL_EXYNOS_GEM_CREATE		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_GEM_CREATE, struct drm_exynos_gem_create)

#define DRM_IOCTL_EXYNOS_GEM_MAP_OFFSET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_GEM_MAP_OFFSET, struct drm_exynos_gem_map_off)

#define DRM_IOCTL_EXYNOS_GEM_MMAP	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_GEM_MMAP, struct drm_exynos_gem_mmap)

#define DRM_IOCTL_EXYNOS_GEM_GET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_GEM_GET,	struct drm_exynos_gem_info)

#define DRM_IOCTL_EXYNOS_GEM_CACHE_OP	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_GEM_CACHE_OP, struct drm_exynos_gem_cache_op)

#define DRM_IOCTL_EXYNOS_VIDI_CONNECTION	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_VIDI_CONNECTION, struct drm_exynos_vidi_connection)

#define DRM_IOCTL_EXYNOS_G2D_GET_VER		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_G2D_GET_VER, struct drm_exynos_g2d_get_ver)
#define DRM_IOCTL_EXYNOS_G2D_SET_CMDLIST	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_G2D_SET_CMDLIST, struct drm_exynos_g2d_set_cmdlist)
#define DRM_IOCTL_EXYNOS_G2D_EXEC		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_G2D_EXEC, struct drm_exynos_g2d_exec)
#define DRM_IOCTL_EXYNOS_G2D_LOCK_MODE		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_G2D_LOCK_MODE, struct drm_exynos_g2d_lock_mode)

#define DRM_IOCTL_EXYNOS_IPP_GET_PROPERTY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_IPP_GET_PROPERTY, struct drm_exynos_ipp_prop_list)
#define DRM_IOCTL_EXYNOS_IPP_SET_PROPERTY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_IPP_SET_PROPERTY, struct drm_exynos_ipp_property)
#define DRM_IOCTL_EXYNOS_IPP_QUEUE_BUF	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_IPP_QUEUE_BUF, struct drm_exynos_ipp_queue_buf)
#define DRM_IOCTL_EXYNOS_IPP_CMD_CTRL		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_IPP_CMD_CTRL, struct drm_exynos_ipp_cmd_ctrl)

#define DRM_IOCTL_EXYNOS_HDMI_AUDIO	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_HDMI_AUDIO, struct drm_exynos_hdmi_audio)

#define DRM_IOCTL_EXYNOS_DPMS	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_DPMS, struct drm_exynos_dpms)

/* EXYNOS specific events */
#define DRM_EXYNOS_G2D_EVENT		0x80000000
#define DRM_EXYNOS_IPP_EVENT		0x80000001
#define DRM_EXYNOS_DPMS_EVENT		0x80000002

/* define of ipp operation type */
enum drm_exynos_dpms_type {
	DPMS_SYNC_WORK = 0x0,
	DPMS_EVENT_DRIVEN = 0x1,
};

struct drm_exynos_g2d_event {
	struct drm_event	base;
	__u64			user_data;
	__u32			tv_sec;
	__u32			tv_usec;
	__u32			cmdlist_no;
	__u32			reserved;
};

struct drm_exynos_ipp_event {
	struct drm_event	base;
	__u64			user_data;
	__u32			tv_sec;
	__u32			tv_usec;
	__u32			prop_id;
	__u32			reserved;
	__u32			buf_id[EXYNOS_DRM_OPS_MAX];
};

struct drm_exynos_dpms {
	__u32			connector_id;
	__u32			dpms;
	__u32			user_data;
	enum drm_exynos_dpms_type	type;
};

struct drm_exynos_dpms_event {
	struct drm_event	base;
	__u32			connector_id;
	__u32			dpms;
	__u32			user_data;
};

#ifdef __KERNEL__

/**
 * A structure for mode information.
 *
 * @refresh: dynamic refresh list.
 * @vmode: scan information.
 */
struct exynos_drm_mode_info {
	u32 refresh;
	u32 vmode;
	u32 rotate;
};

/**
 * A structure for lcd panel information.
 *
 * @timing: default video mode for initializing
 * @width_mm: physical size of lcd width.
 * @height_mm: physical size of lcd height.
 * @mode_count: supports dynamic refresh count.
 * @self_refresh: self refresh rate of panel.
 * @mode: supports various mode.
 */
struct exynos_drm_panel_info {
	struct fb_videomode timing;
	u32 width_mm;
	u32 height_mm;
	u32 mode_count;
	u32 self_refresh;
	struct exynos_drm_mode_info *mode;
};

/**
 * Platform Specific Structure for DRM based FIMD.
 *
 * @panel: default panel info for initializing
 * @default_win: default window layer number to be used for UI.
 * @bpp: default bit per pixel.
 * @disp_bus_pdev: add interface platform device.
 * @te_gpio: tearing effect signalling gpio.
 * @update_panel_refresh: update the panel self refresh rate.
 */
struct exynos_drm_fimd_pdata {
	struct exynos_drm_panel_info *panel;
	struct device   *smies_device;
	u32				vidcon0;
	u32				vidcon1;
	u32				vidcon3;
	u32				*reg_gamma;
	u32				reg_gain;
	u32				i80ifcon;
	unsigned int			default_win;
	unsigned int			bpp;
	void	 *disp_bus_pdev;
	bool				mdnie_enabled;
	u32				max_refresh;
	u32				min_refresh;
	int (*te_handler)(struct device *dev);
	int (*smies_on)(struct device *smies);
	int (*smies_off)(struct device *smies);
	int (*smies_mode)(struct device *smies, int mode);
	void (*trigger)(struct device *dev);
	void (*wait_for_frame_done)(struct device *dev);
	void (*stop_trigger)(struct device *dev, bool stop);
	int (*set_runtime_activate)(struct device *dev, const char *str);
	int (*set_smies_activate)(struct device *dev, bool enable);
	int (*set_smies_mode)(struct device *dev, int mode);
	int (*set_gamma_acivate)(struct device *dev, bool enable);
	void (*update_panel_refresh)(struct device *dev, unsigned int rate);
};

/**
 * Platform Specific Structure for DRM based HDMI.
 *
 * @hdmi_dev: device point to specific hdmi driver.
 * @mixer_dev: device point to specific mixer driver.
 *
 * this structure is used for common hdmi driver and each device object
 * would be used to access specific device driver(hdmi or mixer driver)
 */
struct exynos_drm_common_hdmi_pd {
	struct device *hdmi_dev;
	struct device *mixer_dev;
};

/**
 * Platform Specific Structure for DRM based HDMI core.
 *
 * @is_v13: set if hdmi version 13 is.
 * @hdcp_dev: device point to specific cec driver.
 * @cfg_hpd: function pointer to configure hdmi hotplug detection pin
 * @get_hpd: function pointer to get value of hdmi hotplug detection pin
 */
struct exynos_drm_hdmi_pdata {
	bool is_v13;
	struct device *hdcp_dev;
	void (*cfg_hpd)(bool external);
	int (*get_hpd)(void);

	char *extcon_name;
};

/**
 * Platform Specific Structure for DRM based IPP.
 *
 * @inv_pclk: if set 1. invert pixel clock
 * @inv_vsync: if set 1. invert vsync signal for wb
 * @inv_href: if set 1. invert href signal
 * @inv_hsync: if set 1. invert hsync signal for wb
 */
struct exynos_drm_ipp_pol {
	unsigned int inv_pclk;
	unsigned int inv_vsync;
	unsigned int inv_href;
	unsigned int inv_hsync;
};

/**
 * Platform Specific Structure for DRM based FIMC.
 *
 * @pol: current hardware block polarity settings.
 * @clk_rate: current hardware clock rate.
 */
struct exynos_drm_fimc_pdata {
	struct exynos_drm_ipp_pol pol;
	int clk_rate;
};

#ifdef CONFIG_DISPLAY_EARLY_DPMS
enum display_early_dpms_cmd {
	DISPLAY_EARLY_DPMS_MODE_SET = 0,
	DISPLAY_EARLY_DPMS_COMMIT = 1,
	DISPLAY_EARLY_DPMS_MODE_MAX,
};

enum display_early_dpms_id {
	DISPLAY_EARLY_DPMS_ID_PRIMARY = 0,
	DISPLAY_EARLY_DPMS_ID_MAX,
};

struct display_early_dpms_nb_event {
	int id;
	void *data;
};

int display_early_dpms_nb_register(struct notifier_block *nb);
int display_early_dpms_nb_unregister(struct notifier_block *nb);
int display_early_dpms_nb_send_event(unsigned long val, void *v);
int display_early_dpms_notifier_ctrl(struct notifier_block *this,
		unsigned long cmd, void *_data);
#endif

#endif	/* __KERNEL__ */
#endif	/* _EXYNOS_DRM_H_ */
