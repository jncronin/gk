#include <stm32h7xx.h>

#include "thread.h"
#include "util.h"

#include "scheduler.h"
extern Scheduler s;

extern "C" void PendSV_Handler() __attribute__((naked));

extern "C" void PendSV_Handler()
{
    __asm
    (
        /* At entry, we have r0,r1,r2,r3 and r12 to use freely.  Need to save the rest if used */

        /* Disable interrupts, cpsr saved to R3 */
        "mrs r3, primask                \n"
        "cpsid i                        \n"

        /* Get CoreID */
        "push {r3}                      \n"
        "bl GetCoreID                   \n"
        "push {r0}                      \n"
        "push {r0}                      \n"

        /* Get current and next threads.  At this point, [SP], [SP+4] and r0 contain coreID */
        "bl GetNextThreadForCore        \n"
        "mov r1, r0                     \n"
        "pop {r0}                       \n"
        "push {r1}                      \n" /* [SP] is next_t, [SP+4] and r0 = coreID */
        "bl GetCurrentThreadForCore     \n"
        "pop {r1}                       \n"
        "pop {r2}                       \n"
        "pop {r3}                       \n"

        /* Now, R0 = cur_t, R1 = next_t, R2 = coreID, R3 = cpsr */

        /* If the threads are the same just exit */
        "tst r0, r1                     \n"
        "bne    .L0                     \n"
        "msr primask, r3                \n"
        "bx lr                          \n"
        ".L0:                           \n"

        /* Schedule current thread */
        "push {r0-r3}                   \n"
        "bl ScheduleThread              \n"
        "pop {r0-r3}                    \n"

        /* Set current_thread pointer to new one */
        "push {r0-r3}                   \n" /* can optimise to sp = sp - 16 */
        "mov r0, r1                     \n"
        "mov r1, r2                     \n"
        "bl SetNextThreadForCore        \n"
        "pop {r0-r3}                    \n"

        /* Perform task switch proper */

        /* First, save unsaved registers to tss */
        "mrs r12, psp               \n"     // get psp
        "stmia r0!, { r12 }         \n"     // save psp to tss
        "add r0, r0, #4             \n"     // skip saving control
        "stmia r0!, { r4-r11, lr }  \n"     // save other regs
        
        "tst lr, #0x10              \n"     // FPU lazy store?
        "it eq                      \n"
        "vstmiaeq r0!, {s16-s31}    \n"

        /* Set up new task MPU, can discard R0 now */
        "add r0, r1, #108           \n"     // R0 = &cm7_mpu0
        "add r2, r0, r2, lsl #2     \n"     // R2 = &cm7_mpu0 or &cm4_mpu0
        "add r0, r0, #8             \n"     // R0 = &mpuss[0]

        "ldr r4, =0xe000ed94        \n"     // MPU_CTRL
        "ldr r5, [r4]               \n"
        "bic r5, #1                 \n"     // disable mpu
        "str r5, [r4]               \n"

        "ldr r4, =0xe000ed9c        \n"     // R4 = RBAR
        "ldmia r2, {r5-r6}          \n"
        "ldmia r0!, {r7-r12}        \n"
        "stmia r4, {r5-r12}         \n"     // first 4 mpu regions
        "ldmia r0!, {r5-r12}        \n"
        "stmia r4, {r5-r12}         \n"     // second 4 mpu regions

        "ldr r4, =0xe000ed94        \n"     // MPU_CTRL
        "ldr r5, [r4]               \n"
        "orr r5, #1                 \n"     // enable mpu
        "str r5, [r4]               \n"
        "dsb                        \n"

        /* Load saved tss registers */
        "ldmia r1!, { r12 }         \n"     // load registers from tss
        "msr psp, r12               \n"
        "ldmia r1!, { r12 }         \n"
        "msr control, r12           \n"
        "ldmia r1!, { r4-r11, lr }  \n"

        "tst lr, #0x10              \n"     // FPU load only if lazy store
        "it eq                      \n"
        "vldmiaeq r1!, {s16-s31}    \n"

        "msr primask, r3            \n"     // restore interrupts
        "bx lr                      \n"     // return

        ::: "memory"
    );
}
