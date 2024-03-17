#include "syscalls_int.h"
#include "scheduler.h"
extern Scheduler s;

int syscall_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_func)(void *), void *arg, int *_errno)
{
    if(!thread)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(!attr)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(!start_func)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(!attr->is_initialized)
    {
        *_errno = EINVAL;
        return -1;
    }

    auto stack_size = attr->stacksize;
    if(!stack_size)
        stack_size = 4096;
    if(stack_size > 8192)
        stack_size = 8192;
    
    auto stack = memblk_allocate_for_stack((size_t)stack_size, CPUAffinity::Either);
    if(!stack.valid)
    {
        // cannot allocate stack
        *_errno = EAGAIN;
        return -1;
    }

    auto curt = GetCurrentThreadForCore();
    auto p = curt->p;

    auto t = Thread::Create("inproc", (Thread::threadstart_t)start_func, arg, curt->is_privileged,
        curt->base_priority, p, CPUAffinity::Either, stack, curt->tss.mpuss[5], curt->tss.mpuss[6]);
    if(!t)
    {
        memblk_deallocate(stack);
        *_errno = EAGAIN;
        return -1;
    }

    auto id = 0x38000000U - (uint32_t)(uintptr_t)t;
    char tname[32];
    snprintf(tname, 31, "%s_%4X", p.name.c_str(), (unsigned int)id);
    tname[31] = 0;

    t->name = std::string(tname);

    *thread = (pthread_t)id;

    s.Schedule(t);

    return 0;
}

