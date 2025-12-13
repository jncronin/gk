#include "process_interface.h"
#include "_gk_memaddrs.h"
#include "pmem.h"
#include "vblock.h"
#include "logger.h"
#include "vmem.h"
#include "screen.h"
#include "stm32mp2xx.h"

static_assert(GK_PROCESS_INTERFACE_START == 0x20000000ULL * 8191);
PMemBlock process_highest_pt;
PMemBlock process_kernel_info_page;

/* Set up a page table that will be mapped into the highest part of each process lower-half
    memory space.

    All entries are global.
    
    This provides access to:
        Screen buffers (write-through)
        TIM3 (read-only, device)
        A RO page looking at the second half of VDERAM (0xe0b0000) where:
            secure monitor updates _cur_s
            kernel places toffset for current wall clock time calculations
            kernel/MPU (TBD) places joystick and tilt information
        Audio buffers? TODO */

static const constexpr uint64_t scr_buf_vaddrs[] = { GK_SCR_L1_B0, GK_SCR_L1_B1, GK_SCR_L1_B2 };

static int process_interface_map(volatile uint64_t *pt,
    uintptr_t vaddr, uintptr_t paddr, size_t len,
    uint64_t attrs);

void init_process_interface()
{
    process_highest_pt = Pmem.acquire(VBLOCK_64k);
    if(process_highest_pt.valid == false)
    {
        klog("process_interface: couldn't allocate pt\n");
        while(true);
    }

    auto pt = (volatile uint64_t *)PMEM_TO_VMEM(process_highest_pt.base);

    for(unsigned int buf = 0; buf < sizeof(scr_buf_vaddrs) / sizeof(scr_buf_vaddrs[0]); buf++)
    {
        auto buf_vaddr = scr_buf_vaddrs[buf];
        auto buf_paddr = screen_get_buf(0, buf);
        if(buf_paddr.valid)
        {
            process_interface_map(pt, buf_vaddr, buf_paddr.base, buf_paddr.length,
                PAGE_INNER_SHAREABLE | PAGE_ATTR(MT_NORMAL_WT) | PAGE_USER_RW | PAGE_XN);
        }
    }

    process_interface_map(pt, GK_TIM3, TIM3_BASE_NS, VBLOCK_64k,
        PAGE_INNER_SHAREABLE | PAGE_ATTR(MT_DEVICE) | PAGE_USER_RO | PAGE_XN);

    // ROINFO page is device memory as it can potentially be written to by M33
    process_interface_map(pt, GK_ROINFO_PAGE, 0xe0b0000ULL, VBLOCK_64k,
        PAGE_INNER_SHAREABLE | PAGE_ATTR(MT_DEVICE) | PAGE_USER_RO | PAGE_XN);

    // The rest is pure CA35 stuff so is cached
    process_kernel_info_page = Pmem.acquire(VBLOCK_64k);
    if(!process_kernel_info_page.valid)
    {
        klog("process_interface: couldn't allocate kernel_info_page\n");
        while(true);
    }

    auto kinfo = (gk_kernel_info *)PMEM_TO_VMEM(process_kernel_info_page.base);
    kinfo->max_screen_width = GK_SCREEN_WIDTH;
    kinfo->max_screen_height = GK_SCREEN_HEIGHT;
    kinfo->ncores = GK_NUM_CORES;
    kinfo->page_size = VBLOCK_64k;
    kinfo->gk_ver = 0x0400;

    process_interface_map(pt, GK_KERNEL_INFO_PAGE, process_kernel_info_page.base, VBLOCK_64k,
        PAGE_INNER_SHAREABLE | PAGE_ATTR(MT_NORMAL) | PAGE_USER_RO | PAGE_XN);
}

int process_interface_map(volatile uint64_t *pt,
    uintptr_t buf_vaddr, uintptr_t buf_paddr, size_t len,
    uint64_t attrs)
{
    uint64_t offset = 0;
    while(offset < len)
    {
        auto vaddr = buf_vaddr + offset;
        auto paddr = buf_paddr + offset;

        auto page_idx = (vaddr - GK_PROCESS_INTERFACE_START) / VBLOCK_64k;
        if(page_idx >= 8192ULL)
        {
            klog("process_interface: error: page_idx out of range: %llu\n", page_idx);
            while(true);
        }
        pt[page_idx] = paddr | PAGE_ACCESS | DT_PAGE | attrs;

        offset += VBLOCK_64k;
    }

    return 0;
}
