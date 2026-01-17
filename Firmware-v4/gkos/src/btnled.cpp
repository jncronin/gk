#include "stm32mp2xx.h"
#include "btnled.h"
#include "pins.h"
#include <cmath>
#include "gk_conf.h"
#include "vmem.h"

#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define GPIOK_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOK_BASE))
#define GPIOJ_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOJ_BASE))
#define TIM1_VMEM ((TIM_TypeDef *)PMEM_TO_VMEM(TIM1_BASE))

static const constexpr unsigned int arr = 4096;
double btnled_brightness = 0.5;

static constexpr pin btnled_pins[] =
{
    { GPIOJ_VMEM, 6, 8 },    // TIM1_CH1
    { GPIOK_VMEM, 6, 8 },    // TIM1_CH2
    { GPIOK_VMEM, 3, 8 }     // TIM1_CH3
};

void init_btnled()
{
    RCC_VMEM->GPIOKCFGR |= RCC_GPIOKCFGR_GPIOxEN;
    RCC_VMEM->GPIOJCFGR |= RCC_GPIOJCFGR_GPIOxEN;
    RCC_VMEM->TIM1CFGR |= RCC_TIM1CFGR_TIM1EN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    for(const auto &p : btnled_pins)
    {
        p.set_as_af();
    }

    /* Timer clocks on APB2 run between 200 and 300 MHz depending on CPU speed
        We want something in the 5-20 kHz range
        For ARR = 4096, we need a prescaler of 6 to give 8.1 kHz to 12.2 kHz
            Slowed down a bit due to some RC delay on ULN2003 */
    TIM1_VMEM->CCMR1 = (0UL << TIM_CCMR1_CC1S_Pos) |
        TIM_CCMR1_OC1PE |
        (6UL << TIM_CCMR1_OC1M_Pos) |
        (0UL << TIM_CCMR1_CC2S_Pos) |
        TIM_CCMR1_OC2PE |
        (6UL << TIM_CCMR1_OC2M_Pos);
    TIM1_VMEM->CCMR2 = (0UL << TIM_CCMR2_CC3S_Pos) |
        TIM_CCMR2_OC3PE |
        (6UL << TIM_CCMR2_OC3M_Pos);
    TIM1_VMEM->CCER = TIM_CCER_CC1E |
        TIM_CCER_CC2E |
        TIM_CCER_CC3E;
    TIM1_VMEM->PSC = 5UL;    // (ck = ck/(1 + PSC))
    TIM1_VMEM->ARR = arr;
    TIM1_VMEM->CCR1 = 0UL;
    TIM1_VMEM->CCR2 = 0UL;
    TIM1_VMEM->CCR3 = 0UL;
    TIM1_VMEM->BDTR = TIM_BDTR_MOE;
    TIM1_VMEM->CR1 = TIM_CR1_CEN;
}

static inline unsigned int pwm_non_linear(unsigned int input)
{
    auto iflt = static_cast<double>(input);
    auto oflt = std::pow(arr, iflt / 255.0 * btnled_brightness);
    auto oi = static_cast<int>(std::rint(oflt));
    if(oi < 0) oi = 0;
    if(oi > static_cast<int>(arr)) oi = arr;
    return static_cast<unsigned int>(oi);    
}

void btnled_setcolor(uint32_t rgb)
{
    TIM1_VMEM->CCR1 = pwm_non_linear((rgb >> 16) & 0xffUL);
    TIM1_VMEM->CCR2 = pwm_non_linear((rgb >> 8) & 0xffUL);
    TIM1_VMEM->CCR3 = pwm_non_linear(rgb & 0xffUL);
}

static const constexpr pin BTNLED_R { GPIOJ_VMEM, 6 };
static const constexpr pin BTNLED_G { GPIOK_VMEM, 6 };
static const constexpr pin BTNLED_B { GPIOK_VMEM, 3 };

void btnled_setcolor_init(uint32_t rgb)
{
    RCC_VMEM->GPIOKCFGR |= RCC_GPIOKCFGR_GPIOxEN;
    RCC_VMEM->GPIOJCFGR |= RCC_GPIOJCFGR_GPIOxEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    BTNLED_R.set_as_output();
    BTNLED_G.set_as_output();
    BTNLED_B.set_as_output();

    if((rgb >> 16) & 0xffUL)
        BTNLED_R.set();
    else
        BTNLED_R.clear();
    if((rgb >> 8) & 0xffUL)
        BTNLED_G.set();
    else
        BTNLED_G.clear();
    if(rgb & 0xffUL)
        BTNLED_B.set();
    else
        BTNLED_B.clear();
}
