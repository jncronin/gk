#include <osnet.h>
#include <osqueue.h>
#include <thread.h>
#include "process.h"

__attribute__((section(".sram4"))) static FixedQueue<net_msg, 32> msgs;

static void handle_inject_packet(const net_msg &m);
static void handle_send_packet(const net_msg &m);
static void handle_timeouts();

static void *net_thread(void *_params)
{
    (void)_params;

    while(true)
    {
        net_msg m;
        if(msgs.Pop(&m, 100))
        {        
            switch(m.msg_type)
            {
                case net_msg::net_msg_type::InjectPacket:
                    handle_inject_packet(m);
                    break;

                case net_msg::net_msg_type::SendPacket:
                    handle_send_packet(m);
                    break;

                case net_msg::net_msg_type::UDPSendDgram:
                    net_udp_handle_sendto(m);
                    break;

                case net_msg::net_msg_type::TCPSendBuffer:
                    net_tcp_handle_sendto(m);
                    break;

                case net_msg::net_msg_type::SetIPAddress:
                    net_ip_handle_set_ip_address(m);
                    break;

                case net_msg::net_msg_type::SendSocketData:
                    m.msg_data.socketdata.sck->SendPendingData();
                    break;

                case net_msg::net_msg_type::ArpRequestAndSend:
                    net_arp_handle_request_and_send(m);
                    break;

                case net_msg::net_msg_type::HandleWaitingReads:
                    m.msg_data.ipsck->HandleWaitingReads();
                    break;
            }
        }
        handle_timeouts();
    }
}

int net_queue_msg(const net_msg &m)
{
    return msgs.Push(m) ? NET_OK : NET_NOMEM;
}

int net_ret_to_errno(int ret)
{
    switch(ret)
    {
        case NET_OK:
            return EOK;
        case NET_NOMEM:
            return ENOMEM;
        case NET_NOTSUPP:
            return ENOTSUP;
        case NET_TRYAGAIN:
        case NET_NOTUS:
        case NET_DEFER:
            return EOK;
        case NET_MSGSIZE:
            return EMSGSIZE;
        default:
            return EINVAL;
    }
}

void init_net()
{
    extern char _slwip_data, _elwip_data;
    auto start_ptr = (uint32_t)(uintptr_t)&_slwip_data;
    auto end_ptr = (uint32_t)(uintptr_t)&_elwip_data;

    extern Process kernel_proc;
    Schedule(Thread::Create("net", net_thread, nullptr, true, 5, kernel_proc,
        CPUAffinity::PreferM4, InvalidMemregion(),
        MPUGenerate(start_ptr, end_ptr - start_ptr, 6, false, RW, RO, WBWA_NS)));
}

static void handle_inject_packet(const net_msg &m)
{
    auto ret = net_handle_ethernet_packet(m.msg_data.packet.buf, m.msg_data.packet.n,
        m.msg_data.packet.iface);
    if(m.msg_data.packet.release_packet && ret != NET_KEEPPACKET)
        net_deallocate_pbuf((char *)m.msg_data.packet.buf);
}

static void handle_send_packet(const net_msg &m)
{
    m.msg_data.packet.iface->SendEthernetPacket(m.msg_data.packet.buf,
        m.msg_data.packet.n,
        m.msg_data.packet.dest,
        m.msg_data.packet.ethertype,
        m.msg_data.packet.release_packet);
}

void net_arp_handle_timeouts();
void net_tcp_handle_timeouts();

static void handle_timeouts()
{
    net_arp_handle_timeouts();
    net_tcp_handle_timeouts();
}
