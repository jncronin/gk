/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 Etnaviv Project
 */

#ifndef __ETNAVIV_SCHED_H__
#define __ETNAVIV_SCHED_H__

//#include <drm/gpu_scheduler.h>
#include <memory>

struct etnaviv_gpu;

static inline
struct etnaviv_gem_submit *to_etnaviv_submit(struct drm_sched_job *sched_job)
{
	auto esj = reinterpret_cast<etnaviv_sched_job *>(sched_job);
	return esj->submit.get();
}

int etnaviv_sched_init(struct etnaviv_gpu *gpu);
void etnaviv_sched_fini(struct etnaviv_gpu *gpu);
int etnaviv_sched_push_job(std::shared_ptr<etnaviv_gem_submit> submit);

#endif /* __ETNAVIV_SCHED_H__ */
