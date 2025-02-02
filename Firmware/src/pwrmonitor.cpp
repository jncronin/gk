#include "i2c.h"
#include "logger.h"
#include "scheduler.h"
#include "pwr.h"
#include "pins.h"

constexpr const unsigned int addr = 0x36;

static bool init_max();

static constexpr const pin VBAT_STAT = { GPIOC, 5 };

static void read_max();

enum VbatStatState { Unknown, NoBattery, Charging, ChargeComplete, Discharging };
static VbatStatState vbat;

static pwr_status stat = { 0 };

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

    // MAX17048
    bool is_init = false;
    kernel_time last_max_read;

    // MCP73831
    VBAT_STAT.set_as_input();

    // last dump to rtt time
    kernel_time last_pwr_dump;

    while(true)
    {
        if(!is_init)
        {
            is_init = init_max();
        }

        stat.vdd = pwr_get_vdd();

        if(!last_max_read.is_valid() || clock_cur() > (last_max_read + kernel_time::from_ms(1000)))
        {
            read_max();
            last_max_read = clock_cur();

            /* Determine whether we are charging or not */
            auto vb_stat = VBAT_STAT.value();
            if(vb_stat)
            {
                // not charging - either discharging or fully charged
                //  decide based upon whether or not there is a usb cable connected
                if(USB_OTG_HS->GCCFG & USB_OTG_GCCFG_SESSVLD)
                    vbat = VbatStatState::ChargeComplete;
                else
                    vbat = VbatStatState::Discharging;
            }
            else
            {
                vbat = VbatStatState::Charging;
            }

            // Check LT3380
            // poll status register
            uint8_t irqstat, pgstat;
            auto ret1 = i2c_register_read(dev_id, (uint8_t)0x15, &irqstat, 1);
            auto ret2 = i2c_register_read(dev_id, (uint8_t)0x17, &pgstat, 1);

            uint8_t buck1;
            i2c_register_read(dev_id, (uint8_t)0x04, &buck1, 1);

            if(ret1 >= 0)
            {
                if(irqstat & 0x18U)
                    stat.vreg_undervoltage = true;
                else
                    stat.vreg_undervoltage = false;
                if(irqstat & 0x60U)
                    stat.vreg_overtemperature = true;
                else
                    stat.vreg_overtemperature = false;

                if(stat.vreg_overtemperature || stat.vreg_undervoltage)
                {
                    // clear irq
                    uint8_t clirq = 0;
                    i2c_register_write(dev_id, (uint8_t)0x1f, &clirq, 1);
                }
            }

            if(ret2 >= 0)
            {
                if((pgstat & 0x8eU) == 0x8eU)
                    stat.vreg_pgood = true;
                else
                    stat.vreg_pgood = false;
            }

            if(!last_pwr_dump.is_valid() || clock_cur() > (last_pwr_dump + kernel_time::from_ms(10000)))
            {
                klog("pwr: vdd: %f, power good: %s, undervoltage: %s, overtemperature %s\n",
                    stat.vdd,
                    stat.vreg_pgood ? "true" : "false",
                    stat.vreg_undervoltage ? "true" : "false",
                    stat.vreg_overtemperature ? "true" : "false");
                klog("pwr: battery: %s, vbat: %f, soc: %f%%, charge rate: %f%%/hr, remaining: %f mins\n",
                    stat.is_charging ? "charging" : (stat.is_full ? "charged" : "discharging"),
                    stat.vbat,
                    stat.state_of_charge,
                    stat.charge_rate,
                    stat.time_until_full_empty);

                last_pwr_dump = clock_cur();
            }
        }

        Block(clock_cur() + kernel_time::from_ms(50U));
    }
}

bool init_max()
{
    unsigned int ver = 0;

    i2c_register_read(addr, (uint8_t)0x08, &ver, 2);

    {
        klog("max: version %x\n", ver);
    }    

    return true;
}

void read_max()
{
    unsigned int vcell = 0, soc = 0, crate = 0;
    i2c_register_read(addr, (uint8_t)0x02, &vcell, 2);
    i2c_register_read(addr, (uint8_t)0x04, &soc, 2);
    i2c_register_read(addr, (uint8_t)0x16, &crate, 2);

    vcell = __bswap16(vcell);
    soc = __bswap16(soc);
    crate = __bswap16(crate);
    int i_crate = (int)(int16_t)crate;

    double f_vcell = (double)vcell * 78.125 / 1000000.0;
    double f_soc = (double)soc / 256.0;
    double f_crate = (double)i_crate * 0.208;

    bool is_charging = !VBAT_STAT.value();

    auto pct_to_go = is_charging ? (100.0 - f_soc) : f_soc;
    auto mins_left = pct_to_go * 60.0 / std::abs(f_crate);

    stat.charge_rate = f_crate;
    stat.state_of_charge = f_soc;
    stat.time_until_full_empty = mins_left;
    stat.vbat = f_vcell;

    switch(vbat)
    {
        case VbatStatState::Unknown:
            stat.battery_present = false;
            stat.is_charging = false;
            stat.is_full = false;
            break;
        case VbatStatState::Charging:
            stat.battery_present = true;
            stat.is_charging = true;
            stat.is_full = false;
            break;
        case VbatStatState::ChargeComplete:
            stat.battery_present = true;
            stat.is_charging = true;
            stat.is_full = true;
            break;
        case VbatStatState::NoBattery:
            stat.battery_present = false;
            stat.is_charging = true;
            stat.is_full = false;
            break;
        case VbatStatState::Discharging:
            stat.battery_present = true;
            stat.is_charging = false;
            stat.is_full = false;
            break;
    }
}

void pwr_get_status(pwr_status *_stat)
{
    if(_stat)
        *_stat = stat;
}
