#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"
#include "etnaviv_cmdbuf.h"
#include "pmem.h"
#include "vmem.h"
#include "vblock.h"
#include "process.h"
#include "cache.h"

void *dma_alloc(struct device *dev, size_t size,
				 dma_addr_t *dma_addr, gfp_t gfp, unsigned int mt, size_t *vsize,
                size_t *psize)
{
    if(!(gfp & GFP_KERNEL) && !(gfp & GFP_HIGHUSER))
    {
        klog("dma_alloc_wc: GFP_KERNEL or GFP_HIGHUSER not specified\n");
        return nullptr;
    }

    // get pmem first
    auto pmem = Pmem.acquire(size);
    if(!pmem.valid)
    {
        klog("dma_alloc_wc: unable to allocate pmem of length %llu\n", size);
        return nullptr;
    }

    // then vmem
    VMemBlock vmem;
    uintptr_t ttbr0 = ~0ULL;
    if(gfp & GFP_KERNEL)
    {
        vmem = vblock_alloc(vblock_size_for(pmem.length), false, true, false);
    }
    else
    {
        auto p = GetCurrentProcessForCore();
        if(!p)
            return nullptr;

        // A backing store for any unmapped region - shoudln't get used
        auto pmb = MemBlock::ZeroBackedReadOnlyMemory(0, pmem.length, true, false);
        vmem = p->user_mem->vblocks.AllocAny(pmb, false);
        ttbr0 = p->user_mem->ttbr0;
    }
    if(!vmem.valid)
    {
        Pmem.release(pmem);
        klog("dma_alloc_wc: unable to allocate vmem of length %llu\n", pmem.length);
        return nullptr;
    }

    if(vsize)
    {
        *vsize = vmem.length;
    }
    if(psize)
    {
        *psize = pmem.length;
    }

    // then map
    for(auto i = 0ul; i < pmem.length; i += PAGE_SIZE)
    {
        //klog("dma_alloc_wc: map %p phys to %p virt\n", (void *)(pmem.base + i), (void *)(vmem.base + i));
        vmem_map(vmem.base + i, pmem.base + i, false, true, false, ttbr0, ~0ULL, nullptr, mt);

        // and zero
        for(auto j = 0ul; j < PAGE_SIZE; j += CACHE_LINE_SIZE)
        {
            __asm__ volatile("dc zva, %[addr]\n" : : [addr] "r" (vmem.base + i + j) : "memory");
        }
    }

    *dma_addr = pmem.base;

    klog("dma_alloc_wc: %llu bytes @ %p physical, %p virtual\n", size, *dma_addr, (void *)vmem.base);
    return (void *)vmem.base;
}

void *dma_alloc_wc(struct device *dev, size_t size,
				 dma_addr_t *dma_addr, gfp_t gfp)
{
    return dma_alloc(dev, size, dma_addr, gfp, MT_NORMAL_NC);
}


void dma_free_wc(struct device *dev, size_t size,
			       void *cpu_addr, dma_addr_t dma_addr)
{
    klog("dma_free_wc: %llu bytes @ %p physical, %p virtual\n", size, (void *)dma_addr, cpu_addr);

    VMemBlock vb;
    vb.base = (uint64_t)cpu_addr;
    vb.length = size;
    vb.valid = true;
    vmem_unmap(vb);

    vblock_free(vb);

    for(auto i = 0ull; i < size; i += PAGE_SIZE)
    {
        PMemBlock pb;
        pb.base = dma_addr + i;
        pb.length = PAGE_SIZE;
        pb.valid = true;
        Pmem.release(pb);
    }
}

void *memset32(void *p, uint32_t v, size_t n)
{
    auto cp = (uint32_t *)p;
    while(n--)
    {
        *cp++ = v;
    }
    return p;
}

int dma_sync_sgtable_for_cpu(sg_table &sgt, dma_data_direction dir)
{
    switch(dir)
    {
        case dma_data_direction::DMA_FROM_DEVICE:
        case dma_data_direction::DMA_BIDIRECTIONAL:
            for(const auto &sge : sgt)
            {
                InvalidateA35Cache((uintptr_t)sge.vaddr, sge.len, CacheType_t::Data);
            }
            break;
        default:
            break;
    }

    return 0;
}

int dma_sync_sgtable_for_device(sg_table &sgt, dma_data_direction dir)
{
    switch(dir)
    {
        case dma_data_direction::DMA_TO_DEVICE:
        case dma_data_direction::DMA_BIDIRECTIONAL:
            for(const auto &sge : sgt)
            {
                CleanA35Cache((uintptr_t)sge.vaddr, sge.len, CacheType_t::Data);
            }
            break;
        default:
            break;
    }
    return 0;
}

int drm_prime_pages_to_sg(const drm_gem_object &obj, sg_table &sgt)
{
    sg_entry sge;
    sge.vaddr = obj.vaddr;
    sge.paddr = obj.dma_addr;
    sge.len = obj.vsize;
    sgt.push_back(sge);
    return 0;
}

int dma_resv_lock_interruptible(dma_resv &resv, WaitWoundContext &ticket)
{
    return ticket.lock(resv.lock);
}

int dma_resv_lock_slow_interruptible(dma_resv &resv, WaitWoundContext &ticket)
{
    return ticket.lock_slow(resv.lock);
}

int dma_resv_unlock(dma_resv &resv)
{
    resv.lock->unlock();
    return 0;
}
