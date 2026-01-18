.syntax unified
.cpu cortex-m33
.thumb

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

.zero 128*4
.word  TIM6_IRQHandler



.weak      NMI_Handler
.thumb_set NMI_Handler,Default_Handler

.weak      HardFault_Handler
.thumb_set HardFault_Handler,Default_Handler

.weak      MemManage_Handler
.thumb_set MemManage_Handler,Default_Handler

.weak      BusFault_Handler
.thumb_set BusFault_Handler,Default_Handler

.weak      UsageFault_Handler
.thumb_set UsageFault_Handler,Default_Handler

.weak      SVC_Handler
.thumb_set SVC_Handler,Default_Handler

.weak      DebugMon_Handler
.thumb_set DebugMon_Handler,Default_Handler

.weak      PendSV_Handler
.thumb_set PendSV_Handler,Default_Handler

.weak      SysTick_Handler
.thumb_set SysTick_Handler,Default_Handler


.section  .text.Default_Handler,"ax",%progbits
Default_Handler:
    bl FailHandler
.size  Default_Handler, .-Default_Handler


.section .text.Reset_Handler
.global Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    ldr     sp, =_estack

    // enable ICACHE and DCACHE
    ldr r2, =0x40470000
    mov r3, 5
    str r3, [r2]
    //ldr r2, =0x40480000
    //mov r3, 1
    //str r3, [r2]

    // enable fpu (may be called from __libc_init_array)
    ldr r2,  =0xe000ed88      // CPACR
    ldr r3, [r2]             
    orr r3, r3, #(0xf << 20)
    str r3, [r2]
    dsb
    isb

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

    // Init .data
    ldr r2, =_sdata
    ldr r5, =_sdata_flash
    ldr r4, =_edata

    b 4f

3:
    ldr r3, [r5], #4
    str r3, [r2], #4

4:
    cmp r2, r4
    bcc 3b

    // Init libc
    bl __libc_init_array

    bl main

5:
    b 5b

.size   Reset_Handler, .-Reset_Handler
