#include <stm32h7xx.h>

#include "pins.h"
#include "clocks.h"
#include "gk_conf.h"

/* SDRAM pins */
static constexpr pin sdram_pins[] =
{
    { GPIOC, 0, 12 },
    { GPIOC, 2, 12 },
    { GPIOC, 3, 12 },
    { GPIOD, 0, 12 },
    { GPIOD, 1, 12 },
    { GPIOD, 8, 12 },
    { GPIOD, 9, 12 },
    { GPIOD, 10, 12 },
    { GPIOD, 14, 12 },
    { GPIOD, 15, 12 },
    { GPIOE, 0, 12 },
    { GPIOE, 1, 12 },
    { GPIOE, 7, 12 },
    { GPIOE, 8, 12 },
    { GPIOE, 9, 12 },
    { GPIOE, 10, 12 },
    { GPIOE, 11, 12 },
    { GPIOE, 12, 12 },
    { GPIOE, 13, 12 },
    { GPIOE, 14, 12 },
    { GPIOE, 15, 12 },
    { GPIOF, 0, 12 },
    { GPIOF, 1, 12 },
    { GPIOF, 2, 12 },
    { GPIOF, 3, 12 },
    { GPIOF, 4, 12 }, 
    { GPIOF, 5, 12 },
    { GPIOF, 11, 12 },
    { GPIOF, 12, 12 },
    { GPIOF, 13, 12 },
    { GPIOF, 14, 12 },
    { GPIOF, 15, 12 },
    { GPIOG, 0, 12 },
    { GPIOG, 1, 12 },
    { GPIOG, 2, 12 },
    { GPIOG, 4, 12 },
    { GPIOG, 5, 12 },
    { GPIOG, 8, 12 },
    { GPIOG, 15, 12 }
};
static constexpr auto n_pins = sizeof(sdram_pins) / sizeof(pin);

void init_sdram()
{
    for(unsigned int i = 0; i < n_pins; i++)
    {
        sdram_pins[i].set_as_af();
    }

    // Clock the FMC
    RCC->AHB3ENR |= RCC_AHB3ENR_FMCEN;
    (void)RCC->AHB3ENR;

    FMC_Bank1_R->BTCR[0] |= FMC_BCR1_FMCEN;
    if(GK_SDRAM_BASE == 0x60000000)
    {
        FMC_Bank1_R->BTCR[0] &= ~FMC_BCR1_BMAP_Msk;
        FMC_Bank1_R->BTCR[0] |= 1UL << FMC_BCR1_BMAP_Pos;
    }
    
    // Set up control register
    FMC_Bank5_6_R->SDCR[0] = (2UL << FMC_SDCRx_NC_Pos) |
        (2UL << FMC_SDCRx_NR_Pos) |
        (1UL << FMC_SDCRx_MWID_Pos) |
        (1UL << FMC_SDCRx_NB_Pos) |
        (3UL << FMC_SDCRx_CAS_Pos) |
        (0UL << FMC_SDCRx_WP_Pos) |
        (2UL << FMC_SDCRx_SDCLK_Pos) |
        (1UL << FMC_SDCRx_RBURST_Pos) |
        (1UL << FMC_SDCRx_RPIPE_Pos);
    
    // Set up timing register
    FMC_Bank5_6_R->SDTR[0] = (1UL << FMC_SDTRx_TMRD_Pos) |
        (7UL << FMC_SDTRx_TXSR_Pos) |
        (7UL << FMC_SDTRx_TRAS_Pos) |
        (7UL << FMC_SDTRx_TRC_Pos) |
        (1UL << FMC_SDTRx_TWR_Pos) |
        (1UL << FMC_SDTRx_TRP_Pos) |
        (1UL << FMC_SDTRx_TRCD_Pos);

    // Set up timing register
    /*FMC_SDRAM_DEVICE->SDTR[0] = (2UL << FMC_SDTR1_TMRD_Pos) |
        (7UL << FMC_SDTR1_TXSR_Pos) |
        (6UL << FMC_SDTR1_TRAS_Pos) |
        (9UL << FMC_SDTR1_TRC_Pos) |
        (3UL << FMC_SDTR1_TWR_Pos) |
        (3UL << FMC_SDTR1_TRP_Pos) |
        (3UL << FMC_SDTR1_TRCD_Pos);*/

    // Enable the SDRAM clock
    FMC_Bank5_6_R->SDCMR = (1UL << FMC_SDCMR_MODE_Pos) |
        FMC_SDCMR_CTB1;
    delay_ms(2);

    // Send precharge all command
    FMC_Bank5_6_R->SDCMR = (2UL << FMC_SDCMR_MODE_Pos) |
        FMC_SDCMR_CTB1;

    // Two autorefresh cycles
    FMC_Bank5_6_R->SDCMR = (3UL << FMC_SDCMR_MODE_Pos) |
        FMC_SDCMR_CTB1 |
        (2UL << FMC_SDCMR_NRFS_Pos);

    // Load mode register
    uint32_t mode = 0 | (3UL << 4); // Burst Length = 0, CAS=3
    //uint32_t mode = 1UL | (3UL << 4) | (1UL << 9);
    FMC_Bank5_6_R->SDCMR = (4UL << FMC_SDCMR_MODE_Pos) |
        FMC_SDCMR_CTB1 |
        (mode << FMC_SDCMR_MRD_Pos);

    // Set refresh period
    FMC_Bank5_6_R->SDRTR = 729UL << 1;  // refresh period = 64 ms / rows = 8192 * SDCLK - 20

    for(unsigned int i = 0; i < 256; i += 4)
    {
        *(volatile unsigned int *)(GK_SDRAM_BASE + i) = 0xaa55aa55;
    }
}
