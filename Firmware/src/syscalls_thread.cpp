#include "syscalls_int.h"
#include "scheduler.h"
#include "_gk_proccreate.h"
#include <cstring>
#include "elf.h"
#include "SEGGER_RTT.h"
#include "osmutex.h"
#include "clocks.h"

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
        curt->base_priority, p, p.default_affinity, stack, curt->tss.mpuss[5], curt->tss.mpuss[6]);
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

    Schedule(t);

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
    int eret = ext4_fopen(&f, fname, "rb");     // TODO: shouldn't call this
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
    auto heap_size = pcinfo->heap_size;
    if(!heap_size) heap_size = 8192;

    auto core_affinity = pcinfo->core_mask & 3;
    if(core_affinity == 0) core_affinity = Either;
    core_affinity |= (pcinfo->prefer_core_mask & 0x3) << 2;

    // create argc/argv vector
    std::vector<std::string> params;
    for(int i = 0; i < pcinfo->argc; i++)
    {
        params.push_back(std::string(pcinfo->argv[i]));
    }

    Thread *startup_thread;
    Process *proc;
    eret = elf_load_memory((const void *)fbuf.address, cpname, params, heap_size, (CPUAffinity)core_affinity,
        &startup_thread, &proc);
    ext4_fclose(&f);
    memblk_deallocate(fbuf);
    if(eret != 0)
    {
        *_errno = EFAULT;
        return -1;
    }

    // See if we can allocate a larger heap now we have closed the file
    if(proc->heap.length < heap_size)
    {
        memblk_deallocate(proc->heap);

        uint32_t act_heap_size = heap_size;
        while(true)
        {
            proc->heap = memblk_allocate(act_heap_size, MemRegionType::AXISRAM);
            if(!proc->heap.valid)
            {
                proc->heap = memblk_allocate(act_heap_size, MemRegionType::SDRAM);
            }
            if(!proc->heap.valid)
            {
                act_heap_size /= 2;

                if(act_heap_size < 8192)
                {
                    __BKPT();
                    while(true);
                }
            }
            else
            {
                break;
            }
        }
        if(act_heap_size != heap_size)
        {
            SEGGER_RTT_printf(0, "proccreate: couldn't allocate heap size of %u, only %u available\n",
                heap_size, act_heap_size);
        }
    }

    // TODO: inherit fds

    // Set default pixel mode
    switch(pcinfo->pixel_format)
    {
        case GK_PIXELFORMAT_ARGB8888:
        case GK_PIXELFORMAT_RGB888:
        case GK_PIXELFORMAT_RGB565:
            proc->screen_pf = pcinfo->pixel_format;
            break;
        case GK_PIXELFORMAT_L8:
            // DMA2D cannot write to L8 buffers (but can read from them)
            proc->screen_pf = GK_PIXELFORMAT_RGB565;
            break;
        default:
            proc->screen_pf = GK_PIXELFORMAT_RGB565;  // something sensible
            break;
    }

    // Set as focus if possible
    if(pcinfo->with_focus)
    {
        focus_process = proc;
    }

    // schedule startup thread
    Schedule(startup_thread);

    return 0;
}

int syscall_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, int *_errno)
{
    if(!mutex || !attr)
    {   
        *_errno = EINVAL;
        return -1;
    }
    
    auto m = new Mutex(attr->recursive ? true : false);
    if(!m)
    {
        *_errno = ENOMEM;
        return -1;
    }
    
    *reinterpret_cast<Mutex **>(mutex) = m;
    return 0;
}

static bool check_mutex(pthread_mutex_t *mutex)
{
    if(!mutex)
        return false;
    if(*mutex < 0x38000000U || *mutex >= 0x38010000u)
        return false;
    return true;    // TODO: check against list of mutexes this process can access
}

int syscall_pthread_mutex_destroy(pthread_mutex_t *mutex, int *_errno)
{
    if(!check_mutex(mutex))
    {   
        *_errno = EINVAL;
        return -1;
    }

    auto m = *reinterpret_cast<Mutex **>(mutex);
    auto ret = m->try_delete();
    if(ret)
    {
        delete m;
        return 0;
    }

    *_errno = EBUSY;
    return -1;
}

int syscall_pthread_mutex_trylock(pthread_mutex_t *mutex, int *_errno)
{
    if(!check_mutex(mutex))
    {   
        *_errno = EINVAL;
        return -1;
    }

    auto m = *reinterpret_cast<Mutex **>(mutex);
    if(m->try_lock())
        return 0;
    else
        return -3;      // Try again
}

int syscall_pthread_mutex_unlock(pthread_mutex_t *mutex, int *_errno)
{
    if(!check_mutex(mutex))
    {   
        *_errno = EINVAL;
        return -1;
    }

    auto m = *reinterpret_cast<Mutex **>(mutex);
    if(m->unlock())
        return 0;
    else
    {   
        *_errno = EPERM;
        return -1;
    }
}

int syscall_pthread_key_create(pthread_key_t *key, void (*destructor)(void *), int *_errno)
{
    if(!key)
    {
        *_errno = EINVAL;
        return -1;
    }

    auto t = GetCurrentThreadForCore();
    {
        CriticalGuard cg_t(t->sl);

        auto &p = t->p;
        CriticalGuard cg_p(p.sl);

        auto ret = p.next_key++;
        p.tls_data[ret] = destructor;
        *key = ret;

        return 0;
    }
}

int syscall_pthread_key_delete(pthread_key_t key, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    CriticalGuard cg_t(t->sl);
    auto &p = t->p;
    CriticalGuard cg_p(p.sl);

    auto iter = p.tls_data.find(key);
    if(iter == p.tls_data.end())
    {
        *_errno = EINVAL;
        return -1;
    }
    p.tls_data.erase(iter);
    return 0;
}

int syscall_pthread_setspecific(pthread_key_t key, const void *val, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    CriticalGuard cg_t(t->sl);
    auto &p = t->p;
    CriticalGuard cg_p(p.sl);

    auto iter = p.tls_data.find(key);
    if(iter == p.tls_data.end())
    {
        *_errno = EINVAL;
        return -1;
    }

    t->tls_data[key] = const_cast<void *>(val);
    return 0;
}

int syscall_pthread_getspecific(pthread_key_t key, void **retval, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    CriticalGuard cg_t(t->sl);

    auto iter = t->tls_data.find(key);
    if(iter == t->tls_data.end())
    {
        *retval = nullptr;
    }
    else
    {
        *retval = iter->second;
    }
    return 0;
}

int syscall_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr, int *_errno)
{
    if(!cond || !attr)
    {
        *_errno = EINVAL;
        return -1;
    }

    auto c = new Condition();
    if(!c)
    {
        *_errno = ENOMEM;
        return -1;
    }
    
    *reinterpret_cast<Condition **>(cond) = c;
    return 0;
}

int syscall_pthread_cond_destroy(pthread_cond_t *cond, int *_errno)
{
    if(!check_mutex(cond))
    {
        *_errno = EINVAL;
        return -1;
    }

    auto c = *reinterpret_cast<Condition **>(cond);
    delete c;
    return 0;
}

int syscall_pthread_cond_timedwait(pthread_cond_t *cond,
    pthread_mutex_t *mutex, const struct timespec *abstime,
    int *signalled, int *_errno)
{
    if(!check_mutex(cond) || !check_mutex(mutex))
    {
        *_errno = EINVAL;
        return -1;
    }

    auto c = *reinterpret_cast<Condition **>(cond);
    auto m = *reinterpret_cast<Mutex **>(mutex);
    m->unlock();

    uint64_t tout = UINT64_MAX;
    if(abstime)
    {
        tout = clock_timespec_to_ms(*abstime);
    }
    c->Wait(tout, signalled);
    return 0;
}

int syscall_pthread_cond_signal(pthread_cond_t *cond, int *_errno)
{
    if(!check_mutex(cond))
    {
        *_errno = EINVAL;
        return -1;
    }

    auto c = *reinterpret_cast<Condition **>(cond);
    c->Signal(false);
    return 0;
}

int syscall_pthread_join(Thread *thread, void **retval, int *_errno)
{
    // we need to make sure the thread hasn't already been destroyed
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    {
        CriticalGuard cg_p(p.sl);

        bool thread_exists = false;
        for(auto curt : p.threads)
        {
            if(curt == thread)
            {
                thread_exists = true;
                break;
            }
        }

        if(!thread_exists)
        {
            *_errno = ESRCH;
            return -1;
        }

        // At this point the thread exists, check if it has already been destroyed
        {
            CriticalGuard cg_t(thread->sl);
            if(thread->for_deletion)
            {
                *retval = thread->retval;
                return 0;
            }

            // is anything else waiting?
            if(thread->join_thread)
            {
                *_errno = EDEADLK;
                return -1;
            }

            // else, tell the thread we are waiting for it to be destroyed
            thread->join_thread = t;
            thread->join_thread_retval = retval;

            t->is_blocking = true;
            t->block_until = 0;
            Yield();

            return 0;
        }
    }
}

int syscall_pthread_setname_np(pthread_t thread, const char *name, int *_errno)
{
    if(!name)
    {
        *_errno = EINVAL;
        return -1;
    }
    if(strlen(name) > 64)
    {
        *_errno = ERANGE;
        return -1;
    }
    if(thread > 64*1024U)
    {
        *_errno = EINVAL;
        return -1;
    }
    auto t = reinterpret_cast<Thread *>(0x38000000U + thread);
    t->name = std::string(name);
    return 0;
}
