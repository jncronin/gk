#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include "gk_conf.h"

enum
{
#if GK_ENABLE_NETWORK
    ITF_NUM_ETH = 0,
    ITH_NUM_ETH_DATA,
    ITF_NUM_CDC,
#elif GK_ENABLE_USB_CDC
    ITF_NUM_CDC = 0,
#endif
#if GK_ENABLE_USB_CDC
    ITF_NUM_CDC_DATA,
#endif
#if GK_LOG_USB
    ITF_NUM_LOG_CDC,
    ITF_NUM_LOG_CDC_DATA,
#endif
#if GK_ENABLE_USB_MASS_STORAGE
    ITF_NUM_MSC,
#endif
    ITF_NUM_TOTAL
};

#define USBD_MAX_NUM_INTERFACES ITF_NUM_TOTAL

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

#define EPNUM_MSC_OUT     0x03
#define EPNUM_MSC_IN      0x83

#define EPNUM_ETH_NOTIF   0x84
#define EPNUM_ETH_OUT     0x05
#define EPNUM_ETH_IN      0x85

#define EPNUM_CDC_LOG_NOTIF 0x86
#define EPNUM_CDC_LOG_OUT   0x06
#define EPNUM_CDC_LOG_IN    0x87


#endif
