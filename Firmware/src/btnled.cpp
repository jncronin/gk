#include "stm32h7rsxx.h"
#include "btnled.h"
#include "pins.h"
#include <cmath>

static const constexpr unsigned int arr = 4096;
double btnled_brightness = 0.5;

static constexpr pin btnled_pins[] =
{
    { GPIOE, 9, 1 },    // TIM1_CH1
    { GPIOC, 3, 1 },    // TIM1_CH2
    { GPIOE, 13, 1 }     // TIM1_CH3
};

void init_btnled()
{
    for(const auto &p : btnled_pins)
    {
        p.set_as_af();
    }

    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;

    /* Timer clocks on APB2 run between 200 and 300 MHz depending on CPU speed
        We want something in the 5-20 kHz range
        For ARR = 4096, we need a prescaler of 6 to give 8.1 kHz to 12.2 kHz
            Slowed down a bit due to some RC delay on ULN2003 */
    TIM1->CCMR1 = (0UL << TIM_CCMR1_CC1S_Pos) |
        TIM_CCMR1_OC1PE |
        (6UL << TIM_CCMR1_OC1M_Pos) |
        (0UL << TIM_CCMR1_CC2S_Pos) |
        TIM_CCMR1_OC2PE |
        (6UL << TIM_CCMR1_OC2M_Pos);
    TIM1->CCMR2 = (0UL << TIM_CCMR2_CC3S_Pos) |
        TIM_CCMR2_OC3PE |
        (6UL << TIM_CCMR2_OC3M_Pos);
    TIM1->CCER = TIM_CCER_CC1E |
        TIM_CCER_CC2E |
        TIM_CCER_CC3E;
    TIM1->PSC = 5UL;    // (ck = ck/(1 + PSC))
    TIM1->ARR = arr;
    TIM1->CCR1 = 0UL;
    TIM1->CCR2 = 0UL;
    TIM1->CCR3 = 0UL;
    TIM1->BDTR = TIM_BDTR_MOE;
    TIM1->CR1 = TIM_CR1_CEN;
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
    TIM1->CCR1 = pwm_non_linear((rgb >> 16) & 0xffUL);
    TIM1->CCR2 = pwm_non_linear((rgb >> 8) & 0xffUL);
    TIM1->CCR3 = pwm_non_linear(rgb & 0xffUL);
}
