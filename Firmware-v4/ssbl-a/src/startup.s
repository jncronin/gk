.cpu cortex-a35

.section .vtors
.word _estack
.word Reset_Handler
.word  NMI_Handler
.word  HardFault_Handler
.word  MemManage_Handler
.word  BusFault_Handler
.word  UsageFault_Handler
.word  0
.word  0
.word  0
.word  0
.word  SVC_Handler
.word  DebugMon_Handler
.word  0
.word  PendSV_Handler
.word  SysTick_Handler

.weak      NMI_Handler
.set NMI_Handler,Default_Handler

.weak      HardFault_Handler
.set HardFault_Handler,Default_Handler

.weak      MemManage_Handler
.set MemManage_Handler,Default_Handler

.weak      BusFault_Handler
.set BusFault_Handler,Default_Handler

.weak      UsageFault_Handler
.set UsageFault_Handler,Default_Handler

.weak      SVC_Handler
.set SVC_Handler,Default_Handler

.weak      DebugMon_Handler
.set DebugMon_Handler,Default_Handler

.weak      PendSV_Handler
.set PendSV_Handler,Default_Handler

.weak      SysTick_Handler
.set SysTick_Handler,Default_Handler


.section  .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
    b  Infinite_Loop
.size  Default_Handler, .-Default_Handler


.section .text.Reset_Handler
.global Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    // keep APs in WFI
    mrs x2, mpidr_el1
    and x2, x2, #0xff
    cmp x2, #0
    b.eq 1f
    b AP_Reset_Handler

1:
    // Enable SRAM1 for our stack
    ldr x2, =0x442004f8
    ldr w3, [x2]
    orr w3, w3, #0x2
    str w3, [x2]

    // RISAB3 - enable secure data access
    ldr x2, =0x42110000
    ldr w3, [x2]
    orr w3, w3, 0x80000000
    str w3, [x2]

    ldr x2, =0x0e060000
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
    // Enable SRAM2 for our stack
    ldr x2, =0x442004fc
    ldr w3, [x2]
    orr w3, w3, #0x2
    str w3, [x2]

    // RISAB4 - enable secure data access for the whole thing
    ldr x2, =0x42120000
    ldr w3, [x2]
    orr w3, w3, 0x80000000
    str w3, [x2]

    // and secure instruction access for the first page
    ldr x2, =0x42120100
    mov w3, 0xff
    str w3, [x2]

    // stack setup
    ldr x2, =0x0e080000
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

.section .ap_text, "ax", %progbits
.global AP_Hold
.type AP_Hold, %function

AP_Hold:
    //wfi
    ldr x2, =0x442d0018
    mov w3, #(1U << 6)
    str w3, [x2]
    mov w3, #(1U << (6 + 16))
    str w3, [x2]
    b AP_Hold

.size AP_Hold, .-AP_Hold
