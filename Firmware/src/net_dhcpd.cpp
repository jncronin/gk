#include "osnet.h"
#include "thread.h"
#include "SEGGER_RTT.h"

#include <map>

extern Spinlock s_rtt;

#define DHCPDISCOVER        1
#define DHCPOFFER           2
#define DHCPREQUEST         3
#define DHCPDECLINE         4
#define DHCPACK             5
#define DHCPNACK            6
#define DHCPRELEASE         7
#define DHCPINFORM          8

#define OFFSET_XID          4
#define OFFSET_CIADDR       12
#define OFFSET_YIADDR       16
#define OFFSET_SIADDR       20
#define OFFSET_GIADDR       24
#define OFFSET_CHADDR       28
#define OFFSET_SNAME        44
#define OFFSET_FILE         108
#define OFFSET_OPTIONS      236

#define DHCP_MAGIC_NB       0x63825363

/* DHCP server particularly suited for provisioning devices connected via the USB RNDIS port

    Based upon RFC 2131 i.e. DHCP v4
    The valid options are in RFC 2132
*/

static void build_response(const char *inreq, char *resp, IP4Addr *dest, const sockaddr_in *caddr)
{
    memset(resp, 0, OFFSET_OPTIONS);

    resp[0] = 2;    // BOOTREPLY
    resp[1] = 1;
    resp[2] = 6;
    resp[3] = 0;

    *reinterpret_cast<uint32_t *>(&resp[OFFSET_XID]) = *reinterpret_cast<const uint32_t *>(&inreq[OFFSET_XID]);
    *reinterpret_cast<uint32_t *>(&resp[OFFSET_CIADDR]) = *reinterpret_cast<const uint32_t *>(&inreq[OFFSET_CIADDR]);
    *reinterpret_cast<uint32_t *>(&resp[OFFSET_GIADDR]) = *reinterpret_cast<const uint32_t *>(&inreq[OFFSET_GIADDR]);

    memcpy(&resp[OFFSET_CHADDR], &inreq[OFFSET_CHADDR], 6);

    strcpy(&resp[OFFSET_SNAME], "gk");

    *reinterpret_cast<uint32_t *>(&resp[OFFSET_OPTIONS]) = ntohl(DHCP_MAGIC_NB);

    // determine destination
    IP4Addr giaddr(&inreq[OFFSET_GIADDR]);
    IP4Addr ciaddr(&inreq[OFFSET_CIADDR]);
    auto flags = *reinterpret_cast<uint16_t *>(&resp[6]);
    if(giaddr != 0UL)
    {
        // return via gateway
        *dest = giaddr;
    }
    else if(flags)
    {
        // broadcast
        *dest = IP4Addr(0xffffffffUL);
    }
    else
    {
        *dest = ciaddr;
    }
}

static bool add_option(int id, int len, void *val, int *offset, char *resp, int max_offset)
{
    int req_len = (len < 0) ? 1 : (2 + len);    // either 1 byte or id + len + data[len]
    if((*offset + req_len) > max_offset)
        return false;
    
    resp[*offset] = id;
    if(len < 0)
    {
        *offset = *offset + 1;
        return true;
    }

    resp[*offset + 1] = static_cast<char>(len);

    memcpy(&resp[*offset + 2], val, len);

    *offset = *offset + 2 + len;
    return true;
}

static bool add_option(int id, int len, uint32_t val, int *offset, char *resp, int max_offset)
{
    return add_option(id, len, reinterpret_cast<void *>(&val), offset, resp, max_offset);
}

static bool add_requested_option(int req_id, int *out_offset, char *resp, int max_offset)
{
    // 1, 3, 6, 15, 31, 33, 43, 44, 46, 47, 119, 121, 249, 252
    switch(req_id)
    {
        case 1:
            // subnet mask
            return add_option(1, 4, 0x00ffffffUL, out_offset, resp, max_offset);

        case 3:
            // router
            // we have none
            break;

        case 6:
            // dns server
            // we have none
            break;

        case 15:
            // domain name
            {
                char domainname[] = "gk";
                return add_option(15, strlen(domainname) + 1, domainname, out_offset, resp, max_offset);
            }

        case 31:
            // perform router discovery option
            return add_option(31, 1, 0UL, out_offset, resp, max_offset);

        case 33:
            // static routes
            break;

        case 43:
            // vendor specific info
            break;

        case 44:
            // netbios name server
            break;

        case 46:
            // netbios node type
            break;

        case 47:
            // netbios scope
            break;

        default:
            break;
    }
    return true;
}

static bool add_requested_options(const char *inreq, int req_offset, int *out_offset, char *resp, int max_offset)
{
    int nitems = inreq[req_offset + 1];
    for(int i = 0; i < nitems; i++)
    {
        if(!add_requested_option(inreq[req_offset + 2 + i], out_offset, resp, max_offset))
            return false;
    }
    return true;
}

static void send_response(int sockfd, const char *buf, size_t len, const IP4Addr &dest)
{
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = dest;
    addr.sin_port = htons(68);

    sendto(sockfd, buf, len, 0, reinterpret_cast<const sockaddr *>(&addr), sizeof(sockaddr_in));
}

static void handle_dhcpdiscover(int sockfd, const char *buf, const std::map<int, int> &options, const sockaddr_in *caddr)
{
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "dhcpd: DHCPDISCOVER received, options:\n");
        for(const auto &opt : options)
        {
            SEGGER_RTT_printf(0, "  %d\n", opt.first);
        }
    }

    // build response packet
    char resp[512];
    IP4Addr resp_dest;
    build_response(buf, resp, &resp_dest, caddr);

    int offset = OFFSET_OPTIONS + 4;

    // add DHCPOFFER response
    add_option(53, 1, DHCPOFFER, &offset, resp, sizeof(resp));

    // MUST include lease time
    add_option(51, 4, htonl(600), &offset, resp, sizeof(resp));

    // SHOULD include message
    char msg[] = "OK";
    add_option(56, strlen(msg) + 1, msg, &offset, resp, sizeof(resp));

    // MUST include server identifier
    // We need to get our IP address on the subnet that received this message
    IP4Address ipaddrs[8];
    IP4Addr from(caddr->sin_addr.s_addr);
    IP4Addr servaddr(0UL);
    auto nipaddrs = net_ip_get_addresses(ipaddrs, 8);
    for(unsigned int i = 0; i < nipaddrs; i++)
    {
        if(IP4Addr::Compare(from, ipaddrs[i].addr, ipaddrs[i].nm))
        {
            servaddr = ipaddrs[i].addr;
            break;
        }
    }
    if(servaddr == 0UL && nipaddrs > 0)
    {
        servaddr = ipaddrs[0].addr;
    }
    add_option(54, 4, servaddr, &offset, resp, sizeof(resp));

    // add requested data
    if(auto req_offset = options.find(55); req_offset != options.end())
    {
        add_requested_options(buf, req_offset->second, &offset, resp, sizeof(resp));
    }

    // offer the client an IP
    *reinterpret_cast<uint32_t *>(&resp[OFFSET_YIADDR]) = 0x0207a8c0;

    // pad
    bool valid = add_option(255, -1, 0UL, &offset, resp, sizeof(resp));
    if(valid)
    {
        // send packet
        send_response(sockfd, resp, offset, resp_dest);
    }
}

static void handle_dhcprequest(int sockfd, const char *buf, const std::map<int, int> &options, const sockaddr_in *caddr)
{
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "dhcpd: DHCPREQUEST received, options:\n");
        for(const auto &opt : options)
        {
            SEGGER_RTT_printf(0, "  %d\n", opt.first);
        }
    }

    // build response packet
    char resp[512];
    IP4Addr resp_dest;
    build_response(buf, resp, &resp_dest, caddr);

    int offset = OFFSET_OPTIONS + 4;

    // add DHCPACK response
    add_option(53, 1, DHCPACK, &offset, resp, sizeof(resp));

    // MUST include lease time
    add_option(51, 4, htonl(600), &offset, resp, sizeof(resp));

    // SHOULD include message
    char msg[] = "OK";
    add_option(56, strlen(msg) + 1, msg, &offset, resp, sizeof(resp));

    // MUST include server identifier
    // We need to get our IP address on the subnet that received this message
    IP4Address ipaddrs[8];
    IP4Addr from(caddr->sin_addr.s_addr);
    IP4Addr servaddr(0UL);
    auto nipaddrs = net_ip_get_addresses(ipaddrs, 8);
    for(unsigned int i = 0; i < nipaddrs; i++)
    {
        if(IP4Addr::Compare(from, ipaddrs[i].addr, ipaddrs[i].nm))
        {
            servaddr = ipaddrs[i].addr;
            break;
        }
    }
    if(servaddr == 0UL && nipaddrs > 0)
    {
        servaddr = ipaddrs[0].addr;
    }
    add_option(54, 4, servaddr, &offset, resp, sizeof(resp));

    // add requested data
    if(auto req_offset = options.find(55); req_offset != options.end())
    {
        add_requested_options(buf, req_offset->second, &offset, resp, sizeof(resp));
    }

    // offer the client an IP
    *reinterpret_cast<uint32_t *>(&resp[OFFSET_YIADDR]) = 0x0207a8c0;

    // pad
    bool valid = add_option(255, -1, 0UL, &offset, resp, sizeof(resp));
    if(valid)
    {
        // send packet
        send_response(sockfd, resp, offset, resp_dest);
    }
}


static void handle_dhcpinform(int sockfd, const char *buf, const std::map<int, int> &options, const sockaddr_in *caddr)
{
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "dhcpd: DHCPINFORM received, options:\n");
        for(const auto &opt : options)
        {
            SEGGER_RTT_printf(0, "  %d\n", opt.first);
        }
    }

    // build response packet
    char resp[512];
    IP4Addr resp_dest;
    build_response(buf, resp, &resp_dest, caddr);

    int offset = OFFSET_OPTIONS + 4;

    // add DHCPACK response
    add_option(53, 1, DHCPACK, &offset, resp, sizeof(resp));

    // SHOULD include message
    char msg[] = "OK";
    add_option(56, strlen(msg) + 1, msg, &offset, resp, sizeof(resp));

    // MUST include server identifier
    // We need to get our IP address on the subnet that received this message
    IP4Address ipaddrs[8];
    IP4Addr from(caddr->sin_addr.s_addr);
    IP4Addr servaddr(0UL);
    auto nipaddrs = net_ip_get_addresses(ipaddrs, 8);
    for(unsigned int i = 0; i < nipaddrs; i++)
    {
        if(IP4Addr::Compare(from, ipaddrs[i].addr, ipaddrs[i].nm))
        {
            servaddr = ipaddrs[i].addr;
            break;
        }
    }
    if(servaddr == 0UL && nipaddrs > 0)
    {
        servaddr = ipaddrs[0].addr;
    }
    add_option(54, 4, servaddr, &offset, resp, sizeof(resp));

    // add requested data
    if(auto req_offset = options.find(55); req_offset != options.end())
    {
        add_requested_options(buf, req_offset->second, &offset, resp, sizeof(resp));
    }

    // pad
    bool valid = add_option(255, -1, 0UL, &offset, resp, sizeof(resp));
    if(valid)
    {
        // send packet
        send_response(sockfd, resp, offset, resp_dest);
    }
}

void net_dhcpd_thread(void *p)
{
    (void)p;

    int lsck = socket(AF_INET, SOCK_DGRAM, 0);
    if(lsck < 0)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "dhcpd: socket failed %d\n", errno);
        return;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = 0;
    saddr.sin_port = htons(67);

    int ret = bind(lsck, reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr));
    if(ret < 0)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "dhcpd: bind failed %d\n", errno);
        return;
    }

    while(true)
    {
        char buf[512];
        sockaddr_in caddr;
        socklen_t caddrlen = sizeof(caddr);
        ret = recvfrom(lsck, buf, 512, 0, reinterpret_cast<sockaddr *>(&caddr), &caddrlen);

        if(ret == -1)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "dhcpd: recvfrom failed %d\n", errno);
            return;
        }
        else
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "dhcpd: received %d bytes\n", ret);
        }

        // parse fields
        if(buf[0] == 1 && buf[1] == 1 && buf[2] == 6)
        {
            // valid
            auto xid = *reinterpret_cast<uint32_t *>(&buf[OFFSET_XID]);
            auto chaddr = HwAddr(&buf[OFFSET_CHADDR]);
            auto ciaddr = IP4Addr(&buf[OFFSET_CIADDR]);
            auto giaddr = IP4Addr(&buf[OFFSET_GIADDR]);

            auto options = &buf[OFFSET_OPTIONS];
            auto opt_len = ret - OFFSET_OPTIONS;
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "dhcpd: received BOOTREQUEST from %s (%s), giaddr: %s, xid: %x\n",
                    chaddr.ToString().c_str(),
                    ciaddr.ToString().c_str(),
                    giaddr.ToString().c_str(),
                    (unsigned int)xid);

                SEGGER_RTT_printf(0, "dhcpd: magic %d %d %d %d\n", options[0], options[1], options[2], options[3]);
            }

            if(options[0] == 99 && options[1] == 130 && options[2] == 83 && options[3] == 99)
            {
                options += 4;
                opt_len -= 4;

                std::map<int, int> opts;

                int i = 0;
                int msg_type = -1;
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

                switch(msg_type)
                {
                    case DHCPDISCOVER:
                        handle_dhcpdiscover(lsck, buf, opts, &caddr);
                        break;

                    case DHCPINFORM:
                        handle_dhcpinform(lsck, buf, opts, &caddr);
                        break;

                    case DHCPREQUEST:
                        handle_dhcprequest(lsck, buf, opts, &caddr);
                        break;
                }
            }
        }
    }
}
