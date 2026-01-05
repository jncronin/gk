#include <stm32mp2xx.h>
#include "pins.h"
#include "vmem.h"
#include "clocks.h"
#include "sdif.h"
#include "logger.h"

static void wifi_airoc_reset();

static bool wifi_airoc_init = false;

#define GPIOC_VMEM (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE)
static const constexpr pin WIFI_REG_ON { GPIOC_VMEM, 7 };
static const constexpr pin BT_REG_ON { GPIOC_VMEM, 8 };

#define GPIOI_VMEM (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOI_BASE)
static const constexpr pin MCO1 { GPIOI_VMEM, 6, 1 };

#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))

void init_wifi_airoc()
{
    wifi_airoc_init = false;

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

    wifi_airoc_reset();
}

void wifi_airoc_reset()
{
    if(sdmmc[1].reset() != 0)
        return;
    
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
        std::array<uint8_t, 256U> tuple;
        uint8_t tid;
        size_t len;
        auto ret = sdmmc[1].sdio_read_tuple<256U>(&cis_addr, &tid, &len, tuple);
        if(ret != 0)
            return;
                
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
        return;
    }

    sdmmc[1].sdio_set_func_block_size(0, f0_blk_size);
    sdmmc[1].sdio_set_func_block_size(1, 64);
    sdmmc[1].sdio_set_func_block_size(2, 64);


}
