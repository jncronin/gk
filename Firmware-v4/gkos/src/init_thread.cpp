#include "process.h"

#include "logger.h"
#include "syscalls_int.h"
#include "elf.h"
#include "usb.h"
#include "fs_provision.h"
#include "_gk_scancodes.h"
#include <fcntl.h>

PProcess p_test;

void *init_thread(void *)
{
    // Provision FS prior to usb start
    fs_provision();
    
    usb_process_start();

    // start gkmenu
    auto proc_fd = syscall_open("/gkmenu-0.1.1-gk/bin/gkmenu", O_RDONLY, 0, &errno);
    //auto proc_fd = syscall_open("/glgears-0.1.1-gkv4/bin/glgears", O_RDONLY, 0, &errno);
    if(proc_fd < 0)
    {
        klog("init: failed to open test process\n");
    }

    klog("init: opened test process fd %d\n", proc_fd);
    p_test = Process::Create("test", false, GetCurrentThreadForCore()->p);

    Thread::threadstart_t test_ep;
    auto ret = elf_load_fildes(proc_fd, p_test, &test_ep);
    klog("init: elf_load_fildes: ret: %d, ep: %llx\n", ret, test_ep);

    p_test->env.cwd = "/gkmenu-0.1.1-gk";
    p_test->env.args.clear();
    //p_test->env.args.push_back("Doom");

    p_test->screen.screen_pf = GK_PIXELFORMAT_RGB565;
    p_test->screen.screen_w = 640;
    p_test->screen.screen_h = 480;

    // gkmenu keymap
    memset(&p_test->keymap, 0, sizeof(p_test->keymap));
    p_test->keymap.gamepad_to_scancode[GK_KEYA] = GK_SCANCODE_RETURN;
    p_test->keymap.gamepad_to_scancode[GK_KEYB] = GK_SCANCODE_ESCAPE;
    p_test->keymap.gamepad_to_scancode[GK_KEYJOYDIGILEFT] = GK_SCANCODE_AUDIOPREV;
    p_test->keymap.gamepad_to_scancode[GK_KEYJOYDIGIRIGHT] = GK_SCANCODE_AUDIONEXT;
    p_test->keymap.gamepad_to_scancode[GK_KEYJOYDIGIUP] = GK_SCANCODE_AUDIOPREV;
    p_test->keymap.gamepad_to_scancode[GK_KEYJOYDIGIDOWN] = GK_SCANCODE_AUDIONEXT;
    p_test->keymap.gamepad_to_scancode[GK_KEYLEFT] = GK_SCANCODE_AUDIOPREV;
    p_test->keymap.gamepad_to_scancode[GK_KEYRIGHT] = GK_SCANCODE_AUDIONEXT;
    p_test->keymap.gamepad_to_scancode[GK_KEYUP] = GK_SCANCODE_AUDIOPREV;
    p_test->keymap.gamepad_to_scancode[GK_KEYDOWN] = GK_SCANCODE_AUDIONEXT;

    if(ret == 0)
    {
        auto t_test = Thread::Create("gkmenu", test_ep, nullptr, false, GK_PRIORITY_NORMAL, p_test);
        if(t_test)
        {
            SetFocusProcess(p_test);
            klog("init: thread created, scheduling it\n");
            sched.Schedule(t_test);
        }    
    }

    while(true)
    {
        Block();
    }
}
