#include "thread.h"
#include "vblock.h"
#include "process.h"
#include "vmem.h"

void thread_stub(Thread::threadstart_t func, void *p)
{
    auto ret = func(p);
    auto t = GetCurrentKernelThreadForCore();
    t->thread_retval = ret;

    // TODO call cleanup
    while(true)
    {
        __asm__ volatile("wfi\n" : : : "memory");
    }
}

std::shared_ptr<Thread> Thread::Create(const std::string &name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            std::shared_ptr<Process> owning_process)
{
    auto t = std::make_shared<Thread>();

    klog("thread_create: %s @ %llx\n", name.c_str(), (uintptr_t)t.get());

    t->name = name;
    t->p = owning_process;
    t->base_priority = priority;
    t->is_privileged = is_priv;

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
    // set up kernel thread for return to privileged process (TODO: alter for user code)
    *--kthread_ptr = 0;                 // LR
    *--kthread_ptr = (uintptr_t)recursive_fp;      // FP
    for(auto i = 0U; i < 16U; i++)
    {
        *--kthread_ptr = 0U;    // Q0-7
    }
    *--kthread_ptr = 0U;        // res2
    *--kthread_ptr = 0U;        // res1
    *--kthread_ptr = (is_priv ? (uint64_t)thread_stub : (uint64_t)func);         // ELR_EL1
    *--kthread_ptr = (uint64_t)(is_priv ? spsr_el1_return : spsr_el0_return);     // SPSR_EL1
    for(auto i = 0U; i < 18U; i++)
    {
        *--kthread_ptr = 0U;    // GPRs
    }
    *--kthread_ptr = (uint64_t)(is_priv ? p : nullptr);             // X1
    *--kthread_ptr = (is_priv ? (uint64_t)func : (uint64_t)p);      // X0

    // set up task structure
    memset(&t->tss, 0, sizeof(thread_saved_state));
    t->tss.sp_el1 = (uint64_t)kthread_ptr;

    klog("thread: sp_el1 = %llx\n", kthread_ptr);

    // set up el0 stack
    if(!is_priv)
    {
        CriticalGuard cg(owning_process->user_mem->sl);
        auto el0_stack = vblock_alloc(VBLOCK_4M, true, true, false, GUARD_BITS_64k, GUARD_BITS_64k,
            owning_process->user_mem->blocks);
        if(!el0_stack.valid)
        {
            klog("thread: could not allocate el0 stack\n");
            return nullptr;
        }
        auto uthread_ptr = el0_stack.data_end();
        klog("thread: sp_el0 = %llx\n", uthread_ptr);
        t->tss.sp_el0 = uthread_ptr;

        t->tss.ttbr0 = owning_process->user_mem->ttbr0;
        // TODO userspace tls structure
        //t->tss.tpidr_el0 = ...
    }



    {
        CriticalGuard cg(owning_process->sl);
        owning_process->threads.push_back(t);
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
