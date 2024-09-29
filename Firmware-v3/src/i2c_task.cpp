#include "stm32h7rsxx.h"
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

/* The I2C1 bus runs at 400 kHz and contains:
        LSM6DSL gyro/accelerometer:         Address 0x6a
        CST340 touch screen controller:     Address 0x1a (see https://sbexr.rabexc.org/latest/sources/b1/4dc8ab017aac8c.html)
        MAX17048 battery charge monitor:    Address 0x36
        GT911 touch screen:                 Address 0x5d
        LTC3380 VREG:                       Address 0x3c
        CDCE913 PLL:                        Address 0x65

    I2C1 clock is 24 MHz - we can use the 8MHz examples from RM with a /3 prescaler

*/

#define i2c I2C2

static const constexpr pin I2C2_SDA { GPIOF, 0, 4 };
static const constexpr pin I2C2_SCL { GPIOF, 1, 4 };
extern bool i2c_init;
extern void i2c_reset();

static void *i2c_thread(void *p);

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

int i2c_register_write(unsigned int addr, uint8_t reg, const void *buf, size_t nbytes, bool wait)
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
    msgs[0].ss = wait ? &GetCurrentThreadForCore()->ss : nullptr;

    if(i2c_msgs.Push(msgs, 1) != 1)
    {
        return -1;
    }

    if(wait)
        GetCurrentThreadForCore()->ss.Wait(SimpleSignal::SignalOperation::Set, 0);
    return 0;
}

int i2c_register_write(unsigned int addr, uint16_t reg, const void *buf, size_t nbytes, bool wait)
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
    msgs[0].ss = wait ? &GetCurrentThreadForCore()->ss : nullptr;

    if(i2c_msgs.Push(msgs, 1) != 1)
    {
        return -1;
    }

    if(wait)
        GetCurrentThreadForCore()->ss.Wait(SimpleSignal::SignalOperation::Set, 0);
    return 0;
}

int i2c_register_read(unsigned int addr, uint8_t reg, void *buf, size_t nbytes, bool wait)
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
    msgs[1].ss = wait ? &GetCurrentThreadForCore()->ss : nullptr;

    if(i2c_msgs.Push(msgs, 2) != 2)
    {
        return -1;
    }

    if(wait)
        GetCurrentThreadForCore()->ss.Wait(SimpleSignal::SignalOperation::Set, 0);
    return 0;
}

int i2c_register_read(unsigned int addr, uint16_t reg, void *buf, size_t nbytes, bool wait)
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
    msgs[1].ss = wait ? &GetCurrentThreadForCore()->ss : nullptr;

    if(i2c_msgs.Push(msgs, 2) != 2)
    {
        return -1;
    }

    if(wait)
        GetCurrentThreadForCore()->ss.Wait(SimpleSignal::SignalOperation::Set, 0);
    return 0;
}

void init_i2c()
{
    auto t_i2c = Thread::Create("i2c", i2c_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc, PreferM4);
    Schedule(t_i2c);
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
                i2c_reset();
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
                        klog("i2c: transfer failing %x\n", i2c->ISR);
                    }
#endif
                    i2c_init = false;
                }
                else
                {
#if DEBUG_I2C
                    {
                        klog("i2c: NACK\n");
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
                                klog("i2c: early read finish %u, %x\n", n_xmit, i2c->ISR);
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
                            klog("i2c: NACKF during write phase\n");
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
                                klog("i2c: early write finish %u, %x\n", n_xmit, i2c->ISR);
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
