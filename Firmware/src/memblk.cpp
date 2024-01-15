#include <stm32h7xx.h>
#include "memblk.h"
#include "buddy.h"

__attribute__((section(".sram4"))) BuddyAllocator<256, 0x80000, 0x24000000> b_axisram;
__attribute__((section(".sram4"))) BuddyAllocator<256, 0x20000, 0x20000000> b_dtcm;
__attribute__((section(".sram4"))) BuddyAllocator<256, 0x80000, 0x30000000> b_sram;
__attribute__((section(".sram4"))) BuddyAllocator<512*1024, 65536*1024, 0xc0000000> b_sdram;


// The following are the ends of all the input sections in the
//  relevant memory region.  We choose the highest to init the
//  memory manager.

// axisram
extern int _edata;
extern int _ebss;

// dtcm
extern int _edtcm_bss;
extern int _edtcm2;
extern int _ecm7_stack;

// sram
extern int _esram_bss;
extern int _ecm4_stack;

// sdram
extern int _esdram;

static void add_memory_region(uintptr_t base, uintptr_t size, MemRegionType type)
{
    BuddyEntry be;
    be.base = base;
    be.length = size;
    
    switch(type)
    {
        case MemRegionType::AXISRAM:
            be.valid = is_power_of_2(size) && is_multiple_of(base, b_axisram.MinBuddySize());
            b_axisram.release(be);
            break;
        case MemRegionType::DTCM:
            be.valid = is_power_of_2(size) && is_multiple_of(base, b_dtcm.MinBuddySize());
            b_dtcm.release(be);
            break;
        case MemRegionType::SDRAM:
            be.valid = is_power_of_2(size) && is_multiple_of(base, b_sdram.MinBuddySize());
            b_sdram.release(be);
            break;
        case MemRegionType::SRAM:
            be.valid = is_power_of_2(size) && is_multiple_of(base, b_sram.MinBuddySize());
            b_sram.release(be);
            break;
    }
}

template<typename T> constexpr static T align_up(T val, T align)
{
    if(val % align)
    {
        val -= val % align;
        val += align;
    }
    return val;
}

void init_memblk()
{
    uintptr_t eaxisram = 0;
    uintptr_t edtcm = 0;
    uintptr_t esram = 0;
    uintptr_t esdram = 0;

    if((uintptr_t)&_edata > eaxisram)
        eaxisram = (uintptr_t)&_edata;
    if((uintptr_t)&_ebss > eaxisram)
        eaxisram = (uintptr_t)&_ebss;

    if((uintptr_t)&_edtcm_bss > edtcm)
        edtcm = (uintptr_t)&_edtcm_bss;
    if((uintptr_t)&_edtcm2 > edtcm)
        edtcm = (uintptr_t)&_edtcm2;
    if((uintptr_t)&_ecm7_stack > edtcm)
        edtcm = (uintptr_t)&_ecm7_stack;

    if((uintptr_t)&_esram_bss > esram)
        esram = (uintptr_t)&_esram_bss;
    if((uintptr_t)&_ecm4_stack > esram)
        esram = (uintptr_t)&_ecm4_stack;

    if((uintptr_t)&_esdram > esdram)
        esdram = (uintptr_t)&_esdram;

    eaxisram = align_up(eaxisram, 1024U);
    edtcm = align_up(edtcm, 1024U);
    esram = align_up(esram, 1024U);
    esdram = align_up(esdram, 1024U);

    add_memory_region(eaxisram, 0x24080000UL - eaxisram, MemRegionType::AXISRAM);
    add_memory_region(edtcm, 0x20020000 - edtcm, MemRegionType::DTCM);
    add_memory_region(esram, 0x30048000 - esram, MemRegionType::SRAM);
    add_memory_region(esdram, 0x60000000UL + 64 * 1024 * 1024 - esdram, MemRegionType::SDRAM);
    //add_memory_region(0x38000000UL, 0x10000);   // SRAM4 - handled separately by malloc interface
}

MemRegion memblk_allocate_for_stack(size_t n, CPUAffinity affinity)
{
    MemRegionType rtlist[4];
    int nrts = 0;

    switch(affinity)
    {
        case CPUAffinity::Either:
        case CPUAffinity::PreferM7:
            rtlist[nrts++] = MemRegionType::AXISRAM;
            rtlist[nrts++] = MemRegionType::SRAM;
            rtlist[nrts++] = MemRegionType::SDRAM;
            break;

        case CPUAffinity::M4Only:
        case CPUAffinity::PreferM4:
            rtlist[nrts++] = MemRegionType::SRAM;
            rtlist[nrts++] = MemRegionType::AXISRAM;
            rtlist[nrts++] = MemRegionType::SDRAM;
            break;

        case CPUAffinity::M7Only:
            rtlist[nrts++] = MemRegionType::DTCM;
            rtlist[nrts++] = MemRegionType::AXISRAM;
            rtlist[nrts++] = MemRegionType::SRAM;
            rtlist[nrts++] = MemRegionType::SDRAM;
            break;
    }

    for(int i = 0; i < nrts; i++)
    {
        auto r = memblk_allocate(n, rtlist[i]);
        if(r.valid)
        {
            return r;
        }
    }

    MemRegion ret;
    ret.valid = false;
    ret.address = 0;
    ret.length = 0;
    return ret;
}
