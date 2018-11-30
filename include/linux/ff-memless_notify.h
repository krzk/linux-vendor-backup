/*
 *  linux/include/lnux/ff-memless_notify.h
 *
 * Copyright (C) 2018 SAMSUNG, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#ifndef _FF_INPUT_MEMLESS_NOTIFY_H
#define _FF_INPUT_MEMLESS_NOTIFY_H

#include <linux/input.h>

#define FF_MEMLESS_EVENT_PLAY	0x01
#define FF_MEMLESS_EVENT_STOP	0x02

#ifdef CONFIG_INPUT_FF_MEMLESS_NOTIFY
extern int ff_memless_register_client(struct notifier_block *nb);
extern int ff_memless_unregister_client(struct notifier_block *nb);
extern int ff_memless_notifier_call_chain(unsigned long val, void *v);
#else
static inline int ff_memless_register_client(struct notifier_block *nb)
{
	return 0;
};

static inline int ff_memless_unregister_client(struct notifier_block *nb)
{
	return 0;
};

static inline int ff_memless_notifier_call_chain(unsigned long val, void *v)
{
	return 0;
};
#endif
#endif /* _FF_INPUT_MEMLESS_NOTIFY_H */
