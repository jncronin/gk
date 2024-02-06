#include "thread.h"
#include "memblk.h"
#include "mpuregions.h"

#include <cstring>

#include "scheduler.h"

extern Scheduler s;

Thread *Thread::Create(std::string name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            CPUAffinity affinity,
            MemRegion stackblk,
            mpu_saved_state extra_permissions,
            mpu_saved_state extra_permissions2)
{
    SRAM4RegionAllocator<Thread> alloc;
    auto tloc = alloc.allocate(1);
    if(!tloc)
        return nullptr;
    auto t = new(tloc) Thread;
    memset(&t->tss, 0, sizeof(thread_saved_state));

    t->affinity = affinity;
    t->base_priority = priority;
    t->is_privileged = is_priv;
    t->name = name;
    t->is_dummy = false;

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
    stack[--top_stack] = 0UL;
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

    SCB_CleanDCache_by_Addr((uint32_t *)t, sizeof(Thread));
    SCB_CleanDCache_by_Addr((uint32_t *)t->stack.address, t->stack.length);

    return t;
}

Thread *GetCurrentThreadForCore(int coreid)
{
    if(coreid == -1)
    {
        coreid = GetCoreID();
    }

    {
        CriticalGuard cg(s.current_thread[coreid].m);
        return s.current_thread[coreid].v;
    }
}

Thread *GetNextThreadForCore(int coreid)
{
    if(coreid == -1)
    {
        coreid = GetCoreID();
    }

    return s.GetNextThread(coreid);
}

int GetCoreID()
{
    return (*( ( volatile uint32_t * ) 0xE000ed00 ) & 0xfff0UL) == 0xc240 ? 1 : 0;
}

void SetNextThreadForCore(Thread *t, int coreid)
{
    bool flush_cache = false;

    if(coreid == -1)
    {
        coreid = GetCoreID();
    }

    {
        CriticalGuard cg(s.current_thread[coreid].m);

        if(s.current_thread[coreid].v)
        {
            CriticalGuard cg2(s.current_thread[coreid].v->sl);
            s.current_thread[coreid].v->chosen_for_core = 0;
            s.current_thread[coreid].v->running_on_core = 0;
            if(s.current_thread[coreid].v->is_dummy)
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

    if(flush_cache)
    {
        SCB_CleanInvalidateDCache();
    }
}

/* Called from PendSV */
void ScheduleThread(Thread *t)
{
    //s.Schedule(t);
}
