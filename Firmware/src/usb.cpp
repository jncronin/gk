#include "stm32h7xx.h"
#include "pins.h"

#include "tusb.h"
#include "mpuregions.h"

#include "osmutex.h"
#include "SEGGER_RTT.h"
#include "thread.h"
#include "scheduler.h"

#define DEBUG_USB 0

extern Spinlock s_rtt;

/* Using USB_OTG_HS1 in FS mode with integrated PHY */
static constexpr pin usb_pins[] = {
    { GPIOB, 14, 12 },
    { GPIOB, 15, 12 },
};
constexpr pin usb_vbus = { GPIOB, 13 };

char _stusb_data, _etusb_data, _slwip_data, _elwip_data;

void init_usb()
{
    RCC->AHB1RSTR = RCC_AHB1RSTR_USB1OTGHSRST;
    (void)RCC->AHB1RSTR;
    RCC->AHB1RSTR = 0;
    (void)RCC->AHB1RSTR;

    RCC->AHB1ENR |= RCC_AHB1ENR_USB1OTGHSEN;
    (void)RCC->AHB1ENR;

    for(const auto &p : usb_pins)
    {
        p.set_as_af();
    }
    usb_vbus.set_as_input(pin::pup::PullDown);

    // Configure HSI48 with CRS
    RCC->CR |= RCC_CR_HSI48ON;
    while(!(RCC->CR & RCC_CR_HSI48RDY));

    RCC->APB1HRSTR = RCC_APB1HRSTR_CRSRST;
    (void)RCC->APB1HRSTR;
    RCC->APB1HRSTR = 0;
    (void)RCC->APB1HRSTR;

    RCC->APB1HENR = RCC_APB1HENR_CRSEN;
    (void)RCC->APB1HENR;

    CRS->CR = CRS_CR_AUTOTRIMEN | CRS_CR_CEN;

    // Clock USB
    RCC->D2CCIP2R |= RCC_D2CCIP2R_USBSEL_Msk;   // HSI48

    // Disable voltage detect
    USB1_OTG_HS->GCCFG |= USB_OTG_GCCFG_VBDEN;

    // B-peripheral session valid override enable
    //USB1_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    //USB1_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;

    // Force device mode
    USB1_OTG_HS->GUSBCFG &= ~USB_OTG_GUSBCFG_FHMOD;
    USB1_OTG_HS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

    // Enable USB power supervisor
    PWR->CR3 |= PWR_CR3_USB33DEN;
}

extern "C" void OTG_HS_IRQHandler()
{
    // tud_int_handler edits stuff in out private data space, so get access to it
    //uint32_t data_start = (uint32_t)&_stusb_data;
    //uint32_t data_end = (uint32_t)&_etusb_data;

    //auto old_mpu6 = GetCurrentThreadForCore()->tss.mpuss[5];
    //SetMPUForCurrentThread(MPUGenerate(data_start, data_end - data_start, 6, false, RW, NoAccess, WBWA_NS));
    tud_int_handler(1);
    //SetMPUForCurrentThread(old_mpu6);
}

void *usb_task(void *pvParams)
{
    (void)pvParams;

#if DEBUG_USB
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "usb: task starting, core %d\n", GetCoreID());
    }
#endif
    init_usb();
#if DEBUG_USB
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "usb: calling tusb_init\n");
    }
#endif
    tusb_init();
    
    //NVIC_EnableIRQ(OTG_HS_IRQn);

    bool is_enabled = false;

    while(true)
    {
#if DEBUG_USB
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "usb: loop\n");
        }
#endif
        if(!is_enabled)
        {
            if(PWR->CR3 & PWR_CR3_USB33RDY)
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
            if(!(PWR->CR3 & PWR_CR3_USB33RDY))
            {
                is_enabled = false;
                NVIC_DisableIRQ(OTG_HS_IRQn);
            }
            else
            {
                tud_task();
            }
        }

        //tud_task();
    }
}
