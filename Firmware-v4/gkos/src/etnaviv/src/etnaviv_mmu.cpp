// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015-2018 Etnaviv Project
 */

//#include <linux/dma-mapping.h>
//#include <linux/scatterlist.h>

//#include <drm/drm_print.h>

#include "common.xml.h"
#include "etnaviv_cmdbuf.h"
#include "etnaviv_drv.h"
#include "etnaviv_gem.h"
#include "etnaviv_gpu.h"
#include "etnaviv_mmu.h"
#include "block_allocator.h"
#include "coalescing_block_allocator.h"

static void etnaviv_context_unmap(struct etnaviv_iommu_context *context,
				 unsigned long iova, size_t size)
{
	size_t unmapped_page, unmapped = 0;
	size_t pgsize = 4096;

	while (unmapped < size) {
		unmapped_page = context->unmap(iova, pgsize);
		if (!unmapped_page)
			break;

		iova += unmapped_page;
		unmapped += unmapped_page;
	}
}

static int etnaviv_context_map(struct etnaviv_iommu_context *context,
			      unsigned long iova, phys_addr_t paddr,
			      size_t size, int prot)
{
	unsigned long orig_iova = iova;
	size_t pgsize = 4096;
	size_t orig_size = size;
	int ret = 0;

	while (size) {
		ret = context->map(iova, paddr, pgsize,	prot);
		if (ret)
			break;

		iova += pgsize;
		paddr += pgsize;
		size -= pgsize;
	}

	/* unroll mapping in case something went wrong */
	if (ret)
		etnaviv_context_unmap(context, orig_iova, orig_size - size);

	return ret;
}

static int etnaviv_iommu_map(struct etnaviv_iommu_context *context,
			     u32 iova, unsigned int va_len,
			     sg_table &sgt, int prot)
{
	unsigned int da = iova;
	unsigned int i = 0;
	int ret;

	if (!context)
		return -EINVAL;

	for(auto &sge : sgt) {
		phys_addr_t pa = sge.paddr;
		unsigned int da_len = sge.len;
		unsigned int bytes = std::min(da_len, va_len);

		VERB("map[%d]: %08x %pap(%x)", i++, da, &pa, bytes);

		if (!IS_ALIGNED(iova | pa | bytes, 4096ul)) {
			dev_err(context->global->dev,
				"unaligned: iova 0x%x pa %pa size 0x%x\n",
				iova, &pa, bytes);
			ret = -EINVAL;
			goto fail;
		}

		ret = etnaviv_context_map(context, da, pa, bytes, prot);
		if (ret)
			goto fail;

		va_len -= bytes;
		da += bytes;
	}

	context->flush_seq++;

	return 0;

fail:
	etnaviv_context_unmap(context, iova, da - iova);
	return ret;
}

static void etnaviv_iommu_unmap(struct etnaviv_iommu_context *context, u32 iova,
				sg_table &sgt, unsigned len)
{
	etnaviv_context_unmap(context, iova, len);

	context->flush_seq++;
}

static void etnaviv_iommu_remove_mapping(struct etnaviv_iommu_context *context,
	std::shared_ptr<etnaviv_vram_mapping> mapping)
{
	struct etnaviv_gem_object *etnaviv_obj = mapping->object;

	BUG_ON(!context->lock->held());

	etnaviv_iommu_unmap(context, mapping->vram_node.start,
			    etnaviv_obj->sgt, etnaviv_obj->size);
	context->mm.alloc.Dealloc(mapping->vram_node.start);
}

void etnaviv_iommu_reap_mapping(std::shared_ptr<etnaviv_vram_mapping> mapping)
{
	struct etnaviv_iommu_context *context = mapping->context.get();

	BUG_ON(!context->lock->held());
	WARN_ON(mapping->use);

	etnaviv_iommu_remove_mapping(context, mapping);
	mapping->context = nullptr;

	context->mm.alloc.Dealloc(mapping->vram_node.start);
}

static int etnaviv_iommu_find_iova(struct etnaviv_iommu_context *context,
				   drm_mm_node *node, size_t size)
{
	/* This function finds a free area in the GPU's virtual memory space.
		If it cannot find one, it keeps removing entries until it
		has enough space.
	*/
	BUG_ON(!context->lock->held());

	// save the last evicted location so we can use it in the loop
	uintptr_t last_evicted = ~0ULL;

	while (1) {
		auto &mm = context->mm.alloc;
		auto iter = mm.end();

		if(last_evicted != ~0ULL)
		{
			iter = mm.AllocFixed({ .start = last_evicted, .length = size });
		}
		if(iter == mm.end())
		{
			iter = mm.AllocAny(size);
		}
		if(iter != mm.end())
		{
			node->start = iter->first.start;
			node->length = iter->first.length;
			node->mm = &context->mm;
			return 0;
		}

		/* Logic for the following search:
			Iterates through context->mappings (a list of mmu_nodes)

			It adds those with a non-zero vram_node.mm member, and a
				zero 'use' member to a list called 'list'

			All of these are potentials for removal.

			It then uses the drm_mm_scan logic to determine if removing
				several of these in combination would create a hole
				large enough to satisfy the request.

			drm_mm_scan_add_block maintains a list of memory blocks.  It
				coalesces them together and eventually returns true if
				any internal coalesced block is large enough to satisfy
				the request (request is initially set up in scan_init).
				The scanner then maintains an internal block representing
				where the hole that is large enough is.

			Once this is the case, entries are again removed from the scanner.
			Any of those which overlap the hole will return true from
			remove_block, those which don't will return false (these latter are
			pruned from the list named 'list'

			Then, those members of 'list' are passed to the driver defined
			cleanup function (etnaviv_iommu_reap_mapping) which does driver
			specific unmapping, as well as removing them from the mm itself.

			Finally, the drm_mm is called again to allocate the block, with
			the hint DRM_MM_INSERT_EVICT meaning to alloc at the last spot
			we evicted.

			We can achieve a similar thing with a map implementation, without
			the remove step.

			Each node contains a list of blocks that have formed it, as well
			as a base and length.

			When we add a new node, we use a list of length 1 spanning the
			node.  We then immediately get its prev() and next() members.  If
			they are adjacent, we coalesce (increase length/base, combine the
			lists of nodes (keep in order)).  If the new node is big enough, we return the
			list of nodes that were used to create it.  Then, we selectively
			evict enough of these (starting from the beginning) to satisfy
			the request.
		*/

		struct scan_node
		{
			std::list<uintptr_t> base_addrs;
			bool CoalesceFrom(scan_node &other, bool other_is_prev)
			{
				if (other_is_prev)
				{
					base_addrs.insert(base_addrs.begin(),
						other.base_addrs.begin(), other.base_addrs.end());
				}
				else
				{
					base_addrs.insert(base_addrs.end(),
						other.base_addrs.begin(), other.base_addrs.end());
				}
				return true;
			}
			
			scan_node(uintptr_t address) { base_addrs.push_back(address); }
		};

		bool found = false;
		CoalescingBlockAllocator<scan_node> scan;
		for(const auto &free : context->mappings)
		{
			/* If this vram node has not been used, skip this. */
			if (!free.second->vram_node.mm)
				continue;

			/*
			 * If the iova is pinned, then it's in-use,
			 * so we must keep its mapping.
			 */
			if (free.second->use)
				continue;

			auto &cmap = free.second->vram_node;
			CoalescingBlockAllocator<scan_node>::BlockAddress baddr
			{ .start = cmap.start, .length = cmap.length };

			auto scan_iter = scan.AllocFixed(baddr, scan_node(cmap.start));

			auto &creg = scan_iter->first;
			if(creg.length >= size)
			{
				// We have identified a sufficiently large region to contain the
				//  new block.  Now successively free the blocks.
				found = true;
				last_evicted = creg.start;

				uintptr_t cur_evicted = 0;
				
				for(const auto &addr : scan_iter->second.base_addrs)
				{
					auto cmapping = context->mappings.find(addr);
					BUG_ON(cmapping == context->mappings.end());
					cur_evicted += cmapping->second->vram_node.length;
					etnaviv_iommu_reap_mapping(cmapping->second);

					if(cur_evicted >= size)
					{
						// stop here
						break;
					}
				}

				break;
			}
		}

		if(!found)
		{
			return -ENOSPC;
		}

		/*
		 * We removed enough mappings so that the new allocation will
		 * succeed, retry the allocation one more time.
		 */
	}
}

static int etnaviv_iommu_insert_exact(struct etnaviv_iommu_context *context,
		   struct drm_mm_node *node, size_t size, u64 va)
{
	BUG_ON(!context->lock->held());

	auto insert_iter = context->mm.alloc.AllocFixed(
		{
			.start = va, .length = size
		}
	);
	if(insert_iter != context->mm.alloc.end())
	{
		return 0;
	}

	/*
	 * When we can't insert the node, due to a existing mapping blocking
	 * the address space, there are two possible reasons:
	 * 1. Userspace genuinely messed up and tried to reuse address space
	 * before the last job using this VMA has finished executing.
	 * 2. The existing buffer mappings are idle, but the buffers are not
	 * destroyed yet (likely due to being referenced by another context) in
	 * which case the mappings will not be cleaned up and we must reap them
	 * here to make space for the new mapping.
	 */

	auto from = context->mm.alloc.LeftBlock(va);
	auto to = context->mm.alloc.RightBlock(va + size);

	std::vector<uintptr_t> del_addrs;

	for(; from != to; from++)
	{
		auto &m = context->mappings[from->first.start];
		if(m->use)
			return -ENOSPC;
		del_addrs.push_back(m->vram_node.start);
	}

	for(const auto del_addr : del_addrs)
	{
		auto &m = context->mappings[del_addr];
		etnaviv_iommu_reap_mapping(m);
	}

	insert_iter = context->mm.alloc.AllocFixed(
		{
			.start = va, .length = size
		}
	);
	if(insert_iter != context->mm.alloc.end())
	{
		return 0;
	}
	else
	{
		return -ENOSPC;
	}
}

int etnaviv_iommu_map_gem(std::shared_ptr<etnaviv_iommu_context> context,
	struct etnaviv_gem_object *etnaviv_obj, u32 memory_base,
	std::shared_ptr<etnaviv_vram_mapping> mapping, u64 va)
{
	sg_table *sgt = &etnaviv_obj->sgt;
	struct drm_mm_node *node;
	int ret;

	BUG_ON(!(etnaviv_obj->lock->held()));

	context->lock->lock();

	/* v1 MMU can optimize single entry (contiguous) scatterlists */
	if (context->global->version == ETNAVIV_IOMMU_V1 &&
	    sgt->size() == 1 && !(etnaviv_obj->flags & ETNA_BO_FORCE_MMU)) {
		u32 iova;

		iova = (*sgt)[0].paddr - memory_base;
		if (iova < 0x80000000 - (*sgt)[0].len) {
			mapping->iova = iova;
			mapping->context = context;
			context->mappings[mapping->vram_node.start] = mapping;

			ret = 0;
			goto unlock;
		}
	}

	node = &mapping->vram_node;

	if (va)
		ret = etnaviv_iommu_insert_exact(context.get(), node, etnaviv_obj->size, va);
	else
		ret = etnaviv_iommu_find_iova(context.get(), node, etnaviv_obj->size);
	if (ret < 0)
		goto unlock;

	mapping->iova = node->start;
	ret = etnaviv_iommu_map(context.get(), node->start, etnaviv_obj->size, *sgt,
				ETNAVIV_PROT_READ | ETNAVIV_PROT_WRITE);

	if (ret < 0) {
		context->mm.alloc.Dealloc(node->start);
		goto unlock;
	}

	mapping->context = context;
	context->mappings[mapping->vram_node.start] = mapping;
unlock:
	context->lock->unlock();

	return ret;
}

void etnaviv_iommu_unmap_gem(std::shared_ptr<etnaviv_iommu_context> context,
	std::shared_ptr<etnaviv_vram_mapping> mapping)
{
	WARN_ON(mapping->use);

	context->lock->lock();

	/* Bail if the mapping has been reaped by another thread */
	if (!mapping->context) {
		context->lock->unlock();
		return;
	}

	/* If the vram node is on the mm, unmap and remove the node */
	if (mapping->vram_node.mm == &context->mm)
		etnaviv_iommu_remove_mapping(context.get(), mapping);

	context->mappings.erase(mapping->vram_node.start);
	mapping->context = nullptr;
}

etnaviv_iommu_context::~etnaviv_iommu_context()
{
	etnaviv_cmdbuf_suballoc_unmap(this, &cmdbuf_mapping);
}

std::shared_ptr<etnaviv_iommu_context>
etnaviv_iommu_context_init(struct etnaviv_iommu_global *global,
			   struct etnaviv_cmdbuf_suballoc *suballoc)
{
	std::shared_ptr<etnaviv_iommu_context> ctx;
	int ret;

	if (global->version == ETNAVIV_IOMMU_V1)
		ctx = etnaviv_iommuv1_context_alloc(global);
	else
		ctx = etnaviv_iommuv2_context_alloc(global);

	if (!ctx)
		return NULL;

	ret = etnaviv_cmdbuf_suballoc_map(suballoc, ctx.get(), &ctx->cmdbuf_mapping,
					  global->memory_base);
	if (ret)
		goto out_free;

	if (global->version == ETNAVIV_IOMMU_V1 &&
	    ctx->cmdbuf_mapping.iova > 0x80000000) {
		dev_err(global->dev,
		        "command buffer outside valid memory window\n");
		goto out_unmap;
	}

	return ctx;

out_unmap:
	etnaviv_cmdbuf_suballoc_unmap(ctx.get(), &ctx->cmdbuf_mapping);
out_free:
	return NULL;
}

void etnaviv_iommu_restore(struct etnaviv_gpu *gpu,
			   std::shared_ptr<etnaviv_iommu_context> context)
{
	context->restore(gpu, context);
}

int etnaviv_iommu_get_suballoc_va(struct etnaviv_iommu_context *context,
				  std::shared_ptr<etnaviv_vram_mapping> mapping,
				  u32 memory_base, dma_addr_t paddr,
				  size_t size)
{
	context->lock->lock();

	if (mapping->use > 0) {
		mapping->use++;
		context->lock->unlock();
		return 0;
	}

	/*
	 * For MMUv1 we don't add the suballoc region to the pagetables, as
	 * those GPUs can only work with cmdbufs accessed through the linear
	 * window. Instead we manufacture a mapping to make it look uniform
	 * to the upper layers.
	 */
	if (context->global->version == ETNAVIV_IOMMU_V1) {
		mapping->iova = paddr - memory_base;
	} else {
		struct drm_mm_node *node = &mapping->vram_node;
		int ret;

		ret = etnaviv_iommu_find_iova(context, node, size);
		if (ret < 0) {
			context->lock->unlock();
			return ret;
		}

		mapping->iova = node->start;
		ret = etnaviv_context_map(context, node->start, paddr, size,
					  ETNAVIV_PROT_READ);
		if (ret < 0) {
			context->mm.alloc.Dealloc(node->start);
			context->lock->unlock();
			return ret;
		}

		context->flush_seq++;
	}

	context->mappings[mapping->vram_node.start] = mapping;
	mapping->use = 1;

	context->lock->unlock();

	return 0;
}

void etnaviv_iommu_put_suballoc_va(struct etnaviv_iommu_context *context,
		  std::shared_ptr<etnaviv_vram_mapping> mapping)
{
	struct drm_mm_node *node = &mapping->vram_node;

	context->lock->lock();
	mapping->use--;

	if (mapping->use > 0 || context->global->version == ETNAVIV_IOMMU_V1) {
		context->lock->unlock();
		return;
	}

	etnaviv_context_unmap(context, node->start, node->length);
	context->mm.alloc.Dealloc(node->start);
	context->lock->unlock();
}

size_t etnaviv_iommu_dump_size(struct etnaviv_iommu_context *context)
{
	return context->dump_size();
}

void etnaviv_iommu_dump(struct etnaviv_iommu_context *context, void *buf)
{
	context->dump(buf);
}

int etnaviv_iommu_global_init(struct etnaviv_gpu *gpu)
{
	enum etnaviv_iommu_version version = ETNAVIV_IOMMU_V1;
	auto &priv = gpu->drm->dev_private;

	if (gpu->identity.minor_features1 & chipMinorFeatures1_MMU_VERSION)
		version = ETNAVIV_IOMMU_V2;

	if (priv->mmu_global) {
		if (priv->mmu_global->version != version) {
			dev_err(gpu->dev,
				"MMU version doesn't match global version\n");
			return -ENXIO;
		}

		priv->mmu_global->use++;
		return 0;
	}

	priv->mmu_global = std::make_unique<etnaviv_iommu_global>();
	auto &global = priv->mmu_global;
	if (!global)
		return -ENOMEM;

	global->bad_page_cpu = dma_alloc_wc(nullptr, 4096, &global->bad_page_dma,
					    GFP_KERNEL);
	if (!global->bad_page_cpu)
		goto free_global;

	memset32(global->bad_page_cpu, 0xdead55aa, 4096 / sizeof(u32));

	if (version == ETNAVIV_IOMMU_V2) {
		global->v2.pta_cpu = (u64 *)dma_alloc_wc(nullptr, ETNAVIV_PTA_SIZE,
					       &global->v2.pta_dma, GFP_KERNEL);
		if (!global->v2.pta_cpu)
			goto free_bad_page;
	}

	global->gpu = gpu;
	global->version = version;
	global->use = 1;

	return 0;

free_bad_page:
	dma_free_wc(nullptr, 4096, global->bad_page_cpu, global->bad_page_dma);
free_global:
	priv->mmu_global = nullptr;

	return -ENOMEM;
}

etnaviv_iommu_global::~etnaviv_iommu_global()
{
	if (v2.pta_cpu)
		dma_free_wc(nullptr, ETNAVIV_PTA_SIZE,
			    v2.pta_cpu, v2.pta_dma);

	if (bad_page_cpu)
		dma_free_wc(nullptr, 4096,
			    bad_page_cpu, bad_page_dma);
}
