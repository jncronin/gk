#include "vblock.h"
#include "pmem.h"
#include "util.h"
#include "pmem.h"
#include "vmem.h"
#include "logger.h"
#include "osspinlock.h"

/* This is a custom buddy implementation with three levels (512 MiB, 4 MiB, 64 kiB) to support upper half
    paging for gkos.

    The memory covers 0xffff ff00 0000 0000
            to
                      0xffff ffff 0000 0000
    i.e. an almost 1 TiB space
    
    There are 2040 x 512 MiB entries in the first level.
     (2048 less the last 4 GiB which are used for the kernel).
    
    With an 8 byte pointer per area, we don't need to allocate any more than 1 64 kiB page for the
     first level.

    An entry of 0 means available.  An entry of ~0 means never free (e.g. the last 8 entries).
    An entry with lowest bit set means allocated as a block at this level (we can potentially use
        the other bits for our own purposes).

    Anything else is considered a pointer to a second/third level.

    At each level we store -
     - number of free pages/sublevels
     - most recently accessed page/sublevel
*/

#define VBLOCK_START    0xffffff0000000000ULL
#define VBLOCK_END      0xffffffff00000000ULL

#define VBLOCK_UNAVAIL          (~0ULL)
#define VBLOCK_BLOCK_ALLOC      1ULL
#define VBLOCK_BLOCK_FREE       0ULL

#define LEVEL1_COUNT    2048
#define LEVEL2_COUNT    128
#define LEVEL3_COUNT    64

// levels 2 and 3 need to store metrics at the start (n free, last accessed) therefore their
//  byte size is increased and aligned to a power of two

struct level2
{
    uint64_t free_count;
    uint64_t last_accessed_block;
    uint64_t last_accessed_buddy;
    uint64_t b[LEVEL2_COUNT];
};

struct level3
{
    uint64_t free_count;
    uint64_t last_accessed_block;
    uint64_t b[LEVEL3_COUNT];
};

static const constexpr size_t level2_buddy_byte_count = align_power_2(sizeof(level2));
static const constexpr size_t level3_buddy_byte_count = align_power_2(sizeof(level3));

static_assert(level2_buddy_byte_count == 2048);
static_assert(level3_buddy_byte_count == 1024);

class VBlock
{
    protected:
        uint64_t *level1 = nullptr;
        
        // because levels 2 and 3 do not occupy a whole page, we can use a single page to store more than
        //  one.  Therefore if a request for a new page comes in, we can chop up the one here.
        uintptr_t cur_level2_page = 0;
        size_t cur_level2_page_offset = 0;

        uintptr_t cur_level3_page = 0;
        size_t cur_level3_page_offset = 0;

        // store metrics for level 1
        size_t level1_free = 0;
        size_t level1_last_accessed_block = 0;      // ideally next available 512 MiB block
        size_t level1_last_accessed_pointer = 0;    // ideally pointer to level2 with next available space

        // our base address
        uint64_t base = 0;

        Spinlock sl;

    protected:
        BuddyEntry AllocLevel1(uint32_t tag);
        BuddyEntry AllocLevel2(uint32_t tag);
        BuddyEntry AllocLevel3(uint32_t tag);

        size_t GetFreeLevel1Index();
        size_t GetLevel1IndexToNotFullLevel2();
        std::pair<size_t, size_t> GetLevel12IndexToNotFullLevel3();
        size_t GetFreeLevel2Index(size_t level1_idx);
        size_t GetFreeLevel3Index(size_t level1_idx, size_t level2_idx);

        uint64_t PmemAllocLevel2();
        uint64_t PmemAllocLevel3();

    public:
        void init();

        BuddyEntry Alloc(size_t size, uint32_t tag = 0);
        void Free(BuddyEntry &be);
        std::pair<BuddyEntry, uint32_t> Valid(uintptr_t addr);
};

VBlock vblock;

void init_vblock()
{
    vblock.init();
}

BuddyEntry vblock_alloc(size_t size, uint32_t tag)
{
    return vblock.Alloc(size, tag);
}

std::pair<bool, uint32_t> vblock_valid(uintptr_t addr)
{
    auto [be, tag] = vblock.Valid(addr);
    return std::make_pair(be.valid, tag);
}

void VBlock::init()
{
    CriticalGuard cg(sl);
    base = VBLOCK_START;

    // get a free page for level1
    level1 = (uint64_t *)PMEM_TO_VMEM(Pmem.acquire(VBLOCK_64k).base);

    for(auto i = 0U; i < 2048; i++)
    {
        if(i < 2040)
            level1[i] = VBLOCK_BLOCK_FREE;
        else
            level1[i] = VBLOCK_UNAVAIL;
    }

    level1_free = 2040;
    level1_last_accessed_block = 0;
    level1_last_accessed_pointer = 0;

    klog("vblock: init\n");
}

BuddyEntry VBlock::Alloc(size_t length, uint32_t tag)
{
    switch(length)
    {
        case VBLOCK_64k:
            return AllocLevel3(tag);

        case VBLOCK_4M:
            return AllocLevel2(tag);

        case VBLOCK_512M:
            return AllocLevel1(tag);

        default:
            klog("vblock: invalid size requested: %llu\n", length);
            return BuddyEntry { .valid = false };
    }
}

size_t VBlock::GetFreeLevel1Index()
{
    if(level1_free == 0)
        return VBLOCK_UNAVAIL;

    // loop offset from last accessed
    for(size_t i = 0U; i < LEVEL1_COUNT; i++)
    {
        auto idx = (i + level1_last_accessed_block) % LEVEL1_COUNT;
        if(level1[idx] == VBLOCK_BLOCK_FREE)
            return idx;
    }

    // shouldn't get here
    klog("vblock: level1 free counter inaccurate\n");
    level1_free = 0;
    return VBLOCK_UNAVAIL;
}

size_t VBlock::GetFreeLevel2Index(size_t l1_idx)
{
    auto l2 = (level2 *)level1[l1_idx];

    for(size_t i = 0U; i < LEVEL2_COUNT; i++)
    {
        auto idx = (i + l2->last_accessed_block) % LEVEL2_COUNT;
        if(l2->b[idx] == VBLOCK_BLOCK_FREE)
            return idx;
    }

    // shouldn't get here
    klog("vblock: level2 free counter inaccurate\n");
    l2->free_count = 0;
    return VBLOCK_UNAVAIL;
}

size_t VBlock::GetFreeLevel3Index(size_t l1_idx, size_t l2_idx)
{
    auto l2 = (level2 *)level1[l1_idx];
    auto l3 = (level3 *)l2->b[l2_idx];

    for(size_t i = 0U; i < LEVEL3_COUNT; i++)
    {
        auto idx = (i + l3->last_accessed_block) % LEVEL3_COUNT;
        if(l3->b[idx] == VBLOCK_BLOCK_FREE)
            return idx;
    }

    // shouldn't get here
    klog("vblock: level3 free counter inaccurate\n");
    l3->free_count = 0;
    return VBLOCK_UNAVAIL;
}

size_t VBlock::GetLevel1IndexToNotFullLevel2()
{
    // loop offset from last accessed
    for(size_t i = 0U; i < LEVEL1_COUNT; i++)
    {
        auto idx = (i + level1_last_accessed_pointer) % LEVEL1_COUNT;
        if(!(level1[idx] & VBLOCK_BLOCK_ALLOC) && level1[idx] != VBLOCK_UNAVAIL &&
            level1[idx] != VBLOCK_BLOCK_FREE)
        {
            // potentially available
            auto l2 = (level2 *)level1[idx];
            if(l2->free_count > 0)
                return idx;
        }
    }
    return VBLOCK_UNAVAIL;
}

std::pair<size_t, size_t> VBlock::GetLevel12IndexToNotFullLevel3()
{
    // loop offset from last accessed
    for(size_t i = 0U; i < LEVEL1_COUNT; i++)
    {
        auto idx = (i + level1_last_accessed_pointer) % LEVEL1_COUNT;
        if(!(level1[idx] & VBLOCK_BLOCK_ALLOC) && level1[idx] != VBLOCK_UNAVAIL &&
            level1[idx] != VBLOCK_BLOCK_FREE)
        {
            // potentially available
            auto l2 = (level2 *)level1[idx];

            for(size_t j = 0; j < LEVEL2_COUNT; j++)
            {
                auto idx2 = (j + l2->last_accessed_buddy) % LEVEL2_COUNT;
                if(!(l2->b[idx2] & VBLOCK_BLOCK_ALLOC) && l2->b[idx2] != VBLOCK_UNAVAIL &&
                    l2->b[idx2] != VBLOCK_BLOCK_FREE)
                {
                    // potentially available
                    auto l3 = (level3 *)l2->b[idx2];

                    if(l3->free_count > 0)
                        return std::make_pair(idx, idx2);
                }
            }
        }
    }

    // if this far then no free level 3 buddies, get a spare level 2 instead
    auto idx = GetLevel1IndexToNotFullLevel2();
    return std::make_pair(idx, VBLOCK_UNAVAIL);
}

BuddyEntry VBlock::AllocLevel1(uint32_t tag)
{
    CriticalGuard cg(sl);
    auto idx = GetFreeLevel1Index();
    if(idx == VBLOCK_UNAVAIL)
    {
        return BuddyEntry { .valid = false };
    }
    level1[idx] = VBLOCK_BLOCK_ALLOC | ((uint64_t)tag << 32);
    level1_free--;
    level1_last_accessed_block = idx + 1;

    klog("vblock: allocated 512MiB at %llx\n", base + idx * VBLOCK_512M);

    return BuddyEntry
    {
        .base = base + idx * VBLOCK_512M,
        .length = VBLOCK_512M,
        .valid = true
    };
}

BuddyEntry VBlock::AllocLevel2(uint32_t tag)
{
    CriticalGuard cg(sl);
    auto idx = GetLevel1IndexToNotFullLevel2();
    uint64_t l2_pmem;
    if(idx == VBLOCK_UNAVAIL)
    {
        // no current pointers to a free level 2 are available - make one
        idx = GetFreeLevel1Index();
        if(idx == VBLOCK_UNAVAIL)
            return BuddyEntry { .valid = false };

        klog("vblock: creating new level2 buddy\n");

        l2_pmem = PmemAllocLevel2();
        level1[idx] = l2_pmem;

        level1_free--;
    }
    else
    {
        l2_pmem = level1[idx];
    }
    auto idx2 = GetFreeLevel2Index(idx);
    if(idx2 == VBLOCK_UNAVAIL)
    {
        klog("vblock: expected available level2 buddy but none available\n");
        return BuddyEntry { .valid = false };
    }
    auto l2 = (level2 *)l2_pmem;
    l2->b[idx2] = VBLOCK_BLOCK_ALLOC | ((uint64_t)tag << 32);
    l2->free_count--;
    l2->last_accessed_block = idx2 + 1;
    level1_last_accessed_pointer = idx;

    auto addr = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M;

    klog("vblock: allocated 4MiB at %llx\n", addr);

    return BuddyEntry
    {
        .base = addr,
        .length = VBLOCK_4M,
        .valid = true
    };
}

BuddyEntry VBlock::AllocLevel3(uint32_t tag)
{
    CriticalGuard cg(sl);
    auto [idx, idx2] = GetLevel12IndexToNotFullLevel3();

    uint64_t l2_pmem;
    uint64_t l3_pmem;
    if(idx == VBLOCK_UNAVAIL)
    {
        // no current pointers to a free level 2 are available - make one
        idx = GetFreeLevel1Index();
        if(idx == VBLOCK_UNAVAIL)
            return BuddyEntry { .valid = false };

        klog("vblock: creating new level2 buddy\n");

        l2_pmem = PmemAllocLevel2();
        level1[idx] = l2_pmem;

        level1_free--;
    }
    else
    {
        l2_pmem = level1[idx];
    }

    auto l2 = (level2 *)l2_pmem;

    if(idx2 == VBLOCK_UNAVAIL)
    {
        // no current pointers to a free level 3 are available - make one
        idx2 = GetFreeLevel2Index(idx);
        if(idx2 == VBLOCK_UNAVAIL)
            return BuddyEntry { .valid = false };
        
        klog("vblock: creating new level3 buddy\n");

        l3_pmem = PmemAllocLevel3();
        l2->b[idx2] = l3_pmem;

        l2->free_count--;
    }
    else
    {
        l3_pmem = l2->b[idx2];
    }

    auto idx3 = GetFreeLevel3Index(idx, idx2);
    if(idx2 == VBLOCK_UNAVAIL)
    {
        klog("vblock: expected available level3 buddy but none available\n");
        return BuddyEntry { .valid = false };
    }

    auto l3 = (level3 *)l3_pmem;
    l3->b[idx3] = VBLOCK_BLOCK_ALLOC | ((uint64_t)tag << 32);
    l3->free_count--;
    l3->last_accessed_block = idx3 + 1;
    level1_last_accessed_pointer = idx;
    l2->last_accessed_buddy = idx2;

    auto addr = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M + idx3 * VBLOCK_64k;

    klog("vblock: allocated 64 kiB at %llx\n", addr);

    return BuddyEntry
    {
        .base = addr,
        .length = VBLOCK_64k,
        .valid = true
    };
}

uint64_t VBlock::PmemAllocLevel2()
{
    if(cur_level2_page == 0 || cur_level2_page_offset >= VBLOCK_64k)
    {
        cur_level2_page = PMEM_TO_VMEM(Pmem.acquire(VBLOCK_64k).base);
        cur_level2_page_offset = 0;

        klog("vblock: new level2 page at %llx\n", cur_level2_page);
    }

    auto ret = cur_level2_page + cur_level2_page_offset;
    cur_level2_page_offset += level2_buddy_byte_count;

    auto l2 = (level2 *)ret;
    l2->free_count = LEVEL2_COUNT;
    l2->last_accessed_block = 0;
    l2->last_accessed_buddy = 0;
    for(auto i = 0U; i < LEVEL2_COUNT; i++)
        l2->b[i] = VBLOCK_BLOCK_FREE;
    
    return ret;
}

uint64_t VBlock::PmemAllocLevel3()
{
    if(cur_level3_page == 0 || cur_level3_page_offset >= VBLOCK_64k)
    {
        cur_level3_page = PMEM_TO_VMEM(Pmem.acquire(VBLOCK_64k).base);
        cur_level3_page_offset = 0;

        klog("vblock: new level3 page at %llx\n", cur_level3_page);
    }

    auto ret = cur_level3_page + cur_level3_page_offset;
    cur_level3_page_offset += level3_buddy_byte_count;

    auto l3 = (level3 *)ret;
    l3->free_count = LEVEL3_COUNT;
    l3->last_accessed_block = 0;
    for(auto i = 0U; i < LEVEL3_COUNT; i++)
        l3->b[i] = VBLOCK_BLOCK_FREE;
    
    return ret;
}

std::pair<BuddyEntry, uint32_t> VBlock::Valid(uintptr_t addr)
{
    addr -= base;

    auto idx = addr / VBLOCK_512M;

    CriticalGuard cg(sl);
    if(level1[idx] == VBLOCK_UNAVAIL || level1[idx] == VBLOCK_BLOCK_FREE)
    {
        // not allocated
        return std::make_pair(BuddyEntry { .valid = false }, 0);
    }
    else if(level1[idx] & VBLOCK_BLOCK_ALLOC)
    {
        // allocated as large block
        return std::make_pair(BuddyEntry
            {
                .base = base + idx * VBLOCK_512M,
                .length = VBLOCK_512M,
                .valid = true
            }, (level1[idx] >> 32));
    }
    
    // check level2
    auto idx2 = (addr % VBLOCK_512M) / VBLOCK_4M;
    auto l2 = (level2 *)level1[idx];
    if(l2->b[idx] == VBLOCK_UNAVAIL || l2->b[idx] == VBLOCK_BLOCK_FREE)
    {
        // not allocated
        return std::make_pair(BuddyEntry { .valid = false }, 0);
    }
    else if(l2->b[idx] & VBLOCK_BLOCK_ALLOC)
    {
        // allocated as large block
        return std::make_pair(BuddyEntry
            {
                .base = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M,
                .length = VBLOCK_4M,
                .valid = true
            }, (l2->b[idx2] >> 32));
    }

    // check level3
    auto idx3 = (addr % VBLOCK_4M) / VBLOCK_64k;
    auto l3 = (level3 *)l2->b[idx2];
    if(!(l3->b[idx] & VBLOCK_BLOCK_ALLOC))
    {
        // not allocated
        return std::make_pair(BuddyEntry { .valid = false }, 0);
    }

    return std::make_pair(BuddyEntry
        {
            .base = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M + idx3 * VBLOCK_64k,
            .length = VBLOCK_64k,
            .valid = true
        }, (l3->b[idx3] >> 32));
}
