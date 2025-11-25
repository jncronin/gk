#include "process.h"
#include "pmem.h"
#include "vmem.h"

Process::Process(const std::string &_name, bool _is_privileged)
{
    name = _name;
    is_privileged = _is_privileged;

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
}

void Process::owned_pages_t::add(const PMemBlock &b)
{
    auto start = b.base & ~(VBLOCK_64k - 1ULL);
    auto end = (b.base + b.length + (VBLOCK_64k - 1ULL)) & ~(VBLOCK_64k - 1ULL);

    while(start < end)
    {
        p.insert(start);
        start += VBLOCK_64k;
    }
}
