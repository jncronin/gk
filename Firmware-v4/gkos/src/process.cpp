#include "process.h"
#include "pmem.h"
#include "vmem.h"
#include "thread.h"

Process::Process(const std::string &_name, bool _is_privileged, PProcess parent)
{
    name = _name;
    is_privileged = _is_privileged;

    // don't allow unprivileged processes to create privileged ones
    if(parent && parent->is_privileged == false)
        is_privileged = false;

    if(!is_privileged)
    {
        // generate a user space paging setup
        auto ttbr0_reg = Pmem.acquire(VBLOCK_64k);
        if(ttbr0_reg.valid == false)
        {
            klog("Process: could not allocate ttbr0\n");
            while(true);
        }
        quick_clear_64((void *)PMEM_TO_VMEM(ttbr0_reg.base));

        {
            CriticalGuard cg(owned_pages.sl);
            owned_pages.add(ttbr0_reg);
        }

        user_mem = std::make_unique<userspace_mem_t>();
        {
            CriticalGuard cg(user_mem->sl);
            user_mem->ttbr0 = ttbr0_reg.base;
            user_mem->blocks.init(0);

            // allocate the first page to catch null pointer references - not actually used except
            //  to prevent other regions being allocated there - the main logic is in TranslationFault_Handler
            vblock_alloc_fixed(VBLOCK_64k, 0, false, false, false, 0, 0, user_mem->blocks);
        }
    }

    // inherit fds + environ
    if(parent)
    {
        {
            CriticalGuard cg(open_files.sl, parent->open_files.sl);
            open_files.f = parent->open_files.f;
        }

        {
            CriticalGuard cg(env.sl, parent->env.sl);
            env.envs = parent->env.envs;
        }
    }
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
