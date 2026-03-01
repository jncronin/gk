#include "process.h"

#include "logger.h"
#include "syscalls_int.h"
#include "elf.h"
#include "usb.h"
#include "fs_provision.h"
#include "_gk_scancodes.h"
#include "supervisor.h"
#include "wifi_airoc_if.h"
#include <fcntl.h>
#include "osnet_onconnect_userprocess.h"

PProcess p_gksupervisor;
id_t pid_gksupervisor;
std::unique_ptr<WifiAirocNetInterface> airoc_if;

void *init_thread(void *)
{
    // Provision FS prior to usb start
    fs_provision();
    
    usb_process_start();
#if GK_ENABLE_NETWORK && GK_ENABLE_WIFI
    airoc_if = std::make_unique<WifiAirocNetInterface>();
    airoc_if->DynamicIP = true;
    airoc_if->OnIPAssign.push_back(std::make_unique<NTPOnConnectScript>());
    airoc_if->OnIPAssign.push_back(std::make_unique<TelnetOnConnectScript>());
    net_register_interface(airoc_if.get());
#endif

    // start supervisor
    init_supervisor();

    // start gksupervisor
    auto proc_fd = syscall_open("/gksupervisor-0.1.1-gk/bin/gksupervisor", O_RDONLY, 0, &errno);
    //auto proc_fd = syscall_open("/glgears-0.1.1-gkv4/bin/glgears", O_RDONLY, 0, &errno);
    if(proc_fd < 0)
    {
        klog("init: failed to open test process\n");
    }

    klog("init: opened gksupervisor process fd %d\n", proc_fd);
    p_gksupervisor = Process::Create("gksupervisor", false, GetCurrentProcessForCore());

    Thread::threadstart_t test_ep;
    auto ret = elf_load_fildes(proc_fd, p_gksupervisor, &test_ep);
    klog("init: elf_load_fildes: ret: %d, ep: %llx\n", ret, test_ep);

    p_gksupervisor->env.cwd = "/gksupervisor-0.1.1-gk";
    p_gksupervisor->env.args.clear();
    //p_gksupervisor->env.args.push_back("Doom");

    p_gksupervisor->screen.screen_pf = GK_PIXELFORMAT_ARGB8888;
    p_gksupervisor->screen.screen_w = 800;
    p_gksupervisor->screen.screen_h = 480;
    p_gksupervisor->screen.updates_each_frame = GK_SCREEN_UPDATE_PARTIAL_NOREADBACK;
    p_gksupervisor->priv_overlay_fb = true;
    p_gksupervisor->cpu_freq = 800000000U;

    // gksupervisor keymap
    memset(&p_gksupervisor->keymap, 0, sizeof(p_gksupervisor->keymap));
    p_gksupervisor->keymap.touch_is_mouse = 1;

    pid_gksupervisor = p_gksupervisor->id;

    if(ret == 0)
    {
        auto t_test = Thread::Create("gksupervisor", test_ep, nullptr, false, GK_PRIORITY_NORMAL, p_gksupervisor);
        if(t_test)
        {
            SetFocusProcess(p_gksupervisor);
            klog("init: thread created, scheduling it\n");
            sched.Schedule(t_test);
        }    
    }

    while(true)
    {
        Block();
    }
}
