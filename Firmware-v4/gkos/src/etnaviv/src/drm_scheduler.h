#ifndef DRM_SCHEDULER_H
#define DRM_SCHEDULER_H

#include "linux_types.h"
#include <list>
#include <queue>
#include <osmutex.h>
#include "kernel_time.h"

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

using DRMSchedulerEntity = std::queue<etnaviv_sched_job>;

enum drm_gpu_sched_stat
{
    DRM_GPU_SCHED_STAT_NONE,
    DRM_GPU_SCHED_STAT_RESET,
    DRM_GPU_SCHED_STAT_ENODEV,
    DRM_GPU_SCHED_STAT_NO_HANG,
};

struct drm_sched_backend_ops {
    std::shared_ptr<dma_fence> (*prepare_job)(struct drm_sched_job *sched_job, struct drm_sched_entity *s_entity);
    std::shared_ptr<dma_fence> (*run_job)(struct drm_sched_job *sched_job);
    enum drm_gpu_sched_stat (*timedout_job)(struct drm_sched_job *sched_job);
    void (*free_job)(struct drm_sched_job *sched_job);
    void (*cancel_job)(struct drm_sched_job *sched_job);
};

class DRMSchedulerPriority
{
    Spinlock sl;
    std::queue<DRMSchedulerEntity> entities;
};

class DRMScheduler
{
    Spinlock sl;
    std::array<DRMSchedulerPriority, DRM_SCHED_PRIORITY_COUNT> priorities;
    bool is_init = false;

public:
    const drm_sched_backend_ops *ops;
    kernel_time timeout;
    void init(size_t priority, size_t entity);
    int push_job(std::unique_ptr<drm_sched_job> &&j);
};

#endif
