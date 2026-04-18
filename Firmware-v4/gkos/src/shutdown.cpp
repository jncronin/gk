#include "supervisor.h"
#include "ext4_thread.h"
#include "screen.h"
#include "sd.h"
#include "pwr.h"
#include "pmic.h"
#include "sound.h"
//#include "reset.h"
#include "vmem.h"
#include <stm32mp2xx.h>

#define USART6_VMEM ((USART_TypeDef *)PMEM_TO_VMEM(USART6_BASE))

static void supervisor_graceful_close()
{
    /* First, switch off the screen and sound */
    screen_set_brightness(0, false);
    sound_set_volume(0, false);
    
    //LTDC->GCR = 0;
    klog("shutdown: screen off\n");

    /* To shut down, we need to make sure there are no SD card transactions ongoing */
    extern bool usb_israwsd;
    if(!usb_israwsd)
    {
        int errno_ext4 = 0;
        gk_ext4_unmount(&errno_ext4);
        klog("shutdown: ext4 unmounted: %d\n", errno_ext4);
    }

    sd_unmount();
    klog("shutdown: sd disabled\n");
    while(!(USART6_VMEM->ISR & USART_ISR_TXFE));
}

void supervisor_shutdown_system()
{
    klog("shutdown: begin shutdown\n");
    supervisor_graceful_close();

    /* Pull the power plug */
    pmic_switchoff();
    Block(clock_cur() + kernel_time_from_ms(500));

    /* Shouldn't get this far */
    klog("shutdown: power did not switch off.  Resetting...\n");
    while(!(USART6_VMEM->ISR & USART_ISR_TXFE));
    pmic_reset();
    while(true);
}

void supervisor_reboot_system()
{
    klog("shutdown: begin reset\n");
    supervisor_graceful_close();

    pmic_reset();
    while(true);
}
