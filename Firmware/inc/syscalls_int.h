#ifndef SYSCALLS_INT_H
#define SYSCALLS_INT_H

#include <sys/types.h>
#include <sys/stat.h>

#include "thread.h"
#include "scheduler.h"
#include "_netinet_in.h"
#include "_gk_event.h"
#include "_sys_dirent.h"

struct sem_t
{
    UserspaceSemaphore *s;
};

int get_free_fildes(Process &p);

int syscall_fstat(int file, struct stat *st, int *_errno);
int syscall_write(int file, char *buf, int nbytes, int *_errno);
int syscall_read(int file, char *buf, int nbytes, int *_errno);
int syscall_isatty(int file, int *_errno);
off_t syscall_lseek(int file, off_t offset, int whence, int *_errno);
int syscall_open(const char *pathname, int flags, int mode, int *_errno);
int syscall_unlink(const char *pathname, int *_errno);
int syscall_ftruncate(int file, off_t length, int *_errno);
int syscall_close1(int file, int *_errno);
int syscall_close2(int file, int *_errno);
int syscall_socket(int domain, int type, int protocol, int *_errno);
int syscall_bind(int sockfd, const sockaddr *addr, socklen_t addrlen, int *_errno);
int syscall_listen(int sockfd, int backlog, int *_errno);
int syscall_accept(int sockfd, sockaddr *addr, socklen_t *addrlen, int *_errno);
int syscall_recvfrom(int sockfd, void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen, int *_errno);
int syscall_sendto(int sockfd, const void *buf, size_t len, int flags,
    const sockaddr *dest_addr, socklen_t addrlen, int *_errno);

int syscall_gettimeofday(struct timeval *tv, struct timezone *tz, int *_errno);

int syscall_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_func)(void *), void *arg, int *_errno);
int syscall_proccreate(const char *fname, const proccreate_t *proc_info, pid_t *pid, int *_errno);

int syscall_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, int *_errno);
int syscall_pthread_mutex_destroy(pthread_mutex_t *mutex, int *_errno);
int syscall_pthread_mutex_trylock(pthread_mutex_t *mutex, int clock_id, const timespec *until, int *_errno);
int syscall_pthread_mutex_unlock(pthread_mutex_t *mutex, int *_errno);

int syscall_pthread_key_create(pthread_key_t *key, void (*destructor)(void *), int *_errno);
int syscall_pthread_getspecific(pthread_key_t key, void **retval, int *_errno);
int syscall_pthread_setspecific(pthread_key_t key, const void *val, int *_errno);
int syscall_pthread_key_delete(pthread_key_t key, int *_errno);
int syscall_get_pthread_dtors(size_t *len, dtor_t *dtors, void **vals, int *_errno);

int syscall_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr, int *_errno);
int syscall_pthread_cond_destroy(pthread_cond_t *cond, int *_errno);
int syscall_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime, int *signalled, int *_errno);
int syscall_pthread_cond_signal(pthread_cond_t *cond, int *_errno);

int syscall_pthread_join(Thread *thread, void **retval, int *_errno);
int syscall_pthread_exit(void **retval, int *_errno);

int syscall_pthread_rwlock_init(pthread_rwlock_t *lock, const pthread_rwlockattr_t *attr, int *_errno);
int syscall_pthread_rwlock_destroy(pthread_rwlock_t *lock, int *_errno);
int syscall_pthread_rwlock_tryrdlock(pthread_rwlock_t *lock, int clock_id, const timespec *until, int *_errno);
int syscall_pthread_rwlock_trywrlock(pthread_rwlock_t *lock, int clock_id, const timespec *until, int *_errno);
int syscall_pthread_rwlock_unlock(pthread_rwlock_t *lock, int *_errno);

int syscall_pthread_setname_np(pthread_t thread, const char *name, int *_errno);

int syscall_sem_init(sem_t *sem, int pshared, unsigned int value, int *_errno);
int syscall_sem_destroy(sem_t *sem, int *_errno);
int syscall_sem_getvalue(sem_t *sem, int *outval, int *_errno);
int syscall_sem_post(sem_t *sem, int *_errno);
int syscall_sem_trywait(sem_t *sem, int clock_id, const timespec *until, int *_errno);

int syscall_set_thread_priority(Thread *thread, int priority, int *_errno);
int syscall_get_thread_priority(Thread *thread, int *_errno);
int syscall_sched_get_priority_max(int policy, int *_errno);
int syscall_sched_get_priority_min(int policy, int *_errno);

int syscall_memalloc(size_t len, void **retaddr, int is_sync, int *_errno);
int syscall_memdealloc(size_t len, const void *addr, int *_errno);
int syscall_setprot(const void *addr, int is_read, int is_write, int is_exec, int *_errno);

int syscall_gpuenqueue(const gpu_message *msgs, size_t nmsg, size_t *nsent, int *_errno);

clock_t syscall_times(struct tms *buf, int *_errno);
int syscall_kill(pid_t pid, int sig, int *_errno);
int syscall_mkdir(const char *pathname, mode_t mode, int *_errno);
int syscall_opendir(const char *pathname, int *_errno);
int syscall_readdir(int dirfd, dirent *de, int *_errno);
int syscall_closedir(int dirfd, int *_errno);
int syscall_chdir(const char *path, int *_errno);

int syscall_peekevent(Event *ev, int *_errno);
int syscall_pushevents(pid_t pid, const Event *e, size_t nevents, int *_errno);

int syscall_setwindowtitle(const char *title, int *_errno);

int syscall_cacheflush(void *addr, size_t len, int is_exec, int *_errno);

int syscall_waitpid(pid_t pid, int *status, int options, int *_errno);

int syscall_getheap(void **addr, size_t *sz, int *_errno);

int syscall_pipe(int pipefd[2], int *_errno);
int syscall_dup2(int oldfd, int newfd, int *_errno);

int syscall_realpath(const char *path, char *resolved_path, size_t len, int *_errno);
int syscall_cmpxchg(void **ptr, void **oldval, void *newval, size_t len, int *_errno);

// needed for supervisor
pid_t syscall_get_focus_pid(int *_errno);
pid_t syscall_get_proc_ppid(pid_t pid, int *_errno);
int syscall_get_pid_valid(pid_t pid, int *_errno);
int syscall_setsupervisorvisible(int visible, int screen, int *_errno);

// environment variable support
int syscall_get_env_count(int *_errno);
int syscall_get_ienv_size(unsigned int idx, int *_errno);
int syscall_get_ienv(char *bufout, size_t buflen, unsigned int idx, int *_errno);

// nema gpu
int syscall_nemaenable(pthread_mutex_t *nema_mutexes, size_t nmutexes,
    void *nema_rb, sem_t *nema_irq_sem, pthread_mutex_t *eof_mutex, int *_errno);

static inline int deferred_return(int ret, int _errno)
{
    if(ret == -1)
    {
        //errno = _errno;
        return ret;
    }
    if(ret == -2)
    {
        // deferred return
        auto t = GetCurrentThreadForCore();
        while(!t->ss.Wait(SimpleSignal::Set, 0));
        if(t->ss_p.ival1 == -1)
        {
            //errno = t->ss_p.ival2;
            return -1;
        }
        else
        {
            return t->ss_p.ival1;
        }
    }
    return ret;
}

template<typename Func, class... Args> int deferred_call(Func f, Args... a)
{
    int _errno = 0;
    int ret = f(a..., &_errno);
    return deferred_return(ret, _errno);
}

/* inline functions to support quick checking of usermode pointers for syscalls */
#if GK_CHECK_USER_ADDRESSES
template<typename T> static inline bool addr_is_valid(const T *buf, bool is_write = false)
{
    return GetCurrentThreadForCore()->addr_is_valid(buf, sizeof(T), is_write);
}

static inline bool addr_is_valid(const void *buf, size_t len, bool is_write = false)
{
    return GetCurrentThreadForCore()->addr_is_valid(buf, len, is_write);
}

#define ADDR_CHECK_BUFFER_RW(buf, len, is_write) \
    do { \
        if(!addr_is_valid(buf, len, is_write)) { \
            *_errno = EFAULT; \
            return -1; \
        } \
    } while(0)

#define ADDR_CHECK_STRUCT_RW(buf, is_write) \
    do { \
        if(!addr_is_valid(buf, is_write)) { \
            *_errno = EFAULT; \
            return -1; \
        } \
    } while(0)

#define ADDR_CHECK_BUFFER_R(buf, len) ADDR_CHECK_BUFFER_RW(buf, len, false)
#define ADDR_CHECK_BUFFER_W(buf, len) ADDR_CHECK_BUFFER_RW(buf, len, true)
#define ADDR_CHECK_STRUCT_R(buf) ADDR_CHECK_STRUCT_RW(buf, false)
#define ADDR_CHECK_STRUCT_W(buf) ADDR_CHECK_STRUCT_RW(buf, true)

#else

#define ADDR_CHECK_BUFFER_R(buf, len) do {} while(0)
#define ADDR_CHECK_BUFFER_W(buf, len) do {} while(0)
#define ADDR_CHECK_STRUCT_R(buf) do {} while(0)
#define ADDR_CHECK_STRUCT_W(buf) do {} while(0)

#endif

#endif
