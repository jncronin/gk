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
} __attribute__((packed));

struct scsi_inquiry_header
{
    uint8_t opcode;
    uint8_t evpd;
    uint8_t page_code;
    uint16_t allocation_length_be;
    uint8_t control;
} __attribute__((packed));

struct scsi_read_format_capacity_header
{
    uint8_t opcode;
    uint8_t lun;
    uint8_t res0, res1, res2, res3, res4;
    uint16_t allocation_length_be;
    uint8_t naca_flag_link;
} __attribute__((packed));

struct scsi_read_capacity_header
{
    uint8_t opcode;
    // all res0
} __attribute__((packed));

struct scsi_mode_sense_header
{
    uint8_t opcode;
    uint8_t dbd;
    uint8_t page_code;
    uint8_t subpage_code;
    uint8_t allocation_length;
    uint8_t control;
} __attribute__((packed));

struct scsi_request_sense_header
{
    uint8_t opcode;
    uint8_t desc;
    uint8_t res0, res1;
    uint8_t allocation_length;
    uint8_t control;
} __attribute__((packed));

__attribute__((aligned(CACHE_LINE_SIZE))) uint8_t msc_buffer[512];

static uint8_t usb_msc_handle_data_sent_in(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci);

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
    if(ci->expected_length != __builtin_bswap16(hdr->allocation_length_be))
    {
        klog("usb_msc: inquiry header length mismatch %u vs %u\n", ci->expected_length,
            __builtin_bswap16(hdr->allocation_length_be));
        return USBD_FAIL;
    }

    // build inquiry response
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
    ci->command_succeeded = true;
    return usb_core_transmit(pdev, ci->epnum_in, msc_buffer, to_send);
}

static uint8_t usb_msc_handle_read_format_capacity(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci,
    scsi_read_format_capacity_header *hdr)
{
    if(ci->expected_length != __builtin_bswap16(hdr->allocation_length_be))
    {
        klog("usb_msc: read format capacity header length mismatch %u vs %u\n", ci->expected_length,
            __builtin_bswap16(hdr->allocation_length_be));
        return USBD_FAIL;
    }

    // build read format capacity response
    msc_buffer[0] = 0;
    msc_buffer[1] = 0;
    msc_buffer[2] = 0;
    msc_buffer[3] = 8;
    *(uint32_t *)&msc_buffer[4] = __builtin_bswap32(usb_msc_cb_get_num_blocks());
    msc_buffer[8] = 2;
    *(uint32_t *)&msc_buffer[9] = __builtin_bswap32(usb_msc_cb_get_block_size() << 8);

    auto to_send = std::min(ci->expected_length, 12U);

    ci->state = usb_msc_status::DATA_SENT_IN;
    ci->data_sent_in_len = to_send;
    ci->data_sent_in_last_packet = true;
    ci->command_succeeded = true;
    return usb_core_transmit(pdev, ci->epnum_in, msc_buffer, to_send);
}

static uint8_t usb_msc_handle_test_unit_ready(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci,
    scsi_header_6 *hdr)
{
    // just send a csw
    if(usb_msc_cb_get_is_ready())
    {
        ci->data_sent_in_len = 0;
        ci->data_sent_in_last_packet = true;
        ci->command_succeeded = true;
    }
    else
    {
        ci->data_sent_in_len = 0;
        ci->data_sent_in_last_packet = true;
        ci->command_succeeded = false;
        ci->sense_key = 2;
        ci->additional_sense_code = 1;
    }
    return usb_msc_handle_data_sent_in(pdev, ci);
}

static uint8_t usb_msc_handle_request_sense(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci,
    scsi_request_sense_header *hdr)
{
    if(ci->expected_length != hdr->allocation_length)
    {
        klog("usb_msc: request sense header length mismatch %u vs %u\n", ci->expected_length,
            hdr->allocation_length);
        return USBD_FAIL;
    }

    // build a response
    msc_buffer[0] = 0x70;
    msc_buffer[1] = 0;
    msc_buffer[2] = ci->sense_key;
    msc_buffer[3] = 0;
    msc_buffer[4] = 0;
    msc_buffer[5] = 0;
    msc_buffer[6] = 0;
    msc_buffer[7] = 0xc;
    msc_buffer[8] = 0;
    msc_buffer[9] = 0;
    msc_buffer[10] = 0;
    msc_buffer[11] = 0;
    msc_buffer[12] = ci->additional_sense_code;
    msc_buffer[13] = 0;
    msc_buffer[14] = 0;
    msc_buffer[15] = 0;
    msc_buffer[16] = 0;
    msc_buffer[17] = 0;
    msc_buffer[18] = 0;
    msc_buffer[19] = 0;

    // reset sense/additional sense
    ci->sense_key = 0;
    ci->additional_sense_code = 0;

    // send
    auto to_send = std::min(ci->expected_length, 20U);

    ci->state = usb_msc_status::DATA_SENT_IN;
    ci->data_sent_in_len = to_send;
    ci->data_sent_in_last_packet = true;
    ci->command_succeeded = true;
    return usb_core_transmit(pdev, ci->epnum_in, msc_buffer, to_send);
}

static uint8_t usb_msc_handle_mode_sense(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci,
    scsi_mode_sense_header *hdr)
{
    if(ci->expected_length != hdr->allocation_length)
    {
        klog("usb_msc: mode sense header length mismatch %u vs %u\n", ci->expected_length,
            hdr->allocation_length);
        return USBD_FAIL;
    }

    // build a response
    msc_buffer[0] = 3;
    msc_buffer[1] = 0;
    msc_buffer[2] = 0;
    msc_buffer[3] = 0;

    // send
    auto to_send = std::min(ci->expected_length, 4U);

    ci->state = usb_msc_status::DATA_SENT_IN;
    ci->data_sent_in_len = to_send;
    ci->data_sent_in_last_packet = true;
    ci->command_succeeded = true;
    return usb_core_transmit(pdev, ci->epnum_in, msc_buffer, to_send);
}

static uint8_t usb_msc_handle_read_capacity(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci,
    scsi_read_capacity_header *hdr)
{
    // no allocation length

    // build a response
    *(uint32_t *)&msc_buffer[0] = __builtin_bswap32(usb_msc_cb_get_num_blocks());
    *(uint32_t *)&msc_buffer[4] = __builtin_bswap32(usb_msc_cb_get_block_size());

    // send
    auto to_send = std::min(ci->expected_length, 8U);

    ci->state = usb_msc_status::DATA_SENT_IN;
    ci->data_sent_in_len = to_send;
    ci->data_sent_in_last_packet = true;
    ci->command_succeeded = true;
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
        case 0x00:
            // TEST UNIT READY
            if(scsi_header_length != 6)
            {
                klog("usb_msc: test unit ready invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }
            return usb_msc_handle_test_unit_ready(pdev, ci, (scsi_header_6 *)&msc_buffer[15]);

        case 0x03:
            // REQUEST SENSE
            if(scsi_header_length != 6 || !is_in)
            {
                klog("usb_msc: request sense invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }
            return usb_msc_handle_request_sense(pdev, ci, (scsi_request_sense_header *)&msc_buffer[15]);

        case 0x12:
            // INQUIRY
            if(scsi_header_length != 6 || !is_in)
            {
                klog("usb_msc: inquiry invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }
            return usb_msc_handle_inquiry(pdev, ci, (scsi_inquiry_header *)&msc_buffer[15]);

        case 0x1a:
            // MODE SENSE
            if(scsi_header_length != 6 || !is_in)
            {
                klog("usb_msc: mods sense invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }
            return usb_msc_handle_mode_sense(pdev, ci, (scsi_mode_sense_header *)&msc_buffer[15]);

        case 0x23:
            // READ FORMAT CAPACITY
            if(scsi_header_length != 10 || !is_in)
            {
                klog("usb_msc: read format capacity invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }
            return usb_msc_handle_read_format_capacity(pdev, ci,
                (scsi_read_format_capacity_header *)&msc_buffer[15]);

        case 0x25:
            // READ CAPACITY
            if(scsi_header_length != 10 || !is_in)
            {
                klog("usb_msc: read capacity invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }
            return usb_msc_handle_read_capacity(pdev, ci,
                (scsi_read_capacity_header *)&msc_buffer[15]);

        default:
            klog("usb_msc: unsupported scsi command %u\n", msc_buffer[15]);
            // send a fail csw
            ci->data_sent_in_len = 0;
            ci->data_sent_in_last_packet = true;
            ci->command_succeeded = false;
            ci->sense_key = 5;
            ci->additional_sense_code = 0;
            return usb_msc_handle_data_sent_in(pdev, ci);
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
        *(uint32_t *)&msc_buffer[8] = (ci->expected_length > ci->data_sent_in_len) ?
            (ci->expected_length - ci->data_sent_in_len) : 0U;
        msc_buffer[12] = ci->command_succeeded ? 0 : 1;

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
