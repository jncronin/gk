#include <stm32h7rsxx.h>

extern "C" int memblk_init_flash_opt_bytes()
{
    // aim for max DTCM, ITCM and no ECC
    auto opw2_mask = FLASH_OBW2SR_DTCM_AXI_SHARE_Msk |
        FLASH_OBW2SR_ITCM_AXI_SHARE_Msk |
        FLASH_OBW2SR_ECC_ON_SRAM_Msk;
    auto opw2_settings = (0U << FLASH_OBW2SRP_ECC_ON_SRAM_Pos) |
        (2U << FLASH_OBW2SRP_DTCM_AXI_SHARE_Pos) |
        (2U << FLASH_OBW2SRP_ITCM_AXI_SHARE_Pos);
    if((FLASH->OBW2SR & opw2_mask) != opw2_settings)
    {
        auto orig_val = FLASH->OBW2SR;
        auto new_val = (orig_val & ~opw2_mask) | opw2_settings;

        // program option bytes

        // unlock OPTCR (5.5.1)
        FLASH->OPTKEYR = 0x08192a3b;
        __DMB();
        FLASH->OPTKEYR = 0x4c5d6e7f;
        __DMB();

        // enable write operations (5.4.3)
        FLASH->OPTCR |= FLASH_OPTCR_PG_OPT;

        // program new value
        FLASH->OBW2SRP = new_val;

        // wait completion
        while(FLASH->SR & FLASH_SR_QW);

        // relock
        FLASH->OPTCR |= FLASH_OPTCR_OPTLOCK;
    }

    return 0;
}
