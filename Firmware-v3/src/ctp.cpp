#include "i2c.h"
#include "thread.h"
#include "scheduler.h"
#include "process.h"
#include "pins.h"
#include "ctp.h"
#include "osmutex.h"
#include "gk_conf.h"

static const constexpr pin CTP_INT { GPIOF, 2 };
static const constexpr pin CTP_NRESET { GPIOC, 0 };
const uint8_t ctp_addr = 0x5d;

[[maybe_unused]] static void *ctp_thread(void *);

static BinarySemaphore ctp_sem;

void init_ctp()
{
#if GK_ENABLE_TOUCH
    Schedule(Thread::Create("ctp", ctp_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc,
        CPUAffinity::PreferM4));
#endif
}

static void ctp_reset()
{
    NVIC_DisableIRQ(EXTI2_IRQn);
    EXTI->PR1 = EXTI_PR1_PR2;
    NVIC_ClearPendingIRQ(EXTI2_IRQn);

    // ctp address is either 0x5d or 0x14 dependent upon CTP_INT value during reset
    //  we set to 0x5d here
    CTP_INT.set_as_input();
    CTP_INT.clear();

    CTP_NRESET.clear();
    CTP_NRESET.set_as_output();

    CTP_INT.set_as_output();

    Block(clock_cur() + kernel_time::from_ms(1U));
    CTP_NRESET.set();

    Block(clock_cur() + kernel_time::from_ms(55U));

    CTP_INT.set_as_input();
}

static bool ctp_init()
{
    ctp_reset();

    // Get product ID
    uint32_t pid;
    auto nr = i2c_register_read(ctp_addr, (uint16_t)0x4081, &pid, 4);

    if(nr == 0 && pid == 0x313139U)
    {
        klog("ctp: detected ctp controller, pid: %x\n", pid);
    }
    else
    {
        klog("ctp: ctp controller not detected: %d\n", nr);
        return false;
    }

    // reset again and configure
    ctp_reset();

    // get current config version
    uint8_t ctp_config_expect[] = { 0x00, 0x80, 0x02, 0xe0, 0x01, 0x05, 0x8c };
    uint8_t ctp_config_cur[sizeof(ctp_config_expect)];
    nr = i2c_register_read(ctp_addr, (uint16_t)0x4780, ctp_config_cur, sizeof(ctp_config_cur));
    if(nr != 0)
    {
        klog("ctp: failed to read config\n");
        return false;
    }

    if(memcmp(&ctp_config_cur[1], &ctp_config_expect[1], sizeof(ctp_config_cur) - 1))
    {
        // we need to reconfigure

        if(ctp_config_cur[0] < 0x82)
            ctp_config_cur[0] = 0x82;
        else
            ctp_config_cur[0]++;
        
        uint8_t ctp_conf[] = {
            ctp_config_cur[0], 0x80, 0x02, 0xe0, 0x01, 0x05, 0x8c, 0x20, 0x01, 0x08, 0x28, 0x05, 0x50, // 0x8047 - 0x8053
            0x3C, 0x0F, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x8054 - 0x8060
            0x00, 0x89, 0x2A, 0x0B, 0x2D, 0x2B, 0x0F, 0x0A, 0x00, 0x00, 0x01, 0xA9, 0x03, // 0x8061 - 0x806D
            0x2D, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, // 0x806E - 0x807A
            0x59, 0x94, 0xC5, 0x02, 0x07, 0x00, 0x00, 0x04, 0x93, 0x24, 0x00, 0x7D, 0x2C, // 0x807B - 0x8087
            0x00, 0x6B, 0x36, 0x00, 0x5D, 0x42, 0x00, 0x53, 0x50, 0x00, 0x53, 0x00, 0x00, // 0x8088	- 0x8094
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x8095 - 0x80A1
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x80A2 - 0x80AD
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, // 0x80AE - 0x80BA
            0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, // 0x80BB - 0x80C7
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x80C8 - 0x80D4
            0x02, 0x04, 0x06, 0x08, 0x0A, 0x0F, 0x10, 0x12, 0x16, 0x18, 0x1C, 0x1D, 0x1E, // 0x80D5 - 0x80E1
            0x1F, 0x20, 0x21, 0x22, 0x24, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, // 0x80E2 - 0x80EE
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x80EF - 0x80FB
            0x00, 0x00, 0xD6, 0x01
        };

        memcpy(&ctp_conf[1], &ctp_config_expect[1], sizeof(ctp_config_expect) - 1);

        ctp_conf[184] = 0;
        for(uint8_t i = 0 ; i < 184 ; i++){
            ctp_conf[184] += ctp_conf[i];
        }
        ctp_conf[184] = (~ctp_conf[184]) + 1;

        nr = i2c_register_write(ctp_addr, (uint16_t)0x4780, ctp_conf, sizeof(ctp_conf));

        if(nr != 0)
        {
            klog("ctp: failed to send config\n");
            return false;
        }

        // read back config
        memset(ctp_conf, 0, sizeof(ctp_conf));
        nr = i2c_register_read(ctp_addr, (uint16_t)0x4780, ctp_conf, sizeof(ctp_conf));
        klog("ctp: saved config %u, sw1: %x\n", ctp_conf[0], ctp_conf[6]);
    }

    uint8_t command = 0;
    nr = i2c_register_write(ctp_addr, (uint16_t)0x4080, &command, 1);

    if(nr != 0)
    {
        klog("ctp: failed to send command\n");
        return false;
    }

    klog("ctp: init successful\n");

    RCC->APB4ENR |= RCC_APB4ENR_SBSEN;
    (void)RCC->APB4ENR;
    SBS->EXTICR[0] = (SBS->EXTICR[0] & ~SBS_EXTICR1_PC_EXTI2_Msk) |
        (5U << SBS_EXTICR1_PC_EXTI2_Pos);

    EXTI->FTSR1 &= ~EXTI_FTSR1_FT2;
    EXTI->RTSR1 |= EXTI_RTSR1_RT2;
    EXTI->IMR1 |= EXTI_IMR1_IM2;

    NVIC_EnableIRQ(EXTI2_IRQn);

    return true;
}

static bool ctp_read()
{
    uint8_t data[0x36];
    i2c_register_read(ctp_addr, (uint16_t)0x4081, data, 0x36);

    if(data[0xe] & 0x80U)
    {
        // found data

        // TODO: process data
        auto ntouch = data[0xe] & 0xfU;
        for(unsigned int i = 0; i < ntouch; i++)
        {
            struct pt { uint16_t x; uint16_t y; };
            pt *curpt = (pt *)&data[0x10 + 0x08 * i];
            klog("ctp: (%u), %u,%u\n",  i,curpt->x, curpt->y);
        }

        // acknowledge
        uint8_t dack = 0;
        i2c_register_write(ctp_addr, (uint16_t)0x4e81, &dack, 1);
    }
    return true;
} 

void *ctp_thread(void *_p)
{
    bool is_init = false;

    while(true)
    {
        if(!is_init)
        {
            is_init = ctp_init();
        }
        else
        {
            ctp_sem.Wait();
            is_init = ctp_read();
        }
    }
}

extern "C" void EXTI2_IRQHandler()
{
    EXTI->PR1 = EXTI_PR1_PR2;
    ctp_sem.Signal();
}
