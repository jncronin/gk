#include "usb_class.h"
#include "usb_device.h"

#include "logger.h"
#include "vblock.h"
#include <cstring>

#if GK_LOG_USB

uint8_t usb_cdc_acm_init(usb_handle *pdev, uint8_t cfgidx, uint16_t itf,
    uint8_t ep_notif, uint8_t epin, uint8_t epout,
    LockFreeBuffer *send_buf, LockFreeBuffer *recv_buf)
{
    (void)cfgidx;       // enable in all configurations

    // create a class info structure
    auto ci = std::make_shared<usb_cdc_acm_status>();

    usb_core_configure_ep(pdev, ep_notif, USBD_EP_TYPE_INTR, 8);
    usb_core_configure_ep(pdev, epin, USBD_EP_TYPE_BULK, 512);
    usb_core_configure_ep(pdev, epout, USBD_EP_TYPE_BULK, 512);

    ci->ep_in = epin;
    ci->ep_out = epout;
    ci->send_data = send_buf;
    ci->recv_data = recv_buf;

    auto &uci = *(usb_class_info *)pdev->class_data;
    uci[ep_notif] = ci;
    uci[epin] = ci;
    uci[epout] = ci;
    uci[itf << 8] = ci;

    size_t rx_len = 0;
    auto rxbuf = ci->get_rx_buffer(&rx_len);
    usb_core_receive(pdev, epout, rxbuf, rx_len);

    return USBD_OK;
}

uint8_t usb_cdc_setup(usb_handle *pdev, uint16_t itf, uint8_t bRequest)
{
    // get interface-specific device data
    auto ci = usb_itf_to_status<usb_cdc_status>(pdev, itf);
    if(!ci)
    {
        return USBD_FAIL;
    }

    switch(bRequest)
    {
        case 0x20:
            // set line coding
            return usb_core_receive_ep0(pdev, ci->line_coding, 7);

        case 0x21:
            // get line coding
            return usb_core_transmit_ep0(pdev, ci->line_coding, 7);

        case 0x22:
            // set_control_line_state
            return USBD_OK;

        case 0x23:
            // send_break - stall here
            return USBD_FAIL;
    }

    return USBD_FAIL;
}

uint8_t usb_cdc_data_in(usb_handle *pdev, uint8_t epnum)
{
    auto ci = usb_ep_to_status<usb_cdc_status>(pdev, epnum);
    if(!ci)
    {
        return USBD_FAIL;
    }
    return ci->tfer_in(pdev, true);
}

uint8_t usb_cdc_data_out(usb_handle *pdev, uint8_t epnum, size_t dlen)
{
    auto ci = usb_ep_to_status<usb_cdc_status>(pdev, epnum);
    if(!ci)
    {
        return USBD_FAIL;
    }

    size_t rx_len = 0;
    auto rxbuf = ci->get_rx_buffer(&rx_len);

    klog("usb_cdc: DATA OUT: %.*s\n", dlen, rxbuf);

    return usb_core_receive(pdev, epnum, rxbuf, rx_len);
}


// ACM handlers - use an internal buffer
usb_cdc_acm_status::usb_cdc_acm_status()
{
    pb_rx = Pmem.acquire(PAGE_SIZE);
    pb_tx = Pmem.acquire(PAGE_SIZE);
    vb_rx = vblock_alloc(PAGE_SIZE, false, true, false);
    vb_tx = vblock_alloc(PAGE_SIZE, false, true, false);

    vmem_map(vb_rx.base, pb_rx.base, false, true, false);
    vmem_map(vb_tx.base, pb_tx.base, false, true, false);
}

usb_cdc_acm_status::~usb_cdc_acm_status()
{
    if(pb_rx.valid && vb_rx.valid)
        vmem_unmap(vb_rx);
    if(pb_tx.valid && vb_tx.valid)
        vmem_unmap(vb_tx);
}

uint8_t *usb_cdc_acm_status::get_rx_buffer(size_t *rx_len)
{
    if(vb_rx.valid)
    {
        if(rx_len) *rx_len = vb_rx.length;
        return (uint8_t *)vb_rx.base;
    }
    return nullptr;
}

uint8_t *usb_cdc_acm_status::get_tx_buffer(size_t *tx_len)
{
    if(vb_tx.valid)
    {
        if(tx_len) *tx_len = vb_tx.length;
        return (uint8_t *)vb_tx.base;
    }
    return nullptr;
}

uint8_t usb_cdc_acm_status::tfer_in(usb_handle *pdev, bool prev_in_complete)
{
    if(!prev_in_complete && in_queued)
        return USBD_OK;
    if(!send_data)
    {
        in_queued = false;
        return USBD_FAIL;
    }

    size_t txbuf_size = 0;
    auto txbuf = get_tx_buffer(&txbuf_size);
    txbuf_size = std::min(txbuf_size, (size_t)pdev->data->in_ep[ep_in & EP_NUM_MASK].maxpacket);

    auto nread = send_data->read(txbuf, txbuf_size);
    if(nread <= 0)
    {
        in_queued = false;
        return USBD_FAIL;
    }

    in_queued = true;

    return usb_core_transmit(pdev, ep_in, txbuf, (uint32_t)nread);
}

#endif
