#include "stm32h7xx.h"
#include "syscalls_int.h"
#include "SEGGER_RTT.h"
#include "osmutex.h"
#include "memblk.h"
#include "gpu.h"
#include "cache.h"
#include <cstring>

extern Spinlock s_rtt;

SRAM4_DATA static BinarySemaphore sem_jpeg_done, sem_jpeg_hdr_done;
SRAM4_DATA static uint32_t __confr[8];

static uint32_t bndtr_prog(uint32_t size)
{
    size = (size + 0x7fffU) & ~0x7fffU;
    auto nblocks = size / 0x8000U;
    return (nblocks << 20) | 0x8000U;
}

int jpeg_decode(const void *src, size_t src_size,
    void *dest, size_t dest_size,
    uint32_t *confr)
{
    RCC->AHB3ENR |= RCC_AHB3ENR_MDMAEN;
    (void)RCC->AHB3ENR;

    // Set up MDMA channel 1 as memory read/JPEG input and 2 as memory write/JPEG output
    MDMA_Channel1->CTCR = (0UL << MDMA_CTCR_TRGM_Pos) |     // 1 buffer per trigger
        (3UL << MDMA_CTCR_TLEN_Pos) |                      // 16 bytes/buffer
        (2UL << MDMA_CTCR_SINCOS_Pos) |                     // 4 byte src increment
        (2UL << MDMA_CTCR_DSIZE_Pos) |
        (2UL << MDMA_CTCR_SSIZE_Pos) |
        (2U << MDMA_CTCR_SINC_Pos);                         // increment src
    MDMA_Channel1->CBNDTR = bndtr_prog(src_size);
    MDMA_Channel1->CSAR = (uint32_t)(uintptr_t)src;
    MDMA_Channel1->CDAR = (uint32_t)(uintptr_t)&JPEG->DIR;
    MDMA_Channel1->CMAR = 0;
    MDMA_Channel1->CIFCR = 0x1f;
    MDMA_Channel1->CTBR = 18U;                              // trigger
    MDMA_Channel1->CBRUR = 0;

    MDMA_Channel2->CTCR = (0UL << MDMA_CTCR_TRGM_Pos) |     // 1 buffer per trigger
        (3UL << MDMA_CTCR_TLEN_Pos) |                      // 16 bytes/buffer
        (2UL << MDMA_CTCR_DINCOS_Pos) |                     // 4 byte dest increment
        (2UL << MDMA_CTCR_DSIZE_Pos) |
        (2UL << MDMA_CTCR_SSIZE_Pos) |
        (2U << MDMA_CTCR_DINC_Pos);                         // increment dest
    MDMA_Channel2->CBNDTR = bndtr_prog(dest_size);
    MDMA_Channel2->CSAR = (uint32_t)(uintptr_t)&JPEG->DOR;
    MDMA_Channel2->CDAR = (uint32_t)(uintptr_t)dest;
    MDMA_Channel2->CMAR = 0;
    MDMA_Channel2->CIFCR = 0x1f;
    MDMA_Channel2->CTBR = 20U;                              // trigger
    MDMA_Channel2->CBRUR = 0;

    MDMA_Channel2->CCR = MDMA_CCR_EN;
    MDMA_Channel1->CCR = MDMA_CCR_EN;

    // Prepare JPEG
    RCC->AHB3ENR |= RCC_AHB3ENR_JPGDECEN;
    (void)RCC->AHB3ENR;

    JPEG->CR = JPEG_CR_JCEN;
    JPEG->CONFR1 = JPEG_CONFR1_DE |
        JPEG_CONFR1_HDR;

    JPEG->CR |= JPEG_CR_OFF;
    JPEG->CR |= JPEG_CR_IFF;

    JPEG->CR |= JPEG_CR_HPDIE | JPEG_CR_EOCIE;
    NVIC_EnableIRQ(JPEG_IRQn);

    JPEG->CONFR0 = JPEG_CONFR0_START;

    // Start MDMA channels
    MDMA_Channel1->CISR = MDMA_CISR_TEIF;
    MDMA_Channel2->CISR = MDMA_CISR_TEIF;
    MDMA->GISR0 |= (1U << 1) | (1U << 2);
    NVIC_EnableIRQ(MDMA_IRQn);

    MDMA_Channel2->CCR = MDMA_CCR_EN;
    MDMA_Channel1->CCR = MDMA_CCR_EN;

    // Await headers
    sem_jpeg_hdr_done.Wait();
    if(confr)
    {
        memcpy(confr, __confr, sizeof(__confr));
    }

    // Await completion
    sem_jpeg_done.Wait();

    return 0;
}

extern "C" void MDMA_IRQHandler()
{
    __asm__ volatile("bkpt \n" ::: "memory");
    while(true);
}

extern "C" void JPEG_IRQHandler()
{
    if(JPEG->SR & JPEG_SR_HPDF)
    {
        memcpy(__confr, (const void *)&JPEG->CONFR0, 8*sizeof(uint32_t));
        JPEG->CFR = JPEG_SR_HPDF;   // cmsis CFR flag is incorrect

        sem_jpeg_hdr_done.Signal();
    }
    if(JPEG->SR & JPEG_SR_EOCF)
    {
        // stop mdma channels and flush fifos
        MDMA_Channel1->CCR = 0;
        MDMA_Channel2->CCR = 0;
        JPEG->CR |= JPEG_CR_OFF;
        JPEG->CR |= JPEG_CR_IFF;

        JPEG->CFR = JPEG_SR_EOCF;
        
        // signal completion
        sem_jpeg_done.Signal();
    }
}

void jpeg_test()
{

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
    
    uint32_t confr[8];
    jpeg_decode((void *)mr_jpeg.address, br,
        (void *)mr_jpegout.address, mr_jpegout.length,
        confr);

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
        SEGGER_RTT_printf(0, "%d x %d\n", w, h);
    }

    GetCurrentThreadForCore()->p.screen_mode = GK_PIXELFORMAT_RGB565;
    CleanM7Cache(mr_jpegout.address, mr_jpegout.length, CacheType_t::Data);

    /* gpu_message gmsgs[3];
    gmsgs[0].type = CleanCache;
    gmsgs[0].dest_addr = mr_jpegout.address;
    gmsgs[0].dest_pf = 11;
    gmsgs[0].w = w;
    gmsgs[0].h = h;
    gmsgs[0].dx = 0;
    gmsgs[0].dy = 0;*/
#if 1
    gpu_message background_msg;

    background_msg.type = BlitImage;
    background_msg.dest_addr = 0;
    background_msg.dx = 0;
    background_msg.dy = 0;
    background_msg.src_addr_color = mr_jpegout.address;
    background_msg.sx = 0;
    background_msg.sy = 0;
    background_msg.src_pf = 11;
    background_msg.sp = 640*3;
    background_msg.w = w;
    background_msg.h = h;

    extern gpu_message gpu_clear_screen_msg;
    gpu_clear_screen_msg = background_msg;
#endif
    //gmsgs[2].type = FlipBuffers;

    //GPUEnqueueMessages(gmsgs, 3);

    
}