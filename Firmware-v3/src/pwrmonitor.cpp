#include "i2c.h"
#include "logger.h"
#include "scheduler.h"
#include "pwr.h"

void *pwr_monitor_thread(void *p)
{
    const unsigned int dev_id = 0x78U >> 1;

    // Set bucks to burst mode
    uint8_t buck12_setup = 0x24U;            // 1.125 MHz switching frequency for buck 1/2
    i2c_register_write(dev_id, (uint8_t)0x03, &buck12_setup, 1);
    i2c_register_write(dev_id, (uint8_t)0x04, &buck12_setup, 1);

    uint8_t buck34_setup = 0x2cU;            // 1.125 MHz switching frequency for buck 3/4, 50% out of phase with 1/2
    i2c_register_write(dev_id, (uint8_t)0x01, &buck34_setup, 1);
    i2c_register_write(dev_id, (uint8_t)0x02, &buck34_setup, 1);

    uint8_t cntrl = 0x1fU;                // undervoltage warning at 3.4 V, overtemperature at 110C
    i2c_register_write(dev_id, (uint8_t)0x09, &cntrl, 1);

    while(true)
    {
        // poll status register
        uint8_t irqstat, pgstat;
        auto ret1 = i2c_register_read(dev_id, (uint8_t)0x15, &irqstat, 1);
        auto ret2 = i2c_register_read(dev_id, (uint8_t)0x17, &pgstat, 1);

        uint8_t buck1;
        i2c_register_read(dev_id, (uint8_t)0x04, &buck1, 1);

        if(ret1 >= 0 && ret2 >= 0)
        {
            klog("pwr: IRQSTAT: %08x, PGSTATRT: %08x\n", (uint32_t)irqstat, (uint32_t)pgstat);
        }

        Block(clock_cur() + kernel_time::from_ms(1000U));
    }
}
