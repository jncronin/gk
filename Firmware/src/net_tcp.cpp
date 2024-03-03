#include "osnet.h"
#include <unordered_map>
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

#define FLAG_FIN    0x1
#define FLAG_SYN    0x2
#define FLAG_RST    0x4
#define FLAG_PSH    0x8
#define FLAG_ACK    0x10
#define FLAG_URG    0x20
#define FLAG_ECE    0x40
#define FLAG_CWR    0x80

SRAM4_DATA static std::unordered_map<sockaddr_in, TCPSocket *> listening_sockets;
SRAM4_DATA static std::unordered_map<sockaddr_pair, TCPSocket *> connected_sockets;
SRAM4_DATA static Spinlock s_tcp;

static void net_tcp_send_reset(const IP4Addr &dest, uint16_t port,
    const IP4Addr &src, uint16_t src_port);

int net_handle_tcp_packet(const IP4Packet &pkt)
{
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "net: tcp: received packet:\n");
        for(unsigned int i = 0; i < pkt.epacket.link_layer_n; i++)
        {
            SEGGER_RTT_printf(0, "%02X ", pkt.epacket.link_layer_data[i]);
        }
        SEGGER_RTT_printf(0, "\n");
    }

    auto src_port = *reinterpret_cast<const uint16_t *>(&pkt.contents[0]);
    auto dest_port = *reinterpret_cast<const uint16_t *>(&pkt.contents[2]);
    auto seq_id = ntohl(*reinterpret_cast<const uint32_t *>(&pkt.contents[4]));
    auto ack_id = ntohl(*reinterpret_cast<const uint32_t *>(&pkt.contents[8]));
    auto hlen = ((pkt.contents[12] >> 4) & 0xfU) * 4;
    auto flags = pkt.contents[13];

    sockaddr_pair sp;
    sp.src.sin_family = AF_INET;
    sp.src.sin_addr.s_addr = pkt.src;
    sp.src.sin_port = src_port;
    sp.dest.sin_family = AF_INET;
    sp.dest.sin_addr.s_addr = pkt.dest;
    sp.dest.sin_port = dest_port;

    auto data = reinterpret_cast<const char *>(&pkt.contents[hlen]);
    auto len = pkt.n - hlen;

    auto opts = reinterpret_cast<const char *>(&pkt.contents[20]);
    auto optlen = hlen - 20;

    // find a listening or connected socket
    TCPSocket *sck = nullptr;

    {
        CriticalGuard cg(s_tcp);
        // handle syn requests - incoming ones are looking for a
        //  socket in the listening state
        if(flags & FLAG_SYN)
        {
            auto fval = listening_sockets.find(sp.dest);
            if(fval == listening_sockets.end())
            {
                // try with matching IPADDR_ANY
                sp.dest.sin_addr.s_addr = 0UL;
                fval = listening_sockets.find(sp.dest);
                if(fval == listening_sockets.end())
                {
                    net_tcp_send_reset(IP4Addr(pkt.src), src_port, IP4Addr(pkt.dest), dest_port);
                    return NET_NOTUS;
                }
                else
                {
                    sck = fval->second;
                }
            }
            else
            {
                sck = fval->second;
            }
        }
        else
        {
            // not syn, therefore look for connected socket
            auto fval = connected_sockets.find(sp);
            if(fval == connected_sockets.end())
            {
                // try with matching IPADDR_ANY
                sp.dest.sin_addr.s_addr = 0UL;
                fval = connected_sockets.find(sp);
                if(fval == connected_sockets.end())
                {
                    // silently drop
                    return NET_NOTUS;
                }
                else
                {
                    sck = fval->second;
                }
            }
            else
            {
                sck = fval->second;
            }
        }
    }

    return sck->HandlePacket(data, len, IP4Addr(pkt.src), src_port,
        IP4Addr(pkt.dest), dest_port, seq_id, ack_id, flags,
        opts, optlen);
}

static void net_tcp_send_reset(const IP4Addr &dest, uint16_t port,
    const IP4Addr &src, uint16_t src_port)
{
    IP4Route route;
    auto route_ret = net_ip_get_route_for_address(dest, &route);
    if(route_ret != NET_OK)
        return;
    
    auto pbuf = net_allocate_pbuf();
    if(!pbuf)
        return;

    auto hdr_size = 20 /* TCP header */ + 20 /* IP header */ + route.addr.iface->GetHeaderSize();

    net_tcp_decorate_packet(&pbuf[hdr_size], 0, dest, port, src, src_port,
        0, 0, FLAG_RST, nullptr, 0, nullptr);    
}

int TCPSocket::HandlePacket(const char *pkt, size_t n,
            IP4Addr src, uint16_t src_port,
            IP4Addr ddest, uint16_t dest_port,
            uint32_t seq_id, uint32_t ack_id,
            unsigned int flags,
            const char *opts, size_t optlen)
{
    return NET_NOTSUPP;
}

int TCPSocket::GetWindowSize() const
{
    return 0;
}

bool net_tcp_decorate_packet(char *data, size_t datalen,
    const IP4Addr &dest, uint16_t dest_port,
    const IP4Addr &src, uint16_t src_port,
    uint32_t seq_id, uint32_t ack_id,
    unsigned int flags,
    const char *opts, size_t optlen,
    TCPSocket *sck)
{
    auto actoptlen = ((optlen + 3) / 4) * 4;
    auto hdr = data - 20 - actoptlen;
    *reinterpret_cast<uint16_t *>(&hdr[0]) = src_port;
    *reinterpret_cast<uint16_t *>(&hdr[2]) = dest_port;
    *reinterpret_cast<uint32_t *>(&hdr[4]) = htonl(seq_id);
    *reinterpret_cast<uint32_t *>(&hdr[8]) = htonl(ack_id);

    auto data_offset = (20 + actoptlen) / 4;
    hdr[12] = static_cast<uint8_t>((data_offset << 4) & 0xffU);
    hdr[13] = flags;

    auto wnd_size = sck ? sck->GetWindowSize() : 0;
    *reinterpret_cast<uint16_t *>(&hdr[14]) = htons(wnd_size);

    *reinterpret_cast<uint16_t *>(&hdr[16]) = 0;    // checksum
    *reinterpret_cast<uint16_t *>(&hdr[18]) = 0;    // urgent pointer

    if(opts)
    {
        memcpy(&hdr[20], opts, optlen);
        if(optlen != actoptlen)
            memset(&hdr[20 + optlen], 0, actoptlen - optlen);
    }

    // calculate checksum based upon TCP header, data and IP pseudo-header
    char ipph[12];
    *(reinterpret_cast<uint32_t *>(&ipph[0])) = src;
    *(reinterpret_cast<uint32_t *>(&ipph[4])) = dest;
    ipph[8] = 0;
    ipph[9] = IPPROTO_TCP;
    *(reinterpret_cast<uint16_t *>(&ipph[10])) = htons(20 + actoptlen + datalen);

    auto csum = net_ip_complete_checksum(
        net_ip_calc_partial_checksum(ipph, 12,
            net_ip_calc_partial_checksum(hdr, 20 + actoptlen + datalen))
    );
    *(reinterpret_cast<uint16_t *>(&hdr[16])) = csum;

    return net_ip_decorate_packet(hdr, datalen + 20 + actoptlen,
        dest, src, IPPROTO_TCP);
}