#include <stm32h7xx.h>
#include "memblk.h"
#include "thread.h"
#include "scheduler.h"

void system_init_cm7();
void system_init_cm4();

static void idle_thread(void *p);

__attribute__((section(".sram4")))Scheduler s;

int main()
{
    system_init_cm7();
    init_memblk();

    s.Schedule(Thread::Create("idle_cm7", idle_thread, (void*)0, true, 0, CPUAffinity::M7Only, 512));
    s.Schedule(Thread::Create("idle_cm4", idle_thread, (void*)1, true, 0, CPUAffinity::M4Only, 512));

    s.StartForCurrentCore();

    return 0;
}

extern "C" int main_cm4()
{
    system_init_cm4();
    
    // TODO: wait upon startup - should signal from idle_cm7
    while(true)
    {
        __WFI();
    }

    s.StartForCurrentCore();

    return 0;
}

void idle_thread(void *p)
{
    (void)p;
    while(true)
    {
        __WFI();
    }
}

/* The following just to get it to compile */
extern "C" void * _sbrk(int n)
{
    while(true);
}
