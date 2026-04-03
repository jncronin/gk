#include "etnaviv/src/etnaviv_drv.h"
#include "etnaviv/src/etnaviv_gpu.h"
#include <stm32mp2xx.h>
#include "pmic.h"
#include "vmem.h"
#include "clocks.h"
#include <assert.h>

#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))

static std::unique_ptr<device> etna_dev;

int etnaviv_gpu_combined_init(struct device &dev);

void init_etnaviv()
{
    // check order_base_2 logic
    for(auto i = 0u; i < 10u; i++)
    {
        klog("order_base_2(%u) = %u\n", i, order_base_2(i));
    }
    
    assert(order_base_2(0) == 0);
    assert(order_base_2(1) == 0);
    assert(order_base_2(2) == 1);
    assert(order_base_2(3) == 2);
    assert(order_base_2(4) == 2);
    assert(order_base_2(5) == 3);

    etna_dev = std::make_unique<device>();
    etnaviv_gpu_combined_init(*etna_dev);
}

int Etnaviv_pm_control::enable()
{
    pmic_set_power(PMIC_Power_Target::GPU, 800);
    udelay(1000);
    PWR_VMEM->CR12 |= PWR_CR12_GPUVMEN;
    __DSB();
    while(!(PWR_VMEM->CR12 & PWR_CR12_VDDGPURDY));
    __DSB();
    PWR_VMEM->CR12 |= PWR_CR12_GPUSV;

    klog("GPU power enabled\n");

    return 0;
}

int Etnaviv_pm_control::disable()
{
    PWR_VMEM->CR12 &= ~PWR_CR12_GPUSV;
    __DSB();
    pmic_set_power(PMIC_Power_Target::GPU, 0);

    klog("GPU power disabled\n");

    return 0;
}

int Etnaviv_reset_control::Assert()
{
    RCC_VMEM->GPUCFGR |= RCC_GPUCFGR_GPURST;
    klog("GPU reset asserted\n");
    return 0;
}

int Etnaviv_reset_control::Deassert()
{
    RCC_VMEM->GPUCFGR &= ~RCC_GPUCFGR_GPURST;
    __DSB();
    while(RCC_VMEM->GPUCFGR & RCC_GPUCFGR_GPURST);
    klog("GPU reset deasserted\n");
    return 0;
}

int Etnaviv_bus_clock::enable(uint64_t)
{
    //flexgen59 for clk_icn_m_gpu, max 600 MHz
    // Use PLL5 = 1200 MHz / 2 -> 600 MHz
    RCC_VMEM->PREDIVxCFGR[59] = 1;
    RCC_VMEM->FINDIVxCFGR[59] = 0x40;
    RCC_VMEM->XBARxCFGR[59] = 0x41;
    __DSB();

    RCC_VMEM->GPUCFGR |= RCC_GPUCFGR_GPUEN;
    __DSB();
    klog("GPU bus clock enabled\n");
    return 0;
}

int Etnaviv_bus_clock::disable()
{
    RCC_VMEM->GPUCFGR &= ~RCC_GPUCFGR_GPUEN;

    RCC_VMEM->XBARxCFGR[59] = 0;

    klog("GPU bus clock disabled\n");
    return 0;
}

int Etnaviv_core_clock::enable(uint64_t new_freq)
{
    // disable before enable
    if(RCC_VMEM->PLL3CFGR1 & RCC_PLL3CFGR1_PLLEN)
    {
        if(new_freq == freq)
        {
            // already enabled at correct rate - do nothing
            return 0;
        }
        disable();
    }

    if(new_freq == ~0ULL)
        new_freq = 800000000;
    freq = new_freq;

    // set reference clock to HSE40
    RCC_VMEM->MUXSELCFGR = (RCC_VMEM->MUXSELCFGR & ~RCC_MUXSELCFGR_MUXSEL7_Msk) |
        (7U << RCC_MUXSELCFGR_MUXSEL1_Pos);
    __DSB();

    if(freq != 800000000)
    {
        klog("GPU frequencies other than 800 MHz not yet implemented (%llu requested)\n",
            freq);
    }

    RCC_VMEM->PLL3CFGR2 = (2UL << RCC_PLL3CFGR2_FREFDIV_Pos) |
        (80UL << RCC_PLL3CFGR2_FBDIV_Pos);
    RCC_VMEM->PLL3CFGR3 = 0x04000000;
    RCC_VMEM->PLL3CFGR4 = 0x00000200;
    RCC_VMEM->PLL3CFGR5 = 0;
    RCC_VMEM->PLL3CFGR6 = 2;
    RCC_VMEM->PLL3CFGR7 = 1;
    RCC_VMEM->PLL3CFGR1 |= RCC_PLL3CFGR1_PLLEN;

    __DSB();
    while(!(RCC_VMEM->PLL3CFGR1 & RCC_PLL3CFGR1_PLLRDY));

    klog("GPU core clock enabled\n");

    return 0;
}

int Etnaviv_core_clock::disable()
{
    if(RCC_VMEM->PLL3CFGR1 & RCC_PLL3CFGR1_PLLEN)
    {
        RCC_VMEM->PLL3CFGR1 &= ~RCC_PLL3CFGR1_PLLEN;
        udelay(10);
    }
    return 0;
}

int drm_dev_register(drm_device &dev, int)
{
    klog("DRM: registered device\n");
    return 0;
}

int usleep(useconds_t usec)
{
    if(usec < 1000)
    {
        udelay(usec);
        return 0;
    }
    Block(clock_cur() + kernel_time_from_us(usec));
    return 0;
}
