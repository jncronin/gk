#include "vmem.h"
#include "osspinlock.h"
#include "logger.h"
#include "pmem.h"
#include "vblock.h"

static Spinlock sl_uh;

int vmem_map(const VMemBlock &vaddr, const PMemBlock &paddr)
{
    uint64_t ptr = 0;

    if(!vaddr.valid)
        return -1;
    
    while(ptr < vaddr.data_length())
    {
        uintptr_t cur_page_paddr;
        if(paddr.valid && (ptr + VBLOCK_64k) < paddr.length)
        {
            cur_page_paddr = paddr.base + ptr;
        }
        else
        {
            auto new_block = Pmem.acquire(VBLOCK_64k);
            if(!new_block.valid)
            {
                klog("OOM\n");
                return -1;
            }
            cur_page_paddr = new_block.base;
        }

        uintptr_t cur_page_vaddr = vaddr.data_start() + ptr;
        auto ret = vmem_map(cur_page_vaddr, cur_page_paddr, vaddr.user, vaddr.write, vaddr.exec);
        if(ret != 0)
            return ret;

        ptr += VBLOCK_64k;
    }

    return 0;
}

int vmem_map(uintptr_t vaddr, uintptr_t paddr, bool user, bool write, bool exec)
{
    Spinlock *ttbr_sl;
    uint64_t ttbr;

    if(vaddr >= UH_START)
    {
        ttbr_sl = &sl_uh;
    }
    else
    {
        klog("lower half mapping not implemented\n");
        while(true);
    }

    CriticalGuard cg(*ttbr_sl);

    if(vaddr >= UH_START)
    {
        __asm__ volatile("mrs %[ttbr], ttbr1_el1\n" : [ttbr] "=r" (ttbr) : : "memory");
        vaddr -= UH_START;
    }
    else
    {
        klog("lower half mapping not implemented\n");
        while(true);
    }

    auto l2_addr = (vaddr >> 29) & 0x1fffULL;
    auto l3_addr = (vaddr >> 16) & 0x1fffULL;

    auto pd = (volatile uint64_t *)PMEM_TO_VMEM(ttbr);
    volatile uint64_t *pt;
    if((pd[l2_addr] & 0x1) == 0)
    {
        // need to map pt

        auto pt_be = Pmem.acquire(VBLOCK_64k);
        if(!pt_be.valid)
        {
            klog("OOM\n");
            return -1;
        }

        auto pt_paddr = pt_be.base;
        pt = (volatile uint64_t *)PMEM_TO_VMEM(pt_paddr);

        quick_clear_64((void *)pt);

        pd[l2_addr] = pt_paddr |
            DT_PT |
            PAGE_ACCESS;
    }
    else
    {
        auto pt_paddr = pd[l2_addr] & 0xffffffff0000ULL;
        pt = (volatile uint64_t *)PMEM_TO_VMEM(pt_paddr);
    }

    if(pt[l3_addr] & 0x1)
    {
        klog("vmem: trying to map already mapped page at %llx\n", vaddr);
        return -1;
    }

    if(!paddr)
    {
        // need to allocate page
        auto paddr_be = Pmem.acquire(VBLOCK_64k);
        if(!paddr_be.valid)
        {
            klog("OOM\n");
            return -1;
        }
        paddr = paddr_be.base;
    }

    uint64_t attr = PAGE_ACCESS | PAGE_INNER_SHAREABLE | DT_PAGE | PAGE_ATTR(MT_NORMAL);
    if(user)
    {
        if(write)
            attr |= PAGE_USER_RW;
        else
            attr |= PAGE_USER_RO;
    }
    else
    {
        if(write)
            attr |= PAGE_PRIV_RW;
        else
            attr |= PAGE_PRIV_RO;
    }
    if(!exec)
        attr |= PAGE_XN;

    if(vaddr < UH_START)
        attr |= PAGE_NG;

    pt[l3_addr] = (paddr & ~0xffffULL) | attr;

    return 0;
}
