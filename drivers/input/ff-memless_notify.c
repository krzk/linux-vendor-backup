/*
 *  linux/drivers/input/ff_memless_notify.c
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

#include <linux/notifier.h>
#include <linux/export.h>


static struct srcu_notifier_head ff_memless_notifier_list;

/**
 *	ff_memless_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int ff_memless_register_client(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&ff_memless_notifier_list, nb);
}
EXPORT_SYMBOL(ff_memless_register_client);

/**
 *	ff_memless_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int ff_memless_unregister_client(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&ff_memless_notifier_list, nb);
}
EXPORT_SYMBOL(ff_memless_unregister_client);

/**
 * fb_memless_notifier_call_chain - notify clients of ff_memless effect
 *
 */
int ff_memless_notifier_call_chain(unsigned long val, void *v)
{
	return srcu_notifier_call_chain(&ff_memless_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(ff_memless_notifier_call_chain);

static int __init init_ff_memless_notifier_list(void)
{
	srcu_init_notifier_head(&ff_memless_notifier_list);
	return 0;
}
pure_initcall(init_ff_memless_notifier_list);
