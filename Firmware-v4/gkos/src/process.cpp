#include "process.h"
#include "pmem.h"
#include "vmem.h"
#include "thread.h"
#include "screen.h"
#include "ipi.h"
#include "process_interface.h"

PProcess Process::Create(const std::string &_name, bool _is_privileged, PProcess parent)
{
    auto ret = ProcessList.Create();

    ret->name = _name;
    ret->is_privileged = _is_privileged;

    // don't allow unprivileged processes to create privileged ones
    if(parent && parent->is_privileged == false)
        ret->is_privileged = false;

    if(!ret->is_privileged)
    {
        // generate a user space paging setup
        auto ttbr0_reg = Pmem.acquire(VBLOCK_64k);
        if(ttbr0_reg.valid == false)
        {
            klog("Process: could not allocate ttbr0\n");
            while(true);
        }
        quick_clear_64((void *)PMEM_TO_VMEM(ttbr0_reg.base));
        ((volatile uint64_t *)PMEM_TO_VMEM(ttbr0_reg.base))[8191] = process_highest_pt.base |
            PAGE_ACCESS | DT_PT;

        {
            CriticalGuard cg(ret->owned_pages.sl);
            ret->owned_pages.add(ttbr0_reg);
        }

        ret->user_mem = std::make_unique<userspace_mem_t>();
        {
            CriticalGuard cg(ret->user_mem->sl);
            ret->user_mem->ttbr0 = ttbr0_reg.base | ((uint64_t)ret->id << 48);
            ret->user_mem->blocks.init(0, 8191);    // use last entry for fixed space e.g. frame buffers, clock_cur etc

            // allocate the first page to catch null pointer references - not actually used except
            //  to prevent other regions being allocated there - the main logic is in TranslationFault_Handler
            vblock_alloc_fixed(VBLOCK_64k, 0, false, false, false, 0, 0, ret->user_mem->blocks);
        }
    }

    // inherit fds + environ
    if(parent)
    {
        {
            CriticalGuard cg(ret->open_files.sl, parent->open_files.sl);
            ret->open_files.f = parent->open_files.f;
        }

        {
            CriticalGuard cg(ret->env.sl, parent->env.sl);
            ret->env.envs = parent->env.envs;
        }
    }

    return ret;
}

void Process::owned_pages_t::add(const PMemBlock &b)
{
    auto start = b.base & ~(VBLOCK_64k - 1ULL);
    auto end = (b.base + b.length + (VBLOCK_64k - 1ULL)) & ~(VBLOCK_64k - 1ULL);

    while(start < end)
    {
        auto val = (uint32_t)(start >> 16);
        if(b.is_shared)
            val |= 0x80000000UL;
        p.insert(val);
        start += VBLOCK_64k;
    }
}

void Process::Kill()
{
    /* For now, just make all threads zombies */

    CriticalGuard cg(sl);
    for(auto t : threads)
    {
        CriticalGuard cg2(t->sl);
        t->for_deletion = true;
        t->blocking.block_indefinite();
    }

    /* yield all cores */
    gic_send_sgi(GIC_SGI_YIELD, GIC_TARGET_ALL);
}

Process::~Process()
{
    klog("process: %s destructor called\n", name.c_str());
}
