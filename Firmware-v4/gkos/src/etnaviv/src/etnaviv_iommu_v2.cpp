// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2018 Etnaviv Project
 */

//#include <linux/bitops.h>
//#include <linux/dma-mapping.h>
//#include <linux/platform_device.h>
//#include <linux/sizes.h>
//#include <linux/slab.h>
//#include <linux/vmalloc.h>

#include "etnaviv_cmdbuf.h"
#include "etnaviv_gpu.h"
#include "etnaviv_mmu.h"
#include "state.xml.h"
#include "state_hi.xml.h"
#include "coalescing_block_allocator.h"

#include <cstring>

#define MMUv2_PTE_PRESENT		BIT(0)
#define MMUv2_PTE_EXCEPTION		BIT(1)
#define MMUv2_PTE_WRITEABLE		BIT(2)

#define MMUv2_MTLB_MASK			0xffc00000
#define MMUv2_MTLB_SHIFT		22
#define MMUv2_STLB_MASK			0x003ff000
#define MMUv2_STLB_SHIFT		12

#define MMUv2_MAX_STLB_ENTRIES		1024

struct etnaviv_iommuv2_context : public etnaviv_iommu_context
{
	unsigned short id;
	/* M(aster) TLB aka first level pagetable */
	u32 *mtlb_cpu;
	dma_addr_t mtlb_dma;
	/* S(lave) TLB aka second level pagetable */
	u32 *stlb_cpu[MMUv2_MAX_STLB_ENTRIES];
	dma_addr_t stlb_dma[MMUv2_MAX_STLB_ENTRIES];

	~etnaviv_iommuv2_context()
	{
		for (auto i = 0; i < MMUv2_MAX_STLB_ENTRIES; i++) {
			if (stlb_cpu[i])
				dma_free_wc(nullptr, 4096u,
						stlb_cpu[i],
						stlb_dma[i]);
		}

		dma_free_wc(nullptr, 4096u, mtlb_cpu, mtlb_dma);

		clear_bit(id, global->v2.pta_alloc);
	}

	int map(unsigned long iova, phys_addr_t paddr,
					size_t size, int prot)
	{
		int mtlb_entry, stlb_entry, ret;
		u32 entry = lower_32_bits(paddr) | MMUv2_PTE_PRESENT;

		if (size != 4096u)
			return -EINVAL;

		//if (IS_ENABLED(CONFIG_PHYS_ADDR_T_64BIT))
			entry |= (upper_32_bits(paddr) & 0xff) << 4;

		if (prot & ETNAVIV_PROT_WRITE)
			entry |= MMUv2_PTE_WRITEABLE;

		mtlb_entry = (iova & MMUv2_MTLB_MASK) >> MMUv2_MTLB_SHIFT;
		stlb_entry = (iova & MMUv2_STLB_MASK) >> MMUv2_STLB_SHIFT;

		ret = etnaviv_iommuv2_ensure_stlb(mtlb_entry);
		if (ret)
			return ret;

		stlb_cpu[mtlb_entry][stlb_entry] = entry;

		return 0;
	}

	size_t unmap(unsigned long iova, size_t size)
	{
		int mtlb_entry, stlb_entry;

		if (size != 4096u)
			return -EINVAL;

		mtlb_entry = (iova & MMUv2_MTLB_MASK) >> MMUv2_MTLB_SHIFT;
		stlb_entry = (iova & MMUv2_STLB_MASK) >> MMUv2_STLB_SHIFT;

		stlb_cpu[mtlb_entry][stlb_entry] = MMUv2_PTE_EXCEPTION;

		return 4096u;
	}

	size_t dump_size()
	{
		size_t dump_size = 4096u;
		int i;

		for (i = 0; i < MMUv2_MAX_STLB_ENTRIES; i++)
			if (mtlb_cpu[i] & MMUv2_PTE_PRESENT)
				dump_size += 4096u;

		return dump_size;
	}

	void dump(void *_buf)
	{
		int i;

		uint8_t *buf = reinterpret_cast<uint8_t *>(_buf);

		memcpy(buf, mtlb_cpu, 4096u);
		buf += 4096u;
		for (i = 0; i < MMUv2_MAX_STLB_ENTRIES; i++)
			if (mtlb_cpu[i] & MMUv2_PTE_PRESENT) {
				memcpy(buf, stlb_cpu[i], 4096u);
				buf += 4096u;
			}
	}

	void etnaviv_iommuv2_restore_nonsec(struct etnaviv_gpu *gpu,
		std::shared_ptr<etnaviv_iommu_context> context)
	{
		u16 prefetch;

		/* If the MMU is already enabled the state is still there. */
		if (gpu_read(gpu, VIVS_MMUv2_CONTROL) & VIVS_MMUv2_CONTROL_ENABLE)
			return;

		gpu->mmu_context = context;

		prefetch = etnaviv_buffer_config_mmuv2(gpu,
					(u32)mtlb_dma,
					(u32)context->global->bad_page_dma);
		etnaviv_gpu_start_fe(gpu, (u32)etnaviv_cmdbuf_get_pa(&gpu->buffer),
					prefetch);
		etnaviv_gpu_wait_idle(gpu, 100);

		gpu_write(gpu, VIVS_MMUv2_CONTROL, VIVS_MMUv2_CONTROL_ENABLE);
	}

	int etnaviv_iommuv2_ensure_stlb(int stlb)
	{
		if (stlb_cpu[stlb])
			return 0;

		stlb_cpu[stlb] =
				(u32 *)dma_alloc_wc(nullptr, 4096u,
						&stlb_dma[stlb],
						GFP_KERNEL);

		if (!stlb_cpu[stlb])
			return -ENOMEM;

		memset32(stlb_cpu[stlb], MMUv2_PTE_EXCEPTION,
			4096u / sizeof(u32));

		mtlb_cpu[stlb] = stlb_dma[stlb] | MMUv2_PTE_PRESENT;

		return 0;
	}

	void etnaviv_iommuv2_restore_sec(struct etnaviv_gpu *gpu,
		std::shared_ptr<etnaviv_iommu_context> context)
	{
		u16 prefetch;

		/* If the MMU is already enabled the state is still there. */
		if (gpu_read(gpu, VIVS_MMUv2_SEC_CONTROL) & VIVS_MMUv2_SEC_CONTROL_ENABLE)
			return;

		gpu->mmu_context = context;

		gpu_write(gpu, VIVS_MMUv2_PTA_ADDRESS_LOW,
			lower_32_bits(context->global->v2.pta_dma));
		gpu_write(gpu, VIVS_MMUv2_PTA_ADDRESS_HIGH,
			upper_32_bits(context->global->v2.pta_dma));
		gpu_write(gpu, VIVS_MMUv2_PTA_CONTROL, VIVS_MMUv2_PTA_CONTROL_ENABLE);

		gpu_write(gpu, VIVS_MMUv2_NONSEC_SAFE_ADDR_LOW,
			lower_32_bits(context->global->bad_page_dma));
		gpu_write(gpu, VIVS_MMUv2_SEC_SAFE_ADDR_LOW,
			lower_32_bits(context->global->bad_page_dma));
		gpu_write(gpu, VIVS_MMUv2_SAFE_ADDRESS_CONFIG,
			VIVS_MMUv2_SAFE_ADDRESS_CONFIG_NON_SEC_SAFE_ADDR_HIGH(
			upper_32_bits(context->global->bad_page_dma)) |
			VIVS_MMUv2_SAFE_ADDRESS_CONFIG_SEC_SAFE_ADDR_HIGH(
			upper_32_bits(context->global->bad_page_dma)));

		context->global->v2.pta_cpu[id] = mtlb_dma |
						VIVS_MMUv2_CONFIGURATION_MODE_MODE4_K;

		/* trigger a PTA load through the FE */
		prefetch = etnaviv_buffer_config_pta(gpu, id);
		etnaviv_gpu_start_fe(gpu, (u32)etnaviv_cmdbuf_get_pa(&gpu->buffer),
					prefetch);
		etnaviv_gpu_wait_idle(gpu, 100);

		gpu_write(gpu, VIVS_MMUv2_SEC_CONTROL, VIVS_MMUv2_SEC_CONTROL_ENABLE);
	}

	void restore(struct etnaviv_gpu *gpu,
						std::shared_ptr<etnaviv_iommu_context> context)
	{
		klog("GPU: restore MMU context @%p (mtlb = %p)\n", context.get(),
			((etnaviv_iommuv2_context *)context.get())->mtlb_dma);
		switch (gpu->sec_mode) {
		case ETNA_SEC_NONE:
			etnaviv_iommuv2_restore_nonsec(gpu, context);
			break;
		case ETNA_SEC_KERNEL:
			etnaviv_iommuv2_restore_sec(gpu, context);
			break;
		default:
			WARN(1, "unhandled GPU security mode\n");
			break;
		}
	}

	struct dump_cba_type
	{
		uint32_t phys_base;
		uint32_t phys_len;

		bool CoalesceFrom(dump_cba_type &other, bool other_is_prev)
		{
			if(other_is_prev)
			{
				if(other.phys_base + other.phys_len == phys_base)
				{
					phys_base = other.phys_base;
					phys_len += other.phys_len;
					return true;
				}
			}
			else
			{
				if(phys_base + phys_len == other.phys_base)
				{
					phys_len += other.phys_len;
					return true;
				}
			}
			return false;
		}
	};

	void dump()
	{
		klog("GPU: IOMMUv2 MMU DUMP: MTLB: %p phys\n", (void *)mtlb_dma);
		CoalescingBlockAllocator<dump_cba_type>::BlockAddress cba_space;
		cba_space.start = 0;
		cba_space.length = 0x100000000ULL;
		CoalescingBlockAllocator<dump_cba_type> cba(cba_space);
		for(auto i = 0u; i < MMUv2_MAX_STLB_ENTRIES; i++)
		{
			auto stlb = stlb_cpu[i];
			if(stlb)
			{
				auto base_addr = i * 4096U * 1024;
				
				for(auto j = 0u; j < 1024; j++)
				{
					if(stlb[j] & MMUv2_PTE_PRESENT)
					{
						auto paddr = stlb[j] & ~0xfffU;

						dump_cba_type cbat;
						cbat.phys_base = paddr;
						cbat.phys_len = 4096;

						CoalescingBlockAllocator<dump_cba_type>::BlockAddress ba;
						ba.start = base_addr + j * 4096;
						ba.length = 4096;

						cba.AllocFixed(ba, std::move(cbat));
					}
				}
			}
		}

		for(auto &b : cba)
		{
			klog("  %08x - %08x @ %p phys\n",
				b.first.start, b.first.end(), b.second.phys_base);
		}
	}
};

u32 etnaviv_iommuv2_get_mtlb_addr(struct etnaviv_iommu_context *context)
{
	struct etnaviv_iommuv2_context *v2_context = reinterpret_cast<etnaviv_iommuv2_context *>(context);

	return v2_context->mtlb_dma;
}

unsigned short etnaviv_iommuv2_get_pta_id(struct etnaviv_iommu_context *context)
{
	struct etnaviv_iommuv2_context *v2_context = reinterpret_cast<etnaviv_iommuv2_context *>(context);

	return v2_context->id;
}

std::shared_ptr<etnaviv_iommu_context>
etnaviv_iommuv2_context_alloc(struct etnaviv_iommu_global *global)
{
	auto v2_context = std::make_shared<etnaviv_iommuv2_context>();
	if (!v2_context)
		return NULL;

	global->lock->lock();
	v2_context->id = find_first_zero_bit(global->v2.pta_alloc,
					     ETNAVIV_PTA_ENTRIES);
	if (v2_context->id < ETNAVIV_PTA_ENTRIES) {
		set_bit(v2_context->id, global->v2.pta_alloc);
	} else {
		global->lock->unlock();
		goto out_free;
	}
	global->lock->unlock();

	v2_context->mtlb_cpu = (u32 *)dma_alloc_wc(nullptr, 4096u,
					    &v2_context->mtlb_dma, GFP_KERNEL);
	if (!v2_context->mtlb_cpu)
		goto out_free_id;

	klog("GPU: IOMMU: MTLB @ %p virt, %p phys\n", v2_context->mtlb_cpu,
		(void *)v2_context->mtlb_dma);

	memset32(v2_context->mtlb_cpu, MMUv2_PTE_EXCEPTION,
		 MMUv2_MAX_STLB_ENTRIES);

	global->v2.pta_cpu[v2_context->id] = v2_context->mtlb_dma;

	v2_context->global = global;
	v2_context->mm.alloc = BlockAllocator<int>(4096u,
		4 * 1024 * 1024 * 1024u - 4096u);

	return v2_context;

out_free_id:
	clear_bit(v2_context->id, global->v2.pta_alloc);
out_free:
	return nullptr;
}

