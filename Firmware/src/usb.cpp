#include "stm32h7rsxx.h"
#include "pins.h"

#include "tusb.h"
#include "mpuregions.h"

#include "osmutex.h"
#include "SEGGER_RTT.h"
#include "thread.h"
#include "scheduler.h"
#include "usb.h"
#include "process.h"

#define DEBUG_USB 0

extern Spinlock s_rtt;

/* Using USB_OTG_HS in HS mode with integrated PHY */
static constexpr pin usb_pins[] = {
    { GPIOM, 5, 10 },
    { GPIOM, 6, 10 },
};
constexpr pin usb_vbus = { GPIOM, 8 };

char _stusb_data, _etusb_data, _slwip_data, _elwip_data;

Process p_usb;

void init_usb()
{
    usb_init_chip_id();
    
    RCC->AHB1RSTR = RCC_AHB1RSTR_OTGHSRST | RCC_AHB1RSTR_USBPHYCRST;
    (void)RCC->AHB1RSTR;
    RCC->AHB1RSTR = 0;
    (void)RCC->AHB1RSTR;

    RCC->AHB1ENR |= RCC_AHB1ENR_OTGHSEN | RCC_AHB1ENR_USBPHYCEN;
    (void)RCC->AHB1ENR;

    for(const auto &p : usb_pins)
    {
        p.set_as_af();
    }
    usb_vbus.set_as_input(pin::pup::PullDown);

    // Enable voltage detect
    USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_VBDEN;

    // B-peripheral session valid override enable
    //USB1_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    //USB1_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;

    // Force device mode
    USB_OTG_HS->GUSBCFG &= ~USB_OTG_GUSBCFG_FHMOD;
    USB_OTG_HS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

    // Enable USB power supervisor
    PWR->CSR2 |= PWR_CSR2_USB33DEN;

    // Do we need this?
    PWR->CSR2 |= PWR_CSR2_USBHSREGEN;
}

extern "C" void OTG_HS_IRQHandler()
{
    tud_int_handler(1);
}

bool usb_process_start()
{
    p_usb.stack_preference = STACK_PREFERENCE_SDRAM_RAM_TCM;
    p_usb.argc = 0;
    p_usb.argv = nullptr;
    p_usb.brk = 0;
    p_usb.code_data = InvalidMemregion();
    p_usb.cwd = "/";
    p_usb.default_affinity = PreferM4;
    p_usb.for_deletion = false;
    p_usb.heap = InvalidMemregion();
    p_usb.name = "usb";
    p_usb.next_key = 0;
    for(int i = 0; i < GK_MAX_OPEN_FILES; i++)
        p_usb.open_files[i] = nullptr;
    p_usb.screen_h = 480;
    p_usb.screen_w = 640;
    p_usb.restart_func = usb_process_start;
    memcpy(p_usb.p_mpu, mpu_default, sizeof(mpu_default));

    proc_list.RegisterProcess(&p_usb);

    Schedule(Thread::Create("tusb", usb_task, nullptr, true, GK_PRIORITY_VHIGH, p_usb, CPUAffinity::PreferM4));

    return true;
}

void *usb_task(void *pvParams)
{
    (void)pvParams;

    klog("usb: thread starting\n");

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

        //tud_task();
    }
}
