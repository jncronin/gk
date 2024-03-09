#include <osnet.h>
#include <osmutex.h>
#include <map>
#include <vector>
#include <limits>
#include "thread.h"
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

SRAM4_DATA static Spinlock s_ips;
SRAM4_DATA static std::vector<IP4Address> ourips;
SRAM4_DATA static std::map<IP4Addr, IP4Address> ipcache;

SRAM4_DATA static Spinlock s_routes;
SRAM4_DATA static std::vector<IP4Route> routes;

int net_handle_ip4_packet(const EthernetPacket &epkt)
{
    auto buf = epkt.contents;

    auto protocol = buf[9];
    auto version = buf[0] >> 4;
    if(version != 4)
    {
        return NET_NOTSUPP;
    }
    auto src = IP4Addr(&buf[12]);
    auto dest = IP4Addr(&buf[16]);

    auto total_len = ntohs(*reinterpret_cast<const uint16_t *>(&buf[2]));
    auto ihl = buf[0] & 0xf;
    auto header_len = static_cast<uint16_t>(ihl * 4);
    auto blen = static_cast<uint16_t>(total_len - header_len);

#if DEBUG_IP4
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "net: ip: version: %u, protocol: %u, src: %s, dest: %s\n",
            version, protocol, src.ToString().c_str(), dest.ToString().c_str());
    }
#endif

    bool is_us = false;
    if(dest.get() == 0xffffffff)
    {
        is_us = true;
    }
    else if(ipcache.find(dest) != ipcache.end())
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

IP4Addr::IP4Addr(const char *_v)
{
    v = *reinterpret_cast<const uint32_t *>(_v);
}

void IP4Addr::ToString(char *buf) const
{
    sprintf(buf, "%" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32, 
        v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
}

std::string IP4Addr::ToString() const
{
    char buf[16];
    ToString(buf);
    return std::string(buf);
}

IP4Addr::operator uint32_t() const
{
    return v;
}

uint32_t IP4Addr::get() const
{
    return v;
}

IP4Addr IP4Address::Broadcast() const
{
    return addr | (~nm);
}

int net_set_ip_address(const IP4Address &addr)
{
    net_msg m;
    m.msg_type = net_msg::net_msg_type::SetIPAddress;
    m.msg_data.ipaddr = addr;
    return net_queue_msg(m);
}

void net_ip_handle_set_ip_address(const net_msg &m)
{
    CriticalGuard cg(s_ips);
    ourips.push_back(m.msg_data.ipaddr);
}

size_t net_ip_get_addresses(IP4Address *out, size_t nout)
{
    CriticalGuard cg(s_ips);
    size_t ret = 0;
    for(const auto &ip : ourips)
    {
        if(ret >= nout)
            return ret;
        
        out[ret++] = ip;
    }
    return ret;
}

IP4Addr net_ip_get_address(const NetInterface *iface)
{
    CriticalGuard cg(s_ips);
    for(const auto &ip : ourips)
    {
        if(ip.iface == iface)
        {
            return ip.addr;
        }
    }
    return IP4Addr(0UL);
}

bool net_ip_decorate_packet(char *data, size_t datalen,
    const IP4Addr &dest, const IP4Addr &src,
    uint8_t protocol,
    bool release_buffer,
    const IP4Route *route)
{
    IP4Route calcroute;
    auto _route = route;
    // get route for dest
    if(!route)
    {
        if(net_ip_get_route_for_address(dest, &calcroute) != NET_OK)
        {
            return false;
        }
        _route = &calcroute;
    }

    // if src is broadcast then rewrite with the one from route
    auto src_val = src;
    if(src == 0UL)
    {
        src_val = _route->addr.addr;
    }

    auto hdr = data - 20;
    hdr[0] = (4UL << 4) | 5UL;
    hdr[1] = 0;
    *reinterpret_cast<uint16_t *>(&hdr[2]) = htons(datalen + 20);
    *reinterpret_cast<uint16_t *>(&hdr[4]) = 0;     // ID
    *reinterpret_cast<uint16_t *>(&hdr[6]) = 0;     // fragments
    hdr[8] = 64UL;
    hdr[9] = protocol;
    *reinterpret_cast<uint16_t *>(&hdr[10]) = 0;    // checksum
    *reinterpret_cast<uint32_t *>(&hdr[12]) = src_val;
    *reinterpret_cast<uint32_t *>(&hdr[16]) = dest;

    *reinterpret_cast<uint16_t *>(&hdr[10]) = net_ip_calc_checksum(hdr, 20);

    net_ip_get_hardware_address_and_send(hdr, datalen + 20, dest, release_buffer, _route);

    return true;
}

int net_ip_get_hardware_address_and_send(char *data, size_t datalen,
    const IP4Addr &dest,
    bool release_buffer,
    const IP4Route *route)
{
    // do we need to use a gateway?
    IP4Addr actdest = (route->addr.addr == route->addr.gw) ? dest : route->addr.gw;

    HwAddr hwaddr;
    if(net_ip_get_hardware_address(actdest, &hwaddr) == NET_OK)
    {
        // decorate and send direct
        route->addr.iface->SendEthernetPacket(data, datalen, hwaddr, IPPROTO_IP, release_buffer);
        return NET_OK;
    }
    else
    {
        // send arp request with promise to send
        net_msg msg;
        msg.msg_type = net_msg::net_msg_type::ArpRequestAndSend;
        msg.msg_data.arp_request.addr = actdest;
        msg.msg_data.arp_request.buf = data;
        msg.msg_data.arp_request.n = datalen;
        msg.msg_data.arp_request.iface = route->addr.iface;
        msg.msg_data.arp_request.release_buffer = release_buffer;

        net_queue_msg(msg);

        return NET_DEFER;
    }
}

int net_ip_add_route(const IP4Route &route)
{
    CriticalGuard cg(s_routes);
    routes.push_back(route);
    return NET_OK;
}

static int net_ip_get_route_for_address_int(const IP4Addr &addr, IP4Route *route)
{
    const IP4Route *best_route = nullptr;
    int cur_metric = std::numeric_limits<int>::max();

    for(const auto &r : routes)
    {
        if(IP4Addr::Compare(addr, r.addr.addr, r.addr.nm) && r.metric < cur_metric)
        {
            cur_metric = r.metric;
            best_route = &r;
        }
    }

    if(!best_route)
    {
        return NET_NOROUTE;
    }

    if(best_route->addr.gw == 0UL)
    {
        // found a direct route.  Set the target as the gateway in ret
        route->metric = cur_metric;
        route->addr.iface = best_route->addr.iface;
        route->addr.addr = net_ip_get_address(route->addr.iface);
        route->addr.gw = addr;
        route->addr.nm = best_route->addr.nm;
        return NET_OK;
    }
    else
    {
        // find the route to the gateway
        return net_ip_get_route_for_address_int(best_route->addr.gw, route);
    }
}

int net_ip_get_route_for_address(const IP4Addr &addr, IP4Route *route)
{
    // parse routing table, resolving gateways as we go
    CriticalGuard cg(s_routes);
    return net_ip_get_route_for_address_int(addr, route);
}

uint32_t net_ip_calc_partial_checksum(const char *data, size_t n, uint32_t csum)
{
    auto d = reinterpret_cast<const uint16_t *>(data);

    while(n > 1)
    {
        csum += *d++;
        n -= 2;
    }
    if(n)
    {
        csum += (*d) & htons(0xff00);
    }
    return csum;
}

uint16_t net_ip_complete_checksum(uint32_t csum)
{
    // add carry bits
    while(csum >> 16)
    {
        csum = (csum & 0xffff) + (csum >> 16);
    }
    // 1s complement
    csum = ~csum;
    return static_cast<uint16_t>(csum & 0xffff);
}

uint16_t net_ip_calc_checksum(const char *data, size_t n)
{
    return net_ip_complete_checksum(
        net_ip_calc_partial_checksum(data, n)
    );
}

void IP4Socket::HandleWaitingReads()
{
    // we should have the spinlock at this point

    // we need to match a packet with an incoming request
    //  request size can be bigger than a packet, the same or less
    //  for dgrams, we only receive at most one packet

    read_waiting_thread rwt;
    while(read_waiting_threads.Peek(&rwt) && !recv_packets.empty())
    {
        size_t nread = 0L;

        IP4Addr from;
        uint16_t from_port;

        recv_packet rp;
        while(recv_packets.Peek(&rp))
        {
            auto pkt_size = rp.nlen - rp.rptr;
            auto buf_addr = &reinterpret_cast<char *>(rwt.buf)[nread];
            auto pkt_addr = &rp.buf[rp.rptr];
            auto buf_size = rwt.n - nread;

            auto to_read = std::min(pkt_size, buf_size);

            memcpy(buf_addr, pkt_addr, to_read);

            from = rp.from;
            from_port = rp.from_port;

            rp.rptr += to_read;
            if(rp.rptr >= rp.nlen)
            {
                // can remove buf
                net_deallocate_pbuf(const_cast<char *>(rp.buf));
                recv_packets.Read(&rp);
            }

            nread += to_read;

            if(nread >= rwt.n)
            {
                break;
            }

            if(is_dgram)
                break;  // only one packet, else we may conflate packets from different peers
        }

        if(nread)
        {
            // can return something to the waiting thread

            if(rwt.srcaddr && rwt.addrlen && *rwt.addrlen >= sizeof(sockaddr_in))
            {
                auto addr = reinterpret_cast<sockaddr_in *>(rwt.srcaddr);
                addr->sin_family = AF_INET;
                addr->sin_addr.s_addr = from;
                addr->sin_port = from_port;
                *rwt.addrlen = sizeof(sockaddr_in);
            }

            rwt.t->ss_p.uval1 = nread;
            rwt.t->ss.Signal();

            // remove this request
            read_waiting_threads.Read(&rwt);
        }
    }
}