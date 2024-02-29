#include <osnet.h>
#include <unordered_map>
#include "thread.h"
#include "osqueue.h"

SRAM4_DATA static std::unordered_map<sockaddr_in, UDPSocket *> bound_udp_sockets;
SRAM4_DATA static Spinlock s_udp;

int net_handle_udp_packet(const IP4Packet &pkt)
{
    // todo: checksum
    auto src_port = *reinterpret_cast<const uint16_t *>(&pkt.contents[0]);
    auto dest_port = *reinterpret_cast<const uint16_t *>(&pkt.contents[2]);
    auto len = *reinterpret_cast<const uint16_t *>(&pkt.contents[4]) - 8;
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
            return NET_NOTUS;
        }
        sck = fval->second;
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

        return 0;
    }
}

int UDPSocket::HandlePacket(const char *pkt, size_t n)
{
    return -1;
}

int UDPSocket::HandlePacket(const char *pkt, size_t n, uint32_t src_addr, uint16_t src_port)
{
    CriticalGuard cg(sl);
    auto avail_space = (recv_rptr > recv_wptr) ? (recv_rptr - recv_wptr) : (GK_NET_SOCKET_BUFSIZE - (recv_wptr - recv_rptr));
    if(avail_space < n)
        return NET_NOMEM;

    auto old_rwptr = recv_wptr;
    
    dgram_desc dd;
    dd.from.sin_family = AF_INET;
    dd.from.sin_addr.s_addr = src_addr;
    dd.from.sin_port = src_port;
    dd.len = n;
    dd.start = recv_wptr;
    recv_wptr = memcpy_split_dest(recvbuf, pkt, n, recv_wptr, GK_NET_SOCKET_BUFSIZE);

    if(!dgram_queue.Write(dd))
    {
        recv_wptr = old_rwptr;
        return NET_NOMEM;
    }

    // now see if we need to give the dgram to a thread
    net_msg msg;
    if(udp_waiting_queue.Read(&msg))
    {
        RecvFromInt(msg, dd);
    }

    return NET_OK;
}

int UDPSocket::SendData(const char *d, size_t n, const void *addr, size_t addrlen)
{
    return -1;
}

int UDPSocket::RecvData(char *d, size_t n, void *addr, size_t addrlen, SimpleSignal &ss)
{
    return -1;
}

int UDPSocket::RecvFromAsync(void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen, int *_errno)
{
    net_msg msg;
    msg.msg_type = net_msg::net_msg_type::UDPRecvDgram;
    msg.msg_data.udprecv.buf = (char *)buf;
    msg.msg_data.udprecv.n = len;
    msg.msg_data.udprecv.flags = flags;
    msg.msg_data.udprecv.src_addr = (sockaddr_in *)src_addr;
    msg.msg_data.udprecv.addrlen = addrlen;
    msg.msg_data.udprecv.t = GetCurrentThreadForCore();
    msg.msg_data.udprecv.sck = this;

    auto qret = net_queue_msg(msg);
    if(qret != NET_OK)
    {
        *_errno = net_ret_to_errno(qret);
        return -1;
    }

    return -2;
}

bool UDPSocket::RecvFromInt(const net_msg &m)
{
    // If we can immediately return a dgram then do so
    CriticalGuard cg(sl);
    dgram_desc dd;
    if(dgram_queue.Read(&dd))
    {
        RecvFromInt(m, dd);
        return true;
    }
    else
    {
        return udp_waiting_queue.Write(m);
    }
}

void UDPSocket::RecvFromInt(const net_msg &m, dgram_desc &dd)
{
    auto len = std::min(m.msg_data.udprecv.n, dd.len);
    recv_rptr = memcpy_split_src(m.msg_data.udprecv.buf, recvbuf, len, dd.start, GK_NET_SOCKET_BUFSIZE);
    if(m.msg_data.udprecv.src_addr && m.msg_data.udprecv.addrlen)
    {
        auto addrlen = std::min(*m.msg_data.udprecv.addrlen, (socklen_t)sizeof(sockaddr_in));
        memcpy(m.msg_data.udprecv.src_addr, &dd.from, addrlen);
        *m.msg_data.udprecv.addrlen = addrlen;
    }
    m.msg_data.udprecv.t->ss_p.ival1 = len;
    m.msg_data.udprecv.t->ss.Signal();
}

void net_udp_handle_recvfrom(const net_msg &m)
{
    m.msg_data.udprecv.sck->RecvFromInt(m);
}
