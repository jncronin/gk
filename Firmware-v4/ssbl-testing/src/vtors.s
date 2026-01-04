.cpu cortex-a35

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

.macro save_regs
    // stack frame needs x0-x18, x29-30
    // this is 21 registers which would leave an unaligned stack, therefore round up to multiple of 16
    // gkos uses newlib which uses fpu registers in, e.g. memcmp, so save these as well
    // we also save spsr and elr in case we trigger another exception within the kernel (e.g. task switch)

    sub sp, sp, #336
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, xzr, [sp, #144] // finish with a zero for alignment
    mrs x0, spsr_el1
    mrs x1, elr_el1
    stp x0, x1, [sp, #160]
    stp xzr, xzr, [sp, #176]
    stp q0, q1, [sp, #192]
    stp q2, q3, [sp, #224]
    stp q4, q5, [sp, #256]
    stp q6, q7, [sp, #288]

    // aarch64 stack frame
    stp x29, x30, [sp, #320]
    mov x29, sp
    add x29, x29, #320
.endm

.macro restore_regs
    msr daifset, #0b0010
    ldp x0, x1, [sp, #160]
    msr elr_el1, x1
    msr spsr_el1, x0
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldr x18, [sp, #144]
    ldp q0, q1, [sp, #192]
    ldp q2, q3, [sp, #224]
    ldp q4, q5, [sp, #256]
    ldp q6, q7, [sp, #288]
    ldp x29, x30, [sp, #320]
    add sp, sp, #336
.endm

.macro exception_stub code 
    save_regs

# check for stack overflow
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cmp x0, #0
    beq 1f

    ldr x0, =_ap_sstack
    ldr x1, =_ap_estack
    beq 2f
1:
    ldr x0, =_mp_sstack
    ldr x1, =_mp_estack
2:
    mov x2, sp
    add x0, x0, #512
    cmp x2, x0
    blo 1f
    cmp x2, x1
    bhi 1f
    b 2f

1:
    // stack overflow
    wfi
    b 1b

2:
    // stack okay

# get appropriate registers
    mrs x0, CurrentEL
    cmp x0, #(1 << 2)
    beq 1f

    mrs x0, esr_el3
    mrs x1, far_el3
    mov x2, \code
    mov x3, sp
    mrs x4, elr_el3

    b 2f

1:
    mrs x0, esr_el1
    mrs x1, far_el1
    mov x2, \code
    orr x2, x2, 1
    mov x3, sp
    mrs x4, elr_el1

2:
    bl Exception_Handler

    # update return address if return value is not zero
    cmp x0, #0
    beq 1f
    msr elr_el3, x0
1:

    restore_regs

    eret
.endm

.type _curel_sync,%function
_curel_sync:
    exception_stub 0x200
.size _curel_sync, .-_curel_sync

.type _curel_irq,%function
_curel_irq:
    exception_stub 0x280
.size _curel_irq, .-_curel_irq

.type _curel_fiq,%function
_curel_fiq:
    exception_stub 0x300
.size _curel_fiq, .-_curel_fiq

.type _curel_serror,%function
_curel_serror:
    exception_stub 0x380
.size _curel_serror, .-_curel_serror

.type _lower64_sync,%function
_lower64_sync:
    exception_stub 0x400
.size _lower64_sync, .-_lower64_sync

.type _lower64_irq,%function
_lower64_irq:
    exception_stub 0x480
.size _lower64_irq, .-_lower64_irq

.type _lower64_fiq,%function
_lower64_fiq:
    exception_stub 0x500
.size _lower64_fiq, .-_lower64_fiq

.type _lower64_serror,%function
_lower64_serror:
    exception_stub 0x580
.size _lower64_serror, .-_lower64_serror

.type _lower32_sync,%function
_lower32_sync:
    exception_stub 0x600
.size _lower32_sync, .-_lower32_sync

.type _lower32_irq,%function
_lower32_irq:
    exception_stub 0x680
.size _lower32_irq, .-_lower32_irq

.type _lower32_fiq,%function
_lower32_fiq:
    exception_stub 0x700
.size _lower32_fiq, .-_lower32_fiq

.type _lower32_serror,%function
_lower32_serror:
    exception_stub 0x780
.size _lower32_serror, .-_lower32_serror

.type Read_SCR_EL3,%function
.global Read_SCR_EL3
Read_SCR_EL3:
    mrs x0, scr_el3
    ret
.size Read_SCR_EL3, .-Read_SCR_EL3

.type Write_SCR_EL3,%function
.global Write_SCR_EL3
Write_SCR_EL3:
    msr scr_el3, x0
    ret
.size Write_SCR_EL3, .-Write_SCR_EL3
