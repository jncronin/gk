#include <stm32h7rsxx.h>
#include "scheduler.h"
#include "logger.h"
#include "reset.h"

void gk_reset()
{
    // trigger reset
    SCB->AIRCR = (0x05faU << SCB_AIRCR_VECTKEY_Pos) |
        SCB_AIRCR_SYSRESETREQ_Msk;
    Block(clock_cur() + kernel_time::from_ms(50));
    klog("shutdown: sysresetreq failed, trying local reset\n");

    SCB->AIRCR = (0x05faU << SCB_AIRCR_VECTKEY_Pos) |
        SCB_AIRCR_VECTRESET_Msk;
    Block(clock_cur() + kernel_time::from_ms(50));
    klog("shutdown: vectreset failed, looping forever\n");

    DisableInterrupts();
    while(true)
    {
        __asm__ volatile("yield \n");
    }
}