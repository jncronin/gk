#include "cleanup.h"
#include "process.h"
#include "scheduler.h"
#include "sync_primitive_locks.h"
#include "syscalls_int.h"
//#include "reset.h"

CleanupQueue_t CleanupQueue;

static void *cleanup_thread(void *);

void init_cleanup()
{
    Schedule(Thread::Create("cleanup", cleanup_thread, nullptr, true, GK_PRIORITY_NORMAL, p_kernel));
}

void *cleanup_thread(void *)
{
    while(true)
    {
        cleanup_message msg;
        if(CleanupQueue.Pop(&msg))
        {
            if(msg.is_thread)
            {
                ThreadList.Release(msg.id);
            }
            else
            {
                ProcessList.Release(msg.id);
            }
        }
    }
}
