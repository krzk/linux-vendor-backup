/*
 * drivers/gpu/ion/ion_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include "ion_priv.h"

void *ion_heap_map_kernel(struct ion_heap *heap,
			  struct ion_buffer *buffer)
{
	struct scatterlist *sg;
	int i, j;
	void *vaddr;
	pgprot_t pgprot;
	struct sg_table *table = buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;

	if (!pages)
		return 0;

	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(table->sgl, sg, table->nents, i) {
		int npages_this_entry = PAGE_ALIGN(sg_dma_len(sg)) / PAGE_SIZE;
		struct page *page = sg_page(sg);
		BUG_ON(i >= npages);
		for (j = 0; j < npages_this_entry; j++) {
			*(tmp++) = page++;
		}
	}
	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	return vaddr;
}

void ion_heap_unmap_kernel(struct ion_heap *heap,
			   struct ion_buffer *buffer)
{
	vunmap(buffer->vaddr);
}

int ion_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
		      struct vm_area_struct *vma)
{
	struct sg_table *table = buffer->sg_table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	int i;

	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sg_dma_len(sg);

		if (offset >= sg_dma_len(sg)) {
			offset -= sg_dma_len(sg);
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg_dma_len(sg) - offset;
			offset = 0;
		}
		len = min(len, remainder);
		remap_pfn_range(vma, addr, page_to_pfn(page), len,
				vma->vm_page_prot);
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

int ion_heap_buffer_zero(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	pgprot_t pgprot;
	struct scatterlist *sg;
	struct vm_struct *vm_struct;
	int i, j, ret = 0;

	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	vm_struct = get_vm_area(PAGE_SIZE, VM_ALLOC);
	if (!vm_struct)
		return -ENOMEM;

	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);
		unsigned long len = sg_dma_len(sg);

		for (j = 0; j < len / PAGE_SIZE; j++) {
			struct page *sub_page = page + j;
			struct page **pages = &sub_page;
			ret = map_vm_area(vm_struct, pgprot, &pages);
			if (ret)
				goto end;
			memset(vm_struct->addr, 0, PAGE_SIZE);
			unmap_kernel_range((unsigned long)vm_struct->addr,
					   PAGE_SIZE);
		}
	}
end:
	free_vm_area(vm_struct);
	return ret;
}

#define VM_PAGE_COUNT_WIDTH 4
#define VM_PAGE_COUNT 4

static void ion_heap_sync_and_unmap(unsigned long vaddr,
					pte_t *ptep, size_t size,
					enum dma_data_direction dir,
					ion_heap_sync_func sync, bool memzero)
{
	int i;

	flush_cache_vmap(vaddr, vaddr + size);

	if (memzero)
		memset((void *) vaddr, 0, size);

	if (sync)
		sync((void *) vaddr, size, dir);

	for (i = 0; i < (size / PAGE_SIZE); i++)
		pte_clear(&init_mm, (void *) vaddr + (i * PAGE_SIZE), ptep + i);

	flush_cache_vunmap(vaddr, vaddr + size);
	flush_tlb_kernel_range(vaddr, vaddr + size);
}

void ion_heap_sync(struct ion_heap *heap, struct sg_table *sgt,
			enum dma_data_direction dir,
			ion_heap_sync_func sync, bool memzero)
{
	struct scatterlist *sg;
	int page_idx, pte_idx, i;
	unsigned long vaddr;
	size_t sum = 0;
	pte_t *ptep;

	down(&heap->vm_sem);

	page_idx = atomic_pop(&heap->page_idx, VM_PAGE_COUNT_WIDTH);
	BUG_ON((page_idx < 0) || (page_idx >= VM_PAGE_COUNT));

	pte_idx = page_idx * (SZ_1M / PAGE_SIZE);
	ptep = heap->pte[pte_idx];
	vaddr = (unsigned long) heap->reserved_vm_area->addr +
				(SZ_1M * page_idx);

	for_each_sg(sgt->sgl, sg, sgt->orig_nents, i) {
		int j;

		if (!PageHighMem(sg_page(sg))) {
			if (memzero)
				memset(page_address(sg_page(sg)),
							0, sg->length);
			if (sync)
				sync(page_address(sg_page(sg)),
							sg->length, dir);
			continue;
		}

		for (j = 0; j < (sg->length / PAGE_SIZE); j++) {
			set_pte_at(&init_mm, vaddr, ptep,
					mk_pte(sg_page(sg) + j, PAGE_KERNEL));
			ptep++;
			vaddr += PAGE_SIZE;
			sum += PAGE_SIZE;

			if (sum == SZ_1M) {
				ptep = heap->pte[pte_idx];
				vaddr =
				(unsigned long)heap->reserved_vm_area->addr
					+ (SZ_1M * page_idx);

				ion_heap_sync_and_unmap(vaddr, ptep, sum,
						dir, sync, memzero);
				sum = 0;
			}
		}
	}

	if (sum != 0) {
		ion_heap_sync_and_unmap(
			(unsigned long) heap->reserved_vm_area->addr +
				(SZ_1M * page_idx),
			heap->pte[pte_idx], sum, dir, sync, memzero);
	}

	atomic_push(&heap->page_idx, page_idx, VM_PAGE_COUNT_WIDTH);

	up(&heap->vm_sem);
}

int ion_heap_reserve_vm(struct ion_heap *heap)
{
	int i;

	atomic_set(&heap->page_idx, -1);

	for (i = VM_PAGE_COUNT - 1; i >= 0; i--) {
		BUG_ON(i >= (1 << VM_PAGE_COUNT_WIDTH));
		atomic_push(&heap->page_idx, i, VM_PAGE_COUNT_WIDTH);
	}

	sema_init(&heap->vm_sem, VM_PAGE_COUNT);
	heap->pte = page_address(
			alloc_pages(GFP_KERNEL,
				get_order(((SZ_1M / PAGE_SIZE) *
						VM_PAGE_COUNT) *
						sizeof(*heap->pte))));
	heap->reserved_vm_area = alloc_vm_area(SZ_1M *
						VM_PAGE_COUNT, heap->pte);
	if (!heap->reserved_vm_area) {
		pr_err("%s: Failed to allocate vm area\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

struct ion_heap *ion_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;

	switch (heap_data->type) {
	case ION_HEAP_TYPE_SYSTEM_CONTIG:
		heap = ion_system_contig_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_SYSTEM:
		heap = ion_system_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_CARVEOUT:
		heap = ion_carveout_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_CHUNK:
		heap = ion_chunk_heap_create(heap_data);
		break;
	default:
		pr_err("%s: Invalid heap type %d\n", __func__,
		       heap_data->type);
		return ERR_PTR(-EINVAL);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %lu size %u\n",
		       __func__, heap_data->name, heap_data->type,
		       heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;

	ion_heap_reserve_vm(heap);

	return heap;
}

void ion_heap_destroy(struct ion_heap *heap)
{
	if (!heap)
		return;

	switch (heap->type) {
	case ION_HEAP_TYPE_SYSTEM_CONTIG:
		ion_system_contig_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_SYSTEM:
		ion_system_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_CARVEOUT:
		ion_carveout_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_CHUNK:
		ion_chunk_heap_destroy(heap);
		break;
	default:
		pr_err("%s: Invalid heap type %d\n", __func__,
		       heap->type);
	}

	free_vm_area(heap->reserved_vm_area);
}
