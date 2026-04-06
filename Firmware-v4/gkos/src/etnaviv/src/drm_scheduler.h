#ifndef DRM_SCHEDULER_H
#define DRM_SCHEDULER_H

#include "linux_types.h"
#include <list>
#include <queue>
#include <osmutex.h>
#include "kernel_time.h"

/* TODO:

    DRM scheduler has:
    Multiple priorities of multiple run queues
    We simplify it a bit so that we only have one run queue per prioriry

    There is also a credit count mechanism for flow control.  Essentially running a job
    on the gpu increases a counter in the scheduler, and finishing that job decreases it.
    If the count is too high, we do not send any more jobs to the gpu but keep them
    batched here.

*/

#define DRM_SCHED_PRIORITY_NORMAL       0
#define DRM_SCHED_PRIORITY_COUNT        1

using DRMSchedulerEntity = std::queue<std::unique_ptr<drm_sched_job>>;

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
public:
    Spinlock sl;
    std::array<DRMSchedulerEntity, DRM_SCHED_PRIORITY_COUNT> priorities;
    std::array<bool, DRM_SCHED_PRIORITY_COUNT> is_init;
    std::array<CountingSemaphore, DRM_SCHED_PRIORITY_COUNT> sems;
    std::array<id_t, DRM_SCHED_PRIORITY_COUNT> tids;
    std::array<bool, DRM_SCHED_PRIORITY_COUNT> shutdown_req;

    unsigned int next_job_id = 0;       // for debugging

    const drm_sched_backend_ops *ops;
    kernel_time timeout;
    void init(size_t priority, size_t entity, std::shared_ptr<DRMScheduler> &sched);
    int push_job(std::unique_ptr<drm_sched_job> &&j);

    DRMScheduler();
};

#endif
