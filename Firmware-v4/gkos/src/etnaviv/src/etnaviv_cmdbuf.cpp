// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017-2018 Etnaviv Project
 */

//#include <linux/dma-mapping.h>

#include "etnaviv_cmdbuf.h"
#include "etnaviv_gem.h"
#include "etnaviv_gpu.h"
#include "etnaviv_mmu.h"

std::unique_ptr<etnaviv_cmdbuf_suballoc>
etnaviv_cmdbuf_suballoc_new(struct device *dev)
{
	auto suballoc = std::make_unique<etnaviv_cmdbuf_suballoc>();
	if (!suballoc)
		return nullptr;

	suballoc->dev = dev;

	BUILD_BUG_ON(ETNAVIV_SOFTPIN_START_ADDRESS < SUBALLOC_SIZE);
	suballoc->vaddr = dma_alloc_wc(dev, SUBALLOC_SIZE,
				       &suballoc->paddr, GFP_KERNEL);
	if (!suballoc->vaddr) {
		return nullptr;
	}

	return suballoc;
}

int etnaviv_cmdbuf_suballoc_map(struct etnaviv_cmdbuf_suballoc *suballoc,
				struct etnaviv_iommu_context *context,
				struct etnaviv_vram_mapping *mapping,
				u32 memory_base)
{
	return etnaviv_iommu_get_suballoc_va(context, mapping, memory_base,
					     suballoc->paddr, SUBALLOC_SIZE);
}

void etnaviv_cmdbuf_suballoc_unmap(struct etnaviv_iommu_context *context,
				   struct etnaviv_vram_mapping *mapping)
{
	etnaviv_iommu_put_suballoc_va(context, mapping);
}

void etnaviv_cmdbuf_suballoc_destroy(struct etnaviv_cmdbuf_suballoc *suballoc)
{
	dma_free_wc(suballoc->dev, SUBALLOC_SIZE, suballoc->vaddr,
		    suballoc->paddr);
}

int etnaviv_cmdbuf_init(struct etnaviv_cmdbuf_suballoc *suballoc,
			struct etnaviv_cmdbuf *cmdbuf, u32 size)
{
	int granule_offs, order, ret;

	cmdbuf->suballoc = suballoc;
	cmdbuf->size = size;

	order = order_base_2(ALIGN(size, SUBALLOC_GRANULE) / SUBALLOC_GRANULE);
retry:
	suballoc->lock.lock();
	granule_offs = bitmap_find_free_region(suballoc->granule_map,
					SUBALLOC_GRANULES, order);
	if (granule_offs < 0) {
		suballoc->free_space = 0;
		suballoc->lock.unlock();
		suballoc->free_event.Wait(clock_cur() + kernel_time_from_ms(10000), &ret);
		if (!ret) {
			klog(
				"Timeout waiting for cmdbuf space\n");
			return -ETIMEDOUT;
		}
		goto retry;
	}
	suballoc->lock.unlock();
	cmdbuf->suballoc_offset = granule_offs * SUBALLOC_GRANULE;
	cmdbuf->vaddr = (void *)((uintptr_t)suballoc->vaddr + cmdbuf->suballoc_offset);

	return 0;
}

void etnaviv_cmdbuf_free(struct etnaviv_cmdbuf *cmdbuf)
{
	struct etnaviv_cmdbuf_suballoc *suballoc = cmdbuf->suballoc;
	int order = order_base_2(ALIGN(cmdbuf->size, SUBALLOC_GRANULE) /
				 SUBALLOC_GRANULE);

	if (!suballoc)
		return;

	suballoc->lock.lock();
	bitmap_release_region(suballoc->granule_map,
			      cmdbuf->suballoc_offset / SUBALLOC_GRANULE,
			      order);
	suballoc->free_space = 1;
	suballoc->lock.unlock();
	suballoc->free_event.Signal();
}

u32 etnaviv_cmdbuf_get_va(struct etnaviv_cmdbuf *buf,
			  struct etnaviv_vram_mapping *mapping)
{
	return mapping->iova + buf->suballoc_offset;
}

dma_addr_t etnaviv_cmdbuf_get_pa(struct etnaviv_cmdbuf *buf)
{
	return buf->suballoc->paddr + buf->suballoc_offset;
}
