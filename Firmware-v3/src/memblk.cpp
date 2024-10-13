#include <stm32h7rsxx.h>
#include "memblk.h"
#include "buddy.h"
#include "SEGGER_RTT.h"
#include "gk_conf.h"
#include "scheduler.h"
#include "thread.h"

#define DEBUG_MEMBLK        0

#if GK_MEMBLK_STATS
struct memblk_stats_t { MemRegion mr; std::string tag; };
static std::map<uint32_t, memblk_stats_t> memblk_tags;
static MemRegion memblk_usage[GK_MEMBLK_USAGE_MAX] = { 0 }; 
#endif

BuddyAllocator<256, 0x80000, 0x24000000> b_axisram;
BuddyAllocator<256, 0x40000, 0x20000000> b_dtcm;
BuddyAllocator<256, 0x8000, 0x30000000> b_sram;
BuddyAllocator<4*1024, GK_SDRAM_SIZE, GK_SDRAM_BASE> b_sdram;
BuddyAllocator<1024, 0x40000, 0> b_itcm;
static bool inited = false;

// The following are the ends of all the input sections in the
//  relevant memory region.  We choose the highest to init the
//  memory manager.

// axisram
extern int _edata;
extern int _ebss;
extern int _edata4;
extern int _ertt;
extern int _elwip_init_data;

// dtcm
extern int _edtcm_bss;
extern int _edtcm2;
extern int _ecm7_stack;

// sram
extern int _esram_bss;
extern int _esramahb;

// sdram
extern int _esdram;
extern int _elwip_data;

// itcm
extern int _eitcm;

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
        case MemRegionType::ITCM:
            be.valid = is_power_of_2(size) && is_multiple_of(base, b_itcm.MinBuddySize());
            b_itcm.release(be);
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

bool MemRegion::is_cacheable() const
{
    if(rt == MemRegionType::DTCM)
        return false;
    if(rt == MemRegionType::SRAM)
        return false;
    return true;
}

static void incr_axisram(const void *addr, uintptr_t *eaxisram, uintptr_t *eaxisram4)
{
    auto p = (uintptr_t)addr;
    if(p >= 0x24020000U && p < 0x24040000U) // SRAM4 start
    {
        if(p > *eaxisram)
            *eaxisram = p;
    }
    else if(p >= 0x24060000U && p < 0x24072000U)
    {
        if(p > *eaxisram4)
            *eaxisram4 = p;
    }
}

extern "C" void init_memblk()
{
    if(inited)
        return;

    b_axisram.init();
    b_dtcm.init();
    b_sdram.init();
    b_sram.init();
    b_itcm.init();
    
    uintptr_t eaxisram = 0;
    uintptr_t eaxisram4 = 0;
    uintptr_t edtcm = 0;
    uintptr_t esram = 0;
    uintptr_t esdram = 0;
    uintptr_t eitcm = 0;

    // handle ends of axisram data in linker script - there may be a split at
    //  the beginning of sram4 so be careful
    incr_axisram(&_edata, &eaxisram, &eaxisram4);
    incr_axisram(&_ebss, &eaxisram, &eaxisram4);
    incr_axisram(&_elwip_init_data, &eaxisram, &eaxisram4);
    incr_axisram(&_edata4, &eaxisram, &eaxisram4);
    incr_axisram(&_ertt, &eaxisram, &eaxisram4);

    if((uintptr_t)&_edtcm_bss > edtcm)
        edtcm = (uintptr_t)&_edtcm_bss;
    if((uintptr_t)&_edtcm2 > edtcm)
        edtcm = (uintptr_t)&_edtcm2;
    if((uintptr_t)&_ecm7_stack > edtcm)
        edtcm = (uintptr_t)&_ecm7_stack;

    if((uintptr_t)&_esram_bss > esram)
        esram = (uintptr_t)&_esram_bss;
    if((uintptr_t)&_esramahb > esram)
        esram = (uintptr_t)&_esramahb;

    if((uintptr_t)&_esdram > esdram)
        esdram = (uintptr_t)&_esdram;
    if((uintptr_t)&_elwip_data > esdram)
        esdram = (uintptr_t)&_elwip_data;

    if((uintptr_t)&_eitcm > eitcm)
        eitcm = (uintptr_t)&_eitcm;

    eaxisram = align_up(eaxisram, (uintptr_t)b_axisram.MinBuddySize());
    eaxisram4 = align_up(eaxisram4, (uintptr_t)b_axisram.MinBuddySize());
    edtcm = align_up(edtcm, (uintptr_t)b_dtcm.MinBuddySize());
    esram = align_up(esram, (uintptr_t)b_sram.MinBuddySize());
    esdram = align_up(esdram, (uintptr_t)b_sdram.MinBuddySize());
    eitcm = align_up(eitcm, (uintptr_t)b_itcm.MinBuddySize());

    // Get sizes of the various memory regions
    // calculate total memory
    unsigned int itcm_size, dtcm_size, axisram_base, axisram_end, axisram_size, ahbsram_size;
    auto obw2sr = FLASH->OBW2SR;
    auto dtcm_axi_share = (obw2sr & FLASH_OBW2SR_DTCM_AXI_SHARE_Msk) >>
        FLASH_OBW2SR_DTCM_AXI_SHARE_Pos;
    auto itcm_axi_share = (obw2sr & FLASH_OBW2SR_ITCM_AXI_SHARE_Msk) >>
        FLASH_OBW2SR_ITCM_AXI_SHARE_Pos;
    switch(dtcm_axi_share)
    {
        case 1:
            dtcm_size = 128*1024;
            axisram_end = 0x24050000;
            break;
        case 2:
            dtcm_size = 192*1024;
            axisram_end = 0x24040000;
            break;
        default:
            dtcm_size = 64*1024;
            axisram_end = 0x24060000;
            break;
    }
    switch(itcm_axi_share)
    {
        case 1:
            itcm_size = 128*1024;
            axisram_base = 0x24010000;
            break;
        case 2:
            itcm_size = 192*1024;
            axisram_base = 0x24020000;
            break;
        default:
            itcm_size = 64*1024;
            axisram_base = 0x24000000;
            break;
    }
    axisram_size = axisram_end - axisram_base;
    ahbsram_size = 0x8000;

    SEGGER_RTT_printf(0, "memory_map:\n"
        "  ITCM:    0x00000000 - 0x%08x (%d kbytes)\n"
        "  DTCM:    0x20000000 - 0x%08x (%d kbytes)\n"
        "  AXISRAM: 0x%08x - 0x%08x (%d kbytes)%s\n"
        "  AHBSRAM: 0x30000000 - 0x%08x (%d kbytes)\n",
        itcm_size, itcm_size/1024,
        0x20000000U + dtcm_size, dtcm_size/1024,
        axisram_base, axisram_end, axisram_size/1024,
        (obw2sr & FLASH_OBW2SR_ECC_ON_SRAM) ? "" : " and 0x24060000 - 0x24072000 (72 kbytes)",
        0x30000000U + ahbsram_size, ahbsram_size/1024);

    if(eaxisram < axisram_base) eaxisram = axisram_base;
    if(eaxisram4 < 0x24060000U) eaxisram4 = 0x24060000U;
    if(edtcm < 0x20000000U) edtcm = 0x20000000U;
    if(esdram < 0x90000000U) esdram = 0x90000000U;

    add_memory_region(eaxisram, axisram_end - eaxisram, MemRegionType::AXISRAM);
    if(!(obw2sr & FLASH_OBW2SR_ECC_ON_SRAM))
        add_memory_region(eaxisram4, 0x24072000 - eaxisram4, MemRegionType::AXISRAM);
    
    add_memory_region(edtcm, 0x20000000U + dtcm_size - edtcm, MemRegionType::DTCM);
    add_memory_region(esram, 0x30008000 - esram, MemRegionType::SRAM);
    add_memory_region(esdram, GK_SDRAM_BASE + GK_SDRAM_SIZE - esdram, MemRegionType::SDRAM);
    add_memory_region(eitcm, itcm_size - eitcm, MemRegionType::ITCM);

    inited = true;
}

#if GK_MEMBLK_STATS
MemRegion memblk_allocate_for_stack(size_t n, CPUAffinity affinity, const std::string &tag,
    int pref)
#else
MemRegion memblk_allocate_for_stack(size_t n, CPUAffinity affinity, int pref)
#endif
{
    MemRegionType rtlist[5];
    int nrts = 0;

    switch(affinity)
    {
#if GK_DUAL_CORE
        case CPUAffinity::Either:
        case CPUAffinity::PreferM7:
            rtlist[nrts++] = MemRegionType::AXISRAM;
            //rtlist[nrts++] = MemRegionType::SRAM;
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

#if !GK_DUAL_CORE && !GK_DUAL_CORE_AMP
        case CPUAffinity::PreferM4:
            rtlist[nrts++] = MemRegionType::SRAM;
            rtlist[nrts++] = MemRegionType::DTCM;
            rtlist[nrts++] = MemRegionType::AXISRAM;
            rtlist[nrts++] = MemRegionType::SDRAM;
            break;
#endif


        case CPUAffinity::M7Only:
            switch(pref)
            {
                case STACK_PREFERENCE_SDRAM_RAM_TCM:
                    // low performance user thread e.g. gkmenu
                    rtlist[nrts++] = MemRegionType::SDRAM;
                    rtlist[nrts++] = MemRegionType::SRAM;
                    rtlist[nrts++] = MemRegionType::AXISRAM;
                    rtlist[nrts++] = MemRegionType::DTCM;
                    rtlist[nrts++] = MemRegionType::ITCM;
                    break;
                case STACK_PREFERENCE_TCM_RAM_SDRAM:
                    // high performance user thread e.g. game
                    rtlist[nrts++] = MemRegionType::DTCM;
                    rtlist[nrts++] = MemRegionType::ITCM;
                    rtlist[nrts++] = MemRegionType::AXISRAM;
                    rtlist[nrts++] = MemRegionType::SRAM;
                    rtlist[nrts++] = MemRegionType::SDRAM;
                    break;
                default:
                    // likely a kernel thread
                    rtlist[nrts++] = MemRegionType::DTCM;
                    rtlist[nrts++] = MemRegionType::AXISRAM;
                    //rtlist[nrts++] = MemRegionType::SRAM;
                    rtlist[nrts++] = MemRegionType::SDRAM;
                    break;
            }
            break;
    }

    for(int i = 0; i < nrts; i++)
    {
        auto r = memblk_allocate(n, rtlist[i]
#if GK_MEMBLK_STATS
            , tag
#endif
        );
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

    if(r.address == 0x60000000) BKPT();
    
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

        case MemRegionType::ITCM:
            b_itcm.release(be);
            r.valid = false;
            break;
    }

#if GK_MEMBLK_STATS
    {
        CriticalGuard cg_ms;
        auto iter = memblk_tags.find(r.address);
        if(iter != memblk_tags.end())
        {
            memblk_tags.erase(iter);
        }
    }
#endif

#if DEBUG_MEMBLK
    if(!r.valid)
    {
        SEGGER_RTT_printf(0, "memblk deallocate: %x - %x by %x\n", r.address, r.address + r.length,
            (uint32_t)(uintptr_t)GetCurrentThreadForCore());
    }
#endif
}

#if GK_MEMBLK_STATS
static MemRegion _memblk_allocate(size_t n, MemRegionType rtype)
#else
MemRegion memblk_allocate(size_t n, MemRegionType rtype)
#endif
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

        case MemRegionType::ITCM:
            ret = b_itcm.acquire(n);
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

#if GK_MEMBLK_STATS
MemRegion memblk_allocate(size_t n, MemRegionType rtype, const std::string &tag)
{
    auto mr = _memblk_allocate(n, rtype);
    if(mr.valid)
    {
        memblk_stats_t ms { .mr = mr, .tag = tag };
        CriticalGuard cg_ms;
        memblk_tags[mr.address] = ms;
    }
    return mr;
}

MemRegion memblk_allocate(size_t n, MemRegionType rtype, int usage)
{
    auto mr = _memblk_allocate(n, rtype);
    if(mr.valid)
    {
        if(usage < GK_MEMBLK_USAGE_MAX)
        {
            CriticalGuard cg_ms;
            memblk_usage[usage] = mr;
        }
    }
    return mr;
}
#endif

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

#if GK_MEMBLK_STATS
void memblk_stats()
{
    CriticalGuard cg;
    // add usage values back as strings
    for(int i = 0; i < GK_MEMBLK_USAGE_MAX; i++)
    {
        if(memblk_usage[i].valid)
        {
            switch(i)
            {
                case GK_MEMBLK_USAGE_KERNEL_HEAP:
                    memblk_tags[memblk_usage[i].address] = { .mr = memblk_usage[i], .tag = "kernel heap" };
                    break;
                default:   
                    memblk_tags[memblk_usage[i].address] = { .mr = memblk_usage[i], .tag = "unknown" };
                    break;
            }
            memblk_usage[i].valid = false;
        }
    }

    klog("memblk: current regions:\n");
    for(const auto &[mr, mst] : memblk_tags)
    {
        klog("%08x-%08x: %s\n", mst.mr.address, mst.mr.address + mst.mr.length, mst.tag.c_str());
    }
    klog("memblk: end of current region dump\n");
}

#endif


extern "C" INTFLASH_FUNCTION int memblk_init_flash_opt_bytes()
{
    // aim for max DTCM, ITCM and no ECC
    auto opw2_mask = FLASH_OBW2SR_DTCM_AXI_SHARE_Msk |
        FLASH_OBW2SR_ITCM_AXI_SHARE_Msk |
        FLASH_OBW2SR_ECC_ON_SRAM_Msk;
    auto opw2_settings = (0U << FLASH_OBW2SRP_ECC_ON_SRAM_Pos) |
        (2U << FLASH_OBW2SRP_DTCM_AXI_SHARE_Pos) |
        (2U << FLASH_OBW2SRP_ITCM_AXI_SHARE_Pos);

    // enable xspi HSLV
    auto opw1_mask = FLASH_OBW1SR_XSPI1_HSLV_Msk |
        FLASH_OBW1SR_XSPI2_HSLV_Msk;
    auto opw1_settings = FLASH_OBW1SR_XSPI1_HSLV |
        FLASH_OBW1SR_XSPI2_HSLV;

    if(((FLASH->OBW2SR & opw2_mask) != opw2_settings) ||
        ((FLASH->OBW1SR & opw1_mask) != opw1_settings))
    {
        auto orig_val = FLASH->OBW2SR;
        auto new_val = (orig_val & ~opw2_mask) | opw2_settings;

        auto orig_val1 = FLASH->OBW1SR;
        auto new_val1 = (orig_val1 & ~opw1_mask) | opw1_settings;

        // program option bytes

        // unlock OPTCR (5.5.1)
        FLASH->OPTKEYR = 0x08192a3b;
        __DMB();
        FLASH->OPTKEYR = 0x4c5d6e7f;
        __DMB();

        // enable write operations (5.4.3)
        FLASH->OPTCR |= FLASH_OPTCR_PG_OPT;

        // program new value
        if(orig_val != new_val)
            FLASH->OBW2SRP = new_val;
        if(orig_val1 != new_val1)
            FLASH->OBW1SRP = new_val1;

        // wait completion
        while(FLASH->SR & FLASH_SR_QW);

        // relock
        FLASH->OPTCR |= FLASH_OPTCR_OPTLOCK;
    }

    return 0;
}