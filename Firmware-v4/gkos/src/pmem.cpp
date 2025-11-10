#include "pmem.h"
#include "vmem.h"
#include "logger.h"

PmemAllocator Pmem;

void init_pmem(uint64_t ddr_start, uint64_t ddr_end)
{
    uint64_t buddy_start = ddr_start & ~(Pmem.MaxBuddySize() - 1);
    uint64_t buddy_end = (ddr_end + (Pmem.MaxBuddySize() - 1)) & ~(Pmem.MaxBuddySize() - 1);

    auto total_length = buddy_end - buddy_start;

    auto mem_required = Pmem.BuddyMemSize(total_length);
    mem_required = (mem_required + 65535ULL) & ~65535ULL;

    klog("pmem: buddy from %llx to %llx needs %llx bytes.  Allocating at %llx.\n",
        buddy_start, buddy_end, mem_required, ddr_start);

    void *mem = (void *)PMEM_TO_VMEM(ddr_start);
    ddr_start += mem_required;

    Pmem.init(mem, total_length);

    // now release all the relevant memory
    ddr_start = (ddr_start + (Pmem.MinBuddySize() - 1)) & ~(Pmem.MinBuddySize() - 1);
    while(ddr_start < ddr_end)
    {
        auto max_size = 1ULL << __builtin_ctz(ddr_start);
        while(max_size > Pmem.MaxBuddySize())
            max_size /= 2ULL;
        while((ddr_start + max_size) > ddr_end)
            max_size /= 2ULL;

        if(max_size < Pmem.MinBuddySize())
        {
            ddr_start += max_size;
            continue;
        }

        PmemBlock pb;
        pb.base = ddr_start;
        pb.length = max_size;
        pb.valid = true;
        
        Pmem.release(pb);

        klog("pmem: release %llx - %llx\n", ddr_start, ddr_start + max_size);

        ddr_start += max_size;
    }
}
