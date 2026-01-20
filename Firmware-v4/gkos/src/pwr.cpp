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

        ina_id[0] = __builtin_bswap16(ina_id[0]);
        ina_id[1] = __builtin_bswap16(ina_id[1]);
        vbus = __builtin_bswap16(vbus);
        vshunt = __builtin_bswap16(vshunt);


        /* For bus voltage, 1 LSB = 1.6 mV
            For shunt, depends on ADCRANGE
                ADCRANGE = 0, 1 LSB = 2.5 uV
                ADCRANGE = 1, 1 LSB = 625 nV
            We have 10 mOhm as shunt
            */

        int vbus_uv = (int)vbus * 1600;
        [[maybe_unused]] int vbus_v = vbus_uv / 1000000;
        [[maybe_unused]] int vbus_fract = vbus_uv % 1000000;

        // ultimately want microamps here
        //  uI = nV * 1000 / uR
        int64_t vshunt_nv = (int64_t)vshunt * 2500;
        int64_t ishunt_ua = (vshunt_nv * 1000) / 10000;

        // P = V * I => uP = uV * uI * 10^-6
        int64_t pshunt_uw = (int64_t)vbus_uv * ishunt_ua / 1000000;
        [[maybe_unused]] int pshunt_w = (int)(pshunt_uw / 1000000);
        [[maybe_unused]] int pshunt_fract = (int)(pshunt_uw % 1000000);

#if GK_ENABLE_PWR_DUMP
        klog("pwr: id: %x, %x, vbus: %u, vshunt: %d\n", ina_id[0], ina_id[1], vbus, vshunt);
        klog("pwr: VBUS: %d.%06d V\n", vbus_v, vbus_fract);
        klog("pwr: ISHUNT: %d uA\n", (int)ishunt_ua);
        klog("pwr: PSHUNT: %d.%06d W\n", pshunt_w, pshunt_fract);
#endif

        Block(clock_cur() + kernel_time_from_ms(1000));
    }
}
