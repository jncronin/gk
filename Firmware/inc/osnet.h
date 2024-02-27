#ifndef OSNET_H
#define OSNET_H

#include <stddef.h>
#include <memory>
#include <string>

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

#endif
