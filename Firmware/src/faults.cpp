/* The stack state on exception entry is:
    SP      - R0
    SP + 4  - R1
    SP + 8  - R2
    SP + 12 - R3
    SP + 16 - R12
    SP + 20 - LR
    SP + 24 - Return address
    SP + 28 - xPSR

    To get the rest of the registers, we push onto the current stack (which may be different from
        the above stack) the values in gk_regs.
    
    We also push LR (EXC_RETURN) again to help with branching to GKHardFault, then 8 more registers.
    SP saved in R0.  Adjusted stack then becomes similar to the structure gk_regs */

#include <cstdint>
#include "logger.h"
#include "thread.h"
#include "scheduler.h"
#include "process.h"
#include "cleanup.h"

#define xstr(s) str(s)
#define str(s) #s

#define FAULT_BREAK     1
#ifdef FAULT_BREAK
#if FAULT_BREAK == 2
#define _FAULT_BREAK    __asm__ volatile ( "bkpt           \n" ::: "memory" );
#else
/* Determine if we are attached to a debugger */
#define _FAULT_BREAK    __asm__ volatile ( \
    "push   { r0, r1 }          \n"     \
    "ldr    r0, [pc, #12]       \n"     \
    "movs   r1, #1              \n"     \
    "ands   r0, r1              \n"     \
    "pop    { r0, r1 }          \n"     \
    "beq.n  .L0%=               \n"     \
    "bkpt   0x0000              \n"     \
    "b.n    .L0%=               \n"     \
    ".word  0xe000edf0          \n"     \
    ".L0%=:                     \n"     \
    ::: "memory");
#endif
#else
#define _FAULT_BREAK    
#endif

struct gk_aapcs_regs
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t ret_addr;
    uint32_t xpsr;
};

struct gk_regs
{
    gk_aapcs_regs *r;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t exc_return;
};

extern "C" {
    void GKHardFault(gk_regs *r);
    void GKMemManage(gk_regs *r);
    void GKBusFault(gk_regs *r);
    void GKUsageFault(gk_regs *r);
    void GKNMI(gk_regs *r);
}


extern "C" void HardFault_Handler() __attribute__((naked));
extern "C" void MemManage_Handler() __attribute__((naked));
extern "C" void BusFault_Handler() __attribute__((naked));
extern "C" void UsageFault_Handler() __attribute__((naked));
extern "C" void NMI_Handler() __attribute__((naked));

extern "C" INTFLASH_FUNCTION void HardFault_Handler()
{
    _FAULT_BREAK
    __asm__ volatile(
        "push { lr }        \n"
        "push { r4-r11 }    \n"
        "mov r0, lr         \n"
        "and r0, #0xf       \n"
        "subs r0, #0xd      \n"
        "cbz r0, .L0%=      \n"
        "b .L1%=            \n"
        "mrs r0, msp        \n"
        ".L0%=:             \n"
        "mrs r0, psp        \n"
        ".L1%=:             \n"
        "push { r0 }        \n"
        "mov r0, sp         \n"
        "bl GKHardFault     \n"
        "add sp, #36        \n"
        "pop { pc }         \n"
    ::: "memory");
}

extern "C" INTFLASH_FUNCTION void MemManage_Handler()
{
    _FAULT_BREAK
    __asm__ volatile(
        "push { lr }        \n"
        "push { r4-r11 }    \n"
        "mov r0, lr         \n"
        "and r0, #0xf       \n"
        "subs r0, #0xd      \n"
        "cbz r0, .L0%=      \n"
        "b .L1%=            \n"
        "mrs r0, msp        \n"
        ".L0%=:             \n"
        "mrs r0, psp        \n"
        ".L1%=:             \n"
        "push { r0 }        \n"
        "mov r0, sp         \n"
        "bl GKMemManage     \n"
        "add sp, #36        \n"
        "pop { pc }         \n"
    ::: "memory");
}

extern "C" INTFLASH_FUNCTION void BusFault_Handler()
{
    _FAULT_BREAK
    __asm__ volatile(
        "push { lr }        \n"
        "push { r4-r11 }    \n"
        "mov r0, lr         \n"
        "and r0, #0xf       \n"
        "subs r0, #0xd      \n"
        "cbz r0, .L0%=      \n"
        "b .L1%=            \n"
        "mrs r0, msp        \n"
        ".L0%=:             \n"
        "mrs r0, psp        \n"
        ".L1%=:             \n"
        "push { r0 }        \n"
        "mov r0, sp         \n"
        "bl GKBusFault      \n"
        "add sp, #36        \n"
        "pop { pc }         \n"
    ::: "memory");
}

extern "C" INTFLASH_FUNCTION void UsageFault_Handler()
{
    _FAULT_BREAK
    __asm__ volatile(
        "push { lr }        \n"
        "push { r4-r11 }    \n"
        "mov r0, lr         \n"
        "and r0, #0xf       \n"
        "subs r0, #0xd      \n"
        "cbz r0, .L0%=      \n"
        "b .L1%=            \n"
        "mrs r0, msp        \n"
        ".L0%=:             \n"
        "mrs r0, psp        \n"
        ".L1%=:             \n"
        "push { r0 }        \n"
        "mov r0, sp         \n"
        "bl GKUsageFault    \n"
        "add sp, #36        \n"
        "pop { pc }         \n"
    ::: "memory");
}

extern "C" INTFLASH_FUNCTION void NMI_Handler()
{
    _FAULT_BREAK
    __asm__ volatile(
        "push { lr }        \n"
        "push { r4-r11 }    \n"
        "mov r0, lr         \n"
        "and r0, #0xf       \n"
        "subs r0, #0xd      \n"
        "cbz r0, .L0%=      \n"
        "b .L1%=            \n"
        "mrs r0, msp        \n"
        ".L0%=:             \n"
        "mrs r0, psp        \n"
        ".L1%=:             \n"
        "push { r0 }        \n"
        "mov r0, sp         \n"
        "bl GKNMI           \n"
        "add sp, #36        \n"
        "pop { pc }         \n"
    ::: "memory");
}

extern "C" INTFLASH_FUNCTION void FPU_IRQHandler()
{
    BKPT();
    while(true);
}

static void end_process(Process &p)
{
    CriticalGuard cg(p.sl);
    for(auto pt : p.threads)
    {
        CriticalGuard cg2(pt->sl);
        pt->for_deletion = true;
        pt->set_is_blocking(true);
    }

    p.rc = 0;
    p.for_deletion = true;

    proc_list.DeleteProcess(p.pid, 0);

    CleanupQueue.Push({ .is_thread = false, .p = &p });

    Yield();
}

INTFLASH_FUNCTION static void log_regs(gk_regs *r, const char *fault_type)
{
    if((!(RCC->AHB5ENR & RCC_AHB5ENR_XSPI1EN)) || (!(RCC->AHB5ENR & RCC_AHB5ENR_XSPI2EN)) ||
        (!(XSPI1->CR & XSPI_CR_EN)) || (!(XSPI2->CR & XSPI_CR_EN)) ||
        ((XSPI1->CR & XSPI_CR_FMODE_Msk) != (3U << XSPI_CR_FMODE_Pos)) ||
        ((XSPI1->CR & XSPI_CR_FMODE_Msk) != (3U << XSPI_CR_FMODE_Pos)))
    {
        // hard fault prior to XSPI init
        BKPT();
    }
    auto t = GetCurrentThreadForCore();
    auto p = t ? &t->p : nullptr;
    auto tname = t ? t->name.c_str() : "unknown";
    auto pname = p ? p->name.c_str() : "unknown";

    auto r_addr = (uint32_t)(uintptr_t)r;
    bool r_valid = false;
    if(r_addr > 0 && r_addr < 0x30000U) r_valid = true;
    if(r_addr >= 0x20000000U && r_addr < 0x20030000U) r_valid = true;
    if(r_addr >= 0x24020000U && r_addr < 0x24072000U) r_valid = true;
    if(r_addr >= 0x30000000U && r_addr < 0x30008000U) r_valid = true;
    if(r_addr >= 0x90000000U && r_addr < 0x98000000U) r_valid = true;
    if(!r_valid) BKPT();

    klog("%s at %x called from %x\n"
        "\n"
        "Process: %s (0x%08x - 0x%08x)\n"
        "Thread: %s (0x%08x - 0x%08x)\n"
        "cfsr:  0x%08x        hfsr:  0x%08x\n"
        "mmfar: 0x%08x\n"
        "\n"
        "r0:    0x%08x        r1:    0x%08x\n"
        "r2:    0x%08x        r3:    0x%08x\n"
        "r4:    0x%08x        r5:    0x%08x\n"
        "r6:    0x%08x        r7:    0x%08x\n"
        "r8:    0x%08x        r9:    0x%08x\n"
        "r10:   0x%08x        r11:   0x%08x\n"
        "r12:   0x%08x        sp:    0x%08x\n"
        "xpsr:  0x%08x\n",
        fault_type, r->r->ret_addr, r->r->lr,
        pname, p->code_data.address, p->code_data.address + p->code_data.length,
        tname, t->stack.address, t->stack.address + t->stack.length,
        *(volatile uint32_t *)0xe000ed28, *(volatile uint32_t *)0xe000ed2c,
        *(volatile uint32_t *)0xe000ed34,
        r->r->r0, r->r->r1,
        r->r->r2, r->r->r3,
        r->r4, r->r5,
        r->r6, r->r7,
        r->r8, r->r9,
        r->r10, r->r11,
        r->r->r12, (uint32_t)r->r + 36,
        r->r->xpsr
        );

    for(unsigned int i = 0; i < 16; i++)
    {
        MPU->RNR = i;
        auto rbar = MPU->RBAR;
        auto rasr = MPU->RASR;

        auto t_rbar = t->tss.mpuss[i].rbar;
        auto t_rasr = t->tss.mpuss[i].rasr;

        // RBAR.VALID always reads as 0
        auto match = ((rbar & ~0x10U) == (t_rbar & ~0x10U)) && (rasr == t_rasr);

        klog("MPU    : %d: RBAR: 0x%08x, RASR: 0x%08x%s\n",
            i, rbar, rasr, match ? "" : " *** DIFFERENT FROM TSS ***");
        if(!match)
        {
            klog("MPU TSS: %d: RBAR: 0x%08x, RASR: 0x%08x\n", i, t_rbar, t_rasr);
        }
    }
}

static void handle_fault()
{
    auto t = GetCurrentThreadForCore();
    auto p = t ? &t->p : nullptr;
    if(p == &kernel_proc)
    {
        klog("Kernel process faulted.  System cannot be recovered.  Logs may be available in /kernel_fault.txt\n");
        log_freeze_persistent_log();

        /*if(CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
        {
            while(true)
            {
                __asm__ volatile ( "bkpt \n" ::: "memory");
            }
        }
        else*/
        {
            // trigger reset
            __BKPT();
            //SCB->AIRCR = (0x05faU << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk ;
        }
    }
    else if(p)
    {
        klog("Terminating process %s\n", p->name.c_str());
        end_process(*p);
        Yield();
    }
    else if(t)
    {
        klog("Unknown process, terminating thread instead\n");
        t->for_deletion = true;
        Yield();
    }
    else
    {
        klog("Unknown process and thread.  System cannot be recovered.  Logs may be available in /kernel_fault.txt\n");
        log_freeze_persistent_log();

        if(CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
        {
            while(true)
            {
                __asm__ volatile ( "bkpt \n" ::: "memory");
            }
        }
        else
        {
            // trigger reset
            BKPT();
            //SCB->AIRCR = (0x05faU << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk;
        }
    }
}

INTFLASH_FUNCTION void GKHardFault(gk_regs *r)
{
    log_regs(r, "Hard Fault");
    handle_fault();
}

INTFLASH_FUNCTION void GKMemManage(gk_regs *r)
{
    log_regs(r, "MemManage Fault");
    handle_fault();
}

INTFLASH_FUNCTION void GKBusFault(gk_regs *r)
{
    log_regs(r, "Bus Fault");
    handle_fault();
}

INTFLASH_FUNCTION void GKUsageFault(gk_regs *r)
{
    log_regs(r, "Usage Fault");
    handle_fault();
}

INTFLASH_FUNCTION void GKNMI(gk_regs *r)
{
    log_regs(r, "NMI");
    handle_fault();
}
