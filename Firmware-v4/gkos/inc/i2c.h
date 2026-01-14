#ifndef I2C_H
#define I2C_H

#include <cstdlib>
#include <cstdint>
#include <stm32mp2xx.h>
#include "osspinlock.h"
#include "pins.h"
#include "osmutex.h"

void init_i2c();

class I2C
{
    protected:
        I2C_TypeDef *inst = nullptr;
        pin SDA, SCL;
        volatile uint32_t *rcc_reg;
        Mutex m{};
        int Transmit(unsigned int addr, void *buf, size_t nbytes, 
            void *buf2, size_t nbytes2,
            bool is_read, bool restart_after_write);
        unsigned int WaitTimeout(unsigned int wait_flag, unsigned int timeout_ms = 5);
        int Init();

        bool init = false;
        volatile unsigned int n_xmit = 0;
        volatile unsigned int n_tc_end = 0;


    public:
        int Read(unsigned int addr, void *buf, size_t nbytes);
        int Write(unsigned int addr, const void *buf, size_t nbytes);
        int RegisterRead(unsigned int addr, uint8_t reg, void *buf, size_t nbytes, bool wait = true);
        int RegisterWrite(unsigned int addr, uint8_t reg, const void *buf, size_t nbytes, bool wait = true);
        int RegisterRead(unsigned int addr, uint16_t reg, void *buf, size_t nbytes, bool wait = true);
        int RegisterWrite(unsigned int addr, uint16_t reg, const void *buf, size_t nbytes, bool wait = true);

        friend void init_i2c();
};

I2C &i2c(unsigned int instance);

#endif
