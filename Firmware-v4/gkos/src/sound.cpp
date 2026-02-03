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
#include "pmic.h"
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
static void dma_irqhandler(exception_regs *, uint64_t);

using audio_conf = Process::audio_conf_t;

/* I2C comms with TAD5112 */
static const constexpr unsigned int tad_addr = 0x50;
static const constexpr unsigned int tad_i2c = 2;
static void tad_reset();
static int tad_write(uint8_t reg, uint8_t val);
static uint8_t tad_read(uint8_t reg);

int volume_pct = 51;

/* We define 2 linked list DMA structures to provide a double buffer mode similar to older STMH7s */
struct ll_dma
{
    uint32_t sar;
    uint32_t next_ll;
};
__attribute__((aligned(CACHE_LINE_SIZE))) static ll_dma ll[8];

static_assert((sizeof(ll) & (CACHE_LINE_SIZE - 1)) == 0);

static constexpr uint32_t ll_addr(const void *next_ll)
{
    return (((uint32_t)(uintptr_t)next_ll) & 0xfffcU) |
        DMA_CLLR_USA | DMA_CLLR_ULL;
}

// buffer
constexpr unsigned int max_buffer_size = VBLOCK_64k;

static void _queue_if_possible(audio_conf &ac, uint64_t ttbr0);

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

static void pcm_mute_set(bool val)
{
    klog("sound: pcm_mute(%s)\n", val ? "true" : "false");
    if(!val)
    {
        // CH_EN
        tad_write(0x76, 0x0c);
    }
    else
    {
        tad_write(0x76, 0);
    }
}

[[maybe_unused]] static void *sound_thread(void *)
{
    // bring up VDD_AUDIO (TODO: could be selectively disabled)
    pmic_set_power(PMIC_Power_Target::Audio, 3300);

#if GK_SOUND_DUMP
    while(true)
    {
        auto DEV_MISC_CFG = tad_read(0x2);
        auto AVDD_IOVDD_STS = tad_read(0x3);
        auto CLK_ERR_STS0 = tad_read(0x3c);
        auto CLK_ERR_STS1 = tad_read(0x3d);
        auto CLK_DET_STS0 = tad_read(0x3e);
        auto CLK_DET_STS1 = tad_read(0x3f);
        auto CLK_DET_STS2 = tad_read(0x40);
        auto CLK_DET_STS3 = tad_read(0x41);
        auto DEV_STS0 = tad_read(0x79);
        auto DEV_STS1 = tad_read(0x7a);

        klog("sound: DEV_MISC_CFG: %x, AVDD_IOVDD_STS: %x, CLK_ERR_STS0: %x, CLK_ERR_STS1: %x\n",
            DEV_MISC_CFG, AVDD_IOVDD_STS, CLK_ERR_STS0, CLK_ERR_STS1);
        klog("sound: CLK_DET_STS0: %x, CLK_DET_STS1: %x, CLK_DET_STS2: %x, CLK_DET_STS3: %x\n",
            CLK_DET_STS0, CLK_DET_STS1, CLK_DET_STS2, CLK_DET_STS3);
        klog("sound: DEV_STS0: %x, DEV_STS1: %x\n",
            DEV_STS0, DEV_STS1);

        // dump all regs
        uint8_t p0[256];
        auto &i2c2 = i2c(tad_i2c);
        i2c2.RegisterRead(tad_addr, (uint8_t)0, p0, 256);
        for(unsigned int i = 0; i < 256; i += 4)
        {
            klog("sound: %2x: %02x, %2x: %02x, %2x: %02x, %2x: %02x\n",
                i, p0[i], i + 1, p0[i + 1], i + 2, p0[i + 2], i + 3, p0[i + 3]);
        }

        Block(clock_cur() + kernel_time_from_ms(1000));
    }
#endif

    return nullptr;
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

    // Enable clock to SAI2_VMEM
    sound_set_extfreq(44100.0);
    RCC_VMEM->SAI2CFGR |= RCC_SAI2CFGR_SAI2EN;
    RCC_VMEM->SAI2CFGR &= ~RCC_SAI2CFGR_SAI2RST;

    RCC_VMEM->HPDMA1CFGR |= RCC_HPDMA1CFGR_HPDMA1EN;
    RCC_VMEM->HPDMA1CFGR &= ~RCC_HPDMA1CFGR_HPDMA1RST;

    HPDMA1_VMEM->SECCFGR |= (1U << 8);
    HPDMA1_VMEM->PRIVCFGR |= (1U << 8);

    //sound_set_volume(50);

    Schedule(Thread::Create("sound", sound_thread, nullptr, true, GK_PRIORITY_VHIGH, p_kernel));
}

int sound_set_extfreq(double freq)
{
    /* We aim for a SCK of 64x FS freq (NODIV in SAI)
        Any given VCOout can be postdivided by up to /49 in the PLL,
            and then by /1,2,4 and then /1-/64 in the flexbar

        Assume a VCOout of 2 GHz

        Freq = 8000 Hz, SCK = 512 kHz = 2 GHz / 3906 = 2 GHz / (4/64 from flexbar) / 15 (from PLL)
        Freq = 96000 Hz, SCK = 6.144 MHz = 2 GHz / 325 = 2 GHz / (1/64 from flexbar) / 5 (from PLL)

        Therefore always divide /64 in flexbar findiv

        For the above examples, therefore, calculate a prediv value of 1, 2, or 4 such that PLL div
         is in [1,49], and ideally closest to 25
        
        Freq = 8000 Hz, SCK*64*25 = 819.2 MHz, Prediv = 2.4, round up to 4
        Freq = 96000 Hz, SCK*64*25 => Prediv = 0.2, round up to 1

        Then back-calculate the nearest PLL divider, then the actual VCOout

        Freq = 8000 Hz, 2 GHz / (SCK * 64 * 4) = 15.25
        
    */

    const double targ_vcoout = 2000000000.0;
    const int xbar_findiv_i = 64;
    auto sck = freq * (double)xbar_findiv_i;
    auto xbar_prediv = targ_vcoout / (sck * (double)xbar_findiv_i * 25.0);
    int xbar_prediv_i = 4;
    if(xbar_prediv <= 1.0)
        xbar_prediv_i = 1;
    else if(xbar_prediv <= 2.0)
        xbar_prediv_i = 2;
    
    double postdiv = targ_vcoout / (sck * (double)xbar_findiv_i * (double)xbar_prediv_i);

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
    double vcoout = sck * (double)pd.div1 * (double)pd.div2 * (double)xbar_prediv_i * (double)xbar_findiv_i;
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

    klog("audio: pll settings: frefdiv: 2, fbdiv: %d, frac: %d, postdiv1: %d, postdiv2: %d, prediv: %d, findiv: %d\n",
        intpart, fract_part, pd.div1, pd.div2, xbar_prediv_i, xbar_findiv_i);
    
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

    // Set up FLEXBAR[24] to use PLL7 / (1,2,4) / 64
    RCC_VMEM->FINDIVxCFGR[24] = 0;       // disable
    RCC_VMEM->PREDIVxCFGR[24] = xbar_prediv_i - 1;       
    RCC_VMEM->XBARxCFGR[24] = 0x43;      // enabled, pll7
    RCC_VMEM->FINDIVxCFGR[24] = 0x40 | (xbar_findiv_i - 1);    // enabled, div 64

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
    sound_set_extfreq((double)freq);
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
    tad_reset();
    pcm_mute_set(true);
    SPKR_NSD.clear();

    SAI2_Block_A_VMEM->CR1 = 0;
    dma->CCR |= DMA_CCR_SUSP;
    while(!(dma->CSR & DMA_CSR_IDLEF));
    dma->CCR = DMA_CCR_RESET;
    while(dma->CCR & DMA_CCR_RESET);
    dma->CCR = 0;

    /* Calculate PLL divisors */
    sound_set_extfreq((double)freq);

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
        (0UL << SAI_xCR1_MCKDIV_Pos) |
        SAI_xCR1_NODIV |
        (dsize << SAI_xCR1_DS_Pos) |
        mono |
        SAI_xCR1_DMAEN;
    SAI2_Block_A_VMEM->CR2 =
        (3UL << SAI_xCR2_FTH_Pos);
    SAI2_Block_A_VMEM->FRCR =
        SAI_xFRCR_FSOFF |
        SAI_xFRCR_FSDEF |
        (31UL << SAI_xFRCR_FSALL_Pos) |
        (63UL << SAI_xFRCR_FRL_Pos);

    SAI2_Block_A_VMEM->SLOTR =
        (1UL << SAI_xSLOTR_NBSLOT_Pos) |        // 2x 32-bit slots
        (3UL << SAI_xSLOTR_SLOTEN_Pos) |
        (2UL << SAI_xSLOTR_SLOTSZ_Pos);

    sound_set_volume(sound_get_volume());

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
            MutexGuard mg(p->user_mem->m);
            ac.mr_sound = p->user_mem->vblocks.AllocAny(
                MemBlock::ZeroBackedReadWriteMemory(0, VBLOCK_64k, true, false, 0, MT_NORMAL_WT));
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
            MutexGuard mg(p->user_mem->m);
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
    klog("ll[0]: { %x, %x }, ll[1]: { %x, %x }\n",
        ll[0].sar, ll[0].next_ll, ll[1].sar, ll[1].next_ll);

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

    gic_set_handler(dma_irq, dma_irqhandler);
    gic_set_target(dma_irq, GIC_ENABLED_CORES);
    gic_set_enable(dma_irq);

    {
        klog("audiosetmode: set %d hz, %d channels, %d bit, %u bytes/buffer, %u buffers @ %08x\n",
            freq, nchan, nbits, ac.buf_size_bytes, ac.nbuffers, ac.mr_sound.base);
    }
    
    return 0;
}

int syscall_audioenable(int enable, int *_errno)
{
    klog("audioenable: %d\n", enable);
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
        dma->CCR |= DMA_CCR_EN;
        SAI2_Block_A_VMEM->CR1 |= SAI_xCR1_SAIEN;

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

[[maybe_unused]] void _queue_if_possible(audio_conf &ac, uint64_t ttbr0)
{
    uint32_t next_buffer;
    if(ac.nbuffers == 1)
    {
        // continuous ring buffer
        next_buffer = (uint32_t)vmem_vaddr_to_paddr_quick(ac.mr_sound.base, ttbr0);
    }
    else if(ac.rd_ready_ptr != ac.wr_ptr)
    {
        // we can queue the next buffer
        next_buffer = (uint32_t)vmem_vaddr_to_paddr_quick(
            ac.mr_sound.base + ac.wr_ptr * ac.buf_size_bytes, ttbr0);
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
        *(volatile uint32_t *)target = next_buffer;
        __DMB();
        target = (dma->CLLR == ll[0].next_ll) ? &ll[1].sar : &ll[0].sar;
        CleanA35Cache((uintptr_t)ll, sizeof(ll), CacheType_t::Data, true);
        if(*(volatile uint32_t *)target == next_buffer) break;
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

void dma_irqhandler(exception_regs *, uint64_t)
{
    //klog("sound: dmairq, CSAR: %x, CLLR: %x\n", dma->CSAR, dma->CLLR);
    auto p = GetFocusProcess();
    if(p)
    {
        auto &ac = p->audio;

        CriticalGuard cg(ac.sl_sound);
#if GK_DUMP_AUDIO_PACKETS
        static kernel_time last_time;

        auto tdiff = clock_cur() - last_time;
        last_time = clock_cur();
        klog("sound: dma, tdiff: %u\n", (uint32_t)kernel_time_to_us(tdiff));
#endif
        if(!ac.mr_sound.valid)
        {
            dma->CFCR = DMA_CFCR_TCF;
            dma->CCR = 0;
            SAI2_Block_A_VMEM->CR1 = 0;
            SPKR_NSD.clear();
            klog("sound: dma, sound buffer not valid, disabling\n");
            return;
        }
        
        _queue_if_possible(ac, p->user_mem ? p->user_mem->ttbr0 : ~0ULL);

        if(_ptr_plus_one(ac.rd_ptr, ac) != ac.wr_ptr)
        {
            ac.waiting_threads.Signal();
        }
    }
    else
    {
        klog("sound: dma, no focus process, disabling\n");
        // disable audio
        SPKR_NSD.clear();
        //pcm_mute_set(true);   // don't run from irq handler
        SAI2_Block_A_VMEM->CR1 &= ~SAI_xCR1_SAIEN;
        dma->CCR &= ~DMA_CCR_EN;
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
        SPKR_NSD.set();
    }
    else
    {
        SPKR_NSD.clear();
    }

    // set volume on PCM
    tad_write(0x67, pcm1753_volume(new_vol_pct));
    tad_write(0x69, pcm1753_volume(new_vol_pct));

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

static int tad_write(uint8_t reg, uint8_t val)
{
    auto &i2c2 = i2c(tad_i2c);
    auto ret = i2c2.RegisterWrite(tad_addr, reg, &val, 1);
    if(ret != 1)
    {
        klog("sound: tad write failed %d\n", ret);
    }
    return ret;
}

static uint8_t tad_read(uint8_t reg)
{
    auto &i2c2 = i2c(tad_i2c);
    uint8_t rb;
    auto ret = i2c2.RegisterRead(tad_addr, reg, &rb, 1);
    if(ret != 1)
    {
        klog("sound: tad read failed %d\n", ret);
        return 0;
    }
    return rb;
}

void tad_reset()
{
    // set page 0
    /*
    tad_write(0, 0);
    tad_write(1, 1);
    Block(clock_cur() + kernel_time_from_ms(10));
    while(tad_read(1));
    */

    // exit sleep mode
    tad_write(0, 0);
    tad_write(2, 0xb);
    Block(clock_cur() + kernel_time_from_ms(2));

    // check register 0x6 - should reset to 0x35
    if(tad_read(0x6) != 0x35)
    {
        klog("sound: tad5112 did not reset\n");
        return;
    }

    // configure vddio to 1.8V
    tad_write(2, 0xb);

    // configure 32-bit i2s mode
    tad_write(0x1a, 0x70);

    // configure I2S left -> mixer CH1, I2S right -> mixer CH2
    tad_write(0x28, 0x20);
    tad_write(0x29, 0x30);

    // configure DAC1A -> left, DAC1B -> right
    tad_write(0x64, 0x24);

    // configure OUT1P and OUT1M as headphone driver
    tad_write(0x65, 0x60);
    tad_write(0x66, 0x60);

    // disable output
    tad_write(0x76, 0);

    // power up DAC channels.
    tad_write(0x78, 0x40);
}
