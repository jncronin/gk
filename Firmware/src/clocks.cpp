#include <stm32h7xx.h>

#include "clocks.h"
#include <ctime>
#include "osmutex.h"
#include "gk_conf.h"

#include "scheduler.h"
#include "ipi.h"

__attribute__((section(".sram4"))) uint32_t SystemCoreClock;
__attribute__((section(".sram4"))) Spinlock sl_toffset;

extern uint64_t _cur_ms;
extern struct timespec toffset;

static void enable_backup_domain();

void init_clocks()
{
    /* Enable clock to the SYSCFG registers */
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;

    /* Set up the clock system
        We have a 16 MHz HSE crystal oscillator, and 16 MHz HSI
    */

    // Enable HSE
    auto cr = RCC->CR;
    cr |= RCC_CR_HSEON;
    cr &= ~RCC_CR_HSEBYP;
    RCC->CR = cr;
    while(!(RCC->CR & RCC_CR_HSERDY));

    enable_backup_domain();


    _cur_ms = 0ULL;



    /* Configure PLLs - TODO: these have been changed - all "M"s are /2 (PLL1 + 3 has been fixed)
        PLL1 = HSE16 / M2 * N96 = 768                   (overclock *120 = 960)
            /P2 = 384 MHz -> SYSCLK                     (overclock = 480)
            /Q4 = 192 MHz -> SDMMC1, SPI1, FMC, RNG     (overclock = 240)
            /R80 = 9.6 MHz -> unused (only able to use for TRACECLK)
            
        PLL2 = HSE16 / M8 * N172 . frac263
            /P7 = 49.152 MHz -> SAI1
            /Q2 = 172 MHz -> unused
            /R8 = 43 MHz -> unused
            
        PLL3 = HSE16 / M2 * N96 = 768
            /P7 = 109 MHz -> unused
            /Q16 = 48 MHz -> USB and SPI5
            /R32 = 24 MHz -> LTDC (60 Hz refresh) and I2C1,2,3

        Peripheral clock PER_CLK is HSI16
            => LPTIM1, LPTIM2, LPTIM345
    */
    
    // Ensure sysclk is HSI64/4 = HSI16 and PLLs disabled
    RCC->CFGR &= ~RCC_CFGR_SW_Msk;
    while(RCC->CFGR & RCC_CFGR_SWS);
    RCC->CR &= ~(RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON);
    while(RCC->CR & (RCC_CR_PLL1RDY | RCC_CR_PLL2RDY | RCC_CR_PLL3RDY));
    RCC->CR &= ~RCC_CR_HSIDIV;
    RCC->CR |= 2UL << RCC_CR_HSIDIV_Pos;

    // Set voltage scaling 1
    PWR->D3CR = (3UL << PWR_D3CR_VOS_Pos);
    while(!(PWR->D3CR & PWR_D3CR_VOSRDY));

    // Set up PLLs as above
    RCC->PLLCKSELR = (2UL << RCC_PLLCKSELR_DIVM3_Pos) |
        (2UL << RCC_PLLCKSELR_DIVM2_Pos) |
        (2UL << RCC_PLLCKSELR_DIVM1_Pos) |
        (0UL << RCC_PLLCKSELR_PLLSRC);

    RCC->PLLCFGR = 0UL;
    RCC->PLL1FRACR = 0UL;
    RCC->PLL2FRACR = 65UL;
    RCC->PLLCFGR = RCC_PLLCFGR_DIVR3EN |
        RCC_PLLCFGR_DIVQ3EN |
        RCC_PLLCFGR_DIVP2EN |
        RCC_PLLCFGR_DIVQ1EN |
        RCC_PLLCFGR_DIVP1EN |
        (2UL << RCC_PLLCFGR_PLL3RGE_Pos) |
        (2UL << RCC_PLLCFGR_PLL2RGE_Pos) |
        (2UL << RCC_PLLCFGR_PLL1RGE_Pos) |
        RCC_PLLCFGR_PLL1FRACEN |
        RCC_PLLCFGR_PLL2FRACEN;
    
    RCC->PLL1DIVR = (1UL << RCC_PLL1DIVR_R1_Pos) |
        (3UL << RCC_PLL1DIVR_Q1_Pos) |
        (1UL << RCC_PLL1DIVR_P1_Pos) |
        (
#if GK_OVERCLOCK
            119UL
#else
            95UL
#endif
            << RCC_PLL1DIVR_N1_Pos);
    RCC->PLL2DIVR = (1UL << RCC_PLL2DIVR_R2_Pos) |
        (0UL << RCC_PLL2DIVR_Q2_Pos) |
        (6UL << RCC_PLL2DIVR_P2_Pos) |
        (42UL << RCC_PLL2DIVR_N2_Pos);
    RCC->PLL3DIVR = (31UL << RCC_PLL3DIVR_R3_Pos) |
        (15UL << RCC_PLL3DIVR_Q3_Pos) |
        (0UL << RCC_PLL3DIVR_P3_Pos) |
        (95UL << RCC_PLL3DIVR_N3_Pos);
    
    RCC->CR |= (RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON);
    while(!(RCC->CR & RCC_CR_PLL1RDY));
    while(!(RCC->CR & RCC_CR_PLL2RDY));
    while(!(RCC->CR & RCC_CR_PLL3RDY));

    // Set up D2/D3 dividers to /2 (D1 divisors set up in clock_set_cpu())
    RCC->D2CFGR = (4UL << RCC_D2CFGR_D2PPRE2_Pos) |
        (4UL << RCC_D2CFGR_D2PPRE1_Pos);
    RCC->D3CFGR = (4UL << RCC_D3CFGR_D3PPRE_Pos);

    // Set cpus to run at slow speed once with plls as sysclk
    clock_set_cpu(clock_cpu_speed::cpu_48_48);

    // Set up peripherals to use the above mappings
    RCC->D1CCIPR = (0UL << RCC_D1CCIPR_CKPERSEL_Pos) |
        (0UL << RCC_D1CCIPR_SDMMCSEL_Pos) |
        (1UL << RCC_D1CCIPR_FMCSEL_Pos);
    RCC->D2CCIP1R = (2UL << RCC_D2CCIP1R_SPI45SEL_Pos) |
        (0UL << RCC_D2CCIP1R_SPI123SEL_Pos) |
        (1UL << RCC_D2CCIP1R_SAI1SEL_Pos);
    RCC->D2CCIP2R = (5UL << RCC_D2CCIP2R_LPTIM1SEL_Pos) |
        (2UL << RCC_D2CCIP2R_USBSEL_Pos) |
        (1UL << RCC_D2CCIP2R_I2C123SEL_Pos) |
        (1UL << RCC_D2CCIP2R_RNGSEL_Pos);
    RCC->D3CCIPR = (5UL << RCC_D3CCIPR_LPTIM2SEL_Pos) |
        (5UL << RCC_D3CCIPR_LPTIM345SEL_Pos);

    // Set up LPTIM1 as a 1 kHz tick
    RCC->APB1LENR |= RCC_APB1LENR_LPTIM1EN;
    (void)RCC->APB1LENR;

    LPTIM1->CR = 0;
    LPTIM1->CR = LPTIM_CR_RSTARE;
    (void)LPTIM1->CR;
    LPTIM1->CR = 0;

    LPTIM1->CFGR = 4UL << LPTIM_CFGR_PRESC_Pos;     // /16 => 1 MHz tick
    LPTIM1->IER = LPTIM_IER_ARRMIE;
    LPTIM1->CR = LPTIM_CR_ENABLE;
    LPTIM1->ARR = 999;                              // Reload every 1 kHz
    
    NVIC_EnableIRQ(LPTIM1_IRQn);
    __enable_irq();
    LPTIM1->CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;
}

SRAM4_DATA Spinlock sl_timer;

extern "C" void LPTIM1_IRQHandler()
{
    CriticalGuard cg(sl_timer);
    // do it this way round or the IRQ is still active on IRQ return
    LPTIM1->ICR = LPTIM_ICR_ARRMCF;
    _cur_ms++;

    // Handler works on core 0 - awake tickless blocking threads if necessary
#if GK_TICKLESS
#if GK_DUAL_CORE_AMP
    if(scheds[0].block_until && _cur_ms >= scheds[0].block_until)
    {
        Yield();
    }
    if(scheds[1].block_until && _cur_ms >= scheds[1].block_until)
    {
        ipi_messages[1].Write({ .type = ipi_message::ipi_message_type_t::Yield });
    }
#else
    if(sched.block_until && _cur_ms >= sched.block_until)
    {
        Yield();
#if GK_DUAL_CORE
        ipi_messages[1].Write({ .type = ipi_message::ipi_message_type_t::Yield });
#endif
    }
#endif
#endif
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

void delay_ms(uint64_t nms)
{
    auto await_val = _cur_ms + nms + 1;
    while(_cur_ms < await_val)
    {
        if(GetCoreID() == 0)
            __WFI();            // timer ticks only come to core 0, else busy wait
    }
}

bool clock_set_cpu(clock_cpu_speed speed)
{
    // Set sysclk to use HSI16
    RCC->CFGR &= ~RCC_CFGR_SW_Msk;
    while(RCC->CFGR & RCC_CFGR_SWS);

    // Set up flash latency (p.166) and new D1CPRE and HPRE
    RCC->AHB3ENR |= RCC_AHB3ENR_FLASHEN;
    (void)RCC->AHB3ENR;

    switch(speed)
    {
        case clock_cpu_speed::cpu_384_192:
            FLASH->ACR = (2UL << FLASH_ACR_WRHIGHFREQ_Pos) |
                (2UL << FLASH_ACR_LATENCY_Pos);
            RCC->D1CFGR = (0UL << RCC_D1CFGR_D1CPRE_Pos) |
                (4UL << RCC_D1CFGR_D1PPRE_Pos) |
                (8UL << RCC_D1CFGR_HPRE_Pos);
#if GK_OVERCLOCK
            SystemCoreClock = 480000000;
#else
            SystemCoreClock = 384000000;
#endif
            break;
            
        case clock_cpu_speed::cpu_192_192:
            FLASH->ACR = (2UL << FLASH_ACR_WRHIGHFREQ_Pos) |
                (2UL << FLASH_ACR_LATENCY_Pos);
            RCC->D1CFGR = (8UL << RCC_D1CFGR_D1CPRE_Pos) |
                (4UL << RCC_D1CFGR_D1PPRE_Pos) |
                (0UL << RCC_D1CFGR_HPRE_Pos);
            SystemCoreClock = 192000000;
            break;

        case clock_cpu_speed::cpu_96_96:
            FLASH->ACR = (1UL << FLASH_ACR_WRHIGHFREQ_Pos) |
                (1UL << FLASH_ACR_LATENCY_Pos);
            RCC->D1CFGR = (9UL << RCC_D1CFGR_D1CPRE_Pos) |
                (4UL << RCC_D1CFGR_D1PPRE_Pos) |
                (0UL << RCC_D1CFGR_HPRE_Pos);
            SystemCoreClock = 96000000;
            break;

        case clock_cpu_speed::cpu_48_48:
            FLASH->ACR = (0UL << FLASH_ACR_WRHIGHFREQ_Pos) |
                (0UL << FLASH_ACR_LATENCY_Pos);
            RCC->D1CFGR = (10UL << RCC_D1CFGR_D1CPRE_Pos) |
                (4UL << RCC_D1CFGR_D1PPRE_Pos) |
                (0UL << RCC_D1CFGR_HPRE_Pos);
            SystemCoreClock = 48000000;
            break;
    }

    // Set sysclk to use PLL
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) |
        (3UL << RCC_CFGR_SW_Pos);
    while((RCC->CFGR & RCC_CFGR_SW_Msk) != 3UL);

    return true;
}

void clock_get_timebase(struct timespec *tp)
{
    CriticalGuard cg(sl_toffset);
    *tp = toffset;
}

void clock_set_timebase(const struct timespec *tp)
{
    CriticalGuard cg(sl_toffset);
    if(tp)
        toffset = *tp;
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

void enable_backup_domain()
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

    // Enable RTC to use HSE/32 (for now, pending actual set up with LSE in v2)
    RCC->CFGR &= ~RCC_CFGR_RTCPRE_Msk;
    RCC->CFGR |= (32UL << RCC_CFGR_RTCPRE_Pos);

    // First check not already enabled (with valid VBAT should remain enabled)
    bool is_enabled = true;
    if(!(PWR->CR2 & PWR_CR2_BREN)) is_enabled = false;
    if(!(RCC->BDCR & RCC_BDCR_RTCEN)) is_enabled = false;
    if(!(RTC->ISR & RTC_ISR_INITS)) is_enabled = false;
    if(is_enabled)
    {
        // clear RSF flag
        PWR->CR1 |= PWR_CR1_DBP;
        __DMB();
        RTC->WPR = 0xca;
        RTC->WPR = 0x53;
        RTC->ISR &= ~RTC_ISR_RSF;
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
    PWR->CR2 |= PWR_CR2_BREN;
    while(!(PWR->CR2 & PWR_CR2_BRRDY));

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
    while(!(RTC->ISR & RTC_ISR_RSF));
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
    ts->tv_sec = mktime(&ct);

    return 0;
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
    RTC->ISR |= RTC_ISR_INIT;
    while(!(RTC->ISR & RTC_ISR_INITF));
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
    RTC->ISR &= ~RTC_ISR_INIT;

    // disable write access to RTC
    RTC->WPR = 0xff;

    // Disable write access to backup domain
    //PWR->CR1 &= ~PWR_CR1_DBP;

    return 0;
}
