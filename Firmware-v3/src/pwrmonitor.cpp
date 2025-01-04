#include "i2c.h"
#include "logger.h"
#include "scheduler.h"
#include "pwr.h"
#include "pins.h"

constexpr const unsigned int addr = 0x36;

static bool init_max();

static constexpr const pin VBAT_STAT = { GPIOC, 5 };

static void vbat_state_machine();
static void read_max();

enum VbatStatState { Unknown, NoBattery, Charging, ChargeComplete };
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

    while(true)
    {
        if(!is_init)
        {
            is_init = init_max();
        }

        vbat_state_machine();

        if(!last_max_read.is_valid() || clock_cur() > (last_max_read + kernel_time::from_ms(1000)))
        {
            read_max();
            last_max_read = clock_cur();

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
            }

            if(ret2 >= 0)
            {
                if((pgstat & 0xfU) == 0xfU)
                    stat.vreg_pgood = true;
                else
                    stat.vreg_pgood = false;
            }
        }

        stat.vdd = pwr_get_vdd();

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

void vbat_state_machine()
{
    /* VBAT_STAT is tristate output, we therefore check its value
        with both pullups and pulldowns active */
    static int state_id = 0;
    static bool pup_val = false;
    static bool pdown_val = false;

    /* Run a state machine that does:
        0: set pullup
        1: sample
        2: set pulldown
        3: sample, interpret */
    switch(state_id)
    {
        case 0:
            VBAT_STAT.set_as_input(pin::PullUp);
            state_id = 1;
            break;
        case 1:
            pup_val = VBAT_STAT.value();
            state_id = 2;
            break;
        case 2:
            VBAT_STAT.set_as_input(pin::PullDown);
            state_id = 3;
            break;
        case 3:
            pdown_val = VBAT_STAT.value();

            if(pup_val && !pdown_val)
            {
                vbat = VbatStatState::NoBattery;
            }
            else if(pup_val && pdown_val)
            {
                vbat = VbatStatState::ChargeComplete;
            }
            else if(!pup_val && !pdown_val)
            {
                vbat = VbatStatState::Charging;
            }
            else
            {
                vbat = VbatStatState::Unknown;
            }
            state_id = 0;
            break;
    }
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

    bool is_charging = /* !VBAT_STAT.value(); */
        i_crate > 0;

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
    }
}

void pwr_get_status(pwr_status *_stat)
{
    if(_stat)
        *_stat = stat;
}
