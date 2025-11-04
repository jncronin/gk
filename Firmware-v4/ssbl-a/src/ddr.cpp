#include <stm32mp2xx.h>
#include "ddr.h"
#include "pmic.h"
#include <cstdio>

static void ddr_set_pmic();
static void ddr_set_clocks();

void init_ddr()
{
    // see AN5723 and RM 15.4.6 p576

    ddr_set_pmic();
    ddr_set_clocks();
}

void ddr_set_pmic()
{
    // Now set stuff up for DDR on EV1 TODO: change for LPDDR on gk
    pmic_vreg vr_buck6 { pmic_vreg::Buck, 6, true, 1200, pmic_vreg::HP };
    pmic_vreg vr_refddr { pmic_vreg::RefDDR, 0, true };
    pmic_vreg vr_ldo3 { pmic_vreg::LDO, 3, true, 600, pmic_vreg::SinkSource };
    pmic_vreg vr_ldo5 { pmic_vreg::LDO, 5, true, 2500 };
    pmic_set(vr_buck6);
    pmic_set(vr_refddr);
    pmic_set(vr_ldo3);
    pmic_set(vr_ldo5);
    
    pmic_dump();
}

void ddr_set_clocks()
{
    // ck_icn_ddr is already enabled in init_clocks @ 600 MHz (crossbar 2)

    /* others as per RM24.1.9 p1000 */
    RCC->DDRCPCFGR |= RCC_DDRCPCFGR_DDRCPEN;                    // ck_icn_p_risaf4
    RCC->DDRITFCFGR |= RCC_DDRITFCFGR_DDRRST;
    RCC->DDRPHYCAPBCFGR |= RCC_DDRPHYCAPBCFGR_DDRPHYCAPBEN;     // ck_icn_p_ddrphyc
    RCC->DDRCAPBCFGR |= RCC_DDRCAPBCFGR_DDRCAPBEN;              // ck_icn_p_ddrc
    RCC->DDRITFCFGR &= ~RCC_DDRITFCFGR_DDRPHYDLP;
    RCC->DDRPHYCCFGR |= RCC_DDRPHYCCFGR_DDRPHYCEN;

    for(int i = 0; i < 1200; i++) __DSB();

    PWR->CR11 &= ~PWR_CR11_DDRRETDIS;

    for(int i = 0; i < 1200; i++) __DSB();

    RCC->DDRCAPBCFGR &= ~RCC_DDRCAPBCFGR_DDRCAPBRST;
    
    for(int i = 0; i < 1200; i++) __DSB();

    RCC->DDRCFGR |= RCC_DDRCFGR_DDRCFGEN;                       // ck_icn_p_ddrcfg

    // Provide ck_pll2_ref from HSI64
    RCC->MUXSELCFGR = RCC->MUXSELCFGR &~ RCC_MUXSELCFGR_MUXSEL6_Msk;    // MUXSEL6 = PLL2 confusingly
    (void)RCC->MUXSELCFGR;

    // Set up PLL2
    ddr_set_mt(1600);
}

void ddr_set_mt(uint32_t mt)
{
    // see 13.4 here
    bool bypass;
    uint32_t pll2_freq;
    if(mt < 667)
    {
        bypass = true;
        pll2_freq = mt * 500000;    // ddr freq in MHz
    }
    else
    {
        bypass = false;
        pll2_freq = mt * 125000;    // ddr freq / 4 in MHz (multiplied by 4 again in PHY block)
    }


    if(bypass)
    {
        DDRDBG->BYPASS_PCLKEN |= DDRDBG_BYPASS_PCLKEN_ENABLE;
    }
    else
    {
        DDRDBG->BYPASS_PCLKEN &= ~DDRDBG_BYPASS_PCLKEN_ENABLE;
    }

    RCC->PLL2CFGR1 &= ~RCC_PLL2CFGR1_PLLEN;

    // Aim for 8x target and post divide to give a reasonable VCO value.
    uint32_t vco_val = pll2_freq * 8;
    if(vco_val < 800000000) vco_val = 800000000;
    if(vco_val > 3200000000) vco_val = 3200000000;

    uint32_t fb_div = vco_val / 32000000;

    RCC->PLL2CFGR2 = (2U << RCC_PLL2CFGR2_FREFDIV_Pos) |
        (fb_div << RCC_PLL2CFGR2_FBDIV_Pos);
    RCC->PLL2CFGR4 = RCC_PLL2CFGR4_FOUTPOSTDIVEN;
    RCC->PLL2CFGR6 = 2U;
    RCC->PLL2CFGR7 = 4U;
    RCC->PLL2CFGR1 |= RCC_PLL2CFGR1_PLLEN;

    while(!(RCC->PLL2CFGR1 & RCC_PLL2CFGR1_PLLRDY));

    printf("DDR: PLL2 frequency set to %u MHz, %s=%u MT/s\n",
        vco_val / 8 / 1000000, bypass ? "bypass mode" : "",
        vco_val / 4 * (bypass ? 1 : 4) / 1000000);
}
