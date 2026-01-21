#include "process.h"
#include "pmem.h"
#include "vmem.h"
#include "thread.h"
#include "screen.h"
#include "ipi.h"
#include "cleanup.h"
#include "process_interface.h"
#include "completion_list.h"
#include "_gk_memaddrs.h"
#include <atomic>

static std::atomic<pid_t> focus_process = 0;

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
            // use last entry for fixed space e.g. frame buffers, clock_cur etc
            // use penultimate entry for heap
            // use one before for 128 stacks
            ret->user_mem->blocks.init(0, 8189);    

            // allocate the first page to catch null pointer references - not actually used except
            //  to prevent other regions being allocated there - the main logic is in TranslationFault_Handler
            vblock_alloc_fixed(VBLOCK_64k, 0, false, false, false, 0, 0, ret->user_mem->blocks);
        }

        {
            // directly allocate the heap somewhere high so it doesn't interefere with userspace mmap requests
            CriticalGuard cg(ret->heap.sl);
            VMemBlock vb;
            vb.base = (uint64_t)GK_HEAP_START;
            vb.length = VBLOCK_512M;
            vb.valid = true;
            vb.user = true;
            vb.write = true;
            vb.exec = false;
            vb.lower_guard = 0;
            vb.upper_guard = 0;
            vb.memory_type = MT_NORMAL;
            ret->heap.vb_heap = vb;
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

        ret->ppid = parent->id;
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

void Process::Kill(int rc)
{
    CriticalGuard cg(sl, ProcessExitCodes.sl);
    for(auto t : threads)
    {
        CriticalGuard cg2(t->sl);
        t->blocking.block_indefinite();
        CleanupQueue.Push({ .is_thread = true, .t = t });
    }

    ProcessExitCodes._set(id, rc);

    CleanupQueue.Push({ .is_thread = false, .p = ProcessList.Get(id) });

    /* yield all cores */
    gic_send_sgi(GIC_SGI_YIELD, GIC_TARGET_ALL);

}

Process::~Process()
{
    klog("process: %s destructor called\n", name.c_str());
}

int SetFocusProcess(PProcess p)
{
    focus_process = p->id;

    // clear screen on process switch
    screen_clear_all_userspace();

    // TODO: enable/disable tilt if appropriate
    // restore palette if used

    return 0;
}

PProcess GetFocusProcess()
{
    return ProcessList.Get(focus_process);
}
