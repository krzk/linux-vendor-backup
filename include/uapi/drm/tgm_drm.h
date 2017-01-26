/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	JinYoung Jeon <jy0.jeon@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _UAPI_TGM_DRM_H_
#define _UAPI_TGM_DRM_H_

#include "drm.h"

enum e_tbm_gem_mem_type {
	/* Physically Continuous memory and used as default. */
	TBM_BO_CONTIG	= 0 << 0,
	/* Physically Non-Continuous memory. */
	TBM_BO_NONCONTIG	= 1 << 0,
	/* non-cachable mapping and used as default. */
	TBM_BO_NONCACHABLE	= 0 << 1,
	/* cachable mapping. */
	TBM_BO_CACHABLE	= 1 << 1,
	/* write-combine mapping. */
	TBM_BO_WC		= 1 << 2,
	TBM_BO_MASK		= TBM_BO_NONCONTIG | TBM_BO_CACHABLE |
					TBM_BO_WC
};

enum tgm_planer {
	TGM_PLANAR_Y,
	TGM_PLANAR_CB,
	TGM_PLANAR_CR,
	TGM_PLANAR_MAX,
};

enum tdm_crtc_id {
	TDM_CRTC_PRIMARY,
	TDM_CRTC_VIRTUAL,
	TDM_CRTC_MAX,
};

enum tdm_dpms_type {
	DPMS_SYNC_WORK = 0x0,
	DPMS_EVENT_DRIVEN = 0x1,
};

struct tdm_control_dpms {
	enum tdm_crtc_id	crtc_id;
	__u32	dpms;
	__u32	user_data;
	enum tdm_dpms_type	type;
};

struct tdm_control_dpms_event {
	struct drm_event	base;
	enum tdm_crtc_id	crtc_id;
	__u32	dpms;
	__u32	user_data;
};

struct tbm_gem_create {
	uint64_t size;
	unsigned int flags;
	unsigned int handle;
};

struct tbm_gem_mmap {
	unsigned int handle;
	unsigned int pad;
	uint64_t size;
	uint64_t mapped;
};

struct tbm_gem_info {
	unsigned int handle;
	unsigned int flags;
	uint64_t size;
};

struct tbm_gem_cpu_access {
	unsigned int handle;
	unsigned int reserved;
};

#define DRM_TBM_GEM_CREATE		0x00
#define DRM_TBM_GEM_MMAP		0x02
#define DRM_TBM_GEM_GET		0x04
#define DRM_TBM_GEM_CPU_PREP		0x05
#define DRM_TBM_GEM_CPU_FINI		0x06
#define DRM_TDM_DPMS_CONTROL		0x50

#define DRM_IOCTL_TBM_GEM_CREATE		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_CREATE, struct tbm_gem_create)

#define DRM_IOCTL_TBM_GEM_MMAP		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_MMAP, struct tbm_gem_mmap)

#define DRM_IOCTL_TBM_GEM_GET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_GET,	struct tbm_gem_info)

#define DRM_IOCTL_TBM_GEM_CPU_PREP		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_CPU_PREP, struct tbm_gem_cpu_access)

#define DRM_IOCTL_TBM_GEM_CPU_FINI		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_CPU_FINI, struct tbm_gem_cpu_access)

#define DRM_IOCTL_TDM_DPMS_CONTROL	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TDM_DPMS_CONTROL, struct tdm_control_dpms)

#define TDM_DPMS_EVENT		0x80000002

#endif /* _UAPI_TGM_DRM_H_ */
