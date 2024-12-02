#include <stm32h7rsxx.h>
#include "pwr.h"
#include "clocks.h"
#include "osmutex.h"
#include "gk_conf.h"

extern volatile uint64_t _cur_ms;
extern struct timespec toffset;
uint32_t SystemCoreClock = 0;

time_t timegm (struct tm* tim_p);

static void enable_backup_sram();

extern "C" INTFLASH_FUNCTION void init_clocks()
{
    SystemCoreClock = 64000000U;

    /* Clock syscfg/sbs */
    RCC->APB4ENR |= RCC_APB4ENR_SBSEN;
    (void)RCC->APB4ENR;

    /* Set up the clock system
        We have a 24 MHz HSE crystal oscillator and 64 MHz HSI */
    
    // Enable HSE
    auto cr = RCC->CR;
    cr |= RCC_CR_HSEON | RCC_CR_HSECSSON;
    cr &= ~RCC_CR_HSEBYP;
    RCC->CR = cr;
    while(!(RCC->CR & RCC_CR_HSERDY));

    enable_backup_sram();

    _cur_ms = 0ULL;

    /* Configure PLLs from 24 MHz HSE:
        PLL1 = HSE24 / M3 * 75 = 600
            /P1 = 600 -> SYSCLK 
            
    */

    // Ensure sysclk is HSI64 and PLLs disabled
    RCC->CFGR &= ~RCC_CFGR_SW_Msk;
    while(RCC->CFGR & RCC_CFGR_SWS);
    RCC->CR &= ~(RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON);
    while(RCC->CR & (RCC_CR_PLL1RDY | RCC_CR_PLL2RDY | RCC_CR_PLL3RDY));
    RCC->CR &= ~RCC_CR_HSIDIV;

    // Set vreg to output higher VCORE

    // for I2C set clk to HSI64
    // boost VCORE
    bool vos = pwr_set_vos_high() == 0;

    // set PLL M dividers
    RCC->PLLCKSELR = (3U << RCC_PLLCKSELR_DIVM3_Pos) |
        (4U << RCC_PLLCKSELR_DIVM2_Pos) |
        (3U << RCC_PLLCKSELR_DIVM1_Pos) |
        (2U << RCC_PLLCKSELR_PLLSRC_Pos);
    
    RCC->PLLCFGR = 0U;
    RCC->PLL1FRACR = 0U;
    RCC->PLL2FRACR = 0U;
    RCC->PLL3FRACR = 0U;

    /* PLL1 -> CPU at 600 (VOS high -> 8*75/1) or 400 (VOS low -> 8*100/2) */
    RCC->PLL1DIVR1 = (1U << RCC_PLL1DIVR1_DIVQ_Pos) |
        ((vos ? 0U : 1U) << RCC_PLL1DIVR1_DIVP_Pos) |
        ((vos ? 74U : 99U) << RCC_PLL1DIVR1_DIVN_Pos);
    RCC->PLL1DIVR2 = (1U << RCC_PLL1DIVR2_DIVS_Pos);

    /* PLL2:
        P -> LPTIM1 @ 32 MHz
        S -> XSPI1 @ 384 MHz (then prescaled /2)
        T -> SD @ 192 MHz */
    RCC->PLL2DIVR1 = (1U << RCC_PLL2DIVR1_DIVR_Pos) |
        (1U << RCC_PLL2DIVR1_DIVQ_Pos) |
        (23U << RCC_PLL2DIVR1_DIVP_Pos) |
        (127U << RCC_PLL2DIVR1_DIVN_Pos);
    RCC->PLL2DIVR2 = (3U << RCC_PLL2DIVR2_DIVT_Pos) |
        (1U << RCC_PLL2DIVR2_DIVS_Pos);

    /* PLL3:
        P -> SPI2,3 @240 MHz
        Q -> SPI4,5,6 @240 MHz
        R -> LTDC @24 MHz (60 Hz * 800 * 500) */
    RCC->PLL3DIVR1 = (19U << RCC_PLL3DIVR1_DIVR_Pos) |
        (1U << RCC_PLL3DIVR1_DIVQ_Pos) |
        (1U << RCC_PLL3DIVR1_DIVP_Pos) |
        (59U << RCC_PLL3DIVR1_DIVN_Pos);
    RCC->PLL3DIVR2 = (1U << RCC_PLL3DIVR2_DIVS_Pos);

    /* Enable the requested outputs */
    RCC->PLLCFGR = RCC_PLLCFGR_PLL3REN |
        RCC_PLLCFGR_PLL3QEN |
        RCC_PLLCFGR_PLL3PEN |
        RCC_PLLCFGR_PLL2TEN |
        RCC_PLLCFGR_PLL2PEN |
        RCC_PLLCFGR_PLL2SEN |
        RCC_PLLCFGR_PLL1PEN |
        (3U << RCC_PLLCFGR_PLL3RGE_Pos) |
        (3U << RCC_PLLCFGR_PLL2RGE_Pos) |
        (3U << RCC_PLLCFGR_PLL1RGE_Pos);

    RCC->CR |= (RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON);
    while(!(RCC->CR & RCC_CR_PLL1RDY));
    while(!(RCC->CR & RCC_CR_PLL2RDY));
    while(!(RCC->CR & RCC_CR_PLL3RDY));

    /* Prepare for ssyclk switch.
        Sysclk will be 600 (or 400) MHz
        /CPRE = 1 -> CPUCLK 600
        /BMPRE = 2 -> HCLK  300
        /PPRE[1,2,4,5] = 2 -> APB clocks 150 */
    RCC->CDCFGR = 0U << RCC_CDCFGR_CPRE_Pos;
    RCC->BMCFGR = 8U << RCC_BMCFGR_BMPRE_Pos;
    RCC->APBCFGR = (4U << RCC_APBCFGR_PPRE5_Pos) |
        (4U << RCC_APBCFGR_PPRE4_Pos) |
        (4U << RCC_APBCFGR_PPRE2_Pos) |
        (4U << RCC_APBCFGR_PPRE1_Pos);

    /* Set up internal flash wait states */
    if(vos)
    {
        FLASH->ACR = (3U << FLASH_ACR_WRHIGHFREQ_Pos) |
            (7U << FLASH_ACR_LATENCY_Pos);
    }
    else
    {
        FLASH->ACR = (2U << FLASH_ACR_WRHIGHFREQ_Pos) |
            (5U << FLASH_ACR_LATENCY_Pos);
    }
    RCC->APB4ENR |= RCC_APB4ENR_SBSEN;
    (void)RCC->APB4ENR;
    SBS->PMCR |= SBS_PMCR_AXISRAM_WS;

    /* Set sysclk to use PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) |
        (3U << RCC_CFGR_SW_Pos);
    while((RCC->CFGR & RCC_CFGR_SWS_Msk) != (3U << RCC_CFGR_SWS_Pos));

    if(vos)
        SystemCoreClock = 600000000U;
    else
        SystemCoreClock = 400000000U;

    /* Configure peripheral clocks */
    RCC->CCIPR1 = (2U << RCC_CCIPR1_CKPERSEL_Pos) |     // PERCLK = HSE24
        (2U << RCC_CCIPR1_ADCSEL_Pos) |                 // ADC = HSE24
        (3U << RCC_CCIPR1_OTGFSSEL_Pos) |               // OTGFS from USBPHYC
        (0U << RCC_CCIPR1_USBPHYCSEL_Pos) |             // USBPHY from HSE
        (0xaU << RCC_CCIPR1_USBREFCKSEL_Pos) |          // USBPHY is 24 MHz
        (0U << RCC_CCIPR1_XSPI2SEL_Pos) |               // XSPI2 from hclk5=300MHz
        (1U << RCC_CCIPR1_XSPI1SEL_Pos) |               // XSPI1 from PLL2S=384MHz
        (1U << RCC_CCIPR1_SDMMC12SEL_Pos);              // SDMMC from PLL2T=192MHz
    RCC->CCIPR2 = (1U << RCC_CCIPR2_LPTIM1SEL_Pos) |    // LPTIM1 = PLL2P=32MHz
        (2U << RCC_CCIPR2_I2C1_I3C1SEL_Pos) |           // I2C1 = HSI64
        (2U << RCC_CCIPR2_I2C23SEL_Pos) |               // I2C2/3 = HSI64
        (2U << RCC_CCIPR2_SPI23SEL_Pos) |               // SPI2/3 = PLL3P=240
        (3U << RCC_CCIPR2_UART234578SEL_Pos);           // UARTs = HSI64
    RCC->CCIPR3 = (2U << RCC_CCIPR3_SPI45SEL_Pos) |     // SPI3,4 = PLL3Q=240
    
        0 ;    // TODO: SAI needs I2S_CKIN to be running before selecting it
    RCC->CCIPR4 = (1U << RCC_CCIPR4_LPTIM45SEL_Pos) |   // LPTIM4,5 = PLL2P=32MHz
        (1U << RCC_CCIPR4_LPTIM23SEL_Pos) |             // LPTIM2,3 = PLL2P=32MHz
        (2U << RCC_CCIPR4_SPI6SEL_Pos) |                // SPI6  = PLL3Q=240
        (3U << RCC_CCIPR4_LPUART1SEL_Pos);              // LPUART = HSI64

    // Set up LPTIM1 as a 1 kHz tick
    RCC->APB1ENR1 |= RCC_APB1ENR1_LPTIM1EN;
    (void)RCC->APB1ENR1;

    LPTIM1->CR = 0;
    LPTIM1->CR = LPTIM_CR_RSTARE;
    (void)LPTIM1->CR;
    LPTIM1->CR = 0;

    LPTIM1->CFGR = 5UL << LPTIM_CFGR_PRESC_Pos;     // /32 => 1 MHz tick
    LPTIM1->DIER = LPTIM_DIER_ARRMIE;
    LPTIM1->CR = LPTIM_CR_ENABLE;
    LPTIM1->ARR = 999;                              // Reload every 1 kHz
    
    NVIC_EnableIRQ(LPTIM1_IRQn);
    __enable_irq();
    LPTIM1->CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;
}

extern "C" INTFLASH_FUNCTION void LPTIM1_IRQHandler()
{
    CriticalGuard cg;
    // do it this way round or the IRQ is still active on IRQ return
    LPTIM1->ICR = LPTIM_ICR_ARRMCF;
    _cur_ms++;
    __DMB();
}

kernel_time clock_cur()
{
    return kernel_time::from_us(clock_cur_us());
}

uint64_t clock_cur_ms()
{
    return clock_cur_us() / 1000ULL;
}

uint64_t clock_cur_us()
{
    /* The basic idea here is to try and read both LPTIM1->CNT and _cur_ms atomically.
        We need to account for the fact that on calling this function:
            1) interrupts may be enabled and so _cur_ms may change as we read LPTIM1->CNT
            2) interrupts may be disabled and so LPTIM1->CNT may rollover without a change in
                _cur_ms - luckily LPTIM1->ISR & ARRM will be set in this instance.
    */

    uint32_t cnt = 0U;
    uint64_t cms = 0U;
    uint32_t isr = 0U;

    while(true)
    {
        cms = _cur_ms;
        isr = LPTIM1->ISR;
        cnt = LPTIM1->CNT;

        auto isr2 = LPTIM1->ISR;
        auto cms2 = _cur_ms;

        if(isr == isr2 && cms == cms2) break;
    }

    auto ret = cms + ((isr & LPTIM_ISR_ARRM) ? 1ULL : 0ULL);
    ret *= 1000ULL;
    ret += cnt;
    return ret;
}

INTFLASH_FUNCTION void delay_ms(uint64_t nms)
{
    auto await_val = _cur_ms + nms + 1;
    while(_cur_ms < await_val)
    {
        __asm__ volatile("yield \n");
        //__WFI();
    }
}

void clock_get_now(struct timespec *tp)
{
    clock_get_timebase(tp);
    auto curt = clock_cur_us();

    auto cur_ns = (curt % 1000000ULL) * 1000ULL;
    auto cur_s = curt / 1000000ULL;

    tp->tv_nsec += cur_ns;
    while(tp->tv_nsec >= 1000000000)
    {
        tp->tv_sec++;
        tp->tv_nsec -= 1000000000;
    }
    tp->tv_sec += cur_s;
}

uint64_t clock_timespec_to_ms(const struct timespec &tp)
{
    struct timespec tzero;
    clock_get_timebase(&tzero);

    auto ns_diff = tp.tv_nsec - tzero.tv_nsec;
    auto s_diff = tp.tv_sec - tzero.tv_sec;

    while(ns_diff < 0)
    {
        s_diff--;
        ns_diff += 1000000000;
    }
    return static_cast<uint64_t>(ns_diff / 1000000) +
        static_cast<uint64_t>(s_diff * 1000);
}

INTFLASH_FUNCTION void enable_backup_sram()
{
    RCC->APB4ENR |= RCC_APB4ENR_RTCAPBEN;
    (void)RCC->APB4ENR;

    // Permit write access to backup domain - needed for write access to RTC, backup SRAM etc
    PWR->CR1 |= PWR_CR1_DBP;
    __DMB();

    // Enable backup regulator
    PWR->CSR1 |= PWR_CSR1_BREN;
    while(!(PWR->CSR1 & PWR_CSR1_BRRDY));

    // Enable RTC to use HSE/32 (for now, pending actual set up with LSE in v2)
    RCC->BDCR = RCC_BDCR_RTCEN |
        (3UL << RCC_BDCR_RTCSEL_Pos);


    RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN;
    (void)RCC->AHB4ENR;
}

extern "C" void clock_configure_backup_domain()
{
    /* There is rather complex nomeclature around the backup domain.  In terms of power supplies:
        VBAT is external power from battery
        VDD is external power from VDD pins
        
        Either VBAT or VDD (preferentially) supplies VSW
        
        VSW powers LSE, RTC and backup IOs
        
        VSW also produces VBKP through the backup regulator
        
        Either VBKP or VCORE (preferentially) supplies the backup SRAM at 0x38800000
        

        The PWR CR2 register, and RTC registers are preserved across power fail if VBAT good
    */

    RCC->APB4ENR |= RCC_APB4ENR_RTCAPBEN;
    (void)RCC->APB4ENR;

    // Enable RTC to use HSE/32 (for now, pending actual set up with LSE in v2)
    RCC->CFGR &= ~RCC_CFGR_RTCPRE_Msk;
    RCC->CFGR |= (32UL << RCC_CFGR_RTCPRE_Pos);

    // First check not already enabled (with valid VBAT should remain enabled)
    bool is_enabled = true;
    if(!(PWR->CSR1 & PWR_CSR1_BREN)) is_enabled = false;
    if(!(RCC->BDCR & RCC_BDCR_RTCEN)) is_enabled = false;
    if(!(RTC->ICSR & RTC_ICSR_INITS)) is_enabled = false;
    if(is_enabled)
    {
        // clear RSF flag
        PWR->CR1 |= PWR_CR1_DBP;
        __DMB();
        RTC->WPR = 0xca;
        RTC->WPR = 0x53;
        RTC->ICSR &= ~RTC_ICSR_RSF;
        RTC->WPR = 0xff;
        //PWR->CR1 &= ~PWR_CR1_DBP;

        RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN;
        (void)RCC->AHB4ENR;

        timespec cts;
        if(clock_get_timespec_from_rtc(&cts) == 0)
        {
            clock_set_timebase(&cts);
        }
        return;
    }

    // Permit write access to backup domain - needed for write access to RTC, backup SRAM etc
    PWR->CR1 |= PWR_CR1_DBP;
    __DMB();

    // Enable backup regulator
    PWR->CSR1 |= PWR_CSR1_BREN;
    while(!(PWR->CSR1 & PWR_CSR1_BRRDY));

    // Enable RTC to use HSE/32 (for now, pending actual set up with LSE in v2)
    RCC->BDCR = RCC_BDCR_RTCEN |
        (3UL << RCC_BDCR_RTCSEL_Pos);

    // Disallow write access to backup domain
    //PWR->CR1 &= ~PWR_CR1_DBP;

    RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN;
    (void)RCC->AHB4ENR;

    // Set up basic RTC time - changed later by network time or user

    // For now, default to the time of writing this file
    timespec ct { .tv_sec = 1720873685, .tv_nsec = 0 };
    clock_set_rtc_from_timespec(&ct);
}

static constexpr unsigned int to_bcd(unsigned int val)
{
    unsigned int out = 0;
    unsigned int out_shift = 0;
    while(val)
    {
        auto cval = val % 10;
        out |= (cval << out_shift);
        out_shift += 4;
        val /= 10;
    }
    return out;
}

static constexpr unsigned int from_bcd(unsigned int val)
{
    unsigned int out = 0;
    unsigned int out_mult = 1;
    while(val)
    {
        auto cval = val & 0xfU;
        out += (cval * out_mult);
        out_mult *= 10;
        val >>= 4;
    }
    return out;
}

int clock_get_timespec_from_rtc(timespec *ts)
{
    while(!(RTC->ICSR & RTC_ICSR_RSF));
    unsigned int tr = 0, dr = 0;
    while(true)
    {
        tr = RTC->TR;
        dr = RTC->DR;

        auto tr2 = RTC->TR;
        auto dr2 = RTC->DR;

        if(tr2 == tr && dr2 == dr) break;
    }

    tm ct;
    ct.tm_hour = from_bcd((tr & (RTC_TR_HT_Msk | RTC_TR_HU_Msk)) >> RTC_TR_HU_Pos);
    ct.tm_min = from_bcd((tr & (RTC_TR_MNT_Msk | RTC_TR_MNU_Msk)) >> RTC_TR_MNU_Pos);
    ct.tm_sec = from_bcd((tr & (RTC_TR_ST_Msk | RTC_TR_SU_Msk)) >> RTC_TR_SU_Pos);
    ct.tm_year = from_bcd((dr & (RTC_DR_YT_Msk | RTC_DR_YU_Msk)) >> RTC_DR_YU_Pos) + 100;
    ct.tm_mon = from_bcd((dr & (RTC_DR_MT_Msk | RTC_DR_MU_Msk)) >> RTC_DR_MU_Pos) - 1;
    ct.tm_wday = from_bcd((dr & (RTC_DR_WDU_Msk)) >> RTC_DR_WDU_Pos);
    if(ct.tm_wday == 7) ct.tm_wday = 0;
    ct.tm_mday = from_bcd((dr & (RTC_DR_DT_Msk | RTC_DR_DU_Msk)) >> RTC_DR_DU_Pos);
    ct.tm_isdst = (RTC->CR & RTC_CR_BKP) ? 1 : 0;

    static const unsigned int yday_upto[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    ct.tm_yday = yday_upto[ct.tm_mon] + ct.tm_mday - 1;
    if(ct.tm_mon > 1 &&
        (ct.tm_year % 4 == 0 && (ct.tm_year % 100 != 0 || ct.tm_year % 400 == 0)))
    {
        ct.tm_yday++;
    }

    // convert to timespec
    ts->tv_nsec = 0;
    ts->tv_sec = timegm(&ct);
    
    return 0;
}

void clock_get_timebase(struct timespec *tp)
{
    CriticalGuard cg;
    *tp = toffset;
}

void clock_set_timebase(const struct timespec *tp)
{
    CriticalGuard cg;
    if(tp)
        toffset = *tp;
}

int clock_set_rtc_from_timespec(const timespec *ts)
{
    // Permit write access to backup domain - needed for write access to RTC, backup SRAM etc
    PWR->CR1 |= PWR_CR1_DBP;
    __DMB();

    // Set up RTC divisors - change once using LSE

    // for 500 kHz HSE/32 we can use /125 asynchronous = 4000 Hz and /4000 sync = 1Hz
    // enable write access to RTC
    RTC->WPR = 0xca;
    RTC->WPR = 0x53;
    RTC->ICSR |= RTC_ICSR_INIT;
    while(!(RTC->ICSR & RTC_ICSR_INITF));
    unsigned int adiv = 125;
    unsigned int sdiv = 4000;
    RTC->PRER = (adiv - 1) << RTC_PRER_PREDIV_A_Pos;
    RTC->PRER = ((adiv - 1) << RTC_PRER_PREDIV_A_Pos) |
        ((sdiv - 1) << RTC_PRER_PREDIV_S_Pos);

    // convert timespec to BCD H:M:s etc
    auto lt = gmtime(&ts->tv_sec);
    auto lt_sec = to_bcd(lt->tm_sec);
    auto lt_min = to_bcd(lt->tm_min);
    auto lt_hour = to_bcd(lt->tm_hour);
    auto lt_year = to_bcd((lt->tm_year - 100) % 100);           // tm_year is years since 1900
    auto lt_month = to_bcd(lt->tm_mon + 1);                     // 0-11 -> 1-12
    auto lt_wday = to_bcd(lt->tm_wday == 0 ? 7 : lt->tm_wday);  // 0=Sunday,6=Sat -> 1=Mon,7=Sun
    auto lt_mday = to_bcd(lt->tm_mday);

    RTC->TR = lt_sec << RTC_TR_SU_Pos |
        lt_min << RTC_TR_MNU_Pos |
        lt_hour << RTC_TR_HU_Pos;
    RTC->DR = lt_mday << RTC_DR_DU_Pos |
        lt_month << RTC_DR_MU_Pos |
        lt_wday << RTC_DR_WDU_Pos |
        lt_year << RTC_DR_YU_Pos;

    if(lt->tm_isdst)
        RTC->CR |= RTC_CR_BKP;
    else
        RTC->CR &= ~RTC_CR_BKP;

    // exit init mode
    RTC->ICSR &= ~RTC_ICSR_INIT;

    // disable write access to RTC
    RTC->WPR = 0xff;

    // Disable write access to backup domain
    //PWR->CR1 &= ~PWR_CR1_DBP;

    return 0;
}
