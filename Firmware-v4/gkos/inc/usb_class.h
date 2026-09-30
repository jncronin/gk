#ifndef USB_CLASS_H
#define USB_CLASS_H

#include "gk_conf.h"
#include <stdint.h>
#include <stddef.h>
#include "usb_device.h"
#include "vmem.h"
#include "pmem.h"

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
uint64_t usb_msc_cb_get_num_blocks();
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
        uint64_t next_lba;
        void *next_buf;
        size_t buflen;

        uint8_t epnum_in, epnum_out;

        ~usb_msc_status() = default;
};

#endif

#if GK_ENABLE_USB_NETWORK || GK_ENABLE_USB_CDC || GK_LOG_USB
uint8_t usb_cdc_setup(usb_handle *pdev, uint16_t itf, uint8_t bRequest);
uint8_t usb_cdc_acm_init(usb_handle *pdev, uint8_t cfgidx, uint16_t itf,
    uint8_t ep_notif, uint8_t epin, uint8_t epout);
uint8_t usb_cdc_data_out(usb_handle *pdev, uint8_t epnum);
uint8_t usb_cdc_data_in(usb_handle *pdev, uint8_t epnum);

class usb_cdc_status : public usb_class_status
{
    public:
        virtual uint8_t *get_rx_buffer(size_t *rx_buf_len) = 0;
        virtual uint8_t *get_tx_buffer(size_t *tx_buf_len) = 0;
        uint8_t line_coding[8] = { 0 };
        ~usb_cdc_status() = default;
};

class usb_cdc_acm_status : public usb_cdc_status
{
    protected:
        PMemBlock pb_rx, pb_tx;
        VMemBlock vb_rx, vb_tx;

    public:
        uint8_t *get_rx_buffer(size_t *rx_buf_len);
        uint8_t *get_tx_buffer(size_t *tx_buf_len);

        usb_cdc_acm_status();
        ~usb_cdc_acm_status();
};

#endif

#include <memory>
#include <map>

using usb_class_info = std::map<uint16_t, std::shared_ptr<usb_class_status>>;

template <class T> std::shared_ptr<T> usb_itf_to_status(usb_handle *pdev, uint16_t itf)
{
    auto &uci = *(usb_class_info *)pdev->class_data;
    auto iter = uci.find(itf << 8);
    if(iter == uci.end())
        return nullptr;
    return std::reinterpret_pointer_cast<T>(iter->second);
}

template <class T> std::shared_ptr<T> usb_ep_to_status(usb_handle *pdev, uint16_t ep)
{
    auto &uci = *(usb_class_info *)pdev->class_data;
    auto iter = uci.find(ep);
    if(iter == uci.end())
        return nullptr;
    return std::reinterpret_pointer_cast<T>(iter->second);
}

#endif
