#include <stm32h7xx.h>

#include "thread.h"
#include "util.h"

extern "C" void PendSV_Handler()
{
    auto cpsr = DisableInterrupts();

    auto cur_t = GetCurrentThreadForCore();
    auto next_t = GetNextThreadForCore();

    if(cur_t == next_t)
    {
        // nothing to do
        RestoreInterrupts(cpsr);
        return;
    }

    {
        register auto cur_tss asm("r0") = &cur_t->tss;
        register auto next_tss asm("r1") = &cur_t->tss;
        register auto core_id asm("r2") = GetCoreID() * 4;
        register auto _cpsr asm("r3") = cpsr;

        // Perform task switch
        __asm volatile
        (
            "mrs r12, psp               \n"     // get psp
            "stmia r0!, { r12 }         \n"     // save psp
            "add r0, r0, #4             \n"     // skip saving control
            "stmia r0!, { r4-r11, lr }  \n"     // save other regs
            
            "tst lr, #0x10              \n"     // FPU lazy store?
            "it eq                      \n"
            "vstmiaeq r0!, {s16-s31}    \n"

            "add r0, r1, #108           \n"     // R0 = &cm7_mpu0
            "add r2, r2, r0             \n"     // R2 = &cm7_mpu0 or &cm4_mpu0
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

            : : "r"(cur_tss), "r"(next_tss), "r"(core_id), "r"(_cpsr) : "memory", "cc", "r12"
        );
    }
}
