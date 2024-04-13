#include "stm32h7xx.h"
#include "pins.h"
#include "supervisor.h"

constexpr const pin BTN_MENU { GPIOB, 4 };

void init_buttons()
{
    BTN_MENU.set_as_input();

    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;

    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[4] &~ SYSCFG_EXTICR2_EXTI4_Msk) | SYSCFG_EXTICR2_EXTI4_PB;

    EXTI->FTSR1 |= (1U << 4);
    EXTI->RTSR1 |= (1U << 4);
    EXTI->IMR1 |= (1U << 4);

    NVIC_EnableIRQ(EXTI4_IRQn);
}

extern "C" void EXTI4_IRQHandler()
{
    if(BTN_MENU.value())
    {
        p_supervisor.events.Push({ Event::KeyUp, .key = ' ' });
    }
    else
    {
        p_supervisor.events.Push({ Event::KeyDown, .key = ' ' });
    }
    EXTI->PR1 = EXTI_PR1_PR4;
    __DMB();
}
