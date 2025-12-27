#include "pwr.h"
#include "thread.h"
#include "scheduler.h"
#include "process.h"
#include "i2c.h"

static void *pwr_thread(void *);

void init_pwr()
{
    Schedule(Thread::Create("pwr", pwr_thread, nullptr, true, GK_PRIORITY_VHIGH, p_kernel));
}

void *pwr_thread(void *)
{
    auto &i2c_pwr = i2c(2);

    while(true)
    {
        // sample various power monitors

        // INA236A on 0x40
        const unsigned int ina236a_addr = 0x40;

        // dump id
        uint16_t ina_id[2];
        uint16_t vbus, vshunt;
        i2c_pwr.RegisterRead(ina236a_addr, (uint8_t)0x3e, &ina_id[0], 2);
        i2c_pwr.RegisterRead(ina236a_addr, (uint8_t)0x3f, &ina_id[1], 2);
        i2c_pwr.RegisterRead(ina236a_addr, (uint8_t)0x2, &vbus, 2);
        i2c_pwr.RegisterRead(ina236a_addr, (uint8_t)0x1, &vshunt, 2);

        klog("pwr: id: %x, %x, vbus: %u, vshunt: %u\n", ina_id[0], ina_id[1], vbus, vshunt);

        Block(clock_cur() + kernel_time_from_ms(1000));
    }
}
