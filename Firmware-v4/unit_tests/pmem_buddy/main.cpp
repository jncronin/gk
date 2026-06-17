#include "pmem.h"
#include "block_allocator.h"

#include <cstdlib>
#include <ctime>
#include <vector>

int main()
{
	PmemAllocator Pmem;

	auto ddr_start = 0x80123456ull;
	auto ddr_end = 0x100000000ull;

    uint64_t buddy_start = ddr_start & ~(Pmem.MaxBuddySize() - 1);
    uint64_t buddy_end = (ddr_end + (Pmem.MaxBuddySize() - 1)) & ~(Pmem.MaxBuddySize() - 1);

    auto total_length = buddy_end - buddy_start;

    auto mem_required = Pmem.BuddyMemSize(total_length);
    mem_required = (mem_required + 65535ULL) & ~65535ULL;

    klog("pmem: buddy from %llx to %llx needs %llx bytes.  Allocating at %llx.\n",
        buddy_start, buddy_end, mem_required, ddr_start);

    auto mem = new uint8_t[mem_required];

    ddr_start += mem_required;

    Pmem.init(mem, total_length);

    // now release all the relevant memory
    ddr_start = (ddr_start + (Pmem.MinBuddySize() - 1)) & ~(Pmem.MinBuddySize() - 1);

    auto ba_start = ddr_start;
    auto ba_end = ddr_end;

    while (ddr_start < ddr_end)
    {
        auto max_size = 1ULL << std::countr_zero(ddr_start);
        while (max_size > Pmem.MaxBuddySize())
            max_size /= 2ULL;
        while ((ddr_start + max_size) > ddr_end)
            max_size /= 2ULL;

        if (max_size < Pmem.MinBuddySize())
        {
            ddr_start += max_size;
            continue;
        }

        PMemBlock pb;
        pb.base = ddr_start;
        pb.length = max_size;
        pb.valid = true;

        Pmem.release(pb);

        klog("pmem: release %llx - %llx\n", ddr_start, ddr_start + max_size);

        ddr_start += max_size;
    }

    // init complete, now run some tests

    // use a block allocator for checking we don't allocate stuff
    //  we already have
    BlockAllocator<int> ba(ba_start, ba_end - ba_start);

    srand(time(nullptr));

    std::vector<PMemBlock> alloced;

    for (auto i = 0; i < 100000; i++)
    {
        if (((rand() % 10) < 4) && alloced.size())
        {
            // do a free
            auto free_idx = rand() % alloced.size();

            auto pmb = alloced[free_idx];

            Pmem.release(pmb);
            ba.Dealloc(pmb.base);

            alloced.erase(alloced.begin() + free_idx);

            //printf("%d: free %llx - %llx\n", i, pmb.base, pmb.base + pmb.length);
        }
        else
        {
            // do an alloc of size x
            uint64_t size = 0;
            do
            {
                size = 1ULL << (rand() % 32);
            } while (size < 0x10000 || size > 0x20000000);


            auto pmb = Pmem.acquire(size);
            if (!pmb.valid)
            {
                //printf("%d: could not allocate size %llx\n", i, size);
                continue;
            }

            if (pmb.base < ba_start || pmb.base + pmb.length > ba_end)
            {
                printf("%d: out of bounds %llx - %llx\n",
                    i, pmb.base, pmb.base + pmb.length);
            }

            BlockAllocator<int>::BlockAddress ba_adr;
            ba_adr.start = pmb.base;
            ba_adr.length = pmb.length;
            auto ba_iter = ba.AllocFixed(ba_adr, 0);

            if (ba_iter == ba.end())
            {
                printf("%d: allocated overlapping region %llx - %llx\n",
                    i, pmb.base, pmb.base + pmb.length);
            }

            //printf("%d: alloc %llx - %llx\n", i, pmb.base, pmb.base + pmb.length);

            alloced.push_back(pmb);
        }
    }

    return 0;
}
