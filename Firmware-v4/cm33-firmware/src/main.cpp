#include <stm32mp2xx.h>
#include "pins.h"
#include "debounce.h"
#include "_gk_proccreate.h"
#include "i2c.h"
#include "clocks.h"

const pin BTN_MCU_VOLUP { GPIOH, 2 };
const pin BTN_MCU_VOLDOWN { GPIOJ, 0 };

unsigned int keystate = 0;

unsigned int ioexp_keystate = 0;

class ioexp_pin
{
    public:
        unsigned int v;
        ioexp_pin(unsigned int _v) : v(_v) {}
        bool value() const
        {
            if(ioexp_keystate & v)
                return true;
            else
                return false;
        }
};

const ioexp_pin BTN_MCU_A(1U << 0);
const ioexp_pin BTN_MCU_B(1U << 1);
const ioexp_pin BTN_MCU_X(1U << 2);
const ioexp_pin BTN_MCU_Y(1U << 3);
const ioexp_pin BTN_MCU_U(1U << 4);
const ioexp_pin BTN_MCU_D(1U << 5);
const ioexp_pin BTN_MCU_L(1U << 6);
const ioexp_pin BTN_MCU_R(1U << 7);
const ioexp_pin BTN_MCU_LB(1U << 8);
const ioexp_pin BTN_MCU_RB(1U << 9);
const ioexp_pin BTN_MCU_LT(1U << 10);
const ioexp_pin BTN_MCU_RT(1U << 11);
const ioexp_pin BTN_MCU_JOY_A(1U << 12);
const ioexp_pin BTN_MCU_JOY_B(1U << 13);
const ioexp_pin BTN_MCU_MENU(1U << 14);

Debounce db_VOLUP(BTN_MCU_VOLUP, 1U << GK_KEYVOLUP);
Debounce db_VOLDOWN(BTN_MCU_VOLDOWN, 1U << GK_KEYVOLDOWN);
Debounce db_A(BTN_MCU_A, 1U << GK_KEYA);
Debounce db_B(BTN_MCU_B, 1U << GK_KEYB);
Debounce db_X(BTN_MCU_X, 1U << GK_KEYX);
Debounce db_Y(BTN_MCU_Y, 1U << GK_KEYY);
Debounce db_U(BTN_MCU_U, 1U << GK_KEYUP);
Debounce db_D(BTN_MCU_D, 1U << GK_KEYDOWN);
Debounce db_L(BTN_MCU_L, 1U << GK_KEYLEFT);
Debounce db_R(BTN_MCU_R, 1U << GK_KEYRIGHT);
Debounce db_LB(BTN_MCU_LB, 1U << GK_KEYLB);
Debounce db_RB(BTN_MCU_RB, 1U << GK_KEYRB);
Debounce db_LT(BTN_MCU_LT, 1U << GK_KEYLT);
Debounce db_RT(BTN_MCU_RT, 1U << GK_KEYRT);
Debounce db_JOYBTNA(BTN_MCU_JOY_A, 1U << GK_KEYJOY);
Debounce db_JOYBTNB(BTN_MCU_JOY_B, 1U << GK_KEYJOYB);
Debounce db_MENU(BTN_MCU_MENU, 1U << GK_KEYMENU);

int main()
{
    // TODO: add SLEEPONEXIT to SCB->SCR to ensure fully interrupt driven mode

    init_i2c();

    // Timer
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

template <class T> void db_tick(T &db)
{
    auto ret = db.tick();
    auto v = db.get_val();
    if(ret & pin_state::StableHigh)
    {
        keystate &= ~v;
    }
    else if(ret & pin_state::StableLow)
    {
        keystate |= v;
    }
} 

static void tick()
{
    uint8_t ioexp_vals[2];
    auto &i2c1 = i2c(1);
    if(i2c1.RegisterRead(0x20, (uint8_t)0, ioexp_vals, 2) == 2)
    {
        ioexp_keystate = (unsigned int)ioexp_vals[0] |
            (((unsigned int)ioexp_vals[1]) << 8);
    }

    db_tick(db_VOLUP);
    db_tick(db_VOLDOWN);
    db_tick(db_A);
    db_tick(db_B);
    db_tick(db_X);
    db_tick(db_Y);
    db_tick(db_U);
    db_tick(db_D);
    db_tick(db_L);
    db_tick(db_R);
    db_tick(db_LB);
    db_tick(db_RB);
    db_tick(db_LT);
    db_tick(db_RT);
    db_tick(db_JOYBTNA);
    db_tick(db_JOYBTNB);
    db_tick(db_MENU);    
}

extern "C" void TIM6_IRQHandler()
{
    clock_tick();
    tick();
    TIM6->SR = 0;
    __DMB();
}
