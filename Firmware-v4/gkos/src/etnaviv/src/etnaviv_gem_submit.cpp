#include "linux_types.h"


// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2015 Etnaviv Project
 */

//#include <drm/drm_file.h>
//#include <drm/drm_print.h>
//#include <linux/dma-fence-array.h>
//#include <linux/file.h>
//#include <linux/dma-resv.h>
//#include <linux/sync_file.h>
//#include <linux/uaccess.h>
//#include <linux/vmalloc.h>

#include "etnaviv_cmdbuf.h"
#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"
#include "etnaviv_gem.h"
#include "etnaviv_perfmon.h"
#include "etnaviv_sched.h"
#include <cstring>
#include "process.h"
#include "waitwound.h"
#include "fencefile.h"

/*
 * Cmdstream submission:
 */

#define BO_INVALID_FLAGS ~(ETNA_SUBMIT_BO_READ | ETNA_SUBMIT_BO_WRITE)
/* make sure these don't conflict w/ ETNAVIV_SUBMIT_BO_x */
#define BO_LOCKED   0x4000
#define BO_PINNED   0x2000

std::shared_ptr<etnaviv_gem_submit> submit_create(struct drm_device *dev,
		struct etnaviv_gpu *gpu, size_t nr_bos, size_t nr_pmrs)
{
	auto submit = std::make_shared<etnaviv_gem_submit>();
	if (!submit)
		return NULL;

	submit->bos.resize(nr_bos);
	submit->pmrs.resize(nr_pmrs);

	submit->nr_bos = nr_bos;
	submit->nr_pmrs = nr_pmrs;
	submit->gpu = gpu;

	return submit;
}

static int submit_lookup_objects(struct etnaviv_gem_submit *submit,
	std::shared_ptr<drm_file> &f, struct drm_etnaviv_gem_submit_bo *submit_bos,
	unsigned nr_bos)
{
	struct drm_etnaviv_gem_submit_bo *bo;
	unsigned i;
	int ret = 0;

	// We use the spin_lock in the drm_gem.cpp file
	//spin_lock(&file->table_lock);

	for (i = 0, bo = submit_bos; i < nr_bos; i++, bo++) {
		if (bo->flags & BO_INVALID_FLAGS) {
			DRM_ERROR("invalid flags: %x\n", bo->flags);
			ret = -EINVAL;
			goto out_unlock;
		}

		submit->bos[i].flags = bo->flags;
		if (submit->flags & ETNA_SUBMIT_SOFTPIN) {
			if (bo->presumed < ETNAVIV_SOFTPIN_START_ADDRESS) {
				DRM_ERROR("invalid softpin address\n");
				ret = -EINVAL;
				goto out_unlock;
			}
			submit->bos[i].va = bo->presumed;
		}

		/* normally use drm_gem_object_lookup(), but for bulk lookup
		 * all under single table_lock just hit object_idr directly:
		 */
		submit->bos[i].obj = std::reinterpret_pointer_cast<etnaviv_gem_object>(
			drm_gem_object_lookup(f, bo->handle));
		if (!submit->bos[i].obj) {
			DRM_ERROR("invalid handle %u at index %u\n",
				  bo->handle, i);
			ret = -EINVAL;
			goto out_unlock;
		}
	}

out_unlock:
	submit->nr_bos = i;
	//spin_unlock(&file->table_lock);

	return ret;
}



static void submit_unlock_object(struct etnaviv_gem_submit *submit, int i)
{
	if (submit->bos[i].flags & BO_LOCKED) {
		auto &obj = submit->bos[i].obj;

		dma_resv_unlock(obj->resv);
		submit->bos[i].flags &= ~BO_LOCKED;
	}
}

static int submit_lock_objects(struct etnaviv_gem_submit *submit,
		WaitWoundContext &ticket)
{
	int contended, slow_locked = -1, i, ret = 0;

retry:
	for (i = 0; i < (int)submit->nr_bos; i++) {
		auto &obj = submit->bos[i].obj;

		if (slow_locked == i)
			slow_locked = -1;

		contended = i;

		if (!(submit->bos[i].flags & BO_LOCKED)) {
			ret = dma_resv_lock_interruptible(obj->resv, ticket);
			if (ret == -EALREADY)
				DRM_ERROR("BO at index %u already on submit list\n",
					  i);
			if (ret)
				goto fail;
			submit->bos[i].flags |= BO_LOCKED;
		}
	}

	ticket.acquire_done();

	return 0;

fail:
	for (; i >= 0; i--)
		submit_unlock_object(submit, i);

	if (slow_locked > 0)
		submit_unlock_object(submit, slow_locked);

	if (ret == -EDEADLK) {
		auto &obj = submit->bos[contended].obj;

		/* we lost out in a seqno race, lock and retry.. */
		ret = dma_resv_lock_slow_interruptible(obj->resv, ticket);
		if (!ret) {
			submit->bos[contended].flags |= BO_LOCKED;
			slow_locked = contended;
			goto retry;
		}
	}

	return ret;
}

static int submit_fence_sync(struct etnaviv_gem_submit *submit)
{
	int i, ret = 0;

	for (i = 0; i < (int)submit->nr_bos; i++) {
		auto &bo = submit->bos[i];
		auto &robj = bo.obj->resv;

		/* not required - we use a std::list */
		/* ret = dma_resv_reserve_fences(robj, 1);
		if (ret)
			return ret; */

		if (submit->flags & ETNA_SUBMIT_NO_IMPLICIT)
			continue;

		/* I _think_ this function adds all the fences within the resv objects
			to the dependency list of the sched_job
			
			If write == true, must wait for all fences
			If write == false, just need to wait for previous writer to finish
			
			We remove any fences from the lists that are already signalled to prevent
			these lists growing on every frame */

		/* We already have the lock on the resv objects */
		if(bo.flags & ETNA_SUBMIT_BO_WRITE)
		{
			for(auto iter = robj.write_fences.begin(); iter != robj.write_fences.end(); )
			{
				auto &dep = *iter;
				if(dep->IsSignalled())
					iter = robj.write_fences.erase(iter);
				else
				{
					submit->sched_job->deps.push_back(dep);
					iter++;
				}
			}
		}
		for(auto iter = robj.read_fences.begin(); iter != robj.read_fences.end(); )
		{
			auto &dep = *iter;
			if(dep->IsSignalled())
				iter = robj.read_fences.erase(iter);
			else
			{
				submit->sched_job->deps.push_back(dep);
				iter++;
			}
		}

		/*
		ret = drm_sched_job_add_implicit_dependencies(&submit->sched_job,
							      &bo->obj->base,
							      bo->flags & ETNA_SUBMIT_BO_WRITE);
		if (ret)
			return ret; */
	}

	return ret;
}

static void submit_attach_object_fences(struct etnaviv_gem_submit *submit)
{
	int i;

	for (i = 0; i < (int)submit->nr_bos; i++) {
		auto &obj = submit->bos[i].obj;
		bool write = submit->bos[i].flags & ETNA_SUBMIT_BO_WRITE;

		if(write)
			obj->resv.write_fences.push_back(submit->out_fence);
		else
			obj->resv.read_fences.push_back(submit->out_fence);

		if(obj->resv.write_fences.size() > 100)
			klog("resv.write_fences.size() = %u\n", obj->resv.write_fences.size());
		if(obj->resv.read_fences.size() > 100)
			klog("resv.read_fences.size() = %u\n", obj->resv.read_fences.size());

		submit_unlock_object(submit, i);
	}
}

static int submit_pin_objects(struct etnaviv_gem_submit *submit)
{
	int ret = 0;

#if GPU_DEBUG > 3
	klog("GPU: submit: %u objects\n", submit->nr_bos);
#endif

	for (auto i = 0u; i < submit->nr_bos; i++) {
		auto &etnaviv_obj = submit->bos[i].obj;

		auto mapping = etnaviv_gem_mapping_get(etnaviv_obj,
						  submit->mmu_context,
						  submit->bos[i].va);
		if (!mapping) {
			DRM_ERROR("object does not have a mapping\n");
			return -1;
		}

#if GPU_DEBUG > 3
		klog("obj: %u, %08x/%08x/%08x va @ %p phys\n", 
			i, mapping->iova, mapping->vram_node.start, submit->bos[i].va,
			submit->bos[i].obj->dma_addr);
#endif

		if ((submit->flags & ETNA_SUBMIT_SOFTPIN) &&
		     submit->bos[i].va != mapping->iova) {
			etnaviv_gem_mapping_unreference(mapping.get());
			DRM_ERROR("softpin with incorrect va\n");
			return -EINVAL;
		}

		etnaviv_obj->gpu_active.fetch_add(1);

		submit->bos[i].flags |= BO_PINNED;
		submit->bos[i].mapping = mapping;
	}

	return ret;
}

static int submit_bo(struct etnaviv_gem_submit *submit, u32 idx,
	struct etnaviv_gem_submit_bo **bo)
{
	if (idx >= submit->nr_bos) {
		DRM_ERROR("invalid buffer index: %u (out of %u)\n",
				idx, submit->nr_bos);
		return -EINVAL;
	}

	*bo = &submit->bos[idx];

	return 0;
}

/* process the reloc's and patch up the cmdstream as needed: */
static int submit_reloc(struct etnaviv_gem_submit *submit, void *stream,
		u32 size, const struct drm_etnaviv_gem_submit_reloc *relocs,
		u32 nr_relocs)
{
	u32 i, last_offset = 0;
	u32 *ptr = (u32 *)stream;
	int ret;

#if GPU_DEBUG > 3
	klog("GPU: submit: %u relocs\n", nr_relocs);
#endif

	/* Submits using softpin don't blend with relocs */
	if ((submit->flags & ETNA_SUBMIT_SOFTPIN) && nr_relocs != 0)
	{
		DRM_ERROR("softpin with relocs\n");
		return -EINVAL;
	}

	for (i = 0; i < nr_relocs; i++) {
		const struct drm_etnaviv_gem_submit_reloc *r = relocs + i;
		struct etnaviv_gem_submit_bo *bo;
		u32 off;

		if (unlikely(r->flags)) {
			DRM_ERROR("invalid reloc flags\n");
			return -EINVAL;
		}

		if (r->submit_offset % 4) {
			DRM_ERROR("non-aligned reloc offset: %u\n",
				  r->submit_offset);
			return -EINVAL;
		}

		/* offset in dwords: */
		off = r->submit_offset / 4;

		if ((off >= size ) ||
				(off < last_offset)) {
			DRM_ERROR("invalid offset %u at reloc %u\n", off, i);
			return -EINVAL;
		}

		ret = submit_bo(submit, r->reloc_idx, &bo);
		if (ret)
			return ret;

		if (r->reloc_offset > bo->obj->psize - sizeof(*ptr)) {
			DRM_ERROR("relocation %u outside object\n", i);
			return -EINVAL;
		}

		ptr[off] = bo->mapping->iova + r->reloc_offset;

#if GPU_DEBUG > 3
		klog("stream offset %u -> %08x + %u\n", off, bo->mapping->iova, r->reloc_offset);
#endif

		last_offset = off;
	}

	return 0;
}

static int submit_perfmon_validate(struct etnaviv_gem_submit *submit,
		u32 exec_state, const struct drm_etnaviv_gem_submit_pmr *pmrs)
{
	u32 i;

	for (i = 0; i < submit->nr_pmrs; i++) {
		const struct drm_etnaviv_gem_submit_pmr *r = pmrs + i;
		struct etnaviv_gem_submit_bo *bo;
		int ret;

		ret = submit_bo(submit, r->read_idx, &bo);
		if (ret)
			return ret;

		/* at offset 0 a sequence number gets stored used for userspace sync */
		if (r->read_offset == 0) {
			DRM_ERROR("perfmon request: offset is 0");
			return -EINVAL;
		}

		if (r->read_offset >= bo->obj->psize - sizeof(u32)) {
			DRM_ERROR("perfmon request: offset %u outside object", i);
			return -EINVAL;
		}

		if (r->flags & ~(ETNA_PM_PROCESS_PRE | ETNA_PM_PROCESS_POST)) {
			DRM_ERROR("perfmon request: flags are not valid");
			return -EINVAL;
		}

		if (etnaviv_pm_req_validate(r, exec_state)) {
			DRM_ERROR("perfmon request: domain or signal not valid");
			return -EINVAL;
		}

		submit->pmrs[i].flags = r->flags;
		submit->pmrs[i].domain = r->domain;
		submit->pmrs[i].signal = r->signal;
		submit->pmrs[i].sequence = r->sequence;
		submit->pmrs[i].offset = r->read_offset;
		submit->pmrs[i].bo_vma = (u32 *)etnaviv_gem_vmap(bo->obj);
	}

	return 0;
}


etnaviv_gem_submit::~etnaviv_gem_submit()
{
	auto submit = this;

	unsigned i;

	if (submit->cmdbuf.suballoc)
		etnaviv_cmdbuf_free(&submit->cmdbuf);

	for (i = 0; i < submit->nr_bos; i++) {
		auto etnaviv_obj = submit->bos[i].obj;

		/* unpin all objects */
		if (submit->bos[i].flags & BO_PINNED) {
			etnaviv_gem_mapping_unreference(submit->bos[i].mapping.get());
			etnaviv_obj->gpu_active.fetch_sub(1);
			submit->bos[i].mapping = NULL;
			submit->bos[i].flags &= ~BO_PINNED;
		}

		/* if the GPU submit failed, objects might still be locked */
		submit_unlock_object(submit, i);
	}
}

int etnaviv_ioctl_gem_submit(struct drm_device *dev, void *data,
		std::shared_ptr<drm_file> &file)
{
	//struct etnaviv_file_private *ctx = file->driver_priv;
	[[maybe_unused]] struct etnaviv_drm_private *priv = dev->dev_private.get();
	struct drm_etnaviv_gem_submit *args = reinterpret_cast<drm_etnaviv_gem_submit *>(data);
	std::shared_ptr<etnaviv_gem_submit> submit;
	struct etnaviv_gpu *gpu;
	std::shared_ptr<drm_file> fil = file;
	int out_fence_fd = -1;
	auto pid = GetCurrentProcessForCore()->id;
	int ret = 0;
	WaitWoundContext ticket;


	if (args->pipe >= ETNA_MAX_PIPES)
	{
		DRM_ERROR("invalid pipe\n");
		return -EINVAL;
	}

	gpu = priv->gpu.get();
	if (!gpu)
	{
		DRM_ERROR("no device\n");
		return -ENXIO;
	}

	if (args->stream_size % 4) {
		DRM_ERROR("non-aligned cmdstream buffer size: %u\n",
			  args->stream_size);
		return -EINVAL;
	}

	if (args->exec_state != ETNA_PIPE_3D &&
	    args->exec_state != ETNA_PIPE_2D &&
	    args->exec_state != ETNA_PIPE_VG) {
		DRM_ERROR("invalid exec_state: 0x%x\n", args->exec_state);
		return -EINVAL;
	}

	if (args->flags & ~ETNA_SUBMIT_FLAGS) {
		DRM_ERROR("invalid flags: 0x%x\n", args->flags);
		return -EINVAL;
	}

	if ((args->flags & ETNA_SUBMIT_SOFTPIN) &&
	    priv->mmu_global->version != ETNAVIV_IOMMU_V2) {
		DRM_ERROR("softpin requested on incompatible MMU\n");
		return -EINVAL;
	}

	if (args->stream_size > SZ_128K || args->nr_relocs > SZ_128K ||
	    args->nr_bos > SZ_128K || args->nr_pmrs > 128) {
		DRM_ERROR("submit arguments out of size limits\n");
		return -EINVAL;
	}

	/*
	 * Copy the command submission and bo array to kernel space in
	 * one go, and do this outside of any locks.
	 */
	std::unique_ptr<drm_etnaviv_gem_submit_bo[]> bos(new drm_etnaviv_gem_submit_bo[args->nr_bos]);
	std::unique_ptr<drm_etnaviv_gem_submit_reloc[]> relocs(new drm_etnaviv_gem_submit_reloc[args->nr_relocs]);
	std::unique_ptr<drm_etnaviv_gem_submit_pmr[]> pmrs(new drm_etnaviv_gem_submit_pmr[args->nr_pmrs]);
	std::unique_ptr<uint8_t[]> stream(new uint8_t[args->stream_size]);
	if (!bos || !relocs || !pmrs || !stream) {
		ret = -ENOMEM;
		DRM_ERROR("no mem\n");
		goto err_submit_cmds;
	}

	memcpy(bos.get(), (void *)args->bos, args->nr_bos * sizeof(drm_etnaviv_gem_submit_bo));
	memcpy(relocs.get(), (void *)args->relocs, args->nr_relocs * sizeof(drm_etnaviv_gem_submit_reloc));
	memcpy(pmrs.get(), (void *)args->pmrs, args->nr_pmrs * sizeof(drm_etnaviv_gem_submit_pmr));
	memcpy(stream.get(), (void *)args->stream, args->stream_size);

	if (args->flags & ETNA_SUBMIT_FENCE_FD_OUT) {
		auto p = GetCurrentProcessForCore();
		CriticalGuard cg(p->open_files.sl);
		out_fence_fd = p->open_files.get_free_fildes();
		if (out_fence_fd < 0) {
			ret = out_fence_fd;
			DRM_ERROR("no out fence\n");
			goto err_submit_cmds;
		}
		fence_open(&p->open_files.f[out_fence_fd]);
	}

	/* 
		So this is complex.

		The ww (wait/wound) system essentially tries to prevent deadlocks with 
		multiple threads trying to lock the gem buffers.  There is a call later
		(submit_lock_objects) that tries to acquire a mutex within each
		drm_gem_obejct (the resv member) that is listed in bos[].

		When we inic the ww system (ww_acquire_ticket), we are given a ticket
		which is essentially a timestamp.  When we lock each mutex, this timestamp
		is stored in the mutex.  If we don't lock the mutex, we compare our
		timestamp with that of already in the lock.

		If we are younger, we must release all our locked mutexes and try again.
		In particular, for the failing mutex, we must go down a slow path and
		wait in line
		
		If we are older, we simply wait for the other thread to release the mutex -
		this may be because it has finished doing its work, or it may be because it
		has deadlocked by trying to acquire a mutex which we already have, but
		because it has a younger ticket, it has released everything allowing us to
		pass.
	*/

	submit = submit_create(dev, gpu, args->nr_bos, args->nr_pmrs);
	if (!submit) {
		ret = -ENOMEM;
		DRM_ERROR("submit_create failed\n");
		goto err_submit_ww_acquire;
	}
	submit->pid = pid;

	ret = etnaviv_cmdbuf_init(priv->cmdbuf_suballoc.get(), &submit->cmdbuf,
				  ALIGN(args->stream_size, 8) + 8);
	if (ret)
	{
		DRM_ERROR("cmdbuf_init failed\n");
		goto err_submit_put;
	}

	submit->ctx = std::static_pointer_cast<etnaviv_file_private>(fil);
	submit->mmu_context = submit->ctx->mmu;
	submit->exec_state = args->exec_state;
	submit->flags = args->flags;

	submit->sched_job = std::make_unique<etnaviv_sched_job>();

	/*ret = drm_sched_job_init(&submit->sched_job,
				 &ctx->sched_entity[args->pipe],
				 1, submit->ctx, file->client_id);
	if (ret)
		goto err_submit_put; */

	ret = submit_lookup_objects(submit.get(), file, bos.get(), args->nr_bos);
	if (ret)
	{
		DRM_ERROR("lookup_objects failed\n");
		goto err_submit_job;
	}

	if ((priv->mmu_global->version != ETNAVIV_IOMMU_V2) &&
	    !etnaviv_cmd_validate_one(gpu, (u32 *)stream.get(), args->stream_size / 4,
				      relocs.get(), args->nr_relocs)) {
		DRM_ERROR("etnaviv_cmd_validate_one failed\n");
		ret = -EINVAL;
		goto err_submit_job;
	}

	if (args->flags & ETNA_SUBMIT_FENCE_FD_IN) {
		auto in_fence = sync_file_get_fence(args->fence_fd);
		if (!in_fence) {
			DRM_ERROR("in fence not found\n");
			ret = -EINVAL;
			goto err_submit_job;
		}

		submit->sched_job->deps.push_back(std::move(in_fence));
	}

	ret = submit_pin_objects(submit.get());
	if (ret)
	{
		DRM_ERROR("pin_objects failed\n");
		goto err_submit_job;
	}

	ret = submit_reloc(submit.get(), stream.get(), args->stream_size / 4,
			   relocs.get(), args->nr_relocs);
	if (ret)
	{
		DRM_ERROR("submit_reloc failed\n");
		goto err_submit_job;
	}

	ret = submit_perfmon_validate(submit.get(), args->exec_state, pmrs.get());
	if (ret)
	{
		DRM_ERROR("perfmon_validate failed\n");
		goto err_submit_job;
	}

	memcpy(submit->cmdbuf.vaddr, stream.get(), args->stream_size);

	ret = submit_lock_objects(submit.get(), ticket);
	if (ret)
	{
		DRM_ERROR("lock_objects failed\n");
		goto err_submit_job;
	}

	ret = submit_fence_sync(submit.get());
	if (ret)
	{
		DRM_ERROR("fence_sync failed\n");
		goto err_submit_job;
	}

	ret = etnaviv_sched_push_job(submit);
	if (ret)
	{
		DRM_ERROR("sched_push_job failed\n");
		goto err_submit_job;
	}

	submit_attach_object_fences(submit.get());

	args->fence_fd = out_fence_fd;
	args->fence = submit->out_fence_id;

err_submit_job:
	if (ret)
	{
		submit->sched_job->scheduled = nullptr;
		submit->sched_job->finished = nullptr;
		submit->sched_job->deps.clear();
	}
err_submit_put:

err_submit_ww_acquire:


err_submit_cmds:
	if (ret && (out_fence_fd >= 0))
	{
		auto p = GetCurrentProcessForCore();
		CriticalGuard cg(p->open_files.sl);
		p->open_files.f[out_fence_fd] = nullptr;
	}

	return ret;
}
