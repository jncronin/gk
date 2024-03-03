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

    if(!sb.Alloc())
    {
        *_errno = ENOMEM;
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

int UDPSocket::HandlePacket(const char *pkt, size_t n, uint32_t src_addr, uint16_t src_port)
{
    CriticalGuard cg(sl);
    auto avail_space = sb.AvailableRecvSpace();
    //auto avail_space = (recv_rptr > recv_wptr) ? (recv_rptr - recv_wptr) : (GK_NET_SOCKET_BUFSIZE - (recv_wptr - recv_rptr));
    if(avail_space < n)
        return NET_NOMEM;

    auto old_rwptr = sb.recv_wptr;
    
    dgram_desc dd;
    dd.from.sin_family = AF_INET;
    dd.from.sin_addr.s_addr = src_addr;
    dd.from.sin_port = src_port;
    dd.len = n;
    dd.start = sb.recv_wptr;
    sb.recv_wptr = memcpy_split_dest(sb.recvbuf, pkt, n, sb.recv_wptr, SocketBuffer::buflen);

    {
        CriticalGuard cg2(s_rtt);
        SEGGER_RTT_printf(0, "udp: incoming packet, store %d bytes to %x\n", (int)n, old_rwptr);
    }

    // now see if we need to give the dgram to a thread
    net_msg msg;
    if(udp_waiting_queue.Read(&msg))
    {
        RecvFromInt(msg, dd);
    }
    else if(!dgram_queue.Write(dd)) // store to pending queue
    {
        sb.recv_wptr = old_rwptr;
        return NET_NOMEM;
    }

    return NET_OK;
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

bool UDPSocket::SendToInt(const net_msg &m)
{
    // store to output buffer
    CriticalGuard cg(sl);

    auto avail_space = sb.AvailableSendSpace();
    if(avail_space < m.msg_data.udpsend.n)
    {
        // deferred return
        m.msg_data.udpsend.t->ss_p.ival1 = -1;
        m.msg_data.udpsend.t->ss_p.ival2 = ENOMEM;
        m.msg_data.udpsend.t->ss.Signal();
        return false;
    }

    auto old_wptr = sb.send_wptr;
    sb.send_wptr = memcpy_split_dest(sb.sendbuf, m.msg_data.udpsend.buf,
        m.msg_data.udpsend.n, sb.send_wptr, GK_NET_SOCKET_BUFSIZE);

    dgram_send_desc dd;
    dd.to = *m.msg_data.udpsend.dest_addr;
    dd.start = old_wptr;
    dd.len = m.msg_data.udpsend.n;
    dd.t = m.msg_data.udpsend.t;

    if(!dgram_send_queue.Write(dd))
    {
        sb.send_wptr = old_wptr;
        // deferred return
        m.msg_data.udpsend.t->ss_p.ival1 = -1;
        m.msg_data.udpsend.t->ss_p.ival2 = ENOMEM;
        m.msg_data.udpsend.t->ss.Signal();
        return false;
    }

    // tell the stack we have data to send
    net_msg msg;
    msg.msg_type = net_msg::net_msg_type::SendSocketData;
    msg.msg_data.socketdata.sck = this;
    if(!net_queue_msg(msg))
    {
        // this should hopefully never happen because we have just popped a message to get here
        return false;
    }

    return true;        
}

void UDPSocket::RecvFromInt(const net_msg &m, dgram_desc &dd)
{
    auto len = std::min(m.msg_data.udprecv.n, dd.len);
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "udp: read() request, load %d bytes from %x\n", (int)len, dd.start);
    }
    sb.recv_rptr = memcpy_split_src(m.msg_data.udprecv.buf, sb.recvbuf, len, dd.start, SocketBuffer::buflen);
    if(m.msg_data.udprecv.src_addr && m.msg_data.udprecv.addrlen)
    {
        auto addrlen = std::min(*m.msg_data.udprecv.addrlen, (socklen_t)sizeof(sockaddr_in));
        memcpy(m.msg_data.udprecv.src_addr, &dd.from, addrlen);
        *m.msg_data.udprecv.addrlen = addrlen;
    }
    m.msg_data.udprecv.t->ss_p.ival1 = len;
    m.msg_data.udprecv.t->ss.Signal();
}

int UDPSocket::SendPendingData()
{
    // build packets for each dgram
    CriticalGuard cg(sl);
    while(true)
    {
        dgram_send_desc dd;
        if(dgram_send_queue.Read(&dd))
        {
            IP4Route route;
            int ret = NET_OK;
            auto route_ret = net_ip_get_route_for_address(IP4Addr(dd.to.sin_addr.s_addr), &route);

            if(route_ret != NET_OK)
            {
                ret = route_ret;
            }
            else
            {
                // we have a valid dgram, try and build a packet
                auto hdr_size = 8 /* UDP header */ + 20 /* IP header */ + route.addr.iface->GetHeaderSize();
                auto pkt_size = hdr_size + dd.len + route.addr.iface->GetFooterSize();

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
                        memcpy_split_src(&pbuf[hdr_size], sb.sendbuf, dd.len, dd.start, SocketBuffer::buflen);
                        net_udp_decorate_packet(&pbuf[hdr_size], dd.len, &dd.to, this);
                    }
                }
            }

            if(ret == NET_OK)
            {
                // deferred success
                dd.t->ss_p.ival1 = dd.len;
                dd.t->ss.Signal();
            }
            else
            {
                // still need to deallocate data from the socket stream
                sb.send_wptr = (sb.send_wptr + dd.len) % SocketBuffer::buflen;

                // deferred result failed
                dd.t->ss_p.ival1 = -1;
                dd.t->ss_p.ival2 = net_ret_to_errno(ret);
                dd.t->ss.Signal();
            }
        }
        else
        {
            return NET_OK;
        }
    }
}

void net_udp_handle_recvfrom(const net_msg &m)
{
    m.msg_data.udprecv.sck->RecvFromInt(m);
}

void net_udp_handle_sendto(const net_msg &m)
{
    m.msg_data.udpsend.sck->SendToInt(m);
}

bool net_udp_decorate_packet(char *data, size_t datalen, const sockaddr_in *dest, UDPSocket *src)
{
    auto hdr = data - 8;
    *reinterpret_cast<uint16_t *>(&hdr[0]) = src->port;
    *reinterpret_cast<uint16_t *>(&hdr[2]) = dest->sin_port;
    *reinterpret_cast<uint16_t *>(&hdr[4]) = htons(8 + datalen);
    *reinterpret_cast<uint16_t *>(&hdr[6]) = 0;     // checksum optional for udp in ip4

    return net_ip_decorate_packet(hdr, datalen + 8, IP4Addr(dest->sin_addr.s_addr), src->bound_addr,
        IPPROTO_UDP);
}
