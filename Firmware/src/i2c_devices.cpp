#include "stm32h7xx.h"
#include "pins.h"
#include "scheduler.h"
#include "clocks.h"
#include "process.h"
#include "osmutex.h"
#include "gk_conf.h"

/* The I2C1 bus runs at 400 kHz and contains:
        LSM6DSL gyro/accelerometer:         Address
        CST340 touch screen controller:     Address 0x1a (see https://sbexr.rabexc.org/latest/sources/b1/4dc8ab017aac8c.html)
        MAX17048 battery charge monitor:    Address

    I2C1 clock is 24 MHz - we can use the 8MHz examples from RM with a /3 prescaler

    The CST340 chip is complex - see https://git.zx2c4.com/linux/plain/drivers/input/touchscreen/hynitron_cstxxx.c

*/

#define i2c I2C1

constexpr const pin CTP_NRESET { GPIOC, 13 };
constexpr const pin CTP_INT { GPIOC, 14 };
constexpr const pin LSM_INT { GPIOI, 0 };
constexpr const pin I2C1_SDA { GPIOB, 7, 4 };
constexpr const pin I2C1_SCL { GPIOB, 6, 4 };

static void *i2c_thread(void *p);
static void *lsm_thread(void *p);
static void *cst_thread(void *p);
static void *max_thread(void *p);

SRAM4_DATA bool i2c_init = false;
SRAM4_DATA bool ctp_init = false;
SRAM4_DATA static BinarySemaphore sem_ctp;

struct i2c_msg
{
    bool is_read;
    unsigned char i2c_address;
    void *buf;
    size_t nbytes;
};

SRAM4_DATA FixedQueue<i2c_msg, 32> i2c_msgs;
SRAM4_DATA static volatile i2c_msg cur_i2c_msg;

static void reset_i2c()
{
    RCC->APB1LENR &= ~RCC_APB1LENR_I2C1EN;
    (void)RCC->APB1LENR;
    RCC->APB1LRSTR = RCC_APB1LRSTR_I2C1RST;
    (void)RCC->APB1LRSTR;
    RCC->APB1LRSTR = 0;
    (void)RCC->APB1LRSTR;
    RCC->APB1LENR |= RCC_APB1LENR_I2C1EN;
    (void)RCC->APB1LENR;

    // Toggle pins to reset digital filter
    I2C1_SDA.set_as_output(pin::OpenDrain);
    I2C1_SCL.set_as_output(pin::OpenDrain);
    I2C1_SDA.clear();
    I2C1_SCL.clear();
    Block(clock_cur_ms() + 1ULL);
    I2C1_SDA.set();
    I2C1_SCL.set();
    Block(clock_cur_ms() + 1ULL);

    I2C1_SDA.set_as_af(pin::OpenDrain);
    I2C1_SCL.set_as_af(pin::OpenDrain);

    i2c->CR1 = 0U;

    // configure TIMINGR as per RM p2135 with /3 prescaler
    i2c->TIMINGR = (2U << I2C_TIMINGR_PRESC_Pos) |
        (0x9U << I2C_TIMINGR_SCLL_Pos) |
        (0x3U << I2C_TIMINGR_SCLH_Pos) |
        (0x1U << I2C_TIMINGR_SDADEL_Pos) |
        (0x3U << I2C_TIMINGR_SCLDEL_Pos);

    i2c->CR1 = I2C_CR1_PE;

    i2c_init = true;
}

void init_i2c()
{
    auto t_i2c = Thread::Create("i2c", i2c_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc, PreferM4);
    auto t_ctp = Thread::Create("ctp340", cst_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc, PreferM4);
    auto t_lsm = Thread::Create("lsm6dsl", lsm_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc, PreferM4);
    auto t_max = Thread::Create("max17048", max_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc, PreferM4);
    Schedule(t_i2c);
    Schedule(t_ctp);
    Schedule(t_lsm);
    Schedule(t_max);
}

void *i2c_thread(void *params)
{
    while(true)
    {
        if(i2c_msgs.Pop(const_cast<i2c_msg *>(&cur_i2c_msg)))
        {
            if(!i2c_init)
            {
                reset_i2c();
            }

            // TODO handle message
        }
    }
}

void *cst_thread(void *param)
{
    while(true)
    {
        if(!ctp_init)
        {
            CTP_NRESET.set_as_output();
            CTP_NRESET.clear();
            Block(clock_cur_ms() + 50ULL);
            CTP_NRESET.set();
            Block(clock_cur_ms() + 300ULL);
            
            // TODO init CTP
        }

        sem_ctp.Wait(clock_cur_ms() + 100ULL);
        // TODO poll ctp
    }
}

void *lsm_thread(void *param)
{
    while(true)
    {
        // TODO
        Block();
    }
}

void *max_thread(void *param)
{
    while(true)
    {
        // TODO
        Block();
    }
}
