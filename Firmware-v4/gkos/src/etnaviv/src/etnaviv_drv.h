/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015-2018 Etnaviv Project
 */

#ifndef __ETNAVIV_DRV_H__
#define __ETNAVIV_DRV_H__

//#include <linux/io.h>
//#include <linux/list.h>
//#include <linux/mm_types.h>
//#include <linux/sizes.h>
//#include <linux/time64.h>
#include "linux_types.h"
//#include <linux/xarray.h>

//#include <drm/drm_drv.h>
//#include <drm/drm_gem.h>
#include "etnaviv_drm.h"
//#include <drm/gpu_scheduler.h>

#include <memory>

struct etnaviv_cmdbuf;
struct etnaviv_gpu;
struct etnaviv_mmu;
struct etnaviv_gem_object;
struct etnaviv_gem_submit;
struct etnaviv_iommu_global;

#define ETNAVIV_SOFTPIN_START_ADDRESS	(4*1024*1024ul) /* must be >= SUBALLOC_SIZE */

struct etnaviv_iommu_context;

struct etnaviv_file_private {
	int id;
	std::shared_ptr<etnaviv_iommu_context> mmu;
	struct drm_sched_entity		sched_entity;
};

struct etnaviv_cmdbuf_suballoc;

struct etnaviv_drm_private {
	int num_gpus = 0;
	std::unique_ptr<etnaviv_gpu> gpu;
	gfp_t shm_gfp_mask = GFP_HIGHUSER;

	std::unique_ptr<etnaviv_cmdbuf_suballoc> cmdbuf_suballoc;
	std::unique_ptr<etnaviv_iommu_global> mmu_global;

	std::vector<void *> active_contexts;
	u32 next_context_id;

	/* list of GEM objects: */
	std::shared_ptr<Mutex> gem_lock = MutexList.Create();
	struct list_head gem_list;

	/* ppu flop reset data */
	std::unique_ptr<etnaviv_cmdbuf> flop_reset_data_ppu;
};

int etnaviv_ioctl_gem_submit(struct drm_device *dev, void *data,
		struct drm_file *file);

int etnaviv_gem_mmap_offset(struct drm_gem_object *obj, u64 *offset);
sg_table etnaviv_gem_prime_get_sg_table(struct drm_gem_object *obj);
int etnaviv_gem_prime_vmap(struct drm_gem_object *obj, struct iosys_map *map);
struct drm_gem_object *etnaviv_gem_prime_import_sg_table(struct drm_device *dev,
	struct dma_buf_attachment *attach, sg_table *sg);
int etnaviv_gem_prime_pin(struct drm_gem_object *obj);
void etnaviv_gem_prime_unpin(struct drm_gem_object *obj);
void *etnaviv_gem_vmap(struct drm_gem_object *obj);
int etnaviv_gem_cpu_prep(struct drm_gem_object *obj, u32 op,
		struct drm_etnaviv_timespec *timeout);
int etnaviv_gem_cpu_fini(struct drm_gem_object *obj);
void etnaviv_gem_free_object(struct drm_gem_object *obj);
int etnaviv_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		u32 size, u32 flags, u32 *handle);
int etnaviv_gem_new_userptr(struct drm_device *dev, struct drm_file *file,
	uintptr_t ptr, u32 size, u32 flags, u32 *handle);
u16 etnaviv_buffer_init(struct etnaviv_gpu *gpu);
u16 etnaviv_buffer_config_mmuv2(struct etnaviv_gpu *gpu, u32 mtlb_addr, u32 safe_addr);
u16 etnaviv_buffer_config_pta(struct etnaviv_gpu *gpu, unsigned short id);
void etnaviv_buffer_end(struct etnaviv_gpu *gpu);
void etnaviv_sync_point_queue(struct etnaviv_gpu *gpu, unsigned int event);
void etnaviv_buffer_queue(struct etnaviv_gpu *gpu, u32 exec_state,
	struct etnaviv_iommu_context *mmu,
	unsigned int event, struct etnaviv_cmdbuf *cmdbuf);
void etnaviv_validate_init(void);
bool etnaviv_cmd_validate_one(struct etnaviv_gpu *gpu,
	u32 *stream, unsigned int size,
	struct drm_etnaviv_gem_submit_reloc *relocs, unsigned int reloc_size);

#ifdef CONFIG_DEBUG_FS
void etnaviv_gem_describe_objects(struct etnaviv_drm_private *priv,
	struct seq_file *m);
#endif

#define DBG(fmt, ...) DRM_DEBUG(fmt"\n", ##__VA_ARGS__)
#define VERB(fmt, ...) if (1) DRM_DEBUG(fmt"\n", ##__VA_ARGS__)

/*
 * Return the storage size of a structure with a variable length array.
 * The array is nelem elements of elem_size, where the base structure
 * is defined by base.  If the size overflows size_t, return zero.
 */
static inline size_t size_vstruct(size_t nelem, size_t elem_size, size_t base)
{
	if (elem_size && nelem > (SIZE_MAX - base) / elem_size)
		return 0;
	return base + nelem * elem_size;
}

#endif /* __ETNAVIV_DRV_H__ */
