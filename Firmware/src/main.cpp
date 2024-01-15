#include <stm32h7xx.h>
#include "memblk.h"
#include "thread.h"

void system_init_cm7();
void system_init_cm4();

static void idle_thread(void *p);

int main()
{
    system_init_cm7();
    init_memblk();

    Thread::Create("idle_cm7", idle_thread, nullptr, true, 0, CPUAffinity::M7Only, 512);
    Thread::Create("idle_cm4", idle_thread, nullptr, true, 0, CPUAffinity::M4Only, 512);


    return 0;
}

extern "C" int main_cm4()
{
    system_init_cm4();
    
    while(true)
    {
        __WFI();
    }

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
