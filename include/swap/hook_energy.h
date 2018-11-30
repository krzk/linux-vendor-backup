/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2017
 *
 *    2017      Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 */


#ifndef _LINUX_SWAP_HOOK_ENERGY_H
#define _LINUX_SWAP_HOOK_ENERGY_H


#ifdef CONFIG_SWAP_HOOK_ENERGY

#include <linux/compiler.h>

struct socket;

struct swap_hook_energy {
	void (*bt_recvmsg)(struct socket *sock, int len);
	void (*bt_sendmsg)(struct socket *sock, int len);
	void (*wifi_recvmsg)(struct socket *sock, int len);
	void (*wifi_sendmsg)(struct socket *sock, int len);
};

extern struct swap_hook_energy *swap_nrg_hook;

int swap_hook_energy_set(struct swap_hook_energy *hook);
void swap_hook_energy_unset(void);


/* private interface */
struct swap_hook_energy *swap_hook_energy_get(void);
void swap_hook_energy_put(void);

#define SWAP_ENERGY_HOOK_CALL(hook_name, ...)		\
	if (unlikely(swap_nrg_hook)) {			\
		struct swap_hook_energy *hook = swap_hook_energy_get();\
		if (hook) {				\
			hook->hook_name(__VA_ARGS__);	\
			swap_hook_energy_put();	\
		}					\
	}

#else /* CONFIG_SWAP_HOOK_ENERGY */

#define SWAP_ENERGY_HOOK_CALL(hook_name, ...)

#endif /* CONFIG_SWAP_HOOK_ENERGY */

static inline void swap_bt_recvmsg(struct socket *sock, int len)
{
	SWAP_ENERGY_HOOK_CALL(bt_recvmsg, sock, len);
}

static inline void swap_bt_sendmsg(struct socket *sock, int len)
{
	SWAP_ENERGY_HOOK_CALL(bt_sendmsg, sock, len);
}

static inline void swap_sock_recvmsg(struct socket *sock, int len)
{
	/* we interested only in wifi consumption
	 * wifi_recvmsg has checking for wifi interface
	 */
	SWAP_ENERGY_HOOK_CALL(wifi_recvmsg, sock, len);
}

static inline void swap_sock_sendmsg(struct socket *sock, int len)
{
	/* we interested only in wifi consumption
	 * wifi_sendmsg has checking for wifi interface
	 */
	SWAP_ENERGY_HOOK_CALL(wifi_sendmsg, sock, len);
}

#endif /* _LINUX_SWAP_HOOK_ENERGY_H */
