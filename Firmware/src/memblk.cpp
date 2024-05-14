#include <stm32h7xx.h>
#include "memblk.h"
#include "buddy.h"
#include "SEGGER_RTT.h"
#include "gk_conf.h"
#include "scheduler.h"
#include "thread.h"

#define DEBUG_MEMBLK        0

__attribute__((section(".sram4"))) BuddyAllocator<256, 0x80000, 0x24000000> b_axisram;
__attribute__((section(".sram4"))) BuddyAllocator<256, 0x20000, 0x20000000> b_dtcm;
__attribute__((section(".sram4"))) BuddyAllocator<256, 0x80000, 0x30000000> b_sram;
__attribute__((section(".sram4"))) BuddyAllocator<512*1024, 65536*1024, GK_SDRAM_BASE> b_sdram;

// The following are the ends of all the input sections in the
//  relevant memory region.  We choose the highest to init the
//  memory manager.

// axisram
extern int _edata;
extern int _ebss;
extern int _elwip_data;

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
    if((uintptr_t)&_elwip_data > eaxisram)
        eaxisram = (uintptr_t)&_elwip_data;

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
    add_memory_region(esdram, GK_SDRAM_BASE + 64 * 1024 * 1024 - esdram, MemRegionType::SDRAM);
    //add_memory_region(0x38000000UL, 0x10000);   // SRAM4 - handled separately by malloc interface
}

MemRegion memblk_allocate_for_stack(size_t n, CPUAffinity affinity)
{
    MemRegionType rtlist[4];
    int nrts = 0;

    switch(affinity)
    {
#if GK_DUAL_CORE
        case CPUAffinity::Either:
        case CPUAffinity::PreferM7:
            rtlist[nrts++] = MemRegionType::AXISRAM;
            rtlist[nrts++] = MemRegionType::SRAM;
            rtlist[nrts++] = MemRegionType::SDRAM;
            break;
#endif

#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
        case CPUAffinity::M4Only:
#if GK_DUAL_CORE
        case CPUAffinity::PreferM4:
#endif
            rtlist[nrts++] = MemRegionType::SRAM;
            rtlist[nrts++] = MemRegionType::AXISRAM;
            rtlist[nrts++] = MemRegionType::SDRAM;
            break;
#endif

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

void memblk_deallocate(MemRegion &r)
{
    if(!r.valid)
        return;
    
    BuddyEntry be;
    be.base = r.address;
    be.length = r.length;
    be.valid = true;

    switch(r.rt)
    {
        case MemRegionType::AXISRAM:
            b_axisram.release(be);
            r.valid = false;
            break;

        case MemRegionType::DTCM:
            b_dtcm.release(be);
            r.valid = false;
            break;

        case MemRegionType::SDRAM:
            b_sdram.release(be);
            r.valid = false;
            break;

        case MemRegionType::SRAM:
            b_sram.release(be);
            r.valid = false;
            break;
    }

#if DEBUG_MEMBLK
    if(!r.valid)
    {
        SEGGER_RTT_printf(0, "memblk deallocate: %x - %x by %x\n", r.address, r.address + r.length,
            (uint32_t)(uintptr_t)GetCurrentThreadForCore());
    }
#endif
}

MemRegion memblk_allocate(size_t n, MemRegionType rtype)
{
    BuddyEntry ret;
    if(!n)
    {
        MemRegion mret;
        mret.valid = false;
        return mret;
    }

    switch(rtype)
    {
        case MemRegionType::AXISRAM:
            ret = b_axisram.acquire(n);
            break;

        case MemRegionType::DTCM:
            ret = b_dtcm.acquire(n);
            break;

        case MemRegionType::SDRAM:
            ret = b_sdram.acquire(n);
            break;

        case MemRegionType::SRAM:
            ret = b_sram.acquire(n);
            break;

        default:
            ret.valid = false;
            break;
    }

    MemRegion mr;
    mr.rt = rtype;
    if(ret.valid)
    {
        mr.address = ret.base;
        mr.length = ret.length;
#if DEBUG_MEMBLK
        SEGGER_RTT_printf(0, "memblk allocate: %x - %x from %x\n", mr.address, mr.address + mr.length,
            (uint32_t)(uintptr_t)GetCurrentThreadForCore());
#endif
    }
    else
    {
        mr.address = 0;
        mr.length = 0;
    }
    mr.valid = ret.valid;

    return mr;
}

bool operator==(const MemRegion &a, const MemRegion &b)
{
    if(!a.valid && !b.valid) return true;
    if(a.address != b.address) return false;
    if(a.length != b.length) return false;
    if(a.rt != b.rt) return false;
    if(a.valid != b.valid) return false;
    return true;
}

bool operator!=(const MemRegion &a, const MemRegion &b)
{
    return !(a == b);
}
