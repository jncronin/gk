#ifndef GK_TF_A_DEFS_H
#define GK_TF_A_DEFS_H

/* Utility functions/macros used by ported TF-A code */

#include <stdint.h>
#include "clocks.h"
#include "logger.h"
#include <string.h>

#ifndef NULL
#define NULL nullptr
#endif

#ifndef BIT
#define BIT(x)  (1ULL << x)
#endif

#ifndef BIT_32
#define BIT_32(x) (1UL << x)
#endif

#ifndef MAX
#define MAX(a, b) (((a) < (b)) ? (b) : (a))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define GENMASK_32(high, low) \
	((~0U >> (32U - 1U - (high))) ^ (((1U << (low)) - 1U)))
#define GENMASK_64(high, low) \
	((~0ULL >> (64U - 1U - (high))) ^ (((1ULL << (low)) - 1ULL)))
#define GENMASK GENMASK_64

#define U uintptr_t

static inline void mmio_write_16(uintptr_t addr, uint16_t val)
{
    *(volatile uint16_t *)addr = val;
    __asm__ volatile("dmb st\n" ::: "memory");
}

static inline uint16_t mmio_read_16(uintptr_t addr)
{
    __asm__ volatile("dsb sy\n" ::: "memory");
    return *(volatile uint16_t *)addr;
}

static inline void mmio_write_32(uintptr_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
    __asm__ volatile("dmb st\n" ::: "memory");
}

static inline uint32_t mmio_read_32(uintptr_t addr)
{
    __asm__ volatile("dsb sy\n" ::: "memory");
    return *(volatile uint32_t *)addr;
}

static inline uint64_t timeout_init_us(uint64_t us)
{
    return clock_cur_us() + us;
}

static inline bool timeout_elapsed(uint64_t tout)
{
    return clock_cur_us() > tout;
}

static inline void mmio_setbits_32(uintptr_t reg, uint32_t val)
{
    auto r = mmio_read_32(reg);
    r |= val;
    mmio_write_32(reg, r);
}

static inline void mmio_clrbits_32(uintptr_t reg, uint32_t val)
{
    auto r = mmio_read_32(reg);
    r &= ~val;
    mmio_write_32(reg, r);
}

static inline void mmio_clrsetbits_32(uintptr_t reg, uint32_t mask, uint32_t val)
{
    auto r = mmio_read_32(reg);
    r &= ~mask;
    r |= val;
    mmio_write_32(reg, r);
}

static inline void panic()
{
    klog("DDR: panic\n");
    while(true);
}

#include "cache.h"
static inline void clean_dcache_range(uintptr_t addr, size_t size)
{
    CleanA35Cache(addr, size, CacheType_t::Data, true);
}
static inline void inv_dcache_range(uintptr_t addr, size_t size)
{
    InvalidateA35Cache(addr, size, CacheType_t::Data, true);
}

#include "logger.h"
#define ERROR klog

#if DEBUG_TF_A_VERBOSE
#define VERBOSE klog
#else
#define VERBOSE(...)
#endif

#if DEBUG_TF_A_INFO
#define INFO klog
#else
#define INFO(...)
#endif

#include "scheduler.h"
static inline void mdelay(unsigned int ms)
{
    Block(clock_cur() + kernel_time_from_ms(ms));
}

static inline void zeromem(void *v, size_t sz)
{
    memset(v, 0, sz);
}

#endif
