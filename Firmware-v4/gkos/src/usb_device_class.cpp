#include "usb_device.h"
#include "logger.h"
#include "gk_conf.h"
#include "usb_class.h"

//TODO: initialize usb_device_class similar to plat/st/common/usb_class.c

uint8_t usb_class_init(struct usb_handle *pdev, uint8_t cfgidx)
{
    klog("usb: class_init(..., %u)\n", cfgidx);

#if GK_ENABLE_USB_MASS_STORAGE
    usb_msc_init(pdev, cfgidx);
#endif

    return USBD_OK;
}

uint8_t usb_class_de_init(struct usb_handle *pdev, uint8_t cfgidx)
{
    klog("usb: class_de_init(..., %u)\n", cfgidx);
    return USBD_OK;
}

uint8_t usb_class_data_in(struct usb_handle *pdev, uint8_t epnum)
{
    klog("usb: class_data_in(..., %u)\n", epnum);

    switch(epnum | 0x80)
    {
#if GK_ENABLE_USB_MASS_STORAGE
        case EPNUM_MSC_IN:
            return usb_msc_data_in(pdev, epnum);
#endif

        default:
            return USBD_FAIL;
    }
}

uint8_t usb_class_ep0_rx_ready(struct usb_handle *pdev)
{
    klog("usb: class_ep0_rx_ready(...)\n");
    return USBD_OK;
}

uint8_t usb_class_ep0_tx_ready(struct usb_handle *pdev)
{
    klog("usb: class_ep0_tx_ready(...)\n");
    return USBD_OK;
}

uint8_t usb_class_sof(struct usb_handle *pdev)
{
    klog("usb: class_sof(...)\n");
    return USBD_OK;
}

uint8_t usb_class_iso_in_incomplete(struct usb_handle *pdev, uint8_t epnum)
{
    klog("usb: class_iso_in_incomplete(..., %u)\n", epnum);
    return USBD_OK;
}

uint8_t usb_class_iso_out_incomplete(struct usb_handle *pdev, uint8_t epnum)
{
    klog("usb: class_iso_out_incomplete(..., %u)\n", epnum);
    return USBD_OK;
}

uint8_t usb_class_data_out(struct usb_handle *pdev, uint8_t epnum)
{
    klog("usb: class_data_out(..., %u)\n", epnum);

    switch(epnum)
    {
#if GK_ENABLE_USB_MASS_STORAGE
        case EPNUM_MSC_OUT:
            return usb_msc_data_out(pdev, epnum);
#endif

        default:
            return USBD_FAIL;
    }
}

uint8_t usb_class_setup(struct usb_handle *pdev, struct usb_setup_req *req)
{
    klog("usb: class_setup(..., bm_request=%u, b_request=%u)\n",
        req->bm_request, req->b_request);

    switch (req->bm_request & USB_REQ_TYPE_MASK)
    {
        case USB_REQ_TYPE_CLASS:
            switch (req->b_request)
            {
#if GK_ENABLE_USB_MASS_STORAGE
                case 254:
                case 255:
                    return usb_msc_setup(pdev, req->b_request);
#endif


                default:
                    return USBD_FAIL;
            }
            break;
    }

    return USBD_OK;
}

usb_class usb_class_handlers = {
	.init = usb_class_init,
	.de_init = usb_class_de_init,
	.setup = usb_class_setup,
	.ep0_tx_sent = usb_class_ep0_tx_ready,
	.ep0_rx_ready = usb_class_ep0_rx_ready,
	.data_in = usb_class_data_in,
	.data_out = usb_class_data_out,
	.sof = usb_class_sof,
	.iso_in_incomplete = usb_class_iso_in_incomplete,
	.iso_out_incomplete = usb_class_iso_out_incomplete,
};
