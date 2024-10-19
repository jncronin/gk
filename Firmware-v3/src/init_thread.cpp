#include "process.h"
#include "syscalls_int.h"
#include "buttons.h"
#include "fs_provision.h"
#include "supervisor.h"
#include "sound.h"
#include "i2c.h"
#include "usb.h"
#include <cstring>
#include <cmath>

pid_t pid_gkmenu = 0;

/* Init thread - loads services from sdcard */
void *init_thread(void *p)
{
    init_i2c();

    sound_set_extfreq(22050*1024);
    init_sound();
    
    init_supervisor();

#if 0
    // audio test
    deferred_call(syscall_audiosetmode, 2, 16, 22050, 4096);
    int csamp = 0;
    void *buf = nullptr;
    deferred_call(syscall_audioqueuebuffer, nullptr, &buf);
    deferred_call(syscall_audioenable, 1);
    while(true)
    {
        // 1 second on, 2 seconds off
        auto cbuf = reinterpret_cast<int16_t *>(buf);
        for(int i = 0; i < 1024; i++)
        {
            if(csamp < 22050)
            {
                auto omega = (double)csamp * 440.0 /  22050.0;
                while(omega >= 1.0) omega -= 1.0;
                auto val = (int16_t)(std::sin(omega * 2.0 * M_PI) * 32000.0);
                cbuf[i * 2] = val;
                cbuf[i * 2 + 1] = val;
            }
            else
            {
                cbuf[i * 2] = 0;
                cbuf[i * 2 + 1] = 0;
            }

            csamp++;
            if(csamp >= 22050*3) csamp = 0;
        }
        deferred_call(syscall_audioqueuebuffer, buf, &buf);
        deferred_call(syscall_audiowaitfree);
    }
#endif

    // Provision root file system, then allow USB write access to MSC
    fs_provision();
#if GK_ENABLE_USB
    Schedule(Thread::Create("tusb", usb_task, nullptr, true, GK_PRIORITY_VHIGH, kernel_proc, CPUAffinity::PreferM4));
#endif

    proccreate_t pt;
    memset(&pt, 0, sizeof(pt));
    pt.core_mask = M4Only;
    pt.is_priv = 0;
#if GK_ENABLE_NET
    deferred_call(syscall_proccreate, "/bin/tftpd", &pt, &errno);
    deferred_call(syscall_proccreate, "/bin/echo", &pt);
#endif

    pt.core_mask = M7Only;
    pt.pixel_format = GK_PIXELFORMAT_RGB565;
    pt.with_focus = 1;

    return nullptr;

    // gkmenu
    const char *args[] = { "Xenon 2 hatari" };        // run a test game
    pt.argv = args;
    pt.argc = sizeof(args) / sizeof(char *);
    pt.cwd = "/gkmenu-0.1.1-gk";
    pt.stack_size = 64 * 1024;
    pt.heap_size = 4*1024*1024;
    pt.screen_w = 640;
    pt.screen_h = 480;
    pt.pixel_format = GK_PIXELFORMAT_RGB565;
    pt.keymap.gamepad_is_keyboard = true;
    pt.keymap.gamepad_is_mouse = true;
    pt.keymap.gamepad_is_joystick = false;
    pt.keymap.gamepad_to_scancode[Process::GamepadKey::Left] = 259;    // NEXT
    pt.keymap.gamepad_to_scancode[Process::GamepadKey::Right] = 258;   // PREV
    pt.keymap.gamepad_to_scancode[Process::GamepadKey::Up] = 259;      // NEXT
    pt.keymap.gamepad_to_scancode[Process::GamepadKey::Down] = 258;    // PREV
    pt.keymap.gamepad_to_scancode[Process::GamepadKey::A] = 40;        // RETURN
    pt.keymap.gamepad_to_scancode[Process::GamepadKey::B] = 41;        // ESC
    pt.stack_preference = STACK_PREFERENCE_SDRAM_RAM_TCM; // try to leave TCM for games

    deferred_call(syscall_proccreate, "/gkmenu-0.1.1-gk/bin/gkmenu", &pt, &pid_gkmenu);

    extern Process p_supervisor;
    p_supervisor.events.Push({ .type = Event::CaptionChange });

    return nullptr;
}
