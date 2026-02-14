#include "osnet.h"

static int net_handle_icmp_echo_request(const IP4Packet &pkt, const IP4Route *route)
{
    // build reply packet, including any data sent in the request
    auto hdr_size = NET_SIZE_IP_OFFSET;
    auto pkt_size = NET_SIZE_IP + pkt.n;

    if(pkt_size > PBUF_SIZE)
    {
        return NET_MSGSIZE;
    }
    else
    {
        // allocate a pbuf
        auto pbuf = net_allocate_pbuf(pkt_size);
        if(!pbuf)
        {
            return NET_NOMEM;
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

            memcpy(&psend[8], &pkt.contents[8], pkt.n - 8);

            // 1s complement checksum
            *reinterpret_cast<uint16_t *>(&psend[2]) = net_ip_calc_checksum(psend, pkt.n);

            return net_ip_decorate_packet(psend, pkt.n, pkt.src, pkt.dest, IPPROTO_ICMP, true, route);
        }
    }
}

int net_handle_icmp_packet(const IP4Packet &pkt)
{
    if(pkt.n < 8)
    {
        return NET_NOTSUPP;
    }

#ifdef DEBUG_ICMP
    {
        CriticalGuard cg;
        klog("net: received ICMP %s -> %s, type %d code %d checksum %d id %d seq %d\n",
            IP4Addr(pkt.src).ToString().c_str(),
            IP4Addr(pkt.dest).ToString().c_str(),
            pkt.contents[0], pkt.contents[1],
            *reinterpret_cast<const uint16_t *>(&pkt.contents[2]),
            *reinterpret_cast<const uint16_t *>(&pkt.contents[4]),
            *reinterpret_cast<const uint16_t *>(&pkt.contents[6]));

        klog("net: received packet:\n");
        for(int i = 0; i < (int)pkt.epacket.n + 18; i++)
        {
            klog("%02X ", pkt.epacket.contents[i - 14]);
        }
        klog("\n");
    }
#endif

    IP4Route route;
    auto route_ret = net_ip_get_route_for_address(pkt.src, &route);

    auto type = pkt.contents[0];

    if(route_ret != NET_OK)
    {
        return route_ret;
    }
    else if(type == 8)
    {
        // send echo reply
        return net_handle_icmp_echo_request(pkt, &route);
    }

    return NET_NOTSUPP;
}
