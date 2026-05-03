#include "cleanup.h"
#include "process.h"
#include "scheduler.h"
#include "sync_primitive_locks.h"
#include "syscalls_int.h"
#include "scheduler.h"
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
                // Ensure the threads have completed before releasing a process
                auto pproc = ProcessList.Get(msg.id).v;
                if(pproc)
                {
                    while(true)
                    {
                        std::vector<id_t> proc_threads;

                        {
                            CriticalGuard cg(pproc->sl);
                            proc_threads = pproc->threads;
                        }

                        if(proc_threads.empty())
                        {
                            break;
                        }

                        /* Check all threads are not running and have no delete guards */
                        std::vector<id_t> finished_threads;
                        for(auto tid : proc_threads)
                        {
                            auto ct = ThreadList.Get(tid);
                            bool has_finished = false;
                            if(ct.v)
                            {
                                CriticalGuard cg(ct.v->sl, sched.sl_cur_next);
                                bool is_running = false;
                                for(auto ccore = 0u; ccore < GK_NUM_CORES; ccore++)
                                {
                                    if(sched.current_thread[ccore] == ct.v || sched.next_thread[ccore] == ct.v)
                                    {
                                        is_running = true;
                                        break;
                                    }
                                }
                                if(ct.v->delete_guards == 0 && !is_running)
                                {
                                    has_finished = true;
                                }                                
                            }
                            else
                            {
                                has_finished = true;
                            }

                            if(has_finished)
                            {
                                finished_threads.push_back(tid);
                            }
                        }

                        {
                            CriticalGuard cg(pproc->sl);
                            for(auto tid : finished_threads)
                            {
                                for(auto iter = pproc->threads.begin(); iter != pproc->threads.end(); iter++)
                                {
                                    if(*iter == tid)
                                    {
                                        pproc->threads.erase(iter);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                ProcessList.Release(msg.id);
            }
        }
    }
}
