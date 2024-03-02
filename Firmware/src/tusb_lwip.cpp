#include <cstdint>
#include <cstring>
#include <tusb.h>
#include <SEGGER_RTT.h>
#include <osmutex.h>

#include "osnet.h"

extern Spinlock s_rtt;

#define LWIP_DATA __attribute__((section(".lwip_data")))
LWIP_DATA uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

LWIP_DATA TUSBNetInterface rndis_if;

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    if(size == 0)
        return true;
    if(size > PBUF_SIZE)
        return true;
    
    auto pbuf = net_allocate_pbuf();
    if(!pbuf)
        return false;
    
    memcpy(pbuf, src, size);
    net_inject_ethernet_packet(pbuf, size, &rndis_if);
    tud_network_recv_renew();

    return true;
}

void tud_network_init_cb(void)
{
    rndis_if.peer_hwaddr = HwAddr(reinterpret_cast<const char *>(tud_network_mac_address));
    char our_addr[6];
    memcpy(our_addr, tud_network_mac_address, 6);
    our_addr[5] = ~our_addr[5];
    rndis_if.our_hwaddr = HwAddr(our_addr);
    rndis_if.is_up = true;

    IP4Address rndis_ip;
    rndis_ip.addr = IP4Addr(0x0107a8c0);
    rndis_ip.gw = IP4Addr(0UL);
    rndis_ip.iface = &rndis_if;
    rndis_ip.nm = IP4Addr(0x00ffffff);

    IP4Route def_route;
    def_route.metric = 10;
    def_route.addr.addr = IP4Addr(0x0007a8c0);
    def_route.addr.gw = IP4Addr(0UL);
    def_route.addr.iface = &rndis_if;
    def_route.addr.nm = IP4Addr(0xffffffff);
    net_ip_add_route(def_route);

    IP4Route bcast_route;
    bcast_route.metric = 10;
    bcast_route.addr.addr = IP4Addr(0xffffffff);
    bcast_route.addr.gw = IP4Addr(0UL);
    bcast_route.addr.iface = &rndis_if;
    bcast_route.addr.nm = IP4Addr(0xffffffffUL);
    net_ip_add_route(bcast_route);

    net_set_ip_address(rndis_ip);
}

const HwAddr &TUSBNetInterface::GetHwAddr() const
{
    return our_hwaddr;
}

bool TUSBNetInterface::GetLinkActive() const
{
    return is_up;
}

int TUSBNetInterface::GetFooterSize() const
{
    return 4;
}

int TUSBNetInterface::GetHeaderSize() const
{
    return 14;
}

int TUSBNetInterface::SendEthernetPacket(char *buf, size_t n, const HwAddr &dest, uint16_t ethertype)
{
    if(!is_up)
    {
        return -1;
    }

    if(tud_network_can_xmit(n))
    {
        // decorate packet
        buf -= 14;
        memcpy(&buf[0], dest.get(), 6);
        memcpy(&buf[6], our_hwaddr.get(), 6);
        *reinterpret_cast<uint16_t *>(&buf[12]) = htons(ethertype);

        // compute crc
        if(n < 64)
        {
            memset(&buf[14 + n], 0, 64 - n);
            n = 64;  // min frame length
        }
        auto crc = net_ethernet_calc_crc(buf, n + 14);
        *reinterpret_cast<uint32_t *>(&buf[n + 14]) = htonl(crc);
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "send packet:\n");
            for(unsigned int i = 0; i < n + 14; i++)
                SEGGER_RTT_printf(0, "%02X ", buf[i]);
            SEGGER_RTT_printf(0, "\nchecksum: %8" PRIx32 "\n", crc);
        }
        tud_network_xmit(const_cast<char *>(buf), n + 18);
        net_deallocate_pbuf(buf);
        return static_cast<int>(n);
    }
    else
    {
        net_msg msg;
        msg.msg_type = net_msg::net_msg_type::SendPacket;
        msg.msg_data.packet.buf = buf;
        msg.msg_data.packet.iface = this;
        msg.msg_data.packet.n = n;
        msg.msg_data.packet.ethertype = ethertype;
        msg.msg_data.packet.dest = dest;
        net_queue_msg(msg);
        return NET_TRYAGAIN;
    }
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    memcpy(dst, ref, arg);
    return arg;
}
