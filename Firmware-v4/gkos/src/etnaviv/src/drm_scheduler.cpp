#include "drm_scheduler.h"
#include "osmutex.h"
#include "logger.h"
#include "thread.h"
#include "scheduler.h"
#include "process.h"
#include "fencefile.h"

static void *drm_sched_worker(void *p);
struct sched_worker_startup
{
    std::shared_ptr<DRMScheduler> s;
    unsigned int priority;
};

int DRMScheduler::push_job(std::unique_ptr<drm_sched_job> &&j)
{
    CriticalGuard cg(sl);
    if(j->priority > DRM_SCHED_PRIORITY_COUNT)
    {
        klog("drm_sched: invalid priority %u\n", j->priority);
        return -1;
    }
    if(!is_init[j->priority])
    {
        klog("drm_sched: not yet init\n");
        return -1;
    }

    j->job_id = next_job_id++;

#if GPU_DEBUG > 1
    klog("drm_sched: scheduling job %u\n", j->job_id);
#endif

    auto priority = j->priority;
    priorities[j->priority].push(std::move(j));
    sems[priority].Signal();

    return 0;
}

void DRMScheduler::init(size_t priority, size_t entity, std::shared_ptr<DRMScheduler> &dsched)
{
    CriticalGuard cg(sl);
    if(priority >= DRM_SCHED_PRIORITY_COUNT)
    {
        klog("drm_sched: invalid priority: %u\n", priority);
    }
    if(!is_init[priority])
    {
        priorities[priority] = DRMSchedulerEntity{};
        sems[priority] = CountingSemaphore{};
        shutdown_req[priority] = false;

        auto ws = new sched_worker_startup();
        ws->priority = priority;
        ws->s = dsched;

        auto t = Thread::Create("drm_worker_" + std::to_string(priority),
            drm_sched_worker, (void *)ws, true, GK_PRIORITY_HIGH,
            p_kernel);
        tids[priority] = t->id;
        Schedule(t);

        is_init[priority] = true;
    }
}

DRMScheduler::DRMScheduler()
{
    for(auto i = 0u; i < DRM_SCHED_PRIORITY_COUNT; i++)
    {
        is_init[i] = false;
    }
}

void *drm_sched_worker(void *p)
{
    auto ws = (sched_worker_startup *)p;
    auto priority = ws->priority;
    auto s = ws->s;
    delete ws;

    klog("drm_sched_worker start, priority %u\n", priority);

    while(true)
    {
        s->sems[priority].Wait();

        if(s->shutdown_req[priority])
            return nullptr;

        std::unique_ptr<drm_sched_job> j;

        {
            CriticalGuard cg(s->sl);

            if(s->priorities[priority].empty())
            {
                klog("drm_sched_worker: woken up without job to run\n");
                continue;
            }

            j = std::move(s->priorities[priority].front());
            s->priorities[priority].pop();
        }

#if GPU_DEBUG > 1
        klog("drm_sched_worker: running job %u\n", j->job_id);
#endif

        /* We now have a job to do - wait on the various fences */
        for(auto &dep : j->deps)
        {
            if(!dep->IsSignalled())
            {
                klog("drm_sched_worker: waiting on dependency\n");

                while(!dep->Wait(clock_cur() + kernel_time_from_ms(1000)))
                {
                    klog("drm_sched_worker: still waiting for dependency\n");
                }
            }
        }

#if GPU_DEBUG > 2
        klog("drm_sched_worker: job %u dependencies satisfied.\n", j->job_id);
#endif
        j->scheduled->Signal();

        auto finish_fence = s->ops->run_job(j.get());

#if GPU_DEBUG > 2
        klog("drm_sched_worker: job %u submitted to gpu, awaiting completion\n", j->job_id);
#endif

        auto job_completed = finish_fence->Wait(clock_cur() + s->timeout);
        
        if(job_completed)
        {
#if GPU_DEBUG > 1
            klog("drm_sched_worker: job %u completed\n", j->job_id);
#endif
            j->finished->Signal();
        }
        else
        {
            klog("drm_sched_worker: job %u timed out\n", j->job_id);
        }
    }
}

drm_sched_job::~drm_sched_job()
{
#if GPU_DEBUG > 4
    klog("drm_sched_job destructor called\n");
#endif
}
