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

struct mempool
{
    uintptr_t base_addr;
    uintptr_t length;
    uintptr_t cur_end;
};

static mempool mp_itcm, mp_dtcm, mp_axisram, mp_axisram4, mp_sram, mp_sdram;

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
extern int _ecm7_stack_align;

// sram
extern int _esram_bss;
extern int _esramahb;

// sdram
extern int _esdram;
extern int _elwip_data;

// itcm
extern int _eitcm;

template<typename T> constexpr static T align_up(T val, T align)
{
    if(val % align)
    {
        val -= val % align;
        val += align;
    }
    return val;
}

static void add_memory_region(const mempool *mp, MemRegionType type)
{
    if(mp->cur_end >= mp->base_addr + mp->length)
        return;
    
    BuddyEntry be;
    be.base = mp->cur_end;
    
    switch(type)
    {
        case MemRegionType::AXISRAM:
            be.base = align_up((uint32_t)mp->cur_end, b_axisram.MinBuddySize());
            be.length = mp->base_addr + mp->length - be.base;
            be.valid = is_power_of_2(be.length) && is_multiple_of(be.length, b_axisram.MinBuddySize()) && ((be.base - b_axisram.Base()) % be.length == 0);
            b_axisram.release(be);
            break;
        case MemRegionType::DTCM:
            be.base = align_up((uint32_t)mp->cur_end, b_dtcm.MinBuddySize());
            be.length = mp->base_addr + mp->length - be.base;
            be.valid = is_power_of_2(be.length) && is_multiple_of(be.length, b_dtcm.MinBuddySize()) && ((be.base - b_dtcm.Base()) % be.length == 0);
            b_dtcm.release(be);
            break;
        case MemRegionType::SDRAM:
            be.base = align_up((uint32_t)mp->cur_end, b_sdram.MinBuddySize());
            be.length = mp->base_addr + mp->length - be.base;
            be.valid = is_power_of_2(be.length) && is_multiple_of(be.length, b_sdram.MinBuddySize()) && ((be.base - b_sdram.Base()) % be.length == 0);
            b_sdram.release(be);
            break;
        case MemRegionType::SRAM:
            be.base = align_up((uint32_t)mp->cur_end, b_sram.MinBuddySize());
            be.length = mp->base_addr + mp->length - be.base;
            be.valid = is_power_of_2(be.length) && is_multiple_of(be.length, b_sram.MinBuddySize()) && ((be.base - b_sram.Base()) % be.length == 0);
            b_sram.release(be);
            break;
        case MemRegionType::ITCM:
            be.base = align_up((uint32_t)mp->cur_end, b_itcm.MinBuddySize());
            be.length = mp->base_addr + mp->length - be.base;
            be.valid = is_power_of_2(be.length) && is_multiple_of(be.length, b_itcm.MinBuddySize()) && ((be.base - b_itcm.Base()) % be.length == 0);
            b_itcm.release(be);
            break;
    }
}

bool MemRegion::is_cacheable() const
{
    if(rt == MemRegionType::DTCM)
        return false;
    if(rt == MemRegionType::SRAM)
        return false;
    return true;
}

static void incr_eptr(const void *addr, mempool *mp)
{
    auto p = (uintptr_t)addr;
    if(p >= mp->base_addr && p < (mp->base_addr + mp->length))
    {
        if(p >= mp->cur_end)
            mp->cur_end = p;
    }
}


static void incr_eptr(const void *addr)
{
    incr_eptr(addr, &mp_itcm);
    incr_eptr(addr, &mp_dtcm);
    incr_eptr(addr, &mp_axisram);
    incr_eptr(addr, &mp_axisram4);
    incr_eptr(addr, &mp_sram);
    incr_eptr(addr, &mp_sdram);
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

    mp_itcm.base_addr = 0;
    mp_itcm.length = itcm_size;
    mp_itcm.cur_end = 0;

    mp_dtcm.base_addr = 0x20000000;
    mp_dtcm.length = dtcm_size;
    mp_dtcm.cur_end = mp_dtcm.base_addr;

    mp_axisram.base_addr = axisram_base;
    mp_axisram.length = axisram_size;
    mp_axisram.cur_end = mp_axisram.base_addr;

    mp_axisram4.base_addr = 0x24060000;
    mp_axisram4.length = (obw2sr & FLASH_OBW2SR_ECC_ON_SRAM) ? 0 : 0x12000;
    mp_axisram4.cur_end = mp_axisram4.base_addr;

    mp_sram.base_addr = 0x30000000;
    mp_sram.length = 0x8000;
    mp_sram.cur_end = mp_sram.base_addr;

    mp_sdram.base_addr = GK_SDRAM_BASE;
    mp_sdram.length = GK_SDRAM_SIZE;
    mp_sdram.cur_end = 0;

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


    // handle ends of axisram data in linker script - there may be a split at
    //  the beginning of sram4 so be careful

    incr_eptr(&_edata);
    incr_eptr(&_ebss);
    incr_eptr(&_elwip_init_data);
    incr_eptr(&_edata4);

    incr_eptr(&_edtcm_bss);
    incr_eptr(&_edtcm2);
    incr_eptr(&_ecm7_stack_align);

    incr_eptr(&_esram_bss);
    incr_eptr(&_esramahb);
    incr_eptr(&_ertt);

    incr_eptr(&_esdram);
    incr_eptr(&_elwip_data);

    incr_eptr(&_eitcm);

    add_memory_region(&mp_itcm, MemRegionType::ITCM);
    add_memory_region(&mp_dtcm, MemRegionType::DTCM);
    add_memory_region(&mp_axisram, MemRegionType::AXISRAM);
    add_memory_region(&mp_axisram4, MemRegionType::AXISRAM);
    add_memory_region(&mp_sram, MemRegionType::SRAM);
    add_memory_region(&mp_sdram, MemRegionType::SDRAM);

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

    if(r.address == 0x60000000) BKPT_IF_DEBUGGER();
    
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
    unsigned int opw2_settings = ((GK_MEM_ECC_SHARE) << FLASH_OBW2SRP_ECC_ON_SRAM_Pos) |
        ((GK_MEM_DTCM_SHARE) << FLASH_OBW2SRP_DTCM_AXI_SHARE_Pos) |
        ((GK_MEM_ITCM_SHARE) << FLASH_OBW2SRP_ITCM_AXI_SHARE_Pos);

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