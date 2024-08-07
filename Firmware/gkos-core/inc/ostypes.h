#ifndef OSTYPES_H
#define OSTYPES_H

#include <stddef.h>
#include <stdint.h>
#include "gk_conf.h"


#if GK_DUAL_CORE_AMP
#if GK_DUAL_CORE
#error Cannot specify both GK_DUAL_CORE_AMP and GK_DUAL_CORE
#endif
#endif

enum CPUAffinity
{
#if GK_DUAL_CORE_AMP
    Either = 1,
    PreferM7 = 1,
    M7Only = 1,
    M4Only = 2,
    PreferM4 = 2
#elif GK_DUAL_CORE
    Either = 3,
    M7Only = 1,
    M4Only = 2,
    PreferM7 = 7,
    PreferM4 = 11
#else
    Either = 1,
    M7Only = 1,
    M4Only = 1,
    PreferM7 = 1,
    PreferM4 = 1
#endif
};

enum MemRegionAccess
{
    NoAccess = 0,
    RO = 1,
    RW = 2
};

struct mpu_saved_state
{
    uint32_t rbar;
    uint32_t rasr;
};

struct thread_saved_state
{
    uint32_t psp;       /* All threads, regardless of privilege, use PSP */
    uint32_t control;   /* used to decide privileged vs unprivileged mode */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t lr;

    uint32_t fpuregs[16];
    struct mpu_saved_state mpuss[8];

    enum CPUAffinity affinity;           
    int running_on_core = 0;
    int chosen_for_core = 0;
    int deschedule_from_core = 0;
    int pinned_on_core = 0;
};

#define GK_TSS_SIZE                 192
#define GK_TSS_MPUSS_OFFSET         108
#define GK_TSS_AFFINITY_OFFSET      172
#define GK_TSS_ROC_OFFSET           176
#define GK_TSS_CFC_OFFSET           180
#define GK_TSS_DFC_OFFSET           184
#define GK_TSS_POC_OFFSET           188

#ifdef __cplusplus
static_assert(sizeof(thread_saved_state) == GK_TSS_SIZE);
static_assert(offsetof(thread_saved_state, mpuss) == GK_TSS_MPUSS_OFFSET);
static_assert(offsetof(thread_saved_state, affinity) == GK_TSS_AFFINITY_OFFSET);
static_assert(offsetof(thread_saved_state, running_on_core) == GK_TSS_ROC_OFFSET);
static_assert(offsetof(thread_saved_state, chosen_for_core) == GK_TSS_CFC_OFFSET);
static_assert(offsetof(thread_saved_state, deschedule_from_core) == GK_TSS_DFC_OFFSET);
static_assert(offsetof(thread_saved_state, pinned_on_core) == GK_TSS_POC_OFFSET);
#endif

#endif
