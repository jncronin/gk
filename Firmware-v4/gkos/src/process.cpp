#include "process.h"
#include "pmem.h"
#include "vmem.h"
#include "thread.h"
#include "screen.h"

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

        {
            CriticalGuard cg(ret->owned_pages.sl);
            ret->owned_pages.add(ttbr0_reg);
        }

        ret->user_mem = std::make_unique<userspace_mem_t>();
        {
            CriticalGuard cg(ret->user_mem->sl);
            ret->user_mem->ttbr0 = ttbr0_reg.base | ((uint64_t)ret->id << 48);
            ret->user_mem->blocks.init(0);

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

void Process::_init_screen()
{
    // screen setup
    if(screen.bufs[0].valid)
        return;
    
    screen.screen_layer = is_privileged ? 1 : 0;

    for(unsigned int buf = 0; buf < 3; buf++)
    {
        screen.bufs[buf] = vblock_alloc(vblock_size_for(scr_layer_size_bytes),
            !is_privileged, true,
            false, 0, 0, is_privileged ? vblock : user_mem->blocks);
        screen.bufs[buf].memory_type = MT_NORMAL_WT;
        screen_map_for_process(screen.bufs[buf], screen.screen_layer, buf,
            is_privileged ? 0U : user_mem->ttbr0);
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
