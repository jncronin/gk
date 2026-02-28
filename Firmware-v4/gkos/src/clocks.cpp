#include <stm32mp2xx.h>
#include "clocks.h"
#include "vmem.h"
#include "pmic.h"
#include "osmutex.h"
#include "gk_conf.h"

#define TIM3_VMEM ((TIM_TypeDef *)PMEM_TO_VMEM(TIM3_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))
#define RTC_VMEM ((RTC_TypeDef *)PMEM_TO_VMEM(RTC_BASE))

volatile uint64_t * _cur_s;
volatile uint64_t * _tim_precision_ns;

static Spinlock sl_timebase;
static timespec timebase;

static void init_rtc();
time_t timegm (struct tm* tim_p);

void init_clocks(const gkos_boot_interface *gbi)
{
    _cur_s = (volatile uint64_t *)(PMEM_TO_VMEM_DEVICE((uint64_t)gbi->cur_s));
    _tim_precision_ns = (volatile uint64_t *)(PMEM_TO_VMEM_DEVICE((uint64_t)gbi->tim_ns_precision));

    init_rtc();
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

unsigned int clock_set_cpu_and_vddcpu(unsigned int freq)
{
#if GK_OVERCLOCK_MHZ
    const unsigned int max_freq = GK_OVERCLOCK_MHZ * 1000000U;
#else
    uint32_t part_no = *(uint32_t *)(0xfffffd0044000024);
    unsigned int max_freq = (part_no & 0x80000000U) ? 1500000000U : 1200000000U;
#endif
    const unsigned int min_freq = 400000000U;

    if(freq > max_freq)
    {
        freq = max_freq;
    }
    if(freq < min_freq)
    {
        freq = min_freq;
    }

    if(freq > 1200000000U)
    {
        // set vddcpu to 0.91V
        pmic_set_power(PMIC_Power_Target::CPU, 910U);
        udelay(5000);
    }

    freq = clock_set_cpu(freq);

    if(freq <= 1200000000U)
    {
        // set vddcpu to 0.8V
        pmic_set_power(PMIC_Power_Target::CPU, 800U);
    }

    return freq;
}

void init_rtc()
{
    // check LSE is available
    if(!(RCC_VMEM->BDCR & RCC_BDCR_LSERDY))
    {
        klog("clock: lse not available - will not have valid RTC\n");
        return;
    }

    // Use LSE to clock RTC
    if(!(RCC_VMEM->BDCR & RCC_BDCR_LSECSSON) ||
        ((RCC_VMEM->BDCR & RCC_BDCR_RTCSRC_Msk) != (1U << RCC_BDCR_RTCSRC_Pos)) ||
        !(RCC_VMEM->BDCR & RCC_BDCR_RTCCKEN))
    {
        PWR_VMEM->BDCR1 |= PWR_BDCR1_DBD3P;
        __asm__ volatile("dsb sy\n" ::: "memory");

        if((RCC_VMEM->BDCR & RCC_BDCR_RTCSRC_Msk) != (1U << RCC_BDCR_RTCSRC_Pos))
        {
            // RTCSRC can only be set by backup domain reset - do this
            RCC_VMEM->BDCR |= RCC_BDCR_VSWRST;
            __asm__ volatile("dsb sy\n" ::: "memory");
            udelay(5000);
            RCC_VMEM->BDCR &= ~RCC_BDCR_VSWRST;
            __asm__ volatile("dsb sy\n" ::: "memory");
        }

        if(!(RCC_VMEM->BDCR & RCC_BDCR_LSEON))
        {
            // we may have just reset the backup domain - enable LSE again
            RCC_VMEM->BDCR &= ~RCC_BDCR_LSEON;
            __asm__ volatile("dsb sy\n" ::: "memory");
            while(RCC_VMEM->BDCR & RCC_BDCR_LSERDY);
            RCC_VMEM->BDCR &= ~RCC_BDCR_LSEBYP;
            __asm__ volatile("dsb sy\n" ::: "memory");
            RCC_VMEM->BDCR |= RCC_BDCR_LSEGFON;
            __asm__ volatile("dsb sy\n" ::: "memory");
            RCC_VMEM->BDCR |= RCC_BDCR_LSEON;
            bool lse_found = false;
            for(uint retries = 0; retries < 100; retries++)
            {
                if(RCC_VMEM->BDCR & RCC_BDCR_LSERDY)
                {
                    lse_found = true;
                    break;
                }
                udelay(5000);
            }
            if(!lse_found)
            {
                klog("clocks: lse not ready\n");
                return;
            }
        }

        RCC_VMEM->BDCR |= 1U << RCC_BDCR_RTCSRC_Pos;
        __asm__ volatile("dsb sy\n" ::: "memory");

        //RCC_VMEM->BDCR |= RCC_BDCR_LSECSSON;
        //__asm__ volatile("dsb sy\n" ::: "memory");

        RCC_VMEM->BDCR |= RCC_BDCR_RTCCKEN;
        __asm__ volatile("dsb sy\n" ::: "memory");

        PWR_VMEM->BDCR1 &= ~PWR_BDCR1_DBD3P;
        __asm__ volatile("dsb sy\n" ::: "memory");
    }

    RCC_VMEM->RTCCFGR = RCC_RTCCFGR_RTCAMEN |
        RCC_RTCCFGR_RTCLPEN |
        RCC_RTCCFGR_RTCEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    if(!(RTC_VMEM->ICSR & RTC_ICSR_INITS))
    {
        // For now, default to the time of writing this file
        timespec ct { .tv_sec = 1770067143, .tv_nsec = 0 };
        clock_set_rtc_from_timespec(&ct);
        clock_set_timebase(&ct);
    }
    else
    {
        // it has a valid time - use it
        timespec cts;
        if(clock_get_timespec_from_rtc(&cts) == 0)
        {
            clock_set_timebase(&cts);
        }
    }
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
    //while(!(RTC_VMEM->ICSR & RTC_ICSR_RSF));
    unsigned int tr = 0, dr = 0;
    while(true)
    {
        tr = RTC_VMEM->TR;
        dr = RTC_VMEM->DR;

        auto tr2 = RTC_VMEM->TR;
        auto dr2 = RTC_VMEM->DR;

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
    ct.tm_isdst = (RTC_VMEM->CR & RTC_CR_BKP) ? 1 : 0;

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

void clock_get_realtime(struct timespec *tp)
{
    CriticalGuard cg(sl_timebase);
    if(tp)
    {
        *tp = timebase + clock_cur();
    }
}

void clock_get_timebase(struct timespec *tp)
{
    CriticalGuard cg(sl_timebase);
    *tp = timebase;
}

void clock_set_timebase(const struct timespec *tp)
{
    CriticalGuard cg(sl_timebase);
    if(tp)
    {
        timebase = *tp - clock_cur();
        
        // hardwired for now because this is what the userspace map expects
        *(timespec *)(((uintptr_t)_cur_s) + 0x10ULL) = timebase;
    }
}

int clock_set_rtc_from_timespec(const timespec *ts)
{
    // Permit write access to backup domain - needed for write access to RTC, backup SRAM etc
    PWR_VMEM->BDCR1 |= PWR_BDCR1_DBD3P;
    __asm__ volatile("dsb sy\n" ::: "memory");

    // Set up RTC divisors - change once using LSE

    // for 500 kHz HSE/32 we can use /125 asynchronous = 4000 Hz and /4000 sync = 1Hz
    // enable write access to RTC
    RTC_VMEM->WPR = 0xca;
    RTC_VMEM->WPR = 0x53;
    RTC_VMEM->ICSR |= RTC_ICSR_INIT;
    while(!(RTC_VMEM->ICSR & RTC_ICSR_INITF));
    unsigned int adiv = 128;
    unsigned int sdiv = 256;
    RTC_VMEM->PRER = (adiv - 1) << RTC_PRER_PREDIV_A_Pos;
    RTC_VMEM->PRER = ((adiv - 1) << RTC_PRER_PREDIV_A_Pos) |
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

    RTC_VMEM->TR = lt_sec << RTC_TR_SU_Pos |
        lt_min << RTC_TR_MNU_Pos |
        lt_hour << RTC_TR_HU_Pos;
    RTC_VMEM->DR = lt_mday << RTC_DR_DU_Pos |
        lt_month << RTC_DR_MU_Pos |
        lt_wday << RTC_DR_WDU_Pos |
        lt_year << RTC_DR_YU_Pos;

    if(lt->tm_isdst)
        RTC_VMEM->CR |= RTC_CR_BKP;
    else
        RTC_VMEM->CR &= ~RTC_CR_BKP;

    // exit init mode
    RTC_VMEM->ICSR &= ~RTC_ICSR_INIT;

    // disable write access to RTC
    RTC_VMEM->WPR = 0xff;

    // Disable write access to backup domain
    PWR_VMEM->BDCR1 &= ~PWR_BDCR1_DBD3P;
    __asm__ volatile("dsb sy\n" ::: "memory");

    return 0;
}
