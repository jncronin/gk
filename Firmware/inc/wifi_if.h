#ifndef WIFI_IF_H
#define WIFI_IF_H

#include "osnet.h"

class WincNetInterface : public NetInterface
{
    protected:
        HwAddr hwaddr;

        std::vector<std::string> good_net_list, cur_net_list;
        unsigned int scan_n_aps;
        void begin_scan();
        bool scan_in_progress = false;
        friend void *wifi_task(void *);
        friend void wifi_handler(uint8 eventCode, void *p_eventData);
        friend void eth_handler(uint8 msgType, void *pvMsg, void *pvCtrlBuf); 

        bool request_activate = false;
        bool request_deactivate = false;

        char *last_receive_buf = nullptr;

    public:
        const HwAddr &GetHwAddr() const;
        bool GetLinkActive() const;
        int SendEthernetPacket(char *buf, size_t n, const HwAddr &dest, uint16_t ethertype,
            bool release_buffer);
        int SetLinkActive(bool active);

        enum state { WIFI_UNINIT = 0, WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_AWAIT_IP, WIFI_CONNECTED };
        state         connected = WIFI_UNINIT;

        std::vector<std::string> ListNetworks() const;
};

#endif
