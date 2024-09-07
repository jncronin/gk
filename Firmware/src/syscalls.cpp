#include <stm32h7xx.h>
#include "syscalls.h"
#include "thread.h"
#include "process.h"
#include "screen.h"
#include "scheduler.h"
#include "clocks.h"
#include "syscalls_int.h"
#include <sys/times.h>
#include <cstring>
#include "SEGGER_RTT.h"
#include "elf.h"
#include "cleanup.h"
#include "sound.h"
#include "btnled.h"

#define DEBUG_SYSCALLS  0

extern Spinlock s_rtt;
extern Process kernel_proc;

extern "C" void SVC_Handler() __attribute__((naked));

extern "C" void SVC_Handler()       /* Can we just define this with the parameters of SyscallHandler? */
{
    __asm volatile
    (
        "push {lr}              \n"
        "bl SyscallHandler      \n"
        "pop {pc}               \n"
        ::: "memory"
    );
}

extern "C" {
    void SyscallHandler(syscall_no num, void *r1, void *r2, void *r3);
}

void SyscallHandler(syscall_no sno, void *r1, void *r2, void *r3)
{
#if DEBUG_SYSCALLS
    uint32_t dr1 = (uint32_t)(uintptr_t)r1;
    uint32_t dr2 = (uint32_t)(uintptr_t)r2;
    uint32_t dr3 = (uint32_t)(uintptr_t)r3;
#endif
    auto syscall_start = clock_cur_ms();
    switch(sno)
    {
        case StartFirstThread:
            // enable systick and trigger a PendSV IRQ
#if GK_TICKLESS
#else
            SysTick->CTRL = 7UL;
#endif
            scheduler_running() = true;
            Yield();

            break;

        case GetThreadHandle:
            *reinterpret_cast<Thread**>(r1) = GetCurrentThreadForCore();
            break;

        case FlipFrameBuffer:
        case GetFrameBuffer:
        case SetFrameBuffer:
            // don't support these anymore - use gpu interface
            *reinterpret_cast<int *>(r1) = -1;
            break;

        case __syscall_sbrk:
            {
                auto t = GetCurrentThreadForCore();
                auto &p = t->p;

                CriticalGuard cg(p.sl);
                if(p.heap.valid)
                {
                    auto nbytes = (int)r2;

                    if(nbytes == 0)
                    {
                        *reinterpret_cast<uint32_t *>(r1) = p.heap.address + p.brk;
                    }
                    else if(nbytes > 0)
                    {
                        auto unbytes = static_cast<uint32_t>(nbytes);

                        if((p.heap.length - p.brk) < unbytes)
                        {
                            *reinterpret_cast<int *>(r1) = -1;
                            *reinterpret_cast<int *>(r3) = ENOMEM;
                        }
                        else
                        {
                            *reinterpret_cast<uint32_t *>(r1) = p.heap.address + p.brk;

                            // Not required to clear - newlib doesn't define MORECORE_CLEARS
                            /*
                            for(unsigned int i = 0; i < unbytes; i += 4)
                            {
                                *reinterpret_cast<uint32_t *>(p.heap.address + p.brk + i) = 0;
                            }
                            */
                            p.brk += unbytes;
                        }
                    }
#if 1
                    else
                    {
                        // nbytes < 0
                        auto unbytes = static_cast<uint32_t>(-nbytes);

                        if(p.brk < unbytes)
                        {
                            *reinterpret_cast<int *>(r1) = -1;
                            *reinterpret_cast<int *>(r3) = ENOMEM;
                        }
                        else
                        {
                            *reinterpret_cast<uint32_t *>(r1) = p.heap.address + p.brk;
                            p.brk -= unbytes;
                        }
                    }
#endif

#if 0
                    {
                        CriticalGuard cg2(s_rtt);
                        klog("sbrk: nbytes: %d, new_brk: %u, ret: %x, ret@: %x\n",
                            nbytes, p.brk, *reinterpret_cast<uint32_t *>(r1), reinterpret_cast<uint32_t>(r1));
                    }
#endif
                }
                else
                {
                    // Invalid heap
                    *reinterpret_cast<int *>(r1) = -1;
                    *reinterpret_cast<int *>(r3) = ENOMEM;
                }
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

#if DEBUG_SYSCALLS
                {
                    CriticalGuard cg(s_rtt);
                    klog("syscall_read: file %d addr: %x, len: %d, ret: %d\n",
                        p->file, (uint32_t)(uintptr_t)p->ptr, p->len, ret);
                }
#endif
            }
            break;

        case __syscall_isatty:
            {
                auto p = reinterpret_cast<int>(r2);
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
                auto f = reinterpret_cast<int>(r2);
                int ret = syscall_close1(f, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_close2:
            {
                auto f = reinterpret_cast<int>(r2);
                int ret = syscall_close2(f, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_exit:
            {
                auto rc = reinterpret_cast<int>(r1);

                auto t = GetCurrentThreadForCore();
                auto &p = t->p;

                CriticalGuard cg(p.sl);
                for(auto pt : p.threads)
                {
                    CriticalGuard cg2(pt->sl);
                    pt->for_deletion = true;
                    pt->is_blocking = true;
                }

                p.rc = rc;
                p.for_deletion = true;

                proc_list.DeleteProcess(p.pid, rc);

                CleanupQueue.Push({ .is_thread = false, .p = &p });

                Yield();
            }
            break;

        case WaitSimpleSignal:
            {
                auto t = GetCurrentThreadForCore();
                CriticalGuard cg(t->sl);
                auto wss_ret = t->ss.WaitOnce(SimpleSignal::Set, 0U);
                if(wss_ret)
                {
                    auto p = reinterpret_cast<WaitSimpleSignal_params *>(r2);
                    *p = t->ss_p;
                }
                *reinterpret_cast<uint32_t *>(r1) = wss_ret;
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

        case __syscall_pthread_create:
            {
                auto p = reinterpret_cast<__syscall_pthread_create_params *>(r2);
                int ret = syscall_pthread_create(p->thread, p->attr, p->start_routine, p->arg,
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

        case __syscall_proccreate:
            {
                auto p = reinterpret_cast<__syscall_proccreate_params *>(r2);
                int ret = syscall_proccreate(p->fname, p->proc_info, p->pid, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
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
                int ret = syscall_pthread_mutex_trylock((pthread_mutex_t *)p->sync, p->clock_id, p->until, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_pthread_mutex_unlock:
            {
                auto p = reinterpret_cast<pthread_mutex_t *>(r2);
                int ret = syscall_pthread_mutex_unlock(p, reinterpret_cast<int *>(r3));
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
                auto p = reinterpret_cast<pthread_key_t>(r2);
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
                int ret = syscall_pthread_join((Thread *)p->t, p->retval, reinterpret_cast<int *>(r3));
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
                            clock_get_now(p->tp);
                            *reinterpret_cast<int *>(r1) = 0;
                            break;

                        case 4: // MONOTONIC
                        case 5: // MONOTONIC_RAW
                            {
                                auto curt = clock_cur_us();

                                p->tp->tv_nsec = (curt % 1000000ULL) * 1000ULL;
                                p->tp->tv_sec = curt / 1000000ULL;
                                *reinterpret_cast<int *>(r1) = 0;
                            }
                            break;

                        default:
                            *reinterpret_cast<int *>(r3) = EINVAL;
                            *reinterpret_cast<int *>(r1) = -1;
                            break;
                    }
                }
            }
            break;

        case __syscall_sleep_ms:
            {
                auto tout = kernel_time::from_ms(clock_cur_ms() + *reinterpret_cast<unsigned long *>(r2));
                auto curt = GetCurrentThreadForCore();
                {
                    CriticalGuard cg(curt->sl);
                    curt->block_until = tout;
                    curt->is_blocking = true;
                    curt->blocking_on = nullptr;
                    *reinterpret_cast<int *>(r1) = 0;
                    Yield();
                }
            }
            break;

        case __syscall_sleep_us:
            {
                auto tout = kernel_time::from_us(clock_cur_us() + *reinterpret_cast<unsigned long *>(r2));
                auto curt = GetCurrentThreadForCore();
                {
                    CriticalGuard cg(curt->sl);
                    curt->block_until = tout;
                    curt->is_blocking = true;
                    curt->blocking_on = nullptr;
                    *reinterpret_cast<int *>(r1) = 0;
                    Yield();
                }
            }
            break;

        case __syscall_memalloc:
            {
                auto p = reinterpret_cast<__syscall_memalloc_params *>(r2);
                int ret = syscall_memalloc(p->len, p->retaddr, p->is_sync, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_memdealloc:
            {
                auto p = reinterpret_cast<__syscall_memdealloc_params *>(r2);
                int ret = syscall_memdealloc(p->len, p->retaddr, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_setprot:
            {
                auto p = reinterpret_cast<__syscall_setprot_params *>(r2);
                int ret = syscall_setprot(p->addr, p->is_read, p->is_write, p->is_exec, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_gpuenqueue:
            {
                auto p = reinterpret_cast<__syscall_gpuenqueue_params *>(r2);
                int ret = syscall_gpuenqueue(p->msgs, p->nmsg, p->nsent, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_times:
            {
                auto p = reinterpret_cast<__syscall_times_params *>(r2);
                *p->retval = syscall_times(p->buf, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = 0;
            }
            break;

        case __syscall_getpid:
            {
                auto t = GetCurrentThreadForCore();
                auto &p = t->p;
                *reinterpret_cast<pid_t *>(r1) = p.pid;
            }
            break;

        case __syscall_kill:
            {
                auto p = reinterpret_cast<__syscall_kill_params *>(r2);
                int ret = syscall_kill(p->pid, p->sig, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_getcwd:
            {
                auto buf = reinterpret_cast<char *>(r1);
                auto bufsize = reinterpret_cast<size_t>(r2);
                auto &p = GetCurrentThreadForCore()->p;
                if(p.cwd.length() > (bufsize - 1U))
                {
                    *reinterpret_cast<int *>(r3) = ERANGE;
                }
                else
                {
                    strcpy(buf, p.cwd.c_str());
                    *reinterpret_cast<int *>(r3) = 0;
                }
            }
            break;

        case __syscall_mkdir:
            {
                auto p = reinterpret_cast<__syscall_mkdir_params *>(r2);
                int ret = syscall_mkdir(p->pathname, p->mode, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_peekevent:
            {
                auto ev = reinterpret_cast<Event *>(r2);
                int ret = syscall_peekevent(ev, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_newlibinithook:
            {
                auto lr = reinterpret_cast<uint32_t>(r1);
                auto retaddr = reinterpret_cast<uint32_t *>(r2);
                handle_newlibinithook(lr, retaddr);
            }
            break;

        case __syscall_getscreenmode:
            {
                auto p = reinterpret_cast<__syscall_getscreenmode_params *>(r2);
                auto &proc = GetCurrentThreadForCore()->p;
                if(p->x) *p->x = proc.screen_w;
                if(p->y) *p->y = proc.screen_h;
                if(p->pf) *p->pf = proc.screen_pf;
                *reinterpret_cast<int *>(r1) = 0;
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

        case __syscall_setwindowtitle:
            {
                auto p = reinterpret_cast<const char *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_setwindowtitle(p, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_pthread_exit:
            {
                auto p = reinterpret_cast<void **>(r2);
                *reinterpret_cast<int *>(r1) = syscall_pthread_exit(p, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_cacheflush:
            {
                auto p = reinterpret_cast<__syscall_cacheflush_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_cacheflush(p->addr, p->len, p->is_exec, reinterpret_cast<int *>(r3));
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

        case __syscall_read_tp:
            {
                *reinterpret_cast<void **>(r1) = (void *)GetCurrentThreadForCore()->mr_tls.address;
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

        case __syscall_thread_cleanup:
            {
                auto t = GetCurrentThreadForCore();
                t->Cleanup(r1, false);
            }
            break;

        case __syscall_set_thread_priority:
            {
                auto p = reinterpret_cast<__syscall_set_thread_priority_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_set_thread_priority((Thread *)p->t, p->priority, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_thread_priority:
            {
                *reinterpret_cast<int *>(r1) = syscall_get_thread_priority((Thread *)r2, reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_waitpid:
            {
                auto p = reinterpret_cast<__syscall_waitpid_params *>(r2);
                *reinterpret_cast<int *>(r1) = syscall_waitpid(p->pid, p->stat_loc, p->options,
                    reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_audioenable:
            {
                auto p = reinterpret_cast<int>(r2);
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

        case __syscall_audiowaitfree:
            {
                *reinterpret_cast<int *>(r1) = syscall_audiowaitfree(reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_getheap:
            {
                syscall_getheap(reinterpret_cast<void **>(r1), reinterpret_cast<size_t *>(r2));
            }
            break;

        case __syscall_sched_get_priority_max:
            {
                *reinterpret_cast<int *>(r1) = syscall_sched_get_priority_max(
                        reinterpret_cast<int>(r2), reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_sched_get_priority_min:
            {
                *reinterpret_cast<int *>(r1) = syscall_sched_get_priority_min(
                        reinterpret_cast<int>(r2), reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_env_count:
            {
                *reinterpret_cast<int *>(r1) = syscall_get_env_count(reinterpret_cast<int *>(r3));
            }
            break;

        case __syscall_get_ienv_size:
            {
                *reinterpret_cast<int *>(r1) = syscall_get_ienv_size(reinterpret_cast<unsigned int>(r2),
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

        default:
            {
                CriticalGuard cg(s_rtt);
                klog("syscall: unhandled syscall %d\n", (int)sno);
            }
            __asm__ volatile ("bkpt #0\n");

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

            break;
    }

    GetCurrentThreadForCore()->total_s_time += clock_cur_ms() - syscall_start;

#if DEBUG_SYSCALLS
    {
        CriticalGuard cg(s_rtt);
        klog("syscalls: %d (%x, %x, %x)\n", (int)sno, dr1, dr2, dr3);
    }
#endif
}
