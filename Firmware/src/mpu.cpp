#include "mpuregions.h"
#include "stm32h7xx.h"
#include "thread.h"
#include "gk_conf.h"
#include "scheduler.h"

__attribute__((section(".sram4"))) volatile Thread *last_thread;

bool SetMPUForCurrentThread(mpu_saved_state const &mpu_reg)
{
#if GK_USE_MPU
    if(!(mpu_reg.rbar & (1UL << 4)))    // VALID not set
        return false;
    auto reg_id = mpu_reg.rbar & 0xfUL;
    if(reg_id < 1 || reg_id > 7)
        return false;
    
    auto t = GetCurrentThreadForCore();
    {
        CriticalGuard cg(t->sl);
        last_thread = t;
        t->tss.mpuss[reg_id - 1] = mpu_reg;
        auto ctrl = MPU->CTRL;
        MPU->CTRL = 0;
        MPU->RBAR = mpu_reg.rbar;
        MPU->RASR = mpu_reg.rasr;
        MPU->CTRL = ctrl;
        __DSB();
        __ISB();
        return true;
    }
#else
    return true;
#endif
}
