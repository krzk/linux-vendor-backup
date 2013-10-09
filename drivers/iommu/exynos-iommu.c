/* linux/drivers/iommu/exynos_iommu.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_EXYNOS_IOMMU_DEBUG
#define DEBUG
#endif

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/notifier.h>

#include <asm/dma-iommu.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>

/* We does not consider super section mapping (16MB) */
#define SECT_ORDER 20
#define LPAGE_ORDER 16
#define SPAGE_ORDER 12

#define SECT_SIZE (1 << SECT_ORDER)
#define LPAGE_SIZE (1 << LPAGE_ORDER)
#define SPAGE_SIZE (1 << SPAGE_ORDER)

#define SECT_MASK (~(SECT_SIZE - 1))
#define LPAGE_MASK (~(LPAGE_SIZE - 1))
#define SPAGE_MASK (~(SPAGE_SIZE - 1))

#define lv1ent_fault(sent) (((*(sent) & 3) == 0) || ((*(sent) & 3) == 3))
#define lv1ent_page(sent) ((*(sent) & 3) == 1)
#define lv1ent_section(sent) ((*(sent) & 3) == 2)

#define lv2ent_fault(pent) ((*(pent) & 3) == 0)
#define lv2ent_small(pent) ((*(pent) & 2) == 2)
#define lv2ent_large(pent) ((*(pent) & 3) == 1)

#define section_phys(sent) (*(sent) & SECT_MASK)
#define section_offs(iova) ((iova) & ~SECT_MASK)
#define lpage_phys(pent) (*(pent) & LPAGE_MASK)
#define lpage_offs(iova) ((iova) & ~LPAGE_MASK)
#define spage_phys(pent) (*(pent) & SPAGE_MASK)
#define spage_offs(iova) ((iova) & ~SPAGE_MASK)

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & 0xFF000) >> SPAGE_ORDER)

#define NUM_LV1ENTRIES 4096
#define NUM_LV2ENTRIES 256

#define LV2TABLE_SIZE (NUM_LV2ENTRIES * sizeof(long))

#define SPAGES_PER_LPAGE (LPAGE_SIZE / SPAGE_SIZE)

#define lv2table_base(sent) (*(sent) & 0xFFFFFC00)

#define mk_lv1ent_sect(pa) ((pa) | 2)
#define mk_lv1ent_page(pa) ((pa) | 1)
#define mk_lv2ent_lpage(pa) ((pa) | 1)
#define mk_lv2ent_spage(pa) ((pa) | 2)

#define CTRL_ENABLE	0x5
#define CTRL_BLOCK	0x7
#define CTRL_DISABLE	0x0

#define CFG_LRU		0x1
#define CFG_QOS(n)	((n & 0xF) << 7)
#define CFG_MASK	0x0150FFFF /* Selecting bit 0-15, 20, 22 and 24 */
#define CFG_ACGEN	(1 << 24) /* System MMU 3.3 only */
#define CFG_SYSSEL	(1 << 22) /* System MMU 3.2 only */
#define CFG_FLPDCACHE	(1 << 20) /* System MMU 3.2+ only */
#define CFG_SHAREABLE	(1 << 12) /* System MMU 3.x only */

#define REG_MMU_CTRL		0x000
#define REG_MMU_CFG		0x004
#define REG_MMU_STATUS		0x008
#define REG_MMU_FLUSH		0x00C
#define REG_MMU_FLUSH_ENTRY	0x010
#define REG_PT_BASE_ADDR	0x014
#define REG_INT_STATUS		0x018
#define REG_INT_CLEAR		0x01C

#define REG_PAGE_FAULT_ADDR	0x024
#define REG_AW_FAULT_ADDR	0x028
#define REG_AR_FAULT_ADDR	0x02C
#define REG_DEFAULT_SLAVE_ADDR	0x030

#define REG_MMU_VERSION		0x034

#define MMU_MAJ_VER(reg)	(reg >> 28)
#define MMU_MIN_VER(reg)	((reg >> 21) & 0x7F)

#define REG_PB0_SADDR		0x04C
#define REG_PB0_EADDR		0x050
#define REG_PB1_SADDR		0x054
#define REG_PB1_EADDR		0x058

static struct kmem_cache *lv2table_kmem_cache;

static unsigned long *section_entry(unsigned long *pgtable, unsigned long iova)
{
	return pgtable + lv1ent_offset(iova);
}

static unsigned long *page_entry(unsigned long *sent, unsigned long iova)
{
	return (unsigned long *)__va(lv2table_base(sent)) + lv2ent_offset(iova);
}

enum exynos_sysmmu_inttype {
	SYSMMU_PAGEFAULT,
	SYSMMU_AR_MULTIHIT,
	SYSMMU_AW_MULTIHIT,
	SYSMMU_BUSERROR,
	SYSMMU_AR_SECURITY,
	SYSMMU_AR_ACCESS,
	SYSMMU_AW_SECURITY,
	SYSMMU_AW_PROTECTION, /* 7 */
	SYSMMU_FAULT_UNKNOWN,
	SYSMMU_FAULTS_NUM
};

static unsigned short fault_reg_offset[SYSMMU_FAULTS_NUM] = {
	REG_PAGE_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_DEFAULT_SLAVE_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_AW_FAULT_ADDR
};

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNKNOWN FAULT"
};

struct exynos_iommu_domain {
	struct list_head clients; /* list of sysmmu_drvdata.node */
	unsigned long *pgtable; /* lv1 page table, 16KB */
	short *lv2entcnt; /* free lv2 entry counter for each section */
	spinlock_t lock; /* lock for this structure */
	spinlock_t pgtablelock; /* lock for modifying page table @ pgtable */
};

struct sysmmu_drvdata {
	struct list_head node; /* entry of exynos_iommu_domain.clients */
	struct device *sysmmu;	/* System MMU's device descriptor */
	struct device *master;	/* Owner of system MMU */
	struct clk *clk;
	struct clk *clk_master;
	int activations;
	spinlock_t lock;
	struct iommu_domain *domain;
	bool runtime_active;
	bool suspended;
	unsigned long pgtable;
	void __iomem *sfrbase;
};

static bool set_sysmmu_active(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU was not active previously
	   and it needs to be initialized */
	return ++data->activations == 1;
}

static bool set_sysmmu_inactive(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU is needed to be disabled */
	BUG_ON(data->activations < 1);
	return --data->activations == 0;
}

static bool is_sysmmu_active(struct sysmmu_drvdata *data)
{
	return data->activations > 0;
}

static unsigned int __sysmmu_version(struct sysmmu_drvdata *data,
				     unsigned int *minor)
{
	unsigned long major;

	major = readl(data->sfrbase + REG_MMU_VERSION);

	if (minor)
		*minor = MMU_MIN_VER(major);

	if (MMU_MAJ_VER(major) > 3)
		return 1;

	return MMU_MAJ_VER(major);
}

static void sysmmu_unblock(void __iomem *sfrbase)
{
	__raw_writel(CTRL_ENABLE, sfrbase + REG_MMU_CTRL);
}

static bool sysmmu_block(void __iomem *sfrbase)
{
	int i = 120;

	__raw_writel(CTRL_BLOCK, sfrbase + REG_MMU_CTRL);
	while ((i > 0) && !(__raw_readl(sfrbase + REG_MMU_STATUS) & 1))
		--i;

	if (!(__raw_readl(sfrbase + REG_MMU_STATUS) & 1)) {
		sysmmu_unblock(sfrbase);
		return false;
	}

	return true;
}

static void __sysmmu_tlb_invalidate(void __iomem *sfrbase)
{
	__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
}

static void __sysmmu_tlb_invalidate_entry(void __iomem *sfrbase,
						unsigned long iova)
{
	__raw_writel((iova & SPAGE_MASK) | 1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

static void __sysmmu_set_ptbase(void __iomem *sfrbase,
				       unsigned long pgd)
{
	__raw_writel(pgd, sfrbase + REG_PT_BASE_ADDR);

	__sysmmu_tlb_invalidate(sfrbase);
}

static void __sysmmu_set_prefbuf(void __iomem *sfrbase, unsigned long base,
						unsigned long size, int idx)
{
	__raw_writel(base, sfrbase + REG_PB0_SADDR + idx * 8);
	__raw_writel(size - 1 + base,  sfrbase + REG_PB0_EADDR + idx * 8);
}

void exynos_sysmmu_set_prefbuf(struct device *dev,
				unsigned long base0, unsigned long size0,
				unsigned long base1, unsigned long size1)
{
	struct sysmmu_drvdata *data = dev_get_drvdata(dev->archdata.iommu);
	unsigned long flags;

	BUG_ON((base0 + size0) <= base0);
	BUG_ON((size1 > 0) && ((base1 + size1) <= base1));

	spin_lock_irqsave(&data->lock, flags);
	if (!is_sysmmu_active(data))
		goto finish;

	clk_enable(data->clk_master);

	if ((readl(data->sfrbase + REG_MMU_VERSION) >> 28) == 3) {
		if (!sysmmu_block(data->sfrbase))
			goto skip;

		if (size1 == 0) {
			if (size0 <= SZ_128K) {
				base1 = base0;
				size1 = size0;
			} else {
				size1 = size0 -
					ALIGN(size0 / 2, SZ_64K);
				size0 = size0 - size1;
				base1 = base0 + size0;
			}
		}

		__sysmmu_set_prefbuf(
				data->sfrbase, base0, size0, 0);
		__sysmmu_set_prefbuf(
				data->sfrbase, base1, size1, 1);

		sysmmu_unblock(data->sfrbase);
	}
skip:
	clk_disable(data->clk_master);
finish:
	spin_unlock_irqrestore(&data->lock, flags);
}

static void show_fault_information(const char *name,
		enum exynos_sysmmu_inttype itype,
		unsigned long pgtable_base, unsigned long fault_addr)
{
	unsigned long *ent;

	if ((itype >= SYSMMU_FAULTS_NUM) || (itype < SYSMMU_PAGEFAULT))
		itype = SYSMMU_FAULT_UNKNOWN;

	pr_err("%s occurred at 0x%lx by %s(Page table base: 0x%lx)\n",
		sysmmu_fault_name[itype], fault_addr, name, pgtable_base);

	ent = section_entry(__va(pgtable_base), fault_addr);
	pr_err("\tLv1 entry: 0x%lx\n", *ent);

	if (lv1ent_page(ent)) {
		ent = page_entry(ent, fault_addr);
		pr_err("\t Lv2 entry: 0x%lx\n", *ent);
	}

	pr_err("Generating Kernel OOPS... because it is unrecoverable.\n");

	BUG();
}

static irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id)
{
	/* SYSMMU is in blocked when interrupt occurred. */
	struct sysmmu_drvdata *data = dev_id;
	enum exynos_sysmmu_inttype itype;
	unsigned long addr = -1;
	int ret = -ENOSYS;

	WARN_ON(!is_sysmmu_active(data));

	spin_lock(&data->lock);

	itype = (enum exynos_sysmmu_inttype)
		__ffs(__raw_readl(data->sfrbase + REG_INT_STATUS));
	if (WARN_ON(!((itype >= 0) && (itype < SYSMMU_FAULT_UNKNOWN))))
		itype = SYSMMU_FAULT_UNKNOWN;
	else
		addr = __raw_readl(
			data->sfrbase + fault_reg_offset[itype]);

	if (data->domain)
		ret = report_iommu_fault(data->domain, data->master,
				addr, itype);

	if (!ret && (itype != SYSMMU_FAULT_UNKNOWN))
		__raw_writel(1 << itype, data->sfrbase + REG_INT_CLEAR);
	else {
		unsigned long ba = data->pgtable;
		if (itype != SYSMMU_FAULT_UNKNOWN)
			ba = __raw_readl(data->sfrbase + REG_PT_BASE_ADDR);
		show_fault_information(dev_name(data->sysmmu),
					itype, ba, addr);
	}

	if (itype != SYSMMU_FAULT_UNKNOWN)
		sysmmu_unblock(data->sfrbase);

	spin_unlock(&data->lock);

	return IRQ_HANDLED;
}

static void __sysmmu_disable_nocount(struct sysmmu_drvdata *data)
{
	if (data->suspended)
		return;

	data->suspended = 1;
	clk_enable(data->clk_master);

	__raw_writel(CTRL_DISABLE, data->sfrbase + REG_MMU_CTRL);
	__raw_writel(0, data->sfrbase + REG_MMU_CFG);

	clk_disable(data->clk);
	clk_disable(data->clk_master);
}

static bool __sysmmu_disable(struct sysmmu_drvdata *data)
{
	bool disabled;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);

	disabled = set_sysmmu_inactive(data);

	if (disabled) {
		data->pgtable = 0;
		data->domain = NULL;

		if (data->runtime_active)
			__sysmmu_disable_nocount(data);

		dev_dbg(data->sysmmu, "Disabled\n");
	} else  {
		dev_dbg(data->sysmmu, "%d times left to disable\n",
					data->activations);
	}

	spin_unlock_irqrestore(&data->lock, flags);

	return disabled;
}


static void __sysmmu_init_config(struct sysmmu_drvdata *data)
{
	unsigned long cfg = CFG_LRU | CFG_QOS(15);
	int maj, min = 0;

	maj = __sysmmu_version(data, &min);
	if (maj == 3) {
		if (min > 1) {
			cfg |= CFG_FLPDCACHE;
			cfg |= (min == 2) ? CFG_SYSSEL : CFG_ACGEN;
		}
	}

	__raw_writel(cfg, data->sfrbase + REG_MMU_CFG);
}

static void __sysmmu_enable_nocount(struct sysmmu_drvdata *data)
{
	if (!data->suspended)
		return;

	data->suspended = 0;

	clk_enable(data->clk_master);
	clk_enable(data->clk);

	__raw_writel(CTRL_BLOCK, data->sfrbase + REG_MMU_CTRL);

	__sysmmu_init_config(data);

	__sysmmu_set_ptbase(data->sfrbase, data->pgtable);

	__raw_writel(CTRL_ENABLE, data->sfrbase + REG_MMU_CTRL);

	clk_disable(data->clk_master);
}

static int __sysmmu_enable(struct sysmmu_drvdata *data,
			unsigned long pgtable, struct iommu_domain *domain)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	if (set_sysmmu_active(data)) {
		data->pgtable = pgtable;
		data->domain = domain;

		if (data->runtime_active)
			__sysmmu_enable_nocount(data);

		dev_dbg(data->sysmmu, "Enabled\n");
	} else {
		ret = (pgtable == data->pgtable) ? 1 : -EBUSY;

		dev_dbg(data->sysmmu, "already enabled\n");
	}

	if (WARN_ON(ret < 0))
		set_sysmmu_inactive(data); /* decrement count */

	spin_unlock_irqrestore(&data->lock, flags);

	return ret;
}

/* __exynos_sysmmu_enable: Enables System MMU
 *
 * returns -error if an error occurred and System MMU is not enabled,
 * 0 if the System MMU has been just enabled and 1 if System MMU was already
 * enabled before.
 */
static int __exynos_sysmmu_enable(struct device *dev, unsigned long pgtable,
				  struct iommu_domain *domain)
{
	int ret = 0;
	struct device *sysmmu = dev->archdata.iommu;
	struct sysmmu_drvdata *data;

	if (WARN_ON(!sysmmu))
		return -ENODEV;

	data = dev_get_drvdata(sysmmu);

	ret = __sysmmu_enable(data, pgtable, domain);
	if (ret >= 0)
		data->master = dev;

	return ret;
}

int exynos_sysmmu_enable(struct device *dev, unsigned long pgtable)
{
	int ret;

	BUG_ON(!memblock_is_memory(pgtable));

	ret = __exynos_sysmmu_enable(dev, pgtable, NULL);

	return ret;
}

static bool exynos_sysmmu_disable(struct sysmmu_drvdata *data)
{
	bool disabled = __sysmmu_disable(data);

	if (disabled)
		data->master = NULL;

	return disabled;
}

static void sysmmu_tlb_invalidate_entry(struct sysmmu_drvdata *data,
					unsigned long iova)
{
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	if (is_sysmmu_active(data) && data->runtime_active) {
		clk_enable(data->clk_master);
		__sysmmu_tlb_invalidate_entry(data->sfrbase, iova);
		clk_disable(data->clk_master);
	} else {
		dev_dbg(data->master,
			"disabled. Skipping TLB invalidation @ %#lx\n", iova);
	}
	spin_unlock_irqrestore(&data->lock, flags);
}

void exynos_sysmmu_tlb_invalidate(struct device *dev)
{
	struct device *sysmmu = dev->archdata.iommu;
	struct sysmmu_drvdata *data;
	unsigned long flags;

	data = dev_get_drvdata(sysmmu);

	spin_lock_irqsave(&data->lock, flags);

	if (is_sysmmu_active(data) && data->runtime_active) {
		clk_enable(data->clk_master);
		if (sysmmu_block(data->sfrbase)) {
			__sysmmu_tlb_invalidate(data->sfrbase);
			sysmmu_unblock(data->sfrbase);
		}
		clk_disable(data->clk_master);
	} else {
		dev_dbg(dev, "disabled. Skipping TLB invalidation\n");
	}
	spin_unlock_irqrestore(&data->lock, flags);
}

static int __init exynos_sysmmu_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct sysmmu_drvdata *data;
	struct resource *res;
	int irq;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "Not enough memory for initialization\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Unable to find IOMEM region\n");
		return -ENOENT;
	}

	data->sfrbase = devm_request_and_ioremap(dev, res);
	if (!data->sfrbase) {
		dev_err(dev, "Unable to map IOMEM @ %#x\n", res->start);
		return -EBUSY;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
	dev_err(dev, "Unable to find IRQ resource\n");
		return -ENOENT;
	}

	ret = devm_request_irq(dev, irq, exynos_sysmmu_irq, 0, dev_name(dev),
			       data);
	if (ret) {
		dev_err(dev, "Unable to register handler to irq %d\n", irq);
		return ret;
	}

	pm_runtime_enable(dev);

	data->sysmmu = dev;

	data->clk = devm_clk_get(dev, "sysmmu");
	if (IS_ERR(data->clk)) {
		dev_info(dev, "No gate clock found!\n");
		data->clk = NULL;
	}

	ret = clk_prepare(data->clk);
	if (ret) {
		dev_err(dev, "Failed to prepare clk\n");
		return ret;
	}

	data->clk_master = devm_clk_get(dev, "master");
	if (IS_ERR(data->clk_master))
		data->clk_master = NULL;

	ret = clk_prepare(data->clk_master);
	if (ret) {
		clk_unprepare(data->clk);
		dev_err(dev, "Failed to prepare master's clk\n");
		return ret;
	}

	data->runtime_active = !pm_runtime_enabled(dev);
	data->suspended = 1;

	spin_lock_init(&data->lock);
	INIT_LIST_HEAD(&data->node);

	platform_set_drvdata(pdev, data);
	dev_dbg(dev, "Probed and initialized\n");

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int sysmmu_suspend(struct device *dev)
{
	struct sysmmu_drvdata *data = dev_get_drvdata(dev);
	unsigned long flags;
	spin_lock_irqsave(&data->lock, flags);
	if (is_sysmmu_active(data) &&
		(!pm_runtime_enabled(dev) || data->runtime_active))
		__sysmmu_disable_nocount(data);
	spin_unlock_irqrestore(&data->lock, flags);
	return 0;
}

static int sysmmu_resume(struct device *dev)
{
	struct sysmmu_drvdata *data = dev_get_drvdata(dev);
	unsigned long flags;
	spin_lock_irqsave(&data->lock, flags);
	if (is_sysmmu_active(data) &&
		(!pm_runtime_enabled(dev) || data->runtime_active))
		__sysmmu_enable_nocount(data);
	spin_unlock_irqrestore(&data->lock, flags);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sysmmu_pm_ops, sysmmu_suspend, sysmmu_resume);

#ifdef CONFIG_OF
static struct of_device_id sysmmu_of_match[] __initconst = {
	{ .compatible	= "samsung,exynos4210-sysmmu", },
	{ },
};
#endif

static struct platform_driver exynos_sysmmu_driver __refdata = {
	.probe	= exynos_sysmmu_probe,
	.driver	= {
		.owner		= THIS_MODULE,
		.name		= "exynos-sysmmu",
		.pm		= &sysmmu_pm_ops,
		.of_match_table	= of_match_ptr(sysmmu_of_match),
	}
};

static inline void pgtable_flush(void *vastart, void *vaend)
{
	dmac_flush_range(vastart, vaend);
	outer_flush_range(virt_to_phys(vastart),
				virt_to_phys(vaend));
}

static int exynos_iommu_domain_init(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pgtable = (unsigned long *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 2);
	if (!priv->pgtable)
		goto err_pgtable;

	priv->lv2entcnt = (short *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 1);
	if (!priv->lv2entcnt)
		goto err_counter;

	pgtable_flush(priv->pgtable, priv->pgtable + NUM_LV1ENTRIES);

	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->pgtablelock);
	INIT_LIST_HEAD(&priv->clients);

	domain->geometry.aperture_start = 0;
	domain->geometry.aperture_end   = ~0UL;
	domain->geometry.force_aperture = true;

	domain->priv = priv;
	return 0;

err_counter:
	free_pages((unsigned long)priv->pgtable, 2);
err_pgtable:
	kfree(priv);
	return -ENOMEM;
}

static void exynos_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct sysmmu_drvdata *data;
	unsigned long flags;
	int i;

	WARN_ON(!list_empty(&priv->clients));

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(data, &priv->clients, node) {
		while (!exynos_sysmmu_disable(data))
			; /* until System MMU is actually disabled */
	}

	while (!list_empty(&priv->clients))
		list_del_init(priv->clients.next);

	spin_unlock_irqrestore(&priv->lock, flags);

	for (i = 0; i < NUM_LV1ENTRIES; i++)
		if (lv1ent_page(priv->pgtable + i))
			kmem_cache_free(lv2table_kmem_cache,
					__va(lv2table_base(priv->pgtable + i)));

	free_pages((unsigned long)priv->pgtable, 2);
	free_pages((unsigned long)priv->lv2entcnt, 1);
	kfree(domain->priv);
	domain->priv = NULL;
}

static int exynos_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct sysmmu_drvdata *data = dev_get_drvdata(dev->archdata.iommu);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&priv->lock, flags);

	ret = __exynos_sysmmu_enable(dev, __pa(priv->pgtable), domain);
	if (ret == 0)
		list_add_tail(&data->node, &priv->clients);

	spin_unlock_irqrestore(&priv->lock, flags);

	if (ret < 0) {
		dev_err(dev, "%s: Failed to attach IOMMU with pgtable %#lx\n",
				__func__, __pa(priv->pgtable));
		return ret;
	}

	dev_dbg(dev, "%s: Attached IOMMU with pgtable 0x%lx%s\n",
		__func__, __pa(priv->pgtable), (ret == 0) ? "" : ", again");

	return 0;
}

static void exynos_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct sysmmu_drvdata *data;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(data, &priv->clients, node) {
		if (data->sysmmu == dev->archdata.iommu) {
			if (exynos_sysmmu_disable(data))
				list_del_init(&data->node);
			break;
		}
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	if (data->sysmmu == dev->archdata.iommu)
		dev_dbg(dev, "%s: Detached IOMMU with pgtable %#lx\n",
					__func__, __pa(priv->pgtable));
	else
		dev_dbg(dev, "%s: No IOMMU is attached\n", __func__);
}

static unsigned long *alloc_lv2entry(unsigned long *sent, unsigned long iova,
					short *pgcounter)
{
	if (lv1ent_fault(sent)) {
		unsigned long *pent;

		pent = kmem_cache_zalloc(lv2table_kmem_cache, GFP_ATOMIC);
		BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
		if (!pent)
			return ERR_PTR(-ENOMEM);

		*sent = mk_lv1ent_page(__pa(pent));
		*pgcounter = NUM_LV2ENTRIES;
		pgtable_flush(pent, pent + NUM_LV2ENTRIES);
		pgtable_flush(sent, sent + 1);
	} else if (lv1ent_section(sent)) {
		return ERR_PTR(-EADDRINUSE);
	}

	return page_entry(sent, iova);
}

static int lv1set_section(unsigned long *sent, phys_addr_t paddr, short *pgcnt)
{
	if (lv1ent_section(sent))
		return -EADDRINUSE;

	if (lv1ent_page(sent)) {
		if (*pgcnt != NUM_LV2ENTRIES)
			return -EADDRINUSE;

		kmem_cache_free(lv2table_kmem_cache, page_entry(sent, 0));

		*pgcnt = 0;
	}

	*sent = mk_lv1ent_sect(paddr);

	pgtable_flush(sent, sent + 1);

	return 0;
}

static void clear_page_table(unsigned long *ent, int n)
{
	if (n > 0)
		memset(ent, 0, sizeof(*ent) * n);
}

static int lv2set_page(unsigned long *pent, phys_addr_t paddr, size_t size,
								short *pgcnt)
{
	if (size == SPAGE_SIZE) {
		if (!lv2ent_fault(pent))
			return -EADDRINUSE;

		*pent = mk_lv2ent_spage(paddr);
		pgtable_flush(pent, pent + 1);
		*pgcnt -= 1;
	} else { /* size == LPAGE_SIZE */
		int i;
		for (i = 0; i < SPAGES_PER_LPAGE; i++, pent++) {
			if (!lv2ent_fault(pent)) {
				clear_page_table(pent - i, i);
				return -EADDRINUSE;
			}

			*pent = mk_lv2ent_lpage(paddr);
		}
		pgtable_flush(pent - SPAGES_PER_LPAGE, pent);
		*pgcnt -= SPAGES_PER_LPAGE;
	}

	return 0;
}

static int exynos_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long *entry;
	unsigned long flags;
	int ret = -ENOMEM;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (size == SECT_SIZE) {
		ret = lv1set_section(entry, paddr,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
	} else {
		unsigned long *pent;

		pent = alloc_lv2entry(entry, iova,
					&priv->lv2entcnt[lv1ent_offset(iova)]);

		if (IS_ERR(pent))
			ret = PTR_ERR(pent);
		else
			ret = lv2set_page(pent, paddr, size,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
	}

	if (ret)
		pr_err("%s: Failed(%d) to map 0x%#x bytes @ %#lx\n",
			__func__, ret, size, iova);

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return ret;
}

static size_t exynos_iommu_unmap(struct iommu_domain *domain,
					       unsigned long iova, size_t size)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct sysmmu_drvdata *data;
	unsigned long flags;
	unsigned long *ent;
	size_t err_pgsize;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	ent = section_entry(priv->pgtable, iova);

	if (lv1ent_section(ent)) {
		if (WARN_ON(size < SECT_SIZE)) {
			err_pgsize = SECT_SIZE;
			goto err;
		}

		*ent = 0;
		pgtable_flush(ent, ent + 1);
		size = SECT_SIZE;
		goto done;
	}

	if (unlikely(lv1ent_fault(ent))) {
		if (size > SECT_SIZE)
			size = SECT_SIZE;
		goto done;
	}

	/* lv1ent_page(sent) == true here */

	ent = page_entry(ent, iova);

	if (unlikely(lv2ent_fault(ent))) {
		size = SPAGE_SIZE;
		goto done;
	}

	if (lv2ent_small(ent)) {
		*ent = 0;
		size = SPAGE_SIZE;
		pgtable_flush(ent, ent +1);
		priv->lv2entcnt[lv1ent_offset(iova)] += 1;
		goto done;
	}

	/* lv1ent_large(ent) == true here */
	if (WARN_ON(size < LPAGE_SIZE)) {
		err_pgsize = LPAGE_SIZE;
		goto err;
	}

	clear_page_table(ent, SPAGES_PER_LPAGE);
	pgtable_flush(ent, ent + SPAGES_PER_LPAGE);

	size = LPAGE_SIZE;
	priv->lv2entcnt[lv1ent_offset(iova)] += SPAGES_PER_LPAGE;
done:
	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	spin_lock_irqsave(&priv->lock, flags);
	list_for_each_entry(data, &priv->clients, node)
		sysmmu_tlb_invalidate_entry(data, iova);
	spin_unlock_irqrestore(&priv->lock, flags);

	return size;
err:
	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	pr_err("%s: Failed due to size(%#x) @ %#lx is"\
		" smaller than page size %#x\n",
		__func__, size, iova, err_pgsize);

	return 0;

}

static phys_addr_t exynos_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long *entry;
	unsigned long flags;
	phys_addr_t phys = 0;

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (lv1ent_section(entry)) {
		phys = section_phys(entry) + section_offs(iova);
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, iova);

		if (lv2ent_large(entry))
			phys = lpage_phys(entry) + lpage_offs(iova);
		else if (lv2ent_small(entry))
			phys = spage_phys(entry) + spage_offs(iova);
	}

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return phys;
}

static struct iommu_ops exynos_iommu_ops = {
	.domain_init = &exynos_iommu_domain_init,
	.domain_destroy = &exynos_iommu_domain_destroy,
	.attach_dev = &exynos_iommu_attach_device,
	.detach_dev = &exynos_iommu_detach_device,
	.map = &exynos_iommu_map,
	.unmap = &exynos_iommu_unmap,
	.iova_to_phys = &exynos_iommu_iova_to_phys,
	.pgsize_bitmap = SECT_SIZE | LPAGE_SIZE | SPAGE_SIZE,
};


#ifdef CONFIG_PM_SLEEP
static int sysmmu_pm_genpd_suspend(struct device *dev)
{
	struct sysmmu_drvdata *data = dev_get_drvdata(dev->archdata.iommu);
	int ret;

	ret = pm_generic_suspend(data->sysmmu);
	if (ret)
		return ret;

	ret = pm_generic_suspend(dev);
	if (ret)
		pm_generic_resume(data->sysmmu);

	return ret;
}

static int sysmmu_pm_genpd_resume(struct device *dev)
{
	struct sysmmu_drvdata *data = dev_get_drvdata(dev->archdata.iommu);
	int ret = 0;

	ret = pm_generic_resume(data->sysmmu);
	if (ret)
		return ret;

	ret = pm_generic_resume(dev);
	if (ret)
		pm_generic_suspend(data->sysmmu);

	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static void sysmmu_restore_state(struct device *sysmmu)
{
	struct sysmmu_drvdata *data = dev_get_drvdata(sysmmu);
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	data->runtime_active = true;
	if (is_sysmmu_active(data))
		__sysmmu_enable_nocount(data);
	spin_unlock_irqrestore(&data->lock, flags);
}

static void sysmmu_save_state(struct device *sysmmu)
{
	struct sysmmu_drvdata *data = dev_get_drvdata(sysmmu);
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	if (is_sysmmu_active(data))
		__sysmmu_disable_nocount(data);
	data->runtime_active = false;
	spin_unlock_irqrestore(&data->lock, flags);
}

static int sysmmu_pm_genpd_save_state(struct device *dev)
{
	struct sysmmu_drvdata *data = dev_get_drvdata(dev->archdata.iommu);
	int (*cb)(struct device *__dev);

	if (dev->type && dev->type->pm)
		cb = dev->type->pm->runtime_suspend;
	else if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_suspend;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_suspend;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_suspend;

	if (cb) {
		int ret;

		ret = cb(dev);
		if (ret)
			return ret;
	}

	sysmmu_save_state(data->sysmmu);

	return 0;
}

static int sysmmu_pm_genpd_restore_state(struct device *dev)
{
	struct sysmmu_drvdata *data = dev_get_drvdata(dev->archdata.iommu);
	int (*cb)(struct device *__dev);

	if (dev->type && dev->type->pm)
		cb = dev->type->pm->runtime_resume;
	else if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_resume;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_resume;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_resume;

	sysmmu_restore_state(data->sysmmu);

	if (cb) {
		int ret;
		ret = cb(dev);
		if (ret) {
			sysmmu_save_state(data->sysmmu);
			return ret;
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_PM_GENERIC_DOMAINS
struct gpd_dev_ops sysmmu_devpm_ops = {
#ifdef CONFIG_PM_RUNTIME
	.save_state = &sysmmu_pm_genpd_save_state,
	.restore_state = &sysmmu_pm_genpd_restore_state,
#endif
#ifdef CONFIG_PM_SLEEP
	.suspend = &sysmmu_pm_genpd_suspend,
	.resume = &sysmmu_pm_genpd_resume,
#endif
};
#endif /* CONFIG_PM_GENERIC_DOMAINS */


static int exynos_create_default_iommu_mapping(struct device *dev)
{
	struct dma_iommu_mapping *mapping;
	dma_addr_t base = 0x20000000;
	unsigned int size = SZ_128M;
	int order = 4;

	mapping = arm_iommu_create_mapping(&platform_bus_type, base, size, order);
	if (!mapping)
		return -ENOMEM;
	dev->dma_parms = kzalloc(sizeof(*dev->dma_parms), GFP_KERNEL);
	dma_set_max_seg_size(dev, 0xffffffffu);
	arm_iommu_attach_device(dev, mapping);
	return 0;
}

static int exynos_remove_iommu_mapping(struct device *dev)
{
	struct dma_iommu_mapping *mapping = to_dma_iommu_mapping(dev);
	arm_iommu_detach_device(dev);
	arm_iommu_release_mapping(mapping);
	return 0;
}

static int sysmmu_hook_driver_register(struct notifier_block *nb,
					unsigned long val,
					void *p)
{
	struct device *dev = p;

	switch (val) {
	case BUS_NOTIFY_BIND_DRIVER:
	{
		struct platform_device *sysmmu;
		struct device_node *np;
		const __be32 *phandle;
		int ret;

		phandle = of_get_property(dev->of_node, "iommu", NULL);
		if (!phandle)
			break;

		/* this always success: see above of_find_property() */
		np = of_parse_phandle(dev->of_node, "iommu", 0);
		sysmmu = of_find_device_by_node(np);
		if (!sysmmu) {
			dev_err(dev, "sysmmu node '%s' is not found\n",
				np->name);
				return -ENODEV;
		}
		dev_info(dev, "attaching sysmmu controller %s\n",
			 dev_name(&sysmmu->dev));

		ret = pm_genpd_add_callbacks(dev, &sysmmu_devpm_ops, NULL);
		if (ret && (ret != -ENOSYS)) {
			dev_err(dev,
				"Failed to register 'dev_pm_ops' for iommu\n");
			return ret;
		}

		dev->archdata.iommu = &sysmmu->dev;

		if (!to_dma_iommu_mapping(dev))
			exynos_create_default_iommu_mapping(dev);
		break;
	}
	case BUS_NOTIFY_BOUND_DRIVER:
	{
		if (dev->archdata.iommu && (!pm_runtime_enabled(dev) ||
					   IS_ERR(dev_to_genpd(dev)))) {
			struct sysmmu_drvdata *data;
			data = dev_get_drvdata(dev->archdata.iommu);
			pm_runtime_disable(data->sysmmu);
			data->runtime_active = !pm_runtime_enabled(data->sysmmu);
			if (data->runtime_active && is_sysmmu_active(data))
				__sysmmu_enable_nocount(data);

		}
		break;
	}
	case BUS_NOTIFY_UNBOUND_DRIVER:
	case BUS_NOTIFY_BIND_FAILED:
	{
		if (dev->archdata.iommu) {
			__pm_genpd_remove_callbacks(dev, false);
			if (to_dma_iommu_mapping(dev))
				exynos_remove_iommu_mapping(dev);
			dev->archdata.iommu = NULL;
		}
		break;
	}
	} /* switch (val) */

	return 0;
}

static struct notifier_block sysmmu_notifier = {
	.notifier_call = &sysmmu_hook_driver_register,
};

static int __init exynos_iommu_init(void)
{
	int ret;

	lv2table_kmem_cache = kmem_cache_create("exynos-iommu-lv2table",
				LV2TABLE_SIZE, LV2TABLE_SIZE, 0, NULL);
	if (!lv2table_kmem_cache) {
		pr_err("%s: Failed to create kmem cache\n", __func__);
		return -ENOMEM;
	}

	ret = platform_driver_register(&exynos_sysmmu_driver);
	if (ret == 0) {
		ret = bus_set_iommu(&platform_bus_type, &exynos_iommu_ops);
		if (ret == 0)
			ret = bus_register_notifier(&platform_bus_type,
						    &sysmmu_notifier);
	}

	if (ret) {
		pr_err("%s: Failed to register exynos-iommu driver.\n",
								__func__);
		kmem_cache_destroy(lv2table_kmem_cache);
	}

	return ret;
}
arch_initcall(exynos_iommu_init);

/*
 * Dummy driver to enable runtime power management for memport
 * devices, which required for correct Exynos SYSMMU operation.
 * Must be registered before drivers, which will use memport nodes.
 */
static int __init exynos_memport_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	pm_runtime_enable(dev);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id memport_of_match[] __initconst = {
	{ .compatible	= "samsung,memport", },
	{ },
};
#endif

static struct platform_driver exynos_memport_driver __refdata = {
	.probe	= exynos_memport_probe,
	.driver	= {
		.owner		= THIS_MODULE,
		.name		= "exynos-memport",
		.of_match_table	= of_match_ptr(memport_of_match),
	}
};

static int __init exynos_memport_init(void)
{
	return platform_driver_register(&exynos_memport_driver);
}
subsys_initcall(exynos_memport_init);
