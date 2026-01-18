#include <stm32mp2xx.h>
#include "pins.h"

const pin BTN_MCU_VOLUP { GPIOH, 3 };
const pin BTN_MCU_VOLDOWN { GPIOJ, 10 };

int main()
{
    // TODO: add SLEEPONEXIT to SCB->SCR to ensure fully interrupt driven mode

    RCC->TIM6CFGR |= RCC_TIM6CFGR_TIM6EN;
    RCC->TIM6CFGR &= ~RCC_TIM6CFGR_TIM6RST;
    (void)RCC->TIM6CFGR;

    // TIM6 clocks at 200 MHz, prescale to 5 MHz, then divide 25000 to get 200 Hz tick
    TIM6->CR1 = 0;
    TIM6->CR2 = 0;
    TIM6->SMCR = 0;
    TIM6->DIER = TIM_DIER_UIE;
    TIM6->CCMR1 = 0;
    TIM6->CCMR2 = 0;
    TIM6->CCMR3 = 0;
    TIM6->PSC = 40 - 1;
    TIM6->ARR = 24999;
    TIM6->CNT = 0;
    TIM6->CR1 = TIM_CR1_CEN;

    NVIC_EnableIRQ(TIM6_IRQn);
    __enable_irq();

    while(true)
    {
        __WFI();
    }
}

extern "C" void TIM6_IRQHandler()
{
    __SEV();
    TIM6->SR = 0;
    __DMB();
}
