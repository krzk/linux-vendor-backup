/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

enum dmabuf_sync_profile_type {
	DMABUF_SYNC_PROFILE_TRY_LOCK	= 1,
	DMABUF_SYNC_PROFILE_LOCK,
	DMABUF_SYNC_PROFILE_START,
	DMABUF_SYNC_PROFILE_END,
	DMABUF_SYNC_PROFILE_UNLOCK
};

#ifdef CONFIG_DMABUF_SYNC_PROFILE
void dmabuf_sync_profile_collect(struct dmabuf_sync *sync, unsigned int type);

void dmabuf_sync_profile_collect_single(struct dma_buf *dmabuf,
					unsigned int type, unsigned int owner);
#else
static inline void dmabuf_sync_profile_collect(struct dmabuf_sync *sync,
						unsigned int type)
{
}

static inline void dmabuf_sync_profile_collect_single(struct dma_buf *dmabuf,
					unsigned int type, unsigned int owner)
{
}
#endif
