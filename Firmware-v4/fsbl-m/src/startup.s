.syntax unified
.cpu cortex-m33
.thumb

.section .text.Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    ldr     sp, =_estack

    // Init .bss
    ldr r2, =_sbss
    movs r3, #0
    ldr r4, =_ebss

    b 2f

1:
    str r3, [r2], #4

2:
    cmp r2, r4
    bcc 1b

    // Init libc
    bl __libc_init_array

    // Main
    bl main

3:
    b 3b

.size   Reset_Handler, .-Reset_Handler
