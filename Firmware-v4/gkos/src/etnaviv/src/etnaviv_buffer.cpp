// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2014-2018 Etnaviv Project
 */

//#include <drm/drm_drv.h>
//#include <drm/drm_print.h>

#include "etnaviv_cmdbuf.h"
#include "etnaviv_gpu.h"
#include "etnaviv_gem.h"
#include "etnaviv_mmu.h"
#include "etnaviv_buffer.h"

#include "common.xml.h"
#include "state.xml.h"
#include "state_blt.xml.h"
#include "state_hi.xml.h"
#include "state_3d.xml.h"
#include "cmdstream.xml.h"

#include "etnaviv_flop_reset.h"

[[maybe_unused]] static void etnaviv_cmd_select_pipe(struct etnaviv_gpu *gpu,
	struct etnaviv_cmdbuf *buffer, u8 pipe)
{
	u32 flush = 0;

	BUG_ON(!gpu->lock->held());

	/*
	 * This assumes that if we're switching to 2D, we're switching
	 * away from 3D, and vice versa.  Hence, if we're switching to
	 * the 2D core, we need to flush the 3D depth and color caches,
	 * otherwise we need to flush the 2D pixel engine cache.
	 */
	if (gpu->exec_state == ETNA_PIPE_2D)
		flush = VIVS_GL_FLUSH_CACHE_PE2D;
	else if (gpu->exec_state == ETNA_PIPE_3D)
		flush = VIVS_GL_FLUSH_CACHE_DEPTH | VIVS_GL_FLUSH_CACHE_COLOR;

	CMD_LOAD_STATE(buffer, VIVS_GL_FLUSH_CACHE, flush);
	CMD_SEM(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
	CMD_STALL(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);

	CMD_LOAD_STATE(buffer, VIVS_GL_PIPE_SELECT,
		       VIVS_GL_PIPE_SELECT_PIPE(pipe));
}

[[maybe_unused]] static void etnaviv_buffer_dump(struct etnaviv_gpu *gpu,
	struct etnaviv_cmdbuf *buf, u32 off, u32 len)
{
	u32 size = buf->size;
	u32 *ptr = (u32 *)((uintptr_t)buf->vaddr + off);

	dev_info(gpu->dev, "virt %p phys 0x%08x free 0x%08x\n",
			ptr, etnaviv_cmdbuf_get_va(buf,
			gpu->mmu_context->cmdbuf_mapping.get()) +
			off, size - len * 4 - off);

	for(auto i = 0u; i < len; i += 4u)
	{
		auto row = &ptr[i];
		klog("GPU: CMD[%4x]: %08x %08x %08x %08x\n", i * 4, row[0], row[1], row[2], row[3]);
	}

	//print_hex_dump(KERN_INFO, "cmd ", DUMP_PREFIX_OFFSET, 16, 4,
	//		ptr, len * 4, 0);
}

/*
 * Safely replace the WAIT of a waitlink with a new command and argument.
 * The GPU may be executing this WAIT while we're modifying it, so we have
 * to write it in a specific order to avoid the GPU branching to somewhere
 * else.  'wl_offset' is the offset to the first byte of the WAIT command.
 */
static void etnaviv_buffer_replace_wait(struct etnaviv_cmdbuf *buffer,
	unsigned int wl_offset, u32 cmd, u32 arg)
{
	u32 *lw = (u32 *)((uintptr_t)buffer->vaddr + wl_offset);

	lw[1] = arg;
	__asm__ volatile("dmb ish\n" ::: "memory");
	lw[0] = cmd;
	__asm__ volatile("dmb ish\n" ::: "memory");
}

/*
 * Ensure that there is space in the command buffer to contiguously write
 * 'cmd_dwords' 64-bit words into the buffer, wrapping if necessary.
 */
static u32 etnaviv_buffer_reserve(struct etnaviv_gpu *gpu,
	struct etnaviv_cmdbuf *buffer, unsigned int cmd_dwords)
{
	if (buffer->user_size + cmd_dwords * sizeof(u64) > buffer->size)
		buffer->user_size = 0;

	return etnaviv_cmdbuf_get_va(buffer,
				     gpu->mmu_context->cmdbuf_mapping.get()) +
	       buffer->user_size;
}

u16 etnaviv_buffer_init(struct etnaviv_gpu *gpu)
{
	struct etnaviv_cmdbuf *buffer = &gpu->buffer;

	BUG_ON(!gpu->lock->held());

	/* initialize buffer */
	buffer->user_size = 0;

	/* Queue in PPU flop reset */
	if (etnaviv_flop_reset_ppu_require(&gpu->identity))
		etnaviv_flop_reset_ppu_run(gpu);

	CMD_WAIT(buffer, gpu->fe_waitcycles);
	CMD_LINK(buffer, 2,
		 etnaviv_cmdbuf_get_va(buffer, gpu->mmu_context->cmdbuf_mapping.get())
		 + buffer->user_size - 4);

	return buffer->user_size / 8;
}

u16 etnaviv_buffer_config_mmuv2(struct etnaviv_gpu *gpu, u32 mtlb_addr, u32 safe_addr)
{
	struct etnaviv_cmdbuf *buffer = &gpu->buffer;

	BUG_ON(!gpu->lock->held());

	buffer->user_size = 0;

	if (gpu->identity.features & chipFeatures_PIPE_3D) {
		CMD_LOAD_STATE(buffer, VIVS_GL_PIPE_SELECT,
			       VIVS_GL_PIPE_SELECT_PIPE(ETNA_PIPE_3D));
		CMD_LOAD_STATE(buffer, VIVS_MMUv2_CONFIGURATION,
			mtlb_addr | VIVS_MMUv2_CONFIGURATION_MODE_MODE4_K);
		CMD_LOAD_STATE(buffer, VIVS_MMUv2_SAFE_ADDRESS, safe_addr);
		CMD_SEM(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
		CMD_STALL(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
	}

	if (gpu->identity.features & chipFeatures_PIPE_2D) {
		CMD_LOAD_STATE(buffer, VIVS_GL_PIPE_SELECT,
			       VIVS_GL_PIPE_SELECT_PIPE(ETNA_PIPE_2D));
		CMD_LOAD_STATE(buffer, VIVS_MMUv2_CONFIGURATION,
			mtlb_addr | VIVS_MMUv2_CONFIGURATION_MODE_MODE4_K);
		CMD_LOAD_STATE(buffer, VIVS_MMUv2_SAFE_ADDRESS, safe_addr);
		CMD_SEM(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
		CMD_STALL(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
	}

	CMD_END(buffer);

	buffer->user_size = ALIGN(buffer->user_size, 8);

	return buffer->user_size / 8;
}

u16 etnaviv_buffer_config_pta(struct etnaviv_gpu *gpu, unsigned short id)
{
	struct etnaviv_cmdbuf *buffer = &gpu->buffer;

	BUG_ON(!gpu->lock->held());

	buffer->user_size = 0;

	CMD_LOAD_STATE(buffer, VIVS_MMUv2_PTA_CONFIG,
		       VIVS_MMUv2_PTA_CONFIG_INDEX(id));

	CMD_END(buffer);

	buffer->user_size = ALIGN(buffer->user_size, 8);

	return buffer->user_size / 8;
}

void etnaviv_buffer_end(struct etnaviv_gpu *gpu)
{
	struct etnaviv_cmdbuf *buffer = &gpu->buffer;
	unsigned int waitlink_offset = buffer->user_size - 16;
	u32 link_target, flush = 0;
	bool has_blt = !!(gpu->identity.minor_features5 &
			  chipMinorFeatures5_BLT_ENGINE);

	BUG_ON(!gpu->lock->held());

	if (gpu->exec_state == ETNA_PIPE_2D)
		flush = VIVS_GL_FLUSH_CACHE_PE2D;
	else if (gpu->exec_state == ETNA_PIPE_3D)
		flush = VIVS_GL_FLUSH_CACHE_DEPTH |
			VIVS_GL_FLUSH_CACHE_COLOR |
			VIVS_GL_FLUSH_CACHE_TEXTURE |
			VIVS_GL_FLUSH_CACHE_TEXTUREVS |
			VIVS_GL_FLUSH_CACHE_SHADER_L2;

	if (flush) {
		unsigned int dwords = 7;

		if (has_blt)
			dwords += 10;

		link_target = etnaviv_buffer_reserve(gpu, buffer, dwords);

		CMD_SEM(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
		CMD_STALL(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
		if (has_blt) {
			CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x1);
			CMD_SEM(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_BLT);
			CMD_STALL(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_BLT);
			CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x0);
		}
		CMD_LOAD_STATE(buffer, VIVS_GL_FLUSH_CACHE, flush);
		if (gpu->exec_state == ETNA_PIPE_3D) {
			if (has_blt) {
				CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x1);
				CMD_LOAD_STATE(buffer, VIVS_BLT_SET_COMMAND, 0x1);
				CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x0);
			} else {
				CMD_LOAD_STATE(buffer, VIVS_TS_FLUSH_CACHE,
					       VIVS_TS_FLUSH_CACHE_FLUSH);
			}
		}
		CMD_SEM(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
		CMD_STALL(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
		if (has_blt) {
			CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x1);
			CMD_SEM(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_BLT);
			CMD_STALL(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_BLT);
			CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x0);
		}
		CMD_END(buffer);

		etnaviv_buffer_replace_wait(buffer, waitlink_offset,
					    VIV_FE_LINK_HEADER_OP_LINK |
					    VIV_FE_LINK_HEADER_PREFETCH(dwords),
					    link_target);
	} else {
		/* Replace the last link-wait with an "END" command */
		etnaviv_buffer_replace_wait(buffer, waitlink_offset,
					    VIV_FE_END_HEADER_OP_END, 0);
	}
}

/* Append a 'sync point' to the ring buffer. */
void etnaviv_sync_point_queue(struct etnaviv_gpu *gpu, unsigned int event)
{
	struct etnaviv_cmdbuf *buffer = &gpu->buffer;
	unsigned int waitlink_offset = buffer->user_size - 16;
	u32 dwords, target;

	BUG_ON(!gpu->lock->held());

	/*
	 * We need at most 3 dwords in the return target:
	 * 1 event + 1 end + 1 wait + 1 link.
	 */
	dwords = 4;
	target = etnaviv_buffer_reserve(gpu, buffer, dwords);

	/* Signal sync point event */
	CMD_LOAD_STATE(buffer, VIVS_GL_EVENT, VIVS_GL_EVENT_EVENT_ID(event) |
		       VIVS_GL_EVENT_FROM_PE);

	/* Stop the FE to 'pause' the GPU */
	CMD_END(buffer);

	/* Append waitlink */
	CMD_WAIT(buffer, gpu->fe_waitcycles);
	CMD_LINK(buffer, 2,
		 etnaviv_cmdbuf_get_va(buffer, gpu->mmu_context->cmdbuf_mapping.get())
		 + buffer->user_size - 4);

	/*
	 * Kick off the 'sync point' command by replacing the previous
	 * WAIT with a link to the address in the ring buffer.
	 */
	etnaviv_buffer_replace_wait(buffer, waitlink_offset,
				    VIV_FE_LINK_HEADER_OP_LINK |
				    VIV_FE_LINK_HEADER_PREFETCH(dwords),
				    target);
}

/* Append a command buffer to the ring buffer. */
void etnaviv_buffer_queue(struct etnaviv_gpu *gpu, u32 exec_state,
	std::shared_ptr<etnaviv_iommu_context> mmu_context, unsigned int event,
	struct etnaviv_cmdbuf *cmdbuf)
{
	struct etnaviv_cmdbuf *buffer = &gpu->buffer;
	unsigned int waitlink_offset = buffer->user_size - 16;
	u32 return_target, return_dwords;
	u32 link_target, link_dwords;
	bool switch_context = gpu->exec_state != (int)exec_state;
	bool switch_mmu_context = gpu->mmu_context != mmu_context;
	unsigned int new_flush_seq = *(const volatile unsigned int *)&mmu_context->flush_seq;
	bool need_flush = switch_mmu_context || gpu->flush_seq != new_flush_seq;
	bool has_blt = !!(gpu->identity.minor_features5 &
			  chipMinorFeatures5_BLT_ENGINE);

	BUG_ON(!gpu->lock->held());

	if (drm_debug_enabled(DRM_UT_DRIVER))
		etnaviv_buffer_dump(gpu, cmdbuf, 0, cmdbuf->size / 4);

	link_target = etnaviv_cmdbuf_get_va(cmdbuf,
					    gpu->mmu_context->cmdbuf_mapping.get());
	link_dwords = cmdbuf->size / 8;

	klog("GPU: cmdbuf: %08x, size %u dwords\n", link_target, link_dwords);

	switch_context = true;
	switch_mmu_context = true;	// adding these does not seem to have helped

	/*
	 * If we need maintenance prior to submitting this buffer, we will
	 * need to append a mmu flush load state, followed by a new
	 * link to this buffer - a total of four additional words.
	 */
	if (need_flush || switch_context) {
		u32 target, extra_dwords;

		klog("GPU: cmdbuf: need_flush: %s, switch_context: %s\n", need_flush ? "true" : "false",
			switch_context ? "true" : "false");

		/* link command */
		extra_dwords = 1;

		/* flush command */
		if (need_flush) {
			if (gpu->mmu_context->global->version == ETNAVIV_IOMMU_V1)
				extra_dwords += 1;
			else
				extra_dwords += 3;
		}

		/* pipe switch commands */
		if (switch_context)
			extra_dwords += 4;

		/* PTA load command */
		if (switch_mmu_context && gpu->sec_mode == ETNA_SEC_KERNEL)
			extra_dwords += 1;

		target = etnaviv_buffer_reserve(gpu, buffer, extra_dwords);
		klog("writing extra commands at %08x\n", target);
		/*
		 * Switch MMU context if necessary. Must be done after the
		 * link target has been calculated, as the jump forward in the
		 * kernel ring still uses the last active MMU context before
		 * the switch.
		 */
		if (switch_mmu_context) {
			gpu->mmu_context = mmu_context;
		}

		if (need_flush) {
			/* Add the MMU flush */
			if (gpu->mmu_context->global->version == ETNAVIV_IOMMU_V1) {
				CMD_LOAD_STATE(buffer, VIVS_GL_FLUSH_MMU,
					       VIVS_GL_FLUSH_MMU_FLUSH_FEMMU |
					       VIVS_GL_FLUSH_MMU_FLUSH_UNK1 |
					       VIVS_GL_FLUSH_MMU_FLUSH_UNK2 |
					       VIVS_GL_FLUSH_MMU_FLUSH_PEMMU |
					       VIVS_GL_FLUSH_MMU_FLUSH_UNK4);
			} else {
				u32 flush = VIVS_MMUv2_CONFIGURATION_MODE_MASK |
					    VIVS_MMUv2_CONFIGURATION_FLUSH_FLUSH;

				if (switch_mmu_context &&
				    gpu->sec_mode == ETNA_SEC_KERNEL) {
					unsigned short id =
						etnaviv_iommuv2_get_pta_id(gpu->mmu_context.get());

					klog("GPU: switching to context id %u\n", id);
					CMD_LOAD_STATE(buffer,
						VIVS_MMUv2_PTA_CONFIG,
						VIVS_MMUv2_PTA_CONFIG_INDEX(id));
				}

				if (gpu->sec_mode == ETNA_SEC_NONE)
					flush |= etnaviv_iommuv2_get_mtlb_addr(gpu->mmu_context.get());

				CMD_LOAD_STATE(buffer, VIVS_MMUv2_CONFIGURATION,
					       flush);
				CMD_SEM(buffer, SYNC_RECIPIENT_FE,
					SYNC_RECIPIENT_PE);
				CMD_STALL(buffer, SYNC_RECIPIENT_FE,
					SYNC_RECIPIENT_PE);
			}

			gpu->flush_seq = new_flush_seq;
		}

		klog("GPU: old exec_state: %u, new exec_state: %u\n", gpu->exec_state, exec_state);

		if (switch_context) {
			etnaviv_cmd_select_pipe(gpu, buffer, exec_state);
			gpu->exec_state = exec_state;
		}

		/* And the link to the submitted buffer */
		link_target = etnaviv_cmdbuf_get_va(cmdbuf,
					gpu->mmu_context->cmdbuf_mapping.get());
		CMD_LINK(buffer, link_dwords, link_target);

		klog("GPU: writing link back to cmdbuf %08x\n", link_target);

		/* Update the link target to point to above instructions */
		link_target = target;
		link_dwords = extra_dwords;
	}

	/*
	 * Append a LINK to the submitted command buffer to return to
	 * the ring buffer.  return_target is the ring target address.
	 * We need at most 7 dwords in the return target: 2 cache flush +
	 * 2 semaphore stall + 1 event + 1 wait + 1 link.
	 */
	return_dwords = 7;

	/*
	 * When the BLT engine is present we need 6 more dwords in the return
	 * target: 3 enable/flush/disable + 4 enable/semaphore stall/disable,
	 * but we don't need the normal TS flush state.
	 */
	if (has_blt)
		return_dwords += 6;

	return_target = etnaviv_buffer_reserve(gpu, buffer, return_dwords);
	CMD_LINK(cmdbuf, return_dwords, return_target);

	/*
	 * Append a cache flush, stall, event, wait and link pointing back to
	 * the wait command to the ring buffer.
	 */
	if (gpu->exec_state == ETNA_PIPE_2D) {
		CMD_LOAD_STATE(buffer, VIVS_GL_FLUSH_CACHE,
				       VIVS_GL_FLUSH_CACHE_PE2D);
	} else {
		CMD_LOAD_STATE(buffer, VIVS_GL_FLUSH_CACHE,
				       VIVS_GL_FLUSH_CACHE_DEPTH |
				       VIVS_GL_FLUSH_CACHE_COLOR |
				       VIVS_GL_FLUSH_CACHE_SHADER_L1);
		if (has_blt) {
			CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x1);
			CMD_LOAD_STATE(buffer, VIVS_BLT_SET_COMMAND, 0x1);
			CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x0);
		} else {
			CMD_LOAD_STATE(buffer, VIVS_TS_FLUSH_CACHE,
					       VIVS_TS_FLUSH_CACHE_FLUSH);
		}
	}
	CMD_SEM(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
	CMD_STALL(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);

	if (has_blt) {
		CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x1);
		CMD_SEM(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_BLT);
		CMD_STALL(buffer, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_BLT);
		CMD_LOAD_STATE(buffer, VIVS_BLT_ENABLE, 0x0);
	}

	CMD_LOAD_STATE(buffer, VIVS_GL_EVENT, VIVS_GL_EVENT_EVENT_ID(event) |
		       VIVS_GL_EVENT_FROM_PE);
	CMD_WAIT(buffer, gpu->fe_waitcycles);
	CMD_LINK(buffer, 2,
		 etnaviv_cmdbuf_get_va(buffer, gpu->mmu_context->cmdbuf_mapping.get())
		 + buffer->user_size - 4);

	if (drm_debug_enabled(DRM_UT_DRIVER))
		pr_info("stream link to 0x%08x @ 0x%08x %p\n",
			return_target,
			etnaviv_cmdbuf_get_va(cmdbuf, gpu->mmu_context->cmdbuf_mapping.get()),
			cmdbuf->vaddr);

	if (drm_debug_enabled(DRM_UT_DRIVER)) {
		//print_hex_dump(KERN_INFO, "cmd ", DUMP_PREFIX_OFFSET, 16, 4,
		//	       cmdbuf->vaddr, cmdbuf->size, 0);

		pr_info("link op: %p\n", (void *)((uintptr_t)buffer->vaddr + waitlink_offset));
		pr_info("addr: 0x%08x\n", link_target);
		pr_info("back: 0x%08x\n", return_target);
		pr_info("event: %d\n", event);
	}

	/* Write an end to the start of our buffer to see if that can isolate a fault */
	//*(volatile uint32_t *)((uintptr_t)cmdbuf->vaddr + 0x120) = VIV_FE_END_HEADER_OP_END;

	// There is an issue with the RS which does blits - here uses to fill a destination
	//  area of memory.  Try using more commands like modern mesa dose

	// the beebbeeb kicker is at 0x128 - 08010580 beebbeeb
	*(volatile uint32_t *)((uintptr_t)cmdbuf->vaddr + 0x128) = 0x18000000;	// NOP
	*(volatile uint32_t *)((uintptr_t)cmdbuf->vaddr + 0x12c) = 0x18000000;	// for good measure
	// 
	//*(volatile uint32_t *)((uintptr_t)cmdbuf->vaddr + 0xdc) = 0x4600;
	__asm__ volatile("dsb sy\n" ::: "memory");

	/*
	 * Kick off the submitted command by replacing the previous
	 * WAIT with a link to the address in the ring buffer.
	 */
	klog("GPU: writing LINK to target %08x (prefetch %u) at address %08x\n",
		link_target, link_dwords, waitlink_offset);

	etnaviv_buffer_replace_wait(buffer, waitlink_offset,
				    VIV_FE_LINK_HEADER_OP_LINK |
				    VIV_FE_LINK_HEADER_PREFETCH(link_dwords),
				    link_target);

	if (drm_debug_enabled(DRM_UT_DRIVER))
		etnaviv_buffer_dump(gpu, buffer, 0, 0x50);
}
