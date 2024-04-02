#include "stm32h7xx.h"
#include "syscalls_int.h"
#include "SEGGER_RTT.h"
#include "osmutex.h"
#include "memblk.h"
#include "gpu.h"

extern Spinlock s_rtt;

void jpeg_test()
{
    RCC->AHB3ENR |= RCC_AHB3ENR_JPGDECEN;
    (void)RCC->AHB3ENR;

    // load file
    auto f = deferred_call(syscall_open, "/testimg.jpg", O_RDONLY, 0);
    if(f < 0)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "couldn't open file\n");
        return;
    }

    auto mr_jpeg = memblk_allocate(1024 * 1024, MemRegionType::SDRAM);
    auto mr_jpegout = memblk_allocate(4096 * 1024, MemRegionType::SDRAM);
    if(!mr_jpeg.valid || !mr_jpegout.valid)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "out of memory\n");
        memblk_deallocate(mr_jpeg);
        memblk_deallocate(mr_jpegout);
        deferred_call(syscall_close1, f);
        deferred_call(syscall_close2, f);
        return;
    }
    auto br = deferred_call(syscall_read, f, (char *)mr_jpeg.address, 1024 * 1024);
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "jpeg read %d bytes\n", br);
    }
    deferred_call(syscall_close1, f);
    deferred_call(syscall_close2, f);

    if(br < 0)
        return;
    
    JPEG->CR = JPEG_CR_JCEN;
    JPEG->CONFR1 = JPEG_CONFR1_DE |
        JPEG_CONFR1_HDR;

    JPEG->CR |= JPEG_CR_OFF;
    JPEG->CR |= JPEG_CR_IFF;

    JPEG->CONFR0 = JPEG_CONFR0_START;
    int nr = 0;
    int nw = 0;
    uint32_t confr[8];
    while(true)
    {
        if(JPEG->SR & JPEG_SR_HPDF)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "header parsing done after %d bytes\n", nr);
            for(int i = 0; i < 8; i++)
            {
                confr[i] = (&JPEG->CONFR0)[i];
                SEGGER_RTT_printf(0, "CONFR%d = %x\n", i, confr[i]);
            }
            JPEG->CFR = (1UL << 6);
        }
        if(JPEG->SR & JPEG_SR_IFTF && nr < (br - 3))
        {
            for(int i = 0; i < 4; i++)
            {
                JPEG->DIR = *(uint32_t *)(mr_jpeg.address + nr);
                nr += 4;
            }
        }
        else if(JPEG->SR & JPEG_SR_IFNFF && nr < br)
        {
            JPEG->DIR = *(uint32_t *)(mr_jpeg.address + nr);
            nr += 4;
        }

        if(JPEG->SR & JPEG_SR_OFTF && nw < ((int)mr_jpegout.length - 3))
        {
            for(int i = 0; i < 4; i++)
            {
                *(uint32_t *)(mr_jpegout.address + nw) = JPEG->DOR;
                nw += 4;
            }
        }
        else if(JPEG->SR & JPEG_SR_OFNEF && nw < (int)mr_jpegout.length)
        {
            *(uint32_t *)(mr_jpegout.address + nw) = JPEG->DOR;
            nw += 4;
        }

        if(JPEG->SR & JPEG_SR_EOCF)
        {
            while(JPEG->SR & JPEG_SR_OFNEF && nw < (int)mr_jpegout.length)
            {
                *(uint32_t *)(mr_jpegout.address + nw) = JPEG->DOR;
                nw += 4;
            }
            break;
        }
    }

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "done\n");
    }
    memblk_deallocate(mr_jpeg);

    // check appropriate format: TODO: be more flexible
    if((confr[1] & JPEG_CONFR1_NF_Msk) != 2U)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "incorrect NF\n");
        return;
    }

    if((confr[1] & JPEG_CONFR1_COLORSPACE_Msk) != (1U << JPEG_CONFR1_COLORSPACE_Pos))
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "incorrect colorspace\n");
        return;
    }

    // get size
    auto h = confr[1] >> 16;
    auto w = confr[3] >> 16;

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "%d x %d, %d bytes\n", w, h, nw);
    }

    GetCurrentThreadForCore()->p.screen_mode = GK_PIXELFORMAT_RGB565;

    gpu_message gmsgs[3];
    gmsgs[0].type = CleanCache;
    gmsgs[0].dest_addr = mr_jpegout.address;
    gmsgs[0].dest_pf = 11;
    gmsgs[0].w = w;
    gmsgs[0].h = h;
    gmsgs[0].dx = 0;
    gmsgs[0].dy = 0;

    gmsgs[1].type = BlitImage;
    gmsgs[1].dest_addr = 0;
    gmsgs[1].dx = 0;
    gmsgs[1].dy = 0;
    gmsgs[1].src_addr_color = mr_jpegout.address;
    gmsgs[1].sx = 0;
    gmsgs[1].sy = 0;
    gmsgs[1].src_pf = 11;
    gmsgs[1].sp = 640*3;
    gmsgs[1].w = w;
    gmsgs[1].h = h;

    gmsgs[2].type = FlipBuffers;

    GPUEnqueueMessages(gmsgs, 3);

    
}