#ifndef USB_CLASS_H
#define USB_CLASS_H

#include "gk_conf.h"
#include <stdint.h>
#include "usb_device.h"

#if GK_ENABLE_USB_MASS_STORAGE
uint8_t usb_msc_setup(usb_handle *pdev, uint8_t bRequest);
uint8_t usb_msc_init(usb_handle *pdev, uint8_t cfgidx);

#endif


#endif
