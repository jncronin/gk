#include <stm32mp2xx.h>
#include "clocks.h"
#include "vmem.h"

#define TIM3_VMEM ((TIM_TypeDef *)PMEM_TO_VMEM(TIM3_BASE))

volatile uint64_t * _cur_s;
volatile uint64_t * _tim_precision_ns;

void init_clocks(const gkos_boot_interface *gbi)
{
    _cur_s = (volatile uint64_t *)(PMEM_TO_VMEM((uint64_t)gbi->cur_s));
    _tim_precision_ns = (volatile uint64_t *)(PMEM_TO_VMEM((uint64_t)gbi->tim_ns_precision));
}

timespec clock_cur()
{
    while(true)
    {
        uint64_t _s_a = *_cur_s;
        uint64_t _cur_sc_ns = TIM3_VMEM->CNT;
        uint64_t _s_b = *_cur_s;

        if(_s_a == _s_b)
        {
            timespec ret;
            ret.tv_nsec = _cur_sc_ns * *_tim_precision_ns;
            ret.tv_sec = _s_a;
            return ret;
        }
    }
}

uint64_t clock_cur_ns()
{
    auto ts = clock_cur();
    return ts.tv_nsec + (uint64_t)ts.tv_sec * 1000000000;
}

uint64_t clock_cur_us()
{
    return clock_cur_ns() / 1000ULL;
}

uint64_t clock_cur_ms()
{
    return clock_cur_ns() / 1000000ULL;
}

void udelay(unsigned int d)
{
    auto until = clock_cur_us() + (uint64_t)d;
    while(clock_cur_us() < until);
}
