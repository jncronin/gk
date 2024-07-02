#include "cleanup.h"
#include "process.h"
#include "scheduler.h"

SRAM4_DATA CleanupQueue_t CleanupQueue;

static void cleanup(Thread *t);
static void cleanup(Process *p);

void *cleanup_thread(void *)
{
    while(true)
    {
        cleanup_message msg;
        if(!CleanupQueue.Pop(&msg))
            continue;
        
        if(msg.is_thread)
        {
            cleanup(msg.t);
        }
        else
        {
            cleanup(msg.p);
        }
    }
    return nullptr;
}

static inline uint32_t lock_sl(Spinlock &_s)
{
    auto cpsr = DisableInterrupts();
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
#if DEBUG_SPINLOCK
    uint32_t lr;
    __asm__ volatile ("mov %0, lr \n" : "=r" (lr));
    _s.lock(lr & ~1UL);
#else
    _s.lock();
#endif
#endif
    return cpsr;
}

void cleanup(Thread *t)
{
    /* we use half the CriticalGuard code here - essentially we want to disable interrupts,
        and optionally lock the thread (if multicore), but not release the lock at the end.
        This prevents the small chance that someone else will try and use the thread in the
        short time between unlocking it and it being deleted.
        */
    auto cpsr = lock_sl(t->sl);

    t->for_deletion = true;

    /* The following should work because we are a high priority thread so the for_deletion thread
        should never pre-empt us, and therefore we just need to wait for the other core, if any,
        to finish with the thread */
    while(t->tss.chosen_for_core || t->tss.running_on_core || t->tss.deschedule_from_core);

    /* TODO: ensure TLS destructors are run here */

    /* TODO: release mutexes and rwlocks held by the thread */
    
    /* Clean up thread resources */
    memblk_deallocate(t->mr_tls);
    memblk_deallocate(t->stack);

    /* Remove from schedulers */
#if GK_DUAL_CORE_AMP
    scheds[0].Unschedule(t);
    scheds[1].Unschedule(t);
#else
    sched.Unschedule(t);
#endif

    delete t;

    RestoreInterrupts(cpsr);
}

void cleanup(Process *p)
{
    /* See logic in cleanup(Thread *) */
    auto cpsr = lock_sl(p->sl);

    p->for_deletion = true;

    for(auto t : p->threads)
    {
        cleanup(t);
    }
    p->threads.clear();

    /* Cleanup process resources */
    for(auto &mr : p->mmap_regions)
    {
        memblk_deallocate(mr.second.mr);
    }
    memblk_deallocate(p->heap);
    memblk_deallocate(p->code_data);

    /* TODO: cleanup mutexes etc (need to store in process structure first) */

    delete p;

    RestoreInterrupts(cpsr);
}
