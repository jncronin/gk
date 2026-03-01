#include "gk_conf.h"
#include "syscalls_int.h"
#include "screen.h"
#include "pmem.h"
#include "util.h"
#include "vmem.h"
#include "process.h"
#include "process_interface.h"
#include "pins.h"
#include "cache.h"
#include <stm32mp2xx.h>
#include <atomic>
#include <cmath>

const unsigned int scr_w = GK_SCREEN_WIDTH;
const unsigned int scr_h = GK_SCREEN_HEIGHT;

#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define RIFSC_VMEM PMEM_TO_VMEM(RIFSC_BASE)
#define GPIOA_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOA_BASE))
#define GPIOB_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOB_BASE))
#define GPIOC_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE))
#define GPIOD_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOD_BASE))
#define GPIOE_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOE_BASE))
#define GPIOF_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOF_BASE))
#define GPIOG_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOG_BASE))
#define GPIOH_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOH_BASE))
#define GPIOI_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOI_BASE))
#define GPIOJ_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOJ_BASE))
#define LTDC_VMEM ((LTDC_TypeDef *)PMEM_TO_VMEM(LTDC_BASE))
#define LTDC_Layer1_VMEM ((LTDC_Layer_TypeDef *)PMEM_TO_VMEM(LTDC_Layer1_BASE))
#define LTDC_Layer2_VMEM ((LTDC_Layer_TypeDef *)PMEM_TO_VMEM(LTDC_Layer2_BASE))
#define LTDC_Layer3_VMEM ((LTDC_Layer_TypeDef *)PMEM_TO_VMEM(LTDC_Layer3_BASE))
#define SYSCFG_VMEM ((SYSCFG_TypeDef *)PMEM_TO_VMEM(SYSCFG_BASE))

static DMA_Channel_TypeDef *dmas[] =
{
    (DMA_Channel_TypeDef *)PMEM_TO_VMEM(HPDMA1_Channel12_BASE),
    (DMA_Channel_TypeDef *)PMEM_TO_VMEM(HPDMA1_Channel13_BASE)
};

#define HPDMA1_VMEM ((DMA_TypeDef *)PMEM_TO_VMEM(HPDMA1_BASE))
#define TIM2_VMEM ((TIM_TypeDef *)PMEM_TO_VMEM(TIM2_BASE))

static const constexpr pin PWM_BACKLIGHT { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOA_BASE), 4, 8 };
static const constexpr pin LS_OE_N { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE), 0 };
static const constexpr pin CTP_WAKE { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOA_BASE), 2 };

static const constexpr unsigned int arr = 4096;

volatile std::atomic_bool screen_flip_in_progress = false;
Condition scr_vsync;

static int brightness = 50;

class FPSCounter
{
    protected:
        Spinlock sl;
        kernel_time last_dump;
        unsigned int nframes = 0;
        int layer;
        double ffps;

    public:
        FPSCounter(int _layer) : layer(_layer) {}

        double GetFPS() const
        {
            return ffps;
        }

        void Tick()
        {
            CriticalGuard cg(sl);
            auto now = clock_cur();
            nframes++;

            auto tdiff = now - last_dump;

            if(tdiff >= kernel_time_from_ms(1000))
            {
                ffps = (double)nframes * 1000000.0 / (double)kernel_time_to_us(tdiff);
#if GK_GPU_SHOW_FPS
                auto fps = (unsigned long)ffps;
                klog("screen: layer %d: FPS: %lu, CPU: %u\n", layer, fps, (unsigned int)(sched.CPUUsage() * 100.0));
#endif
                nframes = 0;
                last_dump = now;
            }
        }
};

static FPSCounter fpsc[scr_n_layers]
{
    FPSCounter(0),
    FPSCounter(1)
};

class TripleBufferScreenLayer
{
    protected:
        Spinlock sl;
        PMemBlock pm[scr_n_bufs] = { InvalidPMemBlock(), InvalidPMemBlock(), InvalidPMemBlock() };
        // store the screen layouts of the last process to call screen_update()
        unsigned int pfs[scr_n_bufs] = { 0, 0, 0 };
        unsigned int lws[scr_n_bufs] = { GK_SCREEN_WIDTH, GK_SCREEN_WIDTH, GK_SCREEN_WIDTH };
        unsigned int lhs[scr_n_bufs] = { GK_SCREEN_HEIGHT, GK_SCREEN_HEIGHT, GK_SCREEN_HEIGHT };
        unsigned int lalphas[scr_n_bufs] = { 255, 255, 255 };
        uint32_t ckeys[scr_n_bufs] = { 0xfffffffU, 0xffffffffU, 0xffffffffU };
        pid_t pids[scr_n_bufs] = { 0, 0, 0 };

        // which buffer is being shown/queued/updated
        unsigned int cur_display = 0;
        unsigned int last_updated = 0;
        unsigned int cur_update = 0;

    public:
        struct layer_details
        {
            uintptr_t paddr;
            unsigned int pf, lw, lh, alpha;
            uint32_t ckey;
            bool new_clut;
            std::vector<uint32_t> clut;
        };

        unsigned int update(unsigned int alpha = 255);
        layer_details vsync();
        std::pair<unsigned int, unsigned int> current();

        friend void init_screen();
        friend PMemBlock screen_get_buf(unsigned int, unsigned int);
        friend void screen_clear_all_userspace();
};

static TripleBufferScreenLayer scrs[2] = { TripleBufferScreenLayer{}, TripleBufferScreenLayer{} };
VMemBlock l1_priv[3] = { InvalidVMemBlock(), InvalidVMemBlock(), InvalidVMemBlock() };

void init_screen()
{
    // Enable HPDMA1 channels 12 and 13
    RCC_VMEM->HPDMA1CFGR |= RCC_HPDMA1CFGR_HPDMA1EN;
    RCC_VMEM->HPDMA1CFGR &= ~RCC_HPDMA1CFGR_HPDMA1RST;

    HPDMA1_VMEM->SECCFGR |= (1U << 12) | (1U << 13);
    HPDMA1_VMEM->PRIVCFGR |= (1U << 12) | (1U << 13);

    // Enable backlight on TIM2 CH1 PA4 AF8
    RCC_VMEM->TIM2CFGR |= RCC_TIM2CFGR_TIM2EN;
    RCC_VMEM->TIM2CFGR &= ~RCC_TIM2CFGR_TIM2RST;
    __asm__ volatile("dsb sy\n" ::: "memory");

    PWM_BACKLIGHT.set_as_af();
    TIM2_VMEM->CCMR1 = (0UL << TIM_CCMR1_CC1S_Pos) |
        TIM_CCMR1_OC1PE |
        (6UL << TIM_CCMR1_OC1M_Pos);
    TIM2_VMEM->CCMR2 =0;
    TIM2_VMEM->CCER = TIM_CCER_CC1E;
    TIM2_VMEM->PSC = 5UL;    // (ck = ck/(1 + PSC))
    TIM2_VMEM->ARR = arr;
    TIM2_VMEM->CCR1 = 0UL;
    TIM2_VMEM->BDTR = TIM_BDTR_MOE;
    TIM2_VMEM->CR1 = TIM_CR1_CEN;

    // prepare screen backbuffers for layers 1 and 2
    for(unsigned int layer = 0; layer < 2; layer++)
    {
        for(unsigned int buf = 0; buf < 3; buf++)
        {
            scrs[layer].pm[buf] = Pmem.acquire(scr_layer_size_bytes);
            klog("screen: layer %u, buffer %u @ %llx\n", layer, buf, scrs[layer].pm[buf].base);

            // clear to zero
            if(scrs[layer].pm[buf].valid)
                memset((void *)PMEM_TO_VMEM(scrs[layer].pm[buf].base), 0, scr_layer_size_bytes);

            if(layer == 1)
            {
                // map for kernel
                l1_priv[buf] = vblock_alloc(vblock_size_for(scr_layer_size_bytes),
                    false, true, false);
                if(l1_priv[buf].valid == false)
                {
                    klog("screen: l1 unable to allocate kernel buffer\n");
                    while(true);
                }
                for(uintptr_t offset = 0; offset < scr_layer_size_bytes; offset += VBLOCK_64k)
                {
                    vmem_map(l1_priv[buf].data_start() + offset,
                        scrs[layer].pm[buf].base + offset, false, true, false, ~0ULL, ~0ULL,
                        nullptr, MT_NORMAL_WT);
                }
            }
        }
    }

    RCC_VMEM->GPIOICFGR |= RCC_GPIOICFGR_GPIOxEN;
    RCC_VMEM->GPIOJCFGR |= RCC_GPIOJCFGR_GPIOxEN;
    RCC_VMEM->GPIOGCFGR |= RCC_GPIOGCFGR_GPIOxEN;
    RCC_VMEM->GPIOACFGR |= RCC_GPIOACFGR_GPIOxEN;
    RCC_VMEM->GPIOBCFGR |= RCC_GPIOBCFGR_GPIOxEN;
    RCC_VMEM->GPIOCCFGR |= RCC_GPIOCCFGR_GPIOxEN;
    RCC_VMEM->LTDCCFGR |= RCC_LTDCCFGR_LTDCEN;
    __asm__ volatile("dsb sy\n");

    // turn on backlight
    screen_set_brightness(screen_get_brightness());

    // enable level shifter
    LS_OE_N.set_as_output();
    LS_OE_N.clear();

    // enable CTP
    CTP_WAKE.set_as_output();
    CTP_WAKE.set();

    // LTDC setup for 800x480 panel
    RCC_VMEM->LTDCCFGR |= RCC_LTDCCFGR_LTDCEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

#if 1
    /* Give LTDC access to DDR via RIFSC.  Use same CID as CA35/secure/priv for the master interface ID 1 */
    *(volatile uint32_t *)(RIFSC_VMEM + 0xc10 + 11 * 0x4) =
        (1UL << 2) |                // use cid specified here
        (1UL << 4) |                // CID 1
        (1UL << 8) |                // secure
        (1UL << 9);                 // priv
    *(volatile uint32_t *)(RIFSC_VMEM + 0xc10 + 12 * 0x4) =
        (1UL << 2) |                // use cid specified here
        (1UL << 4) |                // CID 1
        (1UL << 8) |                // secure
        (1UL << 9);                 // priv    
    /* The IDMA is forced to non-secure if its RISUP (RISUP 119/120) is programmed as non-secure,
        therefore set as secure here */
    {
        const uint32_t risup = 119;
        const uint32_t risup_word = risup / 32;
        const uint32_t risup_bit = risup % 32;
        auto risup_reg = (volatile uint32_t *)(RIFSC_VMEM + 0x10 + 0x4 * risup_word);
        auto old_val = *risup_reg;
        old_val |= 1U << risup_bit;
        *risup_reg = old_val;
        __asm__ volatile("dmb sy\n" ::: "memory");
    }
    {
        const uint32_t risup = 120;
        const uint32_t risup_word = risup / 32;
        const uint32_t risup_bit = risup % 32;
        auto risup_reg = (volatile uint32_t *)(RIFSC_VMEM + 0x10 + 0x4 * risup_word);
        auto old_val = *risup_reg;
        old_val |= 1U << risup_bit;
        *risup_reg = old_val;
        __asm__ volatile("dmb sy\n" ::: "memory");
    }
#endif

    /* LTDC pins */
    static const constexpr pin DISP { GPIOC_VMEM, 5 };         // DISP
    DISP.set_as_output();
    DISP.set();

    static const constexpr pin ltdc_pins[] = {
        { GPIOA_VMEM, 1, 11 },         // R3
        { GPIOB_VMEM, 15, 13 },        // R4
        { GPIOC_VMEM, 6, 14 },         // CLK
        { GPIOC_VMEM, 11, 13 },        // R2
        { GPIOG_VMEM, 1, 13 },         // VSYNC
        { GPIOG_VMEM, 3, 13 },         // R5
        { GPIOG_VMEM, 6, 13 },         // R6
        { GPIOG_VMEM, 7, 13 },         // R7
        { GPIOG_VMEM, 8, 13 },         // G2
        { GPIOG_VMEM, 9, 13 },         // G3
        { GPIOG_VMEM, 10, 13 },        // G4
        { GPIOG_VMEM, 11, 13 },        // G5
        { GPIOG_VMEM, 12, 13 },        // G6
        { GPIOG_VMEM, 13, 13 },        // G7
        { GPIOG_VMEM, 14, 13 },        // B1
        { GPIOG_VMEM, 15, 13 },        // B2
        { GPIOI_VMEM, 0, 13 },         // B3
        { GPIOI_VMEM, 1, 13 },         // B4
        { GPIOI_VMEM, 2, 13 },         // B5
        { GPIOI_VMEM, 3, 13 },         // B6
        { GPIOI_VMEM, 4, 13 },         // B7
        { GPIOI_VMEM, 5, 13 },         // DE
        { GPIOI_VMEM, 7, 13 },         // HSYNC
        { GPIOI_VMEM, 9, 13 },         // B0
        { GPIOI_VMEM, 12, 13 },        // G0
        { GPIOI_VMEM, 13, 13 },        // G1
        { GPIOJ_VMEM, 14, 13 },        // R0
        { GPIOJ_VMEM, 15, 13 },        // R1
    };
    for(const auto &p : ltdc_pins)
    {
        p.set_as_af();
    }

    /* Screen is 800x480
        ER settings:
        H visible 800
            back  140
            front 160
            spw   20
        V visible 480
            back  20
            front 12
            spw   3
        PCLK      falling
        HSYNC     high
        VSYNC     high
        DE        high
    
        1120 x 515 -> 34.608 MHz @ 60 Hz

        We want something easily divisble from 1200 MHz, so use:
        1250 x 640 -> 48 MHz @ 60 Hz
        So, e.g. H front 225, visible 800, back 200, spw 25
                 V front 80, visible 480, back 75, spw 5

        Need /25 divider from 1200 MHz PLL5 off HSE (could use PLL8 otherwise)
    */
    RCC_VMEM->FINDIVxCFGR[27] = 0;
    RCC_VMEM->PREDIVxCFGR[27] = 0;
    RCC_VMEM->XBARxCFGR[27] = 0x41;
    RCC_VMEM->FINDIVxCFGR[27] = 0x40U | 47U;

    // LTDC clock from above
    SYSCFG_VMEM->DISPLAYCLKCR = 0x2U;

    RCC_VMEM->LTDCCFGR &= ~RCC_LTDCCFGR_LTDCRST;
    __asm__ volatile("dsb sy\n" ::: "memory");

    LTDC_VMEM->SSCR = (3UL << LTDC_SSCR_VSH_Pos) |
        (3UL << LTDC_SSCR_HSW_Pos);
    LTDC_VMEM->BPCR = (19UL << LTDC_BPCR_AVBP_Pos) |
        (11UL << LTDC_BPCR_AHBP_Pos);
    LTDC_VMEM->AWCR = (499UL << LTDC_AWCR_AAH_Pos) |
        (811UL << LTDC_AWCR_AAW_Pos);
    LTDC_VMEM->TWCR = (511UL << LTDC_TWCR_TOTALH_Pos) |
        (815UL << LTDC_TWCR_TOTALW_Pos);
    
    LTDC_VMEM->GCR = LTDC_GCR_HSPOL |
        LTDC_GCR_VSPOL |
        LTDC_GCR_DEPOL |
        LTDC_GCR_PCPOL;
    
    LTDC_VMEM->BCCR = 0xff00ffUL;

    LTDC_Layer1_VMEM->CR = 0;
    LTDC_Layer2_VMEM->CR = 0;
    LTDC_Layer3_VMEM->CR = 0;

    LTDC_VMEM->SRCR = LTDC_SRCR_IMR;
    LTDC_VMEM->LIPCR = 511UL;
    LTDC_VMEM->IER = LTDC_IER_LIE | LTDC_IER_RRIE;
    LTDC_VMEM->GCR |= LTDC_GCR_LTDCEN;

    gic_set_target(190, GIC_ENABLED_CORES);
    gic_set_enable(190);
}

static uintptr_t screen_buf_to_vaddr(unsigned int layer, unsigned int buf)
{
    if(layer > 1)
        return 0;
    if(buf > 3)
        return 0;

    if(layer == 0)
    {
        // userspace mapping
        switch(buf)
        {
            case 0:
                return GK_SCR_L1_B0;
            case 1:
                return GK_SCR_L1_B1;
            case 2:
                return GK_SCR_L1_B2;
        }
    }
    else
    {
        return l1_priv[buf].data_start();
    }

    return 0;
}

std::pair<uintptr_t, uintptr_t> screen_current()
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    return _screen_current();
}

std::pair<uintptr_t, uintptr_t> _screen_current()
{
    auto p = GetCurrentProcessForCore();
    if(!p)
        return std::make_pair(0, 0);
    
    auto layer = p->screen.screen_layer;
    auto buf = scrs[layer].current();
    if(buf.first >= 3)
        return std::make_pair(0, 0);

    return std::make_pair(screen_buf_to_vaddr(layer, buf.first),
        screen_buf_to_vaddr(layer, buf.second));
}

uintptr_t screen_update()
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    auto layer = p->screen.screen_layer;
    auto buf = scrs[layer].update();
    fpsc[layer].Tick();
    return screen_buf_to_vaddr(layer, buf);
}

int syscall_screenflip(unsigned int layer, unsigned int alpha, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    if(layer >= 2)
    {
        *_errno = EINVAL;
        return -1;
    }
    if(layer == 1 && p->priv_overlay_fb == false)
    {
        *_errno = EPERM;
        return -1;
    }
    auto buf = scrs[layer].update(alpha);
    fpsc[layer].Tick();
    return buf;
}

PMemBlock screen_get_buf(unsigned int layer, unsigned int buf)
{
    if(layer >= 2 || buf >= 3)
        return InvalidPMemBlock();
    CriticalGuard cg(scrs[layer].sl);
    return scrs[layer].pm[buf];
}

unsigned int TripleBufferScreenLayer::update(unsigned int alpha)
{
    CriticalGuard cg(sl);
    last_updated = cur_update;

    auto p = GetCurrentProcessForCore();

    size_t copy_to_new = 0;
    size_t pitch = 0;
    size_t nlines = 0;
    unsigned int p_layer = 0;
    unsigned int update_type = 0;

    if(p)
    {
        pfs[last_updated] = p->screen.screen_pf;
        lws[last_updated] = p->screen.screen_w;
        lhs[last_updated] = p->screen.screen_h;
        pids[last_updated] = p->id;
        lalphas[last_updated] = alpha;
        ckeys[last_updated] = p->screen.color_key;
        
        if(p->screen.updates_each_frame != GK_SCREEN_UPDATE_FULL)
        {
            pitch = p->screen.screen_h * screen_get_bpp_for_pf(p->screen.screen_pf);
            nlines = p->screen.screen_w;
            copy_to_new = nlines * pitch;
            p_layer = p->screen.screen_layer;
            update_type = p->screen.updates_each_frame;
        }
    }

    // select a new screen to write to
    cur_update = 0;
    while(cur_update == last_updated || cur_update == cur_display)
        cur_update++;

    cg.unlock();

    /* Update the next screen with the current one for apps that only
        perform updates rather than full refreshes */
    if(update_type == GK_SCREEN_UPDATE_PARTIAL_READBACK)
    {
        // Readback updates need the data to be visible to the processor in cache
        //  Could either DMA and invalidate cache or just direct copy (we do the latter)
        auto from = screen_buf_to_vaddr(p_layer, last_updated);
        auto to = screen_buf_to_vaddr(p_layer, cur_update);
        memcpy((void *)to, (void *)from, copy_to_new);
    }
    else if(update_type == GK_SCREEN_UPDATE_PARTIAL_NOREADBACK)
    {
        auto dma = dmas[p_layer];

        // Use HPDMA so we return to the app quicker
        while(dma->CCR & DMA_CCR_EN);

        // decide on beat size - make it a factor of pitch
        [[maybe_unused]] size_t beat_size = 0;
        uint32_t beat_size_log2 = 0;
        if(pitch & 0x1U)
        {
            beat_size = 1;
            beat_size_log2 = 0;
        }
        else if(pitch & 0x2U)
        {
            beat_size = 2;
            beat_size_log2 = 1;
        }
        else if(pitch & 0x4U)
        {
            beat_size = 4;
            beat_size_log2 = 2;
        }
        else
        {
            beat_size = 8;
            beat_size_log2 = 3;
        }

        // program DMA
        dma->CTR1 = 
            DMA_CTR1_DSEC |
            (15U << DMA_CTR1_DBL_1_Pos) |
            DMA_CTR1_DINC |
            (beat_size_log2 << DMA_CTR1_DDW_LOG2_Pos) |
            DMA_CTR1_SSEC |
            (15U << DMA_CTR1_SBL_1_Pos) |
            DMA_CTR1_SINC |
            (beat_size_log2 << DMA_CTR1_SDW_LOG2_Pos);
        dma->CTR2 = (1U << DMA_CTR2_TRIGM_Pos) |
            DMA_CTR2_SWREQ;
        dma->CTR3 = 0U;
        dma->CBR1 = ((nlines - 1) << DMA_CBR1_BRC_Pos) |
            (pitch << DMA_CBR1_BNDT_Pos);
        dma->CBR2 = 0U;
        dma->CSAR = pm[last_updated].base;
        dma->CDAR = pm[cur_update].base;
        dma->CLLR = 0U;
        dma->CCR = DMA_CCR_EN;
    }

    return cur_update;
}

TripleBufferScreenLayer::layer_details TripleBufferScreenLayer::vsync()
{
    CriticalGuard cg(sl, ProcessList.sl);

    if(last_updated != cur_display)
    {
        cur_display = last_updated;
        auto p = (pids[last_updated] == 0) ? nullptr : ProcessList._get(pids[last_updated]).v;
        auto paddr = pm[last_updated].base;
        auto lw = lws[last_updated];
        auto lh = lhs[last_updated];
        auto pf = pfs[last_updated];
        auto alpha = lalphas[last_updated];
        auto ckey = ckeys[last_updated];

        cg.unlock();

        bool new_clut = false;
        std::vector<uint32_t> clut;

        if(p)
        {
            CriticalGuard cg2(true, p->screen.sl);
            if(cg2.IsLocked() && p->screen.new_clut)
            {
                p->screen.new_clut = false;
                new_clut = true;
                clut = std::move(p->screen.clut);
                p->screen.clut.clear();
            }
        }
        return { paddr, pf, lw, lh, alpha, ckey, new_clut, clut };
    }
    else
    {
        return { 0, 0, 0, 0, 0, 0xffffffffu, false, std::vector<uint32_t>() };
    }
}

std::pair<unsigned int, unsigned int> TripleBufferScreenLayer::current()
{
    CriticalGuard cg(sl);
    return std::make_pair(cur_update, last_updated);
}

void LTDC_IRQHandler()
{
    if(LTDC_VMEM->ISR & LTDC_ISR_LIF)
    {
        for(auto layer = 0U; layer < scr_n_layers; layer++)
        {
            auto ldetails = scrs[layer].vsync();
            if(ldetails.paddr)
            {
                auto lw = ldetails.lw;
                auto lh = ldetails.lh;
                auto lpf = ldetails.pf;
                auto lalpha = ldetails.alpha;
                auto ckey = ldetails.ckey;

                auto len = (lalpha > 0) ? LTDC_LxCR_LEN : 0U;
                auto cken = (ckey == 0xffffffffU) ? 0U : LTDC_LxCR_CKEN;

                auto l = (layer == 0) ? LTDC_Layer1_VMEM : LTDC_Layer2_VMEM;

                auto hstart = (((LTDC_VMEM->BPCR & LTDC_BPCR_AHBP_Msk) >> LTDC_BPCR_AHBP_Pos) + 1UL);
                auto vstart = (((LTDC_VMEM->BPCR & LTDC_BPCR_AVBP_Msk) >> LTDC_BPCR_AVBP_Pos) + 1UL);

                // size of the output window - automatically centred on screen.  768x480 is 16:10
                unsigned int disp_w = 800;
                unsigned int disp_h = 480;

                disp_w = std::min(disp_w, scr_w);
                disp_h = std::min(disp_h, scr_h);

                auto scen = ((disp_w > lw) || (disp_h > lh)) ? LTDC_LxCR_SCEN : 0;

                auto bpp = screen_get_bpp_for_pf(lpf);

                hstart += (scr_w - disp_w) / 2;
                vstart += (scr_h - disp_h) / 2;

                l->WHPCR = (hstart << LTDC_LxWHPCR_WHSTPOS_Pos) |
                    ((hstart + disp_w - 1) << LTDC_LxWHPCR_WHSPPOS_Pos);
                l->WVPCR = (vstart << LTDC_LxWVPCR_WVSTPOS_Pos) |
                    ((vstart + disp_h - 1) << LTDC_LxWVPCR_WVSPPOS_Pos);
                l->CKCR = ckey;
                
                uint32_t cluten = 0;
                if(lpf < 7)
                    l->PFCR = lpf;
                else
                {
                    l->PFCR = 7;

                    // flexible pixel format
                    switch(lpf)
                    {
                        case GK_PIXELFORMAT_L8:
                            l->FPF0R = (8U << 14);
                            l->FPF1R = (1U << 18) | (8U << 14) | (8U << 5);
                            cluten = LTDC_LxCR_CLUTEN;
                            break;
                        case GK_PIXELFORMAT_A4L4:
                            l->FPF0R = (4U << 14) | (4U << 5) | (4U << 0);
                            l->FPF1R = (1U << 18) | (4U << 14) | (4U << 5);
                            cluten = LTDC_LxCR_CLUTEN;
                            break;
                        case GK_PIXELFORMAT_ARGB4444:
                            l->FPF0R = (4U << 14) | (8U << 9) | (4U << 5) | (12U << 0);
                            l->FPF1R = (2U << 18) | (4U << 14) | (0U << 9) | (4U << 5) | (4U << 0);
                            break;
                        case GK_PIXELFORMAT_RGB565A8:
                            l->FPF0R = (5U << 14) | (11U << 9) | (8U << 5) | (16U << 0);
                            l->FPF1R = (3U << 18) | (5U << 14) | (0U << 9) | (6U << 5) | (5U << 0);
                            break;
                        default:
                            klog("screen: unsupported pixel format: %u\n", lpf);
                            break;
                    }
                }

                if(ldetails.new_clut)
                {
                    //klog("screen: new clut of size %u\n", ldetails.clut.size());
                    auto clut_size = std::min(256U, (unsigned int)ldetails.clut.size());
                    for(unsigned int i = 0; i < clut_size; i++)
                    {                        
                        l->CLUTWR = (ldetails.clut[i] & 0xffffffU) |
                            (i << 24);
                    }
                    // for AL44 we may need to double the lower nibble of the pix_id variable
                    if(clut_size <= 16U)
                    {
                        for(unsigned int i = 1; i < clut_size; i++)
                        {
                            auto pix_id = i | (i << 4);
                            l->CLUTWR = (ldetails.clut[i] & 0xffffffU) |
                                (pix_id << 24);
                        }
                    }
                }
                l->CACR = lalpha;
                l->DCCR = 0UL;
                l->BFCR = (layer == 0) ?
                    ((4U << LTDC_LxBFCR_BF1_Pos) | (5U << LTDC_LxBFCR_BF2_Pos)) // constant alpha for L0
                    :
                    ((1U << LTDC_LxBFCR_BOR_Pos) |
                        (6U << LTDC_LxBFCR_BF1_Pos) | (7UL << LTDC_LxBFCR_BF2_Pos)); // blend for L1
                l->CFBLR = ((lw * bpp) << LTDC_LxCFBLR_CFBP_Pos) |
                    ((lw * bpp + 7) << LTDC_LxCFBLR_CFBLL_Pos);
                l->CFBLNR = lh;
                l->CFBAR = (uint32_t)(uintptr_t)ldetails.paddr;
                l->CR = 0;

                if(scen)
                {
                    l->CR = 0 | scen;

                    l->SISR = (lw << LTDC_LxSISR_SIH_Pos) |
                        (lh << LTDC_LxSISR_SIV_Pos);
                    l->SOSR = (disp_w << LTDC_LxSOSR_SOH_Pos) |
                        (disp_h << LTDC_LxSOSR_SOV_Pos);
                    l->SHSFR = ((lw - 1) * 4096) / (disp_w - 1);
                    l->SVSFR = ((lh - 1) * 4096) / (disp_h - 1);
                    l->SHSPR = l->SHSFR + 4096;
                    l->SVSPR = l->SVSFR;
                    //l->SHSPR = 0;
                    //l->SVSPR = 0;
                }

                l->CR = len | scen | cluten | cken;

                screen_flip_in_progress = true;
                LTDC_VMEM->SRCR = LTDC_SRCR_IMR;
            }
        }
        
        scr_vsync.Signal();
    }

    if(LTDC_VMEM->ISR & LTDC_ISR_RRIF)
    {
        screen_flip_in_progress = false;
    }

    LTDC_VMEM->ICR = 0xf;
}


void screen_clear_all_userspace()
{
    for(unsigned int buf = 0; buf < scr_n_bufs; buf++)
    {
        // clear to zero
        if(scrs[0].pm[buf].valid)
            memset((void *)PMEM_TO_VMEM(scrs[0].pm[buf].base), 0, scr_layer_size_bytes);
    }
}

int screen_get_brightness()
{
    return brightness;
}

int screen_set_brightness(int bright)
{
    if(bright > 100) bright = 100;
    if(bright < 0) bright = 0;
    brightness = bright;

    // gamma function with minimum set to 2.5% of ARR
    const double minimum = 0.025;
    const double fsr = 1.0 - minimum;       // scale to this * ARR
    const double gamma = 2.2;

    double vin = (double)bright / 100.0;
    double vout = std::pow(vin, gamma);

    double vout_sc = vout * fsr + minimum;
    auto vout_sc_i = (unsigned int)std::round(vout_sc * (double)arr);

    TIM2_VMEM->CCR1 = vout_sc_i;

    return brightness;
}

double screen_get_fps(unsigned int layer)
{
    if(layer >= scr_n_layers)
        return 0.0;
    return fpsc[layer].GetFPS();
}
