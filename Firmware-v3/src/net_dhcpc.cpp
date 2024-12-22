#include "osnet.h"
#include "osnet_dhcp.h"
#include <unordered_map>
#include <map>
#include "clocks.h"
#include "SEGGER_RTT.h"

struct dhcpc_request
{
    NetInterface *iface;
    uint64_t made_at;
    UDPSocket *sck;
    uint32_t xid;
    uint64_t began_at;

    enum req_state_t { DiscoverSent, RequestSent, AwaitLeaseEnd };
    IP4Addr ip;
    uint64_t lease_end;
    req_state_t state;
};

SRAM4_DATA static std::unordered_map<const NetInterface *, dhcpc_request> reqs;

static void add_extra_request(char *buf, size_t *ptr,
    char code, char len, const char *msg)
{
    size_t sptr = *ptr;
    buf[sptr++] = code;
    buf[sptr++] = len;
    for(unsigned int i = 0; i < len; i++)
    {
        buf[sptr++] = msg[i];
    }
    *ptr = sptr;
}

static void add_extra_request(char *buf, size_t *ptr,
    char code, char len, uint32_t v)
{
    add_extra_request(buf, ptr, code, len,
        reinterpret_cast<const char *>(&v));
}

static void add_extra_requests(char *buf, size_t *ptr,
    int reqtype,
    const IP4Addr *req_ip = nullptr,
    const uint32_t *serv_id = nullptr)
{
    add_extra_request(buf, ptr, 53, 1, reqtype);
    if(req_ip)
        add_extra_request(buf, ptr, 50, 4, req_ip->get());
    if(serv_id)
        add_extra_request(buf, ptr, 54, 5, *serv_id);
    
    /* req parameter list:
        1 = subnet mask
        3 = router
        6 = dns server
        15 = domain name
    */
   char plist[] = { 1, 3, 6, 15 };
   add_extra_request(buf, ptr, 55, sizeof(plist), plist);

   // terminate
   buf[*ptr] = 0xff;
   *ptr = *ptr + 1;
}

static int send_discover(dhcpc_request &dr)
{
    auto pbuf = net_allocate_pbuf(PBUF_SIZE);
    if(!pbuf)
    {
        return NET_NOMEM;
    }

    memset(pbuf, 0, PBUF_SIZE);
    auto data = &pbuf[NET_SIZE_UDP_OFFSET];

    dr.xid = dr.xid + 1UL;
    dr.began_at = clock_cur_ms();

    // build route to force use of broadcast
    //  on this interface
    IP4Route route;
    route.addr.iface = dr.iface;
    route.addr.addr = 0UL;
    route.addr.gw = 0UL;

    size_t datalen = OFFSET_OPTIONS;

    data[0] = 1;    // BOOTREQUEST
    data[1] = 1;
    data[2] = 6;
    data[3] = 0;

    //*reinterpret_cast<uint16_t *>(&data[OFFSET_FLAGS]) = htons(0x8000);    // set broadcast to see response on wireshark

    *reinterpret_cast<uint32_t *>(&data[OFFSET_XID]) = dr.xid;
    *reinterpret_cast<uint16_t *>(&data[OFFSET_SECS]) = htons((clock_cur_ms() - dr.began_at) / 1000ULL);
    *reinterpret_cast<uint16_t *>(&data[OFFSET_FLAGS]) = 0;
    memcpy(&data[OFFSET_CHADDR], dr.iface->GetHwAddr().get(), 6);

    *reinterpret_cast<uint32_t *>(&data[OFFSET_OPTIONS]) = ntohl(DHCP_MAGIC_NB);
    datalen += 4;

    // extra requests
    add_extra_requests(data, &datalen, DHCPDISCOVER);

    dr.made_at = clock_cur_ms();
    dr.state = dhcpc_request::DiscoverSent;

    return net_udp_decorate_packet(data, datalen, 0xffffffffUL, htons(67),
        0x00000000UL, dr.sck->port, true, &route);
}

static int send_request(dhcpc_request &dr, const IP4Addr &yiaddr,
    const IP4Addr &siaddr, const uint32_t *servid)
{
    auto pbuf = net_allocate_pbuf(PBUF_SIZE);
    if(!pbuf)
    {
        return NET_NOMEM;
    }

    memset(pbuf, 0, PBUF_SIZE);
    auto data = &pbuf[NET_SIZE_UDP_OFFSET];

    // build route to force use of broadcast
    //  on this interface
    IP4Route route;
    route.addr.iface = dr.iface;
    route.addr.addr = 0UL;
    route.addr.gw = 0UL;

    size_t datalen = OFFSET_OPTIONS;

    data[0] = 1;    // BOOTREQUEST
    data[1] = 1;
    data[2] = 6;
    data[3] = 0;

    //*reinterpret_cast<uint16_t *>(&data[OFFSET_FLAGS]) = htons(0x8000);    // set broadcast to see response on wireshark

    *reinterpret_cast<uint32_t *>(&data[OFFSET_XID]) = dr.xid;
    *reinterpret_cast<uint16_t *>(&data[OFFSET_SECS]) = htons((clock_cur_ms() - dr.began_at) / 1000ULL);
    *reinterpret_cast<uint16_t *>(&data[OFFSET_FLAGS]) = 0;
    *reinterpret_cast<uint32_t *>(&data[OFFSET_SIADDR]) = siaddr;
    memcpy(&data[OFFSET_CHADDR], dr.iface->GetHwAddr().get(), 6);

    *reinterpret_cast<uint32_t *>(&data[OFFSET_OPTIONS]) = ntohl(DHCP_MAGIC_NB);
    datalen += 4;

    // extra requests
    add_extra_requests(data, &datalen, DHCPREQUEST, &yiaddr, servid);

    dr.made_at = clock_cur_ms();
    dr.state = dhcpc_request::RequestSent;

    return net_udp_decorate_packet(data, datalen, 0xffffffffUL, htons(67),
        0x00000000UL, dr.sck->port, true, &route);
}

int net_dhcpc_begin_for_iface(NetInterface *iface)
{
    auto iter = reqs.find(iface);
    if(iter != reqs.end())
    {
        auto &curreq = iter->second;
        if(clock_cur_ms() < curreq.made_at + 5000)
        {
            // no need to try again
            return NET_OK;
        }
        else
        {
            // timeout, try again
            {
                klog("dhcpc: timeout in state %d, restarting\n",
                    curreq.state);
            }
            return send_discover(curreq);
        }
    }
    else
    {
        {
            klog("dhcpc: new request\n");
        }
        // store that we've made a request
        dhcpc_request dr;
        dr.iface = iface;
        dr.sck = new UDPSocket();
        dr.sck->port = htons(68);
        dr.xid = rand();
        return send_discover(reqs.insert_or_assign(iface, dr).first->second);
    }
}

static const char *getopt(int id, const std::map<int, int> &opts, const char *buf, size_t *len = nullptr)
{
    auto iter = opts.find(id);
    if(iter == opts.end())
    {
        return nullptr;
    }
    else
    {
        if(len)
            *len = buf[iter->second + 1];
        return &buf[iter->second + 2];
    }
}

template <typename T> T opt_or_empty(int id, const std::map<int, int> &opts, const char *buf)
{
    auto optval = getopt(id, opts, buf);
    if(optval)
    {
        return *reinterpret_cast<const T *>(optval);
    }
    else
    {
        return T();
    }
}
template <> IP4Addr opt_or_empty<IP4Addr>(int id, const std::map<int, int> &opts, const char *buf)
{
    auto optval = getopt(id, opts, buf);
    if(optval)
        return IP4Addr(optval);
    else
        return IP4Addr(0UL);
}
template <> std::string opt_or_empty<std::string>(int id, const std::map<int, int> &opts, const char *buf)
{
    size_t len;
    auto optval = getopt(id, opts, buf, &len);
    if(optval)
        return std::string(optval, len);
    else
        return std::string();
}

int net_handle_dhcpc_packet(const UDPPacket &pkt)
{
    {
        klog("dhcpc: received packet\n");
    }

    // see if we have a ongoing request for this iface
    auto iter = reqs.find(pkt.ippacket.epacket.iface);
    if(iter == reqs.end())
    {
        return NET_NOTUS;
    }
    auto &dr = iter->second;

    // parse common fields
    auto bprot = pkt.contents[0];
    if(bprot != 2)  // BOOTREPLY
        return NET_NOTSUPP;
    
    auto xid = *reinterpret_cast<const uint32_t *>(&pkt.contents[OFFSET_XID]);
    if(xid != dr.xid)
        return NET_NOTUS;

    if(memcmp(&pkt.contents[OFFSET_CHADDR], dr.iface->GetHwAddr().get(), 6) != 0)
        return NET_NOTUS;

    auto yiaddr = IP4Addr(&pkt.contents[OFFSET_YIADDR]);

    std::map<int, int> opts;
    auto options = &pkt.contents[OFFSET_OPTIONS];
    auto opt_len = pkt.n - OFFSET_OPTIONS;
    int msg_type = -1;

    if(options[0] == 99 && options[1] == 130 && options[2] == 83 && options[3] == 99)
    {
        options += 4;
        opt_len -= 4;

        unsigned int i = 0;
        while(i < opt_len)
        {
            auto start_idx = i + OFFSET_OPTIONS + 4;
            auto field_id = options[i++];

            switch(field_id)
            {
                case 255:   // end
                    i = opt_len;
                    break;

                case 0:
                    // pad
                    break;

                default:
                    auto len = options[i++];

                    switch(field_id)
                    {
                        case 53:
                            msg_type = options[i];
                            /* fall-through */

                        default:
                            opts[field_id] = start_idx;
                            i += len;
                            break;
                    }
            }
        }
    }
    if(msg_type == -1)
    {
        return NET_NOTSUPP;
    }

    {
        klog("dhcpc: msg_type: %d, state: %d, yiaddr: %s\n",
            (int)msg_type, (int)dr.state,
            yiaddr.ToString().c_str());
    }

    switch(dr.state)
    {
        case dhcpc_request::DiscoverSent:
            // expect DHCPOFFER
            if(msg_type == DHCPOFFER)
            {
                // send DHCPREQUEST
                const uint32_t *pservid = nullptr;
                auto oiter = opts.find(54);
                if(oiter != opts.end())
                {
                    pservid = reinterpret_cast<const uint32_t *>(&pkt.contents[oiter->second + 2]);
                }
                return send_request(dr, yiaddr, pkt.ippacket.src, pservid);
            }
            break;

        case dhcpc_request::RequestSent:
            // expect DHCPACK
            if(msg_type == DHCPACK)
            {
                auto ip = yiaddr;
                auto nm = opt_or_empty<IP4Addr>(1, opts, pkt.contents);
                auto gw = opt_or_empty<IP4Addr>(3, opts, pkt.contents);
                auto dns = opt_or_empty<IP4Addr>(6, opts, pkt.contents);
                auto domain = opt_or_empty<std::string>(15, opts, pkt.contents);
                auto lease = ntohl(opt_or_empty<uint32_t>(51, opts, pkt.contents));

                if(nm == 0UL) nm = 0xffffffff;
                klog("dhcpc: DHCPACK received\n");
                klog("  IP: %s\n  NM: %s\n  GW: %s\n  DNS: %s\n  domain name: %s\n  lease: %d\n",
                    ip.ToString().c_str(),
                    nm.ToString().c_str(),
                    gw.ToString().c_str(),
                    dns.ToString().c_str(),
                    domain.c_str(),
                    lease);

                dr.ip = ip;
                dr.lease_end = dr.made_at + static_cast<uint64_t>(lease) * 1000;
                dr.state = dhcpc_request::AwaitLeaseEnd;

                {
                    // add ip address
                    IP4Address addr;
                    addr.iface = dr.iface;
                    addr.addr = ip;
                    addr.nm = nm;
                    addr.gw = 0UL;
                    net_set_ip_address(addr);

                    // add route to local net
                    IP4Route route;
                    route.addr = addr;
                    route.metric = 50;
                    net_ip_add_route(route);

                    // add default route
                    if(gw != 0UL)
                    {
                        IP4Address gw_addr;
                        gw_addr.iface = dr.iface;
                        gw_addr.addr = 0UL;
                        gw_addr.nm = 0xffffffffUL;
                        gw_addr.gw = gw;

                        IP4Route def_route;
                        def_route.addr = gw_addr;
                        def_route.metric = 60;
                        net_ip_add_route(def_route);
                    }
                }

                return NET_OK;
            }
            break;

        default:
            // nothing
            break;

    }

    {
        klog("dhcpc: msg %d in state %d\n",
            (int)msg_type, (int)dr.state);
    }
    return NET_NOTSUPP;
}