#include <stm32h7xx.h>
#include "memblk.h"

void system_init_cm7();
void system_init_cm4();

int main()
{
    system_init_cm7();
    init_memblk();

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
