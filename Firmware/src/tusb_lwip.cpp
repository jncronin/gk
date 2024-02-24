#include <cstdint>
#include <cstring>
#include <lwip/sys.h>
#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/ethip6.h>
#include <lwip/init.h>
#include <lwip/etharp.h>
#include "dhserver.h"
#include <tusb.h>
#include <SEGGER_RTT.h>
#include <osmutex.h>

extern Spinlock s_rtt;

#define LWIP_DATA __attribute__((section(".lwip_data")))
LWIP_DATA uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

LWIP_DATA static struct pbuf *received_frame;
LWIP_DATA static struct netif netif_data;

#define INIT_IP4(a,b,c,d) { PP_HTONL(LWIP_MAKEU32(a,b,c,d)) }
LWIP_DATA static ip4_addr_t ipaddr  = INIT_IP4(192, 168, 7, 1);
LWIP_DATA static ip4_addr_t netmask = INIT_IP4(255, 255, 255, 0);
LWIP_DATA static ip4_addr_t gateway = INIT_IP4(0, 0, 0, 0);

LWIP_DATA static dhcp_entry_t entries[] =
{
    /* mac ip address                          lease time */
    { {0}, INIT_IP4(192, 168, 7, 2), 24 * 60 * 60 },
    { {0}, INIT_IP4(192, 168, 7, 3), 24 * 60 * 60 },
    { {0}, INIT_IP4(192, 168, 7, 4), 24 * 60 * 60 },
};

LWIP_DATA static dhcp_config_t dhcp_config =
{
    .router = ipaddr,            /* router address (if any) */
    .port = 67,                                /* listen port */
    .dns = INIT_IP4(8, 8, 8, 8),               /* dns server (if any) */
    .listen_on = ipaddr,
    "usb",                                     /* dns suffix */
    TU_ARRAY_SIZE(entries),                    /* num entry */
    entries                                    /* entries */
};

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    /* this shouldn't happen, but if we get another packet before
    parsing the previous, we must signal our inability to accept it */
    if (received_frame) return false;

    if (size)
    {
        struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);

        if (p)
        {
            /* pbuf_alloc() has already initialized struct; all we need to do is copy the data */
            memcpy(p->payload, src, size);

            /* store away the pointer for service_traffic() to later handle */
            received_frame = p;
        }
    }

    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    struct pbuf *p = (struct pbuf *)ref;

    (void)arg; /* unused for this example */

    return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

void tusb_lwip_service_traffic(void)
{
    /* handle any packet received by tud_network_recv_cb() */
    if (received_frame)
    {
        tcpip_input(received_frame, &netif_data);
        //pbuf_free(received_frame);
        received_frame = NULL;
        tud_network_recv_renew();
    }

    sys_check_timeouts();
}

static err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
    (void)netif;

    for (;;)
    {
        /* if TinyUSB isn't ready, we must signal back to lwip that there is nothing we can do */
        if (!tud_ready())
            return ERR_USE;

        /* if the network driver can accept another packet, we make it happen */
        if (tud_network_can_xmit(p->tot_len))
        {
            tud_network_xmit(p, 0 /* unused for this example */);
            return ERR_OK;
        }

        /* transfer execution to TinyUSB in the hopes that it will finish transmitting the prior packet */
        //tud_task();
    }
}

static err_t ip4_output_fn(struct netif *netif, struct pbuf *p, const ip4_addr_t *addr)
{
    return etharp_output(netif, p, addr);
}

#if LWIP_IPV6
static err_t ip6_output_fn(struct netif *netif, struct pbuf *p, const ip6_addr_t *addr)
{
    return ethip6_output(netif, p, addr);
}
#endif

static err_t netif_init_cb(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->mtu = CFG_TUD_NET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'T';
    netif->name[1] = 'U';
    netif->linkoutput = linkoutput_fn;
    netif->output = ip4_output_fn;
#if LWIP_IPV6
    netif->output_ip6 = ip6_output_fn;
#endif
    return ERR_OK;
}

void tud_network_init_cb(void)
{
    /* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
    if (received_frame)
    {
        pbuf_free(received_frame);
        received_frame = NULL;
    }

    netif_data.hwaddr_len = sizeof(tud_network_mac_address);
    memcpy(netif_data.hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
    netif_data.hwaddr[5] ^= 0x1;

    auto netif = netif_add(&netif_data, &ipaddr, &netmask, &gateway, NULL, netif_init_cb, tcpip_input);
#if LWIP_IPV6
    netif_create_ip6_linklocal_address(netif, 1);
#endif
    netif_set_default(netif);

    dhserv_init(&dhcp_config);
}
