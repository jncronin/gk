#include "usb_class.h"
#include "usb_device.h"

#include "cache.h"
#include <cstring>

struct scsi_header_6
{
    uint8_t opcode;
    uint8_t misc;
    uint16_t lba_lsb;
    uint8_t tlen;
    uint8_t control;

    uint32_t lba() const
    {
        return (uint32_t)((misc & 0x1f) << 16) | (uint32_t)lba_lsb;
    }
} __attribute__((aligned));

struct scsi_inquiry_header
{
    uint8_t opcode;
    uint8_t evpd;
    uint8_t page_code;
    uint16_t allocation_length;
    uint8_t control;
};

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

    // create a class info structure
    auto ci = std::make_shared<usb_msc_status>();
    ci->state = usb_msc_status::HEADER_REQ;
    ci->epnum_in = EPNUM_MSC_IN;
    ci->epnum_out = EPNUM_MSC_OUT;

    usb_core_configure_ep(pdev, EPNUM_MSC_IN, USBD_EP_TYPE_BULK, 512);
    usb_core_configure_ep(pdev, EPNUM_MSC_OUT, USBD_EP_TYPE_BULK, 512);

    auto &uci = *(usb_class_info *)pdev->class_data;
    uci[EPNUM_MSC_IN] = ci;
    uci[EPNUM_MSC_OUT] = ci;

    usb_core_receive(pdev, EPNUM_MSC_OUT, msc_buffer, 31);  // receive first cbw

    return USBD_OK;
}

static uint8_t usb_msc_handle_inquiry(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci,
    scsi_inquiry_header *hdr)
{
    if(ci->expected_length != hdr->allocation_length)
    {
        klog("usb_msc: inquiry header length mismatch %u vs %u\n", ci->expected_length,
            hdr->allocation_length);
        return USBD_FAIL;
    }

    // build inquiry resposne
    msc_buffer[0] = 0;
    msc_buffer[1] = 0x80;
    msc_buffer[2] = 2;      // as per tusb
    msc_buffer[3] = 2;
    msc_buffer[4] = 31;
    msc_buffer[5] = 0;
    msc_buffer[6] = 0;
    msc_buffer[7] = 0;
    memcpy(&msc_buffer[8], "JC      ", 8);
    memcpy(&msc_buffer[16], "GKV4            ", 16);
    memcpy(&msc_buffer[32], "V4  ", 4);

    auto to_send = std::min(ci->expected_length, 36U);

    ci->state = usb_msc_status::DATA_SENT_IN;
    ci->data_sent_in_len = to_send;
    ci->data_sent_in_last_packet = true;
    return usb_core_transmit(pdev, ci->epnum_in, msc_buffer, to_send);
}

static uint8_t usb_msc_handle_header(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci)
{
    // Interpret a USB Mass Storage Class - Bulk Only Transport Command Block Wrapper
    if(*(uint32_t *)&msc_buffer[0] != 0x43425355U)
    {
        klog("usb_msc: invalid block header\n");
        return USBD_FAIL;
    }

    ci->tag = *(uint32_t *)&msc_buffer[4];
    ci->expected_length = *(uint32_t *)&msc_buffer[8];
    bool is_in = (msc_buffer[12] & 0x80U) != 0;
    // ignore LUN here
    uint8_t scsi_header_length = msc_buffer[14] & 0x1fU;

    // The SCSI header itself starts at offset 15
    switch(msc_buffer[15])
    {
        case 0x12:
            // INQUIRY
            if(scsi_header_length != 6 || !is_in)
            {
                klog("usb_msc: inquiry invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }
            return usb_msc_handle_inquiry(pdev, ci, (scsi_inquiry_header *)&msc_buffer[15]);

        default:
            klog("usb_msc: unsupported scsi command %u\n", msc_buffer[15]);
            return USBD_FAIL;
    }
}

uint8_t usb_msc_data_out(usb_handle *pdev, uint8_t epnum)
{
    auto &uci = *(usb_class_info *)pdev->class_data;
    auto ci = std::reinterpret_pointer_cast<usb_msc_status>(uci[epnum]);

    switch(ci->state)
    {
        case usb_msc_status::HEADER_REQ:
            // we have received a header, interpret it and handle the rest of the message, if any
            return usb_msc_handle_header(pdev, ci);

        default:
            return USBD_FAIL;
    }
}

uint8_t usb_msc_handle_data_sent_in(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci)
{
    if(ci->data_sent_in_last_packet)
    {
        // build a command status wrapper
        *(uint32_t *)&msc_buffer[0] = 0x53425355;
        *(uint32_t *)&msc_buffer[4] = ci->tag;
        *(uint32_t *)&msc_buffer[8] = 0;
        msc_buffer[12] = 0;

        ci->state = usb_msc_status::CSW_SENT_IN;
        return usb_core_transmit(pdev, ci->epnum_in, msc_buffer, 13);
    }
    else
    {
        klog("usb_msc: data_sent_in: packet resuming not implemented\n");
        return USBD_FAIL;
    }
}

uint8_t usb_msc_data_in(usb_handle *pdev, uint8_t epnum)
{
    auto &uci = *(usb_class_info *)pdev->class_data;
    auto ci = std::reinterpret_pointer_cast<usb_msc_status>(uci[epnum]);

    switch(ci->state)
    {
        case usb_msc_status::DATA_SENT_IN:
            return usb_msc_handle_data_sent_in(pdev, ci);

        case usb_msc_status::CSW_SENT_IN:
            ci->state = usb_msc_status::HEADER_REQ;
            return usb_core_receive(pdev, EPNUM_MSC_OUT, msc_buffer, 31);  // receive next cbw

        default:
            klog("msc_data_in: invalid state: %u\n", (unsigned int)ci->state);
            return USBD_FAIL;
    }
}
