#ifndef WIFI_AIROC_IF_H
#define WIFI_AIROC_IF_H

#include "osnet.h"
#include "cybsp.h"
#include "cybsp_wifi.h"
#include "cy_network_buffer.h"
#include "cyabs_rtos.h"
#include "whd_types.h"
#include "cyhal.h"


class WifiAirocNetInterface : public WifiNetInterface
{
    protected:
        virtual int HardwareEvent(const netiface_msg &msg);
        virtual int Activate();
        virtual int Deactivate();
        virtual int DoScan();

        whd_driver_t whd_drv;
        whd_interface_t whd_iface;
        int wifi_airoc_reset();

        Spinlock sl_new_networks{};
        bool new_networks_ready = false;
        std::vector<wifi_network> new_networks;
        whd_scan_result_t new_networks_result;

    public:
        WifiAirocNetInterface();

        virtual int SendEthernetPacket(char *buf, size_t n, const HwAddr &dest, uint16_t ethertype,
            bool release_buffer);

        virtual int GetHeaderSize() const;
        virtual int GetFooterSize() const;
        virtual std::string DeviceName() const;

        friend void cb_wifi_scan(whd_scan_result_t **result_ptr, void *user_data, whd_scan_status_t status);
};

#define AIROC_MSG_SUBTYPE_SCAN_COMPLETE     100

#endif
