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
}


extern "C" void HardFault_Handler() __attribute__((naked));
extern "C" void MemManage_Handler() __attribute__((naked));
extern "C" void BusFault_Handler() __attribute__((naked));
extern "C" void UsageFault_Handler() __attribute__((naked));

extern "C" void HardFault_Handler()
{
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

extern "C" void MemManage_Handler()
{
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

extern "C" void BusFault_Handler()
{
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

extern "C" void UsageFault_Handler()
{
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

static void log_regs(gk_regs *r, const char *fault_type)
{
    auto t = GetCurrentThreadForCore();
    auto p = t ? &t->p : nullptr;
    auto tname = t ? t->name.c_str() : "unknown";
    auto pname = p ? p->name.c_str() : "unknown";

    klog("%s at %x called from %x\n"
        "\n"
        "Process: %s\n"
        "Thread: %s\n"
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
        pname,
        tname,
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
            RCC->APB3ENR |= RCC_APB3ENR_WWDG1EN;
            (void)RCC->APB3ENR;
            RCC->GCR |= RCC_GCR_WW1RSC;
            WWDG1->CR = WWDG_CR_WDGA;    
            while(true);    
        }
    }
    else if(p)
    {
        klog("Terminating process %s\n", p->name.c_str());
        extern ProcessList proc_list;
        proc_list.DeleteProcess(p->pid, 0);
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
            RCC->APB3ENR |= RCC_APB3ENR_WWDG1EN;
            (void)RCC->APB3ENR;
            RCC->GCR |= RCC_GCR_WW1RSC;
            WWDG1->CR = WWDG_CR_WDGA;    
            while(true);    
        }
    }
}

void GKHardFault(gk_regs *r)
{
    log_regs(r, "Hard Fault");
    handle_fault();
}

void GKMemManage(gk_regs *r)
{
    log_regs(r, "MemManage Fault");
    handle_fault();
}

void GKBusFault(gk_regs *r)
{
    log_regs(r, "Bus Fault");
    handle_fault();
}

void GKUsageFault(gk_regs *r)
{
    log_regs(r, "Usage Fault");
    handle_fault();
}