#include "process.h"
#include "syscalls_int.h"
#include "buttons.h"
#include "fs_provision.h"
#include "supervisor.h"
#include <cstring>

pid_t pid_gkmenu = 0;

/* Init thread - loads services from sdcard */
void *init_thread(void *p)
{

    //init_supervisor();

    // Provision root file system, then allow USB write access to MSC
    //fs_provision();
#if GK_ENABLE_USB
    Schedule(Thread::Create("tusb", usb_task, nullptr, true, GK_PRIORITY_VHIGH, kernel_proc, CPUAffinity::M7Only));
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

    // gkmenu
    const char *args[] = { };
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

    deferred_call(syscall_proccreate, "/gkmenu-0.1.1-gk/bin/gkmenu", &pt, &pid_gkmenu);
#if 0
    // gkmusic
    const char *args[] = { "mariner.mp3", "48000" };
    pt.argv = args;
    pt.argc = sizeof(args) / sizeof(char *);
    pt.cwd = "/gkmusic-0.1.1-gk";
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

    deferred_call(syscall_proccreate, "/gkmusic-0.1.1-gk/bin/gkmusic", &pt, nullptr);
#endif
#if 0
    // doom
    const char *args[] = { "-nosound", "-nomusic", "-nosfx",
        "-iwad", "/share/doom/doom1.wad" };
    pt.argv = args;
    pt.argc = sizeof(args) / sizeof(char *);
    pt.cwd = "/gkmenu-0.1.1-gk";
    pt.stack_size = 64 * 1024;
    pt.heap_size = 16*1024*1024;
    pt.screen_w = 320;
    pt.screen_h = 240;
    pt.pixel_format = GK_PIXELFORMAT_RGB565;
    deferred_call(syscall_proccreate, "/sdl2-doom-0.1.1-gk/bin/sdl2-doom", &pt);
#endif


    extern Process p_supervisor;
    p_supervisor.events.Push({ .type = Event::CaptionChange });

    //jpeg_test();

    //init_buttons();

    return nullptr;
}