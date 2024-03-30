#ifndef OSTYPES_H
#define OSTYPES_H

#include <cstddef>
#include <cstdint>

enum MemRegionType
{
    AXISRAM = 0,
    SRAM = 1,
    DTCM = 2,
    SDRAM = 3
};

enum CPUAffinity
{
    Either = 3,
    M7Only = 1,
    M4Only = 2,
    PreferM7 = 7,
    PreferM4 = 11
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
    mpu_saved_state mpuss[8];

    CPUAffinity affinity;           /* always second member, offset 180 */
    int running_on_core = 0;        /* offset 184 */
    int chosen_for_core = 0;        /* offset 188 */
    int deschedule_from_core = 0;   /* offset 192 */
};

#define GK_TSS_SIZE                 188
#define GK_TSS_MPUSS_OFFSET         108
#define GK_TSS_AFFINITY_OFFSET      172
#define GK_TSS_ROC_OFFSET           176
#define GK_TSS_CFC_OFFSET           180
#define GK_TSS_DFC_OFFSET           184

static_assert(sizeof(thread_saved_state) == GK_TSS_SIZE);
static_assert(offsetof(thread_saved_state, mpuss) == GK_TSS_MPUSS_OFFSET);
static_assert(offsetof(thread_saved_state, affinity) == GK_TSS_AFFINITY_OFFSET);
static_assert(offsetof(thread_saved_state, running_on_core) == GK_TSS_ROC_OFFSET);
static_assert(offsetof(thread_saved_state, chosen_for_core) == GK_TSS_CFC_OFFSET);
static_assert(offsetof(thread_saved_state, deschedule_from_core) == GK_TSS_DFC_OFFSET);

#endif
