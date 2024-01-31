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
            size_t stack_size,
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
    t->stack = memblk_allocate_for_stack(stack_size, affinity);
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
    stack[--top_stack] = reinterpret_cast<uint32_t>(p);

    t->tss.psp = t->stack.address + t->stack.length;
    
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
    if(coreid == -1)
    {
        coreid = GetCoreID();
    }

    {
        CriticalGuard cg(s.current_thread[coreid].m);
        s.current_thread[coreid].v = t;
    }    
}

/* Called from PendSV */
void ScheduleThread(Thread *t)
{
    s.Schedule(t);
}
