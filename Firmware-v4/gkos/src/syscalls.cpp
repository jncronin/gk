//#include <stm32h7rsxx.h>
#include "gk_conf.h"
#include "syscalls.h"
#include "thread.h"
#include "process.h"
//#include "screen.h"
#include "scheduler.h"
#include "clocks.h"
#include "syscalls_int.h"
#include <sys/times.h>
#include <cstring>
#include <atomic>
#include <algorithm>
#include "elf.h"
#include "cleanup.h"
#include "sound.h"
#include "btnled.h"
#include "util.h"
#include "screen.h"

#define DEBUG_SYSCALLS  0

extern PProcess p_kernel;

#if GK_COUNT_SYSCALLS
static const constexpr size_t n_syscall_counts = 512;
static std::atomic<size_t> syscall_counts[n_syscall_counts] = { 0 };

static Spinlock sl_last_syscall_dump;
static kernel_time last_syscall_dump = kernel_time_invalid();

std::vector<std::pair<size_t, size_t>> syscalls_get_count()
{
    std::vector<std::pair<size_t, size_t>> ret;
    for(size_t n = 0; n < n_syscall_counts; n++)
    {
        auto cnt = syscall_counts[n].exchange(0);
        if(cnt)
        {
            ret.push_back(std::make_pair(n, cnt));
        }
    }
    // sort descending
    std::sort(ret.begin(), ret.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    return ret;
}
#else
std::vector<std::pair<size_t, size_t>> syscalls_get_count()
{
    std::vector<std::pair<size_t, size_t>> ret;
    return ret;
}
#endif

void SyscallHandler(syscall_no sno, void *r1, void *r2, void *r3, uintptr_t lr, exception_regs *regs)
{
#if DEBUG_SYSCALLS
    void *dr1 = r1;
    void *dr2 = r2;
    void *dr3 = r3;
    void *dr4 = (void *)(uintptr_t)regs->x4;

    if((int)sno != 50 && (int)sno != 51)
    {
        klog("syscalls: start: %d (%p, %p, %p)\n", (int)sno, dr1, dr2, dr3);
    }
#endif
#if GK_COUNT_SYSCALLS
    if((size_t)sno < 512)
    {
        syscall_counts[(size_t)sno]++;
    }

    auto now = clock_cur();
    CriticalGuard cg_last_syscall_dump(sl_last_syscall_dump);
    if(!kernel_time_is_valid(last_syscall_dump) || (now > last_syscall_dump + kernel_time_from_ms(1000)))
    {
        last_syscall_dump = now;
        cg_last_syscall_dump.unlock();

        auto counts = syscalls_get_count();

        klog("syscalls: id, count\n");
        for(const auto &v : counts)
        {
            klog("syscalls: %3u : %u\n", v.first, v.second);
        }
        klog("syscalls: dump ends\n");
    }
    else
    {
        cg_last_syscall_dump.unlock();
    }
#endif
    [[maybe_unused]] auto syscall_start = clock_cur_ms();
    syscall_profile_v1 *prof = nullptr;
    if((uint64_t)r1 & 0x8000000000000000ULL)
    {
        r1 = (void *)((uint64_t)r1 & ~0x8000000000000000ULL);

        // TODO: handle __thread_cleanup and __exit which use r1 specially

#if GK_PROFILE_SYSCALLS
        prof = (syscall_profile_v1 *)regs->x4;
        if(prof->ver_flags != 1)
        {
            prof = nullptr;
        }
#endif
    }

    PROFILE_NOW(prof);

    switch(sno)
    {
        case GetThreadHandle:
            *reinterpret_cast<id_t *>(r1) = GetCurrentThreadForCore()->id;
            break;

        case FlipFrameBuffer:
        case GetFrameBuffer:
        case SetFrameBuffer:
            // don't support these anymore - use gpu interface
            *reinterpret_cast<int *>(r1) = -1;
            break;

        case __syscall_sbrk:
            {
                *reinterpret_cast<intptr_t *>(r1) = syscall_sbrk((intptr_t)r2,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_env_count:
            {
                *reinterpret_cast<int *>(r1) = syscall_get_env_count(reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_ienv_size:
            {
                *reinterpret_cast<int *>(r1) = syscall_get_ienv_size((unsigned int)(uintptr_t)r2,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_ienv:
            {
                auto p = reinterpret_cast<__syscall_get_ienv_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_get_ienv(p->outbuf, p->outbuf_size, p->i,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_arg_count:
            {
                *reinterpret_cast<int *>(r1) = syscall_get_arg_count(reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_iarg_size:
            {
                *reinterpret_cast<int *>(r1) = syscall_get_iarg_size((unsigned int)(uintptr_t)r2,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_iarg:
            {
                auto p = reinterpret_cast<__syscall_get_ienv_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_get_iarg(p->outbuf, p->outbuf_size, p->i,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_pthread_create:
            {
                auto p = reinterpret_cast<__syscall_pthread_create_params *>(r2);
                int ret = syscall_pthread_create(p->thread, p->attr,
                    p->start_routine, p->arg, p->arg2,
                    reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_setname_np:
            {
                auto p = reinterpret_cast<__syscall_pthread_setname_np_params *>(r2);
                int ret = syscall_pthread_setname_np(p->thread, p->name,
                    reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_sigmask:
            {
                auto p = reinterpret_cast<__syscall_pthread_sigmask_params *>(r2);
                if(p->old)
                {
                    p->old = 0;
                }
                *reinterpret_cast<int *>(r1) = 0;
            }
            break;

        case __syscall_pthread_mutex_init:
            {
                auto p = reinterpret_cast<__syscall_pthread_mutex_init_params *>(r2);
                int ret = syscall_pthread_mutex_init(p->mutex, p->attr, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_mutex_destroy:
            {
                auto p = reinterpret_cast<pthread_mutex_t *>(r2);
                int ret = syscall_pthread_mutex_destroy(p, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_mutex_trylock:
            {
                auto p = reinterpret_cast<__syscall_trywait_params *>(r2);
                int ret = syscall_pthread_mutex_trylock((pthread_mutex_t *)p->sync, p->clock_id, p->until, reinterpret_cast<int *>(r3), prof);
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_mutex_unlock:
            {
                auto p = reinterpret_cast<pthread_mutex_t *>(r2);
                int ret = syscall_pthread_mutex_unlock(p, reinterpret_cast<int *>(r3), prof);
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_key_create:
            {
                auto p = reinterpret_cast<__syscall_pthread_key_create_params *>(r2);
                int ret = syscall_pthread_key_create(p->key, p->destructor, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_key_delete:
            {
                auto p = (pthread_key_t)(uintptr_t)r2;
                int ret = syscall_pthread_key_delete(p, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_setspecific:
            {
                auto p = reinterpret_cast<__syscall_pthread_setspecific_params *>(r2);
                int ret = syscall_pthread_setspecific(p->key, p->value, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_getspecific:
            {
                auto p = reinterpret_cast<__syscall_pthread_getspecific_params *>(r2);
                int ret = syscall_pthread_getspecific(p->key, p->retp, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_cond_init:
            {
                auto p = reinterpret_cast<__syscall_pthread_cond_init_params *>(r2);
                int ret = syscall_pthread_cond_init(p->cond, p->attr, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_cond_destroy:
            {
                auto p = reinterpret_cast<pthread_cond_t *>(r2);
                int ret = syscall_pthread_cond_destroy(p, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_cond_timedwait:
            {
                auto p = reinterpret_cast<__syscall_pthread_cond_timedwait_params *>(r2);
                int ret = syscall_pthread_cond_timedwait(p->cond, p->mutex, p->abstime, p->signalled, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_cond_signal:
            {
                auto p = reinterpret_cast<pthread_cond_t *>(r2);
                int ret = syscall_pthread_cond_signal(p, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_join:
            {
                auto p = reinterpret_cast<__syscall_pthread_join_params *>(r2);
                int ret = syscall_pthread_join((pthread_t)(intptr_t)p->t, p->retval, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_exit:
            {
                auto p = reinterpret_cast<void **>(r2);
                *reinterpret_cast<int *>(r1) = syscall_pthread_exit(p, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_pthread_rwlock_init:
            {
                auto p = reinterpret_cast<__syscall_pthread_rwlock_init_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_pthread_rwlock_init(p->lock, p->attr, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_pthread_rwlock_destroy:
            {
                *reinterpret_cast<int *>(r1) = syscall_pthread_rwlock_destroy(reinterpret_cast<pthread_rwlock_t *>(r2), reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_pthread_rwlock_tryrdlock:
            {
                auto p = reinterpret_cast<__syscall_trywait_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_pthread_rwlock_tryrdlock((pthread_rwlock_t *)p->sync,
                    p->clock_id, p->until, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_pthread_rwlock_trywrlock:
            {
                auto p = reinterpret_cast<__syscall_trywait_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_pthread_rwlock_trywrlock((pthread_rwlock_t *)p->sync,
                    p->clock_id, p->until, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_pthread_rwlock_unlock:
            {
                *reinterpret_cast<int *>(r1) = syscall_pthread_rwlock_unlock(reinterpret_cast<pthread_rwlock_t *>(r2), reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_sem_init:
            {
                auto p = reinterpret_cast<__syscall_sem_init_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_sem_init((sem_t *)p->sem, p->pshared, p->value,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_sem_destroy:
            {
                *reinterpret_cast<int *>(r1) = syscall_sem_destroy((sem_t *)r2,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_sem_getvalue:
            {
                auto p = reinterpret_cast<__syscall_sem_getvalue_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_sem_getvalue((sem_t *)p->sem, p->outval,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_sem_post:
            {
                *reinterpret_cast<int *>(r1) = syscall_sem_post((sem_t *)r2, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_sem_trywait:
            {
                auto p = reinterpret_cast<__syscall_trywait_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_sem_trywait((sem_t *)p->sync,
                    p->clock_id, p->until, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_getscreenmode:
            {
                auto p = reinterpret_cast<__syscall_getscreenmode_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_getscreenmodeex(p->x, p->y, p->pf, nullptr,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_getscreenmodeex:
            {
                auto p = reinterpret_cast<__syscall_getscreenmodeex_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_getscreenmodeex(p->x, p->y, p->pf, p->refresh,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_setscreenmode:
            {
                auto p = reinterpret_cast<__syscall_getscreenmodeex_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_setscreenmode(p->x, p->y, p->pf, p->refresh,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_setpalette:
            {
                auto p = reinterpret_cast<__syscall_setpalette_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_setpalette(p->ncols, p->cols,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_gpuenqueue:
            {
                auto p = reinterpret_cast<__syscall_gpuenqueue_params *>(r2);
                int ret = syscall_gpuenqueue(p->msgs, p->nmsg, p->nsent, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_flipscreen:
            *reinterpret_cast<uintptr_t *>(r1) = screen_update();
            break;

        case __syscall_flipscreenex:
            {
                auto p = reinterpret_cast<__syscall_flipscreenex_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_screenflip(p->layer, p->alpha,
                        reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_thread_cleanup:
            {
                auto t = GetCurrentThreadForCore();
                Thread::Kill(t->id, r1);
                Yield();
            }
            break;

        case __syscall_set_thread_priority:
            {
                auto p = reinterpret_cast<__syscall_set_thread_priority_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_set_thread_priority((pthread_t)(intptr_t)p->t, p->priority, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_thread_priority:
            {
                *reinterpret_cast<int *>(r1) = syscall_get_thread_priority((pthread_t)(intptr_t)r2, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_sched_get_priority_max:
            {
                *reinterpret_cast<int *>(r1) = syscall_sched_get_priority_max(
                        (int)(intptr_t)(r2), reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_sched_get_priority_min:
            {
                *reinterpret_cast<int *>(r1) = syscall_sched_get_priority_min(
                        (int)(intptr_t)(r2), reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_pthread_dtors:
            {
                auto p = reinterpret_cast<__syscall_get_pthread_dtors_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_get_pthread_dtors(p->len, p->dtors, p->vals,
                    reinterpret_cast<int *>(r2));
            }
            break;

        case __syscall_fstat:
            {
                auto p = reinterpret_cast<struct __syscall_fstat_params *>(r2);
                int ret = syscall_fstat(p->fd, p->buf, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_write:
            {
                auto p = reinterpret_cast<struct __syscall_read_params *>(r2);
                int ret = syscall_write(p->file, p->ptr, p->len, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_read:
            {
                auto p = reinterpret_cast<struct __syscall_read_params *>(r2);
                int ret = syscall_read(p->file, p->ptr, p->len, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_isatty:
            {
                auto p = (int)(intptr_t)r2;
                int ret = syscall_isatty(p, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_lseek:
            {
                auto p = reinterpret_cast<__syscall_lseek_params *>(r2);
                int ret = syscall_lseek(p->file, p->offset, p->whence, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_open:
            {
                auto p = reinterpret_cast<__syscall_open_params *>(r2);
                int ret = syscall_open(p->name, p->flags, p->mode, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_close1:
            {
                auto f = (int)(intptr_t)r2;
                int ret = syscall_close1(f, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_close2:
            {
                auto f = (int)(intptr_t)r2;
                int ret = syscall_close2(f, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_exit:
            {
                auto rc = (int)(intptr_t)(r1);

                auto p = GetCurrentThreadForCore()->p;

                Process::Kill(p, rc);

                Yield();
            }
            break;

        case __syscall_sleep_ms:
            {
                auto tout = clock_cur() + kernel_time_from_ms(*reinterpret_cast<unsigned int *>(r2));
                auto curt = GetCurrentThreadForCore();
                {
                    curt->blocking.block(tout);
                    *reinterpret_cast<int *>(r1) = 0;
                    Yield();
                }
            }
            break;

        case __syscall_sleep_us:
            {
                auto tout = clock_cur() + kernel_time_from_us(*reinterpret_cast<uint64_t *>(r2));
                auto curt = GetCurrentThreadForCore();
                {
                    curt->blocking.block(tout);
                    *reinterpret_cast<int *>(r1) = 0;
                    Yield();
                }
            }
            break;

        case __syscall_opendir:
            {
                auto path = reinterpret_cast<const char *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_opendir(path, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_readdir:
            {
                auto p = reinterpret_cast<__syscall_readdir_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_readdir(p->fd, p->de, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_peekevent:
            {
                auto ev = reinterpret_cast<Event *>(r2);
                int ret = syscall_peekevent(ev, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_proccreate:
            {
                auto p = reinterpret_cast<__syscall_proccreate_params *>(r2);
                int ret = syscall_proccreate(p->fname, p->proc_info, p->pid, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_audioenable:
            {
                auto p = (int)(intptr_t)r2;
                *reinterpret_cast<int *>(r1) = syscall_audioenable(p, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_audioqueuebuffer:
            {
                auto p = reinterpret_cast<__syscall_audioqueuebuffer_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_audioqueuebuffer(p->buffer, p->next_buffer,
                    reinterpret_cast<int *>(r3));
            }
            break;
        
        case __syscall_audiosetmode:
            {
                auto p = reinterpret_cast<__syscall_audiosetmode_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_audiosetmode(p->nchan, p->nbits, p->freq, p->buf_size_bytes,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_audiosetmodeex:
            {
                auto p = reinterpret_cast<__syscall_audiosetmodeex_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_audiosetmodeex(p->nchan, p->nbits, p->freq,
                    p->buf_size_bytes, p->nbufs,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_audiogetbufferpos:
            {
                auto p = reinterpret_cast<__syscall_audiogetbufferpos_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_audiogetbufferpos(p->nbufs, p->curbuf,
                    p->buflen, p->bufpos, p->nchan, p->nbits, p->freq,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_audiowaitfree:
            {
                *reinterpret_cast<int *>(r1) = syscall_audiowaitfree(reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_audiosetfreq:
            {
                auto freq = (int)(intptr_t)r2;
                *reinterpret_cast<int *>(r1) = syscall_audiosetfreq(freq,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_getcwd:
            {
                auto buf = reinterpret_cast<char *>(r1);
                auto bufsize = reinterpret_cast<size_t>(r2);
                syscall_getcwd(buf, bufsize, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_mkdir:
            {
                auto p = reinterpret_cast<__syscall_mkdir_params *>(r2);
                int ret = syscall_mkdir(p->pathname, p->mode, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_unlink:
            {
                auto p = reinterpret_cast<char *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_unlink(p, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_ftruncate:
            {
                auto p = reinterpret_cast<__syscall_ftruncate_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_ftruncate(p->fd, p->length, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_chdir:
            {
                auto p = reinterpret_cast<const char *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_chdir(p, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_realpath:
            {
                auto p = reinterpret_cast<__syscall_realpath_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_realpath(p->path, p->resolved_path, p->len,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_set_leds:
            {
                auto p = reinterpret_cast<__syscall_set_led_params *>(r2);
                switch(p->led_id)
                {
                    case GK_LED_MAIN:
                        btnled_setcolor(p->color);
                        *reinterpret_cast<int *>(r1) = 0;
                        break;
                    default:
                        *reinterpret_cast<int *>(r3) = EINVAL;
                        *reinterpret_cast<int *>(r1) = -1;
                        break;
                }
            }
            break;

        case __syscall_waitpid:
            {
                auto p = reinterpret_cast<__syscall_waitpid_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_waitpid(p->pid, p->stat_loc, p->options,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_mmapv4:
            {
                auto p = reinterpret_cast<__syscall_mmapv4_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_mmapv4(p->len, p->retaddr,
                    p->is_sync, p->is_read, p->is_write, p->is_exec, p->fd, p->is_fixed, p->foffset,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_getppid:
            {
                auto pid = (int)(intptr_t)(r2);
                *reinterpret_cast<int *>(r1) = syscall_get_proc_ppid(pid,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_getpid:
            {
                auto t = GetCurrentThreadForCore();
                auto p = t->p;
                *reinterpret_cast<pid_t *>(r1) = p;
            }
            break;

        case __syscall_kill:
            {
                auto p = reinterpret_cast<__syscall_kill_params *>(r2);
                int ret = syscall_kill(p->pid, p->sig, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_setwindowtitle:
            {
                auto p = reinterpret_cast<const char *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_setwindowtitle(p, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_setprot:
            {
                auto p = reinterpret_cast<__syscall_setprot_params *>(r2);
                int ret = syscall_setprot(p->addr, p->is_read, p->is_write, p->is_exec, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_joystick_calib:
            {
                auto p = reinterpret_cast<__syscall_joystick_calib_params *>(r2);
                int ret = syscall_joystick_calib(p->axis_pair, p->left, p->right,
                    p->top, p->bottom, p->middle_x, p->middle_y,
                    reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_gettimeofday:
            {
                auto p = reinterpret_cast<__syscall_gettimeofday_params *>(r2);
                int ret = syscall_gettimeofday(p->tv, (timezone *)p->tz, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_clock_gettime:
            {
                auto p = reinterpret_cast<__syscall_clock_gettime_params *>(r2);
                if(!p->tp)
                {
                    *reinterpret_cast<int *>(r3) = EINVAL;
                    *reinterpret_cast<int *>(r1) = -1;
                }
                else
                {
                    switch(p->clk_id)
                    {
                        case CLOCK_REALTIME:
                            clock_get_realtime(p->tp);
                            *reinterpret_cast<int *>(r1) = 0;
                            break;

                        case 4: // MONOTONIC
                        case 5: // MONOTONIC_RAW
                            *p->tp = clock_cur();
                            *reinterpret_cast<int *>(r1) = 0;
                            break;

                        default:
                            *reinterpret_cast<int *>(r3) = EINVAL;
                            *reinterpret_cast<int *>(r1) = -1;
                            break;
                    }
                }
            }
            break;

        case __syscall_setgoldenthread:
            *reinterpret_cast<int *>(r1) = syscall_setgoldenthread(
                (pthread_t)(intptr_t)r2, reinterpret_cast<int *>(r3)
            );
            break;

        case __syscall_times:
            {
                auto p = reinterpret_cast<__syscall_times_params *>(r2);
                *p->retval = syscall_times(p->buf, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = 0;
            }
            break;

        case __syscall_pthread_barrier_init:
            {
                auto p = reinterpret_cast<__syscall_pthread_barrier_init_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_pthread_barrier_init((id_t *)p->barrier,
                    p->attr, p->count, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_pthread_barrier_wait:
            {
                auto bid = reinterpret_cast<id_t *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_pthread_barrier_wait(bid,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_pthread_barrier_destroy:
            {
                auto bid = reinterpret_cast<id_t *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_pthread_barrier_destroy(bid,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_socket:
            {
                auto p = reinterpret_cast<__syscall_socket_params *>(r2);
                int ret = syscall_socket(p->domain, p->type, p->protocol, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_bind:
            {
                auto p = reinterpret_cast<__syscall_bind_params *>(r2);
                int ret = syscall_bind(p->sockfd, p->addr, p->addrlen, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_listen:
            {
                auto p = reinterpret_cast<__syscall_listen_params *>(r2);
                int ret = syscall_listen(p->sockfd, p->backlog, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_accept:
            {
                auto p = reinterpret_cast<__syscall_accept_params *>(r2);
                int ret = syscall_accept(p->sockfd, p->addr, p->addrlen, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_sendto:
            {
                auto p = reinterpret_cast<__syscall_sendto_params *>(r2);
                int ret = syscall_sendto(p->sockfd, p->buf, p->len, p->flags, p->dest_addr, p->addrlen,
                    reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_recvfrom:
            {
                auto p = reinterpret_cast<__syscall_recvfrom_params *>(r2);
                int ret = syscall_recvfrom(p->sockfd, p->buf, p->len, p->flags, p->src_addr, p->addrlen,
                    reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_yield:
            Yield();
            *reinterpret_cast<int *>(r1) = 0;
            break;

        case __syscall_pipe:
            {
                auto p = reinterpret_cast<int *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_pipe(p,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_dup2:
            {
                auto p = reinterpret_cast<__syscall_dup_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_dup2(p->fd1, p->fd2,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_getfocusprocess:
            *reinterpret_cast<id_t *>(r1) = GetFocusPid();
            break;

        case __syscall_getscreenmodeforprocess:
            {
                auto p = reinterpret_cast<__syscall_getscreenmodeforprocess_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_getscreenmodeforprocess(p->pid,
                    p->w, p->h, p->pf, p->refresh, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_getprocessname:
            {
                auto p = reinterpret_cast<__syscall_getprocessname_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_getprocessname(p->pid,
                    p->name, p->len, reinterpret_cast<int *>(r3));
            }
            break;

#if 0

        case __syscall_setsupervisorvisible:
            {
                auto p = reinterpret_cast<__syscall_setsupervisorvisible_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_setsupervisorvisible(p->visible, p->screen,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_cmpxchg:
            {
                auto p = reinterpret_cast<__syscall_cmpxchg_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_cmpxchg(p->ptr, p->oldval, p->newval, p->sz,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_icacheinvalidate:
            *reinterpret_cast<int *>(r1) = syscall_icacheinvalidate(reinterpret_cast<int *>(r3));
            break;
#endif

        default:
            {
                auto [ cur_t, cur_p ] = GetCurrentThreadProcessForCore();
                if(cur_t && cur_p)
                {
                    klog("syscall: unhandled syscall %d, Process %s, Thread %s @ %p, lr: %llx\n", (int)sno,
                        cur_p->name.c_str(),
                        cur_t->name.c_str(),
                        cur_t,
                        lr);
                }
            }
            //__asm__ volatile ("bkpt #0\n");
            if(r1)
            {
                *(uintptr_t *)r1 = -1;
            }

#if 0
            {
                auto t = GetCurrentThreadForCore();
                auto &p = t->p;
                klog("panic: process %s thread %s unhandled syscall\n",
                    p.name.c_str(), t->name.c_str());
                if(&p != &kernel_proc)
                {
                    CriticalGuard cg_p(p.sl);
                    for(auto thr : p.threads)
                    {
                        CriticalGuard cg_t(thr->sl);
                        thr->for_deletion = true;
                    }
                    p.for_deletion = true;
                    Yield();
                }
                else
                {
                    while(true);
                }
            }
#endif

            break;
    }

#if 0
    GetCurrentThreadForCore()->total_s_time += clock_cur_ms() - syscall_start;
#endif

    PROFILE_NOW(prof);

#if DEBUG_SYSCALLS
    if((int)sno != 50 && (int)sno != 51)
    {
        klog("syscalls: end  : %d (%p, %p, %p)\n", (int)sno, dr1, dr2, dr3);
    }
#endif
}
