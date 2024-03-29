#include "thread.h"
#include "memblk.h"
#include "mpuregions.h"

#include <cstring>

#include "scheduler.h"
#include "cache.h"
#include "gk_conf.h"

extern Scheduler s;
extern Thread dt;

Thread::Thread(Process &owning_process) : p(owning_process) {}

void Thread::Cleanup(void *tretval)
{
    CriticalGuard cg(sl);
    for_deletion = true;
    retval = tretval;

    // signal any thread waiting on a join
    if(join_thread)
    {
        if(join_thread_retval)
            *join_thread_retval = tretval;
        join_thread->ss_p.ival1 = 0;
        join_thread->ss.Signal();
    }

    // clean up any tls data
    for(int i = 0; i < 4; i++)  // PTHREAD_DESTRUCTOR_ITERATIONS = 4 on glibc
    {
        bool has_nonnull = false;

        {
            CriticalGuard cg_p(p.sl);
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
                        d(t_iter->second);
                    }
                }
            }
        }

        if(!has_nonnull)
            break;
    }
}

static void thread_cleanup(void *tretval)   // both return value and first param are in R0, so valid
{
    auto t = GetCurrentThreadForCore();
    t->Cleanup(tretval);
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
            mpu_saved_state extra_permissions,
            mpu_saved_state extra_permissions2)
{
    SRAM4RegionAllocator<Thread> alloc;
    auto tloc = alloc.allocate(1);
    if(!tloc)
        return nullptr;
    auto t = new(tloc) Thread(owning_process);
    memset(&t->tss, 0, sizeof(thread_saved_state));

    t->affinity = affinity;
    t->base_priority = priority;
    t->is_privileged = is_priv;
    t->name = name;

    t->tss.lr = 0xfffffffdUL;               // return to thread mode, normal frame, use PSP
    t->tss.control = is_priv ? 2UL : 3UL;   // bit0 = !privilege, bit1 = use PSP

    /* Create stack frame */
    if(stackblk.valid)
    {
        t->stack = stackblk;
    }
    else
    {
        t->stack = memblk_allocate_for_stack(4096U, affinity);
    }
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
    auto stack = reinterpret_cast<uint32_t *>(t->stack.address);
    stack[--top_stack] = 1UL << 24; // THUMB mode
    stack[--top_stack] = reinterpret_cast<uint32_t>(func) | 1UL;
    stack[--top_stack] = reinterpret_cast<uint32_t>(thread_cleanup) | 1UL;
    stack[--top_stack] = 0UL;
    stack[--top_stack] = 0UL;
    stack[--top_stack] = 0UL;
    stack[--top_stack] = 0UL;
    stack[--top_stack] = reinterpret_cast<uint32_t>(p);

    //t->tss.psp = t->stack.address + t->stack.length;
    t->tss.psp = reinterpret_cast<uint32_t>(&stack[top_stack]);
    
    /* Create mpu regions */
    t->tss.cm7_mpu0 = mpu_msp_cm7;
    t->tss.cm4_mpu0 = mpu_msp_cm4;
    t->tss.mpuss[0] = mpu_flash;
    t->tss.mpuss[1] = mpu_periph;
    t->tss.mpuss[2] = MPUGenerate(t->stack.address,
        t->stack.length, 3, false,
        MemRegionAccess::RW, MemRegionAccess::RW,
        WBWA_NS);
    t->tss.mpuss[3] = mpu_sram4;
    t->tss.mpuss[4] = mpu_sdram;
    t->tss.mpuss[5] = extra_permissions;
    t->tss.mpuss[6] = extra_permissions2;

    CleanOrInvalidateM7Cache((uint32_t)t, sizeof(Thread), CacheType_t::Data);
    CleanOrInvalidateM7Cache((uint32_t)t->stack.address, t->stack.length, CacheType_t::Data);
    {
        CriticalGuard cg(owning_process.sl);
        owning_process.threads.push_back(t);
    }

    return t;
}

Thread *GetCurrentThreadForCore(int coreid)
{
#if GK_DUAL_CORE
    if(coreid == -1)
    {
        coreid = GetCoreID();
    }

    {
        //CriticalGuard cg(s.current_thread[coreid].m);
        return s.current_thread[coreid].v;  // <- should be atomic
    }
#else
    return s.current_thread[0].v;
#endif
}

Thread *GetNextThreadForCore(int coreid)
{
#if GK_DUAL_CORE
    if(coreid == -1)
    {
        coreid = GetCoreID();
    }

    return s.GetNextThread(coreid);
#else
    return s.GetNextThread(0);
#endif
}

int GetCoreID()
{
#if GK_DUAL_CORE
    return (*( ( volatile uint32_t * ) 0xE000ed00 ) & 0xfff0UL) == 0xc240 ? 1 : 0;
#else
    return 0;
#endif
}

void SetNextThreadForCore(Thread *t, int coreid)
{
    [[maybe_unused]] bool flush_cache = false;

#if GK_DUAL_CORE
    if(coreid == -1)
    {
        coreid = GetCoreID();
    }
#else
    coreid = 0;
#endif

    {
        CriticalGuard cg(s.current_thread[coreid].m);

        if(s.current_thread[coreid].v)
        {
            CriticalGuard cg2(s.current_thread[coreid].v->sl);
            s.current_thread[coreid].v->chosen_for_core = 0;
            s.current_thread[coreid].v->running_on_core = 0;
            if(s.current_thread[coreid].v == &dt)
            {
                flush_cache = true;
            }
        }

        {
            CriticalGuard cg2(t->sl);
            t->chosen_for_core = 0;
            t->running_on_core = coreid + 1;

            if(t->is_blocking)
            {
                while(true);
            }
        }

        s.current_thread[coreid].v = t;
    }

#if GK_USE_CACHE
    if(flush_cache && coreid == 0)
    {
        SCB_CleanInvalidateDCache();
    }
#endif
}

/* Called from PendSV */
void ScheduleThread(Thread *t)
{
    //s.Schedule(t);
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
