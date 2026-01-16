#include <cstdio>
#include <cstdint>
#include "logger.h"
#include "gic.h"
#include "vblock.h"
#include "vmem.h"
#include "thread.h"
#include "process.h"
#include "syscalls_int.h"

#define DEBUG_PF        0

static uint64_t TranslationFault_Handler(bool user, bool write, uint64_t address, uint64_t el);

extern "C" uint64_t Exception_Handler(uint64_t esr, uint64_t far,
    uint64_t etype, exception_regs *regs, uint64_t lr)
{
    int userspace_fault_code = 0;

    if(etype == 0x401 && (esr == 0x46000000 || esr == 0x56000000))
    {
        // syscalls run with interrupts enabled
        __asm__ volatile("msr daifclr, #0b0010\n" ::: "memory");
        SyscallHandler((syscall_no)regs->x0, (void *)regs->x1, (void *)regs->x2, (void *)regs->x3,
            regs->lr);
        return 0;
    }

    if(etype == 0x201 || etype == 0x401 || etype == 0x601)
    {
        // exception
        auto ec = (esr >> 26) & 0x3fULL;
        auto iss = esr & 0x1ffffffULL;
        if(ec == 0b100100 || ec == 0b100101)
        {
            // data abort

            auto dfsc = iss & 0x3fULL;

            if(dfsc >= 4 && dfsc <= 7)
            {
                // page fault

                bool user = (etype > 0x201) || (ec == 0b100100);
                bool write = (iss & (1ULL << 6)) != 0;

#if DEBUG_PF
                klog("EXCEPTION: type: %llx, esr: %llx, far: %llx, lr: %llx, sp: %llx, nested elr: %llx\n",
                    etype, esr, far, lr, (uint64_t)regs, regs->saved_elr_el1);
#endif

                userspace_fault_code = TranslationFault_Handler(user, write, far, lr);
                if(userspace_fault_code == 0)
                    return userspace_fault_code;
            }
        }
    }

    klog("EXCEPTION: type: %llx, esr: %llx, far: %llx, lr: %llx, sp: %llx, nested elr: %llx\n",
        etype, esr, far, lr, (uint64_t)regs, regs->saved_elr_el1);

    uint64_t ttbr0, ttbr1, tcr;
    __asm__ volatile("mrs %[ttbr0], ttbr0_el1\n"
        "mrs %[ttbr1], ttbr1_el1\n"
        "mrs %[tcr], tcr_el1\n"
    :
        [ttbr0] "=r" (ttbr0),
        [ttbr1] "=r" (ttbr1),
        [tcr] "=r" (tcr)
    : : "memory");
    klog("EXCEPTION: ttbr0: %llx, ttbr1: %llx, tcr: %llx\n", ttbr0, ttbr1, tcr);
    auto t = GetCurrentKernelThreadForCore();
    if(t)
        klog("EXCEPTION: p: %s, t: %s, t*: %llx\n", t->p->name.c_str(), t->name.c_str(), (uintptr_t)t);

    // backtrace
    uint64_t fp = regs->fp;
    int level = 1;
    klog("EXCEPTION: backtrace %3d: %16llx\n", 0, lr);
    while(true)
    {
        if(!vmem_vaddr_to_paddr_quick(fp) || !vmem_vaddr_to_paddr_quick(fp + 15))
            break;  // cannot read from fp/fp+8
        if(!vmem_vaddr_to_paddr_quick(*(uintptr_t *)(fp + 8)) || !vmem_vaddr_to_paddr_quick(*(uintptr_t *)(fp + 8) + 3))
            break;  // cannot read from lr
        klog("EXCEPTION: backtrace %3d: %16llx\n", level, *(uint64_t *)(fp + 8));
        auto new_fp = *(uint64_t *)fp;
        if(new_fp == fp) break;
        fp = new_fp;
        level++;
    }

    if(!t)
    {
        while(true);
    }

    if(!t->is_privileged)
    {
        t->p->Kill((void *)(128 + userspace_fault_code));
        return 0;
    }

    while(true);

    // we can change the address to return to by returning anything other than 0 here
    return 0;
}

static void DumpThreadFault()
{
    klog("Process: %s, Thread: %s @ %p\n",
        (GetCurrentThreadForCore() && GetCurrentProcessForCore()) ? GetCurrentProcessForCore()->name.c_str() : "<NULL>",
        GetCurrentThreadForCore() ? GetCurrentThreadForCore()->name.c_str() : "<NULL>",
        GetCurrentThreadForCore());
}

static uint64_t UserThreadFault(int sig = SIGSEGV)
{
    klog("User thread fault\n");
    DumpThreadFault();
    return sig;
}

static uint64_t SupervisorThreadFault(int sig = SIGSEGV)
{
    klog("Supervisor thread fault\n");
    DumpThreadFault();
    return ~0ULL;
}

uint64_t TranslationFault_Handler(bool user, bool write, uint64_t far, uint64_t lr)
{
#if DEBUG_PF
    klog("TranslationFault %s %s @ %llx from %llx\n", user ? "USER" : "SUPERVISOR",
        write ? "WRITE" : "READ", far, lr);
#endif

    if(far < VBLOCK_64k)
    {
        // catch null references
        return user ? UserThreadFault() : SupervisorThreadFault();
    }

    if(far >= 0x8000000000000000ULL)
    {
        if(user)
        {
            // user access to upper half
            return UserThreadFault();
        }

        // Check vblock for access
        auto be = vblock_valid(far);
        if(!be.valid)
        {
            return SupervisorThreadFault();
        }
        if(far < be.data_start() || far >= be.data_end())
        {
            klog("pf: guard page hit\n");
            return SupervisorThreadFault();
        }

#if DEBUG_PF
        klog("pf: lazy map %llx\n", far);
#endif

        if(vmem_map(far & ~(VBLOCK_64k - 1), 0, be.user, be.write, be.exec))
        {
            return SupervisorThreadFault();
        }
        return 0;
    }
    else
    {
        if(GetCurrentThreadForCore() == nullptr)
        {
            klog("pf: lower half from kernel init\n");
            return SupervisorThreadFault();
        }
        // check vblock for access
        auto umem = GetCurrentThreadForCore()->p->user_mem.get();
        if(umem == nullptr)
        {
            // we may be instead using a temporary lower half - try this
            if(GetCurrentThreadForCore()->lower_half_user_thread != nullptr)
            {
                umem = GetCurrentThreadForCore()->lower_half_user_thread->p->user_mem.get();
            }
            if(umem == nullptr)
            {
                klog("pf: lower half without a valid lower half vblock\n");
                return user ? UserThreadFault() : SupervisorThreadFault();
            }
        }

        VMemBlock be = InvalidVMemBlock();
        uint32_t be_tag;
        {
            CriticalGuard cg(umem->sl);
            be = vblock_valid(far, umem->blocks, &be_tag);
        }

        if(!be.valid)
        {
            klog("pf: user space vblock not found\n");
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

        if(far < be.data_start() || far >= be.data_end())
        {
            klog("pf: guard page hit\n");
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

#if DEBUG_PF
        klog("pf: lazy map %llx\n", far);
#endif

        {
            CriticalGuard cg(umem->sl);

            if(vmem_map(far & ~(VBLOCK_64k - 1), 0, be.user, be.write, be.exec, umem->ttbr0))
            {
                return user ? UserThreadFault() : SupervisorThreadFault();
            }
        }

        if(be_tag & VBLOCK_TAG_TLS)
        {
            // need to copy tls data into the block
            auto dest_offset = far - be.data_start();
            auto dest_page = dest_offset / VBLOCK_64k;

            uintptr_t src_pointer;
            size_t src_size;
            {
                auto p = GetCurrentProcessForCore();
                CriticalGuard cg(p->sl);
                src_pointer = p->vb_tls.data_start();
                src_size = p->vb_tls_data_size;
            }

            auto src_offset = dest_page * VBLOCK_64k - 16;
            auto dst_page_offset = 0;

            if(dest_page == 0)
            {
                auto dst = (volatile uint64_t *)be.data_start();
                dst[0] = 0;     // should point to DTV (see ELF Handling for TLS) but for now catch accesses
                dst[1] = 0;     // reserved

                src_offset += 16;
                dst_page_offset += 16;
            }

            auto bytes_left = src_size - src_offset;
            auto to_copy = std::min(bytes_left, VBLOCK_64k - dst_page_offset);

            memcpy((void *)(be.data_start() + dest_page * VBLOCK_64k + dst_page_offset),
                (const void *)(src_pointer + src_offset), to_copy);

#if DEBUG_PF
            klog("pf: lazy initialize tls region\n");
#endif
        }

#if DEBUG_PF
        klog("pf: done\n");
#endif
        return 0;
    }

    while(true);
}
