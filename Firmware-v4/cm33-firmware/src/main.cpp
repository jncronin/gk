#include <stm32mp2xx.h>
#include "pins.h"
#include "debounce.h"
#include "_gk_proccreate.h"
#include "i2c.h"
#include "clocks.h"
#include "adc.h"

const pin BTN_MCU_VOLUP { GPIOH, 2 };
const pin BTN_MCU_VOLDOWN { GPIOJ, 0 };

unsigned int keystate = 0;
uint32_t adc_vals[4];
int16_t joy_a_x, joy_a_y, joy_b_x, joy_b_y;

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

class joy_pin
{
    public:
        bool positive;
        int16_t *v;

        joy_pin(int16_t *_v, bool pos) : positive(pos), v(_v) {}
        bool value() const
        {
            // pretend to be an active low output

            if(positive)
            {
                return *v < 8000;
            }
            else
            {
                return *v > -8000;
            }
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

const joy_pin JOY_A_LEFT(&joy_a_x, false);
const joy_pin JOY_A_RIGHT(&joy_a_x, true);
const joy_pin JOY_A_UP(&joy_a_y, true);
const joy_pin JOY_A_DOWN(&joy_a_y, false);
const joy_pin JOY_B_LEFT(&joy_b_x, false);
const joy_pin JOY_B_RIGHT(&joy_b_x, true);
const joy_pin JOY_B_UP(&joy_b_y, true);
const joy_pin JOY_B_DOWN(&joy_b_y, false);

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
Debounce db_JOY_A_LEFT(JOY_A_LEFT, 1U << GK_KEYJOYDIGILEFT);
Debounce db_JOY_A_RIGHT(JOY_A_RIGHT, 1U << GK_KEYJOYDIGIRIGHT);
Debounce db_JOY_A_UP(JOY_A_UP, 1U << GK_KEYJOYDIGIUP);
Debounce db_JOY_A_DOWN(JOY_A_DOWN, 1U << GK_KEYJOYDIGIDOWN);
Debounce db_JOY_B_LEFT(JOY_B_LEFT, 1U << GK_KEYJOYBDIGILEFT);
Debounce db_JOY_B_RIGHT(JOY_B_RIGHT, 1U << GK_KEYJOYBDIGIRIGHT);
Debounce db_JOY_B_UP(JOY_B_UP, 1U << GK_KEYJOYBDIGIUP);
Debounce db_JOY_B_DOWN(JOY_B_DOWN, 1U << GK_KEYJOYBDIGIDOWN);

int main()
{
    // TODO: add SLEEPONEXIT to SCB->SCR to ensure fully interrupt driven mode

    init_i2c();
    init_adc();

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

static int16_t joy_scale(uint32_t input, bool invert)
{
    int32_t i_input = (int32_t)input;
    i_input -= 8192;
    if(invert) i_input = -i_input;
    i_input *= 4;

    if(i_input < -32768) i_input = -32768;
    if(i_input > 32767) i_input = 32767;

    return (int16_t)i_input;
}

static void joystick_tick()
{
    /* map joystick axes from raw adc to something interpretable by SDL and
        others

        JOY A X : adc_vals[0] left = 0, right = 16k
        JOY A Y : adc_vals[1] up = 16k, down = 0
        JOY B X : adc_vals[3] left = 0, right = 16k (actually about 12k - problem with switch itself)
        JOY B Y : adc_vals[2] up = 0, down = 16k

        scale to:
            X: left = -32k, right = +32k
            Y: down = -32k, up = +32k

        Add dead zone in the middle of 8k (after scaling), then pass through a debouncer for digital inputs */
    
    joy_a_x = joy_scale(adc_vals[0], false);
    joy_a_y = joy_scale(adc_vals[1], false);
    joy_b_x = joy_scale(adc_vals[3], false);
    joy_b_y = joy_scale(adc_vals[2], true);    
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

    joystick_tick();

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
    db_tick(db_JOY_A_LEFT);
    db_tick(db_JOY_A_RIGHT);
    db_tick(db_JOY_A_UP);
    db_tick(db_JOY_A_DOWN);
    db_tick(db_JOY_B_LEFT);
    db_tick(db_JOY_B_RIGHT);
    db_tick(db_JOY_B_UP);
    db_tick(db_JOY_B_DOWN);
}

extern "C" void TIM6_IRQHandler()
{
    clock_tick();
    tick();
    TIM6->SR = 0;
    __DMB();
}
