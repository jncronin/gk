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

#define DEBUG_I2C 1

extern Spinlock s_rtt;

/* The I2C1 bus runs at 400 kHz and contains:
        LSM6DSL gyro/accelerometer:         Address 0x6a
        CST340 touch screen controller:     Address 0x1a (see https://sbexr.rabexc.org/latest/sources/b1/4dc8ab017aac8c.html)
        MAX17048 battery charge monitor:    Address

    I2C1 clock is 24 MHz - we can use the 8MHz examples from RM with a /3 prescaler

*/

#define i2c I2C1

constexpr const pin LSM_INT { GPIOI, 0 };
constexpr const pin I2C1_SDA { GPIOB, 7, 4 };
constexpr const pin I2C1_SCL { GPIOB, 6, 4 };

static void *i2c_thread(void *p);
void *lsm_thread(void *p);
void *cst_thread(void *p);
static void *max_thread(void *p);

SRAM4_DATA bool i2c_init = false;

struct i2c_msg
{
    bool is_read;
    bool restart_after_read;
    unsigned char i2c_address;
    char *buf, *buf2 = nullptr;
    size_t nbytes, nbytes2 = 0;
    SimpleSignal *ss;
    uint8_t regaddr_buf[2];
};

SRAM4_DATA FixedQueue<i2c_msg, 32> i2c_msgs;
SRAM4_DATA static volatile i2c_msg cur_i2c_msg;
SRAM4_DATA static volatile unsigned int n_xmit;
SRAM4_DATA static volatile unsigned int n_tc_end;

static volatile char *cur_buf_p(const i2c_msg *msg, unsigned int n)
{
    char *buf;
    if(n < msg->nbytes)
    {
        buf = msg->buf;
    }
    else
    {
        buf = msg->buf2;
        n -= msg->nbytes;
    }
    if(buf)
        return &buf[n];
    else
        return (char *)&msg->regaddr_buf[n];
}

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
    msg.nbytes2 = 0;
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

int i2c_register_write(unsigned int addr, uint8_t reg, const void *buf, size_t nbytes)
{
    i2c_msg msgs[1];

    msgs[0].i2c_address = addr;
    msgs[0].regaddr_buf[0] = reg;
    msgs[0].buf = nullptr;
    msgs[0].is_read = false;
    msgs[0].nbytes = 1;
    msgs[0].buf2 = (char *)buf;
    msgs[0].nbytes2 = nbytes;
    msgs[0].restart_after_read = false;
    msgs[0].ss = &GetCurrentThreadForCore()->ss;

    if(i2c_msgs.Push(msgs, 1) != 1)
    {
        return -1;
    }

    GetCurrentThreadForCore()->ss.Wait(SimpleSignal::SignalOperation::Set, 0);
    return 0;
}

int i2c_register_write(unsigned int addr, uint16_t reg, const void *buf, size_t nbytes)
{
    i2c_msg msgs[1];

    msgs[0].i2c_address = addr;
    msgs[0].regaddr_buf[0] = reg & 0xff;
    msgs[0].regaddr_buf[1] = (reg >> 8) & 0xff;
    msgs[0].buf = nullptr;
    msgs[0].is_read = false;
    msgs[0].nbytes = 2;
    msgs[0].buf2 = (char *)buf;
    msgs[0].nbytes2 = nbytes;
    msgs[0].restart_after_read = false;
    msgs[0].ss = &GetCurrentThreadForCore()->ss;

    if(i2c_msgs.Push(msgs, 1) != 1)
    {
        return -1;
    }

    GetCurrentThreadForCore()->ss.Wait(SimpleSignal::SignalOperation::Set, 0);
    return 0;
}

int i2c_register_read(unsigned int addr, uint8_t reg, void *buf, size_t nbytes)
{
    i2c_msg msgs[2];

    msgs[0].i2c_address = addr;
    msgs[0].regaddr_buf[0] = reg;
    msgs[0].buf = nullptr;
    msgs[0].is_read = false;
    msgs[0].nbytes = 1;
    msgs[0].nbytes2 = 0;
    msgs[0].restart_after_read = true;
    msgs[0].ss = nullptr;

    msgs[1].i2c_address = addr;
    msgs[1].buf = (char *)buf;
    msgs[1].is_read = true;
    msgs[1].nbytes = nbytes;
    msgs[1].nbytes2 = 0;
    msgs[1].restart_after_read = false;
    msgs[1].ss = &GetCurrentThreadForCore()->ss;

    if(i2c_msgs.Push(msgs, 2) != 2)
    {
        return -1;
    }

    GetCurrentThreadForCore()->ss.Wait(SimpleSignal::SignalOperation::Set, 0);
    return 0;
}

int i2c_register_read(unsigned int addr, uint16_t reg, void *buf, size_t nbytes)
{
    i2c_msg msgs[2];

    msgs[0].i2c_address = addr;
    msgs[0].regaddr_buf[0] = reg & 0xff;
    msgs[0].regaddr_buf[1] = (reg >> 8) & 0xff;
    msgs[0].buf = nullptr;
    msgs[0].is_read = false;
    msgs[0].nbytes = 2;
    msgs[0].nbytes2 = 0;
    msgs[0].restart_after_read = true;
    msgs[0].ss = nullptr;

    msgs[1].i2c_address = addr;
    msgs[1].buf = (char *)buf;
    msgs[1].is_read = true;
    msgs[1].nbytes = nbytes;
    msgs[1].nbytes2 = 0;
    msgs[1].restart_after_read = false;
    msgs[1].ss = &GetCurrentThreadForCore()->ss;

    if(i2c_msgs.Push(msgs, 2) != 2)
    {
        return -1;
    }

    GetCurrentThreadForCore()->ss.Wait(SimpleSignal::SignalOperation::Set, 0);
    return 0;
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
    Block(clock_cur() + kernel_time::from_ms(1));
    I2C1_SDA.set();
    I2C1_SCL.set();
    Block(clock_cur() + kernel_time::from_ms(1));

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

    Block(clock_cur() + kernel_time::from_ms(5));
}

void init_i2c()
{
    auto t_i2c = Thread::Create("i2c", i2c_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc, PreferM4);
#if GK_ENABLE_CTP340
    auto t_ctp = Thread::Create("ctp340", cst_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc, PreferM4);
    Schedule(t_ctp);
#endif
    auto t_lsm = Thread::Create("lsm6dsl", lsm_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc, PreferM4);
    auto t_max = Thread::Create("max17048", max_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc, PreferM4);
    Schedule(t_i2c);
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
        to_xmit = 255;
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
                cur_i2c_msg.nbytes + cur_i2c_msg.nbytes2, n_xmit, cur_i2c_msg.restart_after_read,
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
#if DEBUG_I2C
                    {
                        CriticalGuard cg(s_rtt);
                        SEGGER_RTT_printf(0, "i2c: transfer failing %x\n", i2c->ISR);
                    }
#endif
                    i2c_init = false;
                }
                else
                {
#if DEBUG_I2C
                    {
                        CriticalGuard cg(s_rtt);
                        SEGGER_RTT_printf(0, "i2c: NACK\n");
                    }
#endif
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
                    *cur_buf_p((i2c_msg*)&cur_i2c_msg, n_xmit++) = d;
                    if(n_xmit == n_tc_end)
                    {
                        while(!(i2c->ISR & (I2C_ISR_TC | I2C_ISR_TCR | I2C_ISR_STOPF)));
                        if((cur_i2c_msg.restart_after_read && (i2c->ISR & I2C_ISR_TC)) ||
                            (i2c->ISR & I2C_ISR_STOPF))
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
#if DEBUG_I2C
                            {
                                CriticalGuard cg(s_rtt);
                                SEGGER_RTT_printf(0, "i2c: early read finish %u, %x\n", n_xmit, i2c->ISR);
                            }
#endif
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
                    auto d = *cur_buf_p((i2c_msg *)&cur_i2c_msg, n_xmit++);
                    i2c->TXDR = d;
                    if(n_xmit == n_tc_end)
                    {
                        while(!(i2c->ISR & (I2C_ISR_TC | I2C_ISR_TCR | I2C_ISR_STOPF)));
                        if(i2c->ISR & I2C_ISR_NACKF)
                        {
                            CriticalGuard cg(s_rtt);
                            SEGGER_RTT_printf(0, "i2c: NACKF during write phase\n");
                        }
                        if((cur_i2c_msg.restart_after_read && (i2c->ISR & I2C_ISR_TC)) ||
                            (i2c->ISR & I2C_ISR_STOPF))
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
#if DEBUG_I2C
                            {
                                CriticalGuard cg(s_rtt);
                                SEGGER_RTT_printf(0, "i2c: early write finish %u, %x\n", n_xmit, i2c->ISR);
                            }
#endif
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

void *max_thread(void *param)
{
    while(true)
    {
        // TODO
        Block();
    }
}
