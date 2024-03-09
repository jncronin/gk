#include <osnet.h>
#include <unordered_map>
#include "thread.h"
#include "osqueue.h"
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

SRAM4_DATA static std::unordered_map<sockaddr_in, UDPSocket *> bound_udp_sockets;
SRAM4_DATA static Spinlock s_udp;

int net_handle_udp_packet(const IP4Packet &pkt)
{
    // todo: checksum
    auto src_port = *reinterpret_cast<const uint16_t *>(&pkt.contents[0]);
    auto dest_port = *reinterpret_cast<const uint16_t *>(&pkt.contents[2]);
    auto len = ntohs(*reinterpret_cast<const uint16_t *>(&pkt.contents[4])) - 8;
    auto data = reinterpret_cast<const char *>(&pkt.contents[8]);

    // do we match a bound socket?
    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = pkt.dest;
    saddr.sin_port = dest_port;

    UDPSocket *sck = nullptr;

    {
        CriticalGuard cg(s_udp);
        auto fval = bound_udp_sockets.find(saddr);
        if(fval == bound_udp_sockets.end())
        {
            // try with matching IPADDR_ANY
            saddr.sin_addr.s_addr = 0UL;
            fval = bound_udp_sockets.find(saddr);
            if(fval == bound_udp_sockets.end())
            {
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
        if(!sck)
        {
            return NET_NOTUS;
        }
    }

    return sck->HandlePacket(data, len, pkt.src, src_port);
}

int UDPSocket::BindAsync(const sockaddr *addr, socklen_t addrlen, int *_errno)
{
    if(addrlen != sizeof(sockaddr_in))
    {
        *_errno = EINVAL;
        return -1;
    }

    auto saddr = *reinterpret_cast<const sockaddr_in *>(addr);
    port = saddr.sin_port;

    if(saddr.sin_family != AF_INET)
    {
        *_errno = EINVAL;
        return -1;
    }

    {
        CriticalGuard cg(s_udp);

        // first check the IPADDR_ANY address is not bound for this port
        if(saddr.sin_addr.s_addr)
        {
            sockaddr_in any_addr = saddr;
            any_addr.sin_addr.s_addr = 0;
            if(bound_udp_sockets.find(any_addr) != bound_udp_sockets.end())
            {
                *_errno = EADDRINUSE;
                return -1;
            }
        }
        if(bound_udp_sockets.find(saddr) != bound_udp_sockets.end())
        {
            // already bound
            // TODO: handle REUSEADDR/REUSEPORT
            *_errno = EADDRINUSE;
            return -1;
        }

        bound_udp_sockets[saddr] = this;
        bound_addr = saddr.sin_addr.s_addr;

        return 0;
    }
}

int UDPSocket::HandlePacket(const char *pkt, size_t n, uint32_t src_addr, uint16_t src_port)
{
    CriticalGuard cg(sl);

    recv_packet rp;
    rp.buf = pkt;
    rp.nlen = n;
    rp.rptr = 0;
    rp.from = src_addr;
    rp.from_port = src_port;

    if(recv_packets.Write(rp))
    {
        HandleWaitingReads();
        return NET_KEEPPACKET;
    }
    else
    {
        return NET_NOMEM;
    }
}

int UDPSocket::RecvFromAsync(void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen, int *_errno)
{
    CriticalGuard cg(sl);

    read_waiting_thread rwt;
    rwt.t = GetCurrentThreadForCore();
    rwt.buf = buf;
    rwt.n = len;
    rwt.srcaddr = src_addr;
    rwt.addrlen = addrlen;

    if(!read_waiting_threads.Write(rwt))
    {
        *_errno = ENOMEM;
        return -1;
    }
    else
    {
        net_msg msg;
        msg.msg_type = net_msg::net_msg_type::HandleWaitingReads;
        msg.msg_data.ipsck = this;
        net_queue_msg(msg);
        return -2;
    }
}

int UDPSocket::SendToAsync(const void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, socklen_t addrlen, int *_errno)
{
    if(!dest_addr)
    {
        *_errno = EINVAL;
        return -1;
    }
    if(addrlen < sizeof(sockaddr_in))
    {
        *_errno = EINVAL;
        return -1;
    }
    if(len >= PBUF_SIZE - NET_SIZE_UDP)
    {
        *_errno = EMSGSIZE;
        return -1;
    }

    net_msg msg;
    msg.msg_type = net_msg::net_msg_type::UDPSendDgram;
    msg.msg_data.udpsend.buf = (char *)buf;
    msg.msg_data.udpsend.n = len;
    msg.msg_data.udpsend.flags = flags;
    msg.msg_data.udpsend.dest_addr = (sockaddr_in *)dest_addr;
    msg.msg_data.udpsend.addrlen = addrlen;
    msg.msg_data.udpsend.t = GetCurrentThreadForCore();
    msg.msg_data.udpsend.sck = this;

    auto qret = net_queue_msg(msg);
    if(qret != NET_OK)
    {
        *_errno = net_ret_to_errno(qret);
        return -1;
    }

    return -2;
}

bool UDPSocket::SendToInt(const net_msg &m)
{
    // store to a packet
    CriticalGuard cg(sl);

    const auto &msg = m.msg_data.udpsend;
    auto pbuf = net_allocate_pbuf(msg.n + NET_SIZE_UDP);
    if(!pbuf)
    {
        // deferred return
        msg.t->ss_p.ival1 = -1;
        msg.t->ss_p.ival2 = ENOMEM;
        msg.t->ss.Signal();
        return false;
    }

    auto data = &pbuf[NET_SIZE_UDP_OFFSET];
    memcpy(data, msg.buf, msg.n);

    auto sret = net_udp_decorate_packet(data, msg.n,
        msg.dest_addr->sin_addr.s_addr, msg.dest_addr->sin_port,
        bound_addr, port, true);
    if(sret)
    {
        msg.t->ss_p.uval1 = msg.n;
        msg.t->ss.Signal();
    }
    else
    {
        msg.t->ss_p.ival1 = -1;
        msg.t->ss_p.ival2 = EFAULT;
        msg.t->ss.Signal();
    }
    return sret;        
}

void net_udp_handle_sendto(const net_msg &m)
{
    m.msg_data.udpsend.sck->SendToInt(m);
}

bool net_udp_decorate_packet(char *data, size_t datalen, 
    const IP4Addr &dest, uint16_t dest_port,
    const IP4Addr &src, uint16_t src_port,
    bool release_buffer,
    const IP4Route *route)
{
    auto hdr = data - 8;
    *reinterpret_cast<uint16_t *>(&hdr[0]) = src_port;
    *reinterpret_cast<uint16_t *>(&hdr[2]) = dest_port;
    *reinterpret_cast<uint16_t *>(&hdr[4]) = htons(8 + datalen);
    *reinterpret_cast<uint16_t *>(&hdr[6]) = 0;     // checksum optional for udp in ip4

    return net_ip_decorate_packet(hdr, datalen + 8, dest, src, IPPROTO_UDP, release_buffer, route);
}
