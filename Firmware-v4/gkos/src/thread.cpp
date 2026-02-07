#include "thread.h"
#include "vblock.h"
#include "process.h"
#include "vmem.h"
#include "threadproclist.h"
#include "cleanup.h"

void thread_stub(Thread::threadstart_t func, void *p)
{
    auto ret = func(p);
    auto t = GetCurrentKernelThreadForCore();
    Thread::Kill(t->id, ret);
    Yield();
    while(true)
    {
        Block();
    }
}

std::shared_ptr<Thread> Thread::Create(const std::string &name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            std::shared_ptr<Process> owning_process,
            void *p2)
{
    auto t = ThreadList.Create();

    klog("thread_create: %s @ %llx\n", name.c_str(), (uintptr_t)t.get());

    t->name = name;
    t->p = owning_process->id;
    t->base_priority = priority;
    t->is_privileged = is_priv;

#if GK_THREAD_LIST_IN_SYSRAM
    *(uint64_t *)(0xfffffd0030000000 + t->id * 0x10) = (uint64_t)(uintptr_t)t.get();
#endif

    if(!owning_process->is_privileged)
        t->is_privileged = false;

    // create a kernel stack for the thread
    t->mr_kernel_thread = vblock_alloc(VBLOCK_4M, false, true, false, GUARD_BITS_64k, GUARD_BITS_64k);
    if(!t->mr_kernel_thread.valid)
    {
        klog("thread_create: could not allocate kernel stack\n");
        return nullptr;
    }
    klog("thread: %s kernel stack at %llx - %llx\n", name.c_str(), t->mr_kernel_thread.data_start(),
        t->mr_kernel_thread.data_end());

    // ensure the entire kernel stack is allocated and mapped - page faults will try to use it
    vmem_map(t->mr_kernel_thread, InvalidPMemBlock());
    
    auto kthread_ptr = (uint64_t *)(t->mr_kernel_thread.data_end());

    const uint64_t spsr_el1_return = 5; // el1 with el1 stack
    const uint64_t spsr_el0_return = 0; // el0 with el0 stack

    /* Set up the SP/FP to link to themselves at the top of the stack. */
    *--kthread_ptr = 0;                 // recursive lr
    auto recursive_fp = --kthread_ptr;
    *recursive_fp = (uintptr_t)recursive_fp;
    // set up kernel thread for return to our thread_stub or a userspace provided function
    for(auto i = 0U; i < 48U; i++)
    {
        *--kthread_ptr = 0U;    // Q0-7, Q16-31
    }
    *--kthread_ptr = (is_priv ? (uint64_t)thread_stub : (uint64_t)func);         // ELR_EL1
    *--kthread_ptr = (uint64_t)(is_priv ? spsr_el1_return : spsr_el0_return);     // SPSR_EL1
    for(auto i = 0U; i < 18U; i++)
    {
        *--kthread_ptr = 0U;    // GPRs
    }
    *--kthread_ptr = (uint64_t)(is_priv ? p : p2);             // X1
    *--kthread_ptr = (is_priv ? (uint64_t)func : (uint64_t)p);      // X0
    *--kthread_ptr = 0;                 // LR
    *--kthread_ptr = (uintptr_t)recursive_fp;      // FP

    // set up task structure
    memset(&t->tss, 0, sizeof(thread_saved_state));
    t->tss.sp_el1 = (uint64_t)kthread_ptr;

    klog("thread: sp_el1 = %llx\n", kthread_ptr);

    // set up el0 stack
    if(!is_priv)
    {
        MutexGuard mg(owning_process->user_mem->m);

        t->mr_user_thread = owning_process->user_mem->vblocks.AllocAny(
            MemBlock::ZeroBackedReadWriteMemory(0, VBLOCK_4M, true, false, GUARD_BITS_64k), false);
        
        if(!t->mr_user_thread.valid)
        {
            klog("thread: could not allocate el0 stack\n");
            return nullptr;
        }
        auto uthread_ptr = t->mr_user_thread.data_end();
        klog("thread: sp_el0 = %llx\n", uthread_ptr);
        t->tss.sp_el0 = uthread_ptr;

        t->tss.ttbr0 = owning_process->user_mem->ttbr0;

        // userspace tls structure
        if(owning_process->vb_tls.valid)
        {
            t->mr_elf_tls = owning_process->user_mem->vblocks.AllocAny(
                MemBlock::TLSMemory(owning_process->vb_tls_data_size + 16,
                    owning_process->vb_tls.data_start()), false);

            if(!t->mr_elf_tls.valid)
            {
                klog("thread: could not allocate block for thread tls of size %llu\n",
                    owning_process->vb_tls_data_size + 16);
                return nullptr;
            }

            t->tss.tpidr_el0 = t->mr_elf_tls.data_start();
        }
    }

    {
        CriticalGuard cg(owning_process->sl);
        owning_process->threads.push_back(t->id);
    }

    return t;
}

bool Thread::addr_is_valid(const void *buf, size_t len, bool for_write) const
{
    /* Upper and lower halves are both 4 TiB in size.  Catch anything larger now to prevent
        overflow */
    if(len > LH_END)
        return false;
    
    auto start = (uintptr_t)buf;
    auto end = start + len;

    // TODO: check against actually assigned regions here - prevent kernel crashing with 
    //  page faults to areas that will never be mapped
    if(is_privileged)
    {
        if(start >= UH_START && end >= UH_START)
        {
            return true;
        }
    }
    if(start < LH_END && end < LH_END)
    {
        return true;
    }

    return false;
}

int Thread::assume_user_thread_lower_half(PThread user_thread)
{
    CriticalGuard cg(sl_lower_half_user_thread);
    if(is_privileged == false)
        return -1;
    if(user_thread == nullptr)
        return -2;
    if(user_thread->is_privileged == true)
        return -3;
    if(tss.ttbr0 != 0)
        return -4;
    if(lower_half_user_thread != 0)
        return -5;
    
    tss.ttbr0 = user_thread->tss.ttbr0;
    lower_half_user_thread = user_thread->id;
    uint64_t tcr_el1;
    __asm__ volatile(
        "msr ttbr0_el1, %[ttbr0]\n"
        "mrs %[tcr_el1], tcr_el1\n"
        "bfc %[tcr_el1], #7, #1\n"
        "msr tcr_el1, %[tcr_el1]\n"
        "isb\n"
#if GK_TLBI_AFTER_TTBR_CHANGE
        "tlbi vmalle1is\n"
        "dsb ish\n"
        "isb\n"
#endif
        :
        [tcr_el1] "=r" (tcr_el1)
        :
        [ttbr0] "r" (tss.ttbr0)
        :
        "memory");

    return 0;
}

int Thread::release_user_thread_lower_half()
{
    CriticalGuard cg(sl_lower_half_user_thread);
    if(is_privileged == false)
        return -1;
    if(lower_half_user_thread == 0)
        return -2;
    if(tss.ttbr0 == 0)
        return -3;

    tss.ttbr0 = 0;
    lower_half_user_thread = 0;

    uint64_t tcr_el1;

    __asm__ volatile(
        "msr ttbr0_el1, xzr\n"
        "mrs %[tcr_el1], tcr_el1\n"
        "orr %[tcr_el1], %[tcr_el1], #(0x1 << 7)\n"
        "msr tcr_el1, %[tcr_el1]\n"
        "isb\n"

#if GK_TLBI_AFTER_TTBR_CHANGE
        "tlbi vmalle1is\n"
        "dsb ish\n"
        "isb\n"
#endif
        :
        [tcr_el1] "=r" (tcr_el1)
        ::
        "memory"
    );

    return 0;
}

int Thread::Kill(id_t id, void *retval)
{
    auto pt = ThreadList.Get(id).v;

    if(!pt)
    {
        return -1;
    }

    klog("thread: kill %u (%s)\n", id, pt->name.c_str());

    CriticalGuard cg(ThreadList.sl, pt->sl);
    auto jt = ThreadList._get(pt->join_thread).v;

    if(jt)
    {
        CriticalGuard cg2(jt->sl);
        if(pt->join_thread_retval)
        {
            *pt->join_thread_retval = retval;
        }
        jt->blocking.unblock();
        signal_thread_woken(jt);
        pt->join_thread = 0;
    }

    ThreadList._setexitcode(id, retval);
    cg.unlock();
    
    CleanupQueue.Push(cleanup_message { .is_thread = true, .id = id });
    sched.Unschedule(pt);

    return 0;
}

Thread::~Thread()
{
    klog("THREAD DESTRUCTOR %u:%s @ %p\n", id, name.c_str(), this);
}

ThreadPrivilegeEscalationGuard::ThreadPrivilegeEscalationGuard()
{
    old_priv = GetCurrentThreadForCore()->is_privileged;
    GetCurrentThreadForCore()->is_privileged = true;
}

ThreadPrivilegeEscalationGuard::~ThreadPrivilegeEscalationGuard()
{
    GetCurrentThreadForCore()->is_privileged = old_priv;
}

#if GK_THREAD_LIST_IN_SYSRAM
extern "C" void thread_save_lr(uint64_t elr_el1)
{
    auto t = GetCurrentThreadForCore();
    if(t)
    {
        *(uint64_t *)(0xfffffd0030000000 + t->id * 0x10 + 8) = elr_el1;
    }
}

#endif
