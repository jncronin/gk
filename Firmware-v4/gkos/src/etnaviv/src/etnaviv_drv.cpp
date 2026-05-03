// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015-2018 Etnaviv Project
 */

//#include <linux/component.h>
//#include <linux/dma-mapping.h>
//#include <linux/module.h>
//#include <linux/of.h>
//#include <linux/of_device.h>
//#include <linux/platform_device.h>
//#include <linux/uaccess.h>

//#include <drm/drm_debugfs.h>
//#include <drm/drm_drv.h>
//#include <drm/drm_file.h>
//#include <drm/drm_ioctl.h>
//#include <drm/drm_of.h>
//#include <drm/drm_prime.h>
//#include <drm/drm_print.h>

#include "etnaviv_cmdbuf.h"
#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"
#include "etnaviv_gem.h"
#include "etnaviv_mmu.h"
#include "etnaviv_perfmon.h"
#include "etnaviv_drm.h"
#include "etnaviv_sched.h"

std::atomic<uint32_t> next_context_id = 0;
std::atomic<uint32_t> next_fence_id = 0;

/*
 * DRM operations:
 */

int etnaviv_open(struct drm_device *dev, std::shared_ptr<drm_file> *file)
{
	auto ctx = std::make_shared<etnaviv_file_private>();
	if (!ctx)
		return -ENOMEM;

	auto gpu = static_cast<etnaviv_gpu *>(dev);

	ctx->id = next_context_id.fetch_add(1);

	ctx->mmu = etnaviv_iommu_context_init(gpu->mmu_global.get(),
					      gpu->cmdbuf_suballoc.get());
	if (!ctx->mmu) {
		return -ENOMEM;
	}

	*file = std::move(ctx);

	return 0;
}

etnaviv_file_private::~etnaviv_file_private()
{
	klog("etnaviv: close file context id %d\n", id);
}

[[maybe_unused]] static void etnaviv_postclose(struct drm_device *dev, struct drm_file *file)
{
#if 0
	struct etnaviv_drm_private *priv = dev->dev_private;
	struct etnaviv_file_private *ctx = file->driver_priv;
	unsigned int i;

	for (i = 0; i < ETNA_MAX_PIPES; i++) {
		struct etnaviv_gpu *gpu = priv->gpu[i];

		if (gpu)
			drm_sched_entity_destroy(&ctx->sched_entity[i]);
	}

	etnaviv_iommu_context_put(ctx->mmu);

	xa_erase(&priv->active_contexts, ctx->id);

	kfree(ctx);
#endif
}

/*
 * DRM debugfs:
 */

#ifdef CONFIG_DEBUG_FS
static int etnaviv_gem_show(struct drm_device *dev, struct seq_file *m)
{
	struct etnaviv_drm_private *priv = dev->dev_private;

	etnaviv_gem_describe_objects(priv, m);

	return 0;
}

static int etnaviv_mm_show(struct drm_device *dev, struct seq_file *m)
{
	struct drm_printer p = drm_seq_file_printer(m);

	read_lock(&dev->vma_offset_manager->vm_lock);
	drm_mm_print(&dev->vma_offset_manager->vm_addr_space_mm, &p);
	read_unlock(&dev->vma_offset_manager->vm_lock);

	return 0;
}

static int etnaviv_mmu_show(struct etnaviv_gpu *gpu, struct seq_file *m)
{
	struct drm_printer p = drm_seq_file_printer(m);
	struct etnaviv_iommu_context *mmu_context;

	seq_printf(m, "Active Objects (%s):\n", dev_name(gpu->dev));

	/*
	 * Lock the GPU to avoid a MMU context switch just now and elevate
	 * the refcount of the current context to avoid it disappearing from
	 * under our feet.
	 */
	mutex_lock(&gpu->lock);
	mmu_context = gpu->mmu_context;
	if (mmu_context)
		etnaviv_iommu_context_get(mmu_context);
	mutex_unlock(&gpu->lock);

	if (!mmu_context)
		return 0;

	mutex_lock(&mmu_context->lock);
	drm_mm_print(&mmu_context->mm, &p);
	mutex_unlock(&mmu_context->lock);

	etnaviv_iommu_context_put(mmu_context);

	return 0;
}

static void etnaviv_buffer_dump(struct etnaviv_gpu *gpu, struct seq_file *m)
{
	struct etnaviv_cmdbuf *buf = &gpu->buffer;
	u32 size = buf->size;
	u32 *ptr = buf->vaddr;
	u32 i;

	seq_printf(m, "virt %p - phys 0x%llx - free 0x%08x\n",
			buf->vaddr, (u64)etnaviv_cmdbuf_get_pa(buf),
			size - buf->user_size);

	for (i = 0; i < size / 4; i++) {
		if (i && !(i % 4))
			seq_puts(m, "\n");
		if (i % 4 == 0)
			seq_printf(m, "\t0x%p: ", ptr + i);
		seq_printf(m, "%08x ", *(ptr + i));
	}
	seq_puts(m, "\n");
}

static int etnaviv_ring_show(struct etnaviv_gpu *gpu, struct seq_file *m)
{
	seq_printf(m, "Ring Buffer (%s): ", dev_name(gpu->dev));

	mutex_lock(&gpu->lock);
	etnaviv_buffer_dump(gpu, m);
	mutex_unlock(&gpu->lock);

	return 0;
}

static int show_unlocked(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	int (*show)(struct drm_device *dev, struct seq_file *m) =
			node->info_ent->data;

	return show(dev, m);
}

static int show_each_gpu(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct etnaviv_drm_private *priv = dev->dev_private;
	struct etnaviv_gpu *gpu;
	int (*show)(struct etnaviv_gpu *gpu, struct seq_file *m) =
			node->info_ent->data;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < ETNA_MAX_PIPES; i++) {
		gpu = priv->gpu[i];
		if (!gpu)
			continue;

		ret = show(gpu, m);
		if (ret < 0)
			break;
	}

	return ret;
}

static struct drm_info_list etnaviv_debugfs_list[] = {
	{"gpu", show_each_gpu, 0, etnaviv_gpu_debugfs},
	{"gem", show_unlocked, 0, etnaviv_gem_show},
	{ "mm", show_unlocked, 0, etnaviv_mm_show },
	{"mmu", show_each_gpu, 0, etnaviv_mmu_show},
	{"ring", show_each_gpu, 0, etnaviv_ring_show},
};

static void etnaviv_debugfs_init(struct drm_minor *minor)
{
	drm_debugfs_create_files(etnaviv_debugfs_list,
				 ARRAY_SIZE(etnaviv_debugfs_list),
				 minor->debugfs_root, minor);
}
#endif

/*
 * DRM ioctls:
 */

static int etnaviv_ioctl_get_param(struct drm_device *dev, void *data,
		std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_param *args = reinterpret_cast<drm_etnaviv_param *>(data);

	if (args->pipe >= ETNA_MAX_PIPES)
		return -EINVAL;

	if (!dev)
		return -ENXIO;

	auto gpu = static_cast<etnaviv_gpu *>(dev);

	return etnaviv_gpu_get_param(*gpu, args->param, &args->value);
}

static int etnaviv_ioctl_gem_new(struct drm_device *dev, void *data,
		std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_gem_new *args = reinterpret_cast<drm_etnaviv_gem_new *>(data);

	if (args->flags & ~(ETNA_BO_CACHED | ETNA_BO_WC | ETNA_BO_UNCACHED |
			    ETNA_BO_FORCE_MMU))
		return -EINVAL;

	return etnaviv_gem_new_handle(dev, file, args->size,
			args->flags, &args->handle);
}

static int etnaviv_ioctl_gem_cpu_prep(struct drm_device *dev, void *data,
		std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_gem_cpu_prep *args = reinterpret_cast<drm_etnaviv_gem_cpu_prep *>(data);
	int ret;

	if (args->op & ~(ETNA_PREP_READ | ETNA_PREP_WRITE | ETNA_PREP_NOSYNC))
		return -EINVAL;

	auto obj = drm_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	ret = etnaviv_gem_cpu_prep(std::reinterpret_pointer_cast<etnaviv_gem_object>(obj),
		args->op, &args->timeout);

	//drm_gem_object_put(obj);

	return ret;
}

static int etnaviv_ioctl_gem_cpu_fini(struct drm_device *dev, void *data,
		std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_gem_cpu_fini *args = reinterpret_cast<drm_etnaviv_gem_cpu_fini *>(data);
	int ret;

	if (args->flags)
		return -EINVAL;

	auto obj = drm_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	ret = etnaviv_gem_cpu_fini(std::reinterpret_pointer_cast<etnaviv_gem_object>(obj));

	return ret;
}

static int etnaviv_ioctl_gem_info(struct drm_device *dev, void *data,
		std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_gem_info *args = reinterpret_cast<drm_etnaviv_gem_info *>(data);
	int ret;

	if (args->pad)
		return -EINVAL;

	auto obj = drm_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	ret = etnaviv_gem_mmap_offset(std::reinterpret_pointer_cast<etnaviv_gem_object>(obj),
		&args->offset);

	return ret;
}

static int etnaviv_ioctl_wait_fence(struct drm_device *dev, void *data,
		std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_wait_fence *args = reinterpret_cast<drm_etnaviv_wait_fence *>(data);
	[[maybe_unused]] struct drm_etnaviv_timespec *timeout = &args->timeout;

	if (args->flags & ~(ETNA_WAIT_NONBLOCK))
		return -EINVAL;

	if (args->pipe >= ETNA_MAX_PIPES)
		return -EINVAL;

	if (!dev)
		return -ENXIO;
	auto gpu = static_cast<etnaviv_gpu *>(dev);

	if (args->flags & ETNA_WAIT_NONBLOCK)
		timeout = NULL;

	return etnaviv_gpu_wait_fence_interruptible(gpu, args->fence,
						    timeout);
}

static int etnaviv_ioctl_gem_userptr(struct drm_device *dev, void *data,
	std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_gem_userptr *args = reinterpret_cast<drm_etnaviv_gem_userptr *>(data);

	if (args->flags & ~(ETNA_USERPTR_READ|ETNA_USERPTR_WRITE) ||
	    args->flags == 0)
		return -EINVAL;

	if (offset_in_page(args->user_ptr | args->user_size) ||
	    (uintptr_t)args->user_ptr != args->user_ptr ||
	    (u32)args->user_size != args->user_size ||
	    args->user_ptr & ~PAGE_MASK)
		return -EINVAL;

	if (!access_ok((void __user *)(unsigned long)args->user_ptr,
		       args->user_size))
		return -EFAULT;

#if 0
	return etnaviv_gem_new_userptr(dev, file, args->user_ptr,
				       args->user_size, args->flags,
				       &args->handle);
#endif
	klog("ioctl_gem_userptr not implemented\n");
	return -ENOTSUP;
}

static int etnaviv_ioctl_gem_wait(struct drm_device *dev, void *data,
	std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_gem_wait *args = reinterpret_cast<drm_etnaviv_gem_wait *>(data);
	[[maybe_unused]] struct drm_etnaviv_timespec *timeout = &args->timeout;
	[[maybe_unused]] struct drm_gem_object *obj;
	int ret;

	if (args->flags & ~(ETNA_WAIT_NONBLOCK))
		return -EINVAL;

	if (args->pipe >= ETNA_MAX_PIPES)
		return -EINVAL;

	if (!dev)
		return -ENXIO;

#if 0
	obj = drm_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	if (args->flags & ETNA_WAIT_NONBLOCK)
		timeout = NULL;

	ret = etnaviv_gem_wait_bo(gpu, obj, timeout);

	drm_gem_object_put(obj);
#endif
	klog("ioctl_gem_wait not implemented\n");
	return -ENOTSUP;

	return ret;
}

static int etnaviv_ioctl_pm_query_dom(struct drm_device *dev, void *data,
	std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_pm_domain *args = reinterpret_cast<drm_etnaviv_pm_domain *>(data);

	if (args->pipe >= ETNA_MAX_PIPES)
		return -EINVAL;

	if (!dev)
		return -ENXIO;
	auto gpu = static_cast<etnaviv_gpu *>(dev);

	return etnaviv_pm_query_dom(gpu, args);
}

static int etnaviv_ioctl_pm_query_sig(struct drm_device *dev, void *data,
	std::shared_ptr<drm_file> &file)
{
	struct drm_etnaviv_pm_signal *args = reinterpret_cast<drm_etnaviv_pm_signal *>(data);

	if (args->pipe >= ETNA_MAX_PIPES)
		return -EINVAL;

	if (!dev)
		return -ENXIO;
	auto gpu = static_cast<etnaviv_gpu *>(dev);

	return etnaviv_pm_query_sig(gpu, args);
}

extern const struct drm_ioctl_desc etnaviv_ioctls[] = {
	{ DRM_IOCTL_ETNAVIV_GET_PARAM,    DRM_RENDER_ALLOW, etnaviv_ioctl_get_param,    "ETNAVIV_GET_PARAM", sizeof(drm_etnaviv_param) },
	{ 1, 0, nullptr, "", 0 },
	{ DRM_IOCTL_ETNAVIV_GEM_NEW,      DRM_RENDER_ALLOW, etnaviv_ioctl_gem_new,      "ETNAVIV_GEM_NEW", sizeof(drm_etnaviv_gem_new) },
	{ DRM_IOCTL_ETNAVIV_GEM_INFO,     DRM_RENDER_ALLOW, etnaviv_ioctl_gem_info,     "ETNAVIV_GEM_INFO", sizeof(drm_etnaviv_gem_info) },
	{ DRM_IOCTL_ETNAVIV_GEM_CPU_PREP, DRM_RENDER_ALLOW, etnaviv_ioctl_gem_cpu_prep, "ETNAVIV_GEM_CPU_PREP", sizeof(drm_etnaviv_gem_cpu_prep) },
	{ DRM_IOCTL_ETNAVIV_GEM_CPU_FINI, DRM_RENDER_ALLOW, etnaviv_ioctl_gem_cpu_fini, "ETNAVIV_GEM_CPU_FINI", sizeof(drm_etnaviv_gem_cpu_fini) },
	{ DRM_IOCTL_ETNAVIV_GEM_SUBMIT,   DRM_RENDER_ALLOW, etnaviv_ioctl_gem_submit,   "ETNAVIV_GEM_SUBMIT", sizeof(drm_etnaviv_gem_submit) },
	{ DRM_IOCTL_ETNAVIV_WAIT_FENCE,   DRM_RENDER_ALLOW, etnaviv_ioctl_wait_fence,   "ETNAVIV_WAIT_FENCE", sizeof(drm_etnaviv_wait_fence) },
	{ DRM_IOCTL_ETNAVIV_GEM_USERPTR,  DRM_RENDER_ALLOW, etnaviv_ioctl_gem_userptr,  "ETNAVIV_GEM_USERPTR", sizeof(drm_etnaviv_gem_userptr) },
	{ DRM_IOCTL_ETNAVIV_GEM_WAIT,     DRM_RENDER_ALLOW, etnaviv_ioctl_gem_wait,     "ETNAVIV_GEM_WAIT", sizeof(drm_etnaviv_gem_wait) },
	{ DRM_IOCTL_ETNAVIV_PM_QUERY_DOM, DRM_RENDER_ALLOW, etnaviv_ioctl_pm_query_dom, "ETNAVIV_PM_QUERY_DOM", sizeof(drm_etnaviv_pm_domain) },
	{ DRM_IOCTL_ETNAVIV_PM_QUERY_SIG, DRM_RENDER_ALLOW, etnaviv_ioctl_pm_query_sig, "ETNAVIV_PM_QUERY_SIG", sizeof(drm_etnaviv_pm_signal) },
};
