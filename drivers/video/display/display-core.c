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

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>

#include <video/display.h>
#include <video/videomode.h>

static struct video_source *video_source_bind(struct display_entity *entity);
static void video_source_unbind(struct display_entity *entity);

/* -----------------------------------------------------------------------------
 * Display Entity
 */

static LIST_HEAD(display_entity_list);
static DEFINE_MUTEX(display_entity_mutex);

struct display_entity *display_entity_get_first(void)
{
	/* FIXME: Don't we need some locking here? */

	if (list_empty(&display_entity_list))
		return NULL;

	return list_first_entry(&display_entity_list, struct display_entity,
			list);
}
EXPORT_SYMBOL(display_entity_get_first);

int display_entity_set_state(struct display_entity *entity,
			     enum display_entity_state state)
{
	int ret;

	if (entity->state == state)
		return 0;

	if (!entity->ops || !entity->ops->set_state)
		return 0;

	ret = entity->ops->set_state(entity, state);
	if (ret < 0)
		return ret;

	entity->state = state;
	return 0;
}
EXPORT_SYMBOL_GPL(display_entity_set_state);

int display_entity_get_modes(struct display_entity *entity,
			     const struct videomode **modes)
{
	if (!entity->ops || !entity->ops->get_modes)
		return 0;

	return entity->ops->get_modes(entity, modes);
}
EXPORT_SYMBOL_GPL(display_entity_get_modes);

int display_entity_get_size(struct display_entity *entity,
			    unsigned int *width, unsigned int *height)
{
	if (!entity->ops || !entity->ops->get_size)
		return -EOPNOTSUPP;

	return entity->ops->get_size(entity, width, height);
}
EXPORT_SYMBOL_GPL(display_entity_get_size);

int display_entity_get_params(struct display_entity *entity,
			      struct display_entity_interface_params *params)
{
	if (!entity->ops || !entity->ops->get_params)
		return -EOPNOTSUPP;

	return entity->ops->get_params(entity, params);
}
EXPORT_SYMBOL_GPL(display_entity_get_params);

static void display_entity_release(struct kref *ref)
{
	struct display_entity *entity =
		container_of(ref, struct display_entity, ref);

	if (entity->release)
		entity->release(entity);
}

struct display_entity *display_entity_get(struct display_entity *entity)
{
	if (entity == NULL)
		return NULL;

	kref_get(&entity->ref);
	return entity;
}
EXPORT_SYMBOL_GPL(display_entity_get);

void display_entity_put(struct display_entity *entity)
{
	kref_put(&entity->ref, display_entity_release);
}
EXPORT_SYMBOL_GPL(display_entity_put);

int __must_check __display_entity_register(struct display_entity *entity,
					   struct module *owner)
{
	struct video_source *src;

	kref_init(&entity->ref);
	entity->owner = owner;
	entity->state = DISPLAY_ENTITY_STATE_OFF;
	entity->source = NULL;

	src = video_source_bind(entity);
	if (!src)
		return -EPROBE_DEFER;

	mutex_lock(&display_entity_mutex);
	list_add(&entity->list, &display_entity_list);
	mutex_unlock(&display_entity_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(__display_entity_register);

void display_entity_unregister(struct display_entity *entity)
{
	video_source_unbind(entity);

	mutex_lock(&display_entity_mutex);

	list_del(&entity->list);
	mutex_unlock(&display_entity_mutex);

	display_entity_put(entity);
}
EXPORT_SYMBOL_GPL(display_entity_unregister);

/* -----------------------------------------------------------------------------
 * Video Source
 */

static LIST_HEAD(video_source_list);
static DEFINE_MUTEX(video_source_mutex);

static void video_source_release(struct kref *ref)
{
	struct video_source *src =
		container_of(ref, struct video_source, ref);

	if (src->release)
		src->release(src);
}

static struct video_source *video_source_get(struct video_source *src)
{
	if (src == NULL)
		return NULL;

	kref_get(&src->ref);
	if (!try_module_get(src->owner)) {
		kref_put(&src->ref, video_source_release);
		return NULL;
	}

	return src;
}

static void video_source_put(struct video_source *src)
{
	module_put(src->owner);
	kref_put(&src->ref, video_source_release);
}

int __must_check __video_source_register(struct video_source *src,
							struct module *owner)
{
	kref_init(&src->ref);
	src->owner = owner;

	mutex_lock(&video_source_mutex);
	list_add(&src->list, &video_source_list);

	mutex_unlock(&video_source_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(__video_source_register);

void video_source_unregister(struct video_source *src)
{
	mutex_lock(&video_source_mutex);

	list_del(&src->list);
	mutex_unlock(&video_source_mutex);

	kref_put(&src->ref, video_source_release);
}
EXPORT_SYMBOL_GPL(video_source_unregister);

static struct video_source *video_source_bind(struct display_entity *entity)
{
	struct video_source *src = NULL;
	int ret;

	if (entity->source)
		return entity->source;

	mutex_lock(&video_source_mutex);

	if (entity->of_node) {
		struct device_node *np;

		np = of_parse_phandle(entity->of_node, "video-source", 0);
		if (!np)
			goto unlock;

		list_for_each_entry(src, &video_source_list, list) {
			if (src->of_node == np)
				goto found;
		}

		src = NULL;
		goto unlock;
	}

	if (!entity->src_name)
		goto unlock;

	list_for_each_entry(src, &video_source_list, list) {
		if (src->id != entity->src_id)
			continue;
		if (!strcmp(src->name, entity->src_name))
			goto found;
	}

	src = NULL;
	goto unlock;

found:
	video_source_get(src);

	if (src->common_ops->bind) {
		ret = src->common_ops->bind(src, entity);
		if (ret != 0) {
			video_source_put(src);
			src = NULL;
			goto unlock;
		}
	}

	src->sink = entity;
	entity->source = src;

unlock:
	mutex_unlock(&video_source_mutex);

	return src;
}

static void video_source_unbind(struct display_entity *entity)
{
	struct video_source *src = entity->source;

	if (!src)
		return;

	if (src->common_ops && src->common_ops->unbind)
		src->common_ops->unbind(src, entity);

	src->sink = NULL;
	entity->source = NULL;

	video_source_put(src);
}

MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_DESCRIPTION("Display Core");
MODULE_LICENSE("GPL");
