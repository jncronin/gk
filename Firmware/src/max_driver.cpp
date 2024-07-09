#include "i2c.h"
#include "thread.h"
#include "scheduler.h"
#include "osmutex.h"
#include "pins.h"
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

constexpr const unsigned int addr = 0x36;

static bool init_max();

static constexpr const pin VBAT_STAT = { GPIOE, 3 };

void *max_thread(void *param)
{
    bool is_init = false;

    VBAT_STAT.set_as_input();

    while(true)
    {
        if(!is_init)
        {
            is_init = init_max();
        }
        else
        {
            unsigned int vcell = 0, soc = 0, crate = 0;
            i2c_register_read(addr, (uint8_t)0x02, &vcell, 2);
            i2c_register_read(addr, (uint8_t)0x04, &soc, 2);
            i2c_register_read(addr, (uint8_t)0x16, &crate, 2);

            vcell = __bswap16(vcell);
            soc = __bswap16(soc);
            crate = __bswap16(crate);

            float f_vcell = (float)vcell * 78.125f / 1000000.0f;
            float f_soc = (float)soc / 256.0f;
            float f_crate = (float)crate * 0.208f;

            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "max: vcell: %sV, soc: %s%%, crate: %s%%/hr, charging: %s\n",
                    std::to_string(f_vcell).c_str(),
                    std::to_string(f_soc).c_str(),
                    std::to_string(f_crate).c_str(),
                    VBAT_STAT.value() ? "no" : "yes");
            }
        }
        Block(clock_cur() + kernel_time::from_ms(1000));
    }
}

bool init_max()
{
    unsigned int ver = 0;

    i2c_register_read(addr, (uint8_t)0x08, &ver, 2);

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "max: version %x\n", ver);
    }    

    return true;
}
