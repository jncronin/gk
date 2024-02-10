#include <stm32h7xx.h>
#include "syscalls.h"
#include "thread.h"
#include "screen.h"
#include "scheduler.h"
#include "syscalls_int.h"

extern Scheduler s;

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
    switch(sno)
    {
        case StartFirstThread:
            // enable systick and trigger a PendSV IRQ
            SysTick->CTRL = 7UL;
            SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
            s.scheduler_running[GetCoreID()] = true;

            SCB_CleanInvalidateDCache();
            break;

        case GetThreadHandle:
            *reinterpret_cast<Thread**>(r1) = GetCurrentThreadForCore();
            break;

        case FlipFrameBuffer:
            *reinterpret_cast<void **>(r1) = screen_flip();
            break;

        case GetFrameBuffer:
            *reinterpret_cast<void **>(r1) = screen_get_frame_buffer();
            break;

        case SetFrameBuffer:
            screen_set_frame_buffer(r1, r2, (uint32_t)r3);
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
                int ret = syscall_fstat((int)r2, (struct stat *)r3);
                *reinterpret_cast<int *>(r1) = ret;
            }
            break;

        case __syscall_write:
            {
                auto p = reinterpret_cast<struct __syscall_read_params *>(r2);
                int ret = syscall_write(p->file, p->ptr, p->len);
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
                    memblk_deallocate(pt->stack);
                }

                memblk_deallocate(p.code_data);
                memblk_deallocate(p.heap);

                p.rc = rc;
                p.for_deletion = true;

                Yield();
            }
            break;

        default:
            while(true);
            break;
    }
}
