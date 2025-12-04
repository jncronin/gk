#include "pins.h"

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
#include "pmem.h"
#include "osqueue.h"

#include "usb_device.h"
#include "usb_dwc3.h"
#include "usb_class.h"

#define DEBUG_USB 1

bool usb_israwsd = false;

PProcess p_usb;

// some/all of these need cache_line_size alignment
__attribute__((aligned(CACHE_LINE_SIZE))) usb_handle usb_core;
__attribute__((aligned(CACHE_LINE_SIZE))) pcd_handle pcd_handle;
__attribute__((aligned(CACHE_LINE_SIZE))) dwc3_handle_t dwc3_handle;

// store the state of individual class statuses
usb_class_info usb_cinfo;

extern usb_desc usb_desc_callback;

#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))
#define SYSCFG_VMEM ((SYSCFG_TypeDef *)PMEM_TO_VMEM(SYSCFG_BASE))
#define RIFSC_VMEM (PMEM_TO_VMEM(RIFSC_BASE))

// messages to send to usb system
#define USB_MESSAGE_INT             0x1
#define USB_MESSAGE_VBUS_DOWN       0x2
#define USB_MESSAGE_VBUS_UP         0x3


// message queue
FixedQueue<uint32_t, 32> usb_queue;

void init_usb()
{
    usb_init_chip_id();

    /* It may be we already have the USB set up in device mode from bootrom - check here */
    klog("usb: PWR->CR1:               %08x\n", PWR_VMEM->CR1);
    klog("usb: RCC->USB3DRDCFGR:       %08x\n", RCC_VMEM->USB3DRDCFGR);
    klog("usb: RCC->USB2PHY2CFGR:      %08x\n", RCC_VMEM->USB2PHY2CFGR);
    klog("usb: SYSCFG->USB2PHY2CR:     %08x\n", SYSCFG_VMEM->USB2PHY2CR);
    klog("usb: SYSCFG->USB3DRCR:       %08x\n", SYSCFG_VMEM->USB3DRCR);

    /* Enable power to USB pins */
    smc_set_power(SMC_Power_Target::USB, 3300);
    PWR_VMEM->CR1 = PWR_VMEM->CR1 | PWR_CR1_USB33VMEN;
    __asm__ volatile("dmb sy\n" ::: "memory");
    while(!(PWR_VMEM->CR1 & PWR_CR1_USB33RDY))
    {
        Block(clock_cur() + kernel_time_from_ms(10));
    }
    klog("usb: power valid\n");

    PWR_VMEM->CR1 |= PWR_CR1_ASV;

    /* Put blocks in reset */
    RCC_VMEM->USB3DRDCFGR |= RCC_USB3DRDCFGR_USB3DRDEN | RCC_USB3DRDCFGR_USB3DRDRST;
    /* RCC_VMEM->USB2PHY2CFGR |= RCC_USB2PHY2CFGR_USB2PHY2EN  | RCC_USB2PHY2CFGR_USB2PHY2RST */;
    __asm__ volatile("dmb sy\n" ::: "memory");

    /* Enable PHY2 clk */
    // flexgen channel 58 to ck_ker_usb2phy2 @ 20 MHz (default)
    // Use PLL5 = 1200 MHz from HSE / 60 -> 20 MHz USB clock
    SYSCFG_VMEM->USB2PHY2CR = (SYSCFG_VMEM->USB2PHY2CR & ~SYSCFG_USB2PHY2CR_USB2PHY2SEL_Msk) |
        (1U << SYSCFG_USB2PHY2CR_USB2PHY2SEL_Pos);

    RCC_VMEM->PREDIVxCFGR[58] = 0;
    RCC_VMEM->FINDIVxCFGR[58] = 59U | 0x40U;
    RCC_VMEM->XBARxCFGR[58] = 0x41U;
    __asm__ volatile("dmb sy\n" ::: "memory");

    // Enable HSE/2 clock
    RCC_VMEM->OCENSETR = RCC_OCENSETR_HSEDIV2ON;
    RCC_VMEM->USB2PHY2CFGR |= RCC_USB2PHY2CFGR_USB2PHY2CKREFSEL;

    Block(clock_cur() + kernel_time_from_us(10));

    /* Force VBUS to always be detected (TODO: on gk board use VBUS sampling from PMIC) */
    SYSCFG_VMEM->USB2PHY2CR |= SYSCFG_USB2PHY2CR_VBUSVLDEXTSEL |
        SYSCFG_USB2PHY2CR_VBUSVLDEXT |
        SYSCFG_USB2PHY2CR_OTGDISABLE0;
    __asm__ volatile("dmb sy\n" ::: "memory");

    /* Release phy from reset */
    RCC_VMEM->USB2PHY2CFGR &= ~RCC_USB2PHY2CFGR_USB2PHY2RST;
    RCC_VMEM->USB2PHY2CFGR |= RCC_USB2PHY2CFGR_USB2PHY2EN;
    __asm__ volatile("dmb sy\n" ::: "memory");

    Block(clock_cur() + kernel_time_from_us(260));

    /* Force USB2 device mode */
    SYSCFG_VMEM->USB3DRCR = SYSCFG_USB3DRCR_USB3DR_USB2ONLYD;
    __asm__ volatile("dmb sy\n" ::: "memory");

    /* Release device from reset */
    RCC_VMEM->USB3DRDCFGR &= ~RCC_USB3DRDCFGR_USB3DRDRST;
    __asm__ volatile("dmb sy\n" ::: "memory");

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

    /* We can observe the frequencies of various clocks here.  EV1 provides PF11/MCO1 on the
        GPIO expansion header.  It is NC on GK.
        
        Can use it to check the USB2PHY2 clock. */
    RCC_VMEM->GPIOFCFGR |= RCC_GPIOFCFGR_GPIOxEN;
    (void)RCC_VMEM->GPIOFCFGR;
    pin PF11 { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOF_BASE), 11, 1 };
    PF11.set_as_af();
    RCC_VMEM->FCALCOBS0CFGR = RCC_FCALCOBS0CFGR_CKOBSEN |
        RCC_FCALCOBS0CFGR_CKOBSEXTSEL |
        (4U << RCC_FCALCOBS0CFGR_CKEXTSEL_Pos);
    RCC_VMEM->MCO1CFGR = RCC_MCO1CFGR_MCO1ON | RCC_MCO1CFGR_MCO1SEL;
}

extern void USB3DR_IRQHandler()
{
    gic_clear_enable(259);
    gic_clear_enable(260);

    usb_queue.Push(USB_MESSAGE_INT);
    //usb_core_handle_it(&usb_core);
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

    extern PMemBlock pb_usb;
    extern VMemBlock vb_usb;
    if(!pb_usb.valid)
    {
        pb_usb = Pmem.acquire(VBLOCK_64k);
    }
    if(!vb_usb.valid)
    {
        vb_usb = vblock.Alloc(VBLOCK_64k);
        vmem_map(vb_usb.base, pb_usb.base, false, true, false, ~0ULL, ~0ULL, nullptr, MT_DEVICE);
    }

    pcd_handle.in_ep[0].maxpacket = 64;
    pcd_handle.out_ep[0].maxpacket = 64;
    usb_dwc3_init_driver(&usb_core, &pcd_handle, &dwc3_handle, (void *)PMEM_TO_VMEM(0x48300000ULL));

    usb_core.ep0_state = USBD_EP0_DATA_IN;
    usb_core.dev_state = USBD_STATE_CONFIGURED;

    extern usb_class usb_class_handlers;
    usb_core._class = &usb_class_handlers;
    usb_core.class_data = &usb_cinfo;

    register_platform(&usb_core, &usb_desc_callback);
    
#if DEBUG_USB
    {
        klog("usb: calling usb_core_start\n");
    }
#endif
    usb_core_start(&usb_core);
    
    //NVIC_EnableIRQ(OTG_HS_IRQn);

    [[maybe_unused]] bool is_enabled = false;

    while(true)
    {
        uint32_t cur_msg = 0;
        while(!usb_queue.Pop(&cur_msg));

#if DEBUG_USB
        klog("usb: loop\n");
#endif

        switch(cur_msg)
        {
            case USB_MESSAGE_INT:
                usb_core_handle_it(&usb_core);
                gic_set_enable(259);
                gic_set_enable(260);
                break;

            default:
                klog("usb: spurious message: %u\n", cur_msg);
                break;
        }
    }
}
