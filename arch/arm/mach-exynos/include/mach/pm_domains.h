/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARCH_PM_RUNTIME_H
#define __ASM_ARCH_PM_RUNTIME_H __FILE__

#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/sysmmu.h>

#include <plat/clock.h>
#include <plat/devs.h>

#define PM_DOMAIN_PREFIX	"PM DOMAIN: "

enum EXYNOS_PROCESS_ORDER {
	EXYNOS_PROCESS_BEFORE	=	0x1,
	EXYNOS_PROCESS_AFTER	=	0x2,
};

enum EXYNOS_PROCESS_TYPE {
	EXYNOS_PROCESS_ON	=	0x1,
	EXYNOS_PROCESS_OFF	=	0x2,
	EXYNOS_PROCESS_ONOFF	=	EXYNOS_PROCESS_ON | EXYNOS_PROCESS_OFF,
};

struct exynos_pm_clk {
	struct list_head node;
	struct clk *clk;
};

struct exynos_pm_reg {
	struct list_head node;
	enum EXYNOS_PROCESS_TYPE reg_type;
	void __iomem *reg;
	unsigned int value;
};

struct exynos_pm_seq_reg {
	struct list_head node;
	enum EXYNOS_PROCESS_TYPE reg_type;
	void __iomem *reg;
	const char *name;
	unsigned int value;
};

struct exynos_pm_domain;

struct exynos_pm_callback {
	struct list_head node;
	int (*callback)(struct exynos_pm_domain *domain);
};

struct exynos_pm_domain {
	struct generic_pm_domain pd;
	void __iomem *base;
	struct list_head clk_list;
	rwlock_t clk_lock;
	struct list_head reg_before_list;
	rwlock_t reg_before_lock;
	struct list_head reg_after_list;
	rwlock_t reg_after_lock;
	struct list_head callback_pre_on_list;
	rwlock_t callback_pre_on_lock;
	int (*on)(struct exynos_pm_domain *domain, int power_flags);
	struct list_head callback_post_on_list;
	rwlock_t callback_post_on_lock;
	struct list_head callback_pre_off_list;
	rwlock_t callback_pre_off_lock;
	int (*off)(struct exynos_pm_domain *domain, int power_flags);
	struct list_head callback_post_off_list;
	rwlock_t callback_post_off_lock;
	bool is_interrupt_domain;
	spinlock_t interrupt_lock;
	struct list_head power_seq_before_list;
	struct list_head power_seq_after_list;
	int (*check_status)(struct exynos_pm_domain *pd);
	ktime_t last_time;
	ktime_t start_active_time;
	ktime_t total_active_time;
};

extern void exynos_pm_powerdomain_init(struct exynos_pm_domain *domain);
extern void exynos_pm_add_subdomain(struct exynos_pm_domain *domain,			\
					struct exynos_pm_domain *subdomain,		\
					bool logical_subdomain);
extern void exynos_pm_add_dev(struct exynos_pm_domain *domain,				\
					struct device *dev);
extern void exynos_pm_add_platdev(struct exynos_pm_domain *domain,			\
					struct platform_device *pdev);
extern void exynos_pm_add_clk(struct exynos_pm_domain *domain,				\
					struct device *dev,				\
					char *con_id);
extern void exynos_pm_add_reg(struct exynos_pm_domain *domain,				\
					enum EXYNOS_PROCESS_ORDER reg_order,		\
					enum EXYNOS_PROCESS_TYPE reg_type,		\
					void __iomem *reg,				\
					unsigned int value);
extern void exynos_pm_add_pwr_seq_reg(struct exynos_pm_domain *domain,				\
					enum EXYNOS_PROCESS_ORDER reg_order,		\
					enum EXYNOS_PROCESS_TYPE reg_type,		\
					void __iomem *reg,		\
					const char *name,		\
					unsigned int value);
extern void exynos_pm_add_callback(struct exynos_pm_domain *domain,			\
					enum EXYNOS_PROCESS_ORDER callback_order,	\
					enum EXYNOS_PROCESS_TYPE callback_type,		\
					bool add_tail,					\
					int (*callback)(struct exynos_pm_domain *domain));
extern void exynos_pm_domain_set_nocallback(struct exynos_pm_domain *domain);
extern int exynos_pm_pd_status(struct exynos_pm_domain *domain);

extern int exynos_pm_genpd_power_on(struct generic_pm_domain *genpd);
extern int exynos_pm_genpd_power_off(struct generic_pm_domain *genpd);
extern int exynos_pm_domain_pre_power_control(struct exynos_pm_domain *domain);
extern int exynos_pm_domain_post_power_control(struct exynos_pm_domain *domain);
extern int exynos_pm_domain_power_control(struct exynos_pm_domain *domain, int power_flags);
struct exynos_pm_domain ** exynos_pm_get_power_domain(void);

#define EXYNOS_GPD(PD, BASE, NAME, ON, OFF, CALLBACKON, CALLBACKOFF, INTERRUPT)		\
struct exynos_pm_domain PD = {								\
	.pd		=	{							\
		.name		=	NAME,						\
		.power_off	=	CALLBACKOFF,					\
		.power_on	=	CALLBACKON,					\
	},										\
	.base			=	(void __iomem *)BASE,				\
	.clk_list		=	LIST_HEAD_INIT((PD).clk_list),			\
	.clk_lock		=	__RW_LOCK_UNLOCKED((PD).clk_lock),		\
	.reg_before_list	=	LIST_HEAD_INIT((PD).reg_before_list),		\
	.reg_before_lock	=	__RW_LOCK_UNLOCKED((PD).reg_before_lock),	\
	.reg_after_list		=	LIST_HEAD_INIT((PD).reg_after_list),		\
	.reg_after_lock		=	__RW_LOCK_UNLOCKED((PD).reg_after_lock),	\
	.callback_pre_on_list	=	LIST_HEAD_INIT((PD).callback_pre_on_list),	\
	.callback_pre_on_lock	=	__RW_LOCK_UNLOCKED((PD).callback_pre_on_lock),	\
	.on			=	ON,						\
	.callback_post_on_list	=	LIST_HEAD_INIT((PD).callback_post_on_list),	\
	.callback_post_on_lock	=	__RW_LOCK_UNLOCKED((PD).callback_post_on_lock),	\
	.callback_pre_off_list	=	LIST_HEAD_INIT((PD).callback_pre_off_list),	\
	.callback_pre_off_lock	=	__RW_LOCK_UNLOCKED((PD).callback_pre_off_lock),	\
	.off			=	OFF,						\
	.callback_post_off_list	=	LIST_HEAD_INIT((PD).callback_post_off_list),	\
	.callback_post_off_lock	=	__RW_LOCK_UNLOCKED((PD).callback_post_off_lock),\
	.is_interrupt_domain	=	INTERRUPT,					\
	.power_seq_before_list	=	LIST_HEAD_INIT((PD).power_seq_before_list),		\
	.power_seq_after_list	=	LIST_HEAD_INIT((PD).power_seq_after_list),		\
	.check_status	= exynos_pm_pd_status,		\
}

#define EXYNOS_COMMON_GPD(PD, BASE, NAME)						\
	EXYNOS_GPD(PD,									\
		BASE,									\
		NAME,									\
		exynos_pm_domain_power_control,						\
		exynos_pm_domain_power_control,						\
		exynos_pm_genpd_power_on,						\
		exynos_pm_genpd_power_off,						\
		false									\
	)

#define for_each_power_domain(index)		for(;*index ;(index++))

int exynos5_pm_domain_init(void);
#endif /* __ASM_ARCH_PM_RUNTIME_H */
