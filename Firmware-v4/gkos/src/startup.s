.cpu cortex-a35

.section .text._kstart
.global _kstart
.type _kstart, %function

_kstart:
    // install vtors
    adr x2, _vtors
    msr vbar_el1, x2
    
    // startup parameters are passed via sp
    mov x24, sp

    // get actual stack
    ldr x1, =_estack
    mov sp, x1

    bl __libc_init_array

    // get parameters stored on initial stack
    ldp x0, x1, [x24]
    bl mp_kmain
1:
    wfi
    b 1b

.size _kstart,.-_kstart