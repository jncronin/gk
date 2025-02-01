#include "i2c.h"
#include "thread.h"
#include "scheduler.h"
#include "osmutex.h"
#include "pins.h"
#include "SEGGER_RTT.h"
#include <string.h>
#include "process.h"

constexpr const unsigned int addr = 0x36;

static constexpr const pin VBAT_STAT = { GPIOC, 5 };

static bool _init_max();

Process p_max;

void *max_thread(void *param)
{
    bool is_init = false;
    uint64_t last_dump = 0;

    VBAT_STAT.set_as_input();

    while(true)
    {
        if(!is_init)
        {
            is_init = _init_max();
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

            bool is_charging = !VBAT_STAT.value();

            auto pct_to_go = is_charging ? (100.0f - f_soc) : f_soc;
            auto mins_left = pct_to_go * 60.0f / std::abs(f_crate);

            if(!last_dump || clock_cur_ms() >= (last_dump + 10000))
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

bool _init_max()
{
    unsigned int ver = 0;

    i2c_register_read(addr, (uint8_t)0x08, &ver, 2);

    {
        CriticalGuard cg;
        klog("max: version %x\n", ver);
    }    

    return true;
}

void init_max()
{
    p_max.stack_preference = STACK_PREFERENCE_SDRAM_RAM_TCM;
    p_max.argc = 0;
    p_max.argv = nullptr;
    p_max.brk = 0;
    p_max.code_data = InvalidMemregion();
    p_max.cwd = "/";
    p_max.default_affinity = PreferM4;
    p_max.for_deletion = false;
    p_max.heap = InvalidMemregion();
    p_max.name = "max";
    p_max.next_key = 0;
    for(int i = 0; i < GK_MAX_OPEN_FILES; i++)
        p_max.open_files[i] = nullptr;
    p_max.screen_h = 480;
    p_max.screen_w = 640;
    memcpy(p_max.p_mpu, mpu_default, sizeof(mpu_default));

    Schedule(Thread::Create("tilt", max_thread, nullptr, true, GK_PRIORITY_HIGH, p_max,
        CPUAffinity::PreferM4));
}
