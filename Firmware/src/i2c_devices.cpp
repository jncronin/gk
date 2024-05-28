#include "stm32h7xx.h"
#include "pins.h"
#include "scheduler.h"
#include "clocks.h"
#include "process.h"
#include "osmutex.h"
#include "gk_conf.h"
#include "ossharedmem.h"
#include "i2c.h"
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

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
    bool restart_after_read;
    unsigned char i2c_address;
    char *buf;
    size_t nbytes;
    SimpleSignal *ss;
};

SRAM4_DATA FixedQueue<i2c_msg, 32> i2c_msgs;
SRAM4_DATA static volatile i2c_msg cur_i2c_msg;
SRAM4_DATA static volatile unsigned int n_xmit;
SRAM4_DATA static volatile unsigned int n_tc_end;

int i2c_xmit(unsigned int addr, void *buf, size_t nbytes, bool is_read)
{
    auto ss = new SimpleSignal();
    if(!ss)
        return -1;
    
    i2c_msg msg;
    msg.i2c_address = addr;
    msg.buf = (char *)buf;
    msg.is_read = is_read;
    msg.nbytes = nbytes;
    msg.restart_after_read = false;
    msg.ss = ss;

    if(!i2c_msgs.Push(msg))
    {
        delete ss;
        return -1;
    }

    auto ret = ss->Wait();
    delete ss;
    if(ret & 0x80000000U)
    {
        ret &= ~0x80000000U;
        return -((int)ret);
    }
    else
    {
        return ret;
    }
}

int i2c_read(unsigned int addr, const void *buf, size_t nbytes)
{
    return i2c_xmit(addr, (void *)buf, nbytes, true);
}

int i2c_send(unsigned int addr, void *buf, size_t nbytes)
{
    return i2c_xmit(addr, buf, nbytes, false);
}

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

    Block(clock_cur_ms() + 5ULL);
}

void init_i2c()
{
    auto t_i2c = Thread::Create("i2c", i2c_thread, nullptr, true, GK_PRIORITY_HIGH, p_kernel_proc, PreferM4);
    auto t_ctp = Thread::Create("ctp340", cst_thread, nullptr, true, GK_PRIORITY_HIGH, p_kernel_proc, PreferM4);
    auto t_lsm = Thread::Create("lsm6dsl", lsm_thread, nullptr, true, GK_PRIORITY_HIGH, p_kernel_proc, PreferM4);
    auto t_max = Thread::Create("max17048", max_thread, nullptr, true, GK_PRIORITY_HIGH, p_kernel_proc, PreferM4);
    Schedule(t_i2c);
    Schedule(t_ctp);
    Schedule(t_lsm);
    Schedule(t_max);
}

static uint32_t calc_cr2(unsigned int i2c_address, bool is_read,
    unsigned int nbytes, unsigned int n_transmitted,
    bool restart_after_read, unsigned int *n_after_current_tc)
{
    uint32_t ret = (cur_i2c_msg.i2c_address << 1) |
                (cur_i2c_msg.is_read ? I2C_CR2_RD_WRN : 0U);
    auto to_xmit = nbytes - n_transmitted;
    if(to_xmit < 256)
    {
        ret |= (to_xmit << I2C_CR2_NBYTES_Pos);
        if(!restart_after_read)
            ret |= I2C_CR2_AUTOEND;
    }
    else
    {
        ret |= (0xffU << I2C_CR2_NBYTES_Pos) |
            I2C_CR2_RELOAD;
    }
    if(n_after_current_tc)
        *n_after_current_tc = n_transmitted + to_xmit;
    return ret;
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

            i2c->ICR = 0x3f38U;

            // handle message
            n_xmit = 0;

            auto cr2 = calc_cr2(cur_i2c_msg.i2c_address, cur_i2c_msg.is_read,
                cur_i2c_msg.nbytes, n_xmit, cur_i2c_msg.restart_after_read,
                (unsigned int *)&n_tc_end);
            i2c->CR2 = cr2;

            i2c->CR2 = cr2 | I2C_CR2_START;

            unsigned int wait_flag = cur_i2c_msg.is_read ? I2C_ISR_RXNE : I2C_ISR_TXIS;

            // synchronous for now
            while(!(i2c->ISR & (wait_flag | I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO)));
            if(i2c->ISR & (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO))
            {
                // fail
                if(cur_i2c_msg.ss)
                {
                    cur_i2c_msg.ss->Signal(SimpleSignal::Set, 0x80000000UL | i2c->ISR);
                }
                if(i2c->ISR & (I2C_ISR_BERR | I2C_ISR_ARLO))
                {
                    i2c_init = false;
                }
                else
                {
                    i2c->ICR = I2C_ICR_NACKCF;
                }
            }
            else if(cur_i2c_msg.is_read)
            {
                SharedMemoryGuard(cur_i2c_msg.buf, cur_i2c_msg.nbytes, false, true);
                bool cont = true;
                while(cont)
                {
                    while(!(i2c->ISR & wait_flag));
                    auto d = i2c->RXDR;
                    cur_i2c_msg.buf[n_xmit++] = d;
                    if(n_xmit == n_tc_end)
                    {
                        if(i2c->ISR & I2C_ISR_TC)
                        {
                            // finished
                            cont = false;
                            if(cur_i2c_msg.ss)
                            {
                                cur_i2c_msg.ss->Signal(SimpleSignal::Set, n_xmit);
                            }
                        }
                        else if(i2c->ISR & I2C_ISR_TCR)
                        {
                            // reload
                            i2c->CR2 = calc_cr2(cur_i2c_msg.i2c_address, cur_i2c_msg.is_read,
                                cur_i2c_msg.nbytes, n_xmit, cur_i2c_msg.restart_after_read,
                                (unsigned int *)&n_tc_end);
                        }
                        else
                        {
                            // early finish
                            cont = false;
                            if(cur_i2c_msg.ss)
                            {
                                cur_i2c_msg.ss->Signal(SimpleSignal::Set, n_xmit);
                            }
                        }
                    }
                }
            }
            else    // is write
            {
                SharedMemoryGuard(cur_i2c_msg.buf, cur_i2c_msg.nbytes, true, false);
                bool cont = true;
                while(cont)
                {
                    while(!(i2c->ISR & wait_flag));
                    auto d = cur_i2c_msg.buf[n_xmit++];
                    i2c->TXDR = d;
                    if(n_xmit == n_tc_end)
                    {
                        if(i2c->ISR & I2C_ISR_TC)
                        {
                            // finished
                            cont = false;
                            if(cur_i2c_msg.ss)
                            {
                                cur_i2c_msg.ss->Signal(SimpleSignal::Set, n_xmit);
                            }
                        }
                        else if(i2c->ISR & I2C_ISR_TCR)
                        {
                            // reload
                            i2c->CR2 = calc_cr2(cur_i2c_msg.i2c_address, cur_i2c_msg.is_read,
                                cur_i2c_msg.nbytes, n_xmit, cur_i2c_msg.restart_after_read,
                                (unsigned int *)&n_tc_end);
                        }
                        else
                        {
                            // early finish
                            cont = false;
                            if(cur_i2c_msg.ss)
                            {
                                cur_i2c_msg.ss->Signal(SimpleSignal::Set, n_xmit);
                            }
                        }
                    }
                }
            }
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

            // check read
            char buf[4];
            [[maybe_unused]] auto ret = i2c_read(0x1a, buf, 4);

#if DEBUG_I2C
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "cst: read returned %d\n", ret);
            }
#endif
            
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
