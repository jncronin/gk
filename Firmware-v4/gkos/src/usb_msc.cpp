#include "usb_class.h"
#include "usb_device.h"

#include "cache.h"
#include "logger.h"
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

struct scsi_read10_header
{
    uint8_t opcode;
    uint8_t rdprotect;
    uint32_t lba_be;
    uint8_t group_number;
    uint16_t nblocks_be;
    uint8_t control;
} __attribute__((packed));

__attribute__((aligned(CACHE_LINE_SIZE))) uint8_t msc_buffer[65536];

// sense codes
#define SENSE_CODE_NO_SENSE                         0
#define SENSE_CODE_RECOVERED_ERROR                  1
#define SENSE_CODE_NOT_READY                        2
#define SENSE_CODE_MEDIUM_ERROR                     3
#define SENSE_CODE_HARDWARE_ERROR                   4
#define SENSE_CODE_ILLEGAL_REQUEST                  5

#define ADDITIONAL_SENSE_LBA_OUT_OF_RANGE           0x21

static uint8_t usb_msc_handle_data_sent_in(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci);
static uint8_t usb_msc_handle_expect_data_out(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci);

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
    *(uint32_t *)&msc_buffer[4] = __builtin_bswap32(usb_msc_cb_get_num_blocks() - 1);   // last lba address
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
    *(uint32_t *)&msc_buffer[0] = __builtin_bswap32(usb_msc_cb_get_num_blocks() - 1);       // last lba address
    *(uint32_t *)&msc_buffer[4] = __builtin_bswap32(usb_msc_cb_get_block_size());

    // send
    auto to_send = std::min(ci->expected_length, 8U);

    ci->state = usb_msc_status::DATA_SENT_IN;
    ci->data_sent_in_len = to_send;
    ci->data_sent_in_last_packet = true;
    ci->command_succeeded = true;
    return usb_core_transmit(pdev, ci->epnum_in, msc_buffer, to_send);
}

static uint8_t usb_msc_handle_write10(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci,
    scsi_read10_header *hdr)
{
    // check validity
    auto dev_n_blocks = usb_msc_cb_get_num_blocks();
    auto lba = __builtin_bswap32(hdr->lba_be);
    auto req_n_blocks = (uint32_t)__builtin_bswap16(hdr->nblocks_be);

    if((req_n_blocks * usb_msc_cb_get_block_size()) != ci->expected_length)
    {
        klog("usb_msc: write10, expected_len: %u, lba: %u, n_blocks: %u\n",
            ci->expected_length, __builtin_bswap32(hdr->lba_be), __builtin_bswap16(hdr->nblocks_be));
        return USBD_FAIL;
    }

#if DEBUG_USB
    klog("usb_msc: write10, expected_len: %u, lba: %u, n_blocks: %u\n",
        ci->expected_length, __builtin_bswap32(hdr->lba_be), __builtin_bswap16(hdr->nblocks_be));
#endif

    if((lba >= dev_n_blocks) || ((lba + req_n_blocks) > dev_n_blocks))
    {
        // out of range
        ci->sense_key = SENSE_CODE_ILLEGAL_REQUEST;
        ci->additional_sense_code = ADDITIONAL_SENSE_LBA_OUT_OF_RANGE;
        ci->command_succeeded = false;
        ci->data_sent_in_len = 0;
        ci->data_sent_in_last_packet = true;

        // TODO: stall data out pipe       

        return usb_msc_handle_data_sent_in(pdev, ci);
    }
    else
    {
        // prep a (potentially multi-sector) write
        ci->next_lba = lba;
        ci->next_buf = msc_buffer;
        ci->buflen = sizeof(msc_buffer);
        ci->data_sent_in_last_packet = false;
        ci->data_sent_in_len = 0;
        ci->data_written_len = 0;
        ci->state = usb_msc_status::EXPECT_DATA_OUT;
        return usb_msc_handle_expect_data_out(pdev, ci);
    }
}

static uint8_t usb_msc_handle_read10(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci,
    scsi_read10_header *hdr)
{
    // check validity
    auto dev_n_blocks = usb_msc_cb_get_num_blocks();
    auto lba = __builtin_bswap32(hdr->lba_be);
    auto req_n_blocks = (uint32_t)__builtin_bswap16(hdr->nblocks_be);

    if((req_n_blocks * usb_msc_cb_get_block_size()) != ci->expected_length)
    {
        klog("usb_msc: read10, expected_len: %u, lba: %u, n_blocks: %u\n",
            ci->expected_length, __builtin_bswap32(hdr->lba_be), __builtin_bswap16(hdr->nblocks_be));
        return USBD_FAIL;
    }

#if DEBUG_USB
    klog("usb_msc: read10, expected_len: %u, lba: %u, n_blocks: %u\n",
        ci->expected_length, __builtin_bswap32(hdr->lba_be), __builtin_bswap16(hdr->nblocks_be));
#endif

    if((lba >= dev_n_blocks) || ((lba + req_n_blocks) > dev_n_blocks))
    {
        // out of range
        ci->sense_key = SENSE_CODE_ILLEGAL_REQUEST;
        ci->additional_sense_code = ADDITIONAL_SENSE_LBA_OUT_OF_RANGE;
        ci->command_succeeded = false;
        ci->data_sent_in_len = 0;
        ci->data_sent_in_last_packet = true;

        klog("usb_msc: read10: out of range: lba: %u, req_n_blocks: %u, dev_n_blocks: %u\n",
            lba, req_n_blocks, dev_n_blocks);
    }
    else
    {
        // prep a (potentially multi-sector) read
        ci->next_lba = lba;
        ci->next_buf = msc_buffer;
        ci->buflen = sizeof(msc_buffer);
        ci->data_sent_in_last_packet = false;
        ci->data_sent_in_len = 0;
    }

    return usb_msc_handle_data_sent_in(pdev, ci);
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
            if(scsi_header_length < 6 || !is_in)
            {
                klog("usb_msc: request sense invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }
            return usb_msc_handle_request_sense(pdev, ci, (scsi_request_sense_header *)&msc_buffer[15]);

        case 0x12:
            // INQUIRY
            if(scsi_header_length < 6 || !is_in)
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
                klog("usb_msc: mode sense invalid params: header_len: %u, is_in: %s\n",
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

        case 0x28:
            // READ (10)
            if(scsi_header_length != 10 || !is_in)
            {
                klog("usb_msc: read (10) invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }

            return usb_msc_handle_read10(pdev, ci,
                (scsi_read10_header *)&msc_buffer[15]);

        case 0x2a:
            // WRITE (10)
            if(scsi_header_length != 10 || is_in)
            {
                klog("usb_msc: write (10) invalid params: header_len: %u, is_in: %s\n",
                    scsi_header_length, is_in ? "TRUE" : "FALSE");
                return USBD_FAIL;
            }

            return usb_msc_handle_write10(pdev, ci,
                (scsi_read10_header *)&msc_buffer[15]);

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

static uint8_t usb_msc_handle_expect_data_out(usb_handle *pdev, std::shared_ptr<usb_msc_status> ci)
{
    // first write any data we have just received
    while(ci->data_written_len < ci->data_sent_in_len)
    {
        auto to_write = ci->data_sent_in_len - ci->data_written_len;
        size_t bwritten = 0;

#if DEBUG_USB
        klog("usb_msc: data_out: data_written_len: %llu, data_sent_in: %llu, to_write: %llu\n",
            ci->data_written_len, ci->data_sent_in_len, to_write);
#endif

        auto ret = usb_msc_cb_write_data(ci->next_lba, to_write, ci->next_buf, &bwritten);
        if(ret != 0)
        {
#if DEBUG_USB
            klog("usb_msc: write_data fail: to_write: %u, lba: %u, buf: %p\n",
                to_write, ci->next_lba, ci->next_buf);
#endif

            // TODO stall out ep
            ci->command_succeeded = false;
            ci->data_sent_in_last_packet = true;
            ci->sense_key = SENSE_CODE_MEDIUM_ERROR;
            ci->additional_sense_code = 0xc;
            return usb_msc_handle_data_sent_in(pdev, ci);
        }

#if DEBUG_USB
        klog("usb_msc: write_data: bwritten: %llu\n", bwritten);
#endif

        auto nsectors = bwritten / usb_msc_cb_get_block_size();
        ci->next_lba += nsectors;
        ci->data_written_len += bwritten;
    }

    // have we finished?
    if(ci->data_sent_in_len >= ci->expected_length)
    {
#if DEBUG_USB
        klog("usb_msc: write complete\n");
#endif
        // send success
        ci->data_sent_in_last_packet = true;
        ci->command_succeeded = true;
        return usb_msc_handle_data_sent_in(pdev, ci);
    }

    // request more data
    auto bytes_left = ci->expected_length - ci->data_sent_in_len;
    auto to_request = std::min(bytes_left, (uint32_t)ci->buflen);
    ci->data_sent_in_len += to_request;
    ci->state = usb_msc_status::EXPECT_DATA_OUT;

#if DEBUG_USB
    klog("usb_msc: to_request: %llu\n", to_request);
#endif
    return usb_core_receive(pdev, ci->epnum_out, (uint8_t *)ci->next_buf, to_request);
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

        case usb_msc_status::EXPECT_DATA_OUT:
            return usb_msc_handle_expect_data_out(pdev, ci);

        default:
            klog("usb_msc: data_out: invalid state: %d\n", ci->state);
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
        // multi-sector read

        // read the sector(s)
        auto bytes_left = ci->expected_length - ci->data_sent_in_len;
        auto sector_size = usb_msc_cb_get_block_size();
        auto sectors_left = (bytes_left + (sector_size - 1)) & ~(sector_size - 1);

        auto bytes_to_read = std::min(ci->buflen, (size_t)sectors_left * sector_size);
        size_t bytes_read = 0;

        auto ret = usb_msc_cb_read_data(ci->next_lba, bytes_to_read, ci->next_buf, &bytes_read);

        if(ret != 0)
        {
            // fail
            klog("usb_msc: read_data(%u, %u, %p, ...) failed: %d\n", ci->next_lba, bytes_to_read,
                    ci->next_buf, ret);
            
            ci->command_succeeded = false;
            ci->data_sent_in_last_packet = true;
            ci->sense_key = SENSE_CODE_MEDIUM_ERROR;
            ci->additional_sense_code = 0x11; // unrecovered read error
            return usb_msc_handle_data_sent_in(pdev, ci);
        }

        // we read some bytes, update our buffers
        auto to_send = std::min((uint32_t)bytes_read, bytes_left);
        ci->data_sent_in_last_packet = to_send == bytes_left;
        ci->next_lba += bytes_read / sector_size;
        ci->data_sent_in_len += to_send;
        ci->command_succeeded = true;

#if DEBUG_USB
        if(ci->data_sent_in_last_packet)
        {
            klog("usb_msc: read10: to_send: %u, bytes_read: %u, bytes_left: %u, data_sent_in_len: %u, next_lba: %u\n", to_send,
                bytes_read, bytes_left, ci->data_sent_in_len, ci->next_lba);
        }
#endif

        // send the data
        ci->state = usb_msc_status::DATA_SENT_IN;
        return usb_core_transmit(pdev, ci->epnum_in, (uint8_t *)ci->next_buf, to_send);
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
