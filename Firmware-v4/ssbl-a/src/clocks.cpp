#include <stm32mp2xx.h>
#include "clocks.h"
#include <cstdio>
#include "time.h"
#include "logger.h"

static uint32_t cpu_freq = 0;

static void clock_start_sys();

volatile uint64_t * const _cur_s = (volatile uint64_t *)0x0e0bfe00;
volatile uint64_t * const _tim_precision_ns = (volatile uint64_t *)0x0e0bfe08;

void init_clocks()
{
    /* CPU1 is initially clocked from ck_cpu1_ext2f (undivided) from the crossbar (63) (i.e. HSI64)

        We want to set it to use its internal PLL1 (within the CA35 subsystem - CA35SS)

        Additionally we need to provide:
            ck_pll1_ref         max 64 MHz          PLL1 reference - should be exactly 64 MHz
            ck_icn_m_cpu1       max 600 MHz         AXI bridge
            ck_icn_s_gic400     max 400 MHz         GIC400
            ck_ker_stgen        max 64 MHz          STGEN timer
            ck_icn_p_stgen      max 200 MHz         STGEN timer programming interface
            ck_icn_p_gicv2m     max 200 MHz         GICv2m
            ck_icn_p_syscpu1    max 200 MHz         SYSCFG registers


        */
    
    // Set up CPU2 clock, ck_icn_hs_mcu, to be PLL5 / 3 = 400 MHz
    RCC->FINDIVxCFGR[0] = 0x42U;
    while(RCC->FINDIVSR1 & (1U << 0));              // wait for finish changing
    RCC->XBARxCFGR[0] = 0x41U;
    while(RCC->XBARxCFGR[0] & RCC_XBARxCFGR_XBARxSTS);     // wait for finish changing  

    // And ck_icn_ls_mcu to be half this
    RCC->LSMCUDIVR |= RCC_LSMCUDIVR_LSMCUDIV;
    while(!(RCC->LSMCUDIVR & RCC_LSMCUDIVR_LSMCUDIVRDY));
    
    // Provide ck_pll1_ref from HSE64
    RCC->MUXSELCFGR = (RCC->MUXSELCFGR &~ RCC_MUXSELCFGR_MUXSEL5_Msk) |
        (1U << RCC_MUXSELCFGR_MUXSEL5_Pos);    // MUXSEL5 = PLL1 confusingly
    (void)RCC->MUXSELCFGR;

    // Provide ck_ker_stgen from HSI64 via crossbar (33)
    RCC->PREDIVxCFGR[33] = 0;       // div 1
    RCC->FINDIVxCFGR[33] = 0x40;    // enabled, div 1
    RCC->XBARxCFGR[33] = 0x48;      // enabled, hsi_ker_ck

    // PLL5 output is 1200 MHz - use to interface AXI via crossbar (2), divided by 2
    RCC->PREDIVxCFGR[2] = 1;
    RCC->FINDIVxCFGR[2] = 0x40;
    RCC->XBARxCFGR[2] = 0x41;

    // Interface GIC400 via crossbar (5) - PLL5 / 3 = 400 MHz
    RCC->PREDIVxCFGR[5] = 0;
    RCC->FINDIVxCFGR[5] = 0x42;
    RCC->XBARxCFGR[5] = 0x41;

    // clk_icn_p_stgen, clk_icn_p_gicv2m come from APB4 clock which is already running
    // clk_icn_p_syscpu1 if from AHB5
    //  just ungate these in RCC
    RCC->STGENCFGR |= RCC_STGENCFGR_STGENEN;
    RCC->GICV2MCFGR |= RCC_GICV2MCFGR_GICV2MEN;
    RCC->SYSCPU1CFGR |= RCC_SYSCPU1CFGR_SYSCPU1EN;

    // Now, set up the internal CA35SS PLL1
    // Note frequencies > 1200 MHz require a "F" chip and a VCPU boost
    clock_set_cpu(1500000000);

    // start the system timer
    clock_start_sys();

    // start STGEN
    STGENC->CNTCR = 0x1;
}


void clock_set_cpu(unsigned int freq)
{
    /* PLL1 enable procedure is the following:
        1. Clear CA35SS_SSC_PLL_ENABLE.NRESET_SWPLL.
        2. Configure PLL parameters in CA35SS_SSC_PLL_FREQ1 and
        CA35SS_SSC_PLL_FREQ2.
        3. Set CA35SS_SSC_PLL_ENABLE.PLL_EN.
        4. Poll CA35SS_SSC_PLL_ENABLE.LOCKP until it is 1.
        5. Set CA35SS_SSC__PLL_ENABLE.NRESET_SWPLL.
        6. Clear CA35SS_SSC_CHGCLKREQ.ARM_CHGCLKREQ.
        7. Poll CA35SS_SSC_CHGCLKREQ.ARM_CHGCLKACK until it is 0.

        The SSC registers are documented in RM 3.6

        In particular they are aliased to allow:
            write at address - set register to value
            write at address+4 - set all set bits
            write at address+8 - clear all set bits
            write at address+12 - toggle all set bits
    */
    
    // CA35SS @ 0x[45]8800000 - I cannot find an easy define to use unfortunately
    struct ssc_reg
    {
        volatile uint32_t value;
        volatile uint32_t set;
        volatile uint32_t clear;
        volatile uint32_t toggle;
    } __attribute__((packed));
    #define SSC_BASE 0x48800000
    [[maybe_unused]] volatile ssc_reg *CA35SS_SSC_CHGCLKREQx = (volatile ssc_reg *)(SSC_BASE + 0);
    [[maybe_unused]] volatile ssc_reg *CA35SS_SSC_BRMx = (volatile ssc_reg *)(SSC_BASE + 0x10);
    [[maybe_unused]] volatile ssc_reg *CA35SS_SSC_PLL_FREQ1_x = (volatile ssc_reg *)(SSC_BASE + 0x80);
    [[maybe_unused]] volatile ssc_reg *CA35SS_SSC_PLL_FREQ2_x = (volatile ssc_reg *)(SSC_BASE + 0x90);
    [[maybe_unused]] volatile ssc_reg *CA35SS_SSC_PLL_ENx = (volatile ssc_reg *)(SSC_BASE + 0xa0);
    
    /* From BOOTROM, we have the PLL already being used

        FREQ1 = 0x0002004b -> FREFDIV = 2, FBDIV = 75
        FREQ2 = 0xe        -> POSTDIV1 = 6, POSTDIV2 = 1
        EN = 0x7 - enabled, not in reset 
        
        This gives a 400 MHz clock from HSI64


        We use HSE40 as the reference clock so need to change this.

        VCOout is between 800 and 3200 MHz

        For frefdiv of 2, we can scale fbdiv between 60 and 150 to give
        VCOout of 1200 to 3000 MHz
        We can then post-divide by 2 to give 600 - 1500 MHz
        And post divide 600 by more to give smaller values.
    */

    // Disable use of PLL
    CA35SS_SSC_CHGCLKREQx->set = 0x1;
    while(!(CA35SS_SSC_CHGCLKREQx->value & 0x2));

    CA35SS_SSC_PLL_ENx->clear = 0x5;
    while(CA35SS_SSC_PLL_ENx->value & 0x5);

    // Get appropriate VCOout
    auto vcoout = (unsigned long)freq * 2UL;
    if(vcoout < 1200000000UL)
        vcoout = 1200000000UL;
    if(vcoout > 3000000000UL)
        vcoout = 3000000000UL;
    
    auto frefdiv = vcoout * 2UL / 40000000UL;

    // calculate postdivider for given VCOout
    uint32_t divider = (uint32_t)(vcoout / (unsigned long)freq);
    uint32_t postdiv2 = divider / 7;
    if(postdiv2 < 1) postdiv2 = 1;
    if(postdiv2 > 7) postdiv2 = 7;
    uint32_t postdiv1 = divider / postdiv2;
    if(postdiv1 < 2) postdiv1 = 2;
    if(postdiv1 > 7) postdiv1 = 7;

    // Program
    CA35SS_SSC_PLL_FREQ1_x->value = 0x00020000UL | frefdiv; 
    CA35SS_SSC_PLL_FREQ2_x->value = postdiv1 | (postdiv2 << 3);

    // Enable PLL
    CA35SS_SSC_PLL_ENx->set = 0x1;
    while(!(CA35SS_SSC_PLL_ENx->value & 0x2));
    CA35SS_SSC_PLL_ENx->set = 0x4;

    // Use it
    CA35SS_SSC_CHGCLKREQx->clear = 0x1;
    while(CA35SS_SSC_CHGCLKREQx->value & 0x2);

    cpu_freq = vcoout / postdiv1 / postdiv2;

    klog("cpu: frequency %llu\n", cpu_freq);
}

void clock_start_sys()
{
    /* We aim for a 10 ns timer here using TIM3 in 32-bit mode
        In theory the maximum timer input frequency is 200 MHz = 5 ns resolution but
        we scale back a bit for power consumption improvements.

       The majority of the timer clocks come from ck_icn_ls_mcu = 200 MHz
    */

    #define TIM_FREQ            100000000
    #define TIM_PRESCALE        (200000000 / TIM_FREQ)
    #define TIM_PRECISION_NS    (1000000000 / TIM_FREQ)

    *_tim_precision_ns = TIM_PRECISION_NS;
    
    RCC->TIM3CFGR |= RCC_TIM3CFGR_TIM3EN;
    RCC->TIM3CFGR &= ~RCC_TIM3CFGR_TIM3RST;
    (void)RCC->TIM3CFGR;

    TIM3->CR1 = 0;
    TIM3->CR2 = 0;
    TIM3->SMCR = 0;
    TIM3->DIER = TIM_DIER_UIE;
    TIM3->CCMR1 = 0;
    TIM3->CCMR2 = 0;
    TIM3->CCMR3 = 0;
    TIM3->PSC = TIM_PRESCALE - 1;
    TIM3->ARR = TIM_FREQ - 1;
    TIM3->CNT = 0;
    TIM3->CR1 = TIM_CR1_CEN;
}

void clock_irq_handler()
{
    TIM3->SR = 0;
    *_cur_s = *_cur_s + 1;
}

timespec clock_cur()
{
    if(!(RCC->TIM3CFGR & RCC_TIM3CFGR_TIM3EN))
        return { 0, 0 };
    while(true)
    {
        uint64_t _s_a = *_cur_s;
        uint64_t _cur_sc_ns = TIM3->CNT;
        uint64_t _s_b = *_cur_s;

        if(_s_a == _s_b)
        {
            timespec ret;
            ret.tv_nsec = _cur_sc_ns * TIM_PRECISION_NS;
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
