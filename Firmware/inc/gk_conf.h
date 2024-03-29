#ifndef GK_CONF_H
#define GK_CONF_H

#define GK_ENABLE_NETWORK           0
#define GK_ENABLE_USB               1
#define GK_ENABLE_USB_MASS_STORAGE  1
#define GK_ENABLE_LWEXT4_WRITE      0
#define GK_ENABLE_TEST_THREADS      0
#define GK_USE_IRQ_PRIORITIES       0
#define GK_DUAL_CORE                0
#define GK_USE_CACHE                1
#define GK_USE_MPU                  0
#define GK_EXT_READONLY             0
#define GK_EXT_USE_JOURNAL          0
#define GK_SD_USE_HS_MODE           1

#define GK_SDRAM_BASE               0x60000000

#define SRAM4_DATA __attribute__((section(".sram4")))

#endif
