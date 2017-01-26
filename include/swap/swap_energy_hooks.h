#ifndef _LINUX_SWAP_ENERGY_HOOKS_H
#define _LINUX_SWAP_ENERGY_HOOKS_H


#ifdef CONFIG_SWAP_ENERGY_HOOKS

#include <linux/compiler.h>

struct socket;

struct swap_energy_hooks {
	void (*bt_recvmsg)(struct socket *sock, int len);
	void (*bt_sendmsg)(struct socket *sock, int len);
	void (*wifi_recvmsg)(struct socket *sock, int len);
	void (*wifi_sendmsg)(struct socket *sock, int len);
};

extern struct swap_energy_hooks *swap_nrg_hooks;

int swap_energy_hooks_set(struct swap_energy_hooks *hooks);
void swap_energy_hooks_unset(void);


/* private interface */
struct swap_energy_hooks *swap_energy_hooks_get(void);
void swap_energy_hooks_put(void);

#define SWAP_ENERGY_HOOK_CALL(hook_name, ...)		\
	if (unlikely(swap_nrg_hooks)) {			\
		struct swap_energy_hooks *hooks = swap_energy_hooks_get();\
		if (hooks) {				\
			hooks->hook_name(__VA_ARGS__);	\
			swap_energy_hooks_put();	\
		}					\
	}

#else /* CONFIG_SWAP_ENERGY_HOOKS */

#define SWAP_ENERGY_HOOK_CALL(hook_name, ...)

#endif /* CONFIG_SWAP_ENERGY_HOOKS */

static inline void swap_bt_recvmsg(struct socket *sock, int len)
{
	SWAP_ENERGY_HOOK_CALL(bt_recvmsg, sock, len);
}

static inline void swap_bt_sendmsg(struct socket *sock, int len)
{
	SWAP_ENERGY_HOOK_CALL(bt_sendmsg, sock, len);
}

static inline void swap_wifi_recvmsg(struct socket *sock, int len)
{
	SWAP_ENERGY_HOOK_CALL(wifi_recvmsg, sock, len);
}

static inline void swap_wifi_sendmsg(struct socket *sock, int len)
{
	SWAP_ENERGY_HOOK_CALL(wifi_sendmsg, sock, len);
}

#endif /* _LINUX_SWAP_ENERGY_HOOKS_H */
