#include "supervisor.h"
#include "ext4_thread.h"
#include "screen.h"
#include "sd.h"
#include "pwr.h"
#include "reset.h"

void supervisor_shutdown_system()
{
    /* First, switch off the screen */
    screen_set_brightness(0, false);
    LTDC->GCR = 0;
    klog("shutdown: screen off\n");

    /* To shut down, we need to make sure there are no SD card transactions ongoing */
    SimpleSignal ss_ext4;
    WaitSimpleSignal_params wss_ext4;
    auto emsg = ext4_unmount_message(ss_ext4, wss_ext4);
    ext4_send_message(emsg);
    ss_ext4.Wait(SimpleSignal::SignalOperation::Noop, 0, clock_cur() + kernel_time::from_ms(500));
    klog("shutdown: ext4 unmounted: %d\n", ss_ext4.Value());

    sd_request sdr;
    SimpleSignal ss_sd;
    int ret_sd;
    sdr.block_count = 0xffffffff;
    sdr.block_start = 0xffffffff;
    sdr.mem_address = (void *)0xffffffff;
    sdr.completion_event = &ss_sd;
    sdr.res_out = &ret_sd;
    sd_perform_transfer_async(sdr);
    ss_sd.Wait(SimpleSignal::SignalOperation::Noop, 0, clock_cur() + kernel_time::from_ms(500));
    klog("shutdown: sd disabled: %d\n", ss_sd.Value());

    /* Pull the power plug */
    pwrbtn_setvregen(0);
    Block(clock_cur() + kernel_time::from_ms(500));

    /* Shouldn't get this far */
    klog("shutdown: power did not switch off.  Resetting...\n");
    gk_reset();
}
