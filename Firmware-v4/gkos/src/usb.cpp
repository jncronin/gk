#include "pins.h"

#include "tusb.h"

#include "osmutex.h"
#include "thread.h"
#include "scheduler.h"
#include "usb.h"
#include "process.h"
#include "logger.h"
#include "smc.h"
#include "stm32mp2xx.h"
#include "vmem.h"
#include "clocks.h"

#define DEBUG_USB 1

bool usb_israwsd = false;

PProcess p_usb;

#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))
#define SYSCFG_VMEM ((SYSCFG_TypeDef *)PMEM_TO_VMEM(SYSCFG_BASE))
#define RIFSC_VMEM (PMEM_TO_VMEM(RIFSC_BASE))

void init_usb()
{
    usb_init_chip_id();

    /* Enable power to USB pins */
    smc_set_power(SMC_Power_Target::USB, 3300);
    PWR_VMEM->CR1 = PWR_VMEM->CR1 | PWR_CR1_USB33VMEN;
    __asm__ volatile("dmb sy\n" ::: "memory");
    while(!(PWR_VMEM->CR1 & PWR_CR1_USB33RDY))
    {
        Block(clock_cur() + kernel_time_from_ms(10));
    }
    klog("usb: power valid\n");

    /* Put blocks in reset */
    RCC_VMEM->USB3DRDCFGR |= RCC_USB3DRDCFGR_USB3DRDEN | RCC_USB3DRDCFGR_USB3DRDRST;
    RCC_VMEM->USB2PHY2CFGR |= RCC_USB2PHY2CFGR_USB2PHY2EN | RCC_USB2PHY2CFGR_USB2PHY2RST;
    __asm__ volatile("dmb sy\n" ::: "memory");

    /* Enable PHY2 clk */
    // flexgen channel 58 to ck_ker_usb2phy2 @ 20 MHz (default)
    // Use PLL4 = 1200 MHz / 60 -> 20 MHz SDCLK
    SYSCFG_VMEM->USB2PHY2CR = (SYSCFG_VMEM->USB2PHY2CR & ~SYSCFG_USB2PHY2CR_USB2PHY2SEL_Msk) |
        (1U << SYSCFG_USB2PHY2CR_USB2PHY2SEL_Pos);

    RCC_VMEM->PREDIVxCFGR[58] = 0;
    RCC_VMEM->FINDIVxCFGR[58] = 59U | 0x40U;
    RCC_VMEM->XBARxCFGR[58] = 0x40U;
    __asm__ volatile("dmb sy\n" ::: "memory");

    Block(clock_cur() + kernel_time_from_us(10));

    /* Release phy from reset */
    RCC_VMEM->USB2PHY2CFGR &= ~RCC_USB2PHY2CFGR_USB2PHY2RST;
    __asm__ volatile("dmb sy\n" ::: "memory");

    Block(clock_cur() + kernel_time_from_us(260));

    /* Release device from reset */
    RCC_VMEM->USB3DRDCFGR &= ~RCC_USB3DRDCFGR_USB3DRDRST;
    __asm__ volatile("dmb sy\n" ::: "memory");

    /* Force USB2 device mode */
    SYSCFG_VMEM->USB3DRCR = SYSCFG_USB3DRCR_USB3DR_USB2ONLYD;

    /* Enable secure access for USB DMA (index 4 / RISUP 66) */
    *(volatile uint32_t *)(RIFSC_VMEM + 0xc10 + 4 * 0x4) =
        (1UL << 2) |                // use cid specified here
        (1UL << 4) |                // CID 1
        (1UL << 8) |                // secure
        (1UL << 9);                 // priv
    const uint32_t risup = 66;
    const uint32_t risup_word = risup / 32;
    const uint32_t risup_bit = risup % 32;
    auto risup_reg = (volatile uint32_t *)(RIFSC_VMEM + 0x10 + 0x4 * risup_word);
    auto old_val = *risup_reg;
    old_val |= 1U << risup_bit;
    *risup_reg = old_val;
    __asm__ volatile("dmb sy\n" ::: "memory");

    /* Enable USB interrupts */
    gic_set_target(259, GIC_ENABLED_CORES);
    gic_set_enable(259);
    gic_set_target(260, GIC_ENABLED_CORES);
    gic_set_enable(260);

    /* Force VBUS to always be detected (TODO: on gk board use VBUS sampling from PMIC) */
    SYSCFG_VMEM->USB2PHY2CR |= SYSCFG_USB2PHY2CR_VBUSVLDEXTSEL |
        SYSCFG_USB2PHY2CR_VBUSVLDEXT;
}

extern void USB3DR_IRQHandler()
{
    tud_int_handler(0);
}

bool usb_process_start()
{
    if(reboot_flags & GK_REBOOTFLAG_RAWSD)
    {
        // cache this value because reboot_flags is reset at end of init_thread
        usb_israwsd = true;
    }

    p_usb = Process::Create("usb", true, p_kernel);
    Schedule(Thread::Create("tusb", usb_task, nullptr, true, GK_PRIORITY_VHIGH, p_usb));

    return true;
}

void *usb_task(void *pvParams)
{
    (void)pvParams;

    klog("usb: thread starting\n");

#if DEBUG_USB
    {
        klog("usb: task starting\n");
    }
#endif
    init_usb();
#if DEBUG_USB
    {
        klog("usb: calling tusb_init\n");
    }
#endif
    tusb_init();
    
    //NVIC_EnableIRQ(OTG_HS_IRQn);

    [[maybe_unused]] bool is_enabled = false;

    while(true)
    {
#if DEBUG_USB
        {
            klog("usb: loop\n");
        }
#endif

#if 0
        if(!is_enabled)
        {
            if(PWR->CSR2 & PWR_CSR2_USB33RDY)
            {
                is_enabled = true;
                NVIC_EnableIRQ(OTG_HS_IRQn);
            }
            else
            {
                Block(clock_cur() + kernel_time::from_ms(500));
            }
        }
        else
        {
            if(!(PWR->CSR2 & PWR_CSR2_USB33RDY))
            {
                is_enabled = false;
                NVIC_DisableIRQ(OTG_HS_IRQn);
            }
            else
            {
                tud_task();
            }
        }
#endif
        tud_task();

        //tud_task();
    }
}
