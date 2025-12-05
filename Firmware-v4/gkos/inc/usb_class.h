#ifndef USB_CLASS_H
#define USB_CLASS_H

#include "gk_conf.h"
#include <stdint.h>
#include <stddef.h>
#include "usb_device.h"

class usb_class_status
{
    public:
        virtual ~usb_class_status() = default;
};

#if GK_ENABLE_USB_MASS_STORAGE
uint8_t usb_msc_setup(usb_handle *pdev, uint8_t bRequest);
uint8_t usb_msc_init(usb_handle *pdev, uint8_t cfgidx);
uint8_t usb_msc_data_out(usb_handle *pdev, uint8_t epnum);
uint8_t usb_msc_data_in(usb_handle *pdev, uint8_t epnum);


// user-provided callbacks
uint32_t usb_msc_cb_get_num_blocks();
uint32_t usb_msc_cb_get_block_size();
bool usb_msc_cb_get_is_ready();
int usb_msc_cb_read_data(uint32_t lba, size_t nbytes, void *buf, size_t *nread);
int usb_msc_cb_write_data(uint32_t lba, size_t nbytes, const void *buf, size_t *nwritten);

class usb_msc_status : public usb_class_status
{
    public:
        enum msc_state { HEADER_REQ, DATA_SENT_IN, CSW_SENT_IN, EXPECT_DATA_OUT };
        msc_state state;
        uint32_t tag;
        uint32_t expected_length;
        uint32_t data_sent_in_len;
        uint32_t data_written_len;
        bool data_sent_in_last_packet;
        bool command_succeeded;
        uint8_t sense_key, additional_sense_code;
        uint32_t next_lba;
        void *next_buf;
        size_t buflen;

        uint8_t epnum_in, epnum_out;

        ~usb_msc_status() = default;
};

#endif

#include <memory>
#include <map>

using usb_class_info = std::map<uint8_t, std::shared_ptr<usb_class_status>>;

#endif
