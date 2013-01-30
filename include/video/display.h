/*
 * Display Core
 *
 * Copyright (C) 2012 Renesas Solutions Corp.
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/module.h>
#include <video/omapdss.h>

struct display_entity;
struct video_source;
struct videomode;

/* -----------------------------------------------------------------------------
 * Display Entity
 */

/* Hack to get the first registered display entity */
struct display_entity *display_entity_get_first(void);

enum display_entity_state {
	DISPLAY_ENTITY_STATE_OFF,
	DISPLAY_ENTITY_STATE_STANDBY,
	DISPLAY_ENTITY_STATE_ON,
};

enum display_entity_interface_type {
	DISPLAY_ENTITY_INTERFACE_DPI,
	DISPLAY_ENTITY_INTERFACE_DSI,
};

#define DSI_MODE_VIDEO			(1 << 0)
#define DSI_MODE_VIDEO_BURST		(1 << 1)
#define DSI_MODE_VIDEO_SYNC_PULSE	(1 << 2)
#define DSI_MODE_VIDEO_AUTO_VERT	(1 << 3)
#define DSI_MODE_VIDEO_HSE		(1 << 4)
#define DSI_MODE_VIDEO_HFP		(1 << 5)
#define DSI_MODE_VIDEO_HBP		(1 << 6)
#define DSI_MODE_VIDEO_HSA		(1 << 7)
#define DSI_MODE_VSYNC_FLUSH		(1 << 8)
#define DSI_MODE_EOT_PACKET		(1 << 9)

enum mipi_dsi_pixel_format {
	DSI_FMT_RGB888,
	DSI_FMT_RGB666,
	DSI_FMT_RGB666_PACKED,
	DSI_FMT_RGB565,
};

struct mipi_dsi_interface_params {
	enum mipi_dsi_pixel_format format;
	unsigned long mode;
	unsigned long hs_clk_freq;
	unsigned long esc_clk_freq;
	unsigned char data_lanes;
	unsigned char cmd_allow;
};

struct display_entity_interface_params {
	enum display_entity_interface_type type;
	union {
		struct mipi_dsi_interface_params dsi;
	} p;
};

struct display_entity_control_ops {
	int (*set_state)(struct display_entity *ent,
			 enum display_entity_state state);
	int (*update)(struct display_entity *ent,
			void (*callback)(int, void *), void *data);
	int (*get_modes)(struct display_entity *ent,
			 const struct videomode **modes);
	int (*get_params)(struct display_entity *ent,
			  struct display_entity_interface_params *params);
	int (*get_size)(struct display_entity *ent,
			unsigned int *width, unsigned int *height);
};

struct display_entity {
	struct list_head list;
	struct device *dev;
	struct device_node *of_node;
	struct module *owner;
	struct kref ref;

	const char *src_name;
	int src_id;
	struct video_source *source;

	const struct display_entity_control_ops *ops;

	void(*release)(struct display_entity *ent);

	enum display_entity_state state;
};

int display_entity_set_state(struct display_entity *entity,
			     enum display_entity_state state);
int display_entity_get_params(struct display_entity *entity,
			      struct display_entity_interface_params *params);
int display_entity_get_modes(struct display_entity *entity,
			     const struct videomode **modes);
int display_entity_get_size(struct display_entity *entity,
			    unsigned int *width, unsigned int *height);

struct display_entity *display_entity_get(struct display_entity *entity);
void display_entity_put(struct display_entity *entity);

int __must_check __display_entity_register(struct display_entity *entity,
					   struct module *owner);
void display_entity_unregister(struct display_entity *entity);

#define display_entity_register(display_entity) \
	__display_entity_register(display_entity, THIS_MODULE)


/* -----------------------------------------------------------------------------
 * Video Source
 */

enum video_source_stream_state {
	DISPLAY_ENTITY_STREAM_STOPPED,
	DISPLAY_ENTITY_STREAM_CONTINUOUS,
};

struct common_video_source_ops {
	int (*set_stream)(struct video_source *src,
			 enum video_source_stream_state state);
	int (*bind)(struct video_source *src, struct display_entity *sink);
	int (*unbind)(struct video_source *src, struct display_entity *sink);
};

struct dpi_video_source_ops {
	int (*set_videomode)(struct video_source *src,
			const struct videomode *vm);
	int (*set_data_lines)(struct video_source *src, int lines);
};

struct dsi_video_source_ops {
	/* enable/disable dsi bus */
	int (*enable)(struct video_source *src);
	int (*disable)(struct video_source *src);

	/* bus configuration */
	int (*configure_pins)(struct video_source *src,
			const struct omap_dsi_pin_config *pins);
	int (*set_clocks)(struct video_source *src,
			unsigned long ddr_clk,
			unsigned long lp_clk);
	/* NOTE: Do we really need configure_pins and set_clocks here? */

	void (*enable_hs)(struct video_source *src, bool enable);

	/* data transfer */
	int (*dcs_write)(struct video_source *src, int channel,
			const u8 *data, size_t len);
	int (*dcs_read)(struct video_source *src, int channel, u8 dcs_cmd,
			u8 *data, size_t len);
	/* NOTE: Do we need more write and read types? */

	int (*update)(struct video_source *src, int channel,
			void (*callback)(int, void *), void *data);
};

struct dvi_video_source_ops {
	int (*set_videomode)(struct video_source *src,
			const struct videomode *vm);
};

struct video_source {
	struct list_head list;
	struct device *dev;
	struct device_node *of_node;
	struct module *owner;
	struct kref ref;

	struct display_entity *sink;

	const char *name;
	int id;

	const struct common_video_source_ops *common_ops;

	union {
		const struct dpi_video_source_ops *dpi;
		const struct dsi_video_source_ops *dsi;
		const struct dvi_video_source_ops *dvi;
	} ops;

	void(*release)(struct video_source *src);
};

static inline int dsi_dcs_write(struct video_source *src, int channel,
						const u8 *data, size_t len)
{
	if (!src->ops.dsi || !src->ops.dsi->dcs_write)
		return -EINVAL;

	return src->ops.dsi->dcs_write(src, channel, data, len);
}

static inline int dsi_dcs_read(struct video_source *src, int channel,
					u8 dcs_cmd, u8 *data, size_t len)
{
	if (!src->ops.dsi || !src->ops.dsi->dcs_read)
		return -EINVAL;

	return src->ops.dsi->dcs_read(src, channel, dcs_cmd, data, len);
}


#define video_source_register(video_source) \
	__video_source_register(video_source, THIS_MODULE)

int __must_check __video_source_register(struct video_source *entity,
							struct module *owner);
void video_source_unregister(struct video_source *entity);

#endif /* __DISPLAY_H__ */
