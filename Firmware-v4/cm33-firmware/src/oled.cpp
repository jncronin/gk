#include "oled.h"
#include "i2c.h"
#include "logger.h"

static const char ch1115_init[] = 
{
    0xae,
    0xd5, 0x80,
    0xa8, 0x3f,
    0xd3, 0x00,
    0x40,
    0xad, 0x8b,
    0xa1,
    0xc8,
    0xda, 0x12,
    0x81, 0x7f,
    0xd9, 0x22,
    0xdb, 0x20,
    0xa4,
    0xa6,
    0xaf
};

void init_oled()
{
    // Set OLED_NRES as output
    auto &i2c1 = i2c(1);
    char i2cmsg = 0x7f;
    auto i2cret = i2c1.RegisterWrite(0x20, (uint8_t)0x07, &i2cmsg, 1);
    klog("A: %d\n", i2cret);

    // Set it high
    i2cmsg = 0x80;
    i2cret = i2c1.RegisterWrite(0x20, (uint8_t)0x03, &i2cmsg, 1);
    klog("B: %d\n", i2cret);

    // Enable TCA9546A ch1 - it doesn't have internal registers
    i2cmsg = 0x1;
    i2cret = i2c1.Write(0x70, &i2cmsg, 1);
    klog("C: %d\n", i2cret);

    // Init OLED CH1115
    i2cret = i2c1.Write(0x3c, ch1115_init, sizeof(ch1115_init));
    klog("D: %d\n", i2cret);
}