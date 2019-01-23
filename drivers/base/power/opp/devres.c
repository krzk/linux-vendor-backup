/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019, Samsung
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/device.h>

int dev_pm_opp_add(struct device *dev, unsigned long freq,
		   unsigned long u_volt);

struct opp_table *_find_opp_table(struct device *dev);
struct opp_table *allocate_opp_table(struct device *dev);
int _opp_add_v1(struct opp_table *opp_table, struct device *dev,
		unsigned long freq, long u_volt, bool dynamic);
void dev_pm_opp_put_opp_table(struct opp_table *opp_table);
void _dev_pm_opp_remove_table(struct opp_table *opp_table, struct device *dev,
			      bool remove_all);
void dev_pm_opp_remove(struct device *dev, unsigned long freq);

static void _devm_pm_remove_opp_table(struct device *dev, void *res)
{
	struct opp_table *opp_table;

	dev_dbg(dev, "OPP_DEVRES removing opp table\n");
	opp_table = _find_opp_table(dev);
	/* Check if someone did not remove the opp_table, i.e. by calling
	 * dev_pm_opp_remove() for each OPP. */
	if (IS_ERR(opp_table))
		return;

	_dev_pm_opp_remove_table(opp_table, dev, true);

	dev_pm_opp_put_opp_table(opp_table);
}


static struct opp_table *_devm_pm_get_opp_table(struct device *dev)
{
	struct opp_table **ptr, *opp_table;

	ptr = devres_alloc(_devm_pm_remove_opp_table, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	opp_table = _find_opp_table(dev);
	if (IS_ERR(opp_table)) {
		dev_dbg(dev, "OPP_DEVRES allocating opp table\n");
		opp_table = allocate_opp_table(dev);
		if (!opp_table) {
			devres_free(ptr);
			return opp_table;
		} else {
			*ptr = opp_table;
			devres_add(dev, ptr);
		}
	}

	dev_dbg(dev, "OPP_DEVRES get opp table\n");
	return opp_table;
}

int devm_pm_opp_add(struct device *dev, unsigned long freq,
		    unsigned long u_volt)
{
	int res = 0;
	struct opp_table *opp_table;
	int ret;

	dev_dbg(dev, "OPP_DEVRES adding OPP\n");

	opp_table = _devm_pm_get_opp_table(dev);
	if (!opp_table)
		return -ENOMEM;

	ret = _opp_add_v1(opp_table, dev, freq, u_volt, true);

	dev_pm_opp_put_opp_table(opp_table);

	return res;
}
EXPORT_SYMBOL_GPL(devm_pm_opp_add);

void devm_pm_opp_remove(struct device *dev, unsigned long freq)
{
	dev_pm_opp_remove(dev, freq);
}
EXPORT_SYMBOL_GPL(devm_pm_opp_remove);
