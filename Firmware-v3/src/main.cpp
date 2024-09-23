#include <stm32h7rsxx.h>
#include <cstring>
#include "pins.h"
#include "i2c.h"
#include "SEGGER_RTT.h"

uint32_t test_val;

uint32_t test_range[256];

static const constexpr pin CTP_NRESET { GPIOC, 0 };
extern "C" void init_xspi();

int main()
{
    SCB_InvalidateDCache();
    SCB_InvalidateICache();
    SCB_EnableICache();
    SCB_EnableDCache();

    // calculate total memory
    unsigned int itcm_size, dtcm_size, axisram_base, axisram_end, axisram_size, ahbsram_size;
    auto obw2sr = FLASH->OBW2SR;
    auto dtcm_axi_share = (obw2sr & FLASH_OBW2SR_DTCM_AXI_SHARE_Msk) >>
        FLASH_OBW2SR_DTCM_AXI_SHARE_Pos;
    auto itcm_axi_share = (obw2sr & FLASH_OBW2SR_ITCM_AXI_SHARE_Msk) >>
        FLASH_OBW2SR_ITCM_AXI_SHARE_Pos;
    switch(dtcm_axi_share)
    {
        case 1:
            dtcm_size = 128*1024;
            axisram_end = 0x24050000;
            break;
        case 2:
            dtcm_size = 192*1024;
            axisram_end = 0x24040000;
            break;
        default:
            dtcm_size = 64*1024;
            axisram_end = 0x24060000;
            break;
    }
    switch(itcm_axi_share)
    {
        case 1:
            itcm_size = 128*1024;
            axisram_base = 0x24010000;
            break;
        case 2:
            itcm_size = 192*1024;
            axisram_base = 0x24020000;
            break;
        default:
            itcm_size = 64*1024;
            axisram_base = 0x24000000;
            break;
    }
    axisram_size = axisram_end - axisram_base;
    ahbsram_size = 0x8000;

    SEGGER_RTT_printf(0, "memory_map:\n"
        "  ITCM:    0x00000000 - 0x%08x (%d kbytes)\n"
        "  DTCM:    0x20000000 - 0x%08x (%d kbytes)\n"
        "  AXISRAM: 0x%08x - 0x%08x (%d kbytes)%s\n"
        "  AHBSRAM: 0x30000000 - 0x%08x (%d kbytes)\n",
        itcm_size, itcm_size/1024,
        0x20000000U + dtcm_size, dtcm_size/1024,
        axisram_base, axisram_end, axisram_size/1024,
        (obw2sr & FLASH_OBW2SR_ECC_ON_SRAM) ? "" : " and 0x24060000 - 0x24072000 (72 kbytes)",
        0x30000000U + ahbsram_size, ahbsram_size/1024);
    
    while(true);

    return 0;
}
