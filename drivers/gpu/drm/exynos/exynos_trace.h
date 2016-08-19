/* exynos_trace.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#if !defined(_EXYNOS_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _EXYNOS_TRACE_H_

#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/fence.h>

#include <drm/drmP.h>
#include "exynos_drm_drv.h"

#undef TRACE_SYSTEM
#define TRACE_SYSTEM exynos

#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE exynos_trace

#ifdef CONFIG_DRM_DMA_SYNC
DECLARE_EVENT_CLASS(exynos_fence,
	    TP_PROTO(struct exynos_drm_crtc *crtc,
			struct exynos_drm_plane *plane),

	    TP_ARGS(crtc, plane),

	    TP_STRUCT__entry(
		    __field(u32, seqno)
		    __field(u32, context)
		    __field(u32, count)
		    __field(u32, type)
		    __field(u32, zpos)
		    ),

	    TP_fast_assign(
		    __entry->seqno = plane->pending_fence->seqno;
		    __entry->context = plane->pending_fence->context;
		    __entry->count = atomic_read(&plane->rcb.count);
		    __entry->type = crtc->type;
		    __entry->zpos = plane->zpos;
		    ),

	    TP_printk("fence(%d) rcb(%d), crtc type(%d), plane zpos(%d)",
		    __entry->seqno, __entry->count,
		    __entry->type, __entry->zpos)
);

DEFINE_EVENT(exynos_fence, exynos_cb_add,
	    TP_PROTO(struct exynos_drm_crtc *crtc,
			struct exynos_drm_plane *plane),

	    TP_ARGS(crtc, plane)
);

DEFINE_EVENT(exynos_fence, exynos_add_shared_fence,
	    TP_PROTO(struct exynos_drm_crtc *crtc,
			struct exynos_drm_plane *plane),

	    TP_ARGS(crtc, plane)
);

DEFINE_EVENT(exynos_fence, exynos_cb_done,
	    TP_PROTO(struct exynos_drm_crtc *crtc,
			struct exynos_drm_plane *plane),

	    TP_ARGS(crtc, plane)
);

DEFINE_EVENT(exynos_fence, exynos_cb_fast_path,
	    TP_PROTO(struct exynos_drm_crtc *crtc,
			struct exynos_drm_plane *plane),

	    TP_ARGS(crtc, plane)
);

DEFINE_EVENT(exynos_fence, exynos_update_cb,
	    TP_PROTO(struct exynos_drm_crtc *crtc,
			struct exynos_drm_plane *plane),

	    TP_ARGS(crtc, plane)
);
#endif

DECLARE_EVENT_CLASS(exynos_modeset,
	    TP_PROTO(struct exynos_drm_crtc *crtc),

	    TP_ARGS(crtc),

	    TP_STRUCT__entry(
		    __field(u32, pipe)
		    __field(u32, type)
		    ),

	    TP_fast_assign(
		    __entry->pipe = crtc->pipe;
		    __entry->type = crtc->type;
		    ),

	    TP_printk("crtc pipe = %d, crtc type = %d",
		    __entry->pipe, __entry->type)
);


DEFINE_EVENT(exynos_modeset, exynos_finish_vsync,
	    TP_PROTO(struct exynos_drm_crtc *crtc),

	    TP_ARGS(crtc)
);

DEFINE_EVENT(exynos_modeset, exynos_request_pageflip,
	    TP_PROTO(struct exynos_drm_crtc *crtc),

	    TP_ARGS(crtc)
);

#endif /* _EXYNOS_TRACE_H_ */

/* This part must be outside protection */
#include <trace/define_trace.h>
