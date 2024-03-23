#ifndef GK_CONF_H
#define GK_CONF_H

#define GK_ENABLE_NETWORK           0
#define GK_ENABLE_USB               1
#define GK_ENABLE_USB_MASS_STORAGE  1
#define GK_ENABLE_LWEXT4_WRITE      0
#define GK_ENABLE_TEST_THREADS      0
#define GK_DUAL_CORE                0
#define GK_USE_CACHE                0

#define SRAM4_DATA __attribute__((section(".sram4")))

#endif
