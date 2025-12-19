#include <stm32mp2xx.h>
#include "sound.h"
#include "pins.h"
#include <math.h>
#include "osmutex.h"
#include <cstring>
#include "scheduler.h"
#include "process.h"
#include "i2c.h"
#include "cache.h"
#include "vmem.h"
#include "pmem.h"
#include "smc.h"
#include "bootinfo.h"

static constexpr const pin SAI2_SD_A { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOJ), 12, 3 };
static constexpr const pin SAI2_SCK_A { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOJ), 11, 3 };
static constexpr const pin SAI2_FS_A { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOG), 2, 4 };
static constexpr const pin SPKR_NSD { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOB), 7 };   // speaker amp enable

#define dma ((DMA_Channel_TypeDef *)PMEM_TO_VMEM(HPDMA1_Channel8_BASE))
#define SAI2_VMEM ((SAI_TypeDef *)PMEM_TO_VMEM(SAI2_BASE))
#define SAI2_Block_A_VMEM ((SAI_Block_TypeDef *)PMEM_TO_VMEM(SAI2_Block_A_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define HPDMA1_VMEM ((DMA_TypeDef *)PMEM_TO_VMEM(HPDMA1_BASE))

#define dma_irq 73

using audio_conf = Process::audio_conf_t;

int volume_pct = 51;

/* We define 2 linked list DMA structures to provide a double buffer mode similar to older STMH7s */
struct ll_dma
{
    uint32_t sar;
    uint32_t next_ll;
};
__attribute__((aligned(CACHE_LINE_SIZE))) static ll_dma ll[4];

static constexpr uint32_t ll_addr(const void *next_ll)
{
    return (((uint32_t)(uintptr_t)next_ll) & 0xfffcU) |
        DMA_CLLR_USA | DMA_CLLR_ULL;
}

// buffer
constexpr unsigned int max_buffer_size = VBLOCK_64k;

static void _queue_if_possible(audio_conf &ac);

[[maybe_unused]] static void _clear_buffers(audio_conf &ac)
{
    ac.wr_ptr = 0;
    ac.rd_ptr = 0;
    ac.rd_ready_ptr = 0;
    ac.enabled = false;
}

[[maybe_unused]] static unsigned int _ptr_plus_one(unsigned int i, audio_conf &ac)
{
    i++;
    if(i >= ac.nbuffers) i = 0;
    return i;
}

/* Return volume as expected to be written to one DAC channel */
static inline uint16_t pcm1753_volume(int volume)
{
    // Linear scale in top of range for now
    return volume ? (155U + volume) : 0U;
}

/* Return volume per channel */
static inline uint16_t pcm1753_volume_left(int volume)
{
    return 0x1000U | pcm1753_volume(volume);
}
static inline uint16_t pcm1753_volume_right(int volume)
{
    return 0x1100U | pcm1753_volume(volume);
}

static void pcm1753_write(const uint16_t *d, size_t n)
{
    klog("pcm1753_write: not impl\n");
}

static void pcm_mute_set(bool val)
{
    uint16_t pcmregs[] = {
        pcm1753_volume_left(val ? 0U : sound_get_volume()),
        pcm1753_volume_right(val ? 0U : sound_get_volume())
    };
    pcm1753_write(pcmregs, sizeof(pcmregs) / sizeof(uint16_t));
    klog("sound: pcm_mute(%s)\n", val ? "true" : "false");
}

void init_sound()
{
    klog("sound: init\n");
    // pins
    RCC_VMEM->GPIOBCFGR |= RCC_GPIOBCFGR_GPIOxEN;
    RCC_VMEM->GPIOGCFGR |= RCC_GPIOGCFGR_GPIOxEN;
    RCC_VMEM->GPIOJCFGR |= RCC_GPIOJCFGR_GPIOxEN;
    __asm__ volatile("dmb sy\n" ::: "memory");

    if(gbi.btype == gkos_boot_interface::board_type::GKV4)
    {
        SAI2_SCK_A.set_as_af();
        SAI2_SD_A.set_as_af();
        SAI2_FS_A.set_as_af();

        SPKR_NSD.clear();
        SPKR_NSD.set_as_output();
    }

    // bring up VDD_AUDIO (TODO: could be selectively disabled)
    smc_set_power(SMC_Power_Target::Audio, 3300);

    // Enable clock to SAI2_VMEM
    sound_set_extfreq(44100.0 * 1024.0);
    RCC_VMEM->SAI2CFGR |= RCC_SAI2CFGR_SAI2EN;
    RCC_VMEM->SAI2CFGR &= ~RCC_SAI2CFGR_SAI2RST;

    RCC_VMEM->HPDMA1CFGR |= RCC_HPDMA1CFGR_HPDMA1EN;
    RCC_VMEM->HPDMA1CFGR &= ~RCC_HPDMA1CFGR_HPDMA1RST;

    HPDMA1_VMEM->SECCFGR |= (1U << 8);
    HPDMA1_VMEM->PRIVCFGR |= (1U << 8);

    //sound_set_volume(50);
}

int sound_set_extfreq(double freq)
{
    /* We use PLL7 which has a fractional divider with 24-bit precision
        Input is 40 MHz HSE/2 = 20 MHz
        VCO output frequency is between 800 and 3200 MHz

        The postdivider is two /1 to /7 dividers so various steps up to /49

        We first aim for somewhere in the middle of the VCO output range with
         integer post-dividers (aim 2000 MHz)
        
        Note input frequencies of < 16000 kHz are too low to generate an
         appropriate divider with min VCO of 800 MHz, so we need to do more
         scaling in the flexbar

        For a flexbar scale of /4, and a PLL postdiv of /24, we can
         achieve FS frequencies of 8.1 to 32.5 kHz, so this is a reasonable
         ballpark if we then adjust PLL postdiv - this gives < 4kHz through 781 kHz
    */
    
    int sai_prescale = 4;
    freq = freq * (double)sai_prescale;

    const double targ_vcoout = 2000000000.0;
    double postdiv = targ_vcoout / freq;

    struct unique_postdivs
    {
        int div1, div2;
    };

    // valid postdivs = 1  2  3  4  5  6  7  8 10 12 14  9 15 18 21 16 20 24 28 25 30 35 36 42 49
    const unique_postdivs upds[] =
    {
        { 1, 1 }, { 1, 2 }, { 3, 1 }, { 2, 2 }, { 5, 1 }, { 3, 2 }, { 7, 1 },
        { 2, 4 }, { 5, 2 }, { 6, 2 }, { 7, 2 }, { 3, 3 }, { 3, 5 }, { 3, 6 },
        { 3, 7 }, { 4, 4 }, { 5, 4 }, { 6, 4 }, { 7, 4 }, { 5, 5 }, { 5, 6 },
        { 7, 5 }, { 6, 6 }, { 7, 6 }, { 7, 7 }
    };

    // get the multiplier larger than or equal to target
    unique_postdivs pd = { 0, 0 };
    for(const auto &upd : upds)
    {
        double test = (double)upd.div1 * (double)upd.div2;
        if(test >= postdiv)
        {
            pd = upd;
            break;
        }
    }
    if(pd.div1 == 0)
    {
        klog("audio: failed to calculate postdiv\n");
        return -1;
    }

    // now use those postdivs to get the actual vco output
    double vcoout = freq * (double)pd.div1 * (double)pd.div2;
    if(vcoout < 800000000.0 || vcoout > 3200000000.0)
    {
        klog("audio: incorrect vcoout freq\n");
        return -1;
    }

    // split vcoout to integer and fractional parts
    const double vcoin = 40000000.0 / 2.0;
    double vcomult = vcoout / vcoin;
    int intpart = (int)(vcomult);
    int fract_part = (int)((vcomult - (double)intpart) * 16777216.0);

    klog("audio: pll settings: frefdiv: 2, fbdiv: %d, frac: %d, postdiv1: %d, postdiv2: %d, sai_div: %d\n",
        intpart, fract_part, pd.div1, pd.div2, sai_prescale);
    
    // Now program the PLL7
    RCC_VMEM->PLL7CFGR1 &= ~RCC_PLL7CFGR1_PLLEN;
    __asm__ volatile("dmb sy\n" ::: "memory");

    // run off HSE
    RCC_VMEM->MUXSELCFGR = (RCC_VMEM->MUXSELCFGR & ~RCC_MUXSELCFGR_MUXSEL3_Msk) |
        (1U << RCC_MUXSELCFGR_MUXSEL3_Pos);

    RCC_VMEM->PLL7CFGR2 = (2U << RCC_PLL7CFGR2_FREFDIV_Pos) |
        ((unsigned int)intpart << RCC_PLL7CFGR2_FBDIV_Pos);
    RCC_VMEM->PLL7CFGR3 = (RCC_VMEM->PLL7CFGR3 & ~RCC_PLL7CFGR3_FRACIN_Msk) |
        ((unsigned int)fract_part << RCC_PLL7CFGR3_FRACIN_Pos);
    RCC_VMEM->PLL7CFGR4 = RCC_PLL7CFGR4_FOUTPOSTDIVEN |
        (((fract_part == 0) ? 0U : 1U) << RCC_PLL7CFGR4_DSMEN_Pos);
    RCC_VMEM->PLL7CFGR6 = (unsigned int)pd.div1 << RCC_PLL7CFGR6_POSTDIV1_Pos;
    RCC_VMEM->PLL7CFGR7 = (unsigned int)pd.div2 << RCC_PLL7CFGR7_POSTDIV2_Pos;
    __asm__ volatile("dmb sy\n" ::: "memory");
    RCC_VMEM->PLL7CFGR1 |= RCC_PLL7CFGR1_PLLEN;

    while(!(RCC_VMEM->PLL7CFGR1 & RCC_PLL7CFGR1_PLLRDY));

    // Set up FLEXBAR[24] to use PLL7 / 4
    RCC_VMEM->FINDIVxCFGR[24] = 0;       // disable
    RCC_VMEM->PREDIVxCFGR[24] = 3;       // div 4
    RCC_VMEM->XBARxCFGR[24] = 0x43;      // enabled, pll7
    RCC_VMEM->FINDIVxCFGR[24] = 0x40;    // enabled, div 1

    // Finally, output a test signal (FLEXBAR[24] output) on PF11 if on EV1
    if(gbi.btype == gkos_boot_interface::board_type::EV1)
    {
        RCC_VMEM->FCALCOBS0CFGR = RCC_FCALCOBS0CFGR_CKOBSEN |
            ((24U + 0xc0U) << RCC_FCALCOBS0CFGR_CKINTSEL_Pos);
    }

    return 0;
}

int syscall_audiosetfreq(int freq, int *_errno)
{
    sound_set_extfreq(1024.0 * (double)freq);
    return 0;
}

int syscall_audiosetmode(int nchan, int nbits, int freq, size_t buf_size_bytes,
    int *_errno)
{
    return syscall_audiosetmodeex(nchan, nbits, freq, buf_size_bytes, 0, _errno);
}

int syscall_audiosetmodeex(int nchan, int nbits, int freq, size_t buf_size_bytes, size_t nbufs,
    int *_errno)
{
    auto p = GetCurrentProcessForCore();
    auto &ac = p->audio;
    CriticalGuard cg(ac.sl_sound);

    /* this should be the first call from any process - stop sound output then reconfigure */
    pcm_mute_set(true);
    SPKR_NSD.clear();

    SAI2_Block_A_VMEM->CR1 = 0;
    dma->CCR |= DMA_CCR_SUSP;
    while(!(dma->CSR & DMA_CSR_IDLEF));
    dma->CCR = DMA_CCR_RESET;
    while(dma->CCR & DMA_CCR_RESET);
    dma->CCR = 0;

    /* Calculate PLL divisors */
    sound_set_extfreq(1024.0 * (double)freq);

    /* SAI2_VMEM is connected to TAD5112

        We program fixed 32 bits/channel
            thus 64 bits for an entire left/right sample

        It produces its own internal MCLK based upon FS and BCLK

        We provide a SAI clock of 1024 * FS then subdivide in SAI by 16 to get
         a BCLK/FS ratio of 64.


        I2S frame is left then right, left has FS low
        First 16 bits is data then zeros, can do this with the 'slot' mechanism of SAI
        - Slot1 = left data, Slot 2 = off
        - Slot3 = right data, Slot 4 = off
    */
    
    unsigned int dsize;
    unsigned int dw_log2;
    switch(nbits)
    {
        case 8:
            dsize = 2;
            dw_log2 = 0;
            break;
        case 16:
            dsize = 4;
            dw_log2 = 1;
            break;
        /*case 24:
            dsize = 6;
            break;*/
        case 32:
            dsize = 7;
            dw_log2 = 2;
            break;
        default:
            if(_errno) *_errno = EINVAL;
            return -1;
    }

    unsigned int mono = 0;
    switch(nchan)
    {
        case 1:
            mono = SAI_xCR1_MONO;
            break;
        case 2:
            mono = 0;
            break;
        default:
            if(_errno) *_errno = EINVAL;
            return -1;
    }

    SAI2_Block_A_VMEM->CR1 =
//        SAI_xCR1_MCKEN |
        SAI_xCR1_OSR |
        (2UL << SAI_xCR1_MCKDIV_Pos) |
        (dsize << SAI_xCR1_DS_Pos) |
        mono |
        SAI_xCR1_DMAEN;
    SAI2_Block_A_VMEM->CR2 =
        (0UL << SAI_xCR2_FTH_Pos);
    SAI2_Block_A_VMEM->FRCR =
        SAI_xFRCR_FSOFF |
        SAI_xFRCR_FSDEF |
        (31UL << SAI_xFRCR_FSALL_Pos) |
        (63UL << SAI_xFRCR_FRL_Pos);

    SAI2_Block_A_VMEM->SLOTR =
        (1UL << SAI_xSLOTR_NBSLOT_Pos) |        // 2x 32-bit slots
        (3UL << SAI_xSLOTR_SLOTEN_Pos) |
        (2UL << SAI_xSLOTR_SLOTSZ_Pos);

    // configure PCM1753
    uint16_t pcm1753_conf[] = {
        pcm1753_volume_left(sound_get_volume()),
        pcm1753_volume_right(sound_get_volume()),
        0x1300,         // enable both DACs
        0x1404,         // I2S format
        0x1604,         // single ZEROA pin
    };
    pcm1753_write(pcm1753_conf, sizeof(pcm1753_conf) / sizeof(uint16_t));

    /* Set up buffers */
    if(nbufs)
        ac.nbuffers = nbufs;
    else
        ac.nbuffers = max_buffer_size / buf_size_bytes - 1;

    /* Limit latency */
    int max_buffer_size_for_latency = ac.audio_max_buffer_size;
#if GK_AUDIO_LATENCY_LIMIT_MS > 0
    if(max_buffer_size_for_latency == 0 || max_buffer_size_for_latency > GK_AUDIO_LATENCY_LIMIT_MS)
        max_buffer_size_for_latency = GK_AUDIO_LATENCY_LIMIT_MS;
#endif

    if(max_buffer_size_for_latency)
    {
        auto buf_length_ms = (buf_size_bytes / (nbits/8) / nchan * 1000) / freq;
        auto buf_nlimit = (max_buffer_size_for_latency + buf_length_ms - 1) / buf_length_ms;
        if(buf_nlimit < 2) buf_nlimit = 2;
        if(buf_nlimit > ac.nbuffers) buf_nlimit = ac.nbuffers;

        ac.nbuffers = buf_nlimit;
    }

    ac.buf_size_bytes = buf_size_bytes;
    ac.buf_ndtr = buf_size_bytes * 8 / nbits;
    _clear_buffers(ac);
    ac.freq = (unsigned)freq;
    ac.nbits = nbits;
    ac.nchan = nchan;

    // special case selecting only one buffer
    if((ac.nbuffers == 0) || ((nbufs != 1) && (ac.nbuffers == 1)))
    {
        *_errno = EINVAL;
        return -1;
    }

    /* Get buffers */
    if(ac.mr_sound.valid == false)
    {
        if(p->user_mem)
        {
            CriticalGuard cg2(p->user_mem->sl);
            ac.mr_sound = vblock_alloc(VBLOCK_64k, true, true, false, 0, 0,
                p->user_mem->blocks);
        }
        else
        {
            ac.mr_sound = vblock_alloc(VBLOCK_64k, false, true, false);
        }
        if(!ac.mr_sound.valid)
        {
            klog("sound: couldn't allocate buffers\n");
            *_errno = ENOMEM;
            return -1;
        }
    }

    if(!ac.p_sound.valid)
    {
        ac.p_sound = Pmem.acquire(ac.mr_sound.length);
        if(!ac.p_sound.valid)
        {
            klog("sound: couldn't allocate pbuffer\n");
            *_errno = ENOMEM;
            return -1;
        }
        {
            CriticalGuard cg2(p->owned_pages.sl);
            p->owned_pages.add(ac.p_sound);
        }
        if(ac.mr_sound.base >= UH_START)
        {
            vmem_map(ac.mr_sound.base, ac.p_sound.base, false, true, false,
                ~0ULL, ~0ULL, nullptr, MT_NORMAL_WT);
        }
        else
        {
            CriticalGuard cg2(p->user_mem->sl);
            vmem_map(ac.mr_sound.base, ac.p_sound.base, true, true, false,
                p->user_mem->ttbr0 & 0xffffffff0000ULL, ~0ULL, nullptr, MT_NORMAL_WT);
        }
    }

    /* Set up silence buffer - last one */
    ac.silence_paddr = vmem_vaddr_to_paddr_quick(ac.mr_sound.base + buf_size_bytes * ac.nbuffers);
    memset((void *)ac.mr_sound.base, 0, buf_size_bytes * (ac.nbuffers + 1)); // zero all buffers including silence

    /* Set up DMA linked lists - always point back to each other */
    ll[0].sar = ac.silence_paddr;
    ll[0].next_ll = ll_addr(&ll[1]); // lower 16 bytes, set ULL, set USA
    ll[1].sar = ac.silence_paddr;
    ll[1].next_ll = ll_addr(&ll[0]); // lower 16 bytes, set ULL, set USA
    CleanA35Cache((uintptr_t)ll, sizeof(ll), CacheType_t::Data, true);

    /* Prepare DMA for running */
    dma->CCR = 
        DMA_CCR_TCIE;
    dma->CTR1 = DMA_CTR1_DAP |
        DMA_CTR1_DSEC |
        (0U << DMA_CTR1_DBL_1_Pos) |
        (dw_log2 << DMA_CTR1_DDW_LOG2_Pos) |
        DMA_CTR1_SSEC |
        (0U << DMA_CTR1_SBL_1_Pos) |
        DMA_CTR1_SINC |
        (dw_log2 << DMA_CTR1_SDW_LOG2_Pos);
    dma->CTR2 = (2U << DMA_CTR2_TCEM_Pos) |
        DMA_CTR2_DREQ |
        (75U << DMA_CTR2_REQSEL_Pos);           // SAI2_A_DMA
    dma->CTR3 = 0U;
    dma->CBR1 = ac.buf_size_bytes;
    dma->CBR2 = 0;
    dma->CSAR = (uint32_t)(uintptr_t)ll[0].sar;
    dma->CDAR = (uint32_t)(uintptr_t)&SAI2_Block_A->DR;
    dma->CLBAR = (uint32_t)(vmem_vaddr_to_paddr_quick((uintptr_t)&ll[0]) & 0xffff0000U);
    dma->CLLR = ll[0].next_ll;

    gic_set_enable(dma_irq);
    gic_set_target(dma_irq, GIC_ENABLED_CORES);

    {
        klog("audiosetmode: set %d hz, %d channels, %d bit, %u bytes/buffer, %u buffers @ %08x\n",
            freq, nchan, nbits, ac.buf_size_bytes, ac.nbuffers, ac.mr_sound.base);
    }
    
    return 0;
}

int syscall_audioenable(int enable, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    auto &ac = p->audio;
    if(!ac.mr_sound.valid)
    {
        *_errno = EINVAL;
        return -1;
    }

    CriticalGuard cg(ac.sl_sound);
    if(enable && !ac.enabled)
    {
        //_queue_if_possible();
        //dma->CCR |= DMA_CCR_EN;
        SAI2_Block_A_VMEM->CR1 |= SAI_xCR1_SAIEN;
        {
            DisableInterrupts();

            int nsent = 0;
            while(true)
            {
                while(((SAI2_Block_A_VMEM->SR & SAI_xSR_FLVL_Msk) >> SAI_xSR_FLVL_Pos) == 5U);
                SAI2_Block_A_VMEM->DR = nsent;
                nsent++;

                if(nsent % 5)
                    klog("sound: sent: %d\n", nsent);
            }
        }
        pcm_mute_set(false);
        SPKR_NSD.set();

        // attenuates 0.5 dB for every 8/fs seconds
        unsigned int us_delay = (8000000U * 128U) / ac.freq;
        Block(clock_cur() + kernel_time_from_us(us_delay));
    }
    else if(!enable && ac.enabled)
    {
        SPKR_NSD.clear();
        pcm_mute_set(true);
        SAI2_Block_A_VMEM->CR1 &= ~SAI_xCR1_SAIEN;
        dma->CCR &= ~DMA_CCR_EN;

        // attenuates 0.5 dB for every 8/fs seconds
        unsigned int us_delay = (8000000U * 128U) / ac.freq;
        Block(clock_cur() + kernel_time_from_us(us_delay));
    }
    ac.enabled = enable;
    return 0;
}

[[maybe_unused]] void _queue_if_possible(audio_conf &ac)
{
    uint32_t next_buffer;
    if(ac.nbuffers == 1)
    {
        // continuous ring buffer
        next_buffer = (uint32_t)vmem_vaddr_to_paddr_quick(ac.mr_sound.base);
    }
    else if(ac.rd_ready_ptr != ac.wr_ptr)
    {
        // we can queue the next buffer
        next_buffer = (uint32_t)vmem_vaddr_to_paddr_quick(ac.mr_sound.base + ac.wr_ptr * ac.buf_size_bytes);
        ac.wr_ptr = _ptr_plus_one(ac.wr_ptr, ac);
    }
    else
    {
        // queue silence
        next_buffer = (uint32_t)ac.silence_paddr;
    }

    // handle the unlikely condition that CT changed whilst we were writing
    while(true)
    {
        auto target = (dma->CLLR == ll[0].next_ll) ? &ll[1].sar : &ll[0].sar;
        *(volatile void **)target = (void*)next_buffer;
        __DMB();
        target = (dma->CLLR == ll[0].next_ll) ? &ll[1].sar : &ll[0].sar;
        CleanA35Cache((uint32_t)(uintptr_t)ll, sizeof(ll), CacheType_t::Data, true);
        if(*(volatile void **)target == (void*)next_buffer) break;
    }
}

int syscall_audioqueuebuffer(const void *buffer, void **next_buffer, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    auto &ac = p->audio;
    if(!ac.mr_sound.valid)
    {
        *_errno = EINVAL;
        return -1;
    }

    CriticalGuard cg(ac.sl_sound);
    if(buffer)
    {
        if(!next_buffer)
        {
            // cannot allow rd_ptr ready to get ahead of rd_ready_ptr
            *_errno = EINVAL;
            return -1;
        }
        ac.rd_ready_ptr = _ptr_plus_one(ac.rd_ready_ptr, ac);        
    }
    if(next_buffer)
    {
        if(ac.nbuffers == 1)
        {
            *next_buffer = (void *)ac.mr_sound.base;
        }
        else
        {
            if(_ptr_plus_one(ac.rd_ptr, ac) == ac.wr_ptr)
            {
                // buffers full
                *_errno = EBUSY;
                return -3;
            }
            *next_buffer = (void *)(ac.mr_sound.base + ac.rd_ptr * ac.buf_size_bytes);
            ac.rd_ptr = _ptr_plus_one(ac.rd_ptr, ac);
        }
    }
    return 0;
}

int syscall_audiowaitfree(int *_errno)
{
    auto p = GetCurrentProcessForCore();
    auto &ac = p->audio;
    if(!ac.mr_sound.valid)
    {
        *_errno = EINVAL;
        return -1;
    }

    CriticalGuard cg(ac.sl_sound);
    if(_ptr_plus_one(ac.rd_ptr, ac) == ac.wr_ptr)
    {
        ac.waiting_threads.Wait();
    }
    return 0;
}

extern "C" void dma_irqhandler()
{
    auto p = GetFocusProcess();
    auto &ac = p->audio;

    CriticalGuard cg(ac.sl_sound);
    //static kernel_time last_time;

    //auto tdiff = clock_cur() - last_time;
    //last_time = clock_cur();
    //klog("sound: dma, tdiff: %u\n", (uint32_t)tdiff.to_us());
    if(!ac.mr_sound.valid)
    {
        dma->CFCR = DMA_CFCR_TCF;
        dma->CCR = 0;
        SAI2_Block_A_VMEM->CR1 = 0;
        return;
    }
    
    _queue_if_possible(ac);

    if(_ptr_plus_one(ac.rd_ptr, ac) != ac.wr_ptr)
    {
        ac.waiting_threads.Signal();
    }

    dma->CFCR = DMA_CFCR_TCF;
    __DMB();
}

int sound_set_volume(int new_vol_pct)
{
    if(new_vol_pct < 0 || new_vol_pct > 100)
        return -1;
    
    volume_pct = new_vol_pct + 1;   // set one more so we can detect 0 = invalid value

    const bool pcmz = false;

    if(!pcmz && volume_pct)
    {
        pcm_mute_set(false);
        SPKR_NSD.set();
    }
    else
    {
        pcm_mute_set(true);
        SPKR_NSD.clear();
    }

    // set volume on PCM
    uint16_t pcmregs[] = {
        pcm1753_volume_left(new_vol_pct),
        pcm1753_volume_right(new_vol_pct),
    };
    klog("sound: set volume val: %d, regs %x, %x, zero=%d\n",
        new_vol_pct,
        pcmregs[0], pcmregs[1],
        pcmz ? 1 : 0);
    pcm1753_write(pcmregs, sizeof(pcmregs) / sizeof(uint16_t));

    return 0;
}

int sound_get_volume()
{
    // 0 = invalid therefore volume_pct in [1,101]
    return (volume_pct > 0 && volume_pct <= 101) ? (volume_pct - 1) : 50;
}

static const constexpr uint8_t cdce_address = 0x65U;

int syscall_audiogetbufferpos(size_t *nbufs, size_t *curbuf, size_t *buflen, size_t *bufpos,
    int *nchan, int *nbits, int *freq, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    auto &ac = p->audio;

    if(!ac.mr_sound.valid)
    {
        *_errno = EINVAL;
        return -1;
    }

    CriticalGuard cg(ac.sl_sound);
    if(nbufs) *nbufs = ac.nbuffers;
    if(curbuf) *curbuf = ((uintptr_t)dma->CSAR - vmem_vaddr_to_paddr_quick(ac.mr_sound.base)) / ac.buf_size_bytes;
    if(buflen) *buflen = ac.buf_size_bytes;
    if(bufpos) *bufpos = ac.buf_size_bytes - (dma->CBR1 & DMA_CBR1_BNDT_Msk);
    if(nchan) *nchan = ac.nchan;
    if(nbits) *nbits = ac.nbits;
    if(freq) *freq = ac.freq;

    return 0;
}
