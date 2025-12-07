/* support pthreads in privileged state - used for thread-aware libstdc++ */

#include "osmutex.h"
#include "gk_conf.h"
#include "logger.h"
#include "unistd.h"
#include "syscalls_int.h"

using pthread_mutex_t = id_t;

extern "C"
{
int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if(*mutex == _PTHREAD_MUTEX_INITIALIZER)
    {
        pthread_mutex_init(mutex, nullptr);
    }
    auto m = MutexList.Get(*mutex);
    if(!m)
    {
        return EINVAL;
    }
    m->lock();
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    if(*mutex == _PTHREAD_MUTEX_INITIALIZER)
    {
        pthread_mutex_init(mutex, nullptr);
    }
    auto m = MutexList.Get(*mutex);
    if(!m)
    {
        return EINVAL;
    }
    if(m->unlock())
        return 0;
    else
        return EPERM;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    attr->prio_ceiling = GK_NPRIORITIES - 1;
    attr->protocol = 0;
    attr->recursive = 0;
    attr->type = 0;
    attr->process_shared = 0;
    attr->is_initialized = 1;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
    attr->type = type;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    attr->is_initialized = 0;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
    pthread_mutexattr_t defattr;
    if(!attr || *mutex == _PTHREAD_MUTEX_INITIALIZER)
    {
        pthread_mutexattr_init(&defattr);
        attr = &defattr;
    }
    if(!attr->is_initialized)
    {
        return EINVAL;
    }

    bool is_recursive = attr && (attr->recursive || attr->type == PTHREAD_MUTEX_RECURSIVE);
    bool is_errorcheck = attr && (attr->type == PTHREAD_MUTEX_ERRORCHECK);
    
    auto m = MutexList.Create(is_recursive, is_errorcheck);
    if(!m)
    {
        return ENOMEM;
    }
    *mutex = m->id;
    return 0;
}

int pthread_once(void *, void *)
{
    klog("priv_pthread: pthread_once not implemented\n");
    while(true);
}

int pthread_cond_wait(void *, pthread_mutex_t *)
{
    klog("priv_pthread: pthread_cond_wait not implemented\n");
    while(true);
}

int pthread_cond_broadcast(void *)
{
    klog("priv_pthread: pthread_cond_broadcast not implemented\n");
    while(true);
}

/* Prior to scheduler start, use a temporary key database for p_kernel */
int next_pkernel_key = 0;

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
    if(GetCurrentThreadForCore() == nullptr)
    {
        *key = next_pkernel_key++;
        return 0;
    }
    int _errno;
    return syscall_pthread_key_create(key, destructor, &_errno);
}

int pthread_key_delete(pthread_key_t)
{
    klog("priv_pthread: pthread_key_delete not implemented\n");
    while(true);
}

void *pthread_getspecific(pthread_key_t)
{
    klog("priv_pthread: pthread_getspecific not implemented\n");
    while(true);
}

int pthread_setspecific(pthread_key_t, const void *)
{
    klog("priv_pthread: pthread_setspecific not implemented\n");
    while(true);
}

//void *__dso_handle = (void *)&__dso_handle;

__attribute__((section(".tdata._tls_stderr"))) int _tls_stderr = STDERR_FILENO;

}
