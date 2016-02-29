/*
 * Contiguous Memory Allocator for DMA mapping framework
 * Copyright (c) 2010-2011 by Samsung Electronics.
 * Written by:
 *	Marek Szyprowski <m.szyprowski@samsung.com>
 *	Michal Nazarewicz <mina86@mina86.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 */

#define pr_fmt(fmt) "cma: " fmt

#ifdef CONFIG_DMA_CMA_DEBUG
#ifndef DEBUG
#  define DEBUG
#endif
#endif

#include <asm/page.h>
#include <asm/dma-contiguous.h>

#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/page-isolation.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/mm_types.h>
#include <linux/dma-contiguous.h>

#ifndef SZ_1M
#define SZ_1M (1 << 20)
#endif

struct cma {
	unsigned long	base_pfn;
	unsigned long	count;
	unsigned long	free;
	unsigned long	*bitmap;
	struct kobject	kobj;
	struct device	*dev;
	int		kernel_shared;
	struct mutex	lock;
};

struct cma *dma_contiguous_default_area;

#ifdef CONFIG_DMA_CMA_SIZE_MBYTES
#define CMA_SIZE_MBYTES CONFIG_DMA_CMA_SIZE_MBYTES
#else
#define CMA_SIZE_MBYTES 0
#endif

/*
 * Default global CMA area size can be defined in kernel's .config.
 * This is usefull mainly for distro maintainers to create a kernel
 * that works correctly for most supported systems.
 * The size can be set in bytes or as a percentage of the total memory
 * in the system.
 *
 * Users, who want to set the size of global CMA area for their system
 * should use cma= kernel parameter.
 */
static const unsigned long size_bytes = CMA_SIZE_MBYTES * SZ_1M;
static long size_cmdline = -1;

static int __init early_cma(char *p)
{
	pr_debug("%s(%s)\n", __func__, p);
	size_cmdline = memparse(p, &p);
	return 0;
}
early_param("cma", early_cma);

#ifdef CONFIG_DMA_CMA_SIZE_PERCENTAGE

static unsigned long __init __maybe_unused cma_early_percent_memory(void)
{
	struct memblock_region *reg;
	unsigned long total_pages = 0;

	/*
	 * We cannot use memblock_phys_mem_size() here, because
	 * memblock_analyze() has not been called yet.
	 */
	for_each_memblock(memory, reg)
		total_pages += memblock_region_memory_end_pfn(reg) -
			       memblock_region_memory_base_pfn(reg);

	return (total_pages * CONFIG_DMA_CMA_SIZE_PERCENTAGE / 100) << PAGE_SHIFT;
}

#else

static inline __maybe_unused unsigned long cma_early_percent_memory(void)
{
	return 0;
}

#endif

/**
 * dma_contiguous_reserve() - reserve area for contiguous memory handling
 * @limit: End address of the reserved memory (optional, 0 for any).
 *
 * This function reserves memory from early allocator. It should be
 * called by arch specific code once the early allocator (memblock or bootmem)
 * has been activated and all other subsystems have already allocated/reserved
 * memory.
 */
void __init dma_contiguous_reserve(phys_addr_t limit)
{
	unsigned long selected_size = 0;

	pr_debug("%s(limit %08lx)\n", __func__, (unsigned long)limit);

	if (size_cmdline != -1) {
		selected_size = size_cmdline;
	} else {
#ifdef CONFIG_DMA_CMA_SIZE_SEL_MBYTES
		selected_size = size_bytes;
#elif defined(CONFIG_DMA_CMA_SIZE_SEL_PERCENTAGE)
		selected_size = cma_early_percent_memory();
#elif defined(CONFIG_DMA_CMA_SIZE_SEL_MIN)
		selected_size = min(size_bytes, cma_early_percent_memory());
#elif defined(CONFIG_DMA_CMA_SIZE_SEL_MAX)
		selected_size = max(size_bytes, cma_early_percent_memory());
#endif
	}

	if (selected_size) {
		pr_debug("%s: reserving %ld MiB for global area\n", __func__,
			 selected_size / SZ_1M);

		dma_declare_contiguous(NULL, selected_size, 0, limit);
	}
};

static DEFINE_MUTEX(cma_mutex);

static __init int cma_activate_area(unsigned long base_pfn, unsigned long count)
{
	unsigned long pfn = base_pfn;
	unsigned i = count >> pageblock_order;
	struct zone *zone;

	WARN_ON_ONCE(!pfn_valid(pfn));
	zone = page_zone(pfn_to_page(pfn));

	do {
		unsigned j;
		base_pfn = pfn;
		for (j = pageblock_nr_pages; j; --j, pfn++) {
			WARN_ON_ONCE(!pfn_valid(pfn));
			if (page_zone(pfn_to_page(pfn)) != zone)
				return -EINVAL;
		}
		init_cma_reserved_pageblock(pfn_to_page(base_pfn));
	} while (--i);
	return 0;
}

static __init struct cma *cma_create_area(unsigned long base_pfn,
				     unsigned long count, struct device *dev)
{
	int bitmap_size = BITS_TO_LONGS(count) * sizeof(long);
	struct cma *cma;
	int ret = -ENOMEM;

	pr_debug("%s(base %08lx, count %lx)\n", __func__, base_pfn, count);

	cma = kzalloc(sizeof *cma, GFP_KERNEL);
	if (!cma)
		return ERR_PTR(-ENOMEM);

	cma->base_pfn = base_pfn;
	cma->count = count;
	cma->free = count;
	cma->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	cma->dev = dev;
	cma->kernel_shared = true;
	mutex_init(&cma->lock);

	if (!cma->bitmap)
		goto no_mem;
	ret = cma_activate_area(base_pfn, count);
	if (ret)
		goto error;

	pr_debug("%s: returned %p\n", __func__, (void *)cma);
	return cma;

error:
	kfree(cma->bitmap);
no_mem:
	mutex_destroy(&cma->lock);
	kfree(cma);
	return ERR_PTR(ret);
}

static struct cma_reserved {
	phys_addr_t start;
	unsigned long size;
	struct device *dev;
	struct cma *cma;
} cma_reserved[MAX_CMA_AREAS] __initdata;
static unsigned cma_reserved_count __initdata;

static int __init cma_init_reserved_areas(void)
{
	struct cma_reserved *r = cma_reserved;
	unsigned i = cma_reserved_count;

	pr_debug("%s()\n", __func__);
	for (; i; --i, ++r) {
		struct cma *cma;
		cma = cma_create_area(PFN_DOWN(r->start),
				      r->size >> PAGE_SHIFT, r->dev);
		if (!IS_ERR(cma)) {
			dev_set_cma_area(r->dev, cma);
			r->cma = cma;
		}
	}
	return 0;
}
core_initcall(cma_init_reserved_areas);

/**
 * dma_declare_contiguous() - reserve area for contiguous memory handling
 *			      for particular device
 * @dev:   Pointer to device structure.
 * @size:  Size of the reserved memory.
 * @base:  Start address of the reserved memory (optional, 0 for any).
 * @limit: End address of the reserved memory (optional, 0 for any).
 *
 * This function reserves memory for specified device. It should be
 * called by board specific code when early allocator (memblock or bootmem)
 * is still activate.
 */
int __init dma_declare_contiguous(struct device *dev, unsigned long size,
				  phys_addr_t base, phys_addr_t limit)
{
	struct cma_reserved *r = &cma_reserved[cma_reserved_count];
	unsigned long alignment;

	pr_debug("%s(size %lx, base %08lx, limit %08lx)\n", __func__,
		 (unsigned long)size, (unsigned long)base,
		 (unsigned long)limit);

	/* Sanity checks */
	if (cma_reserved_count == ARRAY_SIZE(cma_reserved)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size)
		return -EINVAL;

	/* Sanitise input arguments */
	alignment = PAGE_SIZE << max(MAX_ORDER-1, pageblock_order);
	base = ALIGN(base, alignment);
	size = ALIGN(size, alignment);
	limit &= ~(alignment - 1);

	/* Reserve memory */
	if (base) {
		if (memblock_is_region_reserved(base, size) ||
		    memblock_reserve(base, size) < 0) {
			base = -EBUSY;
			goto err;
		}
	} else {
		/*
		 * Use __memblock_alloc_base() since
		 * memblock_alloc_base() panic()s.
		 */
		phys_addr_t addr = __memblock_alloc_base(size, alignment, limit);
		if (!addr) {
			base = -ENOMEM;
			goto err;
		} else if (addr + size > ~(unsigned long)0) {
			memblock_free(addr, size);
			base = -EINVAL;
			goto err;
		} else {
			base = addr;
		}
	}

	/*
	 * Each reserved area must be initialised later, when more kernel
	 * subsystems (like slab allocator) are available.
	 */
	r->start = base;
	r->size = size;
	r->dev = dev;
	cma_reserved_count++;
	pr_info("CMA: reserved %ld MiB at %08lx\n", size / SZ_1M,
		(unsigned long)base);

	/* Architecture specific contiguous memory fixup. */
	dma_contiguous_early_fixup(base, size);
	return 0;
err:
	pr_err("CMA: failed to reserve %ld MiB\n", size / SZ_1M);
	return base;
}

static void clear_cma_bitmap(struct cma *cma, unsigned long pfn, int count)
{
	mutex_lock(&cma->lock);
	bitmap_clear(cma->bitmap, pfn - cma->base_pfn, count);
	mutex_unlock(&cma->lock);
}

/**
 * dma_alloc_from_contiguous() - allocate pages from contiguous area
 * @dev:   Pointer to device for which the allocation is performed.
 * @count: Requested number of pages.
 * @align: Requested alignment of pages (in PAGE_SIZE order).
 *
 * This function allocates memory buffer for specified device. It uses
 * device specific contiguous memory area if available or the default
 * global one. Requires architecture specific get_dev_cma_area() helper
 * function.
 */
struct page *dma_alloc_from_contiguous(struct device *dev, int count,
				       unsigned int align)
{
	unsigned long mask, pfn, pageno, start = 0;
	struct cma *cma = dev_get_cma_area(dev);
	int ret;

	if (!cma || !cma->count)
		return NULL;

	if (align > CONFIG_DMA_CMA_ALIGNMENT)
		align = CONFIG_DMA_CMA_ALIGNMENT;

	pr_debug("%s(cma %p, count %d, align %d)\n", __func__, (void *)cma,
		 count, align);

	if (!count)
		return NULL;

	mask = (1 << align) - 1;

	for (;;) {
		mutex_lock(&cma->lock);
		pageno = bitmap_find_next_zero_area(cma->bitmap, cma->count,
						    start, count, mask);
		if (pageno >= cma->count) {
			mutex_unlock(&cma->lock);
			ret = -ENOMEM;
			goto error;
		}
		bitmap_set(cma->bitmap, pageno, count);
		/*
		 * It's safe to drop the lock here. We've marked this region for
		 * our exclusive use. If the migration fails we will take the
		 * lock again and unmark it.
		 */
		mutex_unlock(&cma->lock);

		pfn = cma->base_pfn + pageno;

		if (cma->kernel_shared) {
			mutex_lock(&cma_mutex);
			ret = alloc_contig_range(pfn, pfn + count, MIGRATE_CMA);
			mutex_unlock(&cma_mutex);
		} else
			ret = 0;

		if (ret == 0) {
			cma->free -= count;
			break;
		} else if (ret != -EBUSY && ret != -EAGAIN) {
			clear_cma_bitmap(cma, pfn, count);
			goto error;
		}
		clear_cma_bitmap(cma, pfn, count);
		pr_debug("%s(): memory range at %p is busy, retrying\n",
			 __func__, pfn_to_page(pfn));
		/* try again with a bit different memory target */
		start = pageno + mask + 1;
	}

	pr_debug("%s(): returned %p\n", __func__, pfn_to_page(pfn));
	return pfn_to_page(pfn);
error:
	return NULL;
}

/**
 * dma_release_from_contiguous() - release allocated pages
 * @dev:   Pointer to device for which the pages were allocated.
 * @pages: Allocated pages.
 * @count: Number of allocated pages.
 *
 * This function releases memory allocated by dma_alloc_from_contiguous().
 * It returns false when provided pages do not belong to contiguous area and
 * true otherwise.
 */
bool dma_release_from_contiguous(struct device *dev, struct page *pages,
				 int count)
{
	struct cma *cma = dev_get_cma_area(dev);
	unsigned long pfn;

	if (!cma || !pages)
		return false;

	pr_debug("%s(page %p)\n", __func__, (void *)pages);

	pfn = page_to_pfn(pages);

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count)
		return false;

	VM_BUG_ON(pfn + count > cma->base_pfn + cma->count);

	if (cma->kernel_shared)
		free_contig_range(pfn, count);
	cma->free += count;
	clear_cma_bitmap(cma, pfn, count);

	return true;
}

struct page *cma_disable_sharing(struct cma *cma)
{
	unsigned long pfn, start = 0;
	int bitmap_size = BITS_TO_LONGS(cma->count) * sizeof(long);
	unsigned long *bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	unsigned long area_start, area_end, count;
	unsigned long free;
	int ret = 0;

	pr_debug("%s(cma %p)\n", __func__, cma);

	free = cma->free;

	mutex_lock(&cma->lock);
	memcpy(bitmap, cma->bitmap, bitmap_size);
	mutex_unlock(&cma->lock);

	while (free > 0) {
		area_start = find_next_zero_bit(bitmap, cma->count, start);
		if (area_start >= cma->count) {
			start = 0;
			continue;
		}
		area_end = find_next_bit(bitmap, cma->count, area_start);

		count = area_end - area_start;
		pfn = cma->base_pfn + area_start;

		for (;;) {
			pr_debug("CMA: disabling sharing for 0x%08x-0x%08x range\n",
				__pfn_to_phys(pfn), __pfn_to_phys(pfn + count));
			ret = alloc_contig_range(pfn, pfn + count, MIGRATE_CMA);
			if (ret == 0) {
				bitmap_set(bitmap, area_start, count);
				free -= count;
				break;
			} else if (ret != -EBUSY && ret != -EAGAIN) {
				goto error;
			}
			pr_debug("CMA: memory range is busy, retrying\n");
		}
	}
	cma->kernel_shared = 0;
error:
	kfree(bitmap);
	return NULL;
}

struct page *cma_enable_sharing(struct cma *cma)
{
	unsigned long pfn, start = 0;
	int bitmap_size = BITS_TO_LONGS(cma->count) * sizeof(long);
	unsigned long *bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	unsigned long area_start, area_end, count;
	unsigned long free;

	pr_debug("%s(cma %p)\n", __func__, cma);

	free = cma->free;

	mutex_lock(&cma->lock);
	memcpy(bitmap, cma->bitmap, bitmap_size);
	mutex_unlock(&cma->lock);

	while (free > 0) {
		area_start = find_next_zero_bit(bitmap, cma->count, start);
		if (area_start >= cma->count) {
			start = 0;
			continue;
		}
		area_end = find_next_bit(bitmap, cma->count, area_start);

		count = area_end - area_start;
		pfn = cma->base_pfn + area_start;

		pr_debug("CMA: enabling sharing for 0x%08x-0x%08x range\n",
			__pfn_to_phys(pfn), __pfn_to_phys(pfn + count));
		free_contig_range(pfn, count);
		bitmap_set(bitmap, area_start, count);
		free -= count;
	}
	cma->kernel_shared = 1;
	kfree(bitmap);
	return NULL;
}

static struct kset *cma_kset;

#define to_cma(x) container_of(x, struct cma, kobj)

struct cma_attribute {
	struct attribute attr;
	ssize_t (*show)(struct cma *cma, struct cma_attribute *attr, char *buf);
	ssize_t (*store)(struct cma *cma, struct cma_attribute *attr, const char *buf, size_t count);
};
#define to_cma_attr(x) container_of(x, struct cma_attribute, attr)

static ssize_t cma_attr_show(struct kobject *kobj,
			     struct attribute *attr,
			     char *buf)
{
	struct cma_attribute *attribute = to_cma_attr(attr);
	struct cma *cma = to_cma(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(cma, attribute, buf);
}

static ssize_t cma_attr_store(struct kobject *kobj,
			      struct attribute *attr,
			      const char *buf, size_t len)
{
	struct cma_attribute *attribute = to_cma_attr(attr);
	struct cma *cma = to_cma(kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(cma, attribute, buf, len);
}

static const struct sysfs_ops cma_sysfs_ops = {
	.show = cma_attr_show,
	.store = cma_attr_store,
};

static void cma_release(struct kobject *kobj)
{
	struct cma *cma = to_cma(kobj);
	kfree(cma);
}

static ssize_t cma_base_show(struct cma *cma, struct cma_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%08x\n", __pfn_to_phys(cma->base_pfn));
}
static struct cma_attribute cma_base_attribute =
	__ATTR(phys_base, 0444, cma_base_show, NULL);

static ssize_t cma_total_pages_show(struct cma *cma, struct cma_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%ld\n", cma->count);
}
static struct cma_attribute cma_total_pages_attribute =
	__ATTR(total_pages, 0444, cma_total_pages_show, NULL);

static ssize_t cma_free_pages_show(struct cma *cma, struct cma_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%ld\n", cma->free);
}
static struct cma_attribute cma_free_pages_attribute =
	__ATTR(free_pages, 0444, cma_free_pages_show, NULL);

static ssize_t cma_shared_show(struct cma *cma, struct cma_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", cma->kernel_shared);
}
static ssize_t cma_shared_store(struct cma *cma, struct cma_attribute *attr,
				const char *buf, size_t count)
{
	int err;
	unsigned long input;

	err = strict_strtoul(buf, 10, &input);
	if (err)
		return err;

	if (cma->kernel_shared && input == 0)
		cma_disable_sharing(cma);
	else if (!cma->kernel_shared && input == 1)
		cma_enable_sharing(cma);

	return count;
}

static struct cma_attribute cma_shared_attribute =
	__ATTR(shared, 0644, cma_shared_show, cma_shared_store);

static ssize_t cma_rebalance_pages_show(struct cma *cma, struct cma_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", cma_perform_rebalance());
}
static struct cma_attribute cma_migrate_pages_attribute =
	__ATTR(rebalance, 0400, cma_rebalance_pages_show, NULL);

static struct attribute *cma_default_attrs[] = {
	&cma_base_attribute.attr,
	&cma_total_pages_attribute.attr,
	&cma_free_pages_attribute.attr,
	&cma_shared_attribute.attr,
	&cma_migrate_pages_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct kobj_type cma_ktype = {
	.sysfs_ops = &cma_sysfs_ops,
	.release = cma_release,
	.default_attrs = cma_default_attrs,
};

static int __init cma_init_sysfs(void)
{
	struct cma_reserved *r = cma_reserved;
	unsigned i;
	int ret;

	printk("CMA: initializing sysfs interface\n");
	cma_kset = kset_create_and_add("cma", NULL, kernel_kobj);
	if (!cma_kset)
		return -ENOMEM;

	for (i = cma_reserved_count; i; --i, ++r) {
		struct cma *cma = r->cma;
		const char *name = cma->dev ? dev_name(cma->dev) : "_global_";
		cma->kobj.kset = cma_kset;
		ret = kobject_init_and_add(&cma->kobj, &cma_ktype, NULL, name);
		if (ret == 0)
			kobject_uevent(&cma->kobj, KOBJ_ADD);
	}
	return 0;
}
device_initcall(cma_init_sysfs);
