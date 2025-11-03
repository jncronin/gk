#ifndef I2C_POLL_H
#define I2C_POLL_H

#include <cstdlib>
#include <cstdint>
#include <cstddef>

int i2c_poll_read(unsigned int addr, const void *buf, size_t nbytes);
int i2c_poll_send(unsigned int addr, void *buf, size_t nbytes);
int i2c_poll_register_read(unsigned int addr, uint8_t reg, void *buf, size_t nbytes);
int i2c_poll_register_write(unsigned int addr, uint8_t reg, const void *buf, size_t nbytes);
int i2c_poll_register_read(unsigned int addr, uint16_t reg, void *buf, size_t nbytes);
int i2c_poll_register_write(unsigned int addr, uint16_t reg, const void *buf, size_t nbytes);
int i2c_poll_xmit(unsigned int addr, void *buf, size_t nbytes, bool is_read);

#endif
