#ifndef RETRAM_H
#define RETRAM_H

// define the layout of the first 64 kiB of RETRAM (0x20080000 +)
// the second 64 kiB is dedicated to log buffers

#include <cstdint>
#include "osspinlock.h"
#include "lockfreebuffer.h"

struct klog_buffer_t
{
    uint64_t magic;
    LockFreeBuffer b_file, b_uart;
};

struct retram_t
{
    klog_buffer_t klog;
};

#include "vmem.h"
[[maybe_unused]] static retram_t *retram = (retram_t *)PMEM_TO_VMEM(0x20080000);

void init_retram();

#endif
