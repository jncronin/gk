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
		unmapped_page = context->global->ops->unmap(context, iova,
							    pgsize);
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
		ret = context->global->ops->map(context, iova, paddr, pgsize,
						prot);
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
	unsigned int i;
	int ret;

	if (!context)
		return -EINVAL;

	for(auto &sge : sgt) {
		phys_addr_t pa = sge.paddr;
		unsigned int da_len = sge.len;
		unsigned int bytes = std::min(da_len, va_len);

		VERB("map[%d]: %08x %pap(%x)", i, da, &pa, bytes);

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
	struct etnaviv_vram_mapping *mapping)
{
	struct etnaviv_gem_object *etnaviv_obj = mapping->object;

	BUG_ON(!context->lock->held());

	etnaviv_iommu_unmap(context, mapping->vram_node.start,
			    etnaviv_obj->sgt, etnaviv_obj->size);
	context->mm.alloc.Dealloc(mapping->vram_node.start);
}

void etnaviv_iommu_reap_mapping(struct etnaviv_vram_mapping *mapping)
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
			if (!free.second.vram_node.mm)
				continue;

			/*
			 * If the iova is pinned, then it's in-use,
			 * so we must keep its mapping.
			 */
			if (free.second.use)
				continue;

			auto &cmap = free.second.vram_node;
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
					cur_evicted += cmapping->second.vram_node.length;
					etnaviv_iommu_reap_mapping(&cmapping->second);

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
	struct etnaviv_vram_mapping *m, *n;
	struct drm_mm_node *scan_node;
	LIST_HEAD(scan_list);
	int ret;

	lockdep_assert_held(&context->lock);

	ret = drm_mm_insert_node_in_range(&context->mm, node, size, 0, 0, va,
					  va + size, DRM_MM_INSERT_LOWEST);
	if (ret != -ENOSPC)
		return ret;

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

	drm_mm_for_each_node_in_range(scan_node, &context->mm, va, va + size) {
		m = container_of(scan_node, struct etnaviv_vram_mapping,
				 vram_node);

		if (m->use)
			return -ENOSPC;

		list_add(&m->scan_node, &scan_list);
	}

	list_for_each_entry_safe(m, n, &scan_list, scan_node) {
		etnaviv_iommu_reap_mapping(m);
		list_del_init(&m->scan_node);
	}

	return drm_mm_insert_node_in_range(&context->mm, node, size, 0, 0, va,
					   va + size, DRM_MM_INSERT_LOWEST);
}

int etnaviv_iommu_map_gem(struct etnaviv_iommu_context *context,
	struct etnaviv_gem_object *etnaviv_obj, u32 memory_base,
	struct etnaviv_vram_mapping *mapping, u64 va)
{
	struct sg_table *sgt = etnaviv_obj->sgt;
	struct drm_mm_node *node;
	int ret;

	lockdep_assert_held(&etnaviv_obj->lock);

	mutex_lock(&context->lock);

	/* v1 MMU can optimize single entry (contiguous) scatterlists */
	if (context->global->version == ETNAVIV_IOMMU_V1 &&
	    sgt->nents == 1 && !(etnaviv_obj->flags & ETNA_BO_FORCE_MMU)) {
		u32 iova;

		iova = sg_dma_address(sgt->sgl) - memory_base;
		if (iova < 0x80000000 - sg_dma_len(sgt->sgl)) {
			mapping->iova = iova;
			mapping->context = etnaviv_iommu_context_get(context);
			list_add_tail(&mapping->mmu_node, &context->mappings);
			ret = 0;
			goto unlock;
		}
	}

	node = &mapping->vram_node;

	if (va)
		ret = etnaviv_iommu_insert_exact(context, node, etnaviv_obj->size, va);
	else
		ret = etnaviv_iommu_find_iova(context, node, etnaviv_obj->size);
	if (ret < 0)
		goto unlock;

	mapping->iova = node->start;
	ret = etnaviv_iommu_map(context, node->start, etnaviv_obj->size, sgt,
				ETNAVIV_PROT_READ | ETNAVIV_PROT_WRITE);

	if (ret < 0) {
		drm_mm_remove_node(node);
		goto unlock;
	}

	mapping->context = etnaviv_iommu_context_get(context);
	list_add_tail(&mapping->mmu_node, &context->mappings);
unlock:
	mutex_unlock(&context->lock);

	return ret;
}

void etnaviv_iommu_unmap_gem(struct etnaviv_iommu_context *context,
	struct etnaviv_vram_mapping *mapping)
{
	WARN_ON(mapping->use);

	mutex_lock(&context->lock);

	/* Bail if the mapping has been reaped by another thread */
	if (!mapping->context) {
		mutex_unlock(&context->lock);
		return;
	}

	/* If the vram node is on the mm, unmap and remove the node */
	if (mapping->vram_node.mm == &context->mm)
		etnaviv_iommu_remove_mapping(context, mapping);

	list_del(&mapping->mmu_node);
	mutex_unlock(&context->lock);
	etnaviv_iommu_context_put(context);
}

static void etnaviv_iommu_context_free(struct kref *kref)
{
	struct etnaviv_iommu_context *context =
		container_of(kref, struct etnaviv_iommu_context, refcount);

	etnaviv_cmdbuf_suballoc_unmap(context, &context->cmdbuf_mapping);
	mutex_destroy(&context->lock);
	context->global->ops->free(context);
}
void etnaviv_iommu_context_put(struct etnaviv_iommu_context *context)
{
	kref_put(&context->refcount, etnaviv_iommu_context_free);
}

struct etnaviv_iommu_context *
etnaviv_iommu_context_init(struct etnaviv_iommu_global *global,
			   struct etnaviv_cmdbuf_suballoc *suballoc)
{
	struct etnaviv_iommu_context *ctx;
	int ret;

	if (global->version == ETNAVIV_IOMMU_V1)
		ctx = etnaviv_iommuv1_context_alloc(global);
	else
		ctx = etnaviv_iommuv2_context_alloc(global);

	if (!ctx)
		return NULL;

	ret = etnaviv_cmdbuf_suballoc_map(suballoc, ctx, &ctx->cmdbuf_mapping,
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
	etnaviv_cmdbuf_suballoc_unmap(ctx, &ctx->cmdbuf_mapping);
out_free:
	global->ops->free(ctx);
	return NULL;
}

void etnaviv_iommu_restore(struct etnaviv_gpu *gpu,
			   struct etnaviv_iommu_context *context)
{
	context->global->ops->restore(gpu, context);
}

int etnaviv_iommu_get_suballoc_va(struct etnaviv_iommu_context *context,
				  struct etnaviv_vram_mapping *mapping,
				  u32 memory_base, dma_addr_t paddr,
				  size_t size)
{
	mutex_lock(&context->lock);

	if (mapping->use > 0) {
		mapping->use++;
		mutex_unlock(&context->lock);
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
			mutex_unlock(&context->lock);
			return ret;
		}

		mapping->iova = node->start;
		ret = etnaviv_context_map(context, node->start, paddr, size,
					  ETNAVIV_PROT_READ);
		if (ret < 0) {
			drm_mm_remove_node(node);
			mutex_unlock(&context->lock);
			return ret;
		}

		context->flush_seq++;
	}

	list_add_tail(&mapping->mmu_node, &context->mappings);
	mapping->use = 1;

	mutex_unlock(&context->lock);

	return 0;
}

void etnaviv_iommu_put_suballoc_va(struct etnaviv_iommu_context *context,
		  struct etnaviv_vram_mapping *mapping)
{
	struct drm_mm_node *node = &mapping->vram_node;

	mutex_lock(&context->lock);
	mapping->use--;

	if (mapping->use > 0 || context->global->version == ETNAVIV_IOMMU_V1) {
		mutex_unlock(&context->lock);
		return;
	}

	etnaviv_context_unmap(context, node->start, node->size);
	drm_mm_remove_node(node);
	mutex_unlock(&context->lock);
}

size_t etnaviv_iommu_dump_size(struct etnaviv_iommu_context *context)
{
	return context->global->ops->dump_size(context);
}

void etnaviv_iommu_dump(struct etnaviv_iommu_context *context, void *buf)
{
	context->global->ops->dump(context, buf);
}

int etnaviv_iommu_global_init(struct etnaviv_gpu *gpu)
{
	enum etnaviv_iommu_version version = ETNAVIV_IOMMU_V1;
	struct etnaviv_drm_private *priv = gpu->drm->dev_private;
	struct etnaviv_iommu_global *global;
	struct device *dev = gpu->drm->dev;

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

	global = kzalloc_obj(*global);
	if (!global)
		return -ENOMEM;

	global->bad_page_cpu = dma_alloc_wc(dev, SZ_4K, &global->bad_page_dma,
					    GFP_KERNEL);
	if (!global->bad_page_cpu)
		goto free_global;

	memset32(global->bad_page_cpu, 0xdead55aa, SZ_4K / sizeof(u32));

	if (version == ETNAVIV_IOMMU_V2) {
		global->v2.pta_cpu = dma_alloc_wc(dev, ETNAVIV_PTA_SIZE,
					       &global->v2.pta_dma, GFP_KERNEL);
		if (!global->v2.pta_cpu)
			goto free_bad_page;
	}

	global->dev = dev;
	global->version = version;
	global->use = 1;
	mutex_init(&global->lock);

	if (version == ETNAVIV_IOMMU_V1)
		global->ops = &etnaviv_iommuv1_ops;
	else
		global->ops = &etnaviv_iommuv2_ops;

	priv->mmu_global = global;

	return 0;

free_bad_page:
	dma_free_wc(dev, SZ_4K, global->bad_page_cpu, global->bad_page_dma);
free_global:
	kfree(global);

	return -ENOMEM;
}

void etnaviv_iommu_global_fini(struct etnaviv_gpu *gpu)
{
	struct etnaviv_drm_private *priv = gpu->drm->dev_private;
	struct etnaviv_iommu_global *global = priv->mmu_global;

	if (!global)
		return;

	if (--global->use > 0)
		return;

	if (global->v2.pta_cpu)
		dma_free_wc(global->dev, ETNAVIV_PTA_SIZE,
			    global->v2.pta_cpu, global->v2.pta_dma);

	if (global->bad_page_cpu)
		dma_free_wc(global->dev, SZ_4K,
			    global->bad_page_cpu, global->bad_page_dma);

	mutex_destroy(&global->lock);
	kfree(global);

	priv->mmu_global = NULL;
}
