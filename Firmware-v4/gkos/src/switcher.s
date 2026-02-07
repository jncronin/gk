#include "gk_conf.h"

.section .text.TaskSwitch
.global TaskSwitch
.type TaskSwitch,%function
TaskSwitch:
    bl GetNextThreadForCore
    mrs x1, tpidr_el1
    cmp x0, x1
    b.eq 4f             // if the same thread then do nothing
    cbz x1, 1f          // if zero in tpidr_el1 this is the first switch - don't save anything

    /* At this point we have saved on the stack:
        x0-x18
        x29-x30
        spsr_el1
        elr_el1
        q0-7, q16-31
        
       We therefore need to save to tss:
        x19-x28
        sp_el0
        sp_el1
        ttbr0_el1
        tpidr_el0
        q8-15
    */

    // save registers to [x1]
    mrs x2, sp_el0
    mov x3, sp
    stp x2, x3, [x1, #0]
    stp x19, x20, [x1, #16]
    stp x21, x22, [x1, #32]
    stp x23, x24, [x1, #48]
    stp x25, x26, [x1, #64]
    stp x27, x28, [x1, #80]
    mrs x2, ttbr0_el1
    str x2, [x1, #96]

    /* store FPU regs.  We don't use lazy save/restore here as
        it makes it more difficult to migrate a thread to the
        other core.  We could use eager save/lazy restore but
        other OSs have stopped doing this due to the high use
        of vectorization code in user space libraries. */
    stp q8, q9, [x1, #128]
    stp q10, q11, [x1, #160]
    stp q12, q13, [x1, #192]
    stp q14, q15, [x1, #224]

1:
    // load registers from [x0]
    ldp x2, x3, [x0, #0]
    msr sp_el0, x2
    mov sp, x3
    ldp x19, x20, [x0, #16]
    ldp x21, x22, [x0, #32]
    ldp x23, x24, [x0, #48]
    ldp x25, x26, [x0, #64]
    ldp x27, x28, [x0, #80]
    ldp x2, x3, [x0, #96]
    msr ttbr0_el1, x2
    msr tpidr_el0, x3

    // if ttbr0_el1 is 0, disable lower half paging to prevent translation table walk faults escalating to el3
    mrs x3, tcr_el1
    cbnz x2, 2f
    orr x3, x3, #(0x1 << 7)
    b 3f
2:
    bfc x3, #7, #1
3:
    msr tcr_el1, x3

#if GK_TLBI_AFTER_TTBR_CHANGE
    isb
    tlbi vmalle1is
    dsb ish
    isb
#endif

    // restore fpu
    ldp q8, q9, [x0, #128]
    ldp q10, q11, [x0, #160]
    ldp q12, q13, [x0, #192]
    ldp q14, q15, [x0, #224]

    // set thread pointer
    msr tpidr_el1, x0

    // we are now done with old thread, so can forget it
    // - this essentially releases a reference to the Thread shared_ptr
    bl SetNextThreadForCore

4:
    b TaskSwitchEnd     // restores interrupt registers

.size TaskSwitch, .-TaskSwitch
