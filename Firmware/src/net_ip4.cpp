#include <osnet.h>
#include <osmutex.h>
#include <map>
#include <vector>
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

static std::vector<IP4Address> ourips;
static std::map<IP4Addr, IP4Address> ipcache;

int net_handle_ip4_packet(const EthernetPacket &epkt)
{
    auto buf = epkt.contents;

    auto protocol = buf[9];
    auto version = buf[0] & 0xf;
    auto src = *reinterpret_cast<const uint32_t *>(&buf[12]);
    auto dest = *reinterpret_cast<const uint32_t *>(&buf[16]);

    auto total_len = ntohs(*reinterpret_cast<const uint16_t *>(&buf[2]));
    auto ihl = buf[0] >> 4;
    auto header_len = static_cast<uint16_t>(ihl * 4);
    auto blen = static_cast<uint16_t>(total_len - header_len);

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "net: ip: version: %u, protocol: %u, src: %u, dest: %u\n",
            version, protocol, src, dest);
    }

    bool is_us = false;
    if(ipcache.find(dest) != ipcache.end())
    {
        is_us = true;
    }
    else
    {
        for(const auto &t : ourips)
        {
            if(IP4Addr::Compare(t.addr, dest, t.nm))
            {
                ipcache[dest] = t;
                is_us = true;
                break;
            }
            else if(t.Broadcast() == dest)
            {
                // is broadcast to us
                ipcache[dest] = t;
                is_us = true;
                break;
            }
        }
    }
    
    if(!is_us)
        return NET_NOTUS;
    
    IP4Packet ipkt { .src = src, .dest = dest, .protocol = protocol,
        .contents = &buf[header_len], .n = blen, .epacket = epkt };
    
    switch(protocol)
    {
        case IPPROTO_ICMP:
            return net_handle_icmp_packet(ipkt);

        case IPPROTO_TCP:
            return net_handle_tcp_packet(ipkt);

        case IPPROTO_UDP:
            return net_handle_udp_packet(ipkt);
    }
    return NET_NOTSUPP;
}

IP4Addr::IP4Addr(uint32_t val) : v(val) {}

bool IP4Addr::operator==(const uint32_t &other) const
{
    return v == other;
}

bool IP4Addr::operator<(const IP4Addr &other) const
{
    return v < other.v;
}

bool IP4Addr::Compare(IP4Addr a, IP4Addr b, IP4Addr mask)
{
    return (a.v & mask.v) == (b.v & mask.v);
}

IP4Addr::operator uint32_t() const
{
    return v;
}

IP4Addr IP4Address::Broadcast() const
{
    return addr | (~nm);
}
