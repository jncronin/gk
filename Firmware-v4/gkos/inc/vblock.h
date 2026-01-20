#ifndef VBLOCK_H
#define VBLOCK_H

#include <cstdint>
#include "buddy.h"
#include "ostypes.h"

#define VBLOCK_TAG_USER                     0x1
#define VBLOCK_TAG_WRITE                    0x2
#define VBLOCK_TAG_EXEC                     0x4
#define VBLOCK_TAG_GUARD_MASK               (0xfULL << 3)
#define VBLOCK_TAG_GUARD_LOWER_POS          3
#define VBLOCK_TAG_GUARD_UPPER_POS          5
#define VBLOCK_TAG_GUARD(lower, upper)      (((lower) << VBLOCK_TAG_GUARD_LOWER_POS) | ((upper) << VBLOCK_TAG_GUARD_UPPER_POS))
#define VBLOCK_TAG_TLS                      (1ULL << 7)
#define VBLOCK_TAG_FILE                     (1ULL << 8)
#define VBLOCK_TAG_WT                       (1ULL << 9)

class VBlock;
extern VBlock vblock;

void init_vblock();
VMemBlock vblock_alloc(size_t size, bool user, bool write, bool exec,
    unsigned int lower_guard = 0, unsigned int upper_guard = 0,
    VBlock &vb = vblock, bool map = false);
VMemBlock vblock_alloc_fixed(size_t size, uintptr_t addr, bool user, bool write, bool exec,
    unsigned int lower_guard = 0, unsigned int upper_guard = 0,
    VBlock &vb = vblock, bool map = false);
VMemBlock vblock_valid(uintptr_t addr, VBlock &vb = vblock, uint32_t *tag = nullptr);
int vblock_free(VMemBlock &v, VBlock &vb = vblock, bool unmap = false);

size_t vblock_size_for(size_t n);

#define VBLOCK_64k      65536ULL
#define VBLOCK_4M       (4ULL*1024*1024)
#define VBLOCK_512M     (512ULL*1024*1024)

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

        // are we initialized?
        bool inited = false;

        Spinlock sl{};

    protected:
        VMemBlock AllocLevel1(uint32_t tag);
        VMemBlock AllocLevel2(uint32_t tag);
        VMemBlock AllocLevel3(uint32_t tag);

        VMemBlock AllocFixedLevel1(uintptr_t addr, uint32_t tag);
        VMemBlock AllocFixedLevel2(uintptr_t addr, uint32_t tag);
        VMemBlock AllocFixedLevel3(uintptr_t addr, uint32_t tag);

        bool FreeLevel1(VMemBlock &v);
        bool FreeLevel2(VMemBlock &v);
        bool FreeLevel3(VMemBlock &v);

        size_t GetFreeLevel1Index();
        size_t GetLevel1IndexToNotFullLevel2();
        std::pair<size_t, size_t> GetLevel12IndexToNotFullLevel3();
        size_t GetFreeLevel2Index(size_t level1_idx);
        size_t GetFreeLevel3Index(size_t level1_idx, size_t level2_idx);

        uint64_t PmemAllocLevel2();
        uint64_t PmemAllocLevel3();

        unsigned int level1_count = 0;

    public:
        void init(uintptr_t base, unsigned int free_level1_count = 8192U, unsigned int level1_count = 0U);

        VMemBlock Alloc(size_t size, uint32_t tag = 0);
        VMemBlock AllocFixed(size_t size, uintptr_t addr, uint32_t tag = 0);
        bool Free(VMemBlock &be);
        std::pair<VMemBlock, uint32_t> Valid(uintptr_t addr);
};

#endif
