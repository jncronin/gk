#include "vblock.h"
#include "pmem.h"
#include "util.h"
#include "pmem.h"
#include "vmem.h"
#include "logger.h"
#include "osspinlock.h"
#include "process.h"
#include "thread.h"

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
#define VBLOCK_BLOCK_ALLOC_MASK 3ULL
#define VBLOCK_BLOCK_ALLOC      1ULL
#define VBLOCK_BLOCK_FREE       0ULL

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

constinit VBlock vblock;

void init_vblock()
{
    vblock.init(VBLOCK_START, 2040U);
}

VMemBlock vblock_alloc(size_t size, bool user, bool write, bool exec,
    unsigned int lower_guard, unsigned int upper_guard, VBlock &vb, bool map)
{
    return vblock_alloc_fixed(size, ~0ULL, user, write, exec, lower_guard, upper_guard, vb, map);
}

VMemBlock vblock_alloc_fixed(size_t size, uintptr_t addr, bool user, bool write, bool exec,
    unsigned int lower_guard, unsigned int upper_guard, VBlock &vb, bool map)
{
    uint32_t tag = 0;
    if(user)
        tag |= VBLOCK_TAG_USER;
    if(write)
        tag |= VBLOCK_TAG_WRITE;
    if(exec)
        tag |= VBLOCK_TAG_EXEC;
    if(lower_guard >= GUARD_BITS_MAX)
        return InvalidVMemBlock();
    if(upper_guard >= GUARD_BITS_MAX)
        return InvalidVMemBlock();
    tag |= (lower_guard << VBLOCK_TAG_GUARD_LOWER_POS) | (upper_guard << VBLOCK_TAG_GUARD_UPPER_POS);
    auto ret = (addr == ~0ULL) ? vb.Alloc(size, tag) : vb.AllocFixed(size, addr, tag);
    if(ret.valid)
    {
        ret.user = user;
        ret.write = write;
        ret.exec = exec;
        ret.upper_guard = upper_guard;
        ret.lower_guard = lower_guard;
    }

    if(map)
    {
        auto p = GetCurrentThreadForCore()->p;

        for(auto ptr = 0U; ptr < ret.data_length(); ptr += VBLOCK_64k)
        {
            auto pb = Pmem.acquire(VBLOCK_64k);
            if(!pb.valid)
            {
                klog("vblock: alloc: request to map but no free physical pages\n");
                vb.Free(ret);
                ret.valid = false;
                return ret;
            }
            auto mapret = vmem_map(ret.data_start() + ptr, pb.base, user, write, exec);
            if(mapret)
            {
                klog("vblock: alloc: request to map but map failed: %d\n", mapret);
                vb.Free(ret);
                ret.valid = false;
                return ret;
            }
            {
                CriticalGuard cg(p->owned_pages.sl);
                p->owned_pages.add(pb);
            }
        }
    }
    return ret;
}

VMemBlock vblock_valid(uintptr_t addr, VBlock &vb)
{
    auto [blk, tag] = vb.Valid(addr);
    if(blk.valid)
    {
        blk.user = (tag & VBLOCK_TAG_USER) != 0;
        blk.write = (tag & VBLOCK_TAG_WRITE) != 0;
        blk.exec = (tag & VBLOCK_TAG_EXEC) != 0;
        blk.lower_guard = (tag >> VBLOCK_TAG_GUARD_LOWER_POS) & GUARD_BITS_MAX;
        blk.upper_guard = (tag >> VBLOCK_TAG_GUARD_UPPER_POS) & GUARD_BITS_MAX;
    } 
    return blk;
}

int vblock_free(VMemBlock &v, VBlock &vb, bool unmap)
{
    auto freed = vb.Free(v);

    if(!freed)
        return -1;
    if(unmap)
    {
        return vmem_unmap(v);
    }
    return 0;
}

void VBlock::init(uintptr_t _base, unsigned int free_pages, unsigned int _level1_count)
{
    CriticalGuard cg(sl);
    base = _base;

    // get a free page for level1
    level1 = (uint64_t *)PMEM_TO_VMEM(Pmem.acquire(VBLOCK_64k).base);

    level1_count = _level1_count ? _level1_count : align_power_2(free_pages);
    if(level1_count > 8192U)
    {
        klog("VBlock::init: level1_count too high: %llu\n", level1_count);
        return;
    }

    for(auto i = 0U; i < level1_count; i++)
    {
        if(i < free_pages)
            level1[i] = VBLOCK_BLOCK_FREE;
        else
            level1[i] = VBLOCK_UNAVAIL;
    }

    level1_free = free_pages;
    level1_last_accessed_block = 0;
    level1_last_accessed_pointer = 0;

    inited = true;

    klog("vblock: init (free pages: %u, level1_count: %u)\n", free_pages, level1_count);
}

VMemBlock VBlock::Alloc(size_t length, uint32_t tag)
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
            return InvalidVMemBlock();
    }
}

VMemBlock VBlock::AllocFixed(size_t length, uintptr_t addr, uint32_t tag)
{
    addr -= base;

    switch(length)
    {
        case VBLOCK_64k:
            return AllocFixedLevel3(addr, tag);

        case VBLOCK_4M:
            return AllocFixedLevel2(addr, tag);

        case VBLOCK_512M:
            return AllocFixedLevel1(addr, tag);

        default:
            klog("vblock: invalid size requested: %llu\n", length);
            return InvalidVMemBlock();
    }
}

size_t VBlock::GetFreeLevel1Index()
{
    if(level1_free == 0)
        return VBLOCK_UNAVAIL;

    // loop offset from last accessed
    for(size_t i = 0U; i < level1_count; i++)
    {
        auto idx = (i + level1_last_accessed_block) % level1_count;
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
    for(size_t i = 0U; i < level1_count; i++)
    {
        auto idx = (i + level1_last_accessed_pointer) % level1_count;
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
    for(size_t i = 0U; i < level1_count; i++)
    {
        auto idx = (i + level1_last_accessed_pointer) % level1_count;
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

VMemBlock VBlock::AllocLevel1(uint32_t tag)
{
    CriticalGuard cg(sl);
    auto idx = GetFreeLevel1Index();
    if(idx == VBLOCK_UNAVAIL)
    {
        return InvalidVMemBlock();
    }
    level1[idx] = VBLOCK_BLOCK_ALLOC | ((uint64_t)tag << 32);
    level1_free--;
    level1_last_accessed_block = idx + 1;

#if DEBUG_VBLOCK
    klog("vblock: allocated 512MiB at %llx\n", base + idx * VBLOCK_512M);
#endif

    return VMemBlock {
        MemRegion {
            .base = base + idx * VBLOCK_512M,
            .length = VBLOCK_512M,
            .valid = true
        } 
    };  
}

VMemBlock VBlock::AllocFixedLevel1(uintptr_t addr, uint32_t tag)
{
    CriticalGuard cg(sl);
    auto idx = addr / VBLOCK_512M;
    
    if(level1[idx] != VBLOCK_BLOCK_FREE)
    {
        return InvalidVMemBlock();
    }

    level1[idx] = VBLOCK_BLOCK_ALLOC | ((uint64_t)tag << 32);
    level1_free--;
    level1_last_accessed_block = idx + 1;

#if DEBUG_VBLOCK
    klog("vblock: allocated 512MiB at %llx\n", base + idx * VBLOCK_512M);
#endif

    return VMemBlock {
        MemRegion {
            .base = base + idx * VBLOCK_512M,
            .length = VBLOCK_512M,
            .valid = true
        } 
    };  
}

VMemBlock VBlock::AllocFixedLevel2(uintptr_t addr, uint32_t tag)
{
    CriticalGuard cg(sl);
    auto idx = addr / VBLOCK_512M;
    if(level1[idx] & VBLOCK_BLOCK_ALLOC)
    {
        return InvalidVMemBlock();
    }
    if(level1[idx] == VBLOCK_BLOCK_FREE)
    {
        // TODO: add all these Pmem.acquires to the process used phys mem list
        level1[idx] = PmemAllocLevel2();
        level1_free--;
    }

    auto idx2 = (addr % VBLOCK_512M) / VBLOCK_4M;
    auto l2 = (level2 *)level1[idx];

    if(l2->b[idx2] != VBLOCK_BLOCK_FREE)
    {
        return InvalidVMemBlock();
    }

    l2->b[idx2] = VBLOCK_BLOCK_ALLOC | ((uint64_t)tag << 32);
    l2->free_count--;
    l2->last_accessed_block = idx2 + 1;
    level1_last_accessed_pointer = idx;

    addr = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M;

#if DEBUG_VBLOCK
    klog("vblock: allocated 4MiB at %llx\n", addr);
#endif

    return VMemBlock {
        MemRegion
        {
            .base = addr,
            .length = VBLOCK_4M,
            .valid = true
        }
    };
}

VMemBlock VBlock::AllocLevel2(uint32_t tag)
{
    CriticalGuard cg(sl);
    auto idx = GetLevel1IndexToNotFullLevel2();
    uint64_t l2_pmem;
    if(idx == VBLOCK_UNAVAIL)
    {
        // no current pointers to a free level 2 are available - make one
        idx = GetFreeLevel1Index();
        if(idx == VBLOCK_UNAVAIL)
            return InvalidVMemBlock();

#if DEBUG_VBLOCK
        klog("vblock: creating new level2 buddy\n");
#endif

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
        return InvalidVMemBlock();
    }
    auto l2 = (level2 *)l2_pmem;
    l2->b[idx2] = VBLOCK_BLOCK_ALLOC | ((uint64_t)tag << 32);
    l2->free_count--;
    l2->last_accessed_block = idx2 + 1;
    level1_last_accessed_pointer = idx;

    auto addr = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M;

#if DEBUG_VBLOCK
    klog("vblock: allocated 4MiB at %llx\n", addr);
#endif

    return VMemBlock {
        MemRegion
        {
            .base = addr,
            .length = VBLOCK_4M,
            .valid = true
        }
    };
}

VMemBlock VBlock::AllocFixedLevel3(uintptr_t addr, uint32_t tag)
{
    CriticalGuard cg(sl);
    auto idx = addr / VBLOCK_512M;
    if(level1[idx] & VBLOCK_BLOCK_ALLOC)
    {
        return InvalidVMemBlock();
    }
    if(level1[idx] == VBLOCK_BLOCK_FREE)
    {
        // TODO: add all these Pmem.acquires to the process used phys mem list
        level1[idx] = PmemAllocLevel2();
        level1_free--;
    }

    auto idx2 = (addr % VBLOCK_512M) / VBLOCK_4M;
    auto l2 = (level2 *)level1[idx];

    if(l2->b[idx2] & VBLOCK_BLOCK_ALLOC)
    {
        return InvalidVMemBlock();
    }
    if(l2->b[idx2] == VBLOCK_BLOCK_FREE)
    {
        l2->b[idx2] = PmemAllocLevel3();
        l2->free_count--;
    }

    auto idx3 = (addr % VBLOCK_4M) / VBLOCK_64k;
    auto l3 = (level3 *)l2->b[idx2];
    if(l3->b[idx3] != VBLOCK_BLOCK_FREE)
    {
        return InvalidVMemBlock();
    }

    l3->b[idx3] = VBLOCK_BLOCK_ALLOC | ((uint64_t)tag << 32);
    l3->free_count--;
    l3->last_accessed_block = idx3 + 1;
    level1_last_accessed_pointer = idx;
    l2->last_accessed_buddy = idx2;

    addr = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M + idx3 * VBLOCK_64k;

#if DEBUG_VBLOCK
    klog("vblock: allocated 64 kiB at %llx\n", addr);
#endif

    return VMemBlock {
        MemRegion
        {
            .base = addr,
            .length = VBLOCK_64k,
            .valid = true
        }
    };
}

VMemBlock VBlock::AllocLevel3(uint32_t tag)
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
            return InvalidVMemBlock();

#if DEBUG_VBLOCK
        klog("vblock: creating new level2 buddy\n");
#endif

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
            return InvalidVMemBlock();

#if DEBUG_VBLOCK
        klog("vblock: creating new level3 buddy\n");
#endif

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
        return InvalidVMemBlock();
    }

    auto l3 = (level3 *)l3_pmem;
    l3->b[idx3] = VBLOCK_BLOCK_ALLOC | ((uint64_t)tag << 32);
    l3->free_count--;
    l3->last_accessed_block = idx3 + 1;
    level1_last_accessed_pointer = idx;
    l2->last_accessed_buddy = idx2;

    auto addr = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M + idx3 * VBLOCK_64k;

#if DEBUG_VBLOCK
    klog("vblock: allocated 64 kiB at %llx\n", addr);
#endif

    return VMemBlock {
        MemRegion
        {
            .base = addr,
            .length = VBLOCK_64k,
            .valid = true
        }
    };
}

uint64_t VBlock::PmemAllocLevel2()
{
    if(cur_level2_page == 0 || cur_level2_page_offset >= VBLOCK_64k)
    {
        cur_level2_page = PMEM_TO_VMEM(Pmem.acquire(VBLOCK_64k).base);
        cur_level2_page_offset = 0;

#if DEBUG_VBLOCK
        klog("vblock: new level2 page at %llx\n", cur_level2_page);
#endif
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

#if DEBUG_VBLOCK
        klog("vblock: new level3 page at %llx\n", cur_level3_page);
#endif
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

std::pair<VMemBlock, uint32_t> VBlock::Valid(uintptr_t addr)
{
    if(!inited)
    {
        return std::make_pair(InvalidVMemBlock(), 0);
    }

    addr -= base;

    auto idx = addr / VBLOCK_512M;

    CriticalGuard cg(sl);
    if(level1[idx] == VBLOCK_UNAVAIL || level1[idx] == VBLOCK_BLOCK_FREE)
    {
        // not allocated
        return std::make_pair(InvalidVMemBlock(), 0);
    }
    else if(level1[idx] & VBLOCK_BLOCK_ALLOC)
    {
        // allocated as large block
        return std::make_pair(VMemBlock {
            MemRegion {
                .base = base + idx * VBLOCK_512M,
                .length = VBLOCK_512M,
                .valid = true
            } }, (level1[idx] >> 32));
    }
    
    // check level2
    auto idx2 = (addr % VBLOCK_512M) / VBLOCK_4M;
    auto l2 = (level2 *)level1[idx];
    if(l2->b[idx2] == VBLOCK_UNAVAIL || l2->b[idx2] == VBLOCK_BLOCK_FREE)
    {
        // not allocated
        return std::make_pair(InvalidVMemBlock(), 0);
    }
    else if(l2->b[idx2] & VBLOCK_BLOCK_ALLOC)
    {
        // allocated as large block
        return std::make_pair(VMemBlock {
            MemRegion {
                .base = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M,
                .length = VBLOCK_4M,
                .valid = true
            } }, (l2->b[idx2] >> 32));
    }

    // check level3
    auto idx3 = (addr % VBLOCK_4M) / VBLOCK_64k;
    auto l3 = (level3 *)l2->b[idx2];
    if(!(l3->b[idx3] & VBLOCK_BLOCK_ALLOC))
    {
        // not allocated
        return std::make_pair(InvalidVMemBlock(), 0);
    }

    return std::make_pair(VMemBlock {
        MemRegion {
            .base = base + idx * VBLOCK_512M + idx2 * VBLOCK_4M + idx3 * VBLOCK_64k,
            .length = VBLOCK_64k,
            .valid = true
        } }, (l3->b[idx3] >> 32));
}

bool VBlock::Free(VMemBlock &v)
{
    switch(v.length)
    {
        case VBLOCK_64k:
            return FreeLevel3(v);
        case VBLOCK_4M:
            return FreeLevel2(v);
        case VBLOCK_512M:
            return FreeLevel1(v);
        default:
            klog("vblock: invalid length of blcok to free: %llu\n", v.length);
            return false;
    }
}

bool VBlock::FreeLevel1(VMemBlock &v)
{
    auto addr = v.base;
    addr -= base;

    auto idx = addr / VBLOCK_512M;

    CriticalGuard cg(sl);
    if((level1[idx] & VBLOCK_BLOCK_ALLOC_MASK) == VBLOCK_BLOCK_ALLOC)
    {
        // allocated
        level1[idx] = VBLOCK_BLOCK_FREE;
        v.valid = false;
        return true;
    }

    v.valid = false;
    return false;
}

bool VBlock::FreeLevel2(VMemBlock &v)
{
    auto addr = v.base;
    addr -= base;

    auto idx = addr / VBLOCK_512M;

    CriticalGuard cg(sl);
    if((level1[idx] & VBLOCK_BLOCK_ALLOC_MASK) == VBLOCK_BLOCK_ALLOC ||
        level1[idx] == VBLOCK_BLOCK_FREE ||
        level1[idx] == VBLOCK_UNAVAIL)
    {
        // invalid for a level1 pointing to level2
        v.valid = false;
        return false;
    }

    // check level2
    auto idx2 = (addr % VBLOCK_512M) / VBLOCK_4M;
    auto l2 = (level2 *)level1[idx];
    if((l2->b[idx2] & VBLOCK_BLOCK_ALLOC_MASK) == VBLOCK_BLOCK_ALLOC)
    {
        l2->b[idx2] = VBLOCK_BLOCK_FREE;
        v.valid = false;
        return true;
    }

    v.valid = false;
    return false;
}

bool VBlock::FreeLevel3(VMemBlock &v)
{
    auto addr = v.base;
    addr -= base;

    auto idx = addr / VBLOCK_512M;

    CriticalGuard cg(sl);
    if((level1[idx] & VBLOCK_BLOCK_ALLOC_MASK) == VBLOCK_BLOCK_ALLOC ||
        level1[idx] == VBLOCK_BLOCK_FREE ||
        level1[idx] == VBLOCK_UNAVAIL)
    {
        // invalid for a level1 pointing to level2
        v.valid = false;
        return false;
    }

    // check level2
    auto idx2 = (addr % VBLOCK_512M) / VBLOCK_4M;
    auto l2 = (level2 *)level1[idx];
    if((l2->b[idx2] & VBLOCK_BLOCK_ALLOC_MASK) == VBLOCK_BLOCK_ALLOC ||
        l2->b[idx2] == VBLOCK_BLOCK_FREE)
    {
        // not pointing to a level3 block
        v.valid = false;
        return false;
    }

    // check level3
    auto idx3 = (addr % VBLOCK_4M) / VBLOCK_64k;
    auto l3 = (level3 *)l2->b[idx2];
    if((l3->b[idx3] & VBLOCK_BLOCK_ALLOC_MASK) == VBLOCK_BLOCK_ALLOC)
    {
        // valid to free
        l3->b[idx3] = VBLOCK_BLOCK_FREE;
        v.valid = false;
        return true;
    }

    v.valid = false;
    return false;
}
