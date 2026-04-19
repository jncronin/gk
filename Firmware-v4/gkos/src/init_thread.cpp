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
#include "persistent.h"
#include "etnaviv.h"
#include "screen.h"
#include "block_dev.h"
#include "sd.h"
#include "mbr.h"

PProcess p_gksupervisor;
id_t pid_gksupervisor;
std::unique_ptr<WifiAirocNetInterface> airoc_if;

extern unsigned int reboot_flags;

std::shared_ptr<BlockDevice> sd_dev;
std::shared_ptr<BlockDevice> ext_dev;
std::shared_ptr<BlockDevice> fat_dev;

void *init_thread(void *)
{
    // Clear out reboot flags (already cached in reboot_flags variable)
    if(reboot_flags)
    {
        PersistentMemoryWriteGuard pmg;
        persistent[PERSISTENT_ID_REBOOT_FLAGS] = 0;
    }

    // Load SD card.  If not present then panic.
    sd_dev = std::move(sd_get_device());
    if(!sd_dev || sd_dev->block_count() == 0)
    {
        klog("PANIC: no SD device found\n");
        screen_set_background_colour(0xff0000);
    }

    // Read partition table of SD card, if no ext partition present then enter rawsd mode
    auto ptable = mbr_parse(sd_dev);
    for(auto &pte : ptable)
    {
        if(pte.driver == "ext" && !ext_dev)
        {
            klog("mbr: found ext partition number %u\n", pte.part_no);
            ext_dev = pte.d;
        }
        else if(pte.driver == "fat" && !fat_dev)
        {
            klog("mbr: found fat partition number %u\n", pte.part_no);
            fat_dev = pte.d;
        }
    }
    if(!ext_dev)
    {
        klog("PANIC: ext partition not found.  Entering Raw SD mode for provisioning\n");
        reboot_flags |= GK_REBOOTFLAG_RAWSD;
    }
    if(!fat_dev)
    {
        klog("WARNING: fat partition not found.  USB provisioning will require Raw SD mode\n");
    }

    if(reboot_flags & GK_REBOOTFLAG_RAWSD)
    {
        // Highlight to the world that we are in raw sd mode
        klog("Entering Raw SD mode\n");
        screen_set_background_colour(0x00ff00);
    }
    else
    {
        screen_set_background_colour(0);
    }

    // Provision FS prior to usb start
    if(!(reboot_flags & GK_REBOOTFLAG_RAWSD) && fat_dev)
        fs_provision();
    
    usb_process_start();

    // start supervisor
    init_supervisor();

    if(reboot_flags & GK_REBOOTFLAG_RAWSD)
        return nullptr;
    
#if GK_ENABLE_NETWORK && GK_ENABLE_WIFI
    airoc_if = std::make_unique<WifiAirocNetInterface>();
    airoc_if->DynamicIP = true;
    airoc_if->OnIPAssign.push_back(std::make_unique<NTPOnConnectScript>());
    //airoc_if->OnIPAssign.push_back(std::make_unique<TelnetOnConnectScript>());
    net_register_interface(airoc_if.get());
#endif

    // start gksupervisor
    auto proc_fd = syscall_open("/gksupervisor-0.1.1-gk/bin/gksupervisor", O_RDONLY, 0, &errno);
    //auto proc_fd = syscall_open("/glgears-0.1.1-gkv4/bin/glgears", O_RDONLY, 0, &errno);
    if(proc_fd < 0)
    {
        klog("init: failed to open supervisor process, enabling rawsd for next reboot\n");

        screen_set_background_colour(0xff0000);
        persistent_reboot_flags_set(GK_REBOOTFLAG_RAWSD);
        return nullptr;
    }

    klog("init: opened gksupervisor process fd %d\n", proc_fd);
    p_gksupervisor = Process::Create("gksupervisor", false, GetCurrentProcessForCore());

    Thread::threadstart_t test_ep;
    auto ret = elf_load_fildes(proc_fd, p_gksupervisor, &test_ep);
    klog("init: elf_load_fildes: ret: %d, ep: %llx\n", ret, test_ep);

    p_gksupervisor->env.cwd = "/gkmenu-0.1.1-gk";   // run in gkmenu dir so we can load osd files from there
    p_gksupervisor->env.args.clear();
    //p_gksupervisor->env.args.push_back("SDL GL Test");

    p_gksupervisor->screen.screen_pf = GK_PIXELFORMAT_RGB565;
    p_gksupervisor->screen.screen_w = 800;
    p_gksupervisor->screen.screen_h = 480;
    p_gksupervisor->screen.color_key = 0x848200U;
    p_gksupervisor->screen.updates_each_frame = GK_SCREEN_UPDATE_PARTIAL_NOREADBACK;
    p_gksupervisor->priv_overlay_fb = true;
    p_gksupervisor->priv_set_brightness = true;
    p_gksupervisor->priv_control_devices = true;
    p_gksupervisor->cpu_freq = 800000000U;

    // gksupervisor keymap
    memset(&p_gksupervisor->keymap, 0, sizeof(p_gksupervisor->keymap));
    p_gksupervisor->keymap.touch_is_mouse = 1;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYA] = GK_SCANCODE_RETURN;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYB] = GK_SCANCODE_ESCAPE;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYJOYDIGILEFT] = GK_SCANCODE_AUDIOPREV;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYJOYDIGIRIGHT] = GK_SCANCODE_AUDIONEXT;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYJOYDIGIUP] = GK_SCANCODE_AUDIOPREV;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYJOYDIGIDOWN] = GK_SCANCODE_AUDIONEXT;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYLEFT] = GK_SCANCODE_LEFT;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYRIGHT] = GK_SCANCODE_RIGHT;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYUP] = GK_SCANCODE_UP;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYDOWN] = GK_SCANCODE_DOWN;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYMENU] = GK_SCANCODE_MENU;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYVOLUP] = GK_SCANCODE_VOLUMEUP;
    p_gksupervisor->keymap.gamepad_to_scancode[GK_KEYVOLDOWN] = GK_SCANCODE_VOLUMEDOWN;

    pid_gksupervisor = p_gksupervisor->id;

    if(ret == 0)
    {
        auto t_test = Thread::Create("gksupervisor", test_ep, nullptr, false, GK_PRIORITY_VERYHIGH, p_gksupervisor);
        if(t_test)
        {
            SetFocusProcess(p_gksupervisor);
            klog("init: thread created, scheduling it\n");
            sched.Schedule(t_test);
        }    
    }

    //init_etnaviv();

    while(true)
    {
        Block();
    }
}
