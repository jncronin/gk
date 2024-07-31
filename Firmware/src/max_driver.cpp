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
    uint64_t last_dump = 0;

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
            int i_crate = (int)(int16_t)crate;

            float f_vcell = (float)vcell * 78.125f / 1000000.0f;
            float f_soc = (float)soc / 256.0f;
            float f_crate = (float)i_crate * 0.208f;

            bool is_charging = /* !VBAT_STAT.value(); */
                i_crate > 0;

            auto pct_to_go = is_charging ? (100.0f - f_soc) : f_soc;
            auto mins_left = pct_to_go * 60.0f / std::abs(f_crate);

            if(!last_dump || clock_cur_ms() >= (last_dump + 60000))
            {
                klog("max: vcell: %sV, soc: %s%%, crate: %s%%/hr, charging: %s, time remaining: %s mins\n",
                    std::to_string(f_vcell).c_str(),
                    std::to_string(f_soc).c_str(),
                    std::to_string(f_crate).c_str(),
                    is_charging ? "yes" : "no",
                    std::to_string(mins_left).c_str());
                last_dump = clock_cur_ms();
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
        klog("max: version %x\n", ver);
    }    

    return true;
}
