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
        vmem_map(vmem.base + i, pmem.base + i, false, true, false, ~0ULL, ~0ULL, nullptr, MT_NORMAL_WT);
    }

    *dma_addr = pmem.base;
    return (void *)vmem.base;
}
