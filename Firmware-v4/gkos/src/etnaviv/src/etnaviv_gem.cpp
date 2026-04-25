#include "linux_types.h"


// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015-2018 Etnaviv Project
 */

//#include <drm/drm_prime.h>
//#include <drm/drm_print.h>
//#include <linux/dma-mapping.h>
//#include <linux/shmem_fs.h>
//#include <linux/spinlock.h>
//#include <linux/vmalloc.h>

#include "etnaviv_drv.h"
#include "etnaviv_gem.h"
#include "etnaviv_gpu.h"
#include "etnaviv_mmu.h"
#include "vmem.h"
#include "cache.h"
#include "process.h"

#if 0

static struct lock_class_key etnaviv_shm_lock_class;
static struct lock_class_key etnaviv_userptr_lock_class;

#endif

#if 0
static void etnaviv_gem_scatterlist_unmap(struct etnaviv_gem_object *etnaviv_obj)
{
	struct drm_device *dev = etnaviv_obj->base.dev;
	struct sg_table *sgt = etnaviv_obj->sgt;

	/*
	 * For non-cached buffers, ensure the new pages are clean
	 * because display controller, GPU, etc. are not coherent:
	 *
	 * WARNING: The DMA API does not support concurrent CPU
	 * and device access to the memory area.  With BIDIRECTIONAL,
	 * we will clean the cache lines which overlap the region,
	 * and invalidate all cache lines (partially) contained in
	 * the region.
	 *
	 * If you have dirty data in the overlapping cache lines,
	 * that will corrupt the GPU-written data.  If you have
	 * written into the remainder of the region, this can
	 * discard those writes.
	 */
	if (etnaviv_obj->flags & ETNA_BO_CACHE_MASK)
		dma_unmap_sgtable(dev->dev, sgt, DMA_BIDIRECTIONAL, 0);
}

#endif
/* called with etnaviv_obj->lock held */
static int etnaviv_gem_shmem_get_pages(std::shared_ptr<etnaviv_gem_object> &etnaviv_obj)
{
 /*
	struct drm_device *dev = etnaviv_obj->base.dev;
	struct page **p = drm_gem_get_pages(&etnaviv_obj->base);

	if (IS_ERR(p)) {
		dev_dbg(dev->dev, "could not get pages: %ld\n", PTR_ERR(p));
		return PTR_ERR(p);
	}

	etnaviv_obj->pages = p;
	*/

	return 0;
}

[[maybe_unused]] static void put_pages(struct etnaviv_gem_object *etnaviv_obj)
{
	/*
	if (etnaviv_obj->sgt) {
		etnaviv_gem_scatterlist_unmap(etnaviv_obj);
		sg_free_table(etnaviv_obj->sgt);
		kfree(etnaviv_obj->sgt);
		etnaviv_obj->sgt = NULL;
	}
	if (etnaviv_obj->pages) {
		drm_gem_put_pages(&etnaviv_obj->base, etnaviv_obj->pages,
				  true, false);

		etnaviv_obj->pages = NULL;
	} */
}

static int etnaviv_gem_mmap_obj(std::shared_ptr<etnaviv_gem_object> &obj,
		struct vm_area_struct *vma)
{
#if 0
	pgprot_t vm_page_prot;

	vm_flags_set(vma, VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);

	vm_page_prot = vm_get_page_prot(vma->vm_flags);

	if (etnaviv_obj->flags & ETNA_BO_WC) {
		vma->vm_page_prot = pgprot_writecombine(vm_page_prot);
	} else if (etnaviv_obj->flags & ETNA_BO_UNCACHED) {
		vma->vm_page_prot = pgprot_noncached(vm_page_prot);
	} else {
		/*
		 * Shunt off cached objs to shmem file so they have their own
		 * address_space (so unmap_mapping_range does what we want,
		 * in particular in the case of mmap'd dmabufs)
		 */
		vma->vm_pgoff = 0;
		vma_set_file(vma, etnaviv_obj->base.filp);

		vma->vm_page_prot = vm_page_prot;
	}
#endif

	return 0;
}

static int etnaviv_gem_mmap(std::shared_ptr<drm_gem_object> &obj, struct vm_area_struct *vma)
{
	auto o2 = obj;
	auto eobj = std::reinterpret_pointer_cast<etnaviv_gem_object>(o2);
	return eobj->ops->mmap(eobj, vma);
}

int etnaviv_gem_mmap_offset(std::shared_ptr<etnaviv_gem_object> obj, u64 *offset)
{
	int ret;

	/* Make it mmapable */
	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		dev_err(obj->dev->dev, "could not allocate mmap offset\n");
	else
		*offset = drm_vma_node_offset_addr(&obj->vma_node);

	return ret;
}


std::shared_ptr<etnaviv_vram_mapping>
etnaviv_gem_get_vram_mapping(struct etnaviv_gem_object *obj,
			     struct etnaviv_iommu_context *context,
				bool delete_from_list = false)
{
	for(auto iter = obj->vram_list.begin(); iter != obj->vram_list.end(); iter++)
	{
		auto &mapping = *iter;
		if(mapping->context.get() == context)
		{
			auto cmapping = mapping;
			if(delete_from_list)
				obj->vram_list.erase(iter);
			return cmapping;
		}
	}

	return nullptr;
}

void etnaviv_gem_mapping_unreference(struct etnaviv_vram_mapping *mapping)
{
	struct etnaviv_gem_object *etnaviv_obj = mapping->object;

	etnaviv_obj->lock->lock();
	WARN_ON(mapping->use == 0);
	mapping->use -= 1;
	etnaviv_obj->lock->unlock();
}

std::shared_ptr<etnaviv_vram_mapping>
etnaviv_gem_mapping_get(
	std::shared_ptr<etnaviv_gem_object> &obj,
	std::shared_ptr<etnaviv_iommu_context> &mmu_context,
	u64 va)
{
	auto &etnaviv_obj = obj;
	int ret = 0;

	etnaviv_obj->lock->lock();
	auto mapping = etnaviv_gem_get_vram_mapping(etnaviv_obj.get(), mmu_context.get());
	if (mapping) {
		/*
		 * Holding the object lock prevents the use count changing
		 * beneath us.  If the use count is zero, the MMU might be
		 * reaping this object, so take the lock and re-check that
		 * the MMU owns this mapping to close this race.
		 */
		if (mapping->use == 0) {
			mmu_context->lock->lock();
			if (mapping->context == mmu_context)
				if (va && mapping->iova != va) {
					etnaviv_iommu_reap_mapping(mapping);
					mapping = nullptr;
				} else {
					mapping->use += 1;
				}
			else
				mapping = NULL;
			mmu_context->lock->unlock();
			if (mapping)
				goto out;
		} else {
			mapping->use += 1;
			goto out;
		}
	}

	/*
	 * See if we have a reaped vram mapping we can re-use before
	 * allocating a fresh mapping.
	 */
	mapping = etnaviv_gem_get_vram_mapping(etnaviv_obj.get(), NULL, true);
	if (!mapping) {
		mapping = std::make_shared<etnaviv_vram_mapping>();
		if (!mapping) {
			DRM_ERROR("could not allocate etnaviv_vram_mapping\n");
			ret = -ENOMEM;
			goto out;
		}

		mapping->object = etnaviv_obj.get();
	}

	mapping->use = 1;

	ret = etnaviv_iommu_map_gem(mmu_context, etnaviv_obj.get(),
				    mmu_context->global->memory_base,
				    mapping, va);
	if (ret >= 0)
		etnaviv_obj->vram_list.push_back(mapping);

	if(ret != 0)
	{
		DRM_ERROR("etnaviv_iommu_map_gem returned %d\n", ret);
	}

out:
	etnaviv_obj->lock->unlock();

	if (ret)
		return nullptr;

	return mapping;
}

void *etnaviv_gem_vmap(std::shared_ptr<etnaviv_gem_object> obj)
{
	auto &etnaviv_obj = obj;

	if (etnaviv_obj->vaddr)
		return etnaviv_obj->vaddr;

	etnaviv_obj->lock->lock();
	/*
	 * Need to check again, as we might have raced with another thread
	 * while waiting for the mutex.
	 */
	if (!etnaviv_obj->vaddr)
		etnaviv_obj->vaddr = etnaviv_obj->ops->vmap(etnaviv_obj);
	etnaviv_obj->lock->unlock();

	return etnaviv_obj->vaddr;
}

static void *etnaviv_gem_vmap_impl(std::shared_ptr<etnaviv_gem_object> &obj)
{
	BUG_ON(!obj->lock->held());

	switch (obj->flags & ETNA_BO_CACHE_MASK) {
	case ETNA_BO_CACHED:
		obj->vaddr = (void *)PMEM_TO_VMEM(obj->dma_addr);	
		break;
	default:
		obj->vaddr = (void *)PMEM_TO_VMEM_NC(obj->dma_addr);
		break;
	}

	return obj->vaddr;
}

static inline enum dma_data_direction etnaviv_op_to_dma_dir(u32 op)
{
	op &= ETNA_PREP_READ | ETNA_PREP_WRITE;

	if (op == ETNA_PREP_READ)
		return DMA_FROM_DEVICE;
	else if (op == ETNA_PREP_WRITE)
		return DMA_TO_DEVICE;
	else
		return DMA_BIDIRECTIONAL;
}

int etnaviv_gem_cpu_prep(std::shared_ptr<etnaviv_gem_object> obj, u32 op,
		struct drm_etnaviv_timespec *timeout)
{
	auto &etnaviv_obj = obj;
	[[maybe_unused]] bool write = !!(op & ETNA_PREP_WRITE);
	[[maybe_unused]] int ret;

	// TODO: add dma sync
#if 0
	if (op & ETNA_PREP_NOSYNC) {
		if (!dma_resv_test_signaled(obj->resv,
					    dma_resv_usage_rw(write)))
			return -EBUSY;
	} else {
		unsigned long remain = etnaviv_timeout_to_jiffies(timeout);

		ret = dma_resv_wait_timeout(obj->resv, dma_resv_usage_rw(write),
					    true, remain);
		if (ret <= 0)
			return ret == 0 ? -ETIMEDOUT : ret;
	}
#endif

	if (etnaviv_obj->flags & ETNA_BO_CACHED) {
		dma_sync_sgtable_for_cpu(etnaviv_obj->sgt,
					 etnaviv_op_to_dma_dir(op));
		etnaviv_obj->last_cpu_prep_op = op;
	}

	return 0;
}


int etnaviv_gem_cpu_fini(std::shared_ptr<etnaviv_gem_object> obj)
{
	auto &etnaviv_obj = obj;

	if (etnaviv_obj->flags & ETNA_BO_CACHED) {
		/* fini without a prep is almost certainly a userspace error */
		WARN_ON(etnaviv_obj->last_cpu_prep_op == 0);
		dma_sync_sgtable_for_device(etnaviv_obj->sgt,
			etnaviv_op_to_dma_dir(etnaviv_obj->last_cpu_prep_op));
		etnaviv_obj->last_cpu_prep_op = 0;
	}

	return 0;
}

int etnaviv_gem_wait_bo(struct etnaviv_gpu *gpu, struct drm_gem_object *obj,
	struct drm_etnaviv_timespec *timeout)
{
	struct etnaviv_gem_object *etnaviv_obj = to_etnaviv_bo(obj);

	return etnaviv_gpu_wait_obj_inactive(gpu, etnaviv_obj, timeout);
}

#ifdef CONFIG_DEBUG_FS
static void etnaviv_gem_describe(struct drm_gem_object *obj, struct seq_file *m)
{
	struct etnaviv_gem_object *etnaviv_obj = to_etnaviv_bo(obj);
	struct dma_resv *robj = obj->resv;
	unsigned long off = drm_vma_node_start(&obj->vma_node);
	int r;

	seq_printf(m, "%08x: %c %2d (%2d) %08lx %p %zd\n",
			etnaviv_obj->flags, is_active(etnaviv_obj) ? 'A' : 'I',
			obj->name, kref_read(&obj->refcount),
			off, etnaviv_obj->vaddr, obj->size);

	r = dma_resv_lock(robj, NULL);
	if (r)
		return;

	dma_resv_describe(robj, m);
	dma_resv_unlock(robj);
}

void etnaviv_gem_describe_objects(struct etnaviv_drm_private *priv,
	struct seq_file *m)
{
	struct etnaviv_gem_object *etnaviv_obj;
	int count = 0;
	size_t size = 0;

	mutex_lock(&priv->gem_lock);
	list_for_each_entry(etnaviv_obj, &priv->gem_list, gem_node) {
		struct drm_gem_object *obj = &etnaviv_obj->base;

		seq_puts(m, "   ");
		etnaviv_gem_describe(obj, m);
		count++;
		size += obj->size;
	}
	mutex_unlock(&priv->gem_lock);

	seq_printf(m, "Total %d objects, %zu bytes\n", count, size);
}
#endif

static void etnaviv_gem_shmem_release(std::shared_ptr<etnaviv_gem_object> &etnaviv_obj)
{
	//vunmap(etnaviv_obj->vaddr);
	//put_pages(etnaviv_obj);
}

static const struct etnaviv_gem_ops etnaviv_gem_shmem_ops = {
	.get_pages = etnaviv_gem_shmem_get_pages,
	.release = etnaviv_gem_shmem_release,
	.vmap = etnaviv_gem_vmap_impl,
	.mmap = etnaviv_gem_mmap_obj,
};

void etnaviv_gem_free_object(struct drm_gem_object *obj)
{
	klog("etnaviv_gem_free_object not implemented\n");
	return;

#if 0
	struct etnaviv_gem_object *etnaviv_obj = to_etnaviv_bo(obj);
	struct etnaviv_drm_private *priv = obj->dev->dev_private;
	struct etnaviv_vram_mapping *mapping, *tmp;

	/* object should not be active */
	WARN_ON(is_active(etnaviv_obj));

	mutex_lock(&priv->gem_lock);
	list_del(&etnaviv_obj->gem_node);
	mutex_unlock(&priv->gem_lock);

	list_for_each_entry_safe(mapping, tmp, &etnaviv_obj->vram_list,
				 obj_node) {
		struct etnaviv_iommu_context *context = mapping->context;

		WARN_ON(mapping->use);

		if (context)
			etnaviv_iommu_unmap_gem(context, mapping);

		list_del(&mapping->obj_node);
		kfree(mapping);
	}

	etnaviv_obj->ops->release(etnaviv_obj);
	drm_gem_object_release(obj);

	mutex_destroy(&etnaviv_obj->lock);
	kfree(etnaviv_obj);
#endif
}

void etnaviv_gem_obj_add(struct drm_device *dev, std::shared_ptr<etnaviv_gem_object> obj)
{
	struct etnaviv_drm_private *priv = dev->dev_private.get();

	priv->gem_lock->lock();
	priv->gem_list.push_back(obj);
	priv->gem_lock->unlock();
}

[[maybe_unused]] static enum drm_gem_object_status etnaviv_gem_status(struct drm_gem_object *obj)
{
	return DRM_GEM_OBJECT_RESIDENT;
}

static const struct vm_operations_struct vm_ops = {
};

static const struct drm_gem_object_funcs etnaviv_gem_object_funcs = {
	.free = etnaviv_gem_free_object,
	.pin = etnaviv_gem_prime_pin,
	.unpin = etnaviv_gem_prime_unpin,
	.get_sg_table = etnaviv_gem_prime_get_sg_table,
	.vmap = etnaviv_gem_prime_vmap,
	.mmap = etnaviv_gem_mmap,
	.vm_ops = &vm_ops,
};

static int etnaviv_gem_new_impl(struct drm_device *dev, u32 size, u32 flags,
	std::shared_ptr<etnaviv_gem_object> *obj)
{
	bool valid = true;

	/* validate flags */
	switch (flags & ETNA_BO_CACHE_MASK) {
	case ETNA_BO_UNCACHED:
	case ETNA_BO_CACHED:
	case ETNA_BO_WC:
		break;
	default:
		valid = false;
	}

	if (!valid) {
		dev_err(dev->dev, "invalid cache flag: %x\n",
			(flags & ETNA_BO_CACHE_MASK));
		return -EINVAL;
	}

	auto etnaviv_obj = std::make_shared<etnaviv_gem_object>();
	if (!etnaviv_obj)
		return -ENOMEM;

	etnaviv_obj->size = ALIGN(size, SZ_4K);
	etnaviv_obj->flags = flags;
	etnaviv_obj->mt = ((flags & ETNA_BO_CACHE_MASK) == ETNA_BO_CACHED) ? MT_NORMAL :
		MT_NORMAL_NC;

	*obj = etnaviv_obj;

	return 0;
}

/* convenience method to construct a GEM buffer object, and userspace handle */
int etnaviv_gem_new_handle(struct drm_device *dev, std::shared_ptr<drm_file> &f,
	u32 size, u32 flags, u32 *handle)
{
	std::shared_ptr<etnaviv_gem_object> obj;
	int ret;

	ret = etnaviv_gem_new_impl(dev, size, flags, &obj);
	if (ret)
		goto fail;

	//lockdep_set_class(&to_etnaviv_bo(obj)->lock, &etnaviv_shm_lock_class);

	ret = drm_gem_object_init(dev, obj, PAGE_ALIGN(size));
	if (ret)
		goto fail;

	/*
	 * Our buffers are kept pinned, so allocating them from the MOVABLE
	 * zone is a really bad idea, and conflicts with CMA. See comments
	 * above new_inode() why this is required _and_ expected if you're
	 * going to pin these pages.
	 */
	//mapping_set_gfp_mask(obj->filp->f_mapping, priv->shm_gfp_mask);

	etnaviv_gem_obj_add(dev, obj);

	ret = drm_gem_handle_create(f, obj, handle);

	/* drop reference from allocate - handle holds it now */
fail:
	if(ret) klog("etnaviv_gem_new_handle failing: %d\n", ret);

	return ret;
}
#if 0
int etnaviv_gem_new_private(struct drm_device *dev, size_t size, u32 flags,
	const struct etnaviv_gem_ops *ops, struct etnaviv_gem_object **res)
{
	struct drm_gem_object *obj;
	int ret;

	ret = etnaviv_gem_new_impl(dev, size, flags, ops, &obj);
	if (ret)
		return ret;

	drm_gem_private_object_init(dev, obj, size);

	*res = to_etnaviv_bo(obj);

	return 0;
}

static int etnaviv_gem_userptr_get_pages(struct etnaviv_gem_object *etnaviv_obj)
{
	struct page **pvec = NULL;
	struct etnaviv_gem_userptr *userptr = &etnaviv_obj->userptr;
	int ret, pinned = 0, npages = etnaviv_obj->base.size >> PAGE_SHIFT;
	unsigned int gup_flags = FOLL_LONGTERM;

	might_lock_read(&current->mm->mmap_lock);

	if (userptr->mm != current->mm)
		return -EPERM;

	pvec = kvmalloc_objs(struct page *, npages);
	if (!pvec)
		return -ENOMEM;

	if (!userptr->ro)
		gup_flags |= FOLL_WRITE;

	do {
		unsigned num_pages = npages - pinned;
		uint64_t ptr = userptr->ptr + pinned * PAGE_SIZE;
		struct page **pages = pvec + pinned;

		ret = pin_user_pages_fast(ptr, num_pages, gup_flags, pages);
		if (ret < 0) {
			unpin_user_pages(pvec, pinned);
			kvfree(pvec);
			return ret;
		}

		pinned += ret;

	} while (pinned < npages);

	etnaviv_obj->pages = pvec;

	return 0;
}

static void etnaviv_gem_userptr_release(struct etnaviv_gem_object *etnaviv_obj)
{
	if (etnaviv_obj->sgt) {
		etnaviv_gem_scatterlist_unmap(etnaviv_obj);
		sg_free_table(etnaviv_obj->sgt);
		kfree(etnaviv_obj->sgt);
	}
	if (etnaviv_obj->pages) {
		unsigned int npages = etnaviv_obj->base.size >> PAGE_SHIFT;

		unpin_user_pages(etnaviv_obj->pages, npages);
		kvfree(etnaviv_obj->pages);
	}
}

static int etnaviv_gem_userptr_mmap_obj(struct etnaviv_gem_object *etnaviv_obj,
		struct vm_area_struct *vma)
{
	return -EINVAL;
}

static const struct etnaviv_gem_ops etnaviv_gem_userptr_ops = {
	.get_pages = etnaviv_gem_userptr_get_pages,
	.release = etnaviv_gem_userptr_release,
	.vmap = etnaviv_gem_vmap_impl,
	.mmap = etnaviv_gem_userptr_mmap_obj,
};

int etnaviv_gem_new_userptr(struct drm_device *dev, struct drm_file *file,
	uintptr_t ptr, u32 size, u32 flags, u32 *handle)
{
	struct etnaviv_gem_object *etnaviv_obj;
	int ret;

	ret = etnaviv_gem_new_private(dev, size, ETNA_BO_CACHED,
				      &etnaviv_gem_userptr_ops, &etnaviv_obj);
	if (ret)
		return ret;

	lockdep_set_class(&etnaviv_obj->lock, &etnaviv_userptr_lock_class);

	etnaviv_obj->userptr.ptr = ptr;
	etnaviv_obj->userptr.mm = current->mm;
	etnaviv_obj->userptr.ro = !(flags & ETNA_USERPTR_WRITE);

	etnaviv_gem_obj_add(dev, &etnaviv_obj->base);

	ret = drm_gem_handle_create(file, &etnaviv_obj->base, handle);

	/* drop reference from allocate - handle holds it now */
	drm_gem_object_put(&etnaviv_obj->base);
	return ret;
}

#endif
