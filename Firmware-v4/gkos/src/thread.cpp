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

    t->name = name;
    t->p = owning_process;
    t->base_priority = priority;

    // create a kernel stack for the thread
    t->mr_kernel_thread = vblock_alloc(VBLOCK_4M, false, true, false, GUARD_BITS_64k, GUARD_BITS_64k);
    if(!t->mr_kernel_thread.valid)
    {
        klog("thread_create: could not allocate kernel stack\n");
    }
    klog("thread: %s kernel stack at %llx - %llx\n", name.c_str(), t->mr_kernel_thread.data_start(),
        t->mr_kernel_thread.data_end());

    // ensure the entire kernel stack is allocated and mapped - page faults will try to use it
    vmem_map(t->mr_kernel_thread, InvalidPMemBlock());
    
    auto kthread_ptr = (uint64_t *)(t->mr_kernel_thread.data_end());

    const uint64_t spsr_el1_return = 5; // el1 with el1 stack
    
    // set up kernel thread for return to privileged process (TODO: alter for user code)
    for(auto i = 0U; i < 16U; i++)
    {
        *--kthread_ptr = 0U;    // Q0-7
    }
    *--kthread_ptr = (uint64_t)thread_stub;                // ELR_EL1
    *--kthread_ptr = (uint64_t)spsr_el1_return;     // SPSR_EL1
    for(auto i = 0U; i < 20U; i++)
    {
        *--kthread_ptr = 0U;    // GPRs
    }
    *--kthread_ptr = (uint64_t)p;       // X1
    *--kthread_ptr = (uint64_t)func;    // X0

    // set up task structure for kernel thread (TODO add userspace stuff)
    memset(&t->tss, 0, sizeof(thread_saved_state));
    t->tss.sp_el0 = 0;
    t->tss.sp_el1 = (uint64_t)kthread_ptr;

    klog("thread: sp_el1 = %llx\n", kthread_ptr);

    // TODO userspace tls structure


    {
        CriticalGuard cg(owning_process->sl);
        owning_process->threads.push_back(t);
    }

    return t;
}
