#include "drm_scheduler.h"
#include "osmutex.h"
#include "logger.h"

int DRMScheduler::push_job(std::unique_ptr<drm_sched_job> &&j)
{
    CriticalGuard cg(sl);
    if(!is_init)
    {
        klog("drm_sched: not yet init\n");
        return -1;
    }

    klog("drm_sched: unimpl\n");
    return -1;
}
