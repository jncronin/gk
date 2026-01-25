.cpu cortex-a35

#include "gic_irq_nos.h"
#include "gk_conf.h"

/* The exception handling mechanism is different for AArch64 compared with cortex-M

    There is no list of addresses per-exception but rather a single page sized object
    containing actual code.
    
    Each EL[1:3] has an address loaded into VBAR_ELx
    
    This then contains code for the various exception types:
        Synchronous
        IRQ/vIRQ
        FIQ/vFIQ
        SError
      at the various privilege levels (see https://developer.arm.com/documentation/102412/0103/Handling-exceptions/Taking-an-exception)
    
    For EL3 exceptions occuring at EL3 (or indeed EL1 at 1, EL2 at 2) these functions start at
      offset 0x200 into the VBAR and each function is 32 bytes long.
    The only saved state is PSTATE into SPSR_ELx and return address into ELR_ELx (where x is the
      _handling_ EL)
      
    Thus, our handler functions need to save the corruptible registers and then branch to an
      actual handler, all within 32 bytes.  */

.section .vtors,"ax",%progbits
.global _vtors
.type _vtors, %object

// define a macro to generate branches outside this vtors section
.macro _vtor_tbl_entry handler
\handler\()_tbl:
    b \handler
.zero 128 - (. - \handler\()_tbl)
.endm

_vtors:

// skip EL0->EL0 handling
.zero 0x200

_vtor_tbl_entry _curel_sync
_vtor_tbl_entry _curel_irq
_vtor_tbl_entry _curel_fiq
_vtor_tbl_entry _curel_serror

// lower EL AArch64 -> EL3
_vtor_tbl_entry _lower64_sync
_vtor_tbl_entry _lower64_irq
_vtor_tbl_entry _lower64_fiq
_vtor_tbl_entry _lower64_serror

// lower EL AArch32 -> EL3 (TODO: implement service calls)
_vtor_tbl_entry _lower32_sync
_vtor_tbl_entry _lower32_irq
_vtor_tbl_entry _lower32_fiq
_vtor_tbl_entry _lower32_serror

.size _vtors, .-_vtors

// put the actual handlers in .text
.section .text

// save all registers
.macro save_regs
    // stack frame needs x0-x18, x29-30
    // this is 21 registers which would leave an unaligned stack, therefore round up to multiple of 16
    // gkos uses newlib which uses fpu registers in, e.g. memcmp, so save these as well
    // we also save spsr and elr in case we trigger another exception within the kernel (e.g. task switch)

    sub sp, sp, #320
    stp x0, x1, [sp, #16]
    stp x2, x3, [sp, #32]
    stp x4, x5, [sp, #48]
    stp x6, x7, [sp, #64]
    stp x8, x9, [sp, #80]
    stp x10, x11, [sp, #96]
    stp x12, x13, [sp, #112]
    stp x14, x15, [sp, #128]
    stp x16, x17, [sp, #144]
    stp x18, xzr, [sp, #160] // finish with a zero for alignment
    mrs x0, spsr_el1
    mrs x1, elr_el1
    stp x0, x1, [sp, #176]
    stp q0, q1, [sp, #192]
    stp q2, q3, [sp, #224]
    stp q4, q5, [sp, #256]
    stp q6, q7, [sp, #288]

    // aarch64 stack frame
    stp x29, x30, [sp, #0]
    mov x29, sp

#if GK_THREAD_LIST_IN_SYSRAM
    mrs x0, elr_el1
    bl thread_save_lr
#endif
.endm

.macro restore_regs
    msr daifset, #0b0010
    ldp x0, x1, [sp, #176]
    msr elr_el1, x1
    msr spsr_el1, x0
    ldp x0, x1, [sp, #16]
    ldp x2, x3, [sp, #32]
    ldp x4, x5, [sp, #48]
    ldp x6, x7, [sp, #64]
    ldp x8, x9, [sp, #80]
    ldp x10, x11, [sp, #96]
    ldp x12, x13, [sp, #112]
    ldp x14, x15, [sp, #128]
    ldp x16, x17, [sp, #144]
    ldr x18, [sp, #160]
    ldp q0, q1, [sp, #192]
    ldp q2, q3, [sp, #224]
    ldp q4, q5, [sp, #256]
    ldp q6, q7, [sp, #288]

    ldp x29, x30, [sp, #0]
    add sp, sp, #320
.endm

# sync exceptions need to extract svc #0 as task switch code and stay in assembly (or with known stack layout)
.macro sync_stub code
    save_regs

    mrs x0, esr_el1
    mov x1, x0
    bfc x1, #32, #32

    # 17/21 << 26 | 1 << 25 | 1     == SVC #1
    ldr x2, =0x56000001
    ldr x3, =0x46000001
    cmp x1, x2
    b.eq 1f
    cmp x1, x3
    b.eq 1f

    b 2f
1:
    # svc #1 instruction
    b TaskSwitch
    # does not return here - can branch to TaskSwitchEnd

2:
    # continue as per irq_stub, ignore the option to be in EL3
    mrs x1, far_el1
    mov x2, \code
    orr x2, x2, 1
    mov x3, sp
    mrs x4, elr_el1

    bl Exception_Handler
 
    restore_regs

    eret
.endm

.macro irq_stub code
    save_regs

# early identify a spurious irq (1023) or task switch irq (8 or 30)
    ldr x1, =GIC_INTERFACE_BASE         // GIC_INTERFACE_BASE
    ldr w0, [x1, #0x20]                 // non-secure IAR alias
    dmb ish
    and w2, w0, #1023

    cmp w2, #GIC_IRQ_SPURIOUS
    b.eq 3f

    cmp w2, #GIC_SGI_YIELD
    b.eq 2f

    cmp w2, #GIC_PPI_NS_PHYS
    b.eq 1f

    # none of the above - use the full interrupt handler
    mov x1, sp
    mrs x2, elr_el1
    bl gic_irq_handler

3:
    restore_regs
    
    eret

1:
    # task switches need to EOI prior to the switch, so do that here
    # timer interrupts are level triggered so need to mask the timer before EOI'ing
    mov x3, #3
    msr cntp_ctl_el0, x3

2:
    # EOI
    str w0, [x1, #0x24]
    dmb st

    b TaskSwitch
    # does not return here - can branch to TaskSwitchEnd
.endm

.macro other_stub code 
    save_regs

# get appropriate registers
    mrs x0, esr_el1
    mrs x1, far_el1
    mov x2, \code
    orr x2, x2, 1
    mov x3, sp
    mrs x4, elr_el1

    bl Exception_Handler

    restore_regs

    eret
.endm

.type _curel_sync,%function
_curel_sync:
    sync_stub 0x200
.size _curel_sync, .-_curel_sync

.type _curel_irq,%function
_curel_irq:
    irq_stub 0x280
.size _curel_irq, .-_curel_irq

.type _curel_fiq,%function
_curel_fiq:
    other_stub 0x300
.size _curel_fiq, .-_curel_fiq

.type _curel_serror,%function
_curel_serror:
    other_stub 0x380
.size _curel_serror, .-_curel_serror

.type _lower64_sync,%function
_lower64_sync:
    sync_stub 0x400
.size _lower64_sync, .-_lower64_sync

.type _lower64_irq,%function
_lower64_irq:
    irq_stub 0x480
.size _lower64_irq, .-_lower64_irq

.type _lower64_fiq,%function
_lower64_fiq:
    other_stub 0x500
.size _lower64_fiq, .-_lower64_fiq

.type _lower64_serror,%function
_lower64_serror:
    other_stub 0x580
.size _lower64_serror, .-_lower64_serror

.type _lower32_sync,%function
_lower32_sync:
    sync_stub 0x600
.size _lower32_sync, .-_lower32_sync

.type _lower32_irq,%function
_lower32_irq:
    irq_stub 0x680
.size _lower32_irq, .-_lower32_irq

.type _lower32_fiq,%function
_lower32_fiq:
    other_stub 0x700
.size _lower32_fiq, .-_lower32_fiq

.type _lower32_serror,%function
_lower32_serror:
    other_stub 0x780
.size _lower32_serror, .-_lower32_serror

.type TaskSwitchEnd,%function
.global TaskSwitchEnd
TaskSwitchEnd:
    restore_regs
    eret
.size TaskSwitchEnd, .-TaskSwitchEnd
