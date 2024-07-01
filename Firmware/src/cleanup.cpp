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

void cleanup(Thread *t)
{
    {
        CriticalGuard cg(t->sl);
        t->for_deletion = true;

        /* The following should work because we are a high priority thread so the for_deletion thread
            should never pre-empt us, and therefore we just need to wait for the other core, if any,
            to finish with the thread */
        while(t->tss.chosen_for_core || t->tss.running_on_core);

        /* TODO: ensure TLS destructors are run here */
        
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
    }

    delete t;
}

void cleanup(Process *p)
{
    {
        CriticalGuard(p->sl);
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

    }

    delete p;
}
