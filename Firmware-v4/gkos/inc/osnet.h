#ifndef OSNET_H
#define OSNET_H

#include <stddef.h>
#include <memory>
#include <string>
#include <queue>
#include <map>
#include <vector>

#include "osmutex.h"
#include "osringbuffer.h"
#include "_netinet_in.h"
#include "osqueue.h"

//#include <driver/include/m2m_types.h>

#ifdef EOK
#undef EOK
#endif
#define EOK 0

#define GK_NET_SOCKET_BUFSIZE       4096

#define SRAM4_DATA 

#define NET_DATA
#define NET_BSS

inline uint16_t ntohs(uint16_t v)
{
    return __builtin_bswap16(v);
}

inline uint32_t ntohl(uint32_t v)
{
    return __builtin_bswap32(v);
}

inline uint16_t htons(uint16_t v)
{
    return __builtin_bswap16(v);
}

inline uint32_t htonl(uint32_t v)
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

        bool operator==(const HwAddr &other) const;
        bool operator!=(const HwAddr &other) const;
        const char *get() const;
};

class IP4Addr
{
    protected:
        uint32_t v;

    public:
        IP4Addr(uint32_t val);
        IP4Addr(const char *v);
        IP4Addr(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
        IP4Addr() = default;

        std::string ToString() const;
        void ToString(char *buf) const;

        uint32_t get() const;

        static bool Compare(IP4Addr a, IP4Addr b, IP4Addr mask = IP4Addr(0xffffffff));
        bool operator==(const uint32_t &other) const;
        bool operator==(const IP4Addr &other) const;
        bool operator<(const IP4Addr &other) const;
        operator uint32_t() const;
};

struct netiface_msg
{
    enum class netiface_msg_type
    {
        Activate,
        Deactivate,
        HardwareEvent,
        Deregister,
        LinkUp
    };

    netiface_msg_type msg_type;
    unsigned int msg_subtype;
};

class NetInterface
{
    protected:
        FixedQueue<netiface_msg, 32> events;
        std::string name;
        bool active = false;
        bool connected = false;
        HwAddr hwaddr;
        virtual int IdleTask();
        virtual int HardwareEvent(const netiface_msg &msg);
        virtual int Activate();
        virtual int Deactivate();

    public:
        virtual const HwAddr &GetHwAddr() const;
        virtual bool GetDeviceActive() const;
        virtual bool GetLinkActive() const;
        virtual int SendEthernetPacket(char *buf, size_t n, const HwAddr &dest, uint16_t ethertype,
            bool release_buffer) = 0;
        virtual std::string DeviceName() const;
        virtual std::string DeviceType() const;
        virtual std::string Name() const;

        virtual int GetHeaderSize() const;
        virtual int GetFooterSize() const;

        virtual int SetActive(bool active);

        virtual void RunTaskLoop();

        friend std::pair<int, std::string> net_register_interface_internal(NetInterface *iface);
};

/* Call this to register an interface and create an event loop */
int net_register_interface(NetInterface *iface);

/* This is called by internal osnet functions to register the event loop with net,
    assign a net id etc */
std::pair<int, std::string> net_register_interface_internal(NetInterface *iface);

/* This is called to remove the interface from the known interface list */
int net_deregister_interface(NetInterface *iface);

/* This is called by internal code to do the actual deregistering */
int net_deregister_interface_internal(NetInterface *iface);

class WifiNetInterface : public NetInterface
{
    public:
        struct wifi_network
        {
            std::string ssid;
            int ch;
            int rssi;
            std::string password = "";
            void *dev_data;
        };

    protected:
        kernel_time last_scan_time = kernel_time_invalid();
        virtual int IdleTask();
        std::vector<wifi_network> networks;
        std::vector<wifi_network> try_connect_networks;
        size_t cur_try_connect_network;
        kernel_time last_try_connect_time;
        bool scan_in_progress = false;
        bool connecting = false;
        virtual int DoScan();
        virtual int Connect(const wifi_network &wn);

    public:
        virtual const std::vector<wifi_network> &ListNetworks() const;
        virtual std::string DeviceType() const;
};

/*
class TUSBNetInterface : public NetInterface
{
    protected:
        HwAddr our_hwaddr, peer_hwaddr;
        bool is_up = false;

    public:
        const HwAddr &GetHwAddr() const;
        bool GetLinkActive() const;
        int SendEthernetPacket(char *buf, size_t n, const HwAddr &dest, uint16_t ethertype,
            bool release_buffer);

        int GetHeaderSize() const;
        int GetFooterSize() const;

        friend void tud_network_init_cb(void);
}; */

class IP4Address
{
    public:
        IP4Addr addr, nm, gw;
        NetInterface *iface;
        IP4Addr Broadcast() const;
        IP4Address() = default;
};

class IP4Route
{
    public:
        IP4Address addr;
        int metric;
};

class Socket;
class UDPSocket;
class TCPSocket;
class IP4Socket;
// messages to be processed by net server
struct net_msg
{
    enum net_msg_type
    {
        InjectPacket,
        SendPacket,
        SendSocketData,
        UDPSendDgram,
        TCPSendBuffer,
        SetIPAddress,
        ArpRequestAndSend,
        HandleWaitingReads,
        DeleteIPAddressForIface,
    };

    net_msg_type msg_type;

    union 
    {
        struct packet_t
        {
            char *buf;
            size_t n;
            NetInterface *iface;
            uint16_t ethertype;
            HwAddr dest;
            bool release_packet;
        } packet;
        struct arp_request_t
        {
            char *buf;
            size_t n;
            NetInterface *iface;
            IP4Addr addr;
            bool release_buffer;
        } arp_request;
        struct socketdata_t
        {
            Socket *sck;
        } socketdata;
        struct udprecvdgram_t
        {
            char *buf;
            size_t n;
            int flags;
            sockaddr_in *src_addr;
            socklen_t *addrlen;
            Thread *t;
            UDPSocket *sck;
        } udprecv;
        struct udpsenddgram_t
        {
            const char *buf;
            size_t n;
            int flags;
            sockaddr_in *dest_addr;
            socklen_t addrlen;
            Thread *t;
            UDPSocket *sck;
        } udpsend;
        struct tcpsend_t
        {
            const char *buf;
            size_t n;
            int flags;
            sockaddr_in *dest_addr;
            socklen_t addrlen;
            Thread *t;
            TCPSocket *sck;
        } tcpsend;
        IP4Address ipaddr;
        IP4Socket *ipsck;
    } msg_data;
};

// error list
#define NET_OK          0
#define NET_NOTUS       -1
#define NET_NOTSUPP     -2
#define NET_TRYAGAIN    -3
#define NET_NOMEM       -4
#define NET_MSGSIZE     -5
#define NET_DEFER       -6
#define NET_NOROUTE     -7
#define NET_KEEPPACKET  -8

/* Keep these a multiple of cache line size */
#define PBUF_SIZE       1664U
#define SPBUF_SIZE      128U

#define NET_SIZE_ETHERNET_HEADER        14U
#define NET_SIZE_ETHERNET_FOOTER        4U
#define NET_SIZE_WIFI_HEADER            (/* M2M_ETHERNET_HDR_OFFSET + M2M_ETH_PAD_SIZE */ 64)
#define NET_SIZE_ETHERNET               (NET_SIZE_ETHERNET_HEADER + NET_SIZE_WIFI_HEADER + NET_SIZE_ETHERNET_FOOTER)
#define NET_SIZE_IP_HEADER              20U
#define NET_SIZE_UDP_HEADER             8U
#define NET_SIZE_TCP_HEADER             20U

#define NET_SIZE_IP                     (NET_SIZE_IP_HEADER + NET_SIZE_ETHERNET)
#define NET_SIZE_UDP                    (NET_SIZE_UDP_HEADER + NET_SIZE_IP)
#define NET_SIZE_TCP                    (NET_SIZE_TCP_HEADER + NET_SIZE_IP)
#define NET_SIZE_IP_OFFSET              (NET_SIZE_ETHERNET_HEADER + NET_SIZE_WIFI_HEADER + NET_SIZE_IP_HEADER)
#define NET_SIZE_TCP_OFFSET             (NET_SIZE_TCP_HEADER + NET_SIZE_IP_OFFSET)
#define NET_SIZE_UDP_OFFSET             (NET_SIZE_UDP_HEADER + NET_SIZE_IP_OFFSET)

bool init_net();
void *net_telnet_thread(void *p);
void *net_dhcpd_thread(void *p);

int net_inject_ethernet_packet(const char *buf, size_t n, NetInterface *iface);
char *net_allocate_pbuf(size_t n);
void net_deallocate_pbuf(char *);
size_t net_pbuf_nfree();

class EthernetPacket
{
    public:
        HwAddr src, dest;
        uint16_t ethertype;

        const char *contents;
        size_t n;

        NetInterface *iface;

        const char *link_layer_data;
        size_t link_layer_n;
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

class UDPPacket
{
    public:
        uint16_t src_port, dest_port;

        const char *contents;
        size_t n;

        const IP4Packet &ippacket;
};

int net_handle_tcp_packet(const IP4Packet &pkt);
int net_handle_udp_packet(const IP4Packet &pkt);
int net_handle_icmp_packet(const IP4Packet &pkt);
int net_handle_dhcpc_packet(const UDPPacket &pkt);


/* Socket interface */
struct recv_packet
{
    const char *buf;
    size_t nlen;
    size_t rptr;

    IP4Addr from;
    uint16_t from_port;
};

class Socket
{
    public:
        Spinlock sl;

        RingBuffer<recv_packet, 64> recv_packets;
        
        int sockfd;

        bool is_bound = false;
        bool is_nonblocking = false;

        virtual int BindAsync(const sockaddr *addr, socklen_t addrlen, int *_errno);
        virtual int RecvFromAsync(void *buf, size_t len, int flags,
            struct sockaddr *src_addr, socklen_t *addrlen, int *_errno);
        virtual int SendToAsync(const void *buf, size_t len, int flags,
            const struct sockaddr *dest_addr, socklen_t addrlen, int *_errno);
        virtual int SendPendingData();
        virtual int ListenAsync(int backlog, int *_errno);
        virtual int AcceptAsync(sockaddr *addr, socklen_t *addrlen, int *_errno);
        virtual int CloseAsync(int *_errno);
        virtual void HandleWaitingReads();

        bool thread_is_blocking_for_recv = false;
        SimpleSignal *blocking_thread_signal = nullptr;

        virtual ~Socket() = default;
};

class IP4Socket : public Socket
{
    protected:

    public:
        IP4Socket(bool _is_dgram) : is_dgram(_is_dgram) {}
        
        IP4Addr bound_addr;
        uint16_t port;
        bool is_dgram;

        struct read_waiting_thread
        {
            Thread *t;
            void *buf;
            size_t n;
            sockaddr *srcaddr;
            socklen_t *addrlen;
        };
        RingBuffer<read_waiting_thread, 8> read_waiting_threads;

        void HandleWaitingReads();
};

class TCPSocket : public IP4Socket
{
    protected:
        struct tcp_sent_packet
        {
            char *buf;
            size_t nlen;
            size_t seq_id;
            TCPSocket *sck;
            int ntimeouts;
            uint64_t ms;
        };

        uint64_t ms_fin_sent = 0ULL;

        struct pending_accept_req
        {
            sockaddr_in from;
            uint32_t my_seq_start, peer_seq_start;
            uint32_t n_data_sent, n_data_received;
        };
        RingBuffer<pending_accept_req, 64> pending_accept_queue;
        Thread *accept_t = nullptr;
        Thread *close_t = nullptr;

        int backlog_max;

        int PairConnectAccept(const pending_accept_req &req,
            Thread *t, int *_errno, bool is_async);

        int handle_rst();
        int handle_closed(bool orderly);
        void handle_ack(size_t start, size_t end);

        std::map<size_t, tcp_sent_packet> sent_packets;

        friend void net_tcp_handle_timeouts();

    public:
        TCPSocket() : IP4Socket(false) {}

        enum tcp_socket_state_t
        {
            Unbound,
            Listen,
            SynSent,
            SynReceived,
            Established,
            FinWait1,
            FinWait2,
            CloseWait,
            Closing,
            LastAck,
            TimeWait,
            Closed
        };



        tcp_socket_state_t state = Unbound;

        uint32_t peer_seq_start = 0UL;
        uint32_t my_seq_start = 0UL;

        uint32_t n_data_received = 0UL;
        uint32_t n_data_sent = 0UL;

        uint64_t last_data_timestamp = 0ULL;

        IP4Addr peer_addr;
        uint16_t peer_port;

        int HandlePacket(const char *pkt, size_t n,
            IP4Addr src, uint16_t src_port,
            IP4Addr ddest, uint16_t dest_port,
            uint32_t seq_id, uint32_t ack_id,
            unsigned int flags,
            const char *opts, size_t optlen);

        bool SendToInt(const net_msg &m);

        int BindAsync(const sockaddr *addr, socklen_t addrlen, int *_errno);
        int ListenAsync(int backlog, int *_errno);
        int AcceptAsync(sockaddr *addr, socklen_t *addrlen, int *_errno);
        int RecvFromAsync(void *buf, size_t len, int flags,
            struct sockaddr *src_addr, socklen_t *addrlen, int *_errno);
        int SendToAsync(const void *buf, size_t len, int flags,
            const struct sockaddr *dest_addr, socklen_t addrlen, int *_errno);
        int CloseAsync(int *_errno);
};

class net_msg;
class UDPSocket : public IP4Socket
{
    public:
        UDPSocket() : IP4Socket(true) {}

        int BindAsync(const sockaddr *addr, socklen_t addrlen, int *_errno);
        int RecvFromAsync(void *buf, size_t len, int flags,
            struct sockaddr *src_addr, socklen_t *addrlen, int *_errno);

        int SendToAsync(const void *buf, size_t len, int flags,
            const struct sockaddr *dest_addr, socklen_t addrlen, int *_errno);
        bool SendToInt(const net_msg &m);

        int HandlePacket(const char *ptr, size_t n, uint32_t from_addr, uint16_t from_port);
        
        size_t operator()(const UDPSocket &s) const noexcept;
        bool operator==(const UDPSocket &other) const noexcept;
};

class RawSocket : public Socket
{

};

int net_bind_udpsocket(UDPSocket *sck);
int net_bind_tcpsocket(TCPSocket *sck);

int net_set_ip_address(const IP4Address &ip);
int net_delete_ip_address_for_iface(const NetInterface *iface);

/* comparison/hash functions for sockaddr_in */
struct sockaddr_pair
{
    sockaddr_in src, dest;

    struct sockaddr_pair dest_any() const
    {
        sockaddr_pair ret;
        ret.src = src;
        ret.dest = dest;
        ret.dest.sin_addr.s_addr = 0UL;
        return ret;
    }
};
namespace std
{
    template<> struct hash<sockaddr_in>
    {
        size_t operator()(const sockaddr_in &s) const noexcept
        {
            return std::hash<uint32_t>{}(s.sin_addr.s_addr) ^ std::hash<uint16_t>{}(s.sin_port);
        }
    };
    template<> struct equal_to<sockaddr_in>
    {
        bool operator()(const sockaddr_in &lhs, const sockaddr_in &rhs) const noexcept
        {
            if(lhs.sin_addr.s_addr != rhs.sin_addr.s_addr)
                return false;
            if(lhs.sin_port != rhs.sin_port)
                return false;
            return true;
        }
    };

    template<> struct hash<sockaddr_pair>
    {
        size_t operator()(const sockaddr_pair &s) const noexcept
        {
            return std::hash<sockaddr_in>{}(s.src) ^
                std::hash<sockaddr_in>{}(s.dest);
        }
    };
    template<> struct equal_to<sockaddr_pair>
    {
        bool operator()(const sockaddr_pair &lhs, const sockaddr_pair &rhs) const noexcept
        {
            if(lhs.src.sin_addr.s_addr != rhs.src.sin_addr.s_addr)
                return false;
            if(lhs.src.sin_port != rhs.src.sin_port)
                return false;
            if(lhs.dest.sin_addr.s_addr != rhs.dest.sin_addr.s_addr)
                return false;
            if(lhs.dest.sin_port != rhs.dest.sin_port)
                return false;
            return true;
        }
    };
}

/* socket interface for kernel threads */
#ifdef __cplusplus
extern "C" {
#endif
int     accept(int, struct sockaddr *, socklen_t *);
int     bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int     connect(int, const struct sockaddr *, socklen_t);
int     getpeername(int, struct sockaddr *, socklen_t *);
int     getsockname(int, struct sockaddr *, socklen_t *);
int     getsockopt(int, int, int, void *, socklen_t *);
int     listen(int, int);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
        struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t recvmsg(int, struct msghdr *, int);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t sendmsg(int, const struct msghdr *, int);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *,
        socklen_t);
int     setsockopt(int, int, int, const void *, socklen_t);
int     shutdown(int, int);
int     sockatmark(int);
int     socket(int domain, int type, int protocol);
int     socketpair(int, int, int, int [2]);
int     close(int);
#ifdef __cplusplus
}
#endif

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
        struct sockaddr *src_addr, socklen_t *addrlen, kernel_time until);

/* kernel threads which handle some services */
void *net_dhcpd_thread(void *p);

int net_queue_msg(const net_msg &m);
int net_ret_to_errno(int ret);

void net_udp_handle_recvfrom(const net_msg &m);
void net_udp_handle_sendto(const net_msg &m);
void net_ip_handle_set_ip_address(const net_msg &m);
void net_ip_handle_delete_ip_address_for_iface(const net_msg &m);

size_t net_ip_get_addresses(IP4Address *out, size_t naddrs, const NetInterface *iface = nullptr);
IP4Addr net_ip_get_address(const NetInterface *iface);
int net_ip_get_route_for_address(const IP4Addr &addr, IP4Route *route);
int net_ip_add_route(const IP4Route &route);
int net_ip_delete_routes_for_iface(const NetInterface *iface);

bool net_udp_decorate_packet(char *data, size_t datalen,
    const IP4Addr &dest, uint16_t dest_port,
    const IP4Addr &src, uint16_t src_port,
    bool release_buffer,
    const IP4Route *route = nullptr);
bool net_ip_decorate_packet(char *data, size_t datalen,
    const IP4Addr &dest, const IP4Addr &src,
    uint8_t protocol,
    bool release_buffer,
    const IP4Route *route = nullptr);
int net_ip_get_hardware_address_and_send(char *data, size_t datalen,
    const IP4Addr &dest,
    bool release_buffer,
    const IP4Route *route = nullptr);
int net_ip_get_hardware_address(const IP4Addr &dest, HwAddr *ret);
uint32_t net_ethernet_calc_crc(const char *data, size_t n);
void net_arp_handle_request_and_send(const net_msg &m);

uint32_t net_ip_calc_partial_checksum(const char *data, size_t n, uint32_t csum = 0);
uint16_t net_ip_complete_checksum(uint32_t partial_csum);
uint16_t net_ip_calc_checksum(const char *data, size_t n);

int net_arp_add_host(const IP4Addr &ip, const HwAddr &hw);

bool net_tcp_decorate_packet(char *data, size_t datalen,
    const IP4Addr &dest, uint16_t dest_port,
    const IP4Addr &src, uint16_t src_port,
    uint32_t seq_id, uint32_t ack_id,
    unsigned int flags,
    const char *opts, size_t optlen,
    uint16_t wndsize,
    bool release_buffer,
    const IP4Route *route = nullptr);
void net_tcp_handle_sendto(const net_msg &m);

int net_dhcpc_begin_for_iface(NetInterface *iface);

std::vector<std::pair<std::string, std::string>> net_get_known_networks();

struct net_ntpc_thread_params
{
    IP4Addr myaddr;
    int *sockfd;
};

void *net_ntpc_thread(void *p);

#endif
