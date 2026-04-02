// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2014-2018 Etnaviv Project
 */

//#include <linux/bitops.h>
//#include <linux/dma-mapping.h>
//#include <linux/platform_device.h>
//#include <linux/sizes.h>
//#include <linux/slab.h>

#include "etnaviv_gpu.h"
#include "etnaviv_mmu.h"
#include "state_hi.xml.h"
#include <cstring>

#define PT_SIZE		(2 * 1024 * 1024ull)
#define PT_ENTRIES	(PT_SIZE / sizeof(u32))

#define GPU_MEM_START	0x80000000

struct etnaviv_iommuv1_context : public etnaviv_iommu_context {
	u32 *pgtable_cpu;
	dma_addr_t pgtable_dma;

	~etnaviv_iommuv1_context()
	{
		dma_free_wc(nullptr, PT_SIZE, pgtable_cpu, pgtable_dma);
		global->v1.shared_context = nullptr;
	}

	int map(unsigned long iova, phys_addr_t paddr,
			       size_t size, int prot)
	{
		unsigned int index = (iova - GPU_MEM_START) / 4096u;

		if (size != 4096u)
			return -EINVAL;

		pgtable_cpu[index] = paddr;

		return 0;
	}

	size_t unmap(unsigned long iova, size_t size)
	{
		unsigned int index = (iova - GPU_MEM_START) / 4096u;

		if (size != 4096u)
			return -EINVAL;

		pgtable_cpu[index] = global->bad_page_dma;

		return 4096u;
	}

	size_t dump_size()
	{
		return PT_SIZE;
	}

	void dump(void *buf)
	{
		memcpy(buf, pgtable_cpu, PT_SIZE);
	}

	void restore(struct etnaviv_gpu *gpu, std::shared_ptr<etnaviv_iommu_context> context)
	{
		u32 pgtable;

		gpu->mmu_context = context;
		
		/* set base addresses */
		gpu_write(gpu, VIVS_MC_MEMORY_BASE_ADDR_RA, context->global->memory_base);
		gpu_write(gpu, VIVS_MC_MEMORY_BASE_ADDR_FE, context->global->memory_base);
		gpu_write(gpu, VIVS_MC_MEMORY_BASE_ADDR_TX, context->global->memory_base);
		gpu_write(gpu, VIVS_MC_MEMORY_BASE_ADDR_PEZ, context->global->memory_base);
		gpu_write(gpu, VIVS_MC_MEMORY_BASE_ADDR_PE, context->global->memory_base);

		/* set page table address in MC */
		pgtable = (u32)pgtable_dma;

		gpu_write(gpu, VIVS_MC_MMU_FE_PAGE_TABLE, pgtable);
		gpu_write(gpu, VIVS_MC_MMU_TX_PAGE_TABLE, pgtable);
		gpu_write(gpu, VIVS_MC_MMU_PE_PAGE_TABLE, pgtable);
		gpu_write(gpu, VIVS_MC_MMU_PEZ_PAGE_TABLE, pgtable);
		gpu_write(gpu, VIVS_MC_MMU_RA_PAGE_TABLE, pgtable);
	}
};

std::shared_ptr<etnaviv_iommu_context>
etnaviv_iommuv1_context_alloc(struct etnaviv_iommu_global *global)
{
	global->lock->lock();

	/*
	 * MMUv1 does not support switching between different contexts without
	 * a stop the world operation, so we only support a single shared
	 * context with this version.
	 */
	if (global->v1.shared_context) {
		auto context = global->v1.shared_context;
		global->lock->unlock();
		return context;
	}

	auto v1_context = std::make_shared<etnaviv_iommuv1_context>();

	if (!v1_context) {
		global->lock->unlock();
		return NULL;
	}

	v1_context->pgtable_cpu = (u32 *)dma_alloc_wc(nullptr, PT_SIZE,
					       &v1_context->pgtable_dma,
					       GFP_KERNEL);
	if (!v1_context->pgtable_cpu)
		goto out_free;

	memset32(v1_context->pgtable_cpu, global->bad_page_dma, PT_ENTRIES);

	v1_context->global = global;

	v1_context->mm.alloc = BlockAllocator<int>(GPU_MEM_START, PT_ENTRIES * 4096u);
	v1_context->global->v1.shared_context = v1_context;

	global->lock->unlock();

	return v1_context;

out_free:
	global->lock->unlock();
	return NULL;
}
