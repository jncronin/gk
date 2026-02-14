#include <osnet.h>

#include "thread.h"

//#define DEBUG_WIFI  1
//#define DEBUG_ETHERNET 1

#ifdef DEBUG_WIFI
extern NetInterface wifi_if;
#endif

int net_handle_ethernet_packet(const char *buf, size_t n, NetInterface *iface)
{
    // for now, just examine the sorts of packets we receive
    HwAddr dest(buf);
    HwAddr src(&buf[6]);

#ifdef DEBUG_WIFI
    if(iface == &wifi_if)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "wifi: recv packet: \n");
        for(unsigned int i = 0; i < n; i++)
        {
            SEGGER_RTT_printf(0, "%02X ", buf[i]);
        }
        SEGGER_RTT_printf(0, "\n");

        /*if(n >= 300)
        {
            __asm__ volatile ("bkpt \n" ::: "memory");
        }*/
    }
#endif

    bool is_vlan_tagged = (buf[14] == 0x81 && buf[15] == 0x00);

    int ptr = 12;
    if(is_vlan_tagged)
        ptr += 4;
    
    uint16_t ethertype = *reinterpret_cast<const uint16_t *>(&buf[ptr]);
    ethertype = __builtin_bswap16(ethertype);
    if(ethertype < 0x0600)
        return NET_NOTSUPP;     // LLC frame or similar
    ptr += 2;

    // TODO check CRC

#if DEBUG_ETHERNET
    {
        CriticalGuard cg;
        SEGGER_RTT_printf(0, "net: ethernet iface: %s, recv: %s to %s, ethertype: %u\n",
            iface->GetHwAddr().ToString().c_str(),
            src.ToString().c_str(), dest.ToString().c_str(), ethertype);
    }
#endif

    // sent to us?
    bool to_us = false;
    bool is_multicast = false;

    if(dest == HwAddr::multicast)
        is_multicast = true;
    if(dest == iface->GetHwAddr())
        to_us = true;
    
    if(to_us || is_multicast)
    {
        EthernetPacket epkt { .src = src, .dest = dest, .ethertype = ethertype,
            .contents = &buf[ptr], .n = n - ptr - 4,
            .iface = iface, .link_layer_data = buf, .link_layer_n = n };
        switch (ethertype)
        {
            case IPPROTO_IP:
                return net_handle_ip4_packet(epkt);

            case 0x0806:
                return net_handle_arp_packet(epkt);

            default:
                return NET_NOTSUPP;
        }
    }

    return NET_NOTUS;
}

int net_inject_ethernet_packet(const char *buf, size_t n, NetInterface *iface)
{
    // add packet to a queue for service by the net thread
    net_msg m;
    m.msg_type = net_msg::net_msg_type::InjectPacket;
    m.msg_data.packet.buf = const_cast<char *>(buf);
    m.msg_data.packet.n = n;
    m.msg_data.packet.iface = iface;
    m.msg_data.packet.release_packet = true;

    return net_queue_msg(m);
}

#define CRC_POLY    0xEDB88320

uint32_t net_ethernet_calc_crc(const char *buf, size_t len)
{
    auto data = reinterpret_cast<const uint8_t *>(buf);

    unsigned int i, j;
    uint32_t crc;

    if (!data)
        return 0;

    if (len < 1)
        return 0;

    crc = 0xFFFFFFFF;

    for (j = 0; j < len; j++) {
        crc ^= data[j];

        for (i = 0; i < 8; i++) {
             crc = (crc & 1) ? ((crc >> 1) ^ CRC_POLY) : (crc >> 1);
        }
    }

    return (crc ^ 0xFFFFFFFF);
}
