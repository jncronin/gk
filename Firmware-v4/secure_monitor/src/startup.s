.cpu cortex-a35

.section .text._kstart
.global _kstart
.type _kstart, %function

_kstart:
    // save startup parameters
    mov x24, x0
    mov x25, x1

    bl __libc_init_array

    // get saved parameters
    mov x0, x24
    mov x1, x25
    bl mp_kmain
1:
    wfi
    b 1b

.size _kstart,.-_kstart