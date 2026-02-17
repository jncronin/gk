#include <stm32mp2xx.h>
#include "pins.h"
#include "vmem.h"
#include "clocks.h"
#include "sdif.h"
#include "logger.h"

#include "wifi_airoc_if.h"
#include "cybsp.h"
#include "cybsp_wifi.h"
#include "cy_network_buffer.h"
#include "cyabs_rtos.h"
#include "whd_types.h"
#include "cyhal.h"

#define DEBUG_WIFI 0


extern "C" void bt_post_reset_cback(void);

#define GPIOC_VMEM (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE)
static const constexpr pin WIFI_REG_ON { GPIOC_VMEM, 7 };
static const constexpr pin BT_REG_ON { GPIOC_VMEM, 8 };

#define GPIOI_VMEM (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOI_BASE)
static const constexpr pin MCO1 { GPIOI_VMEM, 6, 1 };

#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))

static whd_init_config_t init_config_default =
{
    .thread_stack_start = NULL,
    .thread_stack_size  = 65536,
    .thread_priority    = (uint32_t)2,
    .country            = CY_WIFI_COUNTRY
};

static whd_buffer_funcs_t buffer_if_default =
{
    .whd_host_buffer_get                       = cy_host_buffer_get,
    .whd_buffer_release                        = cy_buffer_release,
    .whd_buffer_get_current_piece_data_pointer = cy_buffer_get_current_piece_data_pointer,
    .whd_buffer_get_current_piece_size         = cy_buffer_get_current_piece_size,
    .whd_buffer_set_size                       = cy_buffer_set_size,
    .whd_buffer_add_remove_at_front            = cy_buffer_add_remove_at_front,
};

static whd_netif_funcs_t netif_if_default =
{
    .whd_network_process_ethernet_data = cy_network_process_ethernet_data,
};

extern whd_resource_source_t resource_ops;

void init_wifi_airoc()
{
    RCC_VMEM->GPIOCCFGR |= RCC_GPIOCCFGR_GPIOxEN;
    RCC_VMEM->GPIOICFGR |= RCC_GPIOICFGR_GPIOxEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    // Enable 32kHz sleep clock
    RCC_VMEM->FCALCOBS0CFGR = RCC_FCALCOBS0CFGR_CKOBSEN |
        (0x86U << RCC_FCALCOBS0CFGR_CKINTSEL_Pos);      // LSE
    RCC_VMEM->MCO1CFGR = RCC_MCO1CFGR_MCO1ON | RCC_MCO1CFGR_MCO1SEL;
    __asm__ volatile("dsb sy\n" ::: "memory");
    MCO1.set_as_af();

    init_sdmmc2();

    WIFI_REG_ON.set_as_output();
    WIFI_REG_ON.clear();
    BT_REG_ON.set_as_output();
    BT_REG_ON.set();

    sdmmc[1].supply_off = []() { WIFI_REG_ON.clear(); udelay(10000); return 0; };
    sdmmc[1].supply_on = []() { WIFI_REG_ON.set(); BT_REG_ON.set(); udelay(250000); return 0; };

    //wifi_airoc_reset();
}

int WifiAirocNetInterface::wifi_airoc_reset()
{
    MutexGuard mg(sdmmc[1].m);

    if(sdmmc[1].reset() != 0)
        return -1;
    
    sdmmc[1].sdio_enable_function(0, true);
    sdmmc[1].sdio_enable_function(1, true);
    sdmmc[1].sdio_enable_function(2, true);

    // read some tuples
    unsigned int f0_blk_size = 0;
    unsigned int manf_code = 0;
    unsigned int part_no = 0;

    unsigned int base_addr = 0x9;
    unsigned int cis_addr = ((unsigned int)sdmmc[1].cccr[base_addr]) |
        (((unsigned int)sdmmc[1].cccr[base_addr + 1]) << 8) |
        (((unsigned int)sdmmc[1].cccr[base_addr + 2]) << 16);

    while(cis_addr)
    {
        std::array<uint8_t, 256U> tuple{};
        uint8_t tid;
        size_t len;
        auto ret = sdmmc[1].sdio_read_tuple<256U>(&cis_addr, &tid, &len, tuple);
        if(ret != 0)
            return -1;
                
        switch(tid)
        {
            case 0x20:
                manf_code = ((unsigned int)tuple[0]) |
                        (((unsigned int)tuple[1]) << 8);
                part_no = ((unsigned int)tuple[2]) |
                        (((unsigned int)tuple[3]) << 8);
                klog("airoc: manf: %x, part_no: %x\n", manf_code, part_no);
                break;
                
            case 0x22:
                if(tuple[0] == 0)
                {
                    f0_blk_size = ((unsigned int)tuple[1]) |
                        (((unsigned int)tuple[2]) << 8);
                    klog("airoc: f0_blk_size: %u\n", f0_blk_size);
                    klog("airoc: max_tran_speed: %x\n", tuple[3]);
                }
                break;
        }
    }

    if(manf_code != 0x04b4 || part_no != 0xbd3d)
    {
        klog("airoc: invalid manf/part no\n");
        return -1;
    }

    sdmmc[1].sdio_set_func_block_size(0, f0_blk_size);
    sdmmc[1].sdio_set_func_block_size(1, 64);
    sdmmc[1].sdio_set_func_block_size(2, 64);

    mg.unlock();

    whd_init(&whd_drv, &init_config_default, &resource_ops, &buffer_if_default,
        &netif_if_default);

    whd_sdio_config_t whd_sdio_config =
    {
        .sdio_1bit_mode        = WHD_FALSE,
        .high_speed_sdio_clock = WHD_TRUE,
        .oob_config            = { 
            .host_oob_pin = 0x6,        // GPIOA6
            .drive_mode = CYHAL_GPIO_DRIVE_NONE,
            .init_drive_state = WHD_FALSE,
            .dev_gpio_sel = 0,
            .is_falling_edge = WHD_TRUE,
            .intr_priority = 0
        }
    };
    whd_bus_sdio_attach(whd_drv, &whd_sdio_config, nullptr);

    auto ret = whd_wifi_on(whd_drv, &whd_iface) == WHD_SUCCESS ? 0 : -1;
    if(ret != 0)
        return ret;

    whd_wifi_set_private_data(whd_iface, this);
    
    whd_mac_t macaddr;
    if(whd_wifi_get_mac_address(whd_iface, &macaddr) != WHD_SUCCESS)
    {
        klog("airoc: failed to get mac address\n");
        return -1;
    }

    hwaddr = HwAddr((const char *)macaddr.octet);
    klog("airoc: MAC address %s\n", hwaddr.ToString().c_str());
    klog("airoc: interface up\n");

    //bt_post_reset_cback();
    return 0;
}

int WifiAirocNetInterface::HardwareEvent(const netiface_msg &msg)
{
    switch(msg.msg_subtype)
    {
        case AIROC_MSG_SUBTYPE_SCAN_COMPLETE:
            {
                CriticalGuard cg(sl_new_networks);
                if(new_networks_ready)
                {
                    networks = new_networks;
                    new_networks_ready = false;
                }
                scan_in_progress = false;
                last_scan_time = clock_cur();
            }
            break;

        default:
            klog("airoc: unhandled hwevent %u\n", msg.msg_subtype);
            break;
    }
    return 0;
}

int WifiAirocNetInterface::SendEthernetPacket(pbuf_t buf, const HwAddr &dest, uint16_t ethertype,
            bool release_buffer)
{
    net_ethernet_decorate_packet(buf, hwaddr, dest, ethertype, true);

    if(!release_buffer)
        buf->AddReference();

#if DEBUG_WIFI
    net_dump_pbuf("airoc: send_packet\n", buf);
#endif

    whd_network_send_ethernet_data(whd_iface, (whd_buffer_t)buf);
    return 0;
}

void cy_network_process_ethernet_data(whd_interface_t interface, whd_buffer_t buffer)
{
    WifiNetInterface *iface;
    whd_wifi_get_private_data(interface, (void **)&iface);

#if DEBUG_WIFI
    net_dump_pbuf("airoc: receive packet:\n", (pbuf_t)buffer);
#endif

    net_inject_ethernet_packet((pbuf_t)buffer, iface);

    //klog("cy_network_process_ethernet_data\n");
}

int WifiAirocNetInterface::GetHeaderSize() const
{
    klog("airoc: get_header_size\n");
    return 64;
}

int WifiAirocNetInterface::GetFooterSize() const
{
    klog("airoc: get_footer_size\n");
    return 64;
}

int WifiAirocNetInterface::Activate()
{
    if(wifi_airoc_reset() != 0)
    {
        klog("net: wifi_airoc_reset failed\n");
        return -1;
    }

    return NetInterface::Activate();
}

int WifiAirocNetInterface::Deactivate()
{
    if(has_ip)
    {
        for(auto &p : OnIPAssign)
        {
            p->OnDisconnect(this);
        }
    }
    
    whd_wifi_off(whd_iface);
    connected = false;
    connecting = false;
    has_ip = false;

    sdmmc[1].sd_ready = false;
    if(sdmmc[1].io_0)
        sdmmc[1].io_0();
    if(sdmmc[1].supply_off)
        sdmmc[1].supply_off();

    return NetInterface::Deactivate();
}

std::string WifiAirocNetInterface::DeviceName() const
{
    return "AIROC";
}

WifiAirocNetInterface::WifiAirocNetInterface()
{
    init_wifi_airoc();
}

void cb_wifi_scan(whd_scan_result_t **result_ptr, void *user_data, whd_scan_status_t status)
{
    auto iface = (WifiAirocNetInterface *)user_data;

    switch(status)
    {
        case whd_scan_status_t::WHD_SCAN_COMPLETED_SUCCESSFULLY:
            klog("airoc: scan complete\n");
            {
                CriticalGuard cg(iface->sl_new_networks);
                iface->new_networks_ready = true;
                iface->events.Push({ .msg_type = netiface_msg::netiface_msg_type::HardwareEvent,
                    .msg_subtype = AIROC_MSG_SUBTYPE_SCAN_COMPLETE });
            }
            break;

        case whd_scan_status_t::WHD_SCAN_ABORTED:
            klog("airoc: scan aborted\n");
            {
                CriticalGuard cg(iface->sl_new_networks);
                iface->new_networks_ready = true;
                iface->events.Push({ .msg_type = netiface_msg::netiface_msg_type::HardwareEvent,
                    .msg_subtype = AIROC_MSG_SUBTYPE_SCAN_COMPLETE });
            }
            break;

        case whd_scan_status_t::WHD_SCAN_INCOMPLETE:
            {
                WifiNetInterface::wifi_network wn;
                wn.ch = (*result_ptr)->channel;
                wn.rssi = (*result_ptr)->signal_strength;
                wn.ssid = std::string((const char *)(*result_ptr)->SSID.value,
                    (size_t)(*result_ptr)->SSID.length);
                wn.dev_data = (void *)(*result_ptr)->security;
                
                CriticalGuard cg(iface->sl_new_networks);
                iface->new_networks.push_back(wn);
            }
            break;
    }
}

int WifiAirocNetInterface::DoScan()
{
    {
        CriticalGuard cg(sl_new_networks);
        new_networks_ready = false;
        new_networks.clear();
    }
    auto ret = whd_wifi_scan(whd_iface, whd_scan_type_t::WHD_SCAN_TYPE_ACTIVE,
        whd_bss_type_t::WHD_BSS_TYPE_INFRASTRUCTURE, nullptr, nullptr,
        nullptr, nullptr, cb_wifi_scan, &new_networks_result, this);
    if(ret != WHD_SUCCESS)
    {
        klog("airoc: whd_wifi_scan failed: %x\n", ret);
        return -1;
    }
    return 0;
}

int WifiAirocNetInterface::Connect(const wifi_network &wn)
{
    whd_ssid_t ssid;
    ssid.length = wn.ssid.length();
    memcpy(ssid.value, wn.ssid.c_str(), std::min(32UL,wn.ssid.length()));
    auto ret = whd_wifi_join(whd_iface, &ssid, (whd_security_t)(uintptr_t)wn.dev_data,
        (const uint8_t *)wn.password.c_str(), (uint8_t)wn.password.length());
    if(ret == WHD_SUCCESS)
    {
        klog("airoc: joined network %s\n", wn.ssid.c_str());
        connecting = false;
        connected = true;
        events.Push({ .msg_type = netiface_msg::netiface_msg_type::LinkUp });
        return 0;
    }
    else
    {
        klog("airoc: failed to join network %s (%x)\n", wn.ssid.c_str(), ret);
        if(ret != WHD_WLAN_NOTFOUND)
            return NET_TRYAGAIN;
        return NET_NOTFOUND;
    }
}
