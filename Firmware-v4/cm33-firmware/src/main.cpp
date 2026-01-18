#include <stm32mp2xx.h>
#include "pins.h"
#include "debounce.h"
#include "_gk_proccreate.h"
#include "i2c.h"
#include "clocks.h"
#include "adc.h"
#include "lsm.h"

#include "interface/cm33_data.h"

__attribute__((section(".sram1"))) cm33_data_userspace d;
__attribute__((section(".sram2_header"))) cm33_data_kernel dk;

const pin BTN_MCU_VOLUP { GPIOH, 2 };
const pin BTN_MCU_VOLDOWN { GPIOJ, 0 };

const uint32_t rb_size = 256;
__attribute__((section(".sram2"))) volatile uint32_t rb[rb_size];

uint32_t adc_vals[4];
int lsm_ret = 0;
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

const joy_pin JOY_A_LEFT((int16_t *)&d.joy_a.x, false);
const joy_pin JOY_A_RIGHT((int16_t *)&d.joy_a.x, true);
const joy_pin JOY_A_UP((int16_t *)&d.joy_a.y, true);
const joy_pin JOY_A_DOWN((int16_t *)&d.joy_a.y, false);
const joy_pin JOY_B_LEFT((int16_t *)&d.joy_b.x, false);
const joy_pin JOY_B_RIGHT((int16_t *)&d.joy_b.x, true);
const joy_pin JOY_B_UP((int16_t *)&d.joy_b.y, true);
const joy_pin JOY_B_DOWN((int16_t *)&d.joy_b.y, false);
const joy_pin JOY_TILT_LEFT((int16_t *)&d.joy_tilt.x, false);
const joy_pin JOY_TILT_RIGHT((int16_t *)&d.joy_tilt.x, true);
const joy_pin JOY_TILT_UP((int16_t *)&d.joy_tilt.y, true);
const joy_pin JOY_TILT_DOWN((int16_t *)&d.joy_tilt.y, false);

Debounce db_VOLUP(BTN_MCU_VOLUP, GK_KEYVOLUP);
Debounce db_VOLDOWN(BTN_MCU_VOLDOWN, GK_KEYVOLDOWN);
Debounce db_A(BTN_MCU_A, GK_KEYA);
Debounce db_B(BTN_MCU_B, GK_KEYB);
Debounce db_X(BTN_MCU_X, GK_KEYX);
Debounce db_Y(BTN_MCU_Y, GK_KEYY);
Debounce db_U(BTN_MCU_U, GK_KEYUP);
Debounce db_D(BTN_MCU_D, GK_KEYDOWN);
Debounce db_L(BTN_MCU_L, GK_KEYLEFT);
Debounce db_R(BTN_MCU_R, GK_KEYRIGHT);
Debounce db_LB(BTN_MCU_LB, GK_KEYLB);
Debounce db_RB(BTN_MCU_RB, GK_KEYRB);
Debounce db_LT(BTN_MCU_LT, GK_KEYLT);
Debounce db_RT(BTN_MCU_RT, GK_KEYRT);
Debounce db_JOYBTNA(BTN_MCU_JOY_A, GK_KEYJOY);
Debounce db_JOYBTNB(BTN_MCU_JOY_B, GK_KEYJOYB);
Debounce db_MENU(BTN_MCU_MENU, GK_KEYMENU);
Debounce db_JOY_A_LEFT(JOY_A_LEFT, GK_KEYJOYDIGILEFT);
Debounce db_JOY_A_RIGHT(JOY_A_RIGHT, GK_KEYJOYDIGIRIGHT);
Debounce db_JOY_A_UP(JOY_A_UP, GK_KEYJOYDIGIUP);
Debounce db_JOY_A_DOWN(JOY_A_DOWN, GK_KEYJOYDIGIDOWN);
Debounce db_JOY_B_LEFT(JOY_B_LEFT, GK_KEYJOYBDIGILEFT);
Debounce db_JOY_B_RIGHT(JOY_B_RIGHT, GK_KEYJOYBDIGIRIGHT);
Debounce db_JOY_B_UP(JOY_B_UP, GK_KEYJOYBDIGIUP);
Debounce db_JOY_B_DOWN(JOY_B_DOWN, GK_KEYJOYBDIGIDOWN);
Debounce db_JOY_TILT_LEFT(JOY_TILT_LEFT, GK_KEYTILTLEFT);
Debounce db_JOY_TILT_RIGHT(JOY_TILT_RIGHT, GK_KEYTILTRIGHT);
Debounce db_JOY_TILT_UP(JOY_TILT_UP, GK_KEYTILTUP);
Debounce db_JOY_TILT_DOWN(JOY_TILT_DOWN, GK_KEYTILTDOWN);

int main()
{
    // load defaults
    dk.cr = 0;
    dk.sr = 0;
    dk.joy_a_calib.left = -32767;
    dk.joy_a_calib.right = 32767;
    dk.joy_a_calib.top = 32767;
    dk.joy_a_calib.bottom = -32767;
    dk.joy_a_calib.middle_x = 0;
    dk.joy_a_calib.middle_y = 0;
    dk.joy_b_calib.left = -32767;
    dk.joy_b_calib.right = 32767;
    dk.joy_b_calib.top = 32767;
    dk.joy_b_calib.bottom = -32767;
    dk.joy_b_calib.middle_x = 0;
    dk.joy_b_calib.middle_y = 0;
    dk.tilt_zero = 0.0f;
    dk.rb_r_ptr = 0;
    dk.rb_w_ptr = 0;
    dk.rb_size = rb_size;
    dk.rb_paddr = (uint32_t)(uintptr_t)rb;
    dk.joy_a_raw.x = 0;
    dk.joy_a_raw.y = 0;
    dk.joy_b_raw.x = 0;
    dk.joy_b_raw.y = 0;

    // TODO: add SLEEPONEXIT to SCB->SCR to ensure fully interrupt driven mode

    init_i2c();
    init_adc();
    init_lsm();

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

    // report we are awake
    dk.sr = dk.sr | CM33_DK_SR_OUTPUT_ENABLE | CM33_DK_SR_READY;
    __SEV();

    NVIC_EnableIRQ(TIM6_IRQn);
    __enable_irq();

    while(true)
    {
        __WFI();
    }
}

static void send_message(uint32_t msg)
{
    auto new_w_ptr = dk.rb_w_ptr + 1;
    if(new_w_ptr >= dk.rb_size)
        new_w_ptr = 0;
    if(new_w_ptr == dk.rb_r_ptr)
    {
        // out of space
        dk.sr = dk.sr | CM33_DK_SR_OVERFLOW;
        return;
    }
    rb[dk.rb_w_ptr] = msg;
    dk.rb_w_ptr = new_w_ptr;
}

template <class T> void db_tick(T &db)
{
    auto ret = db.tick();
    auto v = db.get_val();
    if(ret & pin_state::StableHigh)
    {
        d.keystate = d.keystate & ~(1U << v);
    }
    else if(ret & pin_state::StableLow)
    {
        d.keystate = d.keystate | (1U << v);
    }
    if(ret & pin_state::NewStableState)
    {
        if(ret & pin_state::StableHigh)
        {
            send_message(CM33_DK_MSG_PRESS | v);
        }
        else if(ret & pin_state::StableLow)
        {
            send_message(CM33_DK_MSG_RELEASE | v);
        }
    }
    if(ret & pin_state::LongPress)
    {
        send_message(CM33_DK_MSG_LONGPRESS | v);
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

static int16_t joy_apply_calibration(int16_t in, 
    int16_t left, int16_t middle, int16_t right)
{
    // scale "in" to either between [left, middle] or [middle, right] such that left = -32768 and right = 32767
    if(in <= left)
        return -32768;
    else if(in == middle)
        return 0;
    else if(in >= right)
        return 32767;
    else
    {
        int32_t res;
        if(in < middle)
        {
            // scale [left, middle] to [-32767, 0]
            res = (int32_t)((float)(in - left) / (float)(middle - left) * 32767.0f - 32767.0f);
        }
        else
        {
            // scale [middle, right] to [0, 32767]
            res = (int32_t)((float)(in - middle) / (float)(right - middle) * 32767.0f);
        }
        if(res < -32768) res = -32768;
        if(res > 32767) res = 32767;
        return res;
    }
}

static void joy_apply_calibration(const volatile cm33_joystick *in,
    volatile cm33_joystick *out,
    const volatile cm33_joy_calib *calib = nullptr)
{
    out->res0 = 0;
    out->res1 = 0;
    if(!calib)
    {
        out->x = in->x;
        out->y = in->y;
        return;
    }

    out->x = joy_apply_calibration(in->x, calib->left, calib->middle_x, calib->right);
    out->y = joy_apply_calibration(in->y, calib->bottom, calib->middle_y, calib->top);
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

        Add dead zone in the middle of 8k (after scaling and calibration), then pass through a debouncer for digital inputs */
    
    dk.joy_a_raw.x = joy_scale(adc_vals[0], false);
    dk.joy_a_raw.y = joy_scale(adc_vals[1], false);
    dk.joy_b_raw.x = joy_scale(adc_vals[3], false);
    dk.joy_b_raw.y = joy_scale(adc_vals[2], true);

    joy_apply_calibration(&dk.joy_a_raw, &d.joy_a, &dk.joy_a_calib);
    joy_apply_calibration(&dk.joy_b_raw, &d.joy_b, &dk.joy_b_calib);
}

static void tick()
{
    // handle any commands
    if(dk.cr)
    {
        switch(dk.cr)
        {
            case CM33_DK_CMD_TILT_ENABLE:
                dk.sr = dk.sr | CM33_DK_SR_TILT_ENABLE;
                break;
            case CM33_DK_CMD_TILT_DISABLE:
                dk.sr = dk.sr & ~CM33_DK_SR_TILT_ENABLE;
                break;
        }
        dk.cr = 0;
        __SEV();
    }

    uint8_t ioexp_vals[2];
    auto &i2c1 = i2c(1);
    if(i2c1.RegisterRead(0x20, (uint8_t)0, ioexp_vals, 2) == 2)
    {
        ioexp_keystate = (unsigned int)ioexp_vals[0] |
            (((unsigned int)ioexp_vals[1]) << 8);
    }

    joystick_tick();

    if(dk.sr & CM33_DK_SR_TILT_ENABLE)
    {
        lsm_ret = lsm_poll();
        if(lsm_ret == 0)
        {
            /* convert lsm filtered axes to a joystick
                "pitch" is -ve left/+ve right - aim for a ~15 deg deadspace
                "roll" is +ve look up (i.e. stick down), -ve look down, again add 15 deg deadspace, will need calibration

                joy debouncer uses 8000/32000 as its deadspace, therefore scale to +/- 60 degree from the middle
            */
            auto new_tilt_x = (int32_t)(d.pitch * 32767.0f / 60.0f);
            auto new_tilt_y = (int32_t)((-d.roll - dk.tilt_zero) * 32767.0f / 60.0f);
            if(new_tilt_x < -32768) new_tilt_x = -32768;
            if(new_tilt_x > 32767) new_tilt_x = 32767;
            if(new_tilt_y < -32768) new_tilt_y = -32768;
            if(new_tilt_y > 32767) new_tilt_y = 32767;

            d.joy_tilt.x = (int16_t)new_tilt_x;
            d.joy_tilt.y = (int16_t)new_tilt_y;
        }
    }
    else
    {
        lsm_disable();
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
    db_tick(db_JOY_A_LEFT);
    db_tick(db_JOY_A_RIGHT);
    db_tick(db_JOY_A_UP);
    db_tick(db_JOY_A_DOWN);
    db_tick(db_JOY_B_LEFT);
    db_tick(db_JOY_B_RIGHT);
    db_tick(db_JOY_B_UP);
    db_tick(db_JOY_B_DOWN);

    if(dk.sr & CM33_DK_SR_TILT_ENABLE)
    {
        db_tick(db_JOY_TILT_LEFT);
        db_tick(db_JOY_TILT_RIGHT);
        db_tick(db_JOY_TILT_UP);
        db_tick(db_JOY_TILT_DOWN);
    }

    if(dk.rb_r_ptr != dk.rb_w_ptr)
    {
        __SEV();
    }
}

extern "C" void TIM6_IRQHandler()
{
    clock_tick();
    tick();
    TIM6->SR = 0;
    __DMB();
}

// called when an unhandled exception occurs.  Signals the CA35 to restart the core.
extern "C" void FailHandler()
{
    dk.sr = dk.sr | CM33_DK_SR_FAIL;
    __SEV();
    while(true)
    {
        __WFI();
    }
}
