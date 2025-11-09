.cpu cortex-a35

.section .text._kstart
.global _kstart
.type _kstart, %function

_kstart:
    // startup parameters are passed in SP (via EL1_SP setup)
    mov x0, sp

    // get actual stack
    ldr x1, =_estack
    mov sp, x1

    bl __libc_init_array

    bl mp_kmain
1:
    wfi
    b 1b

.size _kstart,.-_kstart