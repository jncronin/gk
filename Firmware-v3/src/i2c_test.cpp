#include "pins.h"
#include "SEGGER_RTT.h"

static const constexpr pin I2C2_SDA { GPIOF, 0, 4 };
static const constexpr pin I2C2_SCL { GPIOF, 1, 4 };

#define i2c I2C2

struct i2c_msg
{
    bool is_read;
    bool restart_after_write;
    unsigned char i2c_address;
    char *buf, *buf2 = nullptr;
    size_t nbytes, nbytes2 = 0;
    //SimpleSignal *ss;
    uint8_t regaddr_buf[2];
};

static i2c_msg cur_i2c_msg;
static volatile unsigned int n_xmit;
static volatile unsigned int n_tc_end;
bool i2c_init = false;

static int i2c_dotfer();

void init_i2c()
{

}

static void reset_i2c()
{
    RCC->APB1ENR1 &= ~RCC_APB1ENR1_I2C2EN;
    (void)RCC->APB1ENR1;
    RCC->APB1RSTR1 = RCC_APB1RSTR1_I2C2RST;
    (void)RCC->APB1RSTR1;
    RCC->APB1RSTR1 = 0;
    (void)RCC->APB1RSTR1;
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C2EN;
    (void)RCC->APB1ENR1;

    // Toggle pins to reset digital filter
    I2C2_SDA.set_as_output(pin::OpenDrain);
    I2C2_SCL.set_as_output(pin::OpenDrain);
    I2C2_SDA.clear();
    I2C2_SCL.clear();
    for(int i = 0; i < 10000; i++) __DMB();
    I2C2_SDA.set();
    I2C2_SCL.set();
    for(int i = 0; i < 10000; i++) __DMB();

    I2C2_SDA.set_as_af(pin::OpenDrain);
    I2C2_SCL.set_as_af(pin::OpenDrain);

    i2c->CR1 = 0U;

    // clock to I2C2 is currently pclk1, which is HSI64
    // configure TIMINGR as per RM p2261 with /8 prescaler -> 400 kHz
    i2c->TIMINGR = (7U << I2C_TIMINGR_PRESC_Pos) |
        (0x9U << I2C_TIMINGR_SCLL_Pos) |
        (0x3U << I2C_TIMINGR_SCLH_Pos) |
        (0x1U << I2C_TIMINGR_SDADEL_Pos) |
        (0x3U << I2C_TIMINGR_SCLDEL_Pos);

    i2c->CR1 = I2C_CR1_PE;

    for(int i = 0; i < 50000; i++) __DMB();
}

static uint32_t calc_cr2(unsigned int i2c_address, bool is_read,
    unsigned int nbytes, unsigned int n_transmitted,
    bool restart_after_write, unsigned int *n_after_current_tc)
{
    uint32_t ret = (i2c_address << 1) |
                (is_read ? I2C_CR2_RD_WRN : 0U);
    auto to_xmit = nbytes - n_transmitted;
    if(to_xmit < 256)
    {
        ret |= (to_xmit << I2C_CR2_NBYTES_Pos);
        if(!restart_after_write)
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

int i2c_xmit(unsigned int addr, void *buf, size_t nbytes, bool is_read)
{
    cur_i2c_msg.i2c_address = addr;
    cur_i2c_msg.buf = (char *)buf;
    cur_i2c_msg.is_read = is_read;
    cur_i2c_msg.nbytes = nbytes;
    cur_i2c_msg.nbytes2 = 0;
    cur_i2c_msg.restart_after_write = false;
    //msg.ss = ss;
    return i2c_dotfer();
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
    cur_i2c_msg.i2c_address = addr;
    cur_i2c_msg.regaddr_buf[0] = reg;
    cur_i2c_msg.buf = nullptr;
    cur_i2c_msg.is_read = false;
    cur_i2c_msg.nbytes = 1;
    cur_i2c_msg.buf2 = (char *)buf;
    cur_i2c_msg.nbytes2 = nbytes;
    cur_i2c_msg.restart_after_write = false;
    //msgs[0].ss = &GetCurrentThreadForCore()->ss;
    return i2c_dotfer();
}

int i2c_register_write(unsigned int addr, uint16_t reg, const void *buf, size_t nbytes)
{
    cur_i2c_msg.i2c_address = addr;
    cur_i2c_msg.regaddr_buf[0] = reg;
    cur_i2c_msg.regaddr_buf[1] = (reg >> 8) & 0xff;
    cur_i2c_msg.buf = nullptr;
    cur_i2c_msg.is_read = false;
    cur_i2c_msg.nbytes = 2;
    cur_i2c_msg.buf2 = (char *)buf;
    cur_i2c_msg.nbytes2 = nbytes;
    cur_i2c_msg.restart_after_write = false;
    //msgs[0].ss = &GetCurrentThreadForCore()->ss;
    return i2c_dotfer();
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
    msgs[0].restart_after_write = true;
    //msgs[0].ss = nullptr;
    cur_i2c_msg = msgs[0];
    auto m1_ret = i2c_dotfer();
    if(m1_ret < 0) return m1_ret;

    msgs[1].i2c_address = addr;
    msgs[1].buf = (char *)buf;
    msgs[1].is_read = true;
    msgs[1].nbytes = nbytes;
    msgs[1].nbytes2 = 0;
    msgs[1].restart_after_write = false;
    //msgs[1].ss = &GetCurrentThreadForCore()->ss;
    cur_i2c_msg = msgs[1];
    return i2c_dotfer();
}

int i2c_register_read(unsigned int addr, uint16_t reg, void *buf, size_t nbytes)
{
    i2c_msg msgs[2];

    msgs[0].i2c_address = addr;
    msgs[0].regaddr_buf[0] = reg;
    msgs[0].regaddr_buf[1] = (reg >> 8) & 0xff;
    msgs[0].buf = nullptr;
    msgs[0].is_read = false;
    msgs[0].nbytes = 2;
    msgs[0].nbytes2 = 0;
    msgs[0].restart_after_write = true;
    //msgs[0].ss = nullptr;
    cur_i2c_msg = msgs[0];
    auto m1_ret = i2c_dotfer();
    if(m1_ret < 0) return m1_ret;

    msgs[1].i2c_address = addr;
    msgs[1].buf = (char *)buf;
    msgs[1].is_read = true;
    msgs[1].nbytes = nbytes;
    msgs[1].nbytes2 = 0;
    msgs[1].restart_after_write = false;
    //msgs[1].ss = &GetCurrentThreadForCore()->ss;
    cur_i2c_msg = msgs[1];
    return i2c_dotfer();
}

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

static int i2c_dotfer()
{
    // just do basic polling transfer
    if(!i2c_init)
    {
        SEGGER_RTT_printf(0, "i2c init starting\n");
        reset_i2c();
        SEGGER_RTT_printf(0, "i2c reset complete\n");
        i2c_init = true;
    }

    // for now, just see if we get ACK
    i2c->ICR = 0x3f38U;
    n_xmit = 0;

    auto cr2 = calc_cr2(cur_i2c_msg.i2c_address, cur_i2c_msg.is_read,
        cur_i2c_msg.nbytes + cur_i2c_msg.nbytes2, n_xmit, cur_i2c_msg.restart_after_write,
        (unsigned int *)&n_tc_end);
    i2c->CR2 = cr2;
    i2c->CR2 = cr2 | I2C_CR2_START;

    unsigned int wait_flag = cur_i2c_msg.is_read ? I2C_ISR_RXNE : I2C_ISR_TXIS;
    while(!(i2c->ISR & (wait_flag | I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO)));
    if(i2c->ISR & (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO))
    {
        // fail
        if(i2c->ISR & I2C_ISR_BERR)
        {
            SEGGER_RTT_printf(0, "i2c: BERR\n");
            i2c_init = false;
            return -1;
        }
        else if(i2c->ISR & I2C_ISR_ARLO)
        {
            SEGGER_RTT_printf(0, "i2c: ARLO\n");
            i2c_init = false;
            return -1;
        }
        else
        {
            SEGGER_RTT_printf(0, "i2c: NACF\n");
            i2c->ICR = I2C_ICR_NACKCF;
            return -1;
        }
    }
    else if(cur_i2c_msg.is_read)
    {
        SEGGER_RTT_printf(0, "i2c: ACK for read\n");

        bool cont = true;
        while(cont)
        {
            while(!(i2c->ISR & wait_flag));
            auto d = i2c->RXDR;
            *cur_buf_p((i2c_msg*)&cur_i2c_msg, n_xmit++) = d;
            if(n_xmit == n_tc_end)
            {
                while(!(i2c->ISR & (I2C_ISR_TC | I2C_ISR_TCR | I2C_ISR_STOPF)));
                if((cur_i2c_msg.restart_after_write && (i2c->ISR & I2C_ISR_TC)) ||
                    (i2c->ISR & I2C_ISR_STOPF))
                {
                    // finished
                    cont = false;
                    //if(cur_i2c_msg.ss)
                    //{
                    //    cur_i2c_msg.ss->Signal(SimpleSignal::Set, n_xmit);
                    //}
                    SEGGER_RTT_printf(0, "i2c: read success\n");
                    return n_xmit;
                }
                else if(i2c->ISR & I2C_ISR_TCR)
                {
                    // reload
                    i2c->CR2 = calc_cr2(cur_i2c_msg.i2c_address, cur_i2c_msg.is_read,
                        cur_i2c_msg.nbytes, n_xmit, cur_i2c_msg.restart_after_write,
                        (unsigned int *)&n_tc_end);
                }
                else
                {
                    {
                        SEGGER_RTT_printf(0, "i2c: early finish\n");
                    }
                    // early finish
                    cont = false;
                    return n_xmit;
                }
            }
        }
    }
    else
    {
        SEGGER_RTT_printf(0, "i2c: ACK for write\n");
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
                    //CriticalGuard cg(s_rtt);
                    //klog("i2c: NACKF during write phase\n");
                    SEGGER_RTT_printf(0, "i2c: NACKF during write phase\n");
                }
                if((cur_i2c_msg.restart_after_write && (i2c->ISR & I2C_ISR_TC)) ||
                    (i2c->ISR & I2C_ISR_STOPF))
                {
                    // finished
                    cont = false;
                    //if(cur_i2c_msg.ss)
                    //{
                    //    cur_i2c_msg.ss->Signal(SimpleSignal::Set, n_xmit);
                    //}
                    SEGGER_RTT_printf(0, "i2c: write success\n");
                    return n_xmit;
                }
                else if(i2c->ISR & I2C_ISR_TCR)
                {
                    // reload
                    i2c->CR2 = calc_cr2(cur_i2c_msg.i2c_address, cur_i2c_msg.is_read,
                        cur_i2c_msg.nbytes, n_xmit, cur_i2c_msg.restart_after_write,
                        (unsigned int *)&n_tc_end);
                }
                else
                {
                    {
                        //CriticalGuard cg(s_rtt);
                        //klog("i2c: early write finish %u, %x\n", n_xmit, i2c->ISR);
                        SEGGER_RTT_printf(0, "i2c: early write finish\n");
                    }
                    // early finish
                    cont = false;
                    //if(cur_i2c_msg.ss)
                    //{
                    //    cur_i2c_msg.ss->Signal(SimpleSignal::Set, n_xmit);
                    //}
                    return n_xmit;
                }
            }
        }
    }

    // why would we get here?
    __asm__ volatile("bkpt \n" ::: "memory");
    return 0;
}
