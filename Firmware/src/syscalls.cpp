#include <stm32h7xx.h>
#include "syscalls.h"
#include "thread.h"
#include "screen.h"
#include "scheduler.h"
#include "clocks.h"
#include "syscalls_int.h"
#include <sys/times.h>
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

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
    auto syscall_start = clock_cur_ms();
    switch(sno)
    {
        case StartFirstThread:
            // enable systick and trigger a PendSV IRQ
            SysTick->CTRL = 7UL;
            SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
            scheduler_running() = true;

#if GK_USE_CACHE
            if(GetCoreID() == 0)
                SCB_CleanInvalidateDCache();
#endif
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
                            p.brk += unbytes;
                        }
                    }
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

                Yield();
            }
            break;

        case WaitSimpleSignal:
            {
                auto t = GetCurrentThreadForCore();
                CriticalGuard cg(t->sl);
                auto wss_ret = t->ss.WaitOnce();
                if(wss_ret)
                {
                    auto p = reinterpret_cast<WaitSimpleSignal_params *>(r2);
                    *p = t->ss_p;
                    t->ss.Reset();
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

        case __syscall_proccreate:
            {
                auto p = reinterpret_cast<__syscall_proccreate_params *>(r2);
                int ret = syscall_proccreate(p->fname, p->proc_info, reinterpret_cast<int *>(r3));
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
                auto p = reinterpret_cast<pthread_mutex_t *>(r2);
                int ret = syscall_pthread_mutex_trylock(p, reinterpret_cast<int *>(r3));
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
                                auto curt = clock_cur_ms();

                                p->tp->tv_nsec = (curt % 1000) * 1000000;
                                p->tp->tv_sec = curt / 1000;
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
                auto tout = clock_cur_ms() + *reinterpret_cast<unsigned long *>(r2);
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
                int ret = syscall_memalloc(p->len, p->retaddr, reinterpret_cast<int *>(r3));
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
                int pid = (int)((uint32_t)(uintptr_t)&p - 0x38000000U);
                *reinterpret_cast<int *>(r1) = pid;
            }
            break;

        case __syscall_kill:
            {
                auto p = reinterpret_cast<__syscall_kill_params *>(r2);
                int ret = syscall_kill(p->pid, p->sig, reinterpret_cast<int *>(r3));
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;
        
        default:
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "syscall: unhandled syscall %d\n", (int)sno);
            }
            __asm__ volatile ("bkpt #0\n");
            while(true);
            break;
    }

    GetCurrentThreadForCore()->total_s_time += clock_cur_ms() - syscall_start;
}
