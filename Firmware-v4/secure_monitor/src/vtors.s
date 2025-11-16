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

// save all registers
.macro save_regs
    // stack frame needs x0-x18, x29-30
    // this is 21 registers which would leave an unaligned stack, therefore round up to multiple of 16
    // ssbl does not use floating point registers so don't save
    sub sp, sp, #192
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x29, [sp, #144]
    stp x30, xzr, [sp, #160] // finish with a zero for alignment
    mrs x0, spsr_el1
    mrs x1, elr_el1
    stp x0, x1, [sp, #176]
.endm

.macro restore_regs
    ldp x0, x1, [sp, #176]
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
    ldp x18, x29, [sp, #144]
    ldr x30, [sp, #160]
    add sp, sp, #192
.endm

.macro exception_stub code 
    save_regs

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
