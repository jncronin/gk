#include "stm32h7xx.h"
#include "pins.h"
#include "supervisor.h"

constexpr const pin BTN_MENU { GPIOB, 4 };
constexpr const pin BTN_VOLUP { GPIOB, 11 };
constexpr const pin BTN_VOLDOWN { GPIOB, 12 };
constexpr const pin BTN_ONOFF { GPIOH, 3 };
constexpr const pin BTN_L { GPIOH, 5 };
constexpr const pin BTN_R { GPIOH, 6 };
constexpr const pin BTN_U { GPIOH, 7 };
constexpr const pin BTN_D { GPIOH, 8 };
constexpr const pin BTN_A { GPIOH, 9 };
constexpr const pin BTN_B { GPIOH, 10 };
constexpr const pin BTN_X { GPIOH, 13 };
constexpr const pin BTN_Y { GPIOH, 15 };

void init_buttons()
{
    BTN_MENU.set_as_input();
    BTN_VOLUP.set_as_input();
    BTN_VOLDOWN.set_as_input();
    BTN_ONOFF.set_as_input();
    BTN_L.set_as_input();
    BTN_R.set_as_input();
    BTN_U.set_as_input();
    BTN_D.set_as_input();
    BTN_A.set_as_input();
    BTN_B.set_as_input();
    BTN_X.set_as_input();
    BTN_Y.set_as_input();

    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;

    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[4] & ~SYSCFG_EXTICR2_EXTI4_Msk) | SYSCFG_EXTICR2_EXTI4_PB;
    SYSCFG->EXTICR[2] = (SYSCFG->EXTICR[2] & ~SYSCFG_EXTICR3_EXTI11_Msk) | SYSCFG_EXTICR3_EXTI11_PB;
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~SYSCFG_EXTICR4_EXTI12_Msk) | SYSCFG_EXTICR4_EXTI12_PB;
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~SYSCFG_EXTICR1_EXTI3_Msk) | SYSCFG_EXTICR1_EXTI3_PH;
    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[1] & ~SYSCFG_EXTICR2_EXTI5_Msk) | SYSCFG_EXTICR2_EXTI5_PH;
    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[1] & ~SYSCFG_EXTICR2_EXTI6_Msk) | SYSCFG_EXTICR2_EXTI6_PH;
    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[1] & ~SYSCFG_EXTICR2_EXTI7_Msk) | SYSCFG_EXTICR2_EXTI7_PH;
    SYSCFG->EXTICR[2] = (SYSCFG->EXTICR[2] & ~SYSCFG_EXTICR3_EXTI8_Msk) | SYSCFG_EXTICR3_EXTI8_PH;
    SYSCFG->EXTICR[2] = (SYSCFG->EXTICR[2] & ~SYSCFG_EXTICR3_EXTI9_Msk) | SYSCFG_EXTICR3_EXTI9_PH;
    SYSCFG->EXTICR[2] = (SYSCFG->EXTICR[2] & ~SYSCFG_EXTICR3_EXTI10_Msk) | SYSCFG_EXTICR3_EXTI10_PH;
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~SYSCFG_EXTICR4_EXTI13_Msk) | SYSCFG_EXTICR4_EXTI13_PH;
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~SYSCFG_EXTICR4_EXTI15_Msk) | SYSCFG_EXTICR4_EXTI15_PH;

    EXTI->FTSR1 |= (1U << 4) | (1U << 11) | (1U << 12) | (1U << 3) |
        (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8) |
        (1U << 9) | (1U << 10) | (1U << 13) | (1U << 15);
    EXTI->RTSR1 |= (1U << 4) | (1U << 11) | (1U << 12) | (1U << 3) |
        (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8) |
        (1U << 9) | (1U << 10) | (1U << 13) | (1U << 15);
    EXTI->IMR1 |= (1U << 4) | (1U << 11) | (1U << 12) | (1U << 3) |
        (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8) |
        (1U << 9) | (1U << 10) | (1U << 13) | (1U << 15);

    NVIC_EnableIRQ(EXTI4_IRQn);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    NVIC_EnableIRQ(EXTI3_IRQn);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

static inline Process &recv_proc()
{
    if(supervisor_is_active(nullptr, nullptr, nullptr, nullptr))
    {
        return p_supervisor;
    }
    else
    {
        return *focus_process;
    }
}

static inline void handle_state_change(int exti, bool pressed)
{
    // TODO: debounce
    Event::event_type_t et = pressed ? Event::KeyDown : Event::KeyUp;

    switch(exti)
    {
        case 3:
            p_supervisor.events.Push({ et, .key = 0 });
            break;
        case 4:
            p_supervisor.events.Push({ et, .key = ' ' });
            break;
        case 5:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Left, pressed);
            break;
        case 6:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Right, pressed);
            break;
        case 7:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Up, pressed);
            break;
        case 8:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Down, pressed);
            break;
        case 9:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::A, pressed);
            break;
        case 10:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::B, pressed);
            break;
        case 11:
            p_supervisor.events.Push({ et, .key = 'u' });
            break;
        case 12:
            p_supervisor.events.Push({ et, .key = 'd' });
            break;
        case 13:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::X, pressed);
            break;
        case 14:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Y, pressed);
            break;
    }
}

static inline void handle_state_change(const pin &p)
{
    handle_state_change(p.pin, !p.value());
}

extern "C" void EXTI4_IRQHandler()
{
    handle_state_change(BTN_MENU);
    EXTI->PR1 = EXTI_PR1_PR4;
    __DMB();
}

extern "C" void EXTI15_10_IRQHandler()
{
    uint32_t clear = 0;
    uint32_t pr1 = EXTI->PR1;
    if(pr1 & EXTI_PR1_PR10)
    {
        handle_state_change(BTN_B);
        clear |= EXTI_PR1_PR10;
    }
    if(pr1 & EXTI_PR1_PR11)
    {
        handle_state_change(BTN_VOLUP);
        clear |= EXTI_PR1_PR11;
    }
    if(pr1 & EXTI_PR1_PR12)
    {
        handle_state_change(BTN_VOLDOWN);
        clear |= EXTI_PR1_PR12;
    }
    if(pr1 & EXTI_PR1_PR13)
    {
        handle_state_change(BTN_X);
        clear |= EXTI_PR1_PR13;
    }
    if(pr1 & EXTI_PR1_PR14)
    {
        // CTP interrupt - TODO: send to ctp thread
        clear |= EXTI_PR1_PR14;
    }
    if(pr1 & EXTI_PR1_PR15)
    {
        handle_state_change(BTN_Y);
        clear |= EXTI_PR1_PR15;
    }
    EXTI->PR1 = clear;
    __DMB();
}

extern "C" void EXTI9_5_IRQHandler()
{
    uint32_t clear = 0;
    uint32_t pr1 = EXTI->PR1;
    if(pr1 & EXTI_PR1_PR5)
    {
        handle_state_change(BTN_L);
        clear |= EXTI_PR1_PR5;
    }
    if(pr1 & EXTI_PR1_PR6)
    {
        handle_state_change(BTN_R);
        clear |= EXTI_PR1_PR6;
    }
    if(pr1 & EXTI_PR1_PR7)
    {
        handle_state_change(BTN_U);
        clear |= EXTI_PR1_PR7;
    }
    if(pr1 & EXTI_PR1_PR8)
    {
        handle_state_change(BTN_D);
        clear |= EXTI_PR1_PR8;
    }
    if(pr1 & EXTI_PR1_PR9)
    {
        handle_state_change(BTN_A);
        clear |= EXTI_PR1_PR9;
    }
    EXTI->PR1 = clear;
    __DMB();    
}

extern "C" void EXTI3_IRQHandler()
{
    handle_state_change(BTN_ONOFF);
    EXTI->PR1 = EXTI_PR1_PR3;
    __DMB();
}
