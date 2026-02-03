#include "process.h"

#include "logger.h"
#include "syscalls_int.h"
#include "elf.h"
#include "usb.h"
#include "fs_provision.h"
#include "_gk_scancodes.h"
#include "supervisor.h"
#include <fcntl.h>

PProcess p_gkmenu;
id_t pid_gkmenu;

void *init_thread(void *)
{
    // Provision FS prior to usb start
    fs_provision();
    
    usb_process_start();

    // start supervisor
    init_supervisor();

    // start gkmenu
    auto proc_fd = syscall_open("/gkmenu-0.1.1-gk/bin/gkmenu", O_RDONLY, 0, &errno);
    //auto proc_fd = syscall_open("/glgears-0.1.1-gkv4/bin/glgears", O_RDONLY, 0, &errno);
    if(proc_fd < 0)
    {
        klog("init: failed to open test process\n");
    }

    klog("init: opened gkmenu process fd %d\n", proc_fd);
    p_gkmenu = Process::Create("gkmenu", false, GetCurrentProcessForCore());

    Thread::threadstart_t test_ep;
    auto ret = elf_load_fildes(proc_fd, p_gkmenu, &test_ep);
    klog("init: elf_load_fildes: ret: %d, ep: %llx\n", ret, test_ep);

    p_gkmenu->env.cwd = "/gkmenu-0.1.1-gk";
    p_gkmenu->env.args.clear();
    //p_gkmenu->env.args.push_back("Doom");

    p_gkmenu->screen.screen_pf = GK_PIXELFORMAT_RGB565;
    p_gkmenu->screen.screen_w = 640;
    p_gkmenu->screen.screen_h = 480;
    p_gkmenu->screen.updates_each_frame = GK_SCREEN_UPDATE_PARTIAL_NOREADBACK;
    p_gkmenu->cpu_freq = 800000000U;

    // gkmenu keymap
    memset(&p_gkmenu->keymap, 0, sizeof(p_gkmenu->keymap));
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYA] = GK_SCANCODE_RETURN;
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYB] = GK_SCANCODE_ESCAPE;
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYJOYDIGILEFT] = 0;
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYJOYDIGIRIGHT] = 0;
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYJOYDIGIUP] = GK_SCANCODE_AUDIOPREV;
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYJOYDIGIDOWN] = GK_SCANCODE_AUDIONEXT;
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYLEFT] = 0;
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYRIGHT] = 0;
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYUP] = GK_SCANCODE_AUDIOPREV;
    p_gkmenu->keymap.gamepad_to_scancode[GK_KEYDOWN] = GK_SCANCODE_AUDIONEXT;

    pid_gkmenu = p_gkmenu->id;

    if(ret == 0)
    {
        auto t_test = Thread::Create("gkmenu", test_ep, nullptr, false, GK_PRIORITY_NORMAL, p_gkmenu);
        if(t_test)
        {
            SetFocusProcess(p_gkmenu);
            klog("init: thread created, scheduling it\n");
            sched.Schedule(t_test);
        }    
    }

    while(true)
    {
        Block();
    }
}
