/*
 * A fairly generic DMA-API to IOMMU-API glue layer.
 *
 * Copyright (C) 2014 ARM Ltd.
 *
 * based in part on arch/arm/mm/dma-mapping.c:
 * Copyright (C) 2000-2004 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/dma-contiguous.h>
#include <linux/dma-iommu.h>

int iommu_dma_init(void)
{
	return 0;
}

struct iommu_dma_domain {
	struct iommu_domain *domain;

	unsigned long		**bitmaps;	/* array of bitmaps */
	unsigned int		nr_bitmaps;	/* nr of elements in array */
	unsigned int		extensions;
	size_t			bitmap_size;	/* size of a single bitmap */
	size_t			bits;		/* per bitmap */
	dma_addr_t		base;

	spinlock_t		lock;

	struct kref kref;
};

static inline dma_addr_t dev_dma_addr(struct device *dev, dma_addr_t addr)
{
	BUG_ON(addr < dev->dma_pfn_offset);
	return addr - ((dma_addr_t)dev->dma_pfn_offset << PAGE_SHIFT);
}

static int __dma_direction_to_prot(enum dma_data_direction dir, bool coherent)
{
	int prot = coherent ? IOMMU_CACHE : 0;

	switch (dir) {
	case DMA_BIDIRECTIONAL:
		return prot | IOMMU_READ | IOMMU_WRITE;
	case DMA_TO_DEVICE:
		return prot | IOMMU_READ;
	case DMA_FROM_DEVICE:
		return prot | IOMMU_WRITE;
	default:
		return 0;
	}
}

static int extend_iommu_mapping(struct iommu_dma_domain *mapping);

static inline int __reserve_iova(struct iommu_dma_domain *mapping,
				 dma_addr_t iova, size_t size)
{
	unsigned long count, start;
	unsigned long flags;
	int i, sbitmap, ebitmap;

	if (iova < mapping->base)
		return -EINVAL;

	start = (iova - mapping->base) >> PAGE_SHIFT;
	count = PAGE_ALIGN(size) >> PAGE_SHIFT;

	sbitmap = start / mapping->bits;
	ebitmap = (start + count) / mapping->bits;
	start = start % mapping->bits;

	if (ebitmap > mapping->extensions)
		return -EINVAL;

	spin_lock_irqsave(&mapping->lock, flags);

	for (i = mapping->nr_bitmaps; i <= ebitmap; i++) {
		if (extend_iommu_mapping(mapping)) {
			spin_unlock_irqrestore(&mapping->lock, flags);
			return -ENOMEM;
		}
	}

	for (i = sbitmap; count && i < mapping->nr_bitmaps; i++) {
		int bits = count;

		if (bits + start > mapping->bits)
			bits = mapping->bits - start;
		bitmap_set(mapping->bitmaps[i], start, bits);
		start = 0;
		count -= bits;
	}

	spin_unlock_irqrestore(&mapping->lock, flags);

	return 0;
}

static inline dma_addr_t __alloc_iova(struct iommu_dma_domain *mapping,
				      size_t size, bool coherent)
{
	unsigned int order = get_order(size);
	unsigned int align = 0;
	unsigned int count, start;
	size_t mapping_size = mapping->bits << PAGE_SHIFT;
	unsigned long flags;
	dma_addr_t iova;
	int i;

	if (order > 8)
		order = 8;

	count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	align = (1 << order) - 1;

	spin_lock_irqsave(&mapping->lock, flags);
	for (i = 0; i < mapping->nr_bitmaps; i++) {
		start = bitmap_find_next_zero_area(mapping->bitmaps[i],
				mapping->bits, 0, count, align);

		if (start > mapping->bits)
			continue;

		bitmap_set(mapping->bitmaps[i], start, count);
		break;
	}

	/*
	 * No unused range found. Try to extend the existing mapping
	 * and perform a second attempt to reserve an IO virtual
	 * address range of size bytes.
	 */
	if (i == mapping->nr_bitmaps) {
		if (extend_iommu_mapping(mapping)) {
			spin_unlock_irqrestore(&mapping->lock, flags);
			return DMA_ERROR_CODE;
		}

		start = bitmap_find_next_zero_area(mapping->bitmaps[i],
				mapping->bits, 0, count, align);

		if (start > mapping->bits) {
			spin_unlock_irqrestore(&mapping->lock, flags);
			return DMA_ERROR_CODE;
		}

		bitmap_set(mapping->bitmaps[i], start, count);
	}
	spin_unlock_irqrestore(&mapping->lock, flags);

	iova = mapping->base + (mapping_size * i);
	iova += start << PAGE_SHIFT;

	return iova;
}

static inline void __free_iova(struct iommu_dma_domain *mapping,
			       dma_addr_t addr, size_t size)
{
	unsigned int start, count;
	size_t mapping_size = mapping->bits << PAGE_SHIFT;
	unsigned long flags;
	dma_addr_t bitmap_base;
	u32 bitmap_index;

	if (!size)
		return;

	bitmap_index = (u32) (addr - mapping->base) / (u32) mapping_size;
	BUG_ON(addr < mapping->base || bitmap_index > mapping->extensions);

	bitmap_base = mapping->base + mapping_size * bitmap_index;

	start = (addr - bitmap_base) >>	PAGE_SHIFT;

	if (addr + size > bitmap_base + mapping_size) {
		/*
		 * The address range to be freed reaches into the iova
		 * range of the next bitmap. This should not happen as
		 * we don't allow this in __alloc_iova (at the
		 * moment).
		 */
		BUG();
	} else
		count = size >> PAGE_SHIFT;

	spin_lock_irqsave(&mapping->lock, flags);
	bitmap_clear(mapping->bitmaps[bitmap_index], start, count);
	spin_unlock_irqrestore(&mapping->lock, flags);
}

static inline size_t iova_offset(dma_addr_t iova)
{
	return iova & ~PAGE_MASK;
}

static inline size_t iova_align(size_t size)
{
	return PAGE_ALIGN(size);
}


/*
 * Create a mapping in device IO address space for specified pages
 */
dma_addr_t iommu_dma_create_iova_mapping(struct device *dev,
		struct page **pages, size_t size, bool coherent)
{
	struct iommu_dma_domain *dom = get_dma_domain(dev);
	struct iommu_domain *domain = dom->domain;
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	dma_addr_t addr_lo, addr_hi;
	int i, prot = __dma_direction_to_prot(DMA_BIDIRECTIONAL, coherent);

	addr_lo = __alloc_iova(dom, size, coherent);
	if (addr_lo == DMA_ERROR_CODE)
		return DMA_ERROR_CODE;

	addr_hi = addr_lo;
	for (i = 0; i < count; ) {
		unsigned int next_pfn = page_to_pfn(pages[i]) + 1;
		phys_addr_t phys = page_to_phys(pages[i]);
		unsigned int len, j;

		for (j = i+1; j < count; j++, next_pfn++)
			if (page_to_pfn(pages[j]) != next_pfn)
				break;

		len = (j - i) << PAGE_SHIFT;
		if (iommu_map(domain, addr_hi, phys, len, prot))
			goto fail;
		addr_hi += len;
		i = j;
	}
	return dev_dma_addr(dev, addr_lo);
fail:
	iommu_unmap(domain, addr_lo, addr_hi - addr_lo);
	__free_iova(dom, addr_lo, size);
	return DMA_ERROR_CODE;
}

int iommu_dma_release_iova_mapping(struct device *dev, dma_addr_t iova,
		size_t size)
{
	struct iommu_dma_domain *dom = get_dma_domain(dev);
	size_t offset = iova_offset(iova);
	size_t len = iova_align(size + offset);

	iommu_unmap(dom->domain, iova - offset, len);
	__free_iova(dom, iova, len);

	return 0;
}

struct page **iommu_dma_alloc_buffer(struct device *dev, size_t size,
		gfp_t gfp, struct dma_attrs *attrs,
		void (clear_buffer)(struct page *page, size_t size))
{
	struct page **pages;
	int count = size >> PAGE_SHIFT;
	int array_size = count * sizeof(struct page *);
	int i = 0;

	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, GFP_KERNEL);
	else
		pages = vzalloc(array_size);
	if (!pages)
		return NULL;

	if (dma_get_attr(DMA_ATTR_FORCE_CONTIGUOUS, attrs)) {
		unsigned long order = get_order(size);
		struct page *page;

		page = dma_alloc_from_contiguous(dev, count, order);
		if (!page)
			goto error;

		if (clear_buffer)
			clear_buffer(page, size);

		for (i = 0; i < count; i++)
			pages[i] = page + i;

		return pages;
	}

	/*
	 * IOMMU can map any pages, so himem can also be used here
	 */
	gfp |= __GFP_NOWARN | __GFP_HIGHMEM;

	while (count) {
		int j, order = __fls(count);

		pages[i] = alloc_pages(gfp, order);
		while (!pages[i] && order)
			pages[i] = alloc_pages(gfp, --order);
		if (!pages[i])
			goto error;

		if (order) {
			split_page(pages[i], order);
			j = 1 << order;
			while (--j)
				pages[i + j] = pages[i] + j;
		}

		if (clear_buffer)
			clear_buffer(pages[i], PAGE_SIZE << order);
		i += 1 << order;
		count -= 1 << order;
	}

	return pages;
error:
	while (i--)
		if (pages[i])
			__free_pages(pages[i], 0);
	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);
	return NULL;
}

int iommu_dma_free_buffer(struct device *dev, struct page **pages, size_t size,
		struct dma_attrs *attrs)
{
	int count = size >> PAGE_SHIFT;
	int array_size = count * sizeof(struct page *);
	int i;

	if (dma_get_attr(DMA_ATTR_FORCE_CONTIGUOUS, attrs)) {
		dma_release_from_contiguous(dev, pages[0], count);
	} else {
		for (i = 0; i < count; i++)
			if (pages[i])
				__free_pages(pages[i], 0);
	}

	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);
	return 0;
}

static dma_addr_t __iommu_dma_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		bool coherent)
{
	dma_addr_t dma_addr;
	struct iommu_dma_domain *dom = get_dma_domain(dev);
	phys_addr_t phys = page_to_phys(page);
	int prot = __dma_direction_to_prot(dir, coherent);
	int len = PAGE_ALIGN(size + offset);

	dma_addr = __alloc_iova(dom, len, coherent);
	if (dma_addr == DMA_ERROR_CODE)
		return dma_addr;

	if (iommu_map(dom->domain, dma_addr, phys, len, prot)) {
		__free_iova(dom, dma_addr, len);
		return DMA_ERROR_CODE;
	}

	return dev_dma_addr(dev, dma_addr + offset);
}

dma_addr_t iommu_dma_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	return __iommu_dma_map_page(dev, page, offset, size, dir, false);
}

dma_addr_t iommu_dma_coherent_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	return __iommu_dma_map_page(dev, page, offset, size, dir, true);
}

void iommu_dma_unmap_page(struct device *dev, dma_addr_t handle, size_t size,
		enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct iommu_dma_domain *dom = get_dma_domain(dev);
	dma_addr_t iova = handle & PAGE_MASK;
	int offset = handle & ~PAGE_MASK;
	int len = PAGE_ALIGN(size + offset);

	iommu_unmap(dom->domain, iova, len);
	__free_iova(dom, iova, len);
}

static int finalise_sg(struct device *dev, struct scatterlist *sg, int nents,
		dma_addr_t dma_addr)
{
	struct scatterlist *s, *seg = sg;
	unsigned long seg_mask = dma_get_seg_boundary(dev);
	unsigned int max_len = dma_get_max_seg_size(dev);
	unsigned int seg_len = 0, seg_dma = 0;
	int i, count = 1;

	for_each_sg(sg, s, nents, i) {
		/* Un-swizzling the fields here, hence the naming mismatch */
		unsigned int s_offset = sg_dma_address(s);
		unsigned int s_length = sg_dma_len(s);
		unsigned int s_dma_len = s->length;

		s->offset = s_offset;
		s->length = s_length;
		sg_dma_address(s) = DMA_ERROR_CODE;
		sg_dma_len(s) = 0;

		if (seg_len && (seg_dma + seg_len == dma_addr + s_offset) &&
		    (seg_len + s_dma_len <= max_len) &&
		    ((seg_dma & seg_mask) <= seg_mask - (seg_len + s_length))
		   ) {
			sg_dma_len(seg) += s_dma_len;
		} else {
			if (seg_len) {
				seg = sg_next(seg);
				count++;
			}
			sg_dma_len(seg) = s_dma_len;
			sg_dma_address(seg) = dma_addr + s_offset;

			seg_len = s_offset;
			seg_dma = dma_addr + s_offset;
		}
		seg_len += s_length;
		dma_addr += s_dma_len;
	}
	return count;
}

static void invalidate_sg(struct scatterlist *sg, int nents)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		if (sg_dma_address(s) != DMA_ERROR_CODE)
			s->offset = sg_dma_address(s);
		if (sg_dma_len(s))
			s->length = sg_dma_len(s);
		sg_dma_address(s) = DMA_ERROR_CODE;
		sg_dma_len(s) = 0;
	}
}

static int __iommu_dma_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs,
		bool coherent)
{
	struct iommu_dma_domain *dom = get_dma_domain(dev);
	dma_addr_t iova;
	struct scatterlist *s;
	dma_addr_t dma_addr;
	size_t iova_len = 0;
	int i, prot = __dma_direction_to_prot(dir, coherent);

	/*
	 * Work out how much IOVA space we need, and align the segments to
	 * IOVA granules for the IOMMU driver to handle. With some clever
	 * trickery we can modify the list in a reversible manner.
	 */
	for_each_sg(sg, s, nents, i) {
		size_t s_offset = iova_offset(s->offset);
		size_t s_length = s->length;

		sg_dma_address(s) = s->offset;
		sg_dma_len(s) = s_length;
		s->offset -= s_offset;
		s_length = iova_align(s_length + s_offset);
		s->length = s_length;

		iova_len += s_length;
	}

	iova = __alloc_iova(dom, iova_len, coherent);
	if (iova == DMA_ERROR_CODE)
		goto out_restore_sg;

	/*
	 * We'll leave any physical concatenation to the IOMMU driver's
	 * implementation - it knows better than we do.
	 */
	dma_addr = iova;
	if (iommu_map_sg(dom->domain, dma_addr, sg, nents, prot) < iova_len)
		goto out_free_iova;

	return finalise_sg(dev, sg, nents, dev_dma_addr(dev, dma_addr));

out_free_iova:
	__free_iova(dom, iova, iova_len);
out_restore_sg:
	invalidate_sg(sg, nents);
	return 0;
}

int iommu_dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir, struct dma_attrs *attrs)
{
	return __iommu_dma_map_sg(dev, sg, nents, dir, attrs, false);
}

int iommu_dma_coherent_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	return __iommu_dma_map_sg(dev, sg, nents, dir, attrs, true);
}

void iommu_dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct iommu_dma_domain *dom = get_dma_domain(dev);
	struct scatterlist *s;
	int i;
	dma_addr_t iova = sg_dma_address(sg) & PAGE_MASK;
	size_t len = 0;

	/*
	 * The scatterlist segments are mapped into contiguous IOVA space,
	 * so just add up the total length and unmap it in one go.
	 */
	for_each_sg(sg, s, nents, i)
		len += sg_dma_len(s);

	iommu_unmap(dom->domain, iova, len);
	__free_iova(dom, iova, len);
}

struct iommu_dma_domain *iommu_dma_create_domain(const struct iommu_ops *ops,
		dma_addr_t base, size_t size)
{
	struct iommu_dma_domain *dom;
	struct iommu_domain *domain;
	struct iommu_domain_geometry *dg;
	unsigned long order, base_pfn, end_pfn;
	unsigned int bits = size >> PAGE_SHIFT;
	unsigned int bitmap_size = BITS_TO_LONGS(bits) * sizeof(long);
	int extensions = 1;

	if (bitmap_size > PAGE_SIZE) {
		extensions = bitmap_size / PAGE_SIZE;
		bitmap_size = PAGE_SIZE;
	}
	pr_debug("base=%pad\tsize=0x%zx\n", &base, size);
	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return NULL;

	dom->bitmap_size = bitmap_size;
	dom->bitmaps = kcalloc(extensions, sizeof(unsigned long *),
				GFP_KERNEL);
	if (!dom->bitmaps)
		goto out_free_dma_domain;

	dom->bitmaps[0] = kzalloc(bitmap_size, GFP_KERNEL);
	if (!dom->bitmaps[0])
		goto out_free_dma_domain;

	dom->nr_bitmaps = 1;
	dom->extensions = extensions;
	dom->base = base;
	dom->bits = BITS_PER_BYTE * bitmap_size;
	spin_lock_init(&dom->lock);

	/*
	 * HACK: We'd like to ask the relevant IOMMU in ops for a suitable
	 * domain, but until that happens, bypass the bus nonsense and create
	 * one directly for this specific device/IOMMU combination...
	 */
	domain = kzalloc(sizeof(*domain), GFP_KERNEL);

	if (!domain)
		goto out_free_dma_domain;
	domain->ops = ops;

	if (ops->domain_init(domain))
		goto out_free_iommu_domain;
	/*
	 * ...and do the bare minimum to sanity-check that the domain allows
	 * at least some access to the device...
	 */
	dg = &domain->geometry;
	if (!(base <= dg->aperture_end && base + size > dg->aperture_start)) {
		pr_warn("DMA range outside IOMMU capability; is DT correct?\n");
		goto out_free_iommu_domain;
	}
	/* ...then finally give it a kicking to make sure it fits */
	dg->aperture_start = max(base, dg->aperture_start);
	dg->aperture_end = min(base + size - 1, dg->aperture_end);
	/*
	 * Note that this almost certainly breaks the case where multiple
	 * devices with different DMA capabilities need to share a domain,
	 * but we don't have the necessary information to handle that here
	 * anyway - "proper" group and domain allocation needs to involve
	 * the IOMMU driver and a complete view of the bus.
	 */

	/* Use the smallest supported page size for IOVA granularity */
	order = __ffs(ops->pgsize_bitmap);
	base_pfn = max(dg->aperture_start >> order, (dma_addr_t)1);
	end_pfn = dg->aperture_end >> order;

	dom->domain = domain;
	kref_init(&dom->kref);
	pr_debug("domain %p created\n", dom);
	return dom;

out_free_iommu_domain:
	kfree(domain);
out_free_dma_domain:
	kfree(dom);
	return NULL;
}

static int extend_iommu_mapping(struct iommu_dma_domain *mapping)
{
	int next_bitmap;

	if (mapping->nr_bitmaps > mapping->extensions)
		return -EINVAL;

	next_bitmap = mapping->nr_bitmaps;
	mapping->bitmaps[next_bitmap] = kzalloc(mapping->bitmap_size,
						GFP_ATOMIC);
	if (!mapping->bitmaps[next_bitmap])
		return -ENOMEM;

	mapping->nr_bitmaps++;

	return 0;
}

static void iommu_dma_free_domain(struct kref *kref)
{
	struct iommu_dma_domain *dom;
	int i;

	dom = container_of(kref, struct iommu_dma_domain, kref);

	for (i = 0; i < dom->nr_bitmaps; i++)
		kfree(dom->bitmaps[i]);
	kfree(dom->bitmaps);

	iommu_domain_free(dom->domain);
	kfree(dom);
	pr_debug("domain %p freed\n", dom);
}

void iommu_dma_release_domain(struct iommu_dma_domain *dom)
{
	kref_put(&dom->kref, iommu_dma_free_domain);
}

struct iommu_domain *iommu_dma_raw_domain(struct iommu_dma_domain *dom)
{
	return dom ? dom->domain : NULL;
}

int iommu_dma_attach_device(struct device *dev, struct iommu_dma_domain *dom)
{
	int ret;

	kref_get(&dom->kref);
	ret = iommu_attach_device(dom->domain, dev);
	if (ret)
		iommu_dma_release_domain(dom);
	else
		set_dma_domain(dev, dom);
	pr_debug("%s%s attached to domain %p\n", dev_name(dev),
			ret?" *not*":"", dom);
	return ret;
}

void iommu_dma_detach_device(struct device *dev)
{
	struct iommu_dma_domain *dom = get_dma_domain(dev);

	if (!dom) {
		dev_warn(dev, "Not attached\n");
		return;
	}
	set_dma_domain(dev, NULL);
	iommu_detach_device(dom->domain, dev);
	iommu_dma_release_domain(dom);
	pr_debug("%s detached from domain %p\n", dev_name(dev), dom);
}

int iommu_dma_supported(struct device *dev, u64 mask)
{
	/*
	 * This looks awful, but really it's reasonable to assume that if an
	 * IOMMU can't address everything that the CPU can, it probably isn't
	 * generic enough to be using this implementation in the first place.
	 */
	return 1;
}

int iommu_dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return dma_addr == DMA_ERROR_CODE;
}

int iommu_add_reserved_mapping(struct device *dev, struct iommu_dma_domain *dma_domain,
				phys_addr_t phys, dma_addr_t dma, size_t size)
{
	int ret;

	ret = __reserve_iova(dma_domain, dma, size);
	if (ret != 0) {
		dev_err(dev, "failed to reserve mapping\n");
		return ret;
	}

	ret = iommu_map(dma_domain->domain, dma, phys, size, IOMMU_READ);
	if (ret != 0) {
		dev_err(dev, "create IOMMU mapping\n");
		return ret;
	}

	dev_info(dev, "created reserved DMA mapping (%pa -> %pad, %zd bytes)\n",
		 &phys, &dma, size);

	return 0;
}

int iommu_dma_init_reserved(struct device *dev, struct iommu_dma_domain *dma_domain)
{
	const char *name = "iommu-reserved-mapping";
	const __be32 *prop = NULL;
	int len, naddr, nsize;
	struct device_node *node = dev->of_node;
	phys_addr_t phys;
	dma_addr_t dma;
	size_t size;

	if (!node)
		return 0;

	naddr = of_n_addr_cells(node);
	nsize = of_n_size_cells(node);

	prop = of_get_property(node, name, &len);
	if (!prop)
		return 0;

	len /= sizeof(u32);

	if (len < 2 * naddr + nsize) {
		dev_err(dev, "invalid length (%d cells) of %s property\n",
			len, name);
		return -EINVAL;
	}

	phys = of_read_number(prop, naddr);
	dma = of_read_number(prop + naddr, naddr);
	size = of_read_number(prop + 2*naddr, nsize);

	return iommu_add_reserved_mapping(dev, dma_domain, phys, dma, size);
}
