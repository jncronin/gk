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
