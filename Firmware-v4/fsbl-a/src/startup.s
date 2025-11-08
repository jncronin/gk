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
    b.eq 2f
    b AP_Reset_Handler

2:
    // enable debug
    ldr x2, =0x44200584
    ldr w3, [x2]
    orr w3, w3, #(0x1 << 1)
    str w3, [x2]

    ldr x2, =0x44000e20
    ldr w3, =0xdeb60fff
    str w3, [x2]

    /* SERC allows invalid physical addresses to be translated to bus errors
     - essentially stops the debugger crashing the core when we inadvertantly
       view memory that we shouldn't */
    ldr x2, =0x442008b8
    ldr w3, [x2]
    orr w3, w3, #(0x1 << 1)
    str w3, [x2]

    ldr x2, =0x44080100
    mov w3, #0x1
    str w3, [x2]
    
    // system setup
    ldr x2, =_estack
    mov sp, x2

    // enable fpu (may be called from __libc_init_array)
/*    ldr r2,  =0xe000ed88      // CPACR
    ldr r3, [r2]             
    orr r3, r3, #(0xf << 20)
    str r3, [r2]
    dsb
    isb */

    // Init .bss
    ldr x2, =_sbss
    mov x3, #0
    ldr x4, =_ebss

    b 4f

3:
    str x3, [x2], #8

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

3:
    b 3b

.size   Reset_Handler, .-Reset_Handler

.section .text.AP_Reset_Handler

.global AP_Target
AP_Target: .quad 0 

.global AP_Reset_Handler
.type AP_Reset_Handler, %function

AP_Reset_Handler:
    ldr x2, =AP_Target
    mov x3, #0
    str x3, [x2]

1:
    isb // (similar to x86 pause here)
    ldr x2, =AP_Target
    ldr x2, [x2]
    cmp x2, #0
    beq 1b
    br x2

.size AP_Reset_Handler, .-AP_Reset_Handler
