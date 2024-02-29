#include <osnet.h>

#include "thread.h"
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

int net_handle_ethernet_packet(const char *buf, size_t n, NetInterface *iface)
{
    // for now, just examine the sorts of packets we receive
    HwAddr dest(buf);
    HwAddr src(&buf[6]);

    bool is_vlan_tagged = (buf[14] == 0x81 && buf[15] == 0x00);

    int ptr = 12;
    if(is_vlan_tagged)
        ptr += 4;
    
    uint16_t ethertype = *reinterpret_cast<const uint16_t *>(&buf[ptr]);
    ethertype = __builtin_bswap16(ethertype);
    ptr += 2;

    // TODO check CRC

    {
        CriticalGuard c(s_rtt);
        SEGGER_RTT_printf(0, "net: ethernet recv: %s to %s, ethertype: %u\n",
            src.ToString().c_str(), dest.ToString().c_str(), ethertype);
    }

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
            .iface = iface };
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
    m.msg_data.packet.buf = buf;
    m.msg_data.packet.n = n;
    m.msg_data.packet.iface = iface;

    return net_queue_msg(m);
}
