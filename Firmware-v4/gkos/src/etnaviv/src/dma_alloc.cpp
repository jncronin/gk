#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"
#include "etnaviv_cmdbuf.h"
#include "pmem.h"
#include "vmem.h"
#include "vblock.h"

void *dma_alloc_wc(struct device *dev, size_t size,
				 dma_addr_t *dma_addr, gfp_t gfp)
{
    if(!(gfp & GFP_KERNEL))
    {
        klog("dma_alloc_wc: GFP_KERNEL not specified\n");
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
    auto vmem = vblock_alloc(vblock_size_for(size), false, true, false);
    if(!vmem.valid)
    {
        Pmem.release(pmem);
        klog("dma_alloc_wc: unable to allocate vmem of length %llu\n", size);
    }

    // then map
    for(auto i = 0ul; i < pmem.length; i += PAGE_SIZE)
    {
        klog("dma_alloc_wc: map %p phys to %p virt\n", (void *)(pmem.base + i), (void *)(vmem.base + i));
        vmem_map(vmem.base + i, pmem.base + i, false, true, false, ~0ULL, ~0ULL, nullptr, MT_NORMAL_WT);
    }

    *dma_addr = pmem.base;

    klog("dma_alloc_wc: %llu bytes @ %p physical, %p virtual\n", size, *dma_addr, (void *)vmem.base);
    return (void *)vmem.base;
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
