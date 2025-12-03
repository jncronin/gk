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
            return usb_core_transmit_ep0(pdev, msc_buffer, 1);

        case 255:
            // Reset
            return USBD_OK;

        default:
            return USBD_FAIL;
    }
}

uint8_t usb_msc_init(usb_handle *pdev, uint8_t cfgidx)
{
    (void)cfgidx;       // enable in all configurations

    usb_core_configure_ep(pdev, EPNUM_MSC_IN, USBD_EP_TYPE_BULK, 512);
    usb_core_configure_ep(pdev, EPNUM_MSC_OUT, USBD_EP_TYPE_BULK, 512);

    usb_core_receive(pdev, EPNUM_MSC_OUT, msc_buffer, 31);  // receive first cbw

    return USBD_OK;
}
