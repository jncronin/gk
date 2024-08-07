#ifndef GK_CONF_H
#define GK_CONF_H

#include "logger.h"

#define GK_ENABLE_NETWORK           0
#define GK_ENABLE_USB               1
#define GK_ENABLE_USB_MASS_STORAGE  1
#define GK_ENABLE_LWEXT4_WRITE      0
#define GK_ENABLE_TEST_THREADS      0
#define GK_USE_IRQ_PRIORITIES       0
#define GK_DUAL_CORE                0
#define GK_USE_CACHE                1
#define GK_USE_MPU                  1
#define GK_EXT_READONLY             0
#define GK_EXT_USE_JOURNAL          0
#define GK_SD_USE_HS_MODE           1
#define GK_GPU_SHOW_FPS             1
#define GK_DUAL_CORE_AMP            0
#define GK_OVERCLOCK                1
#define GK_TICKLESS                 0
#define GK_MEMBLK_STATS             1

#define GK_LOG_PERSISTENT           1
#define GK_LOG_RTT                  1
#define GK_LOG_USB                  1
#define GK_LOG_FILE                 1

#define GK_LOG_SIZE                 (24*1024)

#define GK_ENABLE_CTP340            0

#define GK_SDRAM_BASE               0x60000000

#define GK_NUM_EVENTS_PER_PROCESS   32
#define GK_PRIORITY_IDLE    0
#define GK_PRIORITY_LOW     1
#define GK_PRIORITY_NORMAL  2
#define GK_PRIORITY_GAME    GK_PRIORITY_NORMAL
#define GK_PRIORITY_APP     GK_PRIORITY_NORMAL
#define GK_PRIORITY_HIGH    3
#define GK_PRIORITY_VHIGH   4
#define GK_PRIORITY_VERYHIGH    GK_PRIORITY_VHIGH

#define GK_NPRIORITIES      (GK_PRIORITY_VHIGH + 1)

#define GK_MAX_WINDOW_TITLE 32


#define SRAM4_DATA __attribute__((section(".sram4")))
#define RTCREG_DATA __attribute__((section(".rtcregs")))

// TODO: check presence of debugger and if not show message on screen
#ifdef BKPT
#undef BKPT
#endif
#define BKPT() \
    __asm__ volatile("bkpt \n" ::: "memory")

#define DEBUG_FULLQUEUE     0

#endif
