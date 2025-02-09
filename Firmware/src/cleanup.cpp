#include "cleanup.h"
#include "process.h"
#include "scheduler.h"
#include "sync_primitive_locks.h"

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
    //auto cpsr = lock_sl(t->sl);
    auto cpsr = DisableInterrupts();

    t->for_deletion = true;

    /* The following should work because we are a high priority thread so the for_deletion thread
        should never pre-empt us, and therefore we just need to wait for the other core, if any,
        to finish with the thread */
    while(t->tss.chosen_for_core || t->tss.running_on_core || t->tss.deschedule_from_core);

    /* release mutexes and rwlocks held by the thread */
    unlock_all_thread_sync_primitives(t->locked_mutexes, t);
    unlock_all_thread_sync_primitives(t->locked_rwlocks, t);
    
    /* Remove from schedulers */
#if GK_DUAL_CORE_AMP
    scheds[0].Unschedule(t);
    scheds[1].Unschedule(t);
#else
    sched.Unschedule(t);
#endif

    /* Clean up thread resources */
    t->p.DeleteMPURegion(t->stack);
    t->p.DeleteMPURegion(t->mr_tls);

    t->p.UpdateMPURegionsForThreads();

    memblk_deallocate(t->mr_tls);
    memblk_deallocate(t->stack);

    delete t;

    RestoreInterrupts(cpsr);
}

void cleanup(Process *p)
{
    /* See logic in cleanup(Thread *) */
    //auto cpsr = lock_sl(p->sl);
    auto cpsr = DisableInterrupts();
    
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
    if(p->mr_hot.valid)
        memblk_deallocate(p->mr_hot);
    if(p->audio.mr_sound.valid)
        memblk_deallocate(p->audio.mr_sound);
    if(p->mr_gtext.valid)
        memblk_deallocate(p->mr_gtext);

    /* cleanup mutexes etc */
    delete_all_process_sync_primitives(p->owned_conditions, p);
    delete_all_process_sync_primitives(p->owned_mutexes, p);
    delete_all_process_sync_primitives(p->owned_rwlocks, p);
    delete_all_process_sync_primitives(p->owned_semaphores, p);

    if(p->need_to_free)
        delete p;

    RestoreInterrupts(cpsr);
}
