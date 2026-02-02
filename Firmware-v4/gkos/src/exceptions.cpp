#include <cstdio>
#include <cstdint>
#include "logger.h"
#include "gic.h"
#include "vblock.h"
#include "vmem.h"
#include "thread.h"
#include "process.h"
#include "cache.h"
#include "pmem.h"
#include "syscalls_int.h"
#include "klog_buffer.h"

#define DEBUG_PF        0

static uint64_t TranslationFault_Handler(bool user, bool write, bool exec, uint64_t address, uint64_t el);

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
        if(ec == 0b100100 || ec == 0b100101 || ec == 0b100000 || ec == 0b100001)
        {
            // data/instruction abort - fake dfsc for instruction faults
            auto dfsc = (ec == 0b100000 || ec == 0b100001) ? 7ULL : (iss & 0x3fULL);

            if((dfsc >= 4 && dfsc <= 7) || (dfsc >= 12 && dfsc <= 15))
            {
                // page/permission fault

                bool user = (etype > 0x201) || (ec == 0b100100) || (ec == 0b100000);
                bool write = (iss & (1ULL << 6)) != 0;
                bool exec = (ec == 0b100000 || ec == 0b100001);

#if DEBUG_PF
                klog("EXCEPTION: type: %llx, esr: %llx, far: %llx, lr: %llx, sp: %llx, nested elr: %llx\n",
                    etype, esr, far, lr, (uint64_t)regs, regs->saved_elr_el1);
#endif

                userspace_fault_code = TranslationFault_Handler(user, write, exec, far, lr);
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
    auto [t, p] = GetCurrentThreadProcessForCore();
    if(t && p)
        klog("EXCEPTION: p: %s, t: %s, t*: %llx\n", p->name.c_str(), t->name.c_str(), (uintptr_t)t);

    // regs
    if(t && !t->is_privileged)
    {
        uint64_t sp_el0;
        __asm__ volatile("mrs %[sp_el0], sp_el0\n" : [sp_el0] "=r" (sp_el0) :: "memory");
        klog("EXCEPTION: SP0: %llx\n", sp_el0);
    }
    klog("EXCEPTION: FP: %llx, LR: %llx\n", regs->fp, regs->lr);
    klog("EXCEPTION: X0: %llx, X1: %llx\n", regs->x0, regs->x1);
    klog("EXCEPTION: X2: %llx, X3: %llx\n", regs->x2, regs->x3);
    klog("EXCEPTION: X4: %llx, X5: %llx\n", regs->x4, regs->x5);
    klog("EXCEPTION: X6: %llx, X7: %llx\n", regs->x6, regs->x7);
    klog("EXCEPTION: X8: %llx, X9: %llx\n", regs->x8, regs->x9);
    klog("EXCEPTION: X10: %llx, X11: %llx\n", regs->x10, regs->x11);
    klog("EXCEPTION: X12: %llx, X13: %llx\n", regs->x12, regs->x13);
    klog("EXCEPTION: X14: %llx, X15: %llx\n", regs->x14, regs->x15);
    klog("EXCEPTION: X16: %llx, X17: %llx\n", regs->x16, regs->x17);
    klog("EXCEPTION: X18: %llx\n", regs->x18);

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
        klogbuffer_purge_uart();
        while(true);
    }

    if(!t->is_privileged)
    {
        // required for yield()
        __asm__ volatile("msr daifclr, #0b0010\n" ::: "memory");
        if(p)
        {
            Process::Kill(p->id, 128 + userspace_fault_code);
        }
        while(true)
        {
            Yield();
        }
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

uint64_t TranslationFault_Handler(bool user, bool write, bool exec, uint64_t far, uint64_t lr)
{
#if DEBUG_PF
    klog("TranslationFault %s %s @ %llx from %llx\n", user ? "USER" : "SUPERVISOR",
        exec ? "EXEC" : (write ? "WRITE" : "READ"), far, lr);
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
        // This can all be interruptible
        __asm__ volatile("msr daifclr, #0b0010\n" ::: "memory");

        auto [t, p] = GetCurrentThreadProcessForCore();
        if(t == nullptr)
        {
            klog("pf: lower half from kernel init\n");
            return SupervisorThreadFault();
        }
        // check vblock for access
        auto umem = p->user_mem.get();
        if(umem == nullptr)
        {
            // we may be instead using a temporary lower half - try this
            if(t->lower_half_user_thread != 0)
            {
                auto othert = ThreadList.Get(t->lower_half_user_thread).v;
                if(othert)
                {
                    auto otherp = ProcessList.Get(othert->p).v;
                    if(otherp)
                    {
                        umem = otherp->user_mem.get();
                    }
                }
            }
            if(umem == nullptr)
            {
                klog("pf: lower half without a valid lower half vblock\n");
                return user ? UserThreadFault() : SupervisorThreadFault();
            }
        }

        MutexGuard mg(umem->m);
        auto &uvblock = umem->vblocks.IsAllocated(far);
        if(uvblock.b.valid == false)
        {
            klog("pf: invalid access, blocks:\n");
            umem->vblocks.Traverse([](MemBlock &mb) { klog("mblock: %p - %p\n", mb.b.base, mb.b.end()); return 0; });
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

        // Catch guard accesses
        if(far < uvblock.b.data_start() || far >= uvblock.b.data_end())
        {
            klog("pf: guard page hit\n");
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

        // Catch access to non-user pages
        if(user && !uvblock.b.user)
        {
            klog("pf: user access to supervisor page\n");
            return UserThreadFault();
        }

        // Catch write accessts to RO pages
        if(write && !uvblock.b.write)
        {
            klog("pf: write access to RO page\n");
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

        // Catch exec to NX pages
        if(exec && !uvblock.b.exec)
        {
            klog("pf: exec access to NX page\n");
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

        // Do we have a pte for the page?
        auto pte = vmem_get_pte(far, umem->ttbr0);
        uintptr_t paddr = 0;

        if((pte & DT_PAGE) == DT_PAGE)
        {
            // We do, use it
            paddr = pte & PAGE_PADDR_MASK;

            // break before make
            VMemBlock unmap_block;
            unmap_block.base = far & PAGE_VADDR_MASK;
            unmap_block.length = VBLOCK_64k;
            unmap_block.valid = true;
            vmem_unmap(unmap_block, umem->ttbr0, ~0ULL, false);
        }
        else
        {
            // Allocate one
            auto pmemret = Pmem.acquire(VBLOCK_64k);
            if(!pmemret.valid)
            {
                klog("pf: OOM\n");
                return user ? UserThreadFault() : SupervisorThreadFault();
            }
            paddr = pmemret.base;
        }

        /* map as writeable if:
            If subsequent access (pte != 0) then use block access
            If first access, only writeable if the current access is a write, else read */

        auto mret = vmem_map(far & PAGE_VADDR_MASK, paddr, uvblock.b.user, 
            pte ? uvblock.b.write : write,
            uvblock.b.exec,
            umem->ttbr0, ~0ULL, nullptr, uvblock.b.memory_type);
        if(mret != 0)
        {
            klog("pf: vmem_map failed %d\n", mret);
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

        // decide on the appropriate refill logic
        if(pte != 0)
        {
            if(((pte & PAGE_PRIV_MASK) == PAGE_PRIV_RW) ||
                ((pte & PAGE_PRIV_MASK) == PAGE_USER_RW))
            {
                // pte was already write, so this is a subsequent refill due to DT_PAGE being 0
                if(!uvblock.FillSubsequent)
                {
                    klog("pf: FillSubsequent not defined\n");
                    return user ? UserThreadFault() : SupervisorThreadFault();
                }
                uvblock.FillSubsequent(far & PAGE_VADDR_MASK, paddr, uvblock);
            }
        }
        else
        {
            // pte == 0
            if(!uvblock.FillFirst)
            {
                klog("pf: FillFirst not defined\n");
                return user ? UserThreadFault() : SupervisorThreadFault();
            }
            uvblock.FillFirst(far & PAGE_VADDR_MASK, paddr, uvblock);
        }

#if DEBUG_PF
        klog("pf: done\n");
#endif
        return 0;
    }

    while(true);
}
