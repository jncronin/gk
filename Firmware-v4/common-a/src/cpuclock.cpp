#include <cstdint>
#include "vmem.h"
#include "logger.h"

static uint32_t cpu_freq = 1200000000;

unsigned int clock_get_cpu()
{
    return cpu_freq;
}

unsigned int clock_set_cpu(unsigned int freq)
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
    #define SSC_BASE PMEM_TO_VMEM(0x48800000)
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
    auto test_post_divide = (freq <= 1500000000UL) ? 2UL : 1UL;
    auto vcoout = (unsigned long)freq * test_post_divide;
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
    if(postdiv1 < 1) postdiv1 = 1;
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

    return cpu_freq;
}
