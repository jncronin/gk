#include "etnaviv/src/etnaviv_drv.h"
#include "etnaviv/src/etnaviv_gpu.h"
#include <stm32mp2xx.h>
#include "pmic.h"
#include "vmem.h"
#include "clocks.h"
#include <assert.h>

#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))

#define RISC_VMEM ((RISC_TypeDef *)PMEM_TO_VMEM(RISC_BASE))
#define RIMC_VMEM ((RIMC_TypeDef *)PMEM_TO_VMEM(RIMC_BASE))
#define RIFSC_VMEM PMEM_TO_VMEM(RIFSC_BASE)


static std::shared_ptr<device> etna_dev;

int etnaviv_gpu_combined_init(struct device &dev);

void init_etnaviv()
{
    /* Functions marked __init */
    void etnaviv_validate_init(void);
    etnaviv_validate_init();


    /* GPU is RISUP 79 / RIMC 9
        Set as same as CA35
    */
    //RISC_VMEM->PRIVCFGR[2] &= ~(1U << 15);
    //RIMC_VMEM->ATTR[9] = 0;
    *(volatile uint32_t *)(RIFSC_VMEM + 0xc10 + 9 * 0x4) =
        (1UL << 2) |                // use cid specified here
        (1UL << 4) |                // CID 1
        (1UL << 8) |                // secure
        (1UL << 9);                 // priv

    /* The GPU is forced to non-secure if its RISUP (RISUP 79) is programmed as non-secure,
        therefore set as secure here */
    const uint32_t risup = 79;
    const uint32_t risup_word = risup / 32;
    const uint32_t risup_bit = risup % 32;
    auto risup_reg = (volatile uint32_t *)(RIFSC_VMEM + 0x10 + 0x4 * risup_word);
    auto old_val = *risup_reg;
    old_val |= 1U << risup_bit;
    *risup_reg = old_val;
    __asm__ volatile("dmb sy\n" ::: "memory");


    etna_dev = std::make_shared<device>();
    if(etnaviv_gpu_combined_init(*etna_dev) == 0)
    {
        drm_dev_register(etna_dev, 0);
    }


#if 0
    /* Just try enabling things, without config, to see what happens */
    Etnaviv_bus_clock _bc;
    Etnaviv_core_clock _cc;
    Etnaviv_reg_clock _rc;
    Etnaviv_reset_control _rst;
    Etnaviv_pm_control _pm;

    _rc.enable();
    _bc.enable();
    _pm.enable();
    _rst.Deassert();
    _cc.enable();
#endif
}

int Etnaviv_pm_control::enable()
{
    if(PWR_VMEM->CR12 & PWR_CR12_VDDGPURDY)
        return 0;
    
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
    __DSB();
    while(RCC_VMEM->GPUCFGR & RCC_GPUCFGR_GPURST);
    klog("GPU reset completed\n");
    return 0;
}

int Etnaviv_reset_control::Deassert()
{
    while(RCC_VMEM->GPUCFGR & RCC_GPUCFGR_GPURST);
    klog("GPU reset deasserted\n");
    return 0;
}

int Etnaviv_reg_clock::enable(uint64_t)
{
    if(RCC_VMEM->GPUCFGR & RCC_GPUCFGR_GPUEN)
        return 0;
    
    RCC_VMEM->GPUCFGR |= RCC_GPUCFGR_GPUEN;
    __DSB();
    klog("GPU reg clock enabled\n");
    return 0;
}

int Etnaviv_reg_clock::disable()
{
    RCC_VMEM->GPUCFGR &= ~RCC_GPUCFGR_GPUEN;
    klog("GPU reg clock disabled\n");
    return 0;
}

int Etnaviv_bus_clock::enable(uint64_t)
{
    if(RCC_VMEM->PREDIVxCFGR[59] == 1 &&
        RCC_VMEM->FINDIVxCFGR[59] == 0x40 &&
        RCC_VMEM->XBARxCFGR[59] == 0x41)
    {
        return 0;
    }

    //flexgen59 for clk_icn_m_gpu, max 600 MHz
    // Use PLL5 = 1200 MHz / 2 -> 600 MHz
    RCC_VMEM->XBARxCFGR[59] = 0;
    __DSB();
    RCC_VMEM->PREDIVxCFGR[59] = 1;
    RCC_VMEM->FINDIVxCFGR[59] = 0x40;
    RCC_VMEM->XBARxCFGR[59] = 0x41;
    __DSB();

    klog("GPU bus clock enabled\n");
    return 0;
}

int Etnaviv_bus_clock::disable()
{
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
        (1U << RCC_MUXSELCFGR_MUXSEL7_Pos);
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
