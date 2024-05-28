#if 0

/* malloc interface */
#include "osmutex.h"
#include "region_allocator.h"
__attribute__((section(".sram4"))) static Spinlock sl_sram4;

extern "C"
{
    void *dlmalloc(size_t n);
    void dlfree(void *p);
    void *dlcalloc(size_t nmemb, size_t size);
    void *dlrealloc(void *ptr, size_t size);
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

void *realloc_region(void *ptr, size_t n, int reg_id)
{
    switch(reg_id)
    {
        case REG_ID_SRAM4:
            {
                CriticalGuard cg(sl_sram4);
                return dlrealloc(ptr, n);
            }

        default:
            return nullptr;
    }
}

void *calloc_region(size_t nmemb, size_t size, int reg_id)
{
    switch(reg_id)
    {
        case REG_ID_SRAM4:
            {
                CriticalGuard cg(sl_sram4);
                return dlcalloc(nmemb, size);
            }

        default:
            return nullptr;
    }
}

/* sbrk implementation for sram4 */
static constexpr uint32_t max_sbrk4 = 0x38000000UL + 64*1024UL;
extern char _esram4;
__attribute__((section(".sram4"))) static uint32_t cur_sbrk4 = (uint32_t)(uintptr_t)&_esram4;

extern "C" void *sbrksram4(int n)
{
    if(n < 0) n = 0;
    auto nn = static_cast<uint32_t>(n);

    if((n + cur_sbrk4) > max_sbrk4)
    {
        return (void *)-1;
    }
    auto old_brk = cur_sbrk4;

    cur_sbrk4 += nn;

    return (void *)(uintptr_t)old_brk;
}

#endif