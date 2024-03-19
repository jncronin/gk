#include <sd.h>
#include <ext4_thread.h>
#include <tusb.h>
#include <osnet.h>
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

// Invoked when received SCSI READ10 command
// - Address = lba * BLOCK_SIZE + offset
//   - offset is only needed if CFG_TUD_MSC_EP_BUFSIZE is smaller than BLOCK_SIZE.
//
// - Application fill the buffer (up to bufsize) with address contents and return number of read byte. If
//   - read < bufsize : These bytes are transferred first and callback invoked again for remaining data.
//
//   - read == 0      : Indicate application is not ready yet e.g disk I/O busy.
//                      Callback invoked again with the same parameters later on.
//
//   - read < 0       : Indicate application error e.g invalid address. This request will be STALLed
//                      and return failed status in command status wrapper phase.
int32_t tud_msc_read10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
    if(!buffer || offset || bufsize % 512)
    {
        return -1;
    }
    return sd_perform_transfer(lba, bufsize / 512, buffer, true);
}

// Invoked when received SCSI WRITE10 command
// - Address = lba * BLOCK_SIZE + offset
//   - offset is only needed if CFG_TUD_MSC_EP_BUFSIZE is smaller than BLOCK_SIZE.
//
// - Application write data from buffer to address contents (up to bufsize) and return number of written byte. If
//   - write < bufsize : callback invoked again with remaining data later on.
//
//   - write == 0      : Indicate application is not ready yet e.g disk I/O busy.
//                       Callback invoked again with the same parameters later on.
//
//   - write < 0       : Indicate application error e.g invalid address. This request will be STALLed
//                       and return failed status in command status wrapper phase.
//
// TODO change buffer to const uint8_t*
int32_t tud_msc_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
    if(!buffer || offset || bufsize % 512)
    {
        return -1;
    }
    return sd_perform_transfer(lba, bufsize / 512, buffer, false);
}


// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    if(vendor_id)
    {
        strcpy((char *)vendor_id, "JC");
    }
    if(product_id)
    {
        strcpy((char *)product_id, "GKDrive");
    }
    if(product_rev)
    {
        strcpy((char *)product_rev, "v0.1");
    }
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    return sd_get_ready() && sd_get_mode() == sd_mode_t::MSC;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
    if(block_size)
    {
        *block_size = 512U;
    }

    if(block_count)
    {
        if(!sd_get_ready())
        {
            *block_count = 0;
        }
        else
        {
            auto size = sd_get_size();
            *block_count = static_cast<uint32_t>(size / 512);
        }
    }
}

/**
 * Invoked when received an SCSI command not in built-in list below.
 * - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, TEST_UNIT_READY, START_STOP_UNIT, MODE_SENSE6, REQUEST_SENSE
 * - READ10 and WRITE10 has their own callbacks
 *
 * \param[in]   lun         Logical unit number
 * \param[in]   scsi_cmd    SCSI command contents which application must examine to response accordingly
 * \param[out]  buffer      Buffer for SCSI Data Stage.
 *                            - For INPUT: application must fill this with response.
 *                            - For OUTPUT it holds the Data from host
 * \param[in]   bufsize     Buffer's length.
 *
 * \return      Actual bytes processed, can be zero for no-data command.
 * \retval      negative    Indicate error e.g unsupported command, tinyusb will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 */
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
    union scsi_cmd_type
    {
        uint8_t bbuf[16];
        uint32_t wbuf[4];
    };

    scsi_cmd_type sct;
    memcpy(&sct.bbuf, scsi_cmd, 16);

    CriticalGuard cg(s_rtt);
    {
        SEGGER_RTT_printf(0, "msc: unhandled scsi command %x %x %x %x\n",
            sct.wbuf[0], sct.wbuf[1], sct.wbuf[2], sct.wbuf[3]);
    }

    return -1;
}
