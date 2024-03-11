#include "osnet.h"
#include <map>
#include "clocks.h"
#include "SEGGER_RTT.h"

struct arp_request_and_send_data
{
    uint32_t ip;
    char *buf;
    size_t n;
    uint64_t send_time;
    NetInterface *iface;
    bool release_buffer;
};

SRAM4_DATA std::map<uint32_t, HwAddr> arp_cache;
SRAM4_DATA std::vector<arp_request_and_send_data> arp_requests;

SRAM4_DATA static Spinlock s_arp;

extern Spinlock s_rtt;

int net_arp_add_host(const IP4Addr &ip, const HwAddr &hw)
{
    CriticalGuard cg(s_arp);
    arp_cache[ip] = hw;
    return NET_OK;
}

int net_handle_arp_packet(const EthernetPacket &pkt)
{
    // handle incoming packet of type arp request or reply

    if(*reinterpret_cast<const uint16_t *>(&pkt.contents[0]) != htons(1))
        return NET_NOTSUPP;
    if(*reinterpret_cast<const uint16_t *>(&pkt.contents[2]) != htons(IPPROTO_IP))
        return NET_NOTSUPP;
    if(pkt.contents[4] != 6)
        return NET_NOTSUPP;
    if(pkt.contents[5] != 4)
        return NET_NOTSUPP;

    HwAddr hw_sender(&pkt.contents[8]);
    IP4Addr ip_sender(&pkt.contents[14]);
    HwAddr hw_target(&pkt.contents[18]);
    IP4Addr ip_target(&pkt.contents[24]);
    auto oper = ntohs(*reinterpret_cast<const uint16_t *>(&pkt.contents[6]));

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "net: arp packet %s %s(%s) -> %s(%s) on iface %s\n",
            (oper == 1) ? "request" : ((oper == 2) ? "reply" : "unknown"),
            ip_sender.ToString().c_str(),
            hw_sender.ToString().c_str(),
            ip_target.ToString().c_str(),
            hw_target.ToString().c_str(),
            net_ip_get_address(pkt.iface).ToString().c_str());
    }
    
    {
        CriticalGuard cg(s_arp);
        // regardless of whether we made the request, cache what we receive
        if(ip_sender != 0UL && ip_sender != 0xffffffffUL)
            arp_cache[ip_sender] = hw_sender;
        
        // if this is a reply, and we have the request, deal with any pending send packets
        if(oper == 2 && hw_target == pkt.iface->GetHwAddr())
        {
            auto iter = arp_requests.begin();
            while(iter != arp_requests.end())
            {
                if(iter->ip == ip_sender)
                {
                    iter->iface->SendEthernetPacket(iter->buf, iter->n, hw_sender, IPPROTO_IP, iter->release_buffer);
                    iter = arp_requests.erase(iter);
                }
                else
                {
                    iter++;
                }
            }
        }
        else if(oper == 1 && ip_target == net_ip_get_address(pkt.iface))
        {
            // build reply packet
            auto pbuf = net_allocate_pbuf(SPBUF_SIZE);
            if(!pbuf)
            {
                return NET_NOMEM;
            }

            auto data = &pbuf[pkt.iface->GetHeaderSize()];
            *reinterpret_cast<uint16_t *>(&data[0]) = htons(1);    // ethernet
            *reinterpret_cast<uint16_t *>(&data[2]) = htons(IPPROTO_IP);
            data[4] = 6;
            data[5] = 4;
            *reinterpret_cast<uint16_t *>(&data[6]) = htons(2);    // reply
            memcpy(&data[8], pkt.iface->GetHwAddr().get(), 6);
            *reinterpret_cast<uint32_t *>(&data[14]) = net_ip_get_address(pkt.iface);
            memcpy(&data[18], hw_sender.get(), 6);
            *reinterpret_cast<uint32_t *>(&data[24]) = ip_sender;

            pkt.iface->SendEthernetPacket(data, 28, hw_sender, 0x0806, true);
        }
    }
    
    return NET_NOTSUPP;
}

static int net_ip_get_hardware_address_int(const IP4Addr &addr, HwAddr *ret)
{
    if(addr == 0xffffffffUL)
    {
        *ret = HwAddr::multicast;
        return NET_OK;
    }
    if(auto ptr = arp_cache.find(addr); ptr != arp_cache.end())
    {
        *ret = ptr->second;
        return NET_OK;
    }
    else
    {
        return NET_DEFER;
    }
}

void net_arp_handle_request_and_send(const net_msg &m)
{
    // check we haven't received a result in the meantime
    CriticalGuard cg(s_arp);
    HwAddr ret;
    if(net_ip_get_hardware_address_int(m.msg_data.arp_request.addr, &ret) == NET_OK)
    {
        cg.~CriticalGuard();        // release the guard here because sending may take some time
        m.msg_data.arp_request.iface->SendEthernetPacket(m.msg_data.arp_request.buf,
            m.msg_data.arp_request.n, ret, IPPROTO_IP, m.msg_data.arp_request.release_buffer);
        return;
    }

    // build ARP request
    auto pbuf = net_allocate_pbuf(SPBUF_SIZE);
    if(!pbuf)
    {
        return;
    }

    auto data = &pbuf[m.msg_data.arp_request.iface->GetHeaderSize()];
    *reinterpret_cast<uint16_t *>(&data[0]) = htons(1);    // ethernet
    *reinterpret_cast<uint16_t *>(&data[2]) = htons(IPPROTO_IP);
    data[4] = 6;
    data[5] = 4;
    *reinterpret_cast<uint16_t *>(&data[6]) = htons(1);    // request
    memcpy(&data[8], m.msg_data.arp_request.iface->GetHwAddr().get(), 6);
    *reinterpret_cast<uint32_t *>(&data[14]) = net_ip_get_address(m.msg_data.arp_request.iface);
    memset(&data[18], 0, 6);
    *reinterpret_cast<uint32_t *>(&data[24]) = m.msg_data.arp_request.addr;

    m.msg_data.arp_request.iface->SendEthernetPacket(data, 28, HwAddr::multicast, 0x0806, true);

    // queue that we have made the request
    arp_request_and_send_data arpd;
    arpd.buf = m.msg_data.arp_request.buf;
    arpd.n = m.msg_data.arp_request.n;
    arpd.ip = m.msg_data.arp_request.addr;
    arpd.iface = m.msg_data.arp_request.iface;
    arpd.send_time = clock_cur_ms();
    arpd.release_buffer = m.msg_data.arp_request.release_buffer;
    arp_requests.push_back(arpd);
}

int net_ip_get_hardware_address(const IP4Addr &addr, HwAddr *ret)
{
    if(addr == 0xffffffffUL)
    {
        *ret = HwAddr::multicast;
        return NET_OK;
    }

    {
        CriticalGuard cg(s_arp);
        return net_ip_get_hardware_address_int(addr, ret);
    }
}

void net_arp_handle_timeouts()
{
    CriticalGuard cg(s_arp);
    auto iter = arp_requests.begin();
    while(iter != arp_requests.end())
    {
        if(clock_cur_ms() > iter->send_time + 4000)
        {
            // delete request + send_data pbuf
            net_deallocate_pbuf(iter->buf);
            iter = arp_requests.erase(iter);
            // TODO: report to socket that ARP request failed - likely just close connection
        }
        else
        {
            iter++;
        }
    }
}
