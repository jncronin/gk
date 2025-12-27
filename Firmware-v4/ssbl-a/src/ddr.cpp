#include <stm32mp2xx.h>
#include "ddr.h"
#include "pmic.h"
#include "logger.h"
#include "clocks.h"

#ifndef BIT
#define BIT(x)  (1ULL << x)
#endif

#ifndef MAX
#define MAX(a, b) (((a) < (b)) ? (b) : (a))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define GENMASK_32(high, low) \
	((~0UL >> (32U - 1U - (high))) ^ (((1UL << (low)) - 1UL)))
#define GENMASK_64(high, low) \
	((~0ULL >> (64U - 1U - (high))) ^ (((1ULL << (low)) - 1ULL)))
#define GENMASK GENMASK_64

#define U uintptr_t

#define STM32MP_DDR_FW_DMEM_OFFSET		U(0x400)
#define STM32MP_DDR_FW_IMEM_OFFSET		U(0x800)

#define STM32MP2X 1
#define STM32MP_DDR_DUAL_AXI_PORT 1

static uint64_t timeout_init_us(uint64_t us);
static bool timeout_elapsed(uint64_t tout);
static void mmio_write_16(uintptr_t reg, uint16_t val);
static uint16_t mmio_read_16(uintptr_t reg);
static void mmio_write_32(uintptr_t reg, uint32_t val);
static uint32_t mmio_read_32(uintptr_t reg);
static void mmio_clrbits_32(uintptr_t reg, uint32_t val);
static void mmio_setbits_32(uintptr_t reg, uint32_t val);
static void mmio_clrsetbits_32(uintptr_t reg, uint32_t mask, uint32_t val);
static void panic();

enum ddr_type {
	STM32MP_DDR3,
	STM32MP_DDR4,
	STM32MP_LPDDR4
};

// TODO: select appropriate DDR driver here.  These come from CubeMX projects renamed from stm32mp25-mx.dtsi
//#define STM32MP_DDR4_TYPE 1
#define STM32MP_LPDDR4_TYPE 1
//#include "ddr_configs/stm32mp255f-ev1-ddr.h"
#include "ddr_configs/stm32mp255f-IS43LQ16512A-062BLI-800MHz-LPDDR4.h"
//#include "ddr_configs/stm32mp255f-IS43LQ16512A-062BLI-800MHz-LPDDR4-RP1_5.h"

#if STM32MP_DDR3_TYPE
extern int _binary_ddr3_pmu_train_bin;
#define STM32MP_DDR_FW_BASE ((uintptr_t)&_binary_ddr3_pmu_train_bin)
#endif
#if STM32MP_DDR4_TYPE
extern int _binary_ddr4_pmu_train_bin;
#define STM32MP_DDR_FW_BASE ((uintptr_t)&_binary_ddr4_pmu_train_bin)
#endif
#if STM32MP_LPDDR4_TYPE
extern int _binary_lpddr4_pmu_train_bin;
#define STM32MP_DDR_FW_BASE ((uintptr_t)&_binary_lpddr4_pmu_train_bin)
#endif


// Pull in tf-a phyinit driver
#include "stm32mp2_ddr.h"
#include "stm32mp2_pwr.h"
#include "stm32mp25_rcc.h"
static int stm32mp_board_ddr_power_init(ddr_type ddrtype);
#include "ddrphy_phyinit_sequence.c"
#include "ddrphy_phyinit_calcmb.c"
#include "ddrphy_phyinit_initstruct.c"
#include "ddrphy_phyinit_c_initphyconfig.c"
#include "ddrphy_phyinit_softsetmb.c"
#include "ddrphy_phyinit_progcsrskiptrain.c"
#include "ddrphy_phyinit_reginterface.c"
#include "ddrphy_phyinit_mapdrvstren.c"
#include "ddrphy_phyinit_isdbytedisabled.c"
#include "ddrphy_phyinit_loadpieprodcode.c"
#include "ddrphy_phyinit_d_loadimem.c"
#include "ddrphy_phyinit_f_loaddmem.c"
#include "ddrphy_phyinit_g_execfw.c"
#include "ddrphy_phyinit_i_loadpieimage.c"
#include "ddrphy_phyinit_usercustom_custompretrain.c"
#include "ddrphy_phyinit_usercustom_g_waitfwdone.c"
#include "ddrphy_phyinit_usercustom_saveretregs.c"
#include "ddrphy_phyinit_writeoutmem.c"
#include "ddrphy_phyinit_restore_sequence.c"
#include "stm32mp_ddr.c"
#include "stm32mp2_ddr.c"
#include "stm32mp2_ddr_helpers.c"

#if 0
[[maybe_unused]] static void ddr_set_pmic();
#endif
[[maybe_unused]] static void ddr_set_clocks();
#if 0
[[maybe_unused]] static void ddr_static_config();
#endif
static void ddr_populate_stm_conf(stm32mp_ddr_config *conf);

static stm32mp_ddr_priv ddr_priv_data { 0 };

void init_ddr()
{
    // see AN5723 and RM 15.4.6 p576

    //ddr_set_pmic();
    ddr_set_clocks();
    //ddr_static_config();

    //RCC->DDRPHYCAPBCFGR &= ~RCC_DDRPHYCAPBCFGR_DDRPHYCAPBRST;
    //RCC->DDRCPCFGR &= ~RCC_DDRCPCFGR_DDRCPRST;

	struct stm32mp_ddr_priv *priv = &ddr_priv_data;

	VERBOSE("STM32MP DDR probe\n");

	priv->ctl = (struct stm32mp_ddrctl *)DDRC;
	priv->phy = (struct stm32mp_ddrphy *)DDRPHYC_BASE;
	priv->pwr = PWR_BASE;
	priv->rcc = RCC_BASE;

	priv->info.base = 0x80000000;
	priv->info.size = 0;

    stm32mp_ddr_config conf;
    ddr_populate_stm_conf(&conf);
    
	stm32mp2_ddr_init(priv, &conf);
	ddr_set_sr_mode(ddr_read_sr_mode());

    // Enable AXI clocks
    RCC->DDRCPCFGR &= ~RCC_DDRCPCFGR_DDRCPRST;

    klog("DDR: init complete\n");
}

#if 0
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
#endif

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

    PWR->CR11 |= PWR_CR11_DDRRETDIS;

    for(int i = 0; i < 1200; i++) __DSB();

    RCC->DDRCAPBCFGR &= ~RCC_DDRCAPBCFGR_DDRCAPBRST;
    
    for(int i = 0; i < 1200; i++) __DSB();

    RCC->DDRCFGR |= RCC_DDRCFGR_DDRCFGEN;                       // ck_icn_p_ddrcfg

    // release blocks from reset
    RCC->DDRCFGR &= ~RCC_DDRCFGR_DDRCFGRST;
    RCC->DDRITFCFGR &= ~RCC_DDRITFCFGR_DDRRST;

    // Provide ck_pll2_ref from HSI64
    RCC->MUXSELCFGR = RCC->MUXSELCFGR &~ RCC_MUXSELCFGR_MUXSEL6_Msk;    // MUXSEL6 = PLL2 confusingly
    (void)RCC->MUXSELCFGR;

    // Set up PLL2
    ddr_set_mt(DDR_MEM_SPEED * 2 / 1000);
    //ddr_set_mt(200);
}

void ddr_set_mt(uint32_t mt)
{
    // see 13.4 here
    bool bypass;
    uint32_t pll2_freq;
    if(mt < 667)
    {
        bypass = true;
        pll2_freq = mt * 1000000;    // ddr data rate in Hz
    }
    else
    {
        bypass = false;
        pll2_freq = mt * 250000;    // ddr data rate / 4 in Hz
    }

    // Dump all RCC registers
    klog("DDRCPCFGR:        %08x\n", RCC->DDRCPCFGR);
    klog("DDRITFCFGR:       %08x\n", RCC->DDRITFCFGR);
    klog("DDRPHYCAPBCFGR:   %08x\n", RCC->DDRPHYCAPBCFGR);
    klog("DDRCAPBCFGR:      %08x\n", RCC->DDRCAPBCFGR);
    klog("DDRITFCFGR:       %08x\n", RCC->DDRITFCFGR);
    klog("DDRPHYCCFGR:      %08x\n", RCC->DDRPHYCCFGR);
    klog("DDRCFGR:          %08x\n", RCC->DDRCFGR);
    klog("DBGCFGR:          %08x\n", RCC->DBGCFGR);

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
    uint32_t divider = 8;
    uint32_t vco_val = 0;
    while(divider > 0 && divider <= 16)
    {
        vco_val = pll2_freq * divider;
        if(vco_val < 800000000)
        {
            divider /= 2;
            continue;
        }
        if(vco_val > 3200000000)
        {
            divider *= 2;
            continue;
        }
        break;
    }
    if(divider == 0 || divider > 16)
    {
        klog("ddr: failed to set pll2 for %u MT\n", mt);
        while(true);
    }
    uint32_t div1 = 1;
    uint32_t div2 = 1;
    switch(divider)
    {
        case 1:
            div1 = 1;
            div2 = 1;
            break;
        case 2:
            div1 = 1;
            div2 = 2;
            break;
        case 4:
            div1 = 2;
            div2 = 2;
            break;
        case 8:
            div1 = 2;
            div2 = 4;
            break;
        case 16:
            div1 = 4;
            div2 = 4;
            break;
    }

    uint32_t fb_div = vco_val / 32000000;

    RCC->PLL2CFGR2 = (2U << RCC_PLL2CFGR2_FREFDIV_Pos) |
        (fb_div << RCC_PLL2CFGR2_FBDIV_Pos);
    RCC->PLL2CFGR4 = RCC_PLL2CFGR4_FOUTPOSTDIVEN;
    RCC->PLL2CFGR6 = div1;
    RCC->PLL2CFGR7 = div2;
    RCC->PLL2CFGR1 |= RCC_PLL2CFGR1_PLLEN;

    while(!(RCC->PLL2CFGR1 & RCC_PLL2CFGR1_PLLRDY));

    klog("DDR: PLL2 frequency set to %u MHz, %s=%u MT/s\n",
        vco_val / divider / 1000000, bypass ? "bypass mode" : "",
        vco_val / divider * (bypass ? 1 : 4) / 1000000);
}

#if 0
#define DDR(x) DDRC->x = DDR_ ## x
void ddr_static_config()
{
    DDR(MSTR);
    DDR(MRCTRL0);
    DDR(MRCTRL1);
    DDR(MRCTRL2);
    DDR(DERATEEN);
    DDR(DERATEINT);
    DDR(DERATECTL);
    DDR(PWRCTL);
    DDR(PWRTMG);
    DDR(HWLPCTL);
    DDR(RFSHCTL0);
    DDR(RFSHCTL1);
    DDR(RFSHCTL3);
    DDR(RFSHTMG);
    DDR(RFSHTMG1);
    DDR(CRCPARCTL0);
    DDR(CRCPARCTL1);
    DDR(INIT0);
    DDR(INIT1);
    DDR(INIT2);
    DDR(INIT3);
    DDR(INIT4);
    DDR(INIT5);
    DDR(INIT6);
    DDR(INIT7);
    DDR(DIMMCTL);
    DDR(RANKCTL);
    DDR(RANKCTL1);
    DDR(DRAMTMG0);
    DDR(DRAMTMG1);
    DDR(DRAMTMG2);
    DDR(DRAMTMG3);
    DDR(DRAMTMG4);
    DDR(DRAMTMG5);
    DDR(DRAMTMG6);
    DDR(DRAMTMG7);
    DDR(DRAMTMG8);
    DDR(DRAMTMG9);
    DDR(DRAMTMG10);
    DDR(DRAMTMG11);
    DDR(DRAMTMG12);
    DDR(DRAMTMG13);
    DDR(DRAMTMG14);
    DDR(DRAMTMG15);
    DDR(ZQCTL0);
    DDR(ZQCTL1);
    DDR(ZQCTL2);
    DDR(DFITMG0);
    DDR(DFITMG1);
    DDR(DFILPCFG0);
    DDR(DFILPCFG1);
    DDR(DFIUPD0);
    DDR(DFIUPD1);
    DDR(DFIUPD2);
    DDR(DFIMISC);
    DDR(DFITMG2);
    DDR(DFITMG3);
    DDR(DBICTL);
    DDR(DFIPHYMSTR);
    DDR(ADDRMAP0);
    DDR(ADDRMAP1);
    DDR(ADDRMAP2);
    DDR(ADDRMAP3);
    DDR(ADDRMAP4);
    DDR(ADDRMAP5);
    DDR(ADDRMAP6);
    DDR(ADDRMAP7);
    DDR(ADDRMAP8);
    DDR(ADDRMAP9);
    DDR(ADDRMAP10);
    DDR(ADDRMAP11);
    DDR(ODTCFG);
    DDR(ODTMAP);
    DDR(SCHED);
    DDR(SCHED1);
    DDR(PERFHPR1);
    DDR(PERFLPR1);
    DDR(PERFWR1);
    DDR(SCHED3);
    DDR(SCHED4);
    DDR(DBG0);
    DDR(DBG1);
    DDR(DBGCMD);
    DDR(SWCTL);
    DDR(SWCTLSTATIC);
    DDR(POISONCFG);
    DDR(PCCFG);
    DDR(PCFGR_0);
    DDR(PCFGW_0);
    DDR(PCTRL_0);
    DDR(PCFGQOS0_0);
    DDR(PCFGQOS1_0);
    DDR(PCFGWQOS0_0);
    DDR(PCFGWQOS1_0);
    DDR(PCFGR_1);
    DDR(PCFGW_1);
    DDR(PCTRL_1);
    DDR(PCFGQOS0_1);
    DDR(PCFGQOS1_1);
    DDR(PCFGWQOS0_1);
    DDR(PCFGWQOS1_1);
}
#endif

void ddr_populate_stm_conf(stm32mp_ddr_config *conf)
{
    conf->info.name = DDR_MEM_NAME;
    conf->info.size = DDR_MEM_SIZE;
    conf->info.speed = DDR_MEM_SPEED;

    conf->self_refresh = false;

    conf->c_reg.mstr = DDR_MSTR;
    conf->c_reg.mrctrl0 = DDR_MRCTRL0;
    conf->c_reg.mrctrl1 = DDR_MRCTRL1;
    conf->c_reg.mrctrl2 = DDR_MRCTRL2;
    conf->c_reg.derateen = DDR_DERATEEN;
    conf->c_reg.derateint = DDR_DERATEINT;
    conf->c_reg.deratectl = DDR_DERATECTL;
    conf->c_reg.pwrctl = DDR_PWRCTL;
    conf->c_reg.pwrtmg = DDR_PWRTMG;
    conf->c_reg.hwlpctl = DDR_HWLPCTL;
    conf->c_reg.rfshctl0 = DDR_RFSHCTL0;
    conf->c_reg.rfshctl1 = DDR_RFSHCTL1;
    conf->c_reg.rfshctl3 = DDR_RFSHCTL3;
    conf->c_timing.rfshtmg = DDR_RFSHTMG;
    conf->c_timing.rfshtmg1 = DDR_RFSHTMG1;
    conf->c_reg.crcparctl0 = DDR_CRCPARCTL0;
    conf->c_reg.crcparctl1 = DDR_CRCPARCTL1;
    conf->c_reg.init0 = DDR_INIT0;
    conf->c_reg.init1 = DDR_INIT1;
    conf->c_reg.init2 = DDR_INIT2;
    conf->c_reg.init3 = DDR_INIT3;
    conf->c_reg.init4 = DDR_INIT4;
    conf->c_reg.init5 = DDR_INIT5;
    conf->c_reg.init6 = DDR_INIT6;
    conf->c_reg.init7 = DDR_INIT7;
    conf->c_reg.dimmctl = DDR_DIMMCTL;
    conf->c_reg.rankctl = DDR_RANKCTL;
    conf->c_reg.rankctl1 = DDR_RANKCTL1;
    conf->c_timing.dramtmg0 = DDR_DRAMTMG0;
    conf->c_timing.dramtmg1 = DDR_DRAMTMG1;
    conf->c_timing.dramtmg2 = DDR_DRAMTMG2;
    conf->c_timing.dramtmg3 = DDR_DRAMTMG3;
    conf->c_timing.dramtmg4 = DDR_DRAMTMG4;
    conf->c_timing.dramtmg5 = DDR_DRAMTMG5;
    conf->c_timing.dramtmg6 = DDR_DRAMTMG6;
    conf->c_timing.dramtmg7 = DDR_DRAMTMG7;
    conf->c_timing.dramtmg8 = DDR_DRAMTMG8;
    conf->c_timing.dramtmg9 = DDR_DRAMTMG9;
    conf->c_timing.dramtmg10 = DDR_DRAMTMG10;
    conf->c_timing.dramtmg11 = DDR_DRAMTMG11;
    conf->c_timing.dramtmg12 = DDR_DRAMTMG12;
    conf->c_timing.dramtmg13 = DDR_DRAMTMG13;
    conf->c_timing.dramtmg14 = DDR_DRAMTMG14;
    conf->c_timing.dramtmg15 = DDR_DRAMTMG15;
    conf->c_reg.zqctl0 = DDR_ZQCTL0;
    conf->c_reg.zqctl1 = DDR_ZQCTL1;
    conf->c_reg.zqctl2 = DDR_ZQCTL2;
    conf->c_reg.dfitmg0 = DDR_DFITMG0;
    conf->c_reg.dfitmg1 = DDR_DFITMG1;
    conf->c_reg.dfilpcfg0 = DDR_DFILPCFG0;
    conf->c_reg.dfilpcfg1= DDR_DFILPCFG1;
    conf->c_reg.dfiupd0 = DDR_DFIUPD0;
    conf->c_reg.dfiupd1= DDR_DFIUPD1;
    conf->c_reg.dfiupd2 = DDR_DFIUPD2;
    conf->c_reg.dfimisc = DDR_DFIMISC;
    conf->c_reg.dfitmg2 = DDR_DFITMG2;
    conf->c_reg.dfitmg3= DDR_DFITMG3;
    conf->c_reg.dbictl = DDR_DBICTL;
    conf->c_reg.dfiphymstr = DDR_DFIPHYMSTR;
    conf->c_map.addrmap0 = DDR_ADDRMAP0;
    conf->c_map.addrmap1 = DDR_ADDRMAP1;
    conf->c_map.addrmap2 = DDR_ADDRMAP2;
    conf->c_map.addrmap3 = DDR_ADDRMAP3;
    conf->c_map.addrmap4 = DDR_ADDRMAP4;
    conf->c_map.addrmap5 = DDR_ADDRMAP5;
    conf->c_map.addrmap6 = DDR_ADDRMAP6;
    conf->c_map.addrmap7 = DDR_ADDRMAP7;
    conf->c_map.addrmap8 = DDR_ADDRMAP8;
    conf->c_map.addrmap9 = DDR_ADDRMAP9;
    conf->c_map.addrmap10 = DDR_ADDRMAP10;
    conf->c_map.addrmap11 = DDR_ADDRMAP11;
    conf->c_timing.odtcfg = DDR_ODTCFG;
    conf->c_timing.odtmap= DDR_ODTMAP;
    conf->c_perf.sched = DDR_SCHED;
    conf->c_perf.sched1 = DDR_SCHED1;
    conf->c_perf.perfhpr1 = DDR_PERFHPR1;
    conf->c_perf.perflpr1 = DDR_PERFLPR1;
    conf->c_perf.perfwr1 = DDR_PERFWR1;
    conf->c_perf.sched3 = DDR_SCHED3;
    conf->c_perf.sched4 = DDR_SCHED4;
    conf->c_reg.dbg0 = DDR_DBG0;
    conf->c_reg.dbg1 = DDR_DBG1;
    conf->c_reg.dbgcmd = DDR_DBGCMD;
    conf->c_reg.swctl = DDR_SWCTL;
    conf->c_reg.swctlstatic = DDR_SWCTLSTATIC;
    conf->c_reg.poisoncfg = DDR_POISONCFG;
    conf->c_reg.pccfg = DDR_PCCFG;
    conf->c_perf.pcfgr_0 = DDR_PCFGR_0;
    conf->c_perf.pcfgw_0 = DDR_PCFGW_0;
    conf->c_perf.pctrl_0 = DDR_PCTRL_0;
    conf->c_perf.pcfgqos0_0 = DDR_PCFGQOS0_0;
    conf->c_perf.pcfgqos1_0 = DDR_PCFGQOS1_0;
    conf->c_perf.pcfgwqos0_0 = DDR_PCFGWQOS0_0;
    conf->c_perf.pcfgwqos1_0 = DDR_PCFGWQOS1_0;
    conf->c_perf.pcfgr_1 = DDR_PCFGR_1;
    conf->c_perf.pcfgw_1 = DDR_PCFGW_1;
    conf->c_perf.pctrl_1 = DDR_PCTRL_1;
    conf->c_perf.pcfgqos0_1 = DDR_PCFGQOS0_1;
    conf->c_perf.pcfgqos1_1 = DDR_PCFGQOS1_1;
    conf->c_perf.pcfgwqos0_1 = DDR_PCFGWQOS0_1;
    conf->c_perf.pcfgwqos1_1 = DDR_PCFGWQOS1_1;

    conf->uib.dramtype = DDR_UIB_DRAMTYPE;
    conf->uib.dimmtype = DDR_UIB_DIMMTYPE;
    conf->uib.lp4xmode = DDR_UIB_LP4XMODE;
    conf->uib.numdbyte = DDR_UIB_NUMDBYTE;
    conf->uib.numactivedbytedfi0 = DDR_UIB_NUMACTIVEDBYTEDFI0;
    conf->uib.numactivedbytedfi1 = DDR_UIB_NUMACTIVEDBYTEDFI1;
    conf->uib.numanib = DDR_UIB_NUMANIB;
    conf->uib.numrank_dfi0 = DDR_UIB_NUMRANK_DFI0;
    conf->uib.numrank_dfi1 = DDR_UIB_NUMRANK_DFI1;
    conf->uib.dramdatawidth = DDR_UIB_DRAMDATAWIDTH;
    conf->uib.numpstates = DDR_UIB_NUMPSTATES;
    conf->uib.frequency = DDR_UIB_FREQUENCY_0;
    conf->uib.pllbypass = DDR_UIB_PLLBYPASS_0;
    conf->uib.dfifreqratio = DDR_UIB_DFIFREQRATIO_0;
    conf->uib.dfi1exists = DDR_UIB_DFI1EXISTS;
    conf->uib.train2d = DDR_UIB_TRAIN2D;
    conf->uib.hardmacrover = DDR_UIB_HARDMACROVER;
    conf->uib.readdbienable = DDR_UIB_READDBIENABLE_0;
    conf->uib.dfimode = DDR_UIB_DFIMODE;
    conf->uia.lp4rxpreamblemode = DDR_UIA_LP4RXPREAMBLEMODE_0;
    conf->uia.lp4postambleext = DDR_UIA_LP4POSTAMBLEEXT_0;
    conf->uia.d4rxpreamblelength = DDR_UIA_D4RXPREAMBLELENGTH_0;
    conf->uia.d4txpreamblelength = DDR_UIA_D4TXPREAMBLELENGTH_0;
    conf->uia.extcalresval = DDR_UIA_EXTCALRESVAL;
    conf->uia.is2ttiming = DDR_UIA_IS2TTIMING_0;
    conf->uia.odtimpedance = DDR_UIA_ODTIMPEDANCE_0;
    conf->uia.tximpedance = DDR_UIA_TXIMPEDANCE_0;
    conf->uia.atximpedance = DDR_UIA_ATXIMPEDANCE;
    conf->uia.memalerten = DDR_UIA_MEMALERTEN;
    conf->uia.memalertpuimp = DDR_UIA_MEMALERTPUIMP;
    conf->uia.memalertvreflevel = DDR_UIA_MEMALERTVREFLEVEL;
    conf->uia.memalertsyncbypass = DDR_UIA_MEMALERTSYNCBYPASS;
    conf->uia.disdynadrtri = DDR_UIA_DISDYNADRTRI_0;
    conf->uia.phymstrtraininterval = DDR_UIA_PHYMSTRTRAININTERVAL_0;
    conf->uia.phymstrmaxreqtoack = DDR_UIA_PHYMSTRMAXREQTOACK_0;
    conf->uia.wdqsext = DDR_UIA_WDQSEXT;
    conf->uia.calinterval = DDR_UIA_CALINTERVAL;
    conf->uia.calonce = DDR_UIA_CALONCE;
    conf->uia.lp4rl = DDR_UIA_LP4RL_0;
    conf->uia.lp4wl = DDR_UIA_LP4WL_0;
    conf->uia.lp4wls = DDR_UIA_LP4WLS_0;
    conf->uia.lp4dbird = DDR_UIA_LP4DBIRD_0;
    conf->uia.lp4dbiwr = DDR_UIA_LP4DBIWR_0;
    conf->uia.lp4nwr = DDR_UIA_LP4NWR_0;
    conf->uia.lp4lowpowerdrv = DDR_UIA_LP4LOWPOWERDRV;
    conf->uia.drambyteswap = DDR_UIA_DRAMBYTESWAP;
    conf->uia.rxenbackoff = DDR_UIA_RXENBACKOFF;
    conf->uia.trainsequencectrl = DDR_UIA_TRAINSEQUENCECTRL;
    conf->uia.snpsumctlopt = DDR_UIA_SNPSUMCTLOPT;
    conf->uia.snpsumctlf0rc5x = DDR_UIA_SNPSUMCTLF0RC5X_0;
    conf->uia.txslewrisedq = DDR_UIA_TXSLEWRISEDQ_0;
    conf->uia.txslewfalldq = DDR_UIA_TXSLEWFALLDQ_0;
    conf->uia.txslewriseac = DDR_UIA_TXSLEWRISEAC;
    conf->uia.txslewfallac = DDR_UIA_TXSLEWFALLAC;
    conf->uia.disableretraining = DDR_UIA_DISABLERETRAINING;
    conf->uia.disablephyupdate = DDR_UIA_DISABLEPHYUPDATE;
    conf->uia.enablehighclkskewfix = DDR_UIA_ENABLEHIGHCLKSKEWFIX;
    conf->uia.disableunusedaddrlns = DDR_UIA_DISABLEUNUSEDADDRLNS;
    conf->uia.phyinitsequencenum = DDR_UIA_PHYINITSEQUENCENUM;
    conf->uia.enabledficspolarityfix = DDR_UIA_ENABLEDFICSPOLARITYFIX;
    conf->uia.phyvref = DDR_UIA_PHYVREF;
    conf->uia.sequencectrl = DDR_UIA_SEQUENCECTRL_0;
    conf->uim.mr0 = DDR_UIM_MR0_0;
    conf->uim.mr1 = DDR_UIM_MR1_0;
    conf->uim.mr2 = DDR_UIM_MR2_0;
    conf->uim.mr3 = DDR_UIM_MR3_0;
    conf->uim.mr4 = DDR_UIM_MR4_0;
    conf->uim.mr5 = DDR_UIM_MR5_0;
    conf->uim.mr6 = DDR_UIM_MR6_0;
    conf->uim.mr11 = DDR_UIM_MR11_0;
    conf->uim.mr12 = DDR_UIM_MR12_0;
    conf->uim.mr13 = DDR_UIM_MR13_0;
    conf->uim.mr14 = DDR_UIM_MR14_0;
    conf->uim.mr22 = DDR_UIM_MR22_0;
    conf->uis.swizzle[0] = DDR_UIS_SWIZZLE_0;
    conf->uis.swizzle[1] = DDR_UIS_SWIZZLE_1;
    conf->uis.swizzle[2] = DDR_UIS_SWIZZLE_2;
    conf->uis.swizzle[3] = DDR_UIS_SWIZZLE_3;
    conf->uis.swizzle[4] = DDR_UIS_SWIZZLE_4;
    conf->uis.swizzle[5] = DDR_UIS_SWIZZLE_5;
    conf->uis.swizzle[6] = DDR_UIS_SWIZZLE_6;
    conf->uis.swizzle[7] = DDR_UIS_SWIZZLE_7;
    conf->uis.swizzle[8] = DDR_UIS_SWIZZLE_8;
    conf->uis.swizzle[9] = DDR_UIS_SWIZZLE_9;
    conf->uis.swizzle[10] = DDR_UIS_SWIZZLE_10;
    conf->uis.swizzle[11] = DDR_UIS_SWIZZLE_11;
    conf->uis.swizzle[12] = DDR_UIS_SWIZZLE_12;
    conf->uis.swizzle[13] = DDR_UIS_SWIZZLE_13;
    conf->uis.swizzle[14] = DDR_UIS_SWIZZLE_14;
    conf->uis.swizzle[15] = DDR_UIS_SWIZZLE_15;
    conf->uis.swizzle[16] = DDR_UIS_SWIZZLE_16;
    conf->uis.swizzle[17] = DDR_UIS_SWIZZLE_17;
    conf->uis.swizzle[18] = DDR_UIS_SWIZZLE_18;
    conf->uis.swizzle[19] = DDR_UIS_SWIZZLE_19;
    conf->uis.swizzle[20] = DDR_UIS_SWIZZLE_20;
    conf->uis.swizzle[21] = DDR_UIS_SWIZZLE_21;
    conf->uis.swizzle[22] = DDR_UIS_SWIZZLE_22;
    conf->uis.swizzle[23] = DDR_UIS_SWIZZLE_23;
    conf->uis.swizzle[24] = DDR_UIS_SWIZZLE_24;
    conf->uis.swizzle[25] = DDR_UIS_SWIZZLE_25;
    conf->uis.swizzle[26] = DDR_UIS_SWIZZLE_26;
    conf->uis.swizzle[27] = DDR_UIS_SWIZZLE_27;
    conf->uis.swizzle[28] = DDR_UIS_SWIZZLE_28;
    conf->uis.swizzle[29] = DDR_UIS_SWIZZLE_29;
    conf->uis.swizzle[30] = DDR_UIS_SWIZZLE_30;
    conf->uis.swizzle[31] = DDR_UIS_SWIZZLE_31;
    conf->uis.swizzle[32] = DDR_UIS_SWIZZLE_32;
    conf->uis.swizzle[33] = DDR_UIS_SWIZZLE_33;
    conf->uis.swizzle[34] = DDR_UIS_SWIZZLE_34;
    conf->uis.swizzle[35] = DDR_UIS_SWIZZLE_35;
    conf->uis.swizzle[36] = DDR_UIS_SWIZZLE_36;
    conf->uis.swizzle[37] = DDR_UIS_SWIZZLE_37;
    conf->uis.swizzle[38] = DDR_UIS_SWIZZLE_38;
    conf->uis.swizzle[39] = DDR_UIS_SWIZZLE_39;
    conf->uis.swizzle[40] = DDR_UIS_SWIZZLE_40;
    conf->uis.swizzle[41] = DDR_UIS_SWIZZLE_41;
    conf->uis.swizzle[42] = DDR_UIS_SWIZZLE_42;
    conf->uis.swizzle[43] = DDR_UIS_SWIZZLE_43;
}

void mmio_write_16(uintptr_t addr, uint16_t val)
{
    *(volatile uint16_t *)addr = val;
    __asm__ volatile("dmb st\n" ::: "memory");
}

uint16_t mmio_read_16(uintptr_t addr)
{
    __asm__ volatile("dsb sy\n" ::: "memory");
    return *(volatile uint16_t *)addr;
}

void mmio_write_32(uintptr_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
    __asm__ volatile("dmb st\n" ::: "memory");
}

uint32_t mmio_read_32(uintptr_t addr)
{
    __asm__ volatile("dsb sy\n" ::: "memory");
    return *(volatile uint32_t *)addr;
}

uint64_t timeout_init_us(uint64_t us)
{
    return clock_cur_us() + us;
}

bool timeout_elapsed(uint64_t tout)
{
    return clock_cur_us() > tout;
}

void mmio_setbits_32(uintptr_t reg, uint32_t val)
{
    auto r = mmio_read_32(reg);
    r |= val;
    mmio_write_32(reg, r);
}

void mmio_clrbits_32(uintptr_t reg, uint32_t val)
{
    auto r = mmio_read_32(reg);
    r &= ~val;
    mmio_write_32(reg, r);
}

void mmio_clrsetbits_32(uintptr_t reg, uint32_t mask, uint32_t val)
{
    auto r = mmio_read_32(reg);
    r &= ~mask;
    r |= val;
    mmio_write_32(reg, r);
}

void panic()
{
    klog("DDR: panic\n");
    while(true);
}

int stm32mp_board_ddr_power_init(ddr_type type)
{
    if(type == STM32MP_DDR4)
    {
        if(pmic_read_register(0) == 0x22)
        {
            klog("DDR: DDR4 memory requested but device is STPMIC25B for LPDDR on GK.  Halting");
            while(true);
        }
        pmic_vreg vr_buck6 { pmic_vreg::Buck, 6, true, 1200, pmic_vreg::HP };
        pmic_vreg vr_refddr { pmic_vreg::RefDDR, 0, true };
        pmic_vreg vr_ldo3 { pmic_vreg::LDO, 3, true, 600, pmic_vreg::SinkSource };
        pmic_vreg vr_ldo5 { pmic_vreg::LDO, 5, true, 2500 };
        pmic_set(vr_buck6);
        pmic_set(vr_refddr);
        pmic_set(vr_ldo3);
        pmic_set(vr_ldo5);
    
        return 0;
    }
    else if(type == STM32MP_LPDDR4)
    {
        if(pmic_read_register(0) != 0x22)
        {
            klog("DDR: LPDDR4 memory requested but device is not STPMIC25B.  Halting");
            while(true);
        }

        // VDD1 (1.8V from LDO3 in bypass mode) ramps up first within 0.5 - 2ms,
        //  then 2ms after it is valid, VDD2 (1.1V from BUCK6) ramps up
        pmic_vreg vr_ldo3 { pmic_vreg::LDO, 3, true, 1800, pmic_vreg::Bypass };
        pmic_vreg vr_buck6 { pmic_vreg::Buck, 6, true, 1100, pmic_vreg::HP };
        pmic_set(vr_ldo3);
        udelay(4000);
        pmic_set(vr_buck6);

        return 0;
    }
    return -1;
}

uint64_t ddr_get_size()
{
    return DDR_MEM_SIZE;
}
