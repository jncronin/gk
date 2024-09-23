#include <stm32h7rsxx.h>
#include <cstring>
#include "pins.h"
#include "i2c.h"
#include "memblk.h"
#include "scheduler.h"
#include "process.h"
#include "SEGGER_RTT.h"

uint32_t test_val;

uint32_t test_range[256];

static const constexpr pin CTP_NRESET { GPIOC, 0 };
void system_init_cm7();

Scheduler sched;
Process kernel_proc;
Process p_supervisor;

int main()
{
    system_init_cm7();

    init_memblk();

    
    while(true);

    return 0;
}
