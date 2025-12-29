#include "screen.h"
#include "pins.h"
#include "vmem.h"
#include "i2c.h"

#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define RIFSC_VMEM PMEM_TO_VMEM(RIFSC_BASE)
#define GPIOA_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOA_BASE))
#define GPIOB_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOB_BASE))
#define GPIOC_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE))
#define GPIOD_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOD_BASE))
#define GPIOE_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOE_BASE))
#define GPIOF_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOF_BASE))
#define GPIOG_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOG_BASE))
#define GPIOH_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOH_BASE))
#define GPIOI_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOI_BASE))
#define GPIOJ_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOJ_BASE))
#define LTDC_VMEM ((LTDC_TypeDef *)PMEM_TO_VMEM(LTDC_BASE))
#define LTDC_Layer1_VMEM ((LTDC_Layer_TypeDef *)PMEM_TO_VMEM(LTDC_Layer1_BASE))
#define LTDC_Layer2_VMEM ((LTDC_Layer_TypeDef *)PMEM_TO_VMEM(LTDC_Layer2_BASE))
#define LTDC_Layer3_VMEM ((LTDC_Layer_TypeDef *)PMEM_TO_VMEM(LTDC_Layer3_BASE))
#define SYSCFG_VMEM ((SYSCFG_TypeDef *)PMEM_TO_VMEM(SYSCFG_BASE))

static const constexpr pin PWM_BACKLIGHT { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOA_BASE), 4 };
static const constexpr pin LS_OE_N { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE), 0 };
static const constexpr pin CTP_WAKE { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOA_BASE), 2 };

void init_screen()
{
    RCC_VMEM->GPIOICFGR |= RCC_GPIOICFGR_GPIOxEN;
    RCC_VMEM->GPIOJCFGR |= RCC_GPIOJCFGR_GPIOxEN;
    RCC_VMEM->GPIOGCFGR |= RCC_GPIOGCFGR_GPIOxEN;
    RCC_VMEM->GPIOACFGR |= RCC_GPIOACFGR_GPIOxEN;
    RCC_VMEM->GPIOBCFGR |= RCC_GPIOBCFGR_GPIOxEN;
    RCC_VMEM->GPIOCCFGR |= RCC_GPIOCCFGR_GPIOxEN;
    RCC_VMEM->LTDCCFGR |= RCC_LTDCCFGR_LTDCEN;
    __asm__ volatile("dsb sy\n");

    // turn on backlight
    PWM_BACKLIGHT.set_as_output();
    PWM_BACKLIGHT.set();

    // enable level shifter
    LS_OE_N.set_as_output();
    LS_OE_N.clear();

    // enable CTP
    CTP_WAKE.set_as_output();
    CTP_WAKE.set();

    // LTDC setup for 800x480 panel
    RCC_VMEM->LTDCCFGR |= RCC_LTDCCFGR_LTDCEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

#if 0
    /* Give LTDC access to DDR via RIFSC.  Use same CID as CA35/secure/priv for the master interface ID 1 */
    *(volatile uint32_t *)(RIFSC_VMEM + 0xc10 + 11 * 0x4) =
        (1UL << 2) |                // use cid specified here
        (1UL << 4) |                // CID 1
        (1UL << 8) |                // secure
        (1UL << 9);                 // priv
    *(volatile uint32_t *)(RIFSC_VMEM + 0xc10 + 12 * 0x4) =
        (1UL << 2) |                // use cid specified here
        (1UL << 4) |                // CID 1
        (1UL << 8) |                // secure
        (1UL << 9);                 // priv    
    /* The IDMA is forced to non-secure if its RISUP (RISUP 119/120) is programmed as non-secure,
        therefore set as secure here */
    {
        const uint32_t risup = 119;
        const uint32_t risup_word = risup / 32;
        const uint32_t risup_bit = risup % 32;
        auto risup_reg = (volatile uint32_t *)(RIFSC_VMEM + 0x10 + 0x4 * risup_word);
        auto old_val = *risup_reg;
        old_val |= 1U << risup_bit;
        *risup_reg = old_val;
        __asm__ volatile("dmb sy\n" ::: "memory");
    }
    {
        const uint32_t risup = 120;
        const uint32_t risup_word = risup / 32;
        const uint32_t risup_bit = risup % 32;
        auto risup_reg = (volatile uint32_t *)(RIFSC_VMEM + 0x10 + 0x4 * risup_word);
        auto old_val = *risup_reg;
        old_val |= 1U << risup_bit;
        *risup_reg = old_val;
        __asm__ volatile("dmb sy\n" ::: "memory");
    }
#endif

    /* LTDC pins */
    static const constexpr pin DISP { GPIOC_VMEM, 5 };         // DISP
    DISP.set_as_output();
    DISP.set();

    static const constexpr pin ltdc_pins[] = {
        { GPIOA_VMEM, 1, 11 },         // R3
        { GPIOB_VMEM, 15, 13 },        // R4
        { GPIOC_VMEM, 6, 14 },         // CLK
        { GPIOC_VMEM, 11, 13 },        // R2
        { GPIOG_VMEM, 1, 13 },         // VSYNC
        { GPIOG_VMEM, 3, 13 },         // R5
        { GPIOG_VMEM, 6, 13 },         // R6
        { GPIOG_VMEM, 7, 13 },         // R7
        { GPIOG_VMEM, 8, 13 },         // G2
        { GPIOG_VMEM, 9, 13 },         // G3
        { GPIOG_VMEM, 10, 13 },        // G4
        { GPIOG_VMEM, 11, 13 },        // G5
        { GPIOG_VMEM, 12, 13 },        // G6
        { GPIOG_VMEM, 13, 13 },        // G7
        { GPIOG_VMEM, 14, 13 },        // B1
        { GPIOG_VMEM, 15, 13 },        // B2
        { GPIOI_VMEM, 0, 13 },         // B3
        { GPIOI_VMEM, 1, 13 },         // B4
        { GPIOI_VMEM, 2, 13 },         // B5
        { GPIOI_VMEM, 3, 13 },         // B6
        { GPIOI_VMEM, 4, 13 },         // B7
        { GPIOI_VMEM, 5, 13 },         // DE
        { GPIOI_VMEM, 7, 13 },         // HSYNC
        { GPIOI_VMEM, 9, 13 },         // B0
        { GPIOI_VMEM, 12, 13 },        // G0
        { GPIOI_VMEM, 13, 13 },        // G1
        { GPIOJ_VMEM, 14, 13 },        // R0
        { GPIOJ_VMEM, 15, 13 },        // R1
    };
    for(const auto &p : ltdc_pins)
    {
        p.set_as_af();
    }

    /* Screen is 800x480
        ER settings:
        H visible 800
            back  140
            front 160
            spw   20
        V visible 480
            back  20
            front 12
            spw   3
        PCLK      falling
        HSYNC     high
        VSYNC     high
        DE        high
    
        1120 x 515 -> 34.608 MHz @ 60 Hz

        We want something easily divisble from 1200 MHz, so use:
        1250 x 640 -> 48 MHz @ 60 Hz
        So, e.g. H front 225, visible 800, back 200, spw 25
                 V front 80, visible 480, back 75, spw 5

        Need /25 divider from 1200 MHz PLL5 off HSE (could use PLL8 otherwise)
    */
    RCC_VMEM->FINDIVxCFGR[27] = 0;
    RCC_VMEM->PREDIVxCFGR[27] = 0;
    RCC_VMEM->XBARxCFGR[27] = 0x41;
    RCC_VMEM->FINDIVxCFGR[27] = 0x40U | 47U;

    // LTDC clock from above
    SYSCFG->DISPLAYCLKCR = 0x2U;

    RCC_VMEM->LTDCCFGR &= ~RCC_LTDCCFGR_LTDCRST;
    __asm__ volatile("dsb sy\n" ::: "memory");

    LTDC_VMEM->SSCR = (3UL << LTDC_SSCR_VSH_Pos) |
        (3UL << LTDC_SSCR_HSW_Pos);
    LTDC_VMEM->BPCR = (19UL << LTDC_BPCR_AVBP_Pos) |
        (11UL << LTDC_BPCR_AHBP_Pos);
    LTDC_VMEM->AWCR = (499UL << LTDC_AWCR_AAH_Pos) |
        (811UL << LTDC_AWCR_AAW_Pos);
    LTDC_VMEM->TWCR = (511UL << LTDC_TWCR_TOTALH_Pos) |
        (815UL << LTDC_TWCR_TOTALW_Pos);
    
    LTDC_VMEM->GCR = LTDC_GCR_HSPOL |
        LTDC_GCR_VSPOL |
        LTDC_GCR_DEPOL |
        LTDC_GCR_PCPOL;
    
    LTDC_VMEM->BCCR = 0xff00ffUL;

    LTDC_Layer1_VMEM->CR = 0;
    LTDC_Layer2_VMEM->CR = 0;
    LTDC_Layer3_VMEM->CR = 0;

    LTDC_VMEM->SRCR = LTDC_SRCR_IMR;
    LTDC_VMEM->LIPCR = 511UL;
    LTDC_VMEM->IER = LTDC_IER_LIE | LTDC_IER_RRIE;
    LTDC_VMEM->GCR |= LTDC_GCR_LTDCEN;
}

void screen_poll()
{
    auto &i2c4 = i2c(4);
    // check ctp responds
    uint8_t reg0;
    i2c4.RegisterRead(0x40, (uint8_t)0, &reg0, 1);
    klog("ctp: reg0: %x\n", reg0);

    static unsigned int bit = 0;
    LTDC_VMEM->BCCR = 1U << bit;

    bit++;
    if(bit >= 24) bit = 0;

}
