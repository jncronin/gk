#include "vmem.h"
#include "osspinlock.h"
#include "logger.h"
#include "pmem.h"
#include "vblock.h"
#include "scheduler.h"
#include "process.h"

static Spinlock sl_uh;

int vmem_map(const VMemBlock &vaddr, const PMemBlock &paddr, uintptr_t ttbr0, uintptr_t ttbr1)
{
    uint64_t ptr = 0;

    if(!vaddr.valid)
        return -1;
    
    while(ptr < vaddr.data_length())
    {
        uintptr_t cur_page_paddr;
        if(paddr.valid)
        {
            if((ptr + VBLOCK_64k) <= paddr.length)
            {
                cur_page_paddr = paddr.base + ptr;
            }
            else
            {
                klog("vmem_map: provided paddr not big enough for vaddr\n");
                return -1;
            }
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
        auto ret = vmem_map(cur_page_vaddr, cur_page_paddr, vaddr.user, vaddr.write, vaddr.exec, ttbr0, ttbr1,
            nullptr, vaddr.memory_type);
        if(ret != 0)
            return ret;

        ptr += VBLOCK_64k;
    }

    return 0;
}

static int vmem_map_int(uintptr_t vaddr, uintptr_t paddr, bool user, bool write, bool exec, uintptr_t ttbr, uintptr_t *paddr_out = nullptr,
    unsigned int memory_type = MT_NORMAL, bool is_global = false);
static int vmem_unmap_int(uintptr_t vaddr, uintptr_t len, uintptr_t ttbr, uintptr_t act_vaddr, bool release_page);
static uintptr_t vmem_vaddr_to_paddr_int(uintptr_t vaddr, uintptr_t ttbr);
static uint64_t vmem_get_pte_int(uintptr_t vaddr, uintptr_t ttbr);

int vmem_map(uintptr_t vaddr, uintptr_t paddr, bool user, bool write, bool exec,
    uintptr_t ttbr0, uintptr_t ttbr1, uintptr_t *paddr_out, unsigned int memory_type)
{
    uint64_t ttbr;

    if(vaddr >= UH_START)
    {
        if(ttbr1 == ~0ULL)
        {
            __asm__ volatile("mrs %[ttbr], ttbr1_el1\n" : [ttbr] "=r" (ttbr) : : "memory");
        }
        else
        {
            ttbr = ttbr1;
        }
        vaddr -= UH_START;

        {
            CriticalGuard cg(sl_uh);
            return vmem_map_int(vaddr, paddr, user, write, exec, ttbr, paddr_out, memory_type, true);
        }
    }
    else
    {
        if(ttbr0 == ~0ULL)
        {
            klog("lower half address but ttbr0 not provided\n");
            return -1;
        }
        else
        {
            ttbr = ttbr0;
        }

        // no lock here - already done in calling function
        return vmem_map_int(vaddr, paddr, user, write, exec, ttbr, paddr_out, memory_type, false);
    }
}

static int vmem_map_int(uintptr_t vaddr, uintptr_t paddr, bool user, bool write, bool exec, uintptr_t ttbr,
    uintptr_t *paddr_out, unsigned int memory_type, bool is_global)
{
    auto l2_addr = (vaddr >> 29) & 0x1fffULL;
    auto l3_addr = (vaddr >> 16) & 0x1fffULL;

    auto pd = (volatile uint64_t *)PMEM_TO_VMEM(ttbr & 0xffffffffffffULL);
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

    uint64_t attr = PAGE_ACCESS | PAGE_INNER_SHAREABLE | DT_PAGE | PAGE_ATTR((uint64_t)memory_type);
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

    if(!is_global)
        attr |= PAGE_NG;

    pt[l3_addr] = (paddr & ~0xffffULL) | attr;

    if(paddr_out)
        *paddr_out = paddr;

//    klog("vmem: map vaddr %llx to paddr %llx %s%s%s%s\n",
//        vaddr, paddr, user ? "U" : " ", write ? "W" : " ", exec ? "X" : " ", is_global ? "G" : " ");

    return 0;
}

int vmem_unmap(const VMemBlock &_vaddr, uintptr_t ttbr0, uintptr_t ttbr1, bool release_page)
{
    uint64_t ttbr;
    auto vaddr = _vaddr.base;

    if(vaddr >= UH_START)
    {
        if(ttbr1 == ~0ULL)
        {
            __asm__ volatile("mrs %[ttbr], ttbr1_el1\n" : [ttbr] "=r" (ttbr) : : "memory");
        }
        else
        {
            ttbr = ttbr1;
        }
        vaddr -= UH_START;

        {
            CriticalGuard cg(sl_uh);
            return vmem_unmap_int(vaddr, _vaddr.length, ttbr, vaddr + UH_START, release_page);
        }
    }
    else
    {
        if(ttbr0 == ~0ULL)
        {
            klog("lower half address but ttbr0 not provided\n");
            return -1;
        }
        else
        {
            ttbr = ttbr0;
        }

        // no lock here - already done in calling function
        return vmem_unmap_int(vaddr, _vaddr.length, ttbr, vaddr, release_page);
    }
}

uint64_t vmem_get_pte(uintptr_t vaddr, uintptr_t ttbr0, uintptr_t ttbr1)
{
    if(vaddr >= UH_START)
    {
        // higher half
        vaddr = vaddr - UH_START;

        if(ttbr1 == ~0ULL)
            __asm__ volatile("mrs %[ttbr], ttbr1_el1\n" : [ttbr] "=r" (ttbr1) : : "memory");
        ttbr1 &= 0xffffffffffffULL;

        CriticalGuard cg(sl_uh);
        auto paddr = vmem_get_pte_int(vaddr, ttbr1);

        return paddr;
    }
    else if(vaddr >= LH_END)
    {
        return 0;
    }
    else
    {
        // lower half

        if(ttbr0 == ~0ULL)
        {
            auto p = GetCurrentProcessForCore();
            if(p == nullptr)
                return 0;
            if(p->user_mem == nullptr)
                return 0;
            MutexGuard mg(p->user_mem->m);
            ttbr0 = p->user_mem->ttbr0 & 0xffffffffffffULL;
            auto paddr = vmem_get_pte_int(vaddr, ttbr0);

            return paddr;
        }
        else
        {
            auto paddr = vmem_get_pte_int(vaddr, ttbr0 & 0xffffffffffffULL);
            return paddr;
        }
    }
}

uintptr_t vmem_vaddr_to_paddr(uintptr_t vaddr, uintptr_t ttbr0, uintptr_t ttbr1)
{
    if(vaddr >= UH_START)
    {
        // higher half
        vaddr = vaddr - UH_START;

        if(ttbr1 == ~0ULL)
            __asm__ volatile("mrs %[ttbr], ttbr1_el1\n" : [ttbr] "=r" (ttbr1) : : "memory");
        ttbr1 &= 0xffffffffffffULL;

        CriticalGuard cg(sl_uh);
        auto paddr = vmem_vaddr_to_paddr_int(vaddr, ttbr1);

        return paddr;
    }
    else if(vaddr >= LH_END)
    {
        return 0;
    }
    else
    {
        // lower half

        if(ttbr0 == ~0ULL)
        {
            auto p = GetCurrentProcessForCore();
            if(p == nullptr)
                return 0;
            if(p->user_mem == nullptr)
                return 0;
            MutexGuard mg(p->user_mem->m);
            ttbr0 = p->user_mem->ttbr0 & 0xffffffffffffULL;
            auto paddr = vmem_vaddr_to_paddr_int(vaddr, ttbr0);

            return paddr;
        }
        else
        {
            auto paddr = vmem_vaddr_to_paddr_int(vaddr, ttbr0 & 0xffffffffffffULL);
            return paddr;
        }
    }
}

int vmem_unmap_int(uintptr_t vaddr, uintptr_t len, uintptr_t ttbr, uintptr_t act_vaddr, bool release_page)
{
    auto end = vaddr + len;
    vaddr &= ~(VBLOCK_64k - 1);
    end = (end + (VBLOCK_64k - 1)) & ~(VBLOCK_64k - 1);

    act_vaddr &= ~(VBLOCK_64k - 1);

    auto vaddr_adjust = act_vaddr - vaddr;

    while(vaddr < end)
    {
        // don't handle large pages here
        auto l2_addr = (vaddr >> 29) & 0x1fffULL;
        auto l3_addr = (vaddr >> 16) & 0x1fffULL;

        auto pd = (volatile uint64_t *)PMEM_TO_VMEM(ttbr & PAGE_PADDR_MASK);
        volatile uint64_t *pt;
        if((pd[l2_addr] & DT_PT) == DT_PT)
        {
            auto pt_paddr = pd[l2_addr] & 0xffffffff0000ULL;
            pt = (volatile uint64_t *)PMEM_TO_VMEM(pt_paddr);

            if((pt[l3_addr] & DT_PAGE) == DT_PAGE)
            {
                auto page = pt[l3_addr] & 0xffffffff0000ULL;

                pt[l3_addr] = 0;

                auto act_vpage = vaddr + vaddr_adjust;

#if DEBUG_VMEM
                klog("vmem_unmap: unmap page vaddr %llx paddr %llx\n", act_vpage, page);
#endif

                vmem_invlpg(act_vpage, ttbr);

                if(release_page)
                {
                    PMemBlock pb;
                    pb.base = page;
                    pb.length = VBLOCK_64k;
                    pb.valid = true;
                    Pmem.release(pb);
                }
            }
        }

        vaddr += VBLOCK_64k;
    }

    return 0;
}

void vmem_invlpg(uintptr_t vaddr, uintptr_t ttbr)
{
    // All ttbr1 pages are marked as global, vae1s ignores the asid here and instead acts like vaae1s
    __asm__ volatile(
        "dsb ishst\n"
        "tlbi vae1is, %[addr_enc]\n"
        "dsb ish\n"
        "isb\n"
        : :
        [addr_enc] "r" ((vaddr >> 12) | (ttbr & 0xffff000000000000ULL))
        : "memory"
    );
}

uintptr_t vmem_vaddr_to_paddr_int(uintptr_t vaddr, uintptr_t ttbr)
{
    auto pd = (volatile uint64_t *)PMEM_TO_VMEM(ttbr);

    auto l2_addr = (vaddr >> 29) & 0x1fffULL;
    auto pd_ent = pd[l2_addr];

#if DEBUG_VMEM
    klog("vtp: pd: %llx, l2_addr: %llx, pd_ent: %llx\n", (uintptr_t)pd, l2_addr, pd_ent);
#endif

    if((pd_ent & 0x3) == 0x3)
    {
        // its a table
        auto pt_paddr = pd_ent & 0xffffffff0000ULL;
        auto pt = (volatile uint64_t *)PMEM_TO_VMEM(pt_paddr);

        auto l3_addr = (vaddr >> 16) & 0x1fffULL;
        auto pt_ent = pt[l3_addr];

#if DEBUG_VMEM
        klog("vtp: pf: %llx, l3_addr: %llx, pt_ent: %llx\n", (uintptr_t)pt, l3_addr, pt_ent);
#endif

        if((pt_ent & 0x3) == 0x3)
        {
            // its a valid page
            auto page_addr = pt_ent & 0xffffffff0000ULL;
            auto page_offset = vaddr & 0xffffULL;
            return page_addr | page_offset;
        }
        else
        {
            return 0;
        }
    }
    else if((pd_ent & 0x3) == 0x1)
    {
        // its a block
        auto block_addr = pd_ent & 0xffffe0000000ULL;
        auto block_offset = vaddr & 0x1fffffffULL;
        return block_addr | block_offset;
    }
    else
    {
        return 0;
    }
}

uint64_t vmem_get_pte_int(uintptr_t vaddr, uintptr_t ttbr)
{
    auto pd = (volatile uint64_t *)PMEM_TO_VMEM(ttbr);

    auto l2_addr = (vaddr >> 29) & 0x1fffULL;
    auto pd_ent = pd[l2_addr];

#if DEBUG_VMEM
    klog("vtp: pd: %llx, l2_addr: %llx, pd_ent: %llx\n", (uintptr_t)pd, l2_addr, pd_ent);
#endif

    if((pd_ent & 0x3) == 0x3)
    {
        // its a table
        auto pt_paddr = pd_ent & 0xffffffff0000ULL;
        auto pt = (volatile uint64_t *)PMEM_TO_VMEM(pt_paddr);

        auto l3_addr = (vaddr >> 16) & 0x1fffULL;
        auto pt_ent = pt[l3_addr];
        return pt_ent;
    }
    else if((pd_ent & 0x3) == 0x1)
    {
        // its a block
        return 0;
    }
    else
    {
        return 0;
    }
}
