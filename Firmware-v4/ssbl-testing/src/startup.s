.cpu cortex-a35

.section .text.Reset_Handler
.global Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    // exception handling
    adr x2, _vtors
    msr vbar_el3, x2

    // keep APs in WFI
    mrs x2, mpidr_el1
    and x2, x2, #0xff
    cmp x2, #0
    b.eq 1f
    b AP_Reset_Handler

1:
    // Stack init
    ldr x2, =_mp_estack
    mov sp, x2

    // Init .bss
    ldr x2, =_sbss
    mov x3, #0
    ldr x4, =_ebss

    b 2f

1:
    str x3, [x2], #8

2:
    cmp x2, x4
    bcc 1b

    // Init .data
    ldr x2, =_sdata
    ldr x3, =_sdata_flash
    ldr x4, =_edata

    b 4f
3:
    ldr x5, [x3], #8
    str x5, [x2], #8

4:
    cmp x2, x4
    bcc 3b

    // Save r0 (may be passed by bootrom)
    mov x4, x0

    // Init libc
    bl __libc_init_array

    // Main
    mov x0, x4
    bl main

5:
    b 5b

.size   Reset_Handler, .-Reset_Handler

.section .text.AP_Reset_Handler
.global AP_Reset_Handler
.type AP_Reset_Handler, %function

AP_Reset_Handler:
    // stack setup
    ldr x2, =_ap_estack
    mov sp, x2

    // Init .ap_text
    ldr x2, =_sap_text
    ldr x3, =_sap_text_flash
    ldr x4, =_eap_text

    b 4f
3:
    ldr x5, [x3], #8
    str x5, [x2], #8

4:
    cmp x2, x4
    bcc 3b

    dsb ish
    isb

    b AP_Hold

.size AP_Reset_Handler, .-AP_Reset_Handler

.section .data
.global AP_Target
.align 4
.type AP_Target, %object
AP_Target: .quad 0 
.size AP_Target, .-AP_Target


.section .ap_text, "ax", %progbits
.global AP_Hold
.type AP_Hold, %function

AP_Hold:
    ldr x0, =AP_Target
    ldr x2, =0x442d0018
    str xzr, [x0]

    sevl

1:
    isb // (similar to x86 pause here)
    wfe
    mov w3, #(1U << 6)
    str w3, [x2]
    mov w3, #(1U << (6 + 16))
    str w3, [x2]
    ldr x2, [x0]
    cmp x2, #0
    beq 1b
    br x2

.size AP_Hold, .-AP_Hold
