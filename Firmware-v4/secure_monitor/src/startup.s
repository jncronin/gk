.cpu cortex-a35

.section .text._kstart
.global _kstart
.type _kstart, %function

_kstart:
    // save startup parameters
    mov x24, x0
    mov x25, x1

    // stop _ap_kstart being garbage collected
    adr x26, _ap_kstart

    // init stack space
    bl mp_preinit
    mov sp, x0

    // use our vtors
    adr x0, _vtors
    msr vbar_el3, x0

    bl __libc_init_array

    // get saved parameters
    mov x0, x24
    mov x1, x25
    bl mp_kmain
1:
    wfi
    b 1b

.size _kstart,.-_kstart

.section .data
.global AP_Start
.align 4
.type AP_Start, %object
AP_Start: .quad 0 
.size AP_Start, .-AP_Start

.section .text._ap_kstart
.global _ap_kstart
.type _ap_kstart,%function
_ap_kstart:
    ldr x3, =AP_Start
    mov x24, x0
    mov x25, x1

1:
    isb // (similar to x86 pause here)
    ldr x2, [x3]
    cmp x2, #0
    beq 1b
    
    bl ap_preinit
    mov sp, x0

    // use our vtors
    adr x0, _vtors
    msr vbar_el3, x0

    // call start
    mov x0, x24
    mov x1, x25
    bl ap_kmain
1:
    wfi
    b 1b

.size _ap_kstart, .-_ap_kstart
