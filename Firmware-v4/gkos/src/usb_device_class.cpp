#include "usb_device.h"
#include "logger.h"
#include "gk_conf.h"
#include "usb_class.h"

//TODO: initialize usb_device_class similar to plat/st/common/usb_class.c

uint8_t usb_class_init(struct usb_handle *pdev, uint8_t cfgidx)
{
//    klog("usb: class_init(..., %u)\n", cfgidx);

#if GK_ENABLE_USB_MASS_STORAGE
    usb_msc_init(pdev, cfgidx);
#endif
#if GK_LOG_USB
    usb_cdc_acm_init(pdev, cfgidx, (uint16_t)ITF_NUM_LOG_CDC, EPNUM_CDC_LOG_NOTIF,
        EPNUM_CDC_LOG_IN, EPNUM_CDC_LOG_OUT);
#endif

    return USBD_OK;
}

uint8_t usb_class_de_init(struct usb_handle *pdev, uint8_t cfgidx)
{
//    klog("usb: class_de_init(..., %u)\n", cfgidx);
    return USBD_OK;
}

uint8_t usb_class_data_in(struct usb_handle *pdev, uint8_t epnum)
{
//    klog("usb: class_data_in(..., %u)\n", epnum);

    switch(epnum | 0x80)
    {
#if GK_ENABLE_USB_MASS_STORAGE
        case EPNUM_MSC_IN:
            return usb_msc_data_in(pdev, epnum);
#endif
#if GK_LOG_USB
        case EPNUM_CDC_LOG_IN:
            return usb_cdc_data_in(pdev, epnum);
#endif

        default:
            return USBD_FAIL;
    }
}

uint8_t usb_class_ep0_rx_ready(struct usb_handle *pdev)
{
//    klog("usb: class_ep0_rx_ready(...)\n");
    return USBD_OK;
}

uint8_t usb_class_ep0_tx_ready(struct usb_handle *pdev)
{
//    klog("usb: class_ep0_tx_ready(...)\n");
    return USBD_OK;
}

uint8_t usb_class_sof(struct usb_handle *pdev)
{
//    klog("usb: class_sof(...)\n");
    return USBD_OK;
}

uint8_t usb_class_iso_in_incomplete(struct usb_handle *pdev, uint8_t epnum)
{
//    klog("usb: class_iso_in_incomplete(..., %u)\n", epnum);
    return USBD_OK;
}

uint8_t usb_class_iso_out_incomplete(struct usb_handle *pdev, uint8_t epnum)
{
//    klog("usb: class_iso_out_incomplete(..., %u)\n", epnum);
    return USBD_OK;
}

uint8_t usb_class_data_out(struct usb_handle *pdev, uint8_t epnum)
{
//    klog("usb: class_data_out(..., %u)\n", epnum);

    switch(epnum)
    {
#if GK_ENABLE_USB_MASS_STORAGE
        case EPNUM_MSC_OUT:
            return usb_msc_data_out(pdev, epnum);
#endif
#if GK_LOG_USB
        case EPNUM_CDC_LOG_OUT:
            return usb_cdc_data_out(pdev, epnum);
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
            switch (req->index)
            {
#if GK_ENABLE_USB_MASS_STORAGE
                case (uint16_t)ITF_NUM_MSC:
                    return usb_msc_setup(pdev, req->b_request);
#endif
#if GK_LOG_USB
                case (uint16_t)ITF_NUM_LOG_CDC:
                    return usb_cdc_setup(pdev, req->index, req->b_request);
#endif                    
            }
            break;
    }

    return USBD_FAIL;
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
