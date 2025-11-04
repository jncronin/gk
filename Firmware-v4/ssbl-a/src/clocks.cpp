#include <stm32mp2xx.h>
#include "clocks.h"

static uint32_t cpu_freq = 0;

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
    
    // Set up CPU2 clock, ck_icn_hs_mcu, to be PLL4 / 3 = 400 MHz
    RCC->FINDIVxCFGR[0] = 0x42U;
    while(RCC->FINDIVSR1 & (1U << 0));              // wait for finish changing
    RCC->XBARxCFGR[0] = 0x40U;
    while(RCC->XBARxCFGR[0] & RCC_XBARxCFGR_XBARxSTS);     // wait for finish changing  

    // And ck_icn_ls_mcu to be half this
    RCC->LSMCUDIVR |= RCC_LSMCUDIVR_LSMCUDIV;
    while(!(RCC->LSMCUDIVR & RCC_LSMCUDIVR_LSMCUDIVRDY));
    
    // Provide ck_pll1_ref from HSI64
    RCC->MUXSELCFGR = RCC->MUXSELCFGR &~ RCC_MUXSELCFGR_MUXSEL5_Msk;    // MUXSEL5 = PLL1 confusingly
    (void)RCC->MUXSELCFGR;

    // Provide ck_ker_stgen from HSI64 via crossbar (33)
    RCC->PREDIVxCFGR[33] = 0;       // div 1
    RCC->FINDIVxCFGR[33] = 0x40;    // enabled, div 1
    RCC->XBARxCFGR[33] = 0x48;      // enabled, hsi_ker_ck

    // PLL4 output is 1200 MHz - use to interface AXI via crossbar (2), divided by 2
    RCC->PREDIVxCFGR[2] = 1;
    RCC->FINDIVxCFGR[2] = 0x40;
    RCC->XBARxCFGR[33] = 0x40;

    // Interface GIC400 via crossbar (5) - PLL4 / 3 = 400 MHz
    RCC->PREDIVxCFGR[5] = 0;
    RCC->FINDIVxCFGR[5] = 0x42;
    RCC->XBARxCFGR[5] = 0x40;

    // clk_icn_p_stgen, clk_icn_p_gicv2m come from APB4 clock which is already running
    // clk_icn_p_syscpu1 if from AHB5
    //  just ungate these in RCC
    RCC->STGENCFGR |= RCC_STGENCFGR_STGENEN;
    RCC->GICV2MCFGR |= RCC_GICV2MCFGR_GICV2MEN;
    RCC->SYSCPU1CFGR |= RCC_SYSCPU1CFGR_SYSCPU1EN;

    // Now, set up the internal CA35SS PLL1
    clock_set_cpu(1200000000);
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
        
        This gives a 400 MHz clock */

    // Disable use of PLL
    CA35SS_SSC_CHGCLKREQx->set = 0x1;
    while(!(CA35SS_SSC_CHGCLKREQx->value & 0x2));

    CA35SS_SSC_PLL_ENx->clear = 0x5;
    while(CA35SS_SSC_PLL_ENx->value & 0x5);

    // the BOOTROM configured clock is 2400 MHz - divide appropriately
    uint32_t divider = (uint32_t)(2400000000ULL / (uint64_t)freq);
    uint32_t postdiv2 = divider / 7;
    if(postdiv2 < 1) postdiv2 = 1;
    if(postdiv2 > 7) postdiv2 = 7;
    uint32_t postdiv1 = divider / postdiv2;
    if(postdiv1 < 2) postdiv1 = 2;
    if(postdiv1 > 7) postdiv1 = 7;

    // Program
    CA35SS_SSC_PLL_FREQ1_x->value = 0x0002004b; // as per BOOTROM
    CA35SS_SSC_PLL_FREQ2_x->value = postdiv1 | (postdiv2 << 3);

    // Enable PLL
    CA35SS_SSC_PLL_ENx->set = 0x1;
    while(!(CA35SS_SSC_PLL_ENx->value & 0x2));
    CA35SS_SSC_PLL_ENx->set = 0x4;

    // Use it
    CA35SS_SSC_CHGCLKREQx->clear = 0x1;
    while(CA35SS_SSC_CHGCLKREQx->value & 0x2);

    cpu_freq = 2400000000UL / postdiv1 / postdiv2;
}
