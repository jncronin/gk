#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS               OPT_OS_CUSTOM
#endif

#define CFG_TUSB_DEBUG  1
#define CFG_TUSB_MCU  OPT_MCU_STM32H7

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_DEVICE_RHPORT_NUM
  #define BOARD_DEVICE_RHPORT_NUM     0
#endif

// RHPort max operational speed can defined by board.mk
// Default to Highspeed for MCU with internal HighSpeed PHY (can be port specific), otherwise FullSpeed
#ifndef BOARD_DEVICE_RHPORT_SPEED
  #if (CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX || \
       CFG_TUSB_MCU == OPT_MCU_NUC505  || CFG_TUSB_MCU == OPT_MCU_CXD56 || CFG_TUSB_MCU == OPT_MCU_SAMX7X)
    #define BOARD_DEVICE_RHPORT_SPEED   OPT_MODE_HIGH_SPEED
  #else
    #define BOARD_DEVICE_RHPORT_SPEED   OPT_MODE_FULL_SPEED
  #endif
#endif

// Device mode with rhport and speed defined by board.mk
#if   BOARD_DEVICE_RHPORT_NUM == 0
  #define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)
#elif BOARD_DEVICE_RHPORT_NUM == 1
  #define CFG_TUSB_RHPORT1_MODE     (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)
#else
  #error "Incorrect RHPort configuration"
#endif

#define CFG_TUD_ENDPOINT0_SIZE  64

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

#define CFG_TUD_HID             0
#define CFG_TUD_CDC             1
#define CFG_TUD_MSC             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0
#define CFG_TUD_AUDIO           0

#define CFG_TUD_CDC_RX_BUFSIZE  512
#define CFG_TUD_CDC_TX_BUFSIZE  512

#define CFG_TUD_MSC_EP_BUFSIZE  512

#define CFG_TUSB_DEBUG_PRINTF   rtt_printf_wrapper

#ifdef __cplusplus
 }
#endif

#endif