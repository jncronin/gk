#include <stm32mp2xx.h>
#include "i2c.h"
#include "pins.h"
#include "clocks.h"
#include "kernel_time.h"

I2C i2cs[1];

static const constexpr pin i2c_pins[]
{
    { GPIOJ, 10, 9 },                 // I2C1_SDA
    { GPIOA, 3, 10 },                 // I2C1_SCL
};

[[maybe_unused]] static char msg_i2c_init_starting[] = "i2c: init starting\n";
[[maybe_unused]] static char msg_i2c_reset_complete[] = "i2c: init complete\n";
[[maybe_unused]] static char msg_i2c_berr[] = "i2c: BERR\n";
[[maybe_unused]] static char msg_i2c_arlo[] = "i2c: ARLO\n";
[[maybe_unused]] static char msg_i2c_nackf[] = "i2c: NACKF\n";
[[maybe_unused]] static char msg_i2c_ack_for_read[] = "i2c: ACK for read\n";
[[maybe_unused]] static char msg_i2c_read_success[] = "i2c: read success\n";
[[maybe_unused]] static char msg_i2c_read_early_finish[] = "i2c: early finish during read\n";
[[maybe_unused]] static char msg_i2c_ack_for_write[] = "i2c: ACK for write\n";
[[maybe_unused]] static char msg_i2c_write_success[] = "i2c: write success\n";
[[maybe_unused]] static char msg_i2c_write_early_finish[] = "i2c: early finish during write\n";

#define klog(x) 

void init_i2c()
{
    // init each instance
    i2cs[0].inst = I2C1;
    i2cs[0].SDA = i2c_pins[0];
    i2cs[0].SCL = i2c_pins[1];
    i2cs[0].rcc_reg = &RCC->I2C1CFGR;
}

I2C &i2c(unsigned int instance)
{
    return i2cs[instance - 1];
}

int I2C::Init()
{
    // toggle pins to avoid any busy glitches
    *rcc_reg = 0;
    __asm__ ("dsb sy\n" ::: "memory");
    *rcc_reg = RCC_I2C1CFGR_I2C1RST;
    __asm__ ("dsb sy\n" ::: "memory");
    
#if 1
    SDA.set_as_output(pin::OpenDrain);
    SCL.set_as_output(pin::OpenDrain);
    SDA.clear();
    SCL.clear();
    udelay(500);
    SDA.set();
    SCL.set();
    udelay(500);
#endif
    SDA.set_as_af(pin::OpenDrain);
    SCL.set_as_af(pin::OpenDrain);

    *rcc_reg |= RCC_I2C1CFGR_I2C1EN;
    __asm__ ("dsb sy\n" ::: "memory");
    *rcc_reg &= ~RCC_I2C1CFGR_I2C1RST;
    __asm__ ("dsb sy\n" ::: "memory");

    const bool irqs = false;

    inst->CR1 = 0U;

    // clock to I2C2 is currently pclk1, which is HSI64
    // configure TIMINGR as per RM p2261 with /8 prescaler -> 400 kHz
    inst->TIMINGR = (7U << I2C_TIMINGR_PRESC_Pos) |
        (0x9U << I2C_TIMINGR_SCLL_Pos) |
        (0x3U << I2C_TIMINGR_SCLH_Pos) |
        (0x1U << I2C_TIMINGR_SDADEL_Pos) |
        (0x3U << I2C_TIMINGR_SCLDEL_Pos);

    if(irqs)
    {
        inst->CR1 = I2C_CR1_PE | I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_ADDRIE |
            I2C_CR1_NACKIE | I2C_CR1_STOPIE | I2C_CR1_TCIE | I2C_CR1_ERRIE;
        //NVIC_EnableIRQ(I2C7_ER_IRQn);
        //NVIC_EnableIRQ(I2C7_EV_IRQn);
    }
    else
    {
        inst->CR1 = I2C_CR1_PE;
    }

    init = true;

    return 0;
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

int I2C::Read(unsigned int addr, void *buf, size_t nbytes)
{
    return Transmit(addr, buf, nbytes, nullptr, 0, true, false);
}

int I2C::Write(unsigned int addr, const void *buf, size_t nbytes)
{
    return Transmit(addr, (void *)buf, nbytes, nullptr, 0, false, false);
}

int I2C::RegisterRead(unsigned int addr, uint8_t reg, void *buf, size_t nbytes, bool wait)
{
    auto ret = Transmit(addr, &reg, 1, nullptr, 0, false, true);
    if(ret != 1)
        return ret;
    return Transmit(addr, buf, nbytes, nullptr, 0, true, false);
}

int I2C::RegisterRead(unsigned int addr, uint16_t reg, void *buf, size_t nbytes, bool wait)
{
    auto ret = Transmit(addr, &reg, 2, nullptr, 0, false, true);
    if(ret != 1)
        return ret;
    return Transmit(addr, buf, nbytes, nullptr, 0, true, false);
}

int I2C::RegisterWrite(unsigned int addr, uint8_t reg, const void *buf, size_t nbytes, bool wait)
{
    return Transmit(addr, &reg, 1, (void *)buf, nbytes, false, false) - 1;
}

int I2C::RegisterWrite(unsigned int addr, uint16_t reg, const void *buf, size_t nbytes, bool wait)
{
    return Transmit(addr, &reg, 2, (void *)buf, nbytes, false, false) - 1;
}

static volatile char *cur_buf_p(void *buf, size_t nbytes,
    void *buf2, size_t nbytes2, unsigned int n)
{
    char *b = nullptr;
    if(n < nbytes)
    {
        b = (char *)buf;
    }
    else
    {
        b = (char *)buf2;
        n -= nbytes;
    }
    if(b)
        return &b[n];
    else
        return nullptr;
}

unsigned int I2C::WaitTimeout(unsigned int wait_flag, unsigned int ms)
{
    auto tout = clock_cur() + kernel_time_from_ms(ms);
    while(!(inst->ISR & (wait_flag | I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO)))
    {
        if(clock_cur() > tout)
        {
            klog("i2c: ACK timeout\n");
            init = false;
            return 0;
        }
    }
    if(inst->ISR & (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO))
    {
        // fail
        if(inst->ISR & I2C_ISR_BERR)
        {
            klog(msg_i2c_berr);
            init = false;
            return I2C_ISR_BERR;
        }
        else if(inst->ISR & I2C_ISR_ARLO)
        {
            klog(msg_i2c_arlo);
            init = false;
            return I2C_ISR_ARLO;
        }
        else
        {
            klog(msg_i2c_nackf);
            inst->ICR = I2C_ICR_NACKCF;
            return I2C_ISR_NACKF;
        }
    }
    return inst->ISR & wait_flag;
}

int I2C::Transmit(unsigned int addr, void *buf, size_t nbytes, 
            void *buf2, size_t nbytes2,
            bool is_read, bool restart_after_write)
{
    if(!init)
    {
        Init();
    }

    inst->ICR = 0x3f38U;
    n_xmit = 0;

    auto cr2 = calc_cr2(addr, is_read,
        nbytes + nbytes2, n_xmit, restart_after_write,
        (unsigned int *)&n_tc_end);
    inst->CR2 = cr2;
    inst->CR2 = cr2 | I2C_CR2_START;

    unsigned int wait_flag = is_read ? I2C_ISR_RXNE : I2C_ISR_TXIS;
    // TODO: Block() with deferred irq handling (disable GIC in handler, re-enable here)
    unsigned int err = WaitTimeout(wait_flag);
    if(err != wait_flag)
    {
        return -1;
    }
    else if(is_read)
    {
#if DEBUG_I2C
        klog(msg_i2c_ack_for_read);
#endif

        bool cont = true;
        while(cont)
        {
            err = WaitTimeout(wait_flag);
            if(err != wait_flag)
            {
                return -1;
            }
            auto d = inst->RXDR;
            *cur_buf_p(buf, nbytes, buf2, nbytes2, n_xmit) = d;
            n_xmit = n_xmit + 1;
            if(n_xmit == n_tc_end)
            {
                err = WaitTimeout(I2C_ISR_TC | I2C_ISR_TCR | I2C_ISR_STOPF);
                if((restart_after_write && (err & I2C_ISR_TC)) ||
                    (err & I2C_ISR_STOPF))
                {
                    // finished
                    cont = false;
                    //if(cur_i2c_msg.ss)
                    //{
                    //    cur_i2c_msg.ss->Signal(SimpleSignal::Set, n_xmit);
                    //}
#if DEBUG_I2C
                    klog(msg_i2c_read_success);
#endif
                    return n_xmit;
                }
                else if(err & I2C_ISR_TCR)
                {
                    // reload
                    inst->CR2 = calc_cr2(addr, is_read,
                        nbytes + nbytes2, n_xmit, restart_after_write,
                        (unsigned int *)&n_tc_end);
                }
                else
                {
                    {
                        klog(msg_i2c_read_early_finish);
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
#if DEBUG_I2C
        klog(msg_i2c_ack_for_write);
#endif
        bool cont = true;
        while(cont)
        {
            err = WaitTimeout(wait_flag);
            if(err != wait_flag)
            {
                return -1;
            }
            auto d = *cur_buf_p(buf, nbytes, buf2, nbytes2, n_xmit);
            n_xmit = n_xmit + 1;
            inst->TXDR = d;
            if(n_xmit == n_tc_end)
            {
                err = WaitTimeout(I2C_ISR_TC | I2C_ISR_TCR | I2C_ISR_STOPF);
                if(err & I2C_ISR_NACKF)
                {
                    //CriticalGuard cg(s_rtt);
                    //kklog("i2c: NACKF during write phase\n");
                    klog(msg_i2c_nackf);
                }
                if((restart_after_write && (err & I2C_ISR_TC)) ||
                    (err & I2C_ISR_STOPF))
                {
                    // finished
                    cont = false;
                    //if(cur_i2c_msg.ss)
                    //{
                    //    cur_i2c_msg.ss->Signal(SimpleSignal::Set, n_xmit);
                    //}
#if DEBUG_I2C
                    klog(msg_i2c_write_success);
#endif
                    return n_xmit;
                }
                else if(err & I2C_ISR_TCR)
                {
                    // reload
                    inst->CR2 = calc_cr2(addr, is_read,
                        nbytes + nbytes2, n_xmit, restart_after_write,
                        (unsigned int *)&n_tc_end);
                }
                else
                {
                    {
                        //CriticalGuard cg(s_rtt);
                        //kklog("i2c: early write finish %u, %x\n", n_xmit, inst->ISR);
                        klog(msg_i2c_write_early_finish);
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
