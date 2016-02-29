/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * File            : trustedui.c
 * Created         : 26-02-2010
 */

#include <linux/spinlock.h>
#include <linux/module.h>
#include <t-base-tui.h>

static int trustedui_mode = TRUSTEDUI_MODE_OFF;
static int trustedui_blank_counter = 0;

static DEFINE_SPINLOCK(trustedui_lock);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
static BLOCKING_NOTIFIER_HEAD(trustedui_nb_list);

int trustedui_nb_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(
		&trustedui_nb_list, nb);
}

int trustedui_nb_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
		&trustedui_nb_list, nb);
}

int trustedui_nb_send_event(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(
		&trustedui_nb_list, val, v);
}
#endif

int trustedui_blank_inc(void)
{
	unsigned long flags;
	int newvalue;

	spin_lock_irqsave(&trustedui_lock, flags);
	newvalue = ++trustedui_blank_counter;
	spin_unlock_irqrestore(&trustedui_lock, flags);

	return newvalue;
}
EXPORT_SYMBOL(trustedui_blank_inc);

int trustedui_blank_dec(void)
{
	unsigned long flags;
	int newvalue;

	spin_lock_irqsave(&trustedui_lock, flags);
	newvalue = --trustedui_blank_counter;
	spin_unlock_irqrestore(&trustedui_lock, flags);

	return newvalue;
}
EXPORT_SYMBOL(trustedui_blank_dec);

int trustedui_blank_get_counter(void)
{
	unsigned long flags;
	int newvalue;

	spin_lock_irqsave(&trustedui_lock, flags);
	newvalue = trustedui_blank_counter;
	spin_unlock_irqrestore(&trustedui_lock, flags);

	return newvalue;
}
EXPORT_SYMBOL(trustedui_blank_get_counter);

void trustedui_blank_set_counter(int counter)
{
	unsigned long flags;

	spin_lock_irqsave(&trustedui_lock, flags);
	trustedui_blank_counter = counter;
	spin_unlock_irqrestore(&trustedui_lock, flags);
}
EXPORT_SYMBOL(trustedui_blank_set_counter);

int trustedui_get_current_mode(void)
{
	unsigned long flags;
	int mode;

	spin_lock_irqsave(&trustedui_lock, flags);
	mode = trustedui_mode;
	spin_unlock_irqrestore(&trustedui_lock, flags);

	return mode;
}
EXPORT_SYMBOL(trustedui_get_current_mode);

void trustedui_set_mode(int mode)
{
	unsigned long flags;

	spin_lock_irqsave(&trustedui_lock, flags);
	pr_info("%s:mode[%d->%d]\n", __func__, trustedui_mode, mode);
	if (trustedui_mode == mode) {
		spin_unlock_irqrestore(&trustedui_lock, flags);
		return;
	}
	trustedui_mode = mode;
	spin_unlock_irqrestore(&trustedui_lock, flags);
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
	trustedui_nb_send_event(TRUSTEDUI_MODE_CHANGE, (void *)&mode);
#endif
}
EXPORT_SYMBOL(trustedui_set_mode);


int trustedui_set_mask(int mask)
{
	unsigned long flags;
	int mode;

	spin_lock_irqsave(&trustedui_lock, flags);
	mode = trustedui_mode |= mask;
	spin_unlock_irqrestore(&trustedui_lock, flags);

	return mode;
}
EXPORT_SYMBOL(trustedui_set_mask);

int trustedui_clear_mask(int mask)
{
	unsigned long flags;
	int mode;

	spin_lock_irqsave(&trustedui_lock, flags);
	mode = trustedui_mode &= ~mask;
	spin_unlock_irqrestore(&trustedui_lock, flags);

	return mode;
}
EXPORT_SYMBOL(trustedui_clear_mask);

MODULE_AUTHOR("Trustonic Limited");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("<t-base TUI");
