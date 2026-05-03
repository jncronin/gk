/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 Etnaviv Project
 */

#ifndef __ETNAVIV_SCHED_H__
#define __ETNAVIV_SCHED_H__

//#include <drm/gpu_scheduler.h>
#include <memory>

struct etnaviv_gpu;

class DRMScheduler;

int etnaviv_sched_init(DRMScheduler *sched);
void etnaviv_sched_fini(struct etnaviv_gpu *gpu);
int etnaviv_sched_push_job(std::shared_ptr<etnaviv_gem_submit> &submit);

#endif /* __ETNAVIV_SCHED_H__ */
