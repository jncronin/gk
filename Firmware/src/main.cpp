#include <stm32h7xx.h>
#include "memblk.h"
#include "thread.h"
#include "scheduler.h"
#include "fmc.h"
#include "pins.h"
#include "screen.h"
#include "syscalls.h"
#include "SEGGER_RTT.h"
#include "clocks.h"
#include "gpu.h"
#include <cstdlib>
#include "elf.h"
#include "btnled.h"
#include "sd.h"
#include "ext4_thread.h"
#include <cstring>
#include "usb.h"
#include "osnet.h"
#include "wifi.h"
#include "syscalls_int.h"
#include "fs_provision.h"
#include "mdma.h"
#include "ipi.h"
#include "supervisor.h"
#include "buttons.h"
#include "i2c.h"
#include "gk_conf.h"

__attribute__((section(".sram4"))) Spinlock s_rtt;
extern Condition scr_vsync;

void system_init_cm7();
void system_init_cm4();

static void *idle_thread(void *p);

void *bluescreen_thread(void *p);
void *b_thread(void *p);
void *x_thread(void *p);

void *net_telnet_thread(void *p);

static void *init_thread(void *p);
static void *init_m4_thread(void *p);

extern void jpeg_test();

#if GK_DUAL_CORE_AMP
SRAM4_DATA Scheduler scheds[2] { Scheduler(), Scheduler() };
#else
SRAM4_DATA Scheduler sched;
#endif
__attribute__((section(".sram4")))Process kernel_proc;

extern char _binary__home_jncronin_src_gk_test_build_gk_test_bin_start;

extern uint32_t m4_wakeup;

SRAM4_DATA std::vector<std::string> empty_string_vector;

int main()
{
    EXTI->C2IMR3 |= EXTI_IMR3_IM79;
    NVIC_EnableIRQ(CM4_SEV_IRQn);
    __enable_irq();

    system_init_cm7();
    init_memblk();
    init_sdram();
    init_screen();
    init_btnled();
    init_sd();
    init_ext4();
    init_mdma();
    init_i2c();

#if GK_ENABLE_NETWORK
    init_net();
    init_wifi();
#endif

    usb_init_chip_id();     // can only read UID_BASE et al from M7

    btnled_setcolor(0x020202UL);

    elf_load_memory(&_binary__home_jncronin_src_gk_test_build_gk_test_bin_start, "gktest");

    kernel_proc.name = "kernel";

    Schedule(Thread::Create("idle_cm7", idle_thread, (void*)0, true, GK_PRIORITY_IDLE, kernel_proc, CPUAffinity::M7Only,
        memblk_allocate_for_stack(512, CPUAffinity::M7Only)));
    Schedule(Thread::Create("idle_cm4", idle_thread, (void*)1, true, GK_PRIORITY_IDLE, kernel_proc, CPUAffinity::M4Only,
        memblk_allocate_for_stack(512, CPUAffinity::M4Only)));

#if GK_ENABLE_TEST_THREADS
    Schedule(Thread::Create("blue", bluescreen_thread, nullptr, true, GK_PRIORITY_NORMAL, kernel_proc));
    Schedule(Thread::Create("b", b_thread, nullptr, true, GK_PRIORITY_NORMAL, kernel_proc, CPUAffinity::Either, InvalidMemregion(),
        MPUGenerate(GK_SDRAM_BASE, 0x800000, GK_PRIORITY_NORMAL, false, MemRegionAccess::RW, MemRegionAccess::NoAccess,
        WT_NS)));
    Schedule(Thread::Create("c", x_thread, (void *)'C', true, GK_PRIORITY_NORMAL, kernel_proc));
    Schedule(Thread::Create("d", x_thread, (void *)'D', true, GK_PRIORITY_NORMAL, kernel_proc));
#endif

    Schedule(Thread::Create("gpu", gpu_thread, nullptr, true, GK_PRIORITY_VHIGH, kernel_proc, CPUAffinity::PreferM7));

#if GK_ENABLE_NET
    uint32_t myip = IP4Addr(192, 168, 7, 1).get();
    s.Schedule(Thread::Create("dhcpd", net_dhcpd_thread, (void *)myip, true, GK_PRIORITY_NORMAL, kernel_proc, CPUAffinity::PreferM4));
    s.Schedule(Thread::Create("telnet", net_telnet_thread, nullptr, true, GK_PRIORITY_NORMAL, kernel_proc, CPUAffinity::PreferM4));
    s.Schedule(Thread::Create("wifi", wifi_task, nullptr, true, GK_PRIORITY_NORMAL, kernel_proc, CPUAffinity::PreferM4));
#endif

    auto init_stack = memblk_allocate(8192, MemRegionType::AXISRAM);
    Schedule(Thread::Create("init", init_thread, nullptr, true, GK_PRIORITY_NORMAL, kernel_proc, CPUAffinity::PreferM7, init_stack));

#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
    Schedule(Thread::Create("init_m4", init_m4_thread, nullptr, true, GK_PRIORITY_NORMAL, kernel_proc, CPUAffinity::M4Only));

    // Reset M4
    RCC->APB1LENR |= RCC_APB1LENR_WWDG2EN;
    (void)RCC->APB1LENR;
    WWDG2->CR = WWDG_CR_WDGA;

    delay_ms(5);

    // Nudge M4 to wakeup
    ipi_messages[1].Write( { ipi_message::M4Wakeup, nullptr });
    __SEV();
#endif

    // Prepare systick
    SysTick->CTRL = 0;
    SysTick->VAL = 0;
    SysTick->LOAD = 7680000UL - 1UL;    // 20 ms tick at 384 MHz    

    s().StartForCurrentCore();

    return 0;
}

extern "C" int main_cm4()
{
    system_init_cm4();

    // ensure we can reach HSEM
    RCC->AHB4ENR |= RCC_AHB4ENR_HSEMEN;
    (void)RCC->AHB4ENR;

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "kernel: M4 awaiting scheduler setup\n");
    }

    EXTI->C2IMR3 |= EXTI_IMR3_IM80;
    NVIC_EnableIRQ(CM7_SEV_IRQn);
    __enable_irq();

    // wait upon startup - should signal from idle_cm7
    while(m4_wakeup != M4_MAGIC)
    {
        __WFI();
    }

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "kernel: starting M4\n");
    }

    // Prepare systick
    SysTick->CTRL = 0;
    SysTick->VAL = 0;
    SysTick->LOAD = 3764705U;      // 19.6 ms tick at 192 MHz 
        // (so we don't always try and switch tasks on both cores at the same time)

    s().StartForCurrentCore();

    return 0;
}

void *idle_thread(void *p)
{
    (void)p;
    while(true)
    {
        __WFI();
    }
}

void *bluescreen_thread(void *p)
{
    (void)p;
    /*__syscall_SetFrameBuffer((void *)GK_SDRAM_BASE, (void *)0xc0200000, ARGB8888);
    for(int i = 0; i < 640*480; i++)
    {
        ((volatile uint32_t *)GK_SDRAM_BASE)[i] = 0xff0000ff;
    }
    __syscall_FlipFrameBuffer();*/
    uint64_t last_update = 0ULL;
    while(true)
    {
        if(clock_cur_ms() > (last_update + 250ULL))
        {
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "Hello from %s\n",
                    GetCoreID() ? "M4" : "M7");
            }
            last_update = clock_cur_ms();

        }
        //CriticalGuard cg(s_rtt);
        //SEGGER_RTT_PutChar(0, 'A');
    }
}

static uint32_t rrand()
{
    while(!(RNG->SR & RNG_SR_DRDY));
    return RNG->DR;
}

void *b_thread(void *p)
{
    (void)p;

    auto bb = memblk_allocate(0x200000, MemRegionType::SDRAM);

    if(!bb.valid)
        return nullptr;

    RCC->AHB3ENR |= RCC_AHB3ENR_DMA2DEN;
    (void)RCC->AHB3ENR;

    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    (void)RCC->AHB2ENR;

    clock_set_cpu(clock_cpu_speed::cpu_384_192);

    uint32_t *backbuffer = (uint32_t *)bb.address;
    int cur_m = 320;
    int cur_y = 0;
    int cur_w = 80;

    RNG->CR = RNG_CR_RNGEN;

    // try and load mbr
    auto mbr = memblk_allocate(512, MemRegionType::AXISRAM);
    //SCB_CleanInvalidateDCache();
    memset((void*)mbr.address, 0, 512);
    auto sdt = sd_perform_transfer(0, 1, (void*)mbr.address, true);
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "sdtest: %d, last word: %lx\n", sdt,
            *(uint32_t *)(mbr.address + 508));
    }

    //uint32_t cc = 0;
    int nframes = 0;
    while(true)
    {
        scr_vsync.Wait();
        nframes++;

        while(GPUBusy())
        {
            scr_vsync.Wait();
            nframes++;
        }

        {
            CriticalGuard cg(s_rtt);
            //SEGGER_RTT_printf(0, "nframes: %d\n", nframes);            
        }

        int l_val = cur_m - cur_w / 2;
        int r_val = cur_m + cur_w / 2;
        if(l_val < 5) l_val = 5;
        if(r_val >= 635) r_val = 634;

        for(int f = 0; f < nframes; f++, cur_y++)
        {
            uint32_t *row = &backbuffer[cur_y * 640];
            for(int i = 0; i < 640; i++)
            {
                if(i < l_val)
                {
                    row[i] = 0xff00ff00;
                }
                else if(i < (l_val + 5))
                {
                    row[i] = 0xffffff00;
                }
                else if(i < (r_val - 5))
                {
                    row[i] = 0xff0000ff;
                }
                else if(i < r_val)
                {
                    row[i] = 0xffffff00;
                }
                else
                {
                    row[i] = 0xff00ff00;
                }
            }
            SCB_CleanDCache_by_Addr(row, 640*4);

            if(cur_y >= 480) cur_y = 0;
        }

        auto r1 = rrand() % 4;
        auto r2 = rrand() % 4;
        if(r1 == 2)
        {
            cur_m++;
            if(cur_m >= 480) cur_m = 480;
        }
        if(r1 == 3)
        {
            cur_m--;
            if(cur_m <= 160) cur_m = 160;
        }
        if(r2 == 2)
        {
            cur_w++;
            if(cur_w >= 230) cur_w = 230;
        }
        if(r2 == 3)
        {
            cur_w--;
            if(cur_w <= 30) cur_w = 30;
        }
        nframes = 0;

        // blit
        gpu_message gpu_msgs[] =
        {
            GPUMessageBlitRectangle(backbuffer, 0, cur_y, 640, 480 - cur_y, 0, 0),
            GPUMessageBlitRectangle(backbuffer, 0, 0, 640, cur_y, 0, 480 - cur_y),
            GPUMessageFlip()
        };
        GPUEnqueueMessages(gpu_msgs, sizeof(gpu_msgs)/sizeof(gpu_message));
        //GPUEnqueueBlitRectangle(backbuffer, 0, cur_y, 640, 480 - cur_y, 0, 0);
        //GPUEnqueueBlitRectangle(backbuffer, 0, 0, 640, cur_y, 0, 480 - cur_y);
        //GPUEnqueueFlip();

#if 0
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_PutChar(0, '\n');
            SEGGER_RTT_PutChar(0, 'B');
            
        }

        auto fbuf = reinterpret_cast<uint32_t *>(__syscall_GetFrameBuffer());
        if(fbuf)
        {
            /*for(int y = 0; y < 32; y++)
            {
                for(int x = 0; x < 32; x++)
                {
                    fbuf[x + y * 640] = 0xff000000 | (cc << 16) | (cc << 8) | cc;
                }
            }*/
            /*for(int i = 0; i < 640*480; i++)
            {
                fbuf[i] = 0xff000000 | (cc << 16) | (cc << 8) | cc;
            }*/
            GPUEnqueueFBColor(0xff000000 | (cc << 16) | (cc << 8) | cc);
            GPUEnqueueFlip();

            /* DMA2D->OPFCCR = 0UL;
            DMA2D->OCOLR = 0xff000000 | (cc << 16) | (cc << 8) | cc;
            DMA2D->OMAR = (uint32_t)(uintptr_t)fbuf;
            DMA2D->OOR = 0;
            DMA2D->NLR = (640UL << DMA2D_NLR_PL_Pos) |
                (480UL << DMA2D_NLR_NL_Pos);
            DMA2D->CR = (3UL << DMA2D_CR_MODE_Pos) |
                DMA2D_CR_START;

            while(DMA2D->CR & DMA2D_CR_START);
            //SCB_CleanDCache_by_Addr(fbuf, 640*480*4);
            __syscall_FlipFrameBuffer(); */
            cc++;
            if(cc >= 256) cc = 0;
        }

#endif
    }
}

void *x_thread(void *p)
{
    while(true)
    {
        //CriticalGuard cg(s_rtt);
        //SEGGER_RTT_PutChar(0, (char)(uintptr_t)p);
    }
}

/* M4 init thread - starts services on M4 */
void *init_m4_thread(void *p)
{
    init_buttons();

    return nullptr;
}

/* Init thread - loads services from sdcard */
void *init_thread(void *p)
{
    clock_set_cpu(clock_cpu_speed::cpu_384_192);

    init_supervisor();

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

    pt.heap_size = 8192*1024*4;
    pt.core_mask = M7Only;
    pt.pixel_format = GK_PIXELFORMAT_RGB565;
    //pt.pixel_format = GK_PIXELFORMAT_ARGB8888;
    pt.with_focus = 1;
    //deferred_call(syscall_proccreate, "/tglgears-0.1.1-gk/bin/tglgears", &pt);
#if 0
    const char *args[] = { "-f", "--max-width", "640", "--max-height", "480", "-z", "1.0",
        "--sound", "off", "--fast-boot", "on",
        "--compatible", "off",
        "--cpu-exact", "off",
        "--fpu", "none",
        "--mmu", "off",
        /* "--memstate", "/share/hatari/games/xenon2.ram", */
        "-d", "none",
        "--timer-d", "on",
        "/share/hatari/games/xenon2.st" };
#endif
#if 0
    const char *args[] = { "-nosound", "-nomusic", "-nosfx", "-iwad", "/share/doom/doom1.wad" };
#endif
#if 0
    const char *args[] = { "/share/dosbox/TIE/TIE.EXE" };

    pt.argv = args;
    pt.argc = sizeof(args) / sizeof(char *);
    pt.stack_size = 32 * 1024;
    pt.heap_is_exec = 1;
    //deferred_call(syscall_proccreate, "/Hatari-0.1.1-gk/bin/hatari", &pt);
    //deferred_call(syscall_proccreate, "/sdl2-doom-0.1.1-gk/bin/sdl2-doom", &pt);
    deferred_call(syscall_proccreate, "/DOSBOX-0.74-gk/bin/dosbox", &pt);
    focus_process->screen_w = 320;
    focus_process->screen_h = 240;
    //focus_process->gamepad_to_scancode[Process::GamepadKey::A] = 224; // LCTRL
    //focus_process->gamepad_to_scancode[Process::GamepadKey::B] = 44;  // SPACE
    focus_process->gamepad_to_scancode[Process::GamepadKey::X] = 40;  // RETURN
    focus_process->gamepad_to_scancode[Process::GamepadKey::Y] = 41;  // ESCAPE
    focus_process->gamepad_to_scancode[Process::GamepadKey::Left] = 0;
    focus_process->gamepad_to_scancode[Process::GamepadKey::Right] = 0;
    focus_process->gamepad_to_scancode[Process::GamepadKey::Up] = 0;
    focus_process->gamepad_to_scancode[Process::GamepadKey::Down] = 0;
    focus_process->gamepad_to_scancode[Process::GamepadKey::A] = 0;
    focus_process->gamepad_to_scancode[Process::GamepadKey::B] = 0;
    focus_process->gamepad_is_mouse = true;
    focus_process->gamepad_is_joystick = false;
#endif
#if 0
    // Pacman
    const char *args[] = { };
    pt.argv = args;
    pt.argc = sizeof(args) / sizeof(char *);
    pt.cwd = "/Pacman-0.1.1-gk";
    deferred_call(syscall_proccreate, "/Pacman-0.1.1-gk/pacman", &pt);
#endif
#if 0
    // Pacman
    const char *args[] = { };
    pt.argv = args;
    pt.argc = sizeof(args) / sizeof(char *);
    pt.cwd = "/pacman_ebuc99-0.1.1-gk";
    pt.stack_size = 32 * 1024;
    deferred_call(syscall_proccreate, "/pacman_ebuc99-0.1.1-gk/pacman", &pt);
    focus_process->gamepad_is_mouse = true;
    focus_process->gamepad_is_joystick = false;
    focus_process->gamepad_to_scancode[Process::GamepadKey::A] = 40;  // RETURN
#endif
#if 0
    // mesa gears
    const char *args[] = { };
    pt.argv = args;
    pt.argc = sizeof(args) / sizeof(char *);
    pt.cwd = "/glgears-0.1.1-gk";
    pt.stack_size = 64 * 1024;
    deferred_call(syscall_proccreate, "/glgears-0.1.1-gk/bin/glgears", &pt);
#endif
#if 0
    // tuxracer
    const char *args[] = { "-debug", "all" };
    pt.argv = args;
    pt.argc = sizeof(args) / sizeof(char *);
    pt.cwd = "/tuxracer-0.1.1-gk";
    pt.stack_size = 64 * 1024;
    deferred_call(syscall_proccreate, "/tuxracer-0.1.1-gk/bin/tuxracer", &pt);
#endif
    // mednafen
    const char *args[] = { 
        "-video.driver", "softfb",
        "-video.fs", "1",
        //"-nes.stretch", "0",
        "-sound", "0",
        "-sound.rate", "22050",
        "-gb.stretch", "aspect_int",
//        "-nothrottle", "1",
        "-osd.alpha_blend", "0",
        "-video.blit_timesync", "0",
//        "-video.frameskip", "0",
        "-fps.autoenable", "1",
        "/usr/share/mednafen/games/Sonic The Hedgehog 2 (UE) (V2.2).sms"
    };
    pt.argv = args;
    pt.argc = sizeof(args) / sizeof(char *);
    pt.cwd = "/mednafen-gk";
    pt.stack_size = 64 * 1024;
    pt.pixel_format = GK_PIXELFORMAT_RGB565;
    pt.screen_w = 640;
    pt.screen_h = 480;
    pt.screen_ignore_vsync = 1;
    deferred_call(syscall_proccreate, "/mednafen-gk/bin/mednafen", &pt);

    delay_ms(5);
    /* NES mappings */
    focus_process->gamepad_is_joystick = false;
    focus_process->gamepad_is_mouse = false;
    focus_process->gamepad_is_keyboard = true;
    focus_process->gamepad_to_scancode[Process::GamepadKey::B] = 91;    // keypad 3  = MDFN A (flipped - A on right)
    focus_process->gamepad_to_scancode[Process::GamepadKey::A] = 90;    // keypad 2  = MDFN B
    focus_process->gamepad_to_scancode[Process::GamepadKey::X] = 40;    // enter     = MDFN start
    focus_process->gamepad_to_scancode[Process::GamepadKey::Y] = 43;    // tab       = MDFN select
    focus_process->gamepad_to_scancode[Process::GamepadKey::Left] = 4;  // A         = MDFN left
    focus_process->gamepad_to_scancode[Process::GamepadKey::Right] = 7; // D         = MDFN right
    focus_process->gamepad_to_scancode[Process::GamepadKey::Up] = 26;   // W         = MDFN up
    focus_process->gamepad_to_scancode[Process::GamepadKey::Down] = 22; // S         = MDFN down

    /* lynx mappings */
    focus_process->gamepad_is_joystick = false;
    focus_process->gamepad_is_mouse = false;
    focus_process->gamepad_is_keyboard = true;
    focus_process->gamepad_to_scancode[Process::GamepadKey::B] = 91;    // keypad 3  = MDFN A (flipped - A on right)
    focus_process->gamepad_to_scancode[Process::GamepadKey::A] = 90;    // keypad 2  = MDFN B
    focus_process->gamepad_to_scancode[Process::GamepadKey::X] = 89;    // keypad 1  = MDFN option 2 (lower)
    focus_process->gamepad_to_scancode[Process::GamepadKey::Y] = 95;    // keypad 7  = MDFN option 1 (upper)
    focus_process->gamepad_to_scancode[Process::GamepadKey::Left] = 4;  // A         = MDFN left
    focus_process->gamepad_to_scancode[Process::GamepadKey::Right] = 7; // D         = MDFN right
    focus_process->gamepad_to_scancode[Process::GamepadKey::Up] = 26;   // W         = MDFN up
    focus_process->gamepad_to_scancode[Process::GamepadKey::Down] = 22; // S         = MDFN down

    /* gameboy mappings */
    // use screen 640x480 with integer scaling "aspect_int" for best results
    focus_process->gamepad_is_joystick = false;
    focus_process->gamepad_is_mouse = false;
    focus_process->gamepad_is_keyboard = true;
    focus_process->gamepad_to_scancode[Process::GamepadKey::B] = 91;    // keypad 3  = MDFN A (flipped - A on right)
    focus_process->gamepad_to_scancode[Process::GamepadKey::A] = 90;    // keypad 2  = MDFN B
    focus_process->gamepad_to_scancode[Process::GamepadKey::X] = 40;    // enter     = MDFN start
    focus_process->gamepad_to_scancode[Process::GamepadKey::Y] = 43;    // tab       = MDFN select
    focus_process->gamepad_to_scancode[Process::GamepadKey::Left] = 4;  // A         = MDFN left
    focus_process->gamepad_to_scancode[Process::GamepadKey::Right] = 7; // D         = MDFN right
    focus_process->gamepad_to_scancode[Process::GamepadKey::Up] = 26;   // W         = MDFN up
    focus_process->gamepad_to_scancode[Process::GamepadKey::Down] = 22; // S         = MDFN down

    /* master system mappings */
    focus_process->gamepad_is_joystick = false;
    focus_process->gamepad_is_mouse = false;
    focus_process->gamepad_is_keyboard = true;
    focus_process->gamepad_to_scancode[Process::GamepadKey::B] = 91;    // keypad 3  = MDFN Fire 2
    focus_process->gamepad_to_scancode[Process::GamepadKey::A] = 90;    // keypad 2  = MDFN Fire 1/Start
    focus_process->gamepad_to_scancode[Process::GamepadKey::X] = 0;     // only 2 buttons on SMS
    focus_process->gamepad_to_scancode[Process::GamepadKey::Y] = 0;     // only 2 buttons on SMS
    focus_process->gamepad_to_scancode[Process::GamepadKey::Left] = 4;  // A         = MDFN left
    focus_process->gamepad_to_scancode[Process::GamepadKey::Right] = 7; // D         = MDFN right
    focus_process->gamepad_to_scancode[Process::GamepadKey::Up] = 26;   // W         = MDFN up
    focus_process->gamepad_to_scancode[Process::GamepadKey::Down] = 22; // S         = MDFN down



    extern Process p_supervisor;
    p_supervisor.events.Push({ .type = Event::CaptionChange });

    //jpeg_test();

#if !GK_DUAL_CORE && !GK_DUAL_CORE_AMP
    init_m4_thread(p);
#endif

    return nullptr;
}


/* The following just to get it to compile */
extern "C" void *malloc(size_t nbytes)
{
    return malloc_region(nbytes, REG_ID_SRAM4);
}
extern "C" void *realloc(void *ptr, size_t nbytes)
{
    return realloc_region(ptr, nbytes, REG_ID_SRAM4);
}
extern "C" void free(void *ptr)
{
    free_region(ptr, REG_ID_SRAM4);
}
extern "C" void *calloc(size_t nmemb, size_t size)
{
    return calloc_region(nmemb, size, REG_ID_SRAM4);
}

extern "C" void *sbrksram4(int n);
extern "C" void * _sbrk(int n)
{
    return sbrksram4(n);
}

__attribute__((section(".sram4"))) volatile uint32_t cfsr, hfsr, ret_addr, mmfar;
__attribute__((section(".sram4"))) volatile mpu_saved_state mpuregs[8];
__attribute__((section(".sram4"))) volatile Thread *fault_thread;
extern "C" void HardFault_Handler()
{
    /*uint32_t *pcaddr;
    __asm(  "TST lr, #4\n"
            "ITE EQ\n"
            "MRSEQ %0, MSP\n"
            "MRSNE %0, PSP\n" // stack pointer now in r0
            "ldr %0, [%0, #0x18]\n" // stored pc now in r0
            : "=r"(pcaddr));

    ret_addr = *pcaddr;*/

    fault_thread = GetCurrentThreadForCore();
    for(int i = 0; i < 8; i++)
    {
        MPU->RNR = i;
        mpuregs[i].rbar = MPU->RBAR;
        mpuregs[i].rasr = MPU->RASR;
    }

    cfsr = SCB->CFSR;
    hfsr = SCB->HFSR;
    mmfar = SCB->MMFAR;
    
    while(true);
}

extern "C" void MemManage_Handler()
{
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    SEGGER_RTT_printf(0, "panic: process %s thread %s caused memmanage fault\n",
        p.name.c_str(), t->name.c_str());
    {
        for(int i = 0; i < 8; i++)
        {
            auto cmpu = t->tss.mpuss[i];
            if(cmpu.rasr & 0x1U)
            {
                auto cstart = cmpu.rbar & ~0x1fU;
                auto clen = 2U << ((cmpu.rasr >> 1) & 0x1fU);
                SEGGER_RTT_printf(0, "panic: mpu %d: %8x - %8x\n", i, cstart, cstart + clen);
            }
            else
            {
                SEGGER_RTT_printf(0, "panic: mpu %d: disabled\n", i);
            }
        }

        for(int i = 0; i < 8; i++)
        {
            mpu_saved_state cmpu;
            MPU->RNR = i;
            cmpu.rbar = MPU->RBAR;
            cmpu.rasr = MPU->RASR;
            if(cmpu.rasr & 0x1U)
            {
                auto cstart = cmpu.rbar & ~0x1fU;
                auto clen = 2U << ((cmpu.rasr >> 1) & 0x1fU);
                SEGGER_RTT_printf(0, "panic: mpu %d: %8x - %8x\n", i, cstart, cstart + clen);
            }
            else
            {
                SEGGER_RTT_printf(0, "panic: mpu %d: disabled\n", i);
            }
        }
    }
    if(&p != &kernel_proc)
    {
        CriticalGuard cg_p(p.sl);
        for(auto thr : p.threads)
        {
            CriticalGuard cg_t(thr->sl);
            thr->for_deletion = true;
        }
        p.for_deletion = true;
        Yield();
    }
    else
    {
        HardFault_Handler();
        while(true);
    }
}

extern "C" void BusFault_Handler()
{
    HardFault_Handler();
    while(true);
}

extern "C" void UsageFault_Handler()
{
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    SEGGER_RTT_printf(0, "panic: process %s thread %s caused usage fault\n",
        p.name.c_str(), t->name.c_str());
    if(&p != &kernel_proc)
    {
        CriticalGuard cg_p(p.sl);
        for(auto thr : p.threads)
        {
            CriticalGuard cg_t(thr->sl);
            thr->for_deletion = true;
        }
        p.for_deletion = true;
        Yield();
    }
    else
    {
        HardFault_Handler();
        while(true);
    }
}

extern "C" void SysTick_Handler()
{
    Yield();
}
