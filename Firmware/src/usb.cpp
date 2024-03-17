#include "stm32h7xx.h"
#include "pins.h"

#include "tusb.h"
#include "mpuregions.h"

#include "osmutex.h"
#include "SEGGER_RTT.h"
#include "thread.h"

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

    RCC->AHB1ENR = RCC_AHB1ENR_USB1OTGHSEN;
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
    while(!(PWR->CR3 & PWR_CR3_USB33RDY));
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

    uint32_t data_start = (uint32_t)&_stusb_data;
    uint32_t data_end = (uint32_t)&_etusb_data;
    SetMPUForCurrentThread(MPUGenerate(data_start, data_end - data_start, 6, false,
        RW, NoAccess, WBWA_NS));

    data_start = (uint32_t)&_slwip_data;
    data_end = (uint32_t)&_elwip_data;
    SetMPUForCurrentThread(MPUGenerate(data_start, data_end - data_start, 7, false,
        RW, NoAccess, WBWA_NS));



    init_usb();
    tusb_init();

    NVIC_EnableIRQ(OTG_HS_IRQn);

    while(true)
    {
        tud_task();
    }
}

extern "C" int rtt_printf_wrapper(const char *format, ...)
{
    CriticalGuard cg(s_rtt);
    int r;
    va_list ParamList;
    va_start(ParamList, format);
    r = SEGGER_RTT_vprintf(0, format, &ParamList);
    va_end(ParamList);
    return r;
}
