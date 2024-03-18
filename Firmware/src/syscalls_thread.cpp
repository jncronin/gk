#include "syscalls_int.h"
#include "scheduler.h"
#include "_gk_proccreate.h"
#include <cstring>
#include "elf.h"
#include "SEGGER_RTT.h"

extern Scheduler s;
extern Spinlock s_rtt;

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
    auto &p = curt->p;

    auto t = Thread::Create("inproc", (Thread::threadstart_t)start_func, arg, curt->is_privileged,
        curt->base_priority, p, CPUAffinity::Either, stack, curt->tss.mpuss[5], curt->tss.mpuss[6]);
    if(!t)
    {
        memblk_deallocate(stack);
        *_errno = EAGAIN;
        return -1;
    }

    auto id = (uint32_t)(uintptr_t)t - 0x38000000U;
    char tname[32];
    snprintf(tname, 31, "%s_%4X", p.name.c_str(), (unsigned int)id);
    tname[31] = 0;

    t->name = std::string(tname);

    *thread = (pthread_t)id;

    s.Schedule(t);

    return 0;
}

int syscall_proccreate(const char *fname, const proccreate_t *pcinfo, int *_errno)
{
    if(!fname)
    {
        *_errno = EINVAL;
        return -1;
    }
    if(!pcinfo)
    {
        *_errno = EINVAL;
        return -1;
    }

    // get last part of path to use as process name
    const char *pname = fname;
    auto lastslash = strrchr(fname, '/');
    if(lastslash && strlen(lastslash + 1))
        pname = lastslash + 1;
    
    // load up the file
    ext4_file f;
    int eret = ext4_fopen(&f, fname, "rb");
    if(eret != EOK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "process_create: ext4_fopen failed %d\n", eret);
        *_errno = EFAULT;
        return -1;
    }

    auto fsize = ext4_fsize(&f);
    if(fsize > UINT32_MAX)
    {
        *_errno = E2BIG;
        return -1;
    }
    auto fsize2 = static_cast<uint32_t>(fsize);

    auto fbuf = memblk_allocate_for_stack(fsize2, CPUAffinity::Either); // ensure in AXISRAM or SDRAM
    if(!fbuf.valid)
    {
        *_errno = ENOMEM;
        ext4_fclose(&f);
        return -1;
    }

    size_t rcnt;
    eret = ext4_fread(&f, (void *)fbuf.address, fsize2, &rcnt);
    if(eret != EOK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "process_create: ext4_fread failed %d\n", eret);
        *_errno = EFAULT;
        ext4_fclose(&f);
        memblk_deallocate(fbuf);
        return -1;
    }
    if(rcnt != fsize2)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "process_create: ext4_fread did not read enough %d vs %d\n", rcnt, fsize2);
        *_errno = EFAULT;
        ext4_fclose(&f);
        memblk_deallocate(fbuf);
        return -1;
    }

    // load from memory
    std::string cpname(pname);
    eret = elf_load_memory((const void *)fbuf.address, cpname);
    ext4_fclose(&f);
    memblk_deallocate(fbuf);
    if(eret != 0)
    {
        *_errno = EFAULT;
        return -1;
    }

    return 0;
}
