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

#define GK_DUAL_CORE 1

__attribute__((section(".sram4"))) Spinlock s_rtt;
extern Condition scr_vsync;

void system_init_cm7();
void system_init_cm4();

static void *idle_thread(void *p);

void *bluescreen_thread(void *p);
void *b_thread(void *p);
void *x_thread(void *p);

void *net_telnet_thread(void *p);

__attribute__((section(".sram4")))Scheduler s;
__attribute__((section(".sram4")))Process kernel_proc;

extern char _binary__home_jncronin_src_gk_test_build_gk_test_bin_start;

SRAM4_DATA static volatile uint32_t m4_wakeup = 0;
#define M4_MAGIC 0xa1b2c3d4

int main()
{
    system_init_cm7();
    init_memblk();
    init_sdram();
    init_screen();
    init_btnled();
    init_sd();
    init_ext4();
    init_net();
    init_wifi();

    usb_init_chip_id();     // can only read UID_BASE et al from M7

    btnled_setcolor(0x020202UL);

    elf_load_memory(&_binary__home_jncronin_src_gk_test_build_gk_test_bin_start);

    kernel_proc.name = "kernel";

    s.Schedule(Thread::Create("idle_cm7", idle_thread, (void*)0, true, 0, kernel_proc, CPUAffinity::M7Only,
        memblk_allocate_for_stack(512, CPUAffinity::M7Only)));
    s.Schedule(Thread::Create("idle_cm4", idle_thread, (void*)1, true, 0, kernel_proc, CPUAffinity::M4Only,
        memblk_allocate_for_stack(512, CPUAffinity::M4Only)));

    s.Schedule(Thread::Create("blue", bluescreen_thread, nullptr, true, 5, kernel_proc));
    s.Schedule(Thread::Create("b", b_thread, nullptr, true, 6, kernel_proc, CPUAffinity::Either, InvalidMemregion(),
        MPUGenerate(0xc0000000, 0x800000, 6, false, MemRegionAccess::RW, MemRegionAccess::NoAccess,
        WT_NS)));
    s.Schedule(Thread::Create("c", x_thread, (void *)'C', true, 5, kernel_proc));
    s.Schedule(Thread::Create("d", x_thread, (void *)'D', true, 5, kernel_proc));
    s.Schedule(Thread::Create("gpu", gpu_thread, nullptr, true, 9, kernel_proc));
    s.Schedule(Thread::Create("tusb", usb_task, nullptr, true, GK_NPRIORITIES - 1, kernel_proc));

    uint32_t myip = IP4Addr(192, 168, 7, 1).get();
    s.Schedule(Thread::Create("dhcpd", net_dhcpd_thread, (void *)myip, true, 5, kernel_proc));
    s.Schedule(Thread::Create("telnet", net_telnet_thread, nullptr, true, 5, kernel_proc));
    s.Schedule(Thread::Create("wifi", wifi_task, nullptr, true, 5, kernel_proc));

    // Nudge M4 to wakeup
#ifdef GK_DUAL_CORE
    __asm__ volatile ("sev \n" ::: "memory");
#endif

    // Prepare systick
    SysTick->CTRL = 0;
    SysTick->VAL = 0;
    SysTick->LOAD = 4799999;      // 10 ms tick at 48 MHz

    s.StartForCurrentCore();

    return 0;
}

extern "C" void CM7_SEV_IRQHandler()
{
    m4_wakeup = M4_MAGIC;
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
    SysTick->LOAD = 4799999;      // 10 ms tick at 48 MHz

    s.StartForCurrentCore();

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
    /*__syscall_SetFrameBuffer((void *)0xc0000000, (void *)0xc0200000, ARGB8888);
    for(int i = 0; i < 640*480; i++)
    {
        ((volatile uint32_t *)0xc0000000)[i] = 0xff0000ff;
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

    __syscall_SetFrameBuffer((void *)0xc0000000, (void *)0xc0200000, ARGB8888);
    RCC->AHB3ENR |= RCC_AHB3ENR_DMA2DEN;
    (void)RCC->AHB3ENR;

    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    (void)RCC->AHB2ENR;

    clock_set_cpu(clock_cpu_speed::cpu_384_192);

    uint32_t *backbuffer = (uint32_t *)0xc0400000;
    int cur_m = 320;
    int cur_y = 0;
    int cur_w = 80;

    RNG->CR = RNG_CR_RNGEN;

    // try and load mbr
    auto mbr = memblk_allocate(512, MemRegionType::AXISRAM);
    SetMPUForCurrentThread(MPUGenerate(mbr.address, mbr.length, 7, false,
        RW, NoAccess, WBWA_NS));
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

extern "C" void * _sbrk(int n)
{
    // shouldn't get here
    while(true);
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
    HardFault_Handler();
    while(true);
}

extern "C" void BusFault_Handler()
{
    HardFault_Handler();
    while(true);
}

extern "C" void UsageFault_Handler()
{
    HardFault_Handler();
    while(true);
}

extern "C" void SysTick_Handler()
{
    Yield();
}
