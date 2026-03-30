#ifndef DRM_SCHEDULER_H
#define DRM_SCHEDULER_H

#include "linux_types.h"
#include <list>
#include <osmutex.h>

/* TODO:

    DRM scheduler has:
        multiple run queues with different priorities (etnaviv only uses one - PRIORITY_NORMAL)
            Each run queue has a queue of _entities_ - etnaviv has 4 here one per pipe

    The scheduler then runs each queue as appropriate.  Note that within an entity, the jobs are
        always executed in a FIFO manner.

    There is also a credit count mechanism for flow control.

*/

#define DRM_SCHED_PRIORITY_NORMAL       1
#define DRM_SCHED_PRIORITY_COUNT        1

using DRMSchedulerEntity = std::list<drm_sched_job>;

class DRMSchedulerPriority
{
    Spinlock sl;
    std::vector<DRMSchedulerEntity> entities;
};

class DRMScheduler
{
    Spinlock sl;
    std::array<DRMSchedulerPriority, DRM_SCHED_PRIORITY_COUNT> priorities;

public:
    void init(size_t priority, size_t entity);
};

#endif
