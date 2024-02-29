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
}

const HwAddr &TUSBNetInterface::GetHwAddr() const
{
    return our_hwaddr;
}

bool TUSBNetInterface::GetLinkActive() const
{
    return is_up;
}

int TUSBNetInterface::SendEthernetPacket(const char *buf, size_t n)
{
    if(!is_up)
    {
        return -1;
    }

    if(tud_network_can_xmit(n))
    {
        tud_network_xmit(const_cast<char *>(buf), n);
        return static_cast<int>(n);
    }
    else
    {
        return -1;
    }
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    memcpy(dst, ref, arg);
    return arg;
}
