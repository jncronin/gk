#include "stm32h7xx.h"
#include "btnled.h"
#include "pins.h"

static constexpr pin btnled_pins[] =
{
    { GPIOC, 6, 3 },    // TIM8_CH1
    { GPIOC, 7, 3 },    // TIM8_CH2
    { GPIOI, 7, 3 }     // TIM8_CH3
};

void init_btnled()
{
    for(const auto &p : btnled_pins)
    {
        p.set_as_af();
    }

    RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;
    (void)RCC->APB2ENR;

    /* Timer clocks on APB2 run between 48 and 192 MHz depending on M4 CPU speed
        We want something in the 5-20 kHz range
        For ARR = 4096, we need a prescaler of 2 to give 5.8 kHz to 23.4 kHz */
    TIM8->CCMR1 = (0UL << TIM_CCMR1_CC1S_Pos) |
        TIM_CCMR1_OC1PE |
        (6UL << TIM_CCMR1_OC1M_Pos) |
        (0UL << TIM_CCMR1_CC2S_Pos) |
        TIM_CCMR1_OC2PE |
        (6UL << TIM_CCMR1_OC2M_Pos);
    TIM8->CCMR2 = (0UL << TIM_CCMR2_CC3S_Pos) |
        TIM_CCMR2_OC3PE |
        (6UL << TIM_CCMR2_OC3M_Pos);
    TIM8->CCER = TIM_CCER_CC1E |
        TIM_CCER_CC2E |
        TIM_CCER_CC3E;
    TIM8->PSC = 1UL;    // (ck = ck/(1 + PSC))
    TIM8->ARR = 4096;
    TIM8->CCR1 = 0UL;
    TIM8->CCR2 = 0UL;
    TIM8->CCR3 = 0UL;
    TIM8->BDTR = TIM_BDTR_MOE;
    TIM8->CR1 = TIM_CR1_CEN;
}

void btnled_setcolor(uint32_t rgb)
{
    TIM8->CCR1 = ((rgb >> 16) & 0xffUL) * 16;
    TIM8->CCR2 = ((rgb >> 8) & 0xffUL) * 16;
    TIM8->CCR3 = (rgb & 0xffUL) * 16;
}
