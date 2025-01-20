#define _GNU_SOURCE 1
#include "time.h"

#include "syscalls_int.h"
#include "scheduler.h"
#include "_gk_proccreate.h"
#include <cstring>
#include "elf.h"
#include "SEGGER_RTT.h"
#include "osmutex.h"
#include "clocks.h"
#include "cleanup.h"
#include "sync_primitive_locks.h"

extern Spinlock s_rtt;

int syscall_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_func)(void *), void *arg, int *_errno)
{
    if(!thread)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(!start_func)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(attr && !attr->is_initialized)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_BUFFER_R((void *)start_func, 1);

    auto curt = GetCurrentThreadForCore();
    auto &p = curt->p;

    auto stack_size = attr ? attr->stacksize : 0;
    if(!stack_size)
    {
        if(p.default_stack_size)
            stack_size = p.default_stack_size;
        else
            stack_size = 65536;
    }
    
    auto stack = memblk_allocate_for_stack((size_t)stack_size, CPUAffinity::Either,
        p.name + " new thread stack",
        p.stack_preference);
    if(!stack.valid)
    {
        // cannot allocate stack
        *_errno = EAGAIN;
        return -1;
    }

    auto t = Thread::Create("inproc", (Thread::threadstart_t)start_func, arg, curt->is_privileged,
        curt->base_priority, p, p.default_affinity, stack, curt->tss.mpuss);
    if(!t)
    {
        memblk_deallocate(stack);
        *_errno = EAGAIN;
        return -1;
    }

    auto id = (uint32_t)(uintptr_t)t;
    char tname[32];
    snprintf(tname, 31, "%s_%8X", p.name.c_str(), (unsigned int)id);
    tname[31] = 0;

    t->name = std::string(tname);

    *thread = (pthread_t)id;

    Schedule(t);

    return 0;
}

int syscall_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, int *_errno)
{
    if(!mutex)
    {   
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(mutex);
    if(attr)
        ADDR_CHECK_STRUCT_R(attr);

    bool is_recursive = attr && (attr->recursive || attr->type == PTHREAD_MUTEX_RECURSIVE);
    bool is_errorcheck = attr && (attr->type == PTHREAD_MUTEX_ERRORCHECK);
    
    auto m = new Mutex(is_recursive, is_errorcheck);
    if(!m)
    {
        *_errno = ENOMEM;
        return -1;
    }
    
    *reinterpret_cast<Mutex **>(mutex) = m;

    auto t = GetCurrentThreadForCore();
    add_sync_primitive(m, t->p.owned_mutexes, &t->p);
    return 0;
}

static bool check_mutex(pthread_mutex_t *mutex)
{
    if(!mutex)
        return false;
    if(*mutex == _PTHREAD_MUTEX_INITIALIZER)
    {
        syscall_pthread_mutex_init(mutex, nullptr, nullptr);
    }
    if(*mutex < 0x20000000U || *mutex >= 0x38010000u)
        return false;
    auto t = GetCurrentThreadForCore();
    auto ret = sync_primitive_exists(*(Mutex**)mutex, t->p.owned_mutexes, &t->p);
    if(!ret)
    {
        klog("pthread: mutex %08x invalid (Mutex = %08x)\n",
            (uint32_t)(uintptr_t)mutex,
            (uint32_t)(uintptr_t)*(Mutex **)mutex);
    }
    return ret;
}

static bool check_cond(pthread_cond_t *mutex)
{
    if(!mutex)
        return false;
    if(*mutex == _PTHREAD_COND_INITIALIZER)
    {
        syscall_pthread_cond_init(mutex, nullptr, nullptr);
    }
    if(*mutex < 0x20000000U || *mutex >= 0x38010000u)
        return false;
    auto t = GetCurrentThreadForCore();
    auto ret = sync_primitive_exists(*(Condition**)mutex, t->p.owned_conditions, &t->p);
    if(!ret)
    {
        klog("pthread: cond %08x invalid (Condition = %08x)\n",
            (uint32_t)(uintptr_t)mutex,
            (uint32_t)(uintptr_t)*(Condition **)mutex);
    }
    return ret;
}

static bool check_rwlock(pthread_rwlock_t *lock)
{
    if(!lock)
        return false;
    if(*lock == _PTHREAD_RWLOCK_INITIALIZER)
    {
        syscall_pthread_rwlock_init(lock, nullptr, nullptr);
    }
    if(*lock < 0x30000000U || *lock >= 0x38010000u)
        return false;
    auto t = GetCurrentThreadForCore();
    auto ret = sync_primitive_exists(*(RwLock**)lock, t->p.owned_rwlocks, &t->p);
    if(!ret)
    {
        klog("pthread: rwlock %08x invalid (RwLock = %08x)\n",
            (uint32_t)(uintptr_t)lock,
            (uint32_t)(uintptr_t)*(RwLock **)lock);
    }
    return ret;
}

int syscall_pthread_mutex_destroy(pthread_mutex_t *mutex, int *_errno)
{
    if(!check_mutex(mutex))
    {   
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(mutex);

    auto m = *reinterpret_cast<Mutex **>(mutex);
    auto ret = m->try_delete();
    if(ret)
    {
        auto t = GetCurrentThreadForCore();
        delete_sync_primitive(m, t->p.owned_mutexes, &t->p);

        delete m;

        return 0;
    }

    *_errno = EBUSY;
    return -1;
}

int syscall_pthread_mutex_trylock(pthread_mutex_t *mutex, int clock_id, const timespec *until, int *_errno)
{
    if(!check_mutex(mutex))
    {   
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(mutex);

    auto m = *reinterpret_cast<Mutex **>(mutex);

    bool block = clock_id != CLOCK_TRY_ONCE;
    if(clock_id >= 0)
        ADDR_CHECK_STRUCT_R(until);
    auto tout = kernel_time::from_timespec(until, clock_id);

    int reason;
    if(m->try_lock(&reason, block, tout))
    {
        return 0;
    }
    else
    {
        *_errno = reason;
        if(reason == EBUSY)
            return -3;      // Try again
        else
            return -1;      // hard fail
    }
}

int syscall_pthread_mutex_unlock(pthread_mutex_t *mutex, int *_errno)
{
    if(!check_mutex(mutex))
    {   
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(mutex);

    auto m = *reinterpret_cast<Mutex **>(mutex);
    if(m->unlock())
        return 0;
    else
    {   
        *_errno = EPERM;
        return -1;
    }
}

int syscall_pthread_rwlock_init(pthread_rwlock_t *lock, const pthread_rwlockattr_t *attr, int *_errno)
{
    if(!lock)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(lock);
    if(attr)
        ADDR_CHECK_STRUCT_R(attr);

    auto l = new RwLock();
    if(!l)
    {
        *_errno = ENOMEM;
        return -1;
    }
    
    *reinterpret_cast<RwLock **>(lock) = l;

    auto t = GetCurrentThreadForCore();
    add_sync_primitive(l, t->p.owned_rwlocks, &t->p);
    return 0;
}

int syscall_pthread_rwlock_tryrdlock(pthread_rwlock_t *lock, int clock_id, const timespec *until, int *_errno)
{
    if(!check_rwlock(lock))
    {   
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(lock);

    auto l = *reinterpret_cast<RwLock **>(lock);
    bool block = clock_id != CLOCK_TRY_ONCE;
    if(clock_id >= 0)
        ADDR_CHECK_STRUCT_R(until);
    auto tout = kernel_time::from_timespec(until, clock_id);

    int reason;
    if(l->try_rdlock(&reason, block, tout))
        return 0;
    else
    {
        *_errno = reason;
        if(reason == EBUSY)
            return -3;      // Try again
        else
            return -1;      // hard fail
    }
}

int syscall_pthread_rwlock_trywrlock(pthread_rwlock_t *lock, int clock_id, const timespec *until, int *_errno)
{
    if(!check_rwlock(lock))
    {   
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(lock);

    auto l = *reinterpret_cast<RwLock **>(lock);
    bool block = clock_id != CLOCK_TRY_ONCE;
    if(clock_id >= 0)
        ADDR_CHECK_STRUCT_R(until);
    auto tout = kernel_time::from_timespec(until, clock_id);


    int reason;
    if(l->try_wrlock(&reason, block, tout))
        return 0;
    else
    {
        *_errno = reason;
        if(reason == EBUSY)
            return -3;      // Try again
        else
            return -1;      // hard fail
    }
}

int syscall_pthread_rwlock_unlock(pthread_rwlock_t *lock, int *_errno)
{
    if(!check_rwlock(lock))
    {   
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(lock);

    auto l = *reinterpret_cast<RwLock **>(lock);
    if(l->unlock())
        return 0;
    else
    {
        *_errno = EPERM;
        return -1;
    }
}

int syscall_pthread_rwlock_destroy(pthread_rwlock_t *lock, int *_errno)
{
    if(!check_rwlock(lock))
    {   
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(lock);

    auto l = *reinterpret_cast<RwLock **>(lock);
    if(l->try_delete())
    {
        auto t = GetCurrentThreadForCore();
        delete_sync_primitive(l, t->p.owned_rwlocks, &t->p);

        *reinterpret_cast<RwLock **>(lock) = nullptr;
        delete l;

        return 0;
    }
    else
    {
        *_errno = EBUSY;
        return -1;
    }
}

int syscall_sem_init(sem_t *sem, int pshared, unsigned int value, int *_errno)
{
    if(!sem)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(sem);

    sem->s = new UserspaceSemaphore(value);
    if(!sem->s)
    {
        *_errno = ENOMEM;
        return -1;
    }

    auto t = GetCurrentThreadForCore();
    add_sync_primitive(sem->s, t->p.owned_semaphores, &t->p);

    return 0;
}

int syscall_sem_destroy(sem_t *sem, int *_errno)
{
    if(!sem || !sem->s)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(sem);

    int reason;
    if(!sem->s->try_delete(&reason))
    {
        *_errno = reason;
        return -1;
    }

    auto t = GetCurrentThreadForCore();
    delete_sync_primitive(sem->s, t->p.owned_semaphores, &t->p);

    delete sem->s;
    sem->s = nullptr;

    return 0;
}

int syscall_sem_getvalue(sem_t *sem, int *outval, int *_errno)
{
    if(!sem || !sem->s || !outval)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(sem);
    ADDR_CHECK_STRUCT_W(outval);

    *outval = (int)sem->s->get_value();
    return 0;
}

int syscall_sem_post(sem_t *sem, int *_errno)
{
    if(!sem || !sem->s)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(sem);

    sem->s->post();
    return 0;
}

int syscall_sem_trywait(sem_t *sem, int clock_id, const timespec *until, int *_errno)
{
    if(!sem || !sem->s)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(sem);

    bool block = clock_id != CLOCK_TRY_ONCE;
    if(clock_id >= 0)
        ADDR_CHECK_STRUCT_R(until);
    auto tout = kernel_time::from_timespec(until, clock_id);

    int reason;
    if(sem->s->try_wait(&reason, block, tout))
        return 0;
    else
    {
        *_errno = reason;
        if(reason == EBUSY)
            return -3;
        else
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
    ADDR_CHECK_STRUCT_W(key);
    if(destructor)
        ADDR_CHECK_BUFFER_R((void *)destructor, 1);

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

    ADDR_CHECK_STRUCT_W(retval);

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
    if(!cond)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(cond);
    if(attr)
        ADDR_CHECK_STRUCT_R(attr);

    auto c = new Condition();
    if(!c)
    {
        *_errno = ENOMEM;
        return -1;
    }
    
    *reinterpret_cast<Condition **>(cond) = c;

    auto t = GetCurrentThreadForCore();
    add_sync_primitive(c, t->p.owned_conditions, &t->p);
    return 0;
}

int syscall_pthread_cond_destroy(pthread_cond_t *cond, int *_errno)
{
    if(!check_cond(cond))
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(cond);

    auto c = *reinterpret_cast<Condition **>(cond);

    auto t = GetCurrentThreadForCore();
    delete_sync_primitive(c, t->p.owned_conditions, &t->p);

    delete c;
    return 0;
}

int syscall_pthread_cond_timedwait(pthread_cond_t *cond,
    pthread_mutex_t *mutex, const struct timespec *abstime,
    int *signalled, int *_errno)
{
    if(!check_cond(cond) || !check_mutex(mutex))
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(cond);
    ADDR_CHECK_STRUCT_W(mutex);
    if(abstime)
        ADDR_CHECK_STRUCT_R(abstime);
    if(signalled)
        ADDR_CHECK_STRUCT_W(signalled);

    auto c = *reinterpret_cast<Condition **>(cond);
    auto m = *reinterpret_cast<Mutex **>(mutex);
    m->unlock();

    kernel_time tout;
    if(abstime)
    {
        tout = kernel_time::from_timespec(abstime, CLOCK_REALTIME);
    }
    c->Wait(tout, signalled);
    return 0;
}

int syscall_pthread_cond_signal(pthread_cond_t *cond, int *_errno)
{
    if(!check_cond(cond))
    {
        *_errno = EINVAL;
        return -1;
    }

    ADDR_CHECK_STRUCT_W(cond);

    auto c = *reinterpret_cast<Condition **>(cond);

    c->Signal(false);
    return 0;
}

int syscall_pthread_join(Thread *thread, void **retval, int *_errno)
{
    // we need to make sure the thread hasn't already been destroyed
    auto t = GetCurrentThreadForCore();
    if(retval)
        ADDR_CHECK_STRUCT_W(retval);
    
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
            if(thread->join_thread && thread->join_thread != t)
            {
                *_errno = EDEADLK;
                return -1;
            }

            // else, tell the thread we are waiting for it to be destroyed
            thread->join_thread = t;
            thread->join_thread_retval = retval;

            return -2;
        }
    }
}

int syscall_pthread_exit(void **retval, int *_errno)
{
    ADDR_CHECK_STRUCT_R(retval);

    auto t = GetCurrentThreadForCore();
    {
        CriticalGuard cg(t->sl);

        t->retval = *retval;

        if(t->join_thread)
        {
            CriticalGuard cg2(t->join_thread->sl);
            if(t->join_thread_retval)
            {
                *t->join_thread_retval = *retval;
            }
            t->join_thread->blocking_on = nullptr;
            t->join_thread->set_is_blocking(false);
            if(t->join_thread->base_priority > t->base_priority) Yield();
            t->join_thread = nullptr;
        }

        t->for_deletion = true;

        CleanupQueue.Push({ .is_thread = true, .t = t });
    }
    return 0;
}

int syscall_pthread_setname_np(pthread_t thread, const char *name, int *_errno)
{
    if(!name)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_BUFFER_R(name, 1);
    if(strlen(name) > 64)
    {
        *_errno = ERANGE;
        return -1;
    }

    /* Check we can access the thread */
    auto treq = (Thread *)thread;
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    bool has_thread = false;
    for(const auto &tcheck : p.threads)
    {
        if(tcheck == treq)
        {
            has_thread = true;
            break;
        }
    }
    if(!has_thread)
    {
        *_errno = EINVAL;
        return -1;
    }

    treq->name = std::string(name);
    return 0;
}

int syscall_set_thread_priority(Thread *t, int priority, int *_errno)
{
    // clamp priorities
    if(priority >= GK_PRIORITY_VERYHIGH)
        priority = GK_PRIORITY_VERYHIGH - 1;
    if(priority <= GK_PRIORITY_IDLE)
        priority = GK_PRIORITY_IDLE + 1;
    
    if(priority == t->base_priority)
        return 0;

    auto from = t->base_priority;
    
#if GK_DUAL_CORE
    sched.ChangePriority(t, from, priority);
#elif GK_DUAL_CORE_AMP
    scheds[0].ChangePriority(t, from, priority);
    scheds[1].ChangePriority(t, from, priority);
#else
    sched.ChangePriority(t, from, priority);
#endif

    return 0;
}

int syscall_get_thread_priority(Thread *t, int *_errno)
{
    return t->base_priority;
}

static_assert(GK_PRIORITY_VERYHIGH >= (GK_PRIORITY_IDLE + 2));

int syscall_sched_get_priority_min(int policy, int *_errno)
{
    // for any policy we allow user threads to be 1-3
    return GK_PRIORITY_IDLE + 1;
}

int syscall_sched_get_priority_max(int policy, int *_errno)
{
    return GK_PRIORITY_VERYHIGH - 1;
}

int syscall_get_pthread_dtors(size_t *len, dtor_t *dtors, void **vals, int *_errno)
{
    ADDR_CHECK_STRUCT_W(len);
    
    auto t = GetCurrentThreadForCore();
    CriticalGuard cg_t(t->sl);
    auto &p = t->p;
    CriticalGuard cg_p(p.sl);

    if(*len < p.next_key)
    {
        *_errno = ENOMEM;
        *len = p.next_key;
        return -1;
    }

    if(dtors && vals)
    {
        ADDR_CHECK_BUFFER_W(dtors, p.next_key * sizeof(dtor_t));
        ADDR_CHECK_BUFFER_W(vals, p.next_key * sizeof(void *));

        for(pthread_key_t i = 0; i < p.next_key; i++)
        {
            auto dtor_iter = p.tls_data.find(i);
            auto vals_iter = t->tls_data.find(i);
            if(dtor_iter == p.tls_data.end() || vals_iter == t->tls_data.end())
            {
                // no data here
                dtors[i] = nullptr;
                vals[i] = nullptr;
            }
            else
            {
                dtors[i] = dtor_iter->second;
                vals[i] = vals_iter->second;
            }
        }
    }

    *len = p.next_key;
    return 0;
}
