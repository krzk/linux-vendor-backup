#ifndef __EXYNOS_PM_DOMAIN_H
#define __EXYNOS_PM_DOMAIN_H
#include <linux/pm_domain.h>
#include <linux/notifier.h>

enum {
	EXYNOS_PD_PRE_ON,
	EXYNOS_PD_POST_ON,
	EXYNOS_PD_PRE_OFF,
	EXYNOS_PD_POST_OFF,
};

int exynos_pd_notifier_register(struct generic_pm_domain *,
				struct notifier_block *);
void exynos_pd_notifier_unregister(struct generic_pm_domain *,
				struct notifier_block *);
#endif /* __EXYNOS_PM_DOMAIN_H */
