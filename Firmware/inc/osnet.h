#ifndef OSNET_H
#define OSNET_H

#include <stddef.h>
#include <memory>
#include <string>

#include "osmutex.h"

#define GK_NET_SOCKET_BUFSIZE       4096

#define NET_DATA __attribute__((section(".lwip_data")))
#define SRAM4_DATA __attribute__((section(".sram4")))

inline uint16_t ntohs(uint16_t v)
{
    return __builtin_bswap16(v);
}

inline uint32_t ntohl(uint32_t v)
{
    return __builtin_bswap32(v);
}

class HwAddr
{
    protected:
        char b[6];

    public:
        HwAddr(const char *addr);
        HwAddr() = default;

        std::string ToString() const;
        void ToString(char *buf) const;

        static const HwAddr multicast;

        bool operator==(const HwAddr &other);
};

class IP4Addr
{
    protected:
        uint32_t v;

    public:
        IP4Addr(uint32_t val);
        IP4Addr() = default;

        std::string ToString() const;
        void ToString(char *buf) const;

        uint32_t get() const;

        static bool Compare(IP4Addr a, IP4Addr b, IP4Addr mask = IP4Addr(0xffffffff));
        bool operator==(const uint32_t &other) const;
        bool operator<(const IP4Addr &other) const;
        operator uint32_t() const;
};

class NetInterface
{
    public:
        virtual const HwAddr &GetHwAddr() const = 0;
        virtual bool GetLinkActive() const = 0;
        virtual int SendEthernetPacket(const char *buf, size_t n) = 0;
};

class TUSBNetInterface : public NetInterface
{
    protected:
        HwAddr our_hwaddr, peer_hwaddr;
        bool is_up = false;

    public:
        const HwAddr &GetHwAddr() const;
        bool GetLinkActive() const;
        int SendEthernetPacket(const char *buf, size_t n);

        friend void tud_network_init_cb(void);
};

class IP4Address
{
    public:
        IP4Addr addr, nm, gw;
        NetInterface *iface;
        IP4Addr Broadcast() const;
        IP4Address() = default;
};

// messages to be processed by net server
struct net_msg
{
    enum net_msg_type
    {
        InjectPacket,
        SendPacket,
    };

    net_msg_type msg_type;

    union 
    {
        struct packet_t
        {
            const char *buf;
            size_t n;
            NetInterface *iface;
        } packet;
    } msg_data;
};

// error list
#define NET_OK          0
#define NET_NOTUS       -1
#define NET_NOTSUPP     -2
#define NET_TRYAGAIN    -3
#define NET_NOMEM       -4

#define PBUF_SIZE       1542

void init_net();

int net_queue_msg(const net_msg &m);

int net_inject_ethernet_packet(const char *buf, size_t n, NetInterface *iface);
char *net_allocate_pbuf();
void net_deallocate_pbuf(char *);

class EthernetPacket
{
    public:
        HwAddr src, dest;
        uint16_t ethertype;

        const char *contents;
        size_t n;

        NetInterface *iface;
};

int net_handle_ethernet_packet(const char *buf, size_t n, NetInterface *iface);
int net_handle_ip4_packet(const EthernetPacket &pkt);
int net_handle_arp_packet(const EthernetPacket &pkt);

class IP4Packet
{
    public:
        uint32_t src, dest;
        uint16_t protocol;

        const char *contents;
        size_t n;

        const EthernetPacket &epacket;
};

int net_handle_tcp_packet(const IP4Packet &pkt);
int net_handle_udp_packet(const IP4Packet &pkt);
int net_handle_icmp_packet(const IP4Packet &pkt);


/* Socket interface */
class Socket
{
    public:
        char *recvbuf = nullptr;
        char *sendbuf = nullptr;

        size_t recv_wptr = 0;
        size_t recv_rptr = 0;
        size_t send_wptr = 0;
        size_t send_rptr = 0;

        const size_t buflen = GK_NET_SOCKET_BUFSIZE;

        Spinlock sl;

        int sockfd;

        bool is_bound = false;
        bool is_nonblocking = false;

        virtual int HandlePacket(const char *pkt, size_t n) = 0;
        virtual int SendData(const char *d, size_t n, const void *addr, size_t addrlen) = 0;
        virtual int RecvData(char *d, size_t n, void *addr, size_t addrlen, SimpleSignal &ss) = 0;

        bool thread_is_blocking_for_recv = false;
        SimpleSignal *blocking_thread_signal = nullptr;

        Socket();
        ~Socket();
};

class IP4Socket : public Socket
{
    public:
        IP4Addr bound_addr;
        uint16_t port;
};

class TCPSocket : public IP4Socket
{
    public:
        enum tcp_socket_state_t
        {
            Listening, 
        };

        tcp_socket_state_t state;
};

class UDPSocket : public IP4Socket
{
    public:
        int HandlePacket(const char *pkt, size_t n);
        int SendData(const char *d, size_t n, const void *addr, size_t addrlen);
        int RecvData(char *d, size_t n, void *addr, size_t addrlen, SimpleSignal &ss);

        size_t operator()(const UDPSocket &s) const noexcept;
        bool operator==(const UDPSocket &other) const noexcept;
};

namespace std
{
    template<> struct hash<UDPSocket>
    {
        size_t operator()(const UDPSocket &s) const noexcept
        {
            return std::hash<uint32_t>{}(s.bound_addr.get()) ^ std::hash<uint16_t>{}(s.port);
        }
    };
    template<> struct hash<TCPSocket>
    {
        size_t operator()(const TCPSocket &s) const noexcept
        {
            return std::hash<uint32_t>{}(s.bound_addr.get()) ^ std::hash<uint16_t>{}(s.port);
        }
    };
}

class RawSocket : public Socket
{

};

int net_bind_udpsocket(UDPSocket *sck);
int net_bind_tcpsocket(TCPSocket *sck);

char *net_allocate_sbuf();
void net_deallocate_sbuf(char *buf);

#endif
