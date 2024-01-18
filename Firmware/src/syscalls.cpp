#include <stm32h7xx.h>
#include "syscalls.h"
#include "thread.h"

extern "C" void SVC_Handler() __attribute__((naked));

extern "C" void SVC_Handler()       /* Can we just define this with the parameters of SyscallHandler? */
{
    __asm
    (
        "bl SyscallHandler      \n"
        "bx lr                  \n"
        ::: "memory"
    );
}

void SyscallHandler(syscall_no sno, void *r1, void *r2, void *r3)
{
    switch(sno)
    {
        case StartFirstThread:
            // just trigger a PendSV interrupt
            NVIC_SetPendingIRQ(PendSV_IRQn);
            break;

        case GetThreadHandle:
            *reinterpret_cast<Thread**>(r1) = GetCurrentThreadForCore();
            break;
    }
}
