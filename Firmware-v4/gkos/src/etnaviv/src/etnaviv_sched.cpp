// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 Etnaviv Project
 */

//#include <linux/moduleparam.h>

#include "etnaviv_drv.h"
#include "etnaviv_dump.h"
#include "etnaviv_gem.h"
#include "etnaviv_gpu.h"
#include "etnaviv_sched.h"
#include "state.xml.h"
#include "state_hi.xml.h"
#include "fencefile.h"

[[maybe_unused]] static int etnaviv_job_hang_limit = 0;
[[maybe_unused]] static int etnaviv_hw_jobs_limit = 4;

static std::shared_ptr<dma_fence> etnaviv_sched_run_job(struct drm_sched_job *sched_job)
{
	auto submit = to_etnaviv_submit(sched_job);
	std::shared_ptr<dma_fence> fence = NULL;

	if (likely(!sched_job->finished->IsSignalled()))
		fence = etnaviv_gpu_submit(submit);
	else
		dev_dbg(submit->gpu->dev, "skipping bad job\n");

	return fence;
}


static enum drm_gpu_sched_stat etnaviv_sched_timedout_job(struct drm_sched_job
							  *sched_job)
{
	klog("etnaviv_sched_timedout_job not implemented\n");
	return DRM_GPU_SCHED_STAT_NO_HANG;
#if 0
	struct etnaviv_gem_submit *submit = to_etnaviv_submit(sched_job);
	struct etnaviv_gpu *gpu = submit->gpu;
	u32 dma_addr, primid = 0;
	int change;

	/*
	 * If the GPU managed to complete this jobs fence, the timeout has
	 * fired before free-job worker. The timeout is spurious, so bail out.
	 */
	if (dma_fence_is_signaled(submit->out_fence))
		return DRM_GPU_SCHED_STAT_NO_HANG;

	/*
	 * If the GPU is still making forward progress on the front-end (which
	 * should never loop) we shift out the timeout to give it a chance to
	 * finish the job.
	 */
	dma_addr = gpu_read(gpu, VIVS_FE_DMA_ADDRESS);
	change = dma_addr - gpu->hangcheck_dma_addr;
	if (submit->exec_state == ETNA_PIPE_3D) {
		/* guard against concurrent usage from perfmon_sample */
		mutex_lock(&gpu->lock);
		gpu_write(gpu, VIVS_MC_PROFILE_CONFIG0,
			  VIVS_MC_PROFILE_CONFIG0_FE_CURRENT_PRIM <<
			  VIVS_MC_PROFILE_CONFIG0_FE__SHIFT);
		primid = gpu_read(gpu, VIVS_MC_PROFILE_FE_READ);
		mutex_unlock(&gpu->lock);
	}
	if (gpu->state == ETNA_GPU_STATE_RUNNING &&
	    (gpu->completed_fence != gpu->hangcheck_fence ||
	     change < 0 || change > 16 ||
	     (submit->exec_state == ETNA_PIPE_3D &&
	      gpu->hangcheck_primid != primid))) {
		gpu->hangcheck_dma_addr = dma_addr;
		gpu->hangcheck_primid = primid;
		gpu->hangcheck_fence = gpu->completed_fence;
		return DRM_GPU_SCHED_STAT_NO_HANG;
	}

	/* block scheduler */
	drm_sched_stop(&gpu->sched, sched_job);

	if(sched_job)
		drm_sched_increase_karma(sched_job);

	/* get the GPU back into the init state */
	etnaviv_core_dump(submit);
	etnaviv_gpu_recover_hang(submit);

	drm_sched_resubmit_jobs(&gpu->sched);

	drm_sched_start(&gpu->sched, 0);
	return DRM_GPU_SCHED_STAT_RESET;
#endif
}

static void etnaviv_sched_free_job(struct drm_sched_job *sched_job)
{
	auto escj = reinterpret_cast<etnaviv_sched_job *>(sched_job);
	escj->deps.clear();
	escj->finished = nullptr;
	escj->scheduled = nullptr;
	escj->submit = nullptr;
}

static const struct drm_sched_backend_ops etnaviv_sched_ops = {
	.run_job = etnaviv_sched_run_job,
	.timedout_job = etnaviv_sched_timedout_job,
	.free_job = etnaviv_sched_free_job,
};


int etnaviv_sched_push_job(std::shared_ptr<etnaviv_gem_submit> submit)
{
	auto &gpu = submit->gpu;

	/*
	 * Hold the sched lock across the whole operation to avoid jobs being
	 * pushed out of order with regard to their sched fence seqnos as
	 * allocated in drm_sched_job_arm.
	 */
	gpu->sched_lock->lock();

	//drm_sched_job_arm(&submit->sched_job);	// prepares the jobs fences
	submit->sched_job->scheduled = std::make_shared<dma_fence>();
	submit->sched_job->finished = std::make_shared<dma_fence>();
	submit->sched_job->submit = submit;		// allow us to extract the job again
	submit->sched_job->arm_time = clock_cur();
	submit->sched_job->priority = DRM_SCHED_PRIORITY_NORMAL;

	submit->out_fence = submit->sched_job->finished;
	submit->out_fence_id = gpu->user_fences->Register(submit->out_fence, gpu->user_fences);

	submit->ctx->sched->push_job(std::move(submit->sched_job));

	gpu->sched_lock->unlock();

	return 0;
}

int etnaviv_sched_init(DRMScheduler *dsched)
{
	dsched->ops = &etnaviv_sched_ops;
	dsched->timeout = kernel_time_from_ms(500);

	return 0;
}

void etnaviv_sched_fini(struct etnaviv_gpu *gpu)
{
	//drm_sched_fini(&gpu->sched);
}
