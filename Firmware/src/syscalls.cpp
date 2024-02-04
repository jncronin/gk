#include <stm32h7xx.h>
#include "syscalls.h"
#include "thread.h"
#include "screen.h"

extern "C" void SVC_Handler() __attribute__((naked));

extern "C" void SVC_Handler()       /* Can we just define this with the parameters of SyscallHandler? */
{
    __asm volatile
    (
        "push {lr}              \n"
        "bl SyscallHandler      \n"
        "pop {lr}               \n"
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
            // just trigger a PendSV interrupt
            SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
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
    }
}
