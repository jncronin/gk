#include "thread.h"
#include "memblk.h"
#include "mpuregions.h"

#include <cstring>

#include "scheduler.h"
#include "cache.h"
#include "gk_conf.h"
#include "clocks.h"
#include "ossharedmem.h"
#include "process.h"
#include "cleanup.h"

extern Thread dt;

Thread::Thread(Process &owning_process) : p(owning_process) {}

void Thread::Cleanup(void *tretval, bool from_cleanup)
{
    if(from_cleanup)
    {
        // clean up any tls data
        for(int i = 0; i < 4; i++)  // PTHREAD_DESTRUCTOR_ITERATIONS = 4 on glibc
        {
            bool has_nonnull = false;

            {
                CriticalGuard cg_p;
                for(auto iter = p.tls_data.begin(); iter != p.tls_data.end(); ++iter)
                {
                    auto k = iter->first;
                    auto d = iter->second;

                    auto t_iter = tls_data.find(k);
                    if(t_iter != tls_data.end())
                    {
                        if(t_iter->second)
                        {
                            // non-null
                            has_nonnull = true;

                            // run destructor
                            if(d)
                                d(t_iter->second);
                        }
                    }
                }
            }

            if(!has_nonnull)
                break;
        }
    }
    else
    {
        CriticalGuard cg;
        for_deletion = true;
        retval = tretval;

        // signal any thread waiting on a join
        if(join_thread)
        {
            if(join_thread_retval)
                *join_thread_retval = tretval;
            join_thread->ss_p.ival1 = 0;
            join_thread->ss.Signal();
            join_thread->blocking_on = nullptr;
            join_thread->is_blocking = false;
            join_thread->block_until = kernel_time();

            join_thread = nullptr;
        }

        // remove us from the process' thread list
        {
            CriticalGuard cg2;
            auto iter = p.threads.begin();
            while(iter != p.threads.end())
            {
                if(*iter == this)
                    iter = p.threads.erase(iter);
                else
                    iter++;
            }
        }

        CleanupQueue.Push({ .is_thread = true, .t = this });
    }
}

void thread_cleanup(void *tretval)   // both return value and first param are in R0, so valid
{
    auto t = GetCurrentThreadForCore();
    t->Cleanup(tretval, false);
    while(true)
    {
        Yield();
    }
}

Thread *Thread::Create(std::string name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            Process &owning_process,
            CPUAffinity affinity,
            MemRegion stackblk,
            void *p_r1)
{
    auto t = new Thread(owning_process);
    if(!t) return nullptr;
    
    memset(&t->tss, 0, sizeof(thread_saved_state));

    t->base_priority = priority;
    t->is_privileged = is_priv;
    t->name = name;

    t->tss.lr = 0xfffffffdUL;               // return to thread mode, normal frame, use PSP
    t->tss.control = is_priv ? 2UL : 3UL;   // bit0 = !privilege, bit1 = use PSP

    /* Create TLS, if any */
    if(owning_process.has_tls)
    {
        t->mr_tls = InvalidMemregion();
        if(affinity == CPUAffinity::M4Only || affinity == CPUAffinity::PreferM4)
            t->mr_tls = memblk_allocate(owning_process.tls_memsz + 8, MemRegionType::SRAM, "tls");
        if(!t->mr_tls.valid)
            t->mr_tls = memblk_allocate(owning_process.tls_memsz + 8, MemRegionType::AXISRAM, "tls");
        if(!t->mr_tls.valid)
        {
            t->mr_tls = memblk_allocate(owning_process.tls_memsz + 8, MemRegionType::SDRAM, "tls");
        }
        if(!t->mr_tls.valid)
        {
            return nullptr;
        }

        // initialize TLS segment
        {
            //SharedMemoryGuard smg_write((void *)t->mr_tls.address, owning_process.tls_memsz, false, true);
            //SharedMemoryGuard smg_read((void *)owning_process.tls_base, owning_process.tls_filsz, true, false);
            // ARM32 has TLS segments starting 8 bytes after tp.  We can use these for other things
            *(volatile uint32_t *)(t->mr_tls.address + 0) = (uint32_t)owning_process.pid;   // proc ID
            *(volatile uint32_t *)(t->mr_tls.address + 4) = (uint32_t)(uintptr_t)t;         // thread ID
            memcpy((void *)(t->mr_tls.address + 8), (void *)owning_process.tls_base, owning_process.tls_filsz);
            memset((void *)(t->mr_tls.address + 8 + owning_process.tls_filsz), 0,
                owning_process.tls_memsz - owning_process.tls_filsz);
        }
    }

    /* Create stack frame */
    if(stackblk.valid)
    {
        t->stack = stackblk;
    }
    else
    {
        t->stack = memblk_allocate_for_stack(4096U, affinity, name + " stack", owning_process.stack_preference);
    }
#if !GK_DUAL_CORE_AMP && !GK_DUAL_CORE
    // we keep PreferM4 as a separate option until now to allow better stack placement for low priority tasks
    affinity = CPUAffinity::M7Only;
#endif
    t->tss.affinity = affinity;

    if(!t->stack.valid)
        return nullptr;

    /* Basic stack frame layout is
        top of stack --
            xPSR
            Return address
            LR
            R12
            R3
            R2
            R1
            R0
        bottom of stack --
    */
    auto top_stack = t->stack.length / 4;
    if(t->stack.length > 128)
    {
        // add stack guards
        top_stack -= t->stack.length / 8 / 4;
    }
    auto stack = reinterpret_cast<uint32_t *>(t->stack.address);

    auto cleanup_func = is_priv ? reinterpret_cast<uint32_t>(thread_cleanup) : owning_process.thread_finalizer;
    
    {
        //SharedMemoryGuard sg((const void *)(t->stack.address + t->stack.length - 8*4),
        //    t->stack.is_cacheable() ? 8*4 : 0, false, true);
        stack[--top_stack] = 1UL << 24; // THUMB mode
        stack[--top_stack] = reinterpret_cast<uint32_t>(func) | 1UL;
        stack[--top_stack] = cleanup_func | 1UL;
        stack[--top_stack] = 0UL;
        stack[--top_stack] = 0UL;
        stack[--top_stack] = 0UL;
        stack[--top_stack] = reinterpret_cast<uint32_t>(p_r1);
        stack[--top_stack] = reinterpret_cast<uint32_t>(p);
    }

    //t->tss.psp = t->stack.address + t->stack.length;
    t->tss.psp = reinterpret_cast<uint32_t>(&stack[top_stack]);
    
    /* Create mpu regions */
    memcpy(t->tss.mpuss, owning_process.p_mpu, sizeof(mpu_default));
    if(owning_process.has_tls)
    {
        Process::mmap_region mmr_tls { .mr = t->mr_tls, .fd = -1, .is_read = true, .is_write = true, .is_exec = false, .is_sync = false };
        t->tss.mpuss[owning_process.p_mpu_tls_id] = mmr_tls.to_mpu(owning_process.p_mpu_tls_id);
    }
    owning_process.AddMPURegion({ .mr = t->stack, .fd = -1, .is_read = true, .is_write = true, .is_exec = false, .is_sync = false,
        .is_stack = true, .is_priv = is_priv });

    {
        CriticalGuard cg;
        owning_process.threads.push_back(t);
    }
    
    owning_process.UpdateMPURegionsForThreads();

    return t;
}

Thread *GetCurrentThreadForCore(int coreid)
{
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
    if(coreid == -1)
    {
        coreid = GetCoreID();
    }
#else
    return sched.current_thread[0];
#endif

#if GK_DUAL_CORE
    {
        //CriticalGuard cg(s.current_thread[coreid].m);
        return sched.current_thread[coreid].v;  // <- should be atomic
    }
#elif GK_DUAL_CORE_AMP
    return scheds[coreid].current_thread[0].v;
#endif
}

Thread *GetNextThreadForCore(int coreid)
{
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
    if(coreid == -1)
    {
        coreid = GetCoreID();
    }
#else
    return sched.GetNextThread(0);
#endif

#if GK_DUAL_CORE
    return sched.GetNextThread(coreid);
#elif GK_DUAL_CORE_AMP
    return scheds[coreid].GetNextThread(coreid);
#endif
}

int GetCoreID()
{
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
    return (*( ( volatile uint32_t * ) 0xE000ed00 ) & 0xfff0UL) == 0xc240 ? 1 : 0;
#else
    return 0;
#endif
}

void SetNextThreadForCore(Thread *t, int coreid)
{
    [[maybe_unused]] bool flush_cache = false;

#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
    if(coreid == -1)
    {
        coreid = GetCoreID();
    }
#else
    coreid = 0;
#endif

    {
        CriticalGuard cg;

        if(current_thread(coreid))
        {
            CriticalGuard cg2;
            current_thread(coreid)->tss.chosen_for_core = 0;
            current_thread(coreid)->tss.running_on_core = 0;
            current_thread(coreid)->total_us_time += clock_cur_us() -
                current_thread(coreid)->cur_timeslice_start;
            if(current_thread(coreid) == &dt)
            {
                flush_cache = true;
            }
        }

        {
            CriticalGuard cg2;
            t->tss.chosen_for_core = 0;
            t->tss.running_on_core = coreid + 1;
            t->cur_timeslice_start = clock_cur_us();

            if(t->is_blocking)
            {
                while(true);
            }
        }

        current_thread(coreid) = t;

        extern uint32_t _tls_pointers[];
        _tls_pointers[coreid] = t->mr_tls.address;
    }
}

std::map<uint32_t, Process::mmap_region>::iterator Process::get_mmap_region(uint32_t addr, uint32_t len)
{
    // lookup on addr but also has to match length (we can't delete bits of blocks)
    auto iter = mmap_regions.find(addr);
    if(iter != mmap_regions.end() && iter->second.mr.length == len)
    {
        return iter;
    }
    return mmap_regions.end();
}
