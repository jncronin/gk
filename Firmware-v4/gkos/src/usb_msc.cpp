#include "usb_class.h"
#include "usb_device.h"

#include "cache.h"

__attribute__((aligned(CACHE_LINE_SIZE))) uint8_t msc_buffer[512];

uint8_t usb_msc_setup(usb_handle *pdev, uint8_t bRequest)
{
    switch(bRequest)
    {
        case 254:
            // GetMaxLUN - just return a single LUN
            msc_buffer[0] = 0;
            return usb_core_transmit_ep0(pdev, msc_buffer, 0);

        case 255:
            // Reset
            return USBD_OK;

        default:
            return USBD_FAIL;
    }
}
