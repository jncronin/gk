#define _GNU_SOURCE 1
#include "time.h"

#include "syscalls_int.h"
#include "scheduler.h"
#include "_gk_proccreate.h"
#include <cstring>
#include "elf.h"
#include "osmutex.h"
#include "clocks.h"
#include "cleanup.h"
#include "sync_primitive_locks.h"
#include "threadproclist.h"

#define DEBUG_SYNC 0

int syscall_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_func)(void *), void *arg, void *arg2, int *_errno)
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
    auto p = curt->p;

    auto t = Thread::Create("inproc", (Thread::threadstart_t)start_func, arg, curt->is_privileged,
        curt->base_priority, p, arg2);
    if(!t)
    {
        *_errno = EAGAIN;
        return -1;
    }

    auto id = t->id;
    char tname[32];
    snprintf(tname, 31, "%s_%u", p->name.c_str(), id);
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
    
    auto m = MutexList.Create(is_recursive, is_errorcheck);
    if(!m)
    {
        *_errno = ENOMEM;
        return -1;
    }

#if DEBUG_SYNC
    klog("pthread_mutex: create mutex id %d\n", m->id);
#endif
    
    auto t = GetCurrentThreadForCore();
    *mutex = t->p->owned_mutexes.add(m);

    return 0;
}

static PMutex check_mutex(pthread_mutex_t *mutex)
{
#if DEBUG_SYNC
    klog("check_mutex: mutex = %llx\n", (uintptr_t)mutex);
#endif
    if(!mutex)
        return nullptr;
#if DEBUG_SYNC
    klog("check_mutex: *mutex = %u\n", *mutex);
#endif
    if(*mutex == _PTHREAD_MUTEX_INITIALIZER)
    {
        syscall_pthread_mutex_init(mutex, nullptr, nullptr);
#if DEBUG_SYNC
        klog("check_mutex: mutex_initialzer - init(), now id = %u\n", *mutex);
#endif
    }
    auto t = GetCurrentThreadForCore();
    return t->p->owned_mutexes.get(*mutex);
}

static PCondition check_cond(pthread_cond_t *mutex)
{
    if(!mutex)
        return nullptr;
    if(*mutex == _PTHREAD_COND_INITIALIZER)
    {
        syscall_pthread_cond_init(mutex, nullptr, nullptr);
    }
    auto t = GetCurrentThreadForCore();
    return t->p->owned_conditions.get(*mutex);
}

static PRWLock check_rwlock(pthread_rwlock_t *lock)
{
    if(!lock)
        return nullptr;
    if(*lock == _PTHREAD_RWLOCK_INITIALIZER)
    {
        syscall_pthread_rwlock_init(lock, nullptr, nullptr);
    }
    auto t = GetCurrentThreadForCore();
    return t->p->owned_rwlocks.get(*lock);
}

static PUserspaceSemaphore check_sem(sem_t *sem)
{
    if(!sem || !sem->s)
        return nullptr;
    auto t = GetCurrentThreadForCore();
    return t->p->owned_semaphores.get(sem->s);
}

int syscall_pthread_mutex_destroy(pthread_mutex_t *mutex, int *_errno)
{
    ADDR_CHECK_STRUCT_W(mutex);
    PMutex m = check_mutex(mutex);
    if(!m)
    {   
        *_errno = EINVAL;
        return -1;
    }

    auto ret = m->try_delete();
    if(ret)
    {
        auto t = GetCurrentThreadForCore();
        t->p->owned_mutexes.erase(*mutex);

        return 0;
    }

    *_errno = EBUSY;
    return -1;
}

int syscall_pthread_mutex_trylock(pthread_mutex_t *mutex, int clock_id, const timespec *until, int *_errno)
{
    ADDR_CHECK_STRUCT_W(mutex);
    PMutex m = check_mutex(mutex);
    if(!m)
    {   
        *_errno = EINVAL;
        return -1;
    }

    bool block = clock_id != CLOCK_TRY_ONCE;
    if(clock_id >= 0)
        ADDR_CHECK_STRUCT_R(until);
    auto tout = kernel_time_from_timespec(until, clock_id);

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
    ADDR_CHECK_STRUCT_W(mutex);
    PMutex m = check_mutex(mutex);
    if(!m)
    {   
        *_errno = EINVAL;
        return -1;
    }

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

    auto l = RwLockList.Create();
    if(!l)
    {
        *_errno = ENOMEM;
        return -1;
    }

    auto t = GetCurrentThreadForCore();
    *lock = t->p->owned_rwlocks.add(l);

    return 0;
}

int syscall_pthread_rwlock_tryrdlock(pthread_rwlock_t *lock, int clock_id, const timespec *until, int *_errno)
{
    ADDR_CHECK_STRUCT_W(lock);
    auto l = check_rwlock(lock);
    if(!l)
    {
        *_errno = EINVAL;
        return -1;
    }

    bool block = clock_id != CLOCK_TRY_ONCE;
    if(clock_id >= 0)
        ADDR_CHECK_STRUCT_R(until);
    auto tout = kernel_time_from_timespec(until, clock_id);

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
    ADDR_CHECK_STRUCT_W(lock);
    auto l = check_rwlock(lock);
    if(!l)
    {
        *_errno = EINVAL;
        return -1;
    }

    bool block = clock_id != CLOCK_TRY_ONCE;
    if(clock_id >= 0)
        ADDR_CHECK_STRUCT_R(until);
    auto tout = kernel_time_from_timespec(until, clock_id);


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
    ADDR_CHECK_STRUCT_W(lock);
    auto l = check_rwlock(lock);
    if(!l)
    {
        *_errno = EINVAL;
        return -1;
    }

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
    ADDR_CHECK_STRUCT_W(lock);
    auto l = check_rwlock(lock);
    if(!l)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(l->try_delete())
    {
        auto t = GetCurrentThreadForCore();
        t->p->owned_rwlocks.erase(*lock);
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

    auto s = UserspaceSemaphoreList.Create(value);
    if(!s)
    {
        *_errno = ENOMEM;
        return -1;
    }

    auto t = GetCurrentThreadForCore();
    sem->s = t->p->owned_semaphores.add(s);

    return 0;
}

int syscall_sem_destroy(sem_t *sem, int *_errno)
{
    ADDR_CHECK_STRUCT_W(sem);
    auto s = check_sem(sem);
    if(!s)
    {
        *_errno = EINVAL;
        return -1;
    }

    int reason;
    if(!s->try_delete(&reason))
    {
        *_errno = reason;
        return -1;
    }

    auto t = GetCurrentThreadForCore();
    t->p->owned_semaphores.erase(sem->s);

    return 0;
}

int syscall_sem_getvalue(sem_t *sem, int *outval, int *_errno)
{
    if(!outval)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(sem);
    ADDR_CHECK_STRUCT_W(outval);

    auto s = check_sem(sem);
    if(!s)
    {
        *_errno = EINVAL;
        return -1;
    }

    *outval = (int)s->get_value();
    return 0;
}

int syscall_sem_post(sem_t *sem, int *_errno)
{
    ADDR_CHECK_STRUCT_W(sem);
    auto s = check_sem(sem);
    if(!s)
    {
        *_errno = EINVAL;
        return -1;
    }

    s->post();
    return 0;
}

int syscall_sem_trywait(sem_t *sem, int clock_id, const timespec *until, int *_errno)
{
    ADDR_CHECK_STRUCT_W(sem);
    auto s = check_sem(sem);
    if(!s)
    {
        *_errno = EINVAL;
        return -1;
    }

    bool block = clock_id != CLOCK_TRY_ONCE;
    if(clock_id >= 0)
        ADDR_CHECK_STRUCT_R(until);
    auto tout = kernel_time_from_timespec(until, clock_id);

    int reason;
    if(s->try_wait(&reason, block, tout))
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

    auto &ptls = GetCurrentThreadForCore()->p->pthread_tls;
    CriticalGuard cg(ptls.sl);
    auto ret = ptls.next_key++;
    ptls.tls_data[ret] = destructor;
    *key = ret;
    return 0;
}

int syscall_pthread_key_delete(pthread_key_t key, int *_errno)
{
    auto &ptls = GetCurrentThreadForCore()->p->pthread_tls;
    CriticalGuard cg(ptls.sl);
    auto iter = ptls.tls_data.find(key);
    if(iter == ptls.tls_data.end())
    {
        *_errno = EINVAL;
        return -1;
    }
    ptls.tls_data.erase(iter);
    return 0;
}

int syscall_pthread_setspecific(pthread_key_t key, const void *val, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    auto &ptls = t->p->pthread_tls;

    CriticalGuard cg(t->sl_pthread_tls, ptls.sl);
    auto iter = ptls.tls_data.find(key);
    if(iter == ptls.tls_data.end())
    {
        *_errno = EINVAL;
        return -1;
    }

    t->tls_data[key] = const_cast<void *>(val);
    return 0;
}

int syscall_pthread_getspecific(pthread_key_t key, void **retval, int *_errno)
{
    ADDR_CHECK_STRUCT_W(retval);

    auto t = GetCurrentThreadForCore();
    CriticalGuard cg(t->sl_pthread_tls);

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
    ADDR_CHECK_STRUCT_W(cond);
    if(attr)
        ADDR_CHECK_STRUCT_R(attr);

    auto c = ConditionList.Create();
    if(!c)
    {
        *_errno = ENOMEM;
        return -1;
    }

    auto t = GetCurrentThreadForCore();
    *cond = t->p->owned_conditions.add(c);

    return 0;
}

int syscall_pthread_cond_destroy(pthread_cond_t *cond, int *_errno)
{
    ADDR_CHECK_STRUCT_W(cond);
    auto c = check_cond(cond);
    if(!c)
    {
        *_errno = EINVAL;
        return -1;
    }

    auto t = GetCurrentThreadForCore();
    t->p->owned_conditions.erase(*cond);

    return 0;
}

int syscall_pthread_cond_timedwait(pthread_cond_t *cond,
    pthread_mutex_t *mutex, const struct timespec *abstime,
    int *signalled, int *_errno)
{
    ADDR_CHECK_STRUCT_W(cond);
    ADDR_CHECK_STRUCT_W(mutex);
    auto c = check_cond(cond);
    auto m = check_mutex(mutex);
    if(!c || !m)
    {
        *_errno = EINVAL;
        return -1;
    }
    if(abstime)
        ADDR_CHECK_STRUCT_R(abstime);
    if(signalled)
        ADDR_CHECK_STRUCT_W(signalled);

    m->unlock();

    kernel_time tout;
    if(abstime)
    {
        tout = kernel_time_from_timespec(abstime, CLOCK_REALTIME);
    }
    c->Wait(tout, signalled);
    return 0;
}

int syscall_pthread_cond_signal(pthread_cond_t *cond, int *_errno)
{
    ADDR_CHECK_STRUCT_W(cond);
    auto c = check_cond(cond);
    if(!c)
    {
        *_errno = EINVAL;
        return -1;
    }

    c->Signal(false);
    return 0;
}

int syscall_pthread_join(pthread_t thread, void **retval, int *_errno)
{
    // we need to make sure the thread hasn't already been destroyed
    auto t = GetCurrentThreadForCore();
    if(retval)
        ADDR_CHECK_STRUCT_W(retval);
    
    auto curp = t->p;
    auto tthread = ThreadList.Get(thread);
    if(!thread || tthread->p != curp)
    {
        // no longer exists or doesn't belong to process
        *_errno = ESRCH;
        return -1;
    }

    // At this point the thread exists, check if it has already been destroyed
    {
        CriticalGuard cg_t(tthread->sl, ThreadList.sl);
        if(tthread->for_deletion)
        {
            *retval = tthread->retval;
            return 0;
        }

        // is anything else waiting?
        auto other_wait_thread = ThreadList._get(tthread->join_thread);
        if(other_wait_thread && other_wait_thread.get() != t)
        {
            *_errno = EDEADLK;
            return -1;
        }

        // else, tell the thread we are waiting for it to be destroyed
        tthread->join_thread = t->id;
        tthread->join_thread_retval = retval;
    }
    while(!t->ss.Wait(SimpleSignal::Set, 0));
    return t->ss_p.ival1;
}

int syscall_pthread_exit(void **retval, int *_errno)
{
    ADDR_CHECK_STRUCT_R(retval);

    auto t = GetCurrentThreadForCore();
    {
        CriticalGuard cg(t->sl, ThreadList.sl);

        t->retval = *retval;

        auto jt = ThreadList._get(t->join_thread);
        if(jt)
        {
            CriticalGuard cg2(jt->sl);
            if(t->join_thread_retval)
            {
                *t->join_thread_retval = *retval;
            }
            jt->blocking.unblock();
            signal_thread_woken(jt);
            t->join_thread = 0;
        }

        t->for_deletion = true;

        CleanupQueue.Push({ .is_thread = true, .t = GetCurrentPThreadForCore() });
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
    auto treq = ThreadList.Get(thread);
    auto t = GetCurrentThreadForCore();
    if(!treq || treq->p != t->p)
    {
        *_errno = EINVAL;
        return -1;
    }

    CriticalGuard cg(treq->sl);
    treq->name = std::string(name);
    return 0;
}

int syscall_set_thread_priority(pthread_t thread, int priority, int *_errno)
{
    // clamp priorities
    if(priority >= GK_PRIORITY_VERYHIGH)
        priority = GK_PRIORITY_VERYHIGH - 1;
    if(priority <= GK_PRIORITY_IDLE)
        priority = GK_PRIORITY_IDLE + 1;

    auto t = GetCurrentThreadForCore();
    auto tthread = ThreadList.Get(thread);
    if(!tthread || tthread->p != t->p)
    {
        *_errno = EINVAL;
        return -1;
    }
    
    if(priority == tthread->base_priority)
        return 0;

    auto from = tthread->base_priority;
    
    sched.ChangePriority(tthread, from, priority);

    return 0;
}

int syscall_get_thread_priority(pthread_t thread, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    auto tthread = ThreadList.Get(thread);
    if(!tthread || tthread->p != t->p)
    {
        *_errno = EINVAL;
        return -1;
    }
    return tthread->base_priority;
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
    auto &ptls = t->p->pthread_tls;
    CriticalGuard cg(t->sl_pthread_tls, ptls.sl);

    if(*len < ptls.next_key)
    {
        *_errno = ENOMEM;
        *len = ptls.next_key;
        return -1;
    }

    if(dtors && vals)
    {
        ADDR_CHECK_BUFFER_W(dtors, ptls.next_key * sizeof(dtor_t));
        ADDR_CHECK_BUFFER_W(vals, ptls.next_key * sizeof(void *));

        for(pthread_key_t i = 0; i < ptls.next_key; i++)
        {
            auto dtor_iter = ptls.tls_data.find(i);
            auto vals_iter = t->tls_data.find(i);
            if(dtor_iter == ptls.tls_data.end() || vals_iter == t->tls_data.end())
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

    *len = ptls.next_key;
    return 0;
}

int syscall_pthread_cleanup_push(void (*routine)(void *), void *arg, int *_errno)
{
    ADDR_CHECK_BUFFER_R((void *)routine, 4);

    auto t = GetCurrentThreadForCore();
    CriticalGuard cg(t->sl_pthread_tls);
    
    t->cleanup_list.push_back({ .routine = routine, .arg = arg });

    return 0;
}

int syscall_pthread_cleanup_pop(int execute, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    Thread::cleanup_handler ch { 0 };
    {
        CriticalGuard cg(t->sl_pthread_tls);
        if(t->cleanup_list.size() > 0)
        {
            ch = t->cleanup_list.back();
            t->cleanup_list.pop_back();
        }
    }
    if(execute && ch.routine)
    {
        // TODO: pass the function to execute back to the calling thread, if not privileged
        if(t->is_privileged)
        {
            ch.routine(ch.arg);
        }
    }

    return 0;
}
