/* malloc interface */
#include "osmutex.h"
#include "region_allocator.h"
static Spinlock sl_sram4;

extern "C"
{
    void *dlmalloc(size_t n);
    void dlfree(void *p);
}

void *malloc_region(size_t n, int reg_id)
{
    switch(reg_id)
    {
        case REG_ID_SRAM4:
            {
                CriticalGuard cg(sl_sram4);
                return dlmalloc(n);
            }

        default:
            return nullptr;
    }
}

void free_region(void *p, int reg_id)
{
    switch(reg_id)
    {
        case REG_ID_SRAM4:
            {
                CriticalGuard cg(sl_sram4);
                dlfree(p);
            }

        default:
            return;
    }
}
