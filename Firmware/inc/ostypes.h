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
    mpu_saved_state cm7_mpu0, cm4_mpu0; /* MSP - varies depending on which core is running */
    mpu_saved_state mpuss[7];
};

static_assert(sizeof(thread_saved_state) == 45 * 4);
static_assert(offsetof(thread_saved_state, cm7_mpu0) == 108);

#endif
