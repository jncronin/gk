#include "osnet.h"
#include <unordered_map>
#include <map>
#include "SEGGER_RTT.h"
#include "thread.h"
#include "clocks.h"
#include "syscalls_int.h"

extern Spinlock s_rtt;

#define FLAG_FIN    0x1
#define FLAG_SYN    0x2
#define FLAG_RST    0x4
#define FLAG_PSH    0x8
#define FLAG_ACK    0x10
#define FLAG_URG    0x20
#define FLAG_ECE    0x40
#define FLAG_CWR    0x80

SRAM4_DATA static std::unordered_map<sockaddr_in, TCPSocket *> bound_tcp_sockets;
SRAM4_DATA static std::unordered_map<sockaddr_in, TCPSocket *> listening_sockets;
SRAM4_DATA static std::unordered_map<sockaddr_pair, TCPSocket *> connected_sockets;
SRAM4_DATA static Spinlock s_tcp;

NET_DATA static char tcp_opts[] = {
    0x02, 0x04, 0x05, 0xb4,     // MSS = 1460
    0x01 /* align */, 0x03, 0x03, 0x00,           // Window scale = 0
    0x01, 0x01, 0x01, 0x00      // 3x align, end of list
};

static void net_tcp_send_empty_msg(const IP4Addr &dest, uint16_t port,
    uint32_t seq_id, uint32_t ack_id,
    const IP4Addr &src, uint16_t src_port,
    unsigned int flags,
    uint16_t wnd_size);

static uint16_t get_wnd_size();

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
        // if not a syn, this must be aimed at a socket in the connected state
        if(!(flags & FLAG_SYN))
        {
            auto fval = connected_sockets.find(sp);
            if(fval == connected_sockets.end())
            {
                // try with ipaddr_any
                fval = connected_sockets.find(sp.dest_any());
            }
            if(fval != connected_sockets.end())
            {
                sck = fval->second;
            }
        }
        if(!sck)
        {
            // either we are a syn packet or we haven't found a connected socket
            //  this can apply to either syn in listening state or ack in synreceived state
            auto fval = listening_sockets.find(sp.dest);
            if(fval == listening_sockets.end())
            {
                // try ipaddr_any
                fval = listening_sockets.find(sp.dest_any().dest);
            }
            if(fval != listening_sockets.end())
            {
                sck = fval->second;
            }
            else if(flags & FLAG_SYN)
            {
                // actively refuse
                net_tcp_send_empty_msg(pkt.src, src_port, 0, seq_id + 1 + len,
                    pkt.dest, dest_port, FLAG_RST | FLAG_ACK, 0);
                return NET_OK;
            }
        }
    }

    if(!sck)
    {
        return NET_NOTUS;
    }

    return sck->HandlePacket(data, len, IP4Addr(pkt.src), src_port,
        IP4Addr(pkt.dest), dest_port, seq_id, ack_id, flags,
        opts, optlen);
}

static void net_tcp_send_empty_msg(const IP4Addr &dest, uint16_t port,
    uint32_t seq_id, uint32_t ack_id,
    const IP4Addr &src, uint16_t src_port,
    unsigned int flags,
    uint16_t wnd_size)
{
    IP4Route route;
    auto route_ret = net_ip_get_route_for_address(dest, &route);
    if(route_ret != NET_OK)
        return;
    
    auto pbuf = net_allocate_pbuf(NET_SIZE_TCP);
    if(!pbuf)
        return;

    auto hdr_size = 20 /* TCP header */ + 20 /* IP header */ + route.addr.iface->GetHeaderSize();

    net_tcp_decorate_packet(&pbuf[hdr_size], 0, dest, port, src, src_port,
        seq_id, ack_id, flags, nullptr, 0, wnd_size, true, &route);    
}

int TCPSocket::handle_rst()
{
    // rst means the peer wont send us data or receive data, therefore we can
    // delete all outgoing packets if necessary
    for(auto &sp : sent_packets)
    {
        net_deallocate_pbuf(sp.second.buf);
    }
    sent_packets.clear();
    return handle_closed(false);
}

int TCPSocket::handle_closed(bool orderly)
{
    // this is called on RST or successful FIN FIN close

    // we mark the socket as closed, and wakeup any pending read() with a socket close message
    read_waiting_thread rwt;
    while(read_waiting_threads.Read(&rwt))
    {
        if(orderly)
        {
            rwt.t->ss_p.ival1 = 0;
        }
        else
        {
            rwt.t->ss_p.ival1 = -1;
            rwt.t->ss_p.ival2 = ECONNRESET;
        }
        rwt.t->ss.Signal();
    }
    state = tcp_socket_state_t::Closed;

    // remove the socket from the connected queue, if necessary
    {
        CriticalGuard cg(s_tcp);
        auto iter = connected_sockets.begin();
        while(iter != connected_sockets.end())
        {
            if(iter->second == this)
            {
                iter = connected_sockets.erase(iter);
            }
            else
            {
                iter++;
            }
        }
    }

    return NET_OK;
}

int TCPSocket::HandlePacket(const char *pkt, size_t n,
            IP4Addr src, uint16_t src_port,
            IP4Addr dest, uint16_t dest_port,
            uint32_t seq_id, uint32_t ack_id,
            unsigned int flags,
            const char *opts, size_t optlen)
{
    IP4Route route;
    auto route_ret = net_ip_get_route_for_address(dest, &route);
    if(route_ret != NET_OK)
        return route_ret;
    int ret = NET_NOTSUPP;

    CriticalGuard cg(sl);

    {
        CriticalGuard cg2(s_rtt);
        SEGGER_RTT_printf(0, "net: tcpsocket: packet: state %d, flags %x, len %u\n", 
            (int)state, flags, n);
    }

    switch(state)
    {
        case Closed:
            // shouldn't get here because we shouldn't be in a queue
            break;

        case Listen:
            // is this a syn packet?
            if(flags & FLAG_SYN)
            {
                // we can establish a connection
                peer_seq_start = seq_id;
                my_seq_start = rand();

                auto pbuf = net_allocate_pbuf(NET_SIZE_TCP);
                if(!pbuf)
                {
                    return NET_NOMEM;
                }

                n_data_received = 1;

                auto hdr_size = 20 /* TCP header */ + 20 /* IP header */ + route.addr.iface->GetHeaderSize();
                net_tcp_decorate_packet(&pbuf[hdr_size], 0, src, src_port,
                    dest, dest_port, my_seq_start, peer_seq_start + n_data_received,
                    FLAG_ACK | FLAG_SYN, tcp_opts, sizeof(tcp_opts), get_wnd_size(), true);
                
                n_data_sent = 1;

                last_data_timestamp = clock_cur_ms();
                state = SynReceived;
            }
            break;

        case SynReceived:
            // is this an ACK packet?
            if(flags & FLAG_ACK)
            {
                // we would expect seq_id to equal peer_seq + n_data_received
                if((seq_id == peer_seq_start + n_data_received) &&
                    (ack_id == my_seq_start + n_data_sent))
                {
                    // create a buffer
                    pending_accept_req par;
                    par.from.sin_family = AF_INET;
                    par.from.sin_addr.s_addr = src;
                    par.from.sin_port = src_port;
                    par.my_seq_start = my_seq_start;
                    par.peer_seq_start = peer_seq_start;
                    par.n_data_received = n_data_received;
                    par.n_data_sent = n_data_sent;

                    // Return us to listening state - accept() call will receive new TCPSocket fildes,
                    //  we can silently handle a second connection awaiting another accept() call
                    state = Listen;

                    // can we pair with a waiting thread?
                    if(accept_t)
                    {
                        auto caccept_t = accept_t;
                        accept_t = nullptr;
                        return PairConnectAccept(std::move(par), caccept_t, (int *)&accept_t->ss_p.ival2, true);
                    }
                    else
                    {
                        // add to accept queue
                        if(!pending_accept_queue.Write(std::move(par)))
                        {
                            return NET_NOMEM;
                        }
                        return NET_OK;
                    }
                }
            }
            break;

        case Established:
            ret = NET_OK;
            if(n)
            {
                // packet with data

                // For now, silently drop out-of-order packets.  Eventually, store their packets somewhere
                //  and realign later
                // handle wraparound
                auto seq_discrepancy = static_cast<int32_t>((peer_seq_start + n_data_received) - seq_id);
                if(seq_discrepancy >= 0)
                {
                    if(seq_discrepancy == 0)
                    {
                        recv_packet rp;
                        rp.buf = pkt;
                        rp.nlen = n;
                        rp.rptr = 0;
                        rp.from = peer_addr;
                        rp.from_port = peer_port;

                        if(recv_packets.Write(rp))
                        {
                            n_data_received += n;

                            HandleWaitingReads();
                            ret = NET_KEEPPACKET;
                        }
                    }

                    // send ack
                    net_tcp_send_empty_msg(src, src_port, my_seq_start + n_data_sent,
                        peer_seq_start + n_data_received, dest, dest_port, FLAG_ACK,
                        get_wnd_size());
                }
            }
            // ACK packets
            if(flags & FLAG_ACK)
            {
                auto ack_from = peer_seq_start;
                auto ack_to = ack_id - 1;
                if(ack_to > ack_from)
                {
                    handle_ack(ack_from, ack_to);
                }
                else
                {
                    handle_ack(ack_from, 0xffffffffU);
                    handle_ack(0U, ack_to);
                }
            }
            if(flags & FLAG_RST)
            {
                // peer has given up, close socket
                handle_rst();
            }
            if(flags & FLAG_FIN)
            {
                // peer requests close - send combined FIN/ACK
                n_data_received++;
                net_tcp_send_empty_msg(src, src_port, my_seq_start + n_data_sent,
                        peer_seq_start + n_data_received, dest, dest_port, FLAG_FIN | FLAG_ACK,
                        0UL);
                state = tcp_socket_state_t::LastAck;
                n_data_sent++;
                ms_fin_sent = clock_cur_ms();
            }
            break;

        case LastAck:
            // expect an ack
            if(flags & FLAG_ACK)
            {
                // graceful close
                handle_closed(true);
            }
            break;

        case FinWait1:
            // expect ACK or FIN + ACK
            if((flags & FLAG_ACK) && (flags & FLAG_FIN))
            {
                net_tcp_send_empty_msg(src, src_port,
                    my_seq_start + n_data_sent,
                    peer_seq_start + n_data_received,
                    dest, dest_port, FLAG_ACK, 0);
                ms_fin_sent = clock_cur_ms();
                
                // immediate close
                handle_closed(true);
                if(close_t)
                {
                    close_t->ss_p.ival1 = 0;
                    close_t->ss.Signal();
                    close_t = nullptr;
                }
            }
            else if(flags & FLAG_ACK)
            {
                // expect further FIN
                state = tcp_socket_state_t::FinWait2;
            }
            break;

        case FinWait2:
            // expect FIN
            if(flags & FLAG_FIN)
            {
                net_tcp_send_empty_msg(src, src_port,
                    my_seq_start + n_data_sent,
                    peer_seq_start + n_data_received,
                    dest, dest_port, FLAG_ACK, 0);
                ms_fin_sent = clock_cur_ms();
                
                // immediate close
                handle_closed(true);
                if(close_t)
                {
                    close_t->ss_p.ival1 = 0;
                    close_t->ss.Signal();
                    close_t = nullptr;
                }
            }
            break;

        default:
            break;
    }
    return ret;
}

int TCPSocket::CloseAsync(int *_errno)
{
    CriticalGuard cg(sl);
    int ret = 0;
    if(state == tcp_socket_state_t::Established)
    {
        // need to send fin, and await fin/ack
        net_tcp_send_empty_msg(peer_addr, peer_port,
            my_seq_start + n_data_sent,
            peer_seq_start + n_data_received,
            bound_addr, port, FLAG_FIN, 0);
        state = tcp_socket_state_t::FinWait1;
        ms_fin_sent = clock_cur_ms();
        close_t = GetCurrentThreadForCore();
        ret = -2;
    }

    if(state == tcp_socket_state_t::Listen)
    {
        // remove from listen pool
        CriticalGuard cg2(s_tcp);
        auto iter = listening_sockets.begin();
        while(iter != listening_sockets.end())
        {
            if(iter->second == this)
            {
                iter = listening_sockets.erase(iter);
            }
            else
            {
                iter++;
            }
        }
    }

    if(state != tcp_socket_state_t::Unbound)
    {
        // remove from bound pool
        CriticalGuard cg2(s_tcp);
        auto iter = bound_tcp_sockets.begin();
        while(iter != bound_tcp_sockets.end())
        {
            if(iter->second == this)
            {
                iter = bound_tcp_sockets.erase(iter);
            }
            else
            {
                iter++;
            }
        }
    }

    return ret;
}

int TCPSocket::BindAsync(const sockaddr *addr, socklen_t addrlen, int *_errno)
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
        CriticalGuard cg(s_tcp);

        // first check the IPADDR_ANY address is not bound for this port
        if(saddr.sin_addr.s_addr)
        {
            sockaddr_in any_addr = saddr;
            any_addr.sin_addr.s_addr = 0;
            if(bound_tcp_sockets.find(any_addr) != bound_tcp_sockets.end())
            {
                *_errno = EADDRINUSE;
                return -1;
            }
        }
        if(bound_tcp_sockets.find(saddr) != bound_tcp_sockets.end())
        {
            // already bound
            // TODO: handle REUSEADDR/REUSEPORT
            *_errno = EADDRINUSE;
            return -1;
        }

        state = Closed;
        bound_tcp_sockets[saddr] = this;
        bound_addr = saddr.sin_addr.s_addr;

        return 0;
    }
}

int TCPSocket::ListenAsync(int backlog, int *_errno)
{
    sockaddr_in saddr;

    {
        CriticalGuard cg(sl);

        if(state != Closed)
        {
            // TODO: try binding an ephemereal port first
            *_errno = EADDRINUSE;
            return -1;
        }

        state = Listen;
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = bound_addr;
        saddr.sin_port = port;
    }
    
    {
        CriticalGuard cg(s_tcp);
        listening_sockets[saddr] = this;
    }

    return NET_OK;
}

int TCPSocket::PairConnectAccept(const pending_accept_req &req,
            Thread *t, int *_errno, bool is_async)
{
    // This can be called either from the tcpip thread or the application thread
    // We have the spinlock in either case
    int __errno;
    int ret;
    TCPSocket *nsck;
    SocketFile *sfile;
    auto &p = t->p;
    sockaddr_pair sp;
    int fildes;

    // Create new socket
    nsck = new TCPSocket();
    if(!nsck)
    {
        __errno = ENOMEM;
        ret = -1;
        goto handle_fail;
    }

    nsck->my_seq_start = req.my_seq_start;
    nsck->peer_seq_start = req.peer_seq_start;
    nsck->n_data_received = req.n_data_received;
    nsck->n_data_sent = req.n_data_sent;
    nsck->bound_addr = bound_addr;
    nsck->port = port;
    nsck->state = Established;
    nsck->peer_addr = req.from.sin_addr.s_addr;
    nsck->peer_port = req.from.sin_port;

    // Register with fildes
    {
        CriticalGuard cg(p.sl);

        fildes = get_free_fildes(p);
        if(fildes == -1)
        {
            __errno = EMFILE;
            ret = -1;
            goto handle_fail;
        }

        nsck->sockfd = fildes;
        sfile = new SocketFile(nsck);
        if(!sfile)
        {
            __errno = ENOMEM;
            ret = -1;
            goto handle_fail;
        }
        p.open_files[fildes] = sfile;
    }

    // Register as a connected socket
    sp.dest.sin_family = AF_INET;
    sp.dest.sin_addr.s_addr = bound_addr;
    sp.dest.sin_port = port;
    sp.src = req.from;
    {
        CriticalGuard cg(s_tcp);
        connected_sockets[sp] = nsck;
    }

    // return success

    if(is_async)
    {
        t->ss_p.ival1 = fildes;
        t->ss.Signal();
        return NET_OK;
    }
    else
    {
        return fildes;
    }

handle_fail:
    if(is_async)
    {
        t->ss_p.ival1 = ret;
        t->ss_p.ival2 = __errno;
        t->ss.Signal();
        return NET_OK;
    }
    else
    {
        *_errno = __errno;
        return ret;
    }
}

int TCPSocket::RecvFromAsync(void *buf, size_t len, int flags,
            struct sockaddr *src_addr, socklen_t *addrlen, int *_errno)
{
    CriticalGuard cg(sl);
    if(state != Established)
    {
        *_errno = ENOTCONN;
        return -1;
    }

    // blocking request
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

int TCPSocket::SendToAsync(const void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, socklen_t addrlen, int *_errno)
{
    net_msg msg;
    msg.msg_type = net_msg::net_msg_type::TCPSendBuffer;
    msg.msg_data.tcpsend.buf = (char *)buf;
    msg.msg_data.tcpsend.n = len;
    msg.msg_data.tcpsend.flags = flags;
    msg.msg_data.tcpsend.dest_addr = (sockaddr_in *)dest_addr;
    msg.msg_data.tcpsend.addrlen = addrlen;
    msg.msg_data.tcpsend.t = GetCurrentThreadForCore();
    msg.msg_data.tcpsend.sck = this;

    auto qret = net_queue_msg(msg);
    if(qret != NET_OK)
    {
        *_errno = net_ret_to_errno(qret);
        return -1;
    }

    return -2;    
}

int TCPSocket::AcceptAsync(sockaddr *sockaddr, socklen_t *addrlen, int *_errno)
{
    if(addrlen && *addrlen < sizeof(sockaddr_in))
    {
        *_errno = EINVAL;
        return -1;
    }

    if(state != Listen)
    {
        *_errno = EINVAL;
        return -1;
    }

    {
        CriticalGuard cg(sl);

        // try and get a pending connection
        pending_accept_req par;
        if(pending_accept_queue.Read(&par))
        {
            return PairConnectAccept(std::move(par), GetCurrentThreadForCore(), _errno, false);
        }
        else
        {
            // no pending accepts, block
            accept_t = GetCurrentThreadForCore();
            return -2;
        }
    }
}

bool TCPSocket::SendToInt(const net_msg &m)
{
    // we handle this on the server thread to ensure we can
    //  access all the data structures we need
    
    CriticalGuard cg(sl);

    // split into packets
    const auto &msg = m.msg_data.tcpsend;
    unsigned int n_sent = 0;
    while(n_sent < msg.n)
    {
        unsigned int n_left = msg.n - n_sent;
        unsigned int n_packet = std::min(n_left, PBUF_SIZE - NET_SIZE_TCP);

        auto pbuf = net_allocate_pbuf(n_packet + NET_SIZE_TCP);
        if(!pbuf)
        {
            // deferred return
            if(n_sent)
            {
                msg.t->ss_p.uval1 = n_sent;
                msg.t->ss.Signal();
            }
            else
            {
                msg.t->ss_p.ival1 = -1;
                msg.t->ss_p.ival2 = ENOMEM;
                msg.t->ss.Signal();
            }
            return false;
        }

        // copy data to packet
        auto data = &pbuf[NET_SIZE_TCP_OFFSET];
        memcpy(data, &msg.buf[n_sent], n_packet);

        auto seq_id = my_seq_start + n_data_sent;
        auto cur_wnd_size = net_pbuf_nfree();
        cur_wnd_size = std::max(0U, cur_wnd_size - 4);    // leave us some space
        cur_wnd_size *= PBUF_SIZE - NET_SIZE_TCP;
        cur_wnd_size = std::min(cur_wnd_size, 64000U);
        auto sret = net_tcp_decorate_packet(data, n_packet, peer_addr, peer_port,
            bound_addr, port, seq_id, peer_seq_start + n_data_received, FLAG_ACK,
            nullptr, 0,  cur_wnd_size, false, nullptr);
        if(sret)
        {
            // store reference to buffer in timeout list
            tcp_sent_packet sp;
            sp.ms = clock_cur_ms();
            sp.buf = data;
            sp.nlen = n_packet;
            sp.sck = this;
            sp.seq_id = seq_id;
            sp.ntimeouts = 0;
            sent_packets[seq_id] = sp;

            n_data_sent += n_packet;
            n_sent += n_packet;
        }
        else
        {
            // deferred return
            if(n_sent)
            {
                msg.t->ss_p.uval1 = n_sent;
                msg.t->ss.Signal();
            }
            else
            {
                msg.t->ss_p.ival1 = -1;
                msg.t->ss_p.ival2 = EFAULT;
                msg.t->ss.Signal();
            }
            net_deallocate_pbuf(pbuf);
            return false;
        }
    }

    msg.t->ss_p.uval1 = n_sent;
    msg.t->ss.Signal();

    return true;
}

void net_tcp_handle_sendto(const net_msg &m)
{
    m.msg_data.tcpsend.sck->SendToInt(m);
}

bool net_tcp_decorate_packet(char *data, size_t datalen,
    const IP4Addr &dest, uint16_t dest_port,
    const IP4Addr &src, uint16_t src_port,
    uint32_t seq_id, uint32_t ack_id,
    unsigned int flags,
    const char *opts, size_t optlen,
    uint16_t wnd_size,
    bool release_buffer,
    const IP4Route *route)
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

    // if src is ipaddr_any, we need to get route here and calculate
    IP4Route calcroute;
    auto _route = route;
    IP4Addr _src = src;
    if(src == 0UL)
    {
        if(!_route)
        {
            // get route for dest
            if(net_ip_get_route_for_address(dest, &calcroute) != NET_OK)
            {
                return false;
            }
            _route = &calcroute;
        }
        _src = _route->addr.addr;
    }
    char ipph[12];
    *(reinterpret_cast<uint32_t *>(&ipph[0])) = _src;
    *(reinterpret_cast<uint32_t *>(&ipph[4])) = dest;
    ipph[8] = 0;
    ipph[9] = IPPROTO_TCP;
    *(reinterpret_cast<uint16_t *>(&ipph[10])) = htons(20 + actoptlen + datalen);

    auto csum = net_ip_complete_checksum(
        net_ip_calc_partial_checksum(hdr, 20 + actoptlen + datalen,
            net_ip_calc_partial_checksum(ipph, 12))
    );
    *(reinterpret_cast<uint16_t *>(&hdr[16])) = csum;

    return net_ip_decorate_packet(hdr, datalen + 20 + actoptlen,
        dest, _src, IPPROTO_TCP, release_buffer, _route);
}

void TCPSocket::handle_ack(size_t start, size_t end)
{
    auto iter = sent_packets.find(start);
    if(iter == sent_packets.end())
    {
        iter = sent_packets.insert({ start, tcp_sent_packet() }).first;
    }

    while(iter != sent_packets.end())
    {
        if(iter->first <= end)
        {
            net_deallocate_pbuf(iter->second.buf);
            iter = sent_packets.erase(iter);
        }
        else
        {
            break;
        }
    }
}

static uint64_t calc_timeout(uint64_t start, int ntout)
{
    // exponential backoff starting at 200 ms
    return start + (200ULL << ntout);
}

static uint16_t get_wnd_size()
{
    auto cur_wnd_size = net_pbuf_nfree();
    cur_wnd_size = std::max(0U, cur_wnd_size - 4);    // leave us some space
    cur_wnd_size *= PBUF_SIZE - NET_SIZE_TCP;
    cur_wnd_size = std::min(cur_wnd_size, 64000U);
    return cur_wnd_size;
}

void net_tcp_handle_timeouts()
{
    CriticalGuard cg(s_tcp);

    for(auto &cs : connected_sockets)
    {
        auto sck = cs.second;

        CriticalGuard cg2(sck->sl);
        
        auto iter = sck->sent_packets.begin();
        auto now = clock_cur_ms();
        while(iter != sck->sent_packets.end())
        {
            auto &sp = iter->second;
            
            auto tout_at = calc_timeout(sp.ms, sp.ntimeouts);
            if(now > tout_at)
            {
                if(sp.ntimeouts < 10)
                {
                    // resend packet
                    net_tcp_decorate_packet(sp.buf, sp.nlen,
                        sp.sck->peer_addr, sp.sck->peer_port,
                        sp.sck->bound_addr, sp.sck->port,
                        sp.seq_id, sp.sck->peer_seq_start + sp.sck->n_data_received,
                        FLAG_ACK, nullptr, 0, get_wnd_size(), false);
                    sp.ntimeouts++;
                }
                else
                {
                    // fail and close socket
                    net_deallocate_pbuf(sp.buf);
                    sck->handle_closed(false);
                    iter = sck->sent_packets.erase(iter);
                    continue;
                }
            }

            iter++;
        }
    }
}
