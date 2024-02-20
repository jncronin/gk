#include "stm32h7xx.h"
#include "pins.h"

#include "tusb.h"

/* Using USB_OTG_HS1 in FS mode with integrated PHY */
static constexpr pin usb_pins[] = {
    { GPIOB, 14, 12 },
    { GPIOB, 15, 12 },
};
constexpr pin usb_vbus = { GPIOB, 13 };

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
}

extern "C" void OTG_HS_IRQHandler()
{
    tud_int_handler(0);
}

void usb_task(void *pvParams)
{
    (void)pvParams;

    init_usb();
    tusb_init();

    NVIC_EnableIRQ(OTG_HS_IRQn);

    while(true)
    {
        tud_task();
    }
}
