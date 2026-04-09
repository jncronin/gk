/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015-2018 Etnaviv Project
 */

#ifndef __ETNAVIV_GPU_H__
#define __ETNAVIV_GPU_H__

#include "etnaviv_cmdbuf.h"
#include "etnaviv_gem.h"
#include "etnaviv_mmu.h"
#include "etnaviv_drv.h"
#include "common.xml.h"
#include "state.xml.h"

#include "drm_scheduler.h"
#include "workqueue.h"
#include "user_fences.h"

#include "osqueue.h"

struct etnaviv_gem_submit;
struct etnaviv_vram_mapping;

struct etnaviv_chip_identity {
	u32 model;
	u32 revision;
	u32 product_id;
	u32 customer_id;
	u32 eco_id;

	/* Supported feature fields. */
	u32 features;

	/* Supported minor feature fields. */
	u32 minor_features0;
	u32 minor_features1;
	u32 minor_features2;
	u32 minor_features3;
	u32 minor_features4;
	u32 minor_features5;
	u32 minor_features6;
	u32 minor_features7;
	u32 minor_features8;
	u32 minor_features9;
	u32 minor_features10;
	u32 minor_features11;

	/* Number of streams supported. */
	u32 stream_count;

	/* Total number of temporary registers per thread. */
	u32 register_max;

	/* Maximum number of threads. */
	u32 thread_count;

	/* Number of shader cores. */
	u32 shader_core_count;

	/* Number of Neural Network cores. */
	u32 nn_core_count;

	/* Size of the vertex cache. */
	u32 vertex_cache_size;

	/* Number of entries in the vertex output buffer. */
	u32 vertex_output_buffer_size;

	/* Number of pixel pipes. */
	u32 pixel_pipes;

	/* Number of instructions. */
	u32 instruction_count;

	/* Number of constants. */
	u32 num_constants;

	/* Buffer size */
	u32 buffer_size;

	/* Number of varyings */
	u8 varyings_count;
};

enum etnaviv_sec_mode {
	ETNA_SEC_NONE = 0,
	ETNA_SEC_KERNEL,
	ETNA_SEC_TZ
};

struct etnaviv_event {
	struct std::shared_ptr<dma_fence> fence;
	struct std::shared_ptr<etnaviv_gem_submit> submit;

	void (*sync_point)(struct etnaviv_gpu *gpu, struct etnaviv_event *event);
};

struct etnaviv_cmdbuf_suballoc;
struct regulator;
struct clk;
class reset_control;

#define ETNA_NR_EVENTS 30

enum etnaviv_gpu_state {
	ETNA_GPU_STATE_UNKNOWN = 0,
	ETNA_GPU_STATE_IDENTIFIED,
	ETNA_GPU_STATE_RESET,
	ETNA_GPU_STATE_INITIALIZED,
	ETNA_GPU_STATE_RUNNING,
	ETNA_GPU_STATE_FAULT,
};

struct etnaviv_gpu {
	struct drm_device *drm = nullptr;
	struct thermal_cooling_device *cooling = nullptr;
	struct device *dev = nullptr;
	std::shared_ptr<Mutex> lock = MutexList.Create();
	struct etnaviv_chip_identity identity{};
	enum etnaviv_sec_mode sec_mode{};
	WorkQueue wq;
	std::shared_ptr<Mutex> sched_lock = MutexList.Create();
	std::shared_ptr<DRMScheduler> sched = std::make_shared<DRMScheduler>();
	enum etnaviv_gpu_state state{};

	/* 'ring'-buffer: */
	struct etnaviv_cmdbuf buffer{};
	int exec_state = 0;

	/* event management: */
	//DECLARE_BITMAP(event_bitmap, ETNA_NR_EVENTS);
	struct etnaviv_event event[ETNA_NR_EVENTS];
	//struct completion event_free;
	//Spinlock event_spinlock{};
	//Condition event_free{};
	FixedQueue<unsigned int, ETNA_NR_EVENTS> free_events;

	u32 idle_mask = 0;

	/* Fencing support */
	//xarray user_fences;
	u32 next_user_fence = 0;
	u32 next_fence = 0;
	u32 completed_fence = 0;
	//wait_queue_head_t fence_event;
	u64 fence_context = 0;
	Spinlock fence_spinlock;
	std::shared_ptr<UserFenceManager> user_fences = std::make_shared<UserFenceManager>();

	/* worker for handling 'sync' points: */
	//struct work_struct sync_point_work;
	int sync_point_event = 0;

	/* hang detection */
	u32 hangcheck_dma_addr = 0;
	u32 hangcheck_primid = 0;
	u32 hangcheck_fence = 0;

	void __iomem *mmio = nullptr;
	int irq = 0;

	std::shared_ptr<etnaviv_iommu_context> mmu_context;
	unsigned int flush_seq = 0;

	/* Power Control: */
	std::unique_ptr<clk> clk_bus;
	std::unique_ptr<clk> clk_reg;
	std::unique_ptr<clk> clk_core;
	std::unique_ptr<clk> clk_shader;
	std::unique_ptr<reset_control> rst;

	unsigned int freq_scale = 0;
	unsigned int fe_waitcycles = 0;
	unsigned long base_rate_core = 0;
	unsigned long base_rate_shader = 0;
};

static inline void gpu_write(struct etnaviv_gpu *gpu, u32 reg, u32 data)
{
	*(volatile uint32_t *)((uintptr_t)gpu->mmio + reg) = data;
	__asm__ volatile("dsb sy\n" ::: "memory");
}

static inline u32 gpu_read(struct etnaviv_gpu *gpu, u32 reg)
{
	/* On some variants, such as the GC7000r6009, some FE registers
	 * need two reads to be consistent. Do that extra read here and
	 * throw away the result.
	 */
	if (reg >= VIVS_FE_DMA_STATUS && reg <= VIVS_FE_AUTO_FLUSH)
		*(volatile uint32_t *)((uintptr_t)gpu->mmio + reg);

	return *(volatile uint32_t *)((uintptr_t)gpu->mmio + reg);
}

static inline u32 gpu_fix_power_address(struct etnaviv_gpu *gpu, u32 reg)
{
	/* Power registers in GC300 < 2.0 are offset by 0x100 */
	if (gpu->identity.model == chipModel_GC300 &&
	    gpu->identity.revision < 0x2000)
		reg += 0x100;

	return reg;
}

static inline void gpu_write_power(struct etnaviv_gpu *gpu, u32 reg, u32 data)
{
	gpu_write(gpu, gpu_fix_power_address(gpu, reg), data);
}

static inline u32 gpu_read_power(struct etnaviv_gpu *gpu, u32 reg)
{
	return gpu_read(gpu, gpu_fix_power_address(gpu, reg));
}

int etnaviv_gpu_get_param(struct etnaviv_gpu &gpu, u32 param, u64 *value);

int etnaviv_gpu_init(struct etnaviv_gpu &gpu);
bool etnaviv_fill_identity_from_hwdb(struct etnaviv_gpu *gpu);

#ifdef CONFIG_DEBUG_FS
int etnaviv_gpu_debugfs(struct etnaviv_gpu *gpu, struct seq_file *m);
#endif

void etnaviv_gpu_recover_hang(struct etnaviv_gem_submit *submit);
void etnaviv_gpu_retire(struct etnaviv_gpu *gpu);
int etnaviv_gpu_wait_fence_interruptible(struct etnaviv_gpu *gpu,
	u32 fence, struct drm_etnaviv_timespec *timeout);
int etnaviv_gpu_wait_obj_inactive(struct etnaviv_gpu *gpu,
	struct etnaviv_gem_object *etnaviv_obj,
	struct drm_etnaviv_timespec *timeout);
std::shared_ptr<dma_fence> etnaviv_gpu_submit(std::shared_ptr <etnaviv_gem_submit> submit);
int etnaviv_gpu_pm_get_sync(struct etnaviv_gpu *gpu);
void etnaviv_gpu_pm_put(struct etnaviv_gpu *gpu);
int etnaviv_gpu_wait_idle(struct etnaviv_gpu *gpu, unsigned int timeout_ms);
void etnaviv_gpu_start_fe(struct etnaviv_gpu *gpu, u32 address, u16 prefetch);

extern struct platform_driver etnaviv_gpu_driver;

#endif /* __ETNAVIV_GPU_H__ */
