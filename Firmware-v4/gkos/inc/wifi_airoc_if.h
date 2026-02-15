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
        virtual int HardwareEvent();
        virtual int Activate();
        virtual int Deactivate();

        whd_driver_t whd_drv;
        whd_interface_t whd_iface;
        int wifi_airoc_reset();

    public:
        WifiAirocNetInterface();

        virtual int SendEthernetPacket(char *buf, size_t n, const HwAddr &dest, uint16_t ethertype,
            bool release_buffer);

        virtual int GetHeaderSize() const;
        virtual int GetFooterSize() const;
        virtual std::string DeviceName() const;
};


#endif
