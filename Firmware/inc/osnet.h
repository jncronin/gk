#ifndef OSNET_H
#define OSNET_H

#include <stddef.h>
#include <memory>
#include <string>

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


#define PBUF_SIZE       1542

int net_inject_ethernet_packet(const char *buf, size_t n, const NetInterface &iface);
char *net_allocate_pbuf();
void net_deallocate_pbuf(char *);

int net_handle_ip4_packet(const char *buf, size_t n,
    const HwAddr &hw_src, const HwAddr &hw_dest,
    const NetInterface &iface);

#endif
