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

    // These bits should be performed by the wifi connection manager module - for testing run them separately here
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

    return whd_wifi_on(whd_drv, &whd_iface) == WHD_SUCCESS ? 0 : -1;

    //bt_post_reset_cback();
}

int WifiAirocNetInterface::HardwareEvent()
{
    klog("airoc: hwevent\n");
    return 0;
}

int WifiAirocNetInterface::SendEthernetPacket(char *buf, size_t n, const HwAddr &dest, uint16_t ethertype,
            bool release_buffer)
{
    klog("airoc: send_ethernet_packet\n");
    return 0;
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
    whd_wifi_off(whd_iface);
    sdmmc[1].sd_ready = false;
    sdmmc[1].io_0();
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
