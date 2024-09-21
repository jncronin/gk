#ifndef I2C_H
#define I2C_H

#include <cstdlib>
#include <cstdint>

void init_i2c();
int i2c_read(unsigned int addr, const void *buf, size_t nbytes);
int i2c_send(unsigned int addr, void *buf, size_t nbytes);
int i2c_register_read(unsigned int addr, uint8_t reg, void *buf, size_t nbytes);
int i2c_register_write(unsigned int addr, uint8_t reg, const void *buf, size_t nbytes);
int i2c_register_read(unsigned int addr, uint16_t reg, void *buf, size_t nbytes);
int i2c_register_write(unsigned int addr, uint16_t reg, const void *buf, size_t nbytes);
int i2c_xmit(unsigned int addr, void *buf, size_t nbytes, bool is_read);

#endif
