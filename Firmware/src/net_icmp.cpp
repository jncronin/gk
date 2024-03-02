#include "osnet.h"
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

int net_handle_icmp_packet(const IP4Packet &pkt)
{
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "net: received ICMP %s -> %s, type %d code %d checksum %d id %d seq %d\n",
            IP4Addr(pkt.src).ToString().c_str(),
            IP4Addr(pkt.dest).ToString().c_str(),
            pkt.contents[0], pkt.contents[1],
            *reinterpret_cast<const uint16_t *>(&pkt.contents[2]),
            *reinterpret_cast<const uint16_t *>(&pkt.contents[4]),
            *reinterpret_cast<const uint16_t *>(&pkt.contents[6]));

        SEGGER_RTT_printf(0, "net: received packet:\n");
        for(int i = 0; i < (int)pkt.epacket.n + 18; i++)
        {
            SEGGER_RTT_printf(0, "%02X ", pkt.epacket.contents[i - 14]);
        }
        SEGGER_RTT_printf(0, "\n");
    }

    IP4Route route;
    int ret = NET_OK;
    auto route_ret = net_ip_get_route_for_address(pkt.src, &route);

    if(route_ret != NET_OK)
    {
        ret = route_ret;
    }
    else
    {
        // we have a valid dgram, try and build a packet
        auto hdr_size = 20 /* IP header */ + route.addr.iface->GetHeaderSize();
        auto pkt_size = hdr_size + 8 /* icmp size */ + route.addr.iface->GetFooterSize();

        if(pkt_size > PBUF_SIZE)
        {
            ret = NET_MSGSIZE;
        }
        else
        {
            // allocate a pbuf
            auto pbuf = net_allocate_pbuf();
            if(!pbuf)
            {
                ret = NET_NOMEM;
            }
            else
            {
                // copy data to pbuf
                auto psend = &pbuf[hdr_size];
                psend[0] = 0;   // reply
                psend[1] = 0;   // code
                *reinterpret_cast<uint16_t *>(&psend[2]) = 0;   // checksum
                *reinterpret_cast<uint16_t *>(&psend[4]) =
                    *reinterpret_cast<const uint16_t *>(&pkt.contents[4]);  // id
                *reinterpret_cast<uint16_t *>(&psend[6]) =
                    *reinterpret_cast<const uint16_t *>(&pkt.contents[6]);  // seq_id

                // 1s complement checksum
                *reinterpret_cast<uint16_t *>(&psend[2]) = net_ip_calc_checksum(psend, 8);

                net_ip_decorate_packet(psend, 8, pkt.src, pkt.dest, IPPROTO_ICMP);
            }
        }
    }

    return ret;
}
