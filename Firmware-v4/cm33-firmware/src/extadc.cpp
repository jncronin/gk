#include "adc.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "logger.h"
#include "clocks.h"

static bool has_extadc = false;

static const constexpr uint8_t extadc_addr = 0x17;
extern SemaphoreHandle_t m_i2c1;

extern uint16_t extadc_vals[6];

/* External ADC is ADS7138 with I2C address 0x17
    All register commands are 2 byte:
        MSB is opcode
            0x10 - single register read
            0x08 - single register write
            0x18 - set bit
            0x20 - clear bit
            0x30 - read contiguous block of registers
            0x28 - write contiguous block of registers
        LSB is starting register
*/

int extadc_calib();
int extadc_set_register(uint8_t reg_id, uint8_t reg_val, uint8_t set_msb = 0x18u);

void init_extadc()
{
    klog("extadc: check presence\n");

    auto &i2c1 = i2c(1);

    for(auto test_addr = 0x10u; test_addr < 0x18u; test_addr++)
    {
        xSemaphoreTake(m_i2c1, portMAX_DELAY);
        uint8_t sr;
        auto i2cret = i2c1.RegisterRead(test_addr, (uint16_t)0x1000u, &sr, 1);
        xSemaphoreGive(m_i2c1);

        if(i2cret == 1)
        {
            klog("extadc: found chip on address %02x with SR: %02x\n", test_addr, sr);
        }
    }

    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    uint8_t sr;
    auto i2cret = i2c1.RegisterRead(extadc_addr, (uint16_t)0x1000u, &sr, 1);
    xSemaphoreGive(m_i2c1);

    if(i2cret != 1)
    {
        klog("extadc: not found\n");
        has_extadc = false;
        return;
    }

    klog("extadc: found chip with SR: %02x\n", sr);

    /* Perform reset followed by offset calibration */
    uint8_t rst_bit = 0x1;
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    i2cret = i2c1.RegisterWrite(extadc_addr, (uint16_t)0x1801u, &rst_bit, 1);
    xSemaphoreGive(m_i2c1);
    if(i2cret != 1)
    {
        klog("extadc: set reset bit failed\n");
        has_extadc = false;
        return;
    }

    auto now = clock_cur_ms();
    while(true)
    {
        uint8_t cfgr = 0;
        xSemaphoreTake(m_i2c1, portMAX_DELAY);
        i2cret = i2c1.RegisterRead(extadc_addr, (uint16_t)0x1001u, &cfgr, 1);
        xSemaphoreGive(m_i2c1);
        if(i2cret != 1)
        {
            klog("extadc: read cfgr failed\n");
            has_extadc = false;
            return;
        }

        if(!(cfgr & 0x1))
        {
            break;
        }

        if(clock_cur_ms() >= (now + 10))
        {
            // timeout
            klog("extadc: did not reset\n");
            has_extadc = false;
            return;
        }
    }
    klog("extadc: reset complete\n");

    if(extadc_calib() != 0)
        return;

    /* Initialize it
    
        We want continuous (autonomous) sampling of 6 channels with total sample time of 5 ms

        The device samples (charges internal capacitor with switch open), followed by
        conversion (reads internal capacitor value with switch closed).  tCONV is 0.6 us.
        tSMPL is variable depending on sampling frequency.
        
        The device starts a conversion on each clock tick, therefore a sampling frequency of
        1.3 kSPS is adequate (just under 5 ms for a whole cycle).

        Each conversion can be repeated using oversampling, but this is not affected by sampling
        frequency (see Fig 24).  Converison time is 0.6 us so many repeated samples can be
        performed.

        After conversion, the device switches to the next channel and begins sampling again.
        Therefore, for our setup we have:
        tCYCLE = tSMPL + tCONV * n_OSR
        tSMPL = tCYCLE / (tCONV * n_OSR)

        tCYCLE with a 1.3 kSPS clock is 768 us
        tSMPL is therefore always > 750 us which should be more than adequate to charge the
        internal capacitor.
    */

    // This follows the flowchart in 8.4.4
    // PIN_CFG is already zero (reset value) - all pins are analog in
    // Set channels in the sequence in AUTO_SEQ_CH_SEL
    if(extadc_set_register(0x12, 0x3fu) != 0)
        return;

    // Set SEQ_MODE to auto
    if(extadc_set_register(0x10, 0x1u) != 0)
        return;

    // Configure sampling rate, autonomous mode
    if(extadc_set_register(0x4, 0x39u) != 0)
        return;
    
    // Configure 16x oversampling to get a 16-bit result
    if(extadc_set_register(0x3, 0x4u) != 0)
        return;

    // Enable stats module so we can read the RECENT_ registers
    if(extadc_set_register(0x1, 0x20u, 0x18u) != 0)
        return;

    // Begin comparisons
    if(extadc_set_register(0x10, 0x10u, 0x18u) != 0)
        return;

    klog("extadc: init complete\n");   
}

int extadc_calib()
{
    auto &i2c1 = i2c(1);

    klog("extadc: begin calibration\n");
    uint8_t cal_bit = 0x2u;
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    auto i2cret = i2c1.RegisterWrite(extadc_addr, (uint16_t)0x1801u, &cal_bit, 1);
    xSemaphoreGive(m_i2c1);
    if(i2cret != 1)
    {
        klog("extadc: set cal bit failed\n");
        has_extadc = false;
        return -1;
    }

    auto now = clock_cur_ms();
    while(true)
    {
        uint8_t cfgr = 0;
        xSemaphoreTake(m_i2c1, portMAX_DELAY);
        i2cret = i2c1.RegisterRead(extadc_addr, (uint16_t)0x1001u, &cfgr, 1);
        xSemaphoreGive(m_i2c1);
        if(i2cret != 1)
        {
            klog("extadc: read cfgr failed\n");
            has_extadc = false;
            return -1;
        }

        if(!(cfgr & 0x2))
        {
            break;
        }

        if(clock_cur_ms() >= (now + 50))
        {
            // timeout
            klog("extadc: did not calibrate\n");
            has_extadc = false;
            return -1;
        }
    }
    klog("extadc: calibration complete\n");
    return 0;
}

int extadc_set_register(uint8_t reg_id, uint8_t reg_val, uint8_t set_msb)
{
    auto &i2c1 = i2c(1);

    uint16_t reg_16 = (uint16_t)reg_id | (((uint16_t)set_msb) << 8);
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    auto i2cret = i2c1.RegisterWrite(extadc_addr, (uint16_t)reg_16, &reg_val, 1);
    xSemaphoreGive(m_i2c1);
    if(i2cret != 1)
    {
        klog("extadc: set register %02x failed\n", reg_id);
        has_extadc = false;
        return -1;
    }
    return 0;
}

int extadc_poll()
{
    if(!has_extadc)
        return -1;

    auto &i2c1 = i2c(1);

    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    auto i2cret = i2c1.RegisterRead(extadc_addr, (uint16_t)0x30a0, extadc_vals, sizeof(extadc_vals));
    xSemaphoreGive(m_i2c1);
    if(i2cret != 1)
    {
        klog("extadc: read recent registers failed\n");
        has_extadc = false;
        return -1;
    }

    return 0;
}
