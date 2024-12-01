#include "stm32h7rsxx.h"
#include "sound.h"
#include "pins.h"
#include <math.h>
#include "osmutex.h"
#include <cstring>
#include "scheduler.h"
#include "SEGGER_RTT.h"
#include "process.h"
#include "i2c.h"
#include "cache.h"

#define USE_PCM_ZERO 0

static constexpr const pin SAI1_SCK_A { GPIOE, 5, 6 };
static constexpr const pin SAI1_SD_A { GPIOE, 6, 6 };
static constexpr const pin SAI1_FS_A { GPIOE, 4, 6 };
static constexpr const pin SAI1_MCLK_A { GPIOE, 2, 6 };
static constexpr const pin PCM_ZERO { GPIOB, 0 };   // input
static constexpr const pin SPKR_NSD { GPIOB, 7 };   // speaker amp enable
static constexpr const pin PCM_MUTE { GPIOE, 7 };   // headphone amp #enable

static constexpr const pin SPI2_MOSI { GPIOC, 1, 5 };
static constexpr const pin SPI2_NSS { GPIOB, 9, 5 };
static constexpr const pin SPI2_SCK { GPIOD, 3, 5 };

static constexpr const pin I2S_CKIN { GPIOC, 9, 5 };

#define dma GPDMA1_Channel12
#define dma_irq GPDMA1_Channel12_IRQn
#define dma_irqhandler GPDMA1_Channel12_IRQHandler

RTCREG_DATA int volume_pct;

/* We define 2 linked list DMA structures to provide a double buffer mode similar to older STMH7s */
struct ll_dma
{
    void *sar;
    uint32_t next_ll;
};
__attribute__((aligned(32))) static ll_dma ll[4];   // make 32 bytes so 1 cache line

static constexpr uint32_t ll_addr(const void *next_ll)
{
    return (((uint32_t)(uintptr_t)next_ll) & 0xfffcU) |
        DMA_CLLR_USA | DMA_CLLR_ULL;
}

// buffer
constexpr unsigned int max_buffer_size = 32*1024;

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

static void pcm1753_write(const uint16_t *d, size_t n)
{
    SPI2->CR2 = n;
    SPI2->CR1 = SPI_CR1_SPE;
    SPI2->CR1 = SPI_CR1_SPE | SPI_CR1_CSTART;
    while(n)
    {
        while(!(SPI2->SR & SPI_SR_TXP));
        *(volatile uint16_t *)&SPI2->TXDR = *d++;
        n--;
    }
    while(!(SPI2->SR & SPI_SR_TXTF));
    SPI2->IFCR = SPI_IFCR_TXTFC;
    while(!(SPI2->SR & SPI_SR_EOT));
    SPI2->IFCR = SPI_IFCR_EOTC;
    SPI2->CR1 = 0;
}

static void pcm_mute_set(bool val)
{
    uint16_t pcmreg = val ? 0x1203U : 0x1200U;
    pcm1753_write(&pcmreg, 1);
}

void init_sound()
{
    // pins
    SAI1_SCK_A.set_as_af();
    SAI1_SD_A.set_as_af();
    SAI1_FS_A.set_as_af();
    SAI1_MCLK_A.set_as_af();
    I2S_CKIN.set_as_af();

    SPKR_NSD.clear();
    SPKR_NSD.set_as_output();
    PCM_MUTE.set();
    PCM_MUTE.set_as_output();

#if USE_PCM_ZERO
    PCM_ZERO.set_as_input();
#endif

    SPI2_MOSI.set_as_af();
    SPI2_SCK.set_as_af();
    SPI2_NSS.set_as_af();

    // Enable clock to SAI1
    //sound_set_extfreq(44100.0 * 1024.0);
    RCC->CCIPR3 = (RCC->CCIPR3 & ~RCC_CCIPR3_SAI1SEL_Msk) |
        (3U << RCC_CCIPR3_SAI1SEL_Pos);
    
    // Enable SPI2 control to PCM1753
    RCC->APB1ENR1 |= RCC_APB1ENR1_SPI2EN;
    (void)RCC->APB1ENR1;
    SPI2->CR1 = 0;

    // Input clock is 240 MHz, output max 10 MHz, no MISO, raise NSS between each 16-bit word
    SPI2->CR2 = 0;
    SPI2->CFG1 = (7U << SPI_CFG1_MBR_Pos) |     // baud /256
        // TODO TXDMAEN
        (15U << SPI_CFG1_DSIZE_Pos) |           // 16-bit frame
        0;
    SPI2->CFG2 = SPI_CFG2_AFCNTR |              // always control the SPI data lines even with SPE=0
        SPI_CFG2_SSOM |                         // interleave frames with NSS high
        SPI_CFG2_SSOE |                         // enable NSS control
        SPI_CFG2_MASTER |
        (1U << SPI_CFG2_COMM_Pos) |             // simplex transmitter
        (0xfU << SPI_CFG2_MIDI_Pos) |           // longest possible NSS interleave
        0;

    // configure PCM1753
    uint16_t pcm1753_conf[] = {
        0x1300,         // enable both DACs
        0x1404,         // I2S format
        0x1606,         // single ZEROA pin
    };
    pcm1753_write(pcm1753_conf, sizeof(pcm1753_conf) / sizeof(uint16_t));

#if USE_PCM_ZERO
    // Set PCM_ZERO IRQ, both edges
    RCC->APB4ENR |= RCC_APB4ENR_SBSEN;
    (void)RCC->APB4ENR;
    SBS->EXTICR[0] &= ~SBS_EXTICR1_PC_EXTI0_Msk;
    SBS->EXTICR[0] |= (1U << SBS_EXTICR1_PC_EXTI0_Pos);

    EXTI->RTSR1 |= EXTI_RTSR1_RT0;
    EXTI->FTSR1 |= EXTI_FTSR1_FT0;
    EXTI->IMR1 |= EXTI_IMR1_IM0;

    NVIC_EnableIRQ(EXTI0_IRQn);
#endif

    RCC->APB2ENR |= RCC_APB2ENR_SAI1EN;
    (void)RCC->APB2ENR;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPDMA1EN;
    (void)RCC->AHB1ENR;

    sound_set_volume(50);
}

int syscall_audiosetfreq(int freq, int *_errno)
{
    //sound_set_extfreq(1024.0 * (double)freq);
    return 0;
}

int syscall_audiosetmode(int nchan, int nbits, int freq, size_t buf_size_bytes, int *_errno)
{    
    auto &ac = GetCurrentThreadForCore()->p.audio;
    CriticalGuard cg(ac.sl_sound);

    /* this should be the first call from any process - stop sound output then reconfigure */
    pcm_mute_set(true);
    SPKR_NSD.clear();
    PCM_MUTE.set();
    SAI1_Block_A->CR1 = 0;
    dma->CCR |= DMA_CCR_SUSP;
    while(!(dma->CSR & DMA_CSR_IDLEF));
    dma->CCR = DMA_CCR_RESET;
    while(dma->CCR & DMA_CCR_RESET);
    dma->CCR = 0;

    /* Calculate PLL divisors */
    sound_set_extfreq(1024.0 * (double)freq);

    /* SAI1 is connected to PCM1754 

        This expects 32 bits/channel (we only use 16 of them) 
            thus 64 bits for an entire left/right sample

        To produce accurate MCLK, we need SAI clock of 1024 * Fs (where Fs = e.g. 48kHz)


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

    SAI1_Block_A->CR1 =
        SAI_xCR1_MCKEN |
        SAI_xCR1_OSR |
        (2UL << SAI_xCR1_MCKDIV_Pos) |
        (dsize << SAI_xCR1_DS_Pos) |
        mono |
        SAI_xCR1_DMAEN;
    SAI1_Block_A->CR2 =
        (0UL << SAI_xCR2_FTH_Pos);
    SAI1_Block_A->FRCR =
        SAI_xFRCR_FSOFF |
        SAI_xFRCR_FSDEF |
        (31UL << SAI_xFRCR_FSALL_Pos) |
        (63UL << SAI_xFRCR_FRL_Pos);

    SAI1_Block_A->SLOTR =
        (1UL << SAI_xSLOTR_NBSLOT_Pos) |        // 2x 32-bit slots
        (3UL << SAI_xSLOTR_SLOTEN_Pos) |
        (2UL << SAI_xSLOTR_SLOTSZ_Pos);

    // configure PCM1753
    // set volume on PCM, scaled from 128 (muted) to 255 (full)
    //  for now just linear in top part of range
    auto sound_vol = sound_get_volume();
    uint16_t val = sound_vol ? (155U + sound_vol) : 0U;
    uint16_t pcm1753_conf[] = {
        (uint16_t)(0x1000U | val),
        (uint16_t)(0x1100U | val),
        0x1300,         // enable both DACs
        0x1404,         // I2S format
        0x1604,         // single ZEROA pin
    };
    pcm1753_write(pcm1753_conf, sizeof(pcm1753_conf) / sizeof(uint16_t));

    /* Set up buffers */
    ac.nbuffers = max_buffer_size / buf_size_bytes - 1;
    ac.buf_size_bytes = buf_size_bytes;
    ac.buf_ndtr = buf_size_bytes * 8 / nbits;
    _clear_buffers(ac);

    if(ac.nbuffers < 2)
    {
        *_errno = EINVAL;
        return -1;
    }

    /* Get buffers */
    MemRegion syscall_memalloc_int(size_t len, int is_sync, int allow_sram,
        const std::string &usage, int *_errno);

    int mr_sound_errno;
    ac.mr_sound = syscall_memalloc_int(max_buffer_size, 1, 1, "sound buffer", &mr_sound_errno);

    if(!ac.mr_sound.valid)
    {
        klog("sound: couldn't allocate buffers: %d\n", mr_sound_errno);
        *_errno = ENOMEM;
        return -1;
    }

    /* Set up silence buffer - last one */
    ac.silence = (uint32_t *)(ac.mr_sound.address + buf_size_bytes * ac.nbuffers);
    memset(ac.silence, 0, buf_size_bytes);

    /* Set up DMA linked lists - always point back to each other */
    ll[0].sar = ac.silence;
    ll[0].next_ll = ll_addr(&ll[1]); // lower 16 bytes, set ULL, set USA
    ll[1].sar = ac.silence;
    ll[1].next_ll = ll_addr(&ll[0]); // lower 16 bytes, set ULL, set USA
    CleanM7Cache((uint32_t)(uintptr_t)ll, sizeof(ll), CacheType_t::Data);

    /* Prepare DMA for running */
    dma->CCR = DMA_CCR_LAP |
        DMA_CCR_TCIE;
    dma->CTR1 = DMA_CTR1_DAP |
        (0U << DMA_CTR1_DBL_1_Pos) |
        (dw_log2 << DMA_CTR1_DDW_LOG2_Pos) |
        (0U << DMA_CTR1_SBL_1_Pos) |
        DMA_CTR1_SINC |
        (dw_log2 << DMA_CTR1_SDW_LOG2_Pos);
    dma->CTR2 = (2U << DMA_CTR2_TCEM_Pos) |
        DMA_CTR2_DREQ |
        (63U << DMA_CTR2_REQSEL_Pos);
    dma->CTR3 = 0U;
    dma->CBR1 = ac.buf_size_bytes;
    dma->CBR2 = 0;
    dma->CSAR = (uint32_t)(uintptr_t)ll[0].sar;
    dma->CDAR = (uint32_t)(uintptr_t)&SAI1_Block_A->DR;
    dma->CLBAR = ((uint32_t)(uintptr_t)&ll[0]) & 0xffff0000U;
    dma->CLLR = ll[0].next_ll;

    NVIC_EnableIRQ(dma_irq);

    {
        klog("audiosetmode: set %d hz, %d channels, %d bit, %u bytes/buffer, %u buffers\n",
            freq, nchan, nbits, ac.buf_size_bytes, ac.nbuffers);
    }
    
    return 0;
}

int syscall_audioenable(int enable, int *_errno)
{
    auto &ac = GetCurrentThreadForCore()->p.audio;
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
        SAI1_Block_A->CR1 |= SAI_xCR1_SAIEN;
        pcm_mute_set(false);
        SPKR_NSD.set();
        PCM_MUTE.clear();
    }
    else if(ac.enabled)
    {
        PCM_MUTE.set();
        SPKR_NSD.clear();
        pcm_mute_set(true);
        SAI1_Block_A->CR1 &= ~SAI_xCR1_SAIEN;
        dma->CCR &= ~DMA_CCR_EN;
    }
    ac.enabled = enable;
    return 0;
}

[[maybe_unused]] void _queue_if_possible(audio_conf &ac)
{
    uint32_t next_buffer;
    if(ac.rd_ready_ptr != ac.wr_ptr)
    {
        // we can queue the next buffer
        next_buffer = ac.mr_sound.address + ac.wr_ptr * ac.buf_size_bytes;
        ac.wr_ptr = _ptr_plus_one(ac.wr_ptr, ac);
    }
    else
    {
        // queue silence
        next_buffer = (uint32_t)ac.silence;
    }

    // handle the unlikely condition that CT changed whilst we were writing
    while(true)
    {
        auto target = (dma->CLLR == ll[0].next_ll) ? &ll[1].sar : &ll[0].sar;
        *(volatile void **)target = (void*)next_buffer;
        __DMB();
        target = (dma->CLLR == ll[0].next_ll) ? &ll[1].sar : &ll[0].sar;
        CleanM7Cache((uint32_t)(uintptr_t)ll, sizeof(ll), CacheType_t::Data);
        if(*(volatile void **)target == (void*)next_buffer) break;
    }
}

int syscall_audioqueuebuffer(const void *buffer, void **next_buffer, int *_errno)
{
    auto &ac = GetCurrentThreadForCore()->p.audio;
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
        if(_ptr_plus_one(ac.rd_ptr, ac) == ac.wr_ptr)
        {
            // buffers full
            *_errno = EBUSY;
            return -3;
        }
        *next_buffer = (void *)(ac.mr_sound.address + ac.rd_ptr * ac.buf_size_bytes);
        ac.rd_ptr = _ptr_plus_one(ac.rd_ptr, ac);
    }
    return 0;
}

int syscall_audiowaitfree(int *_errno)
{
    auto &ac = GetCurrentThreadForCore()->p.audio;
    if(!ac.mr_sound.valid)
    {
        *_errno = EINVAL;
        return -1;
    }

    CriticalGuard cg(ac.sl_sound);
    if(_ptr_plus_one(ac.rd_ptr, ac) == ac.wr_ptr)
    {
        ac.waiting_thread = GetCurrentThreadForCore();
        //klog("sound: thread sleep\n");
        return -2;
    }
    else
    {
        ac.waiting_thread = nullptr;
        return 0;
    }
    return 0;
}

extern "C" void dma_irqhandler()
{
    auto &ac = focus_process->audio;

    CriticalGuard cg(ac.sl_sound);
    //static kernel_time last_time;

    //auto tdiff = clock_cur() - last_time;
    //last_time = clock_cur();
    //klog("sound: dma, tdiff: %u\n", (uint32_t)tdiff.to_us());
    if(!ac.mr_sound.valid)
    {
        dma->CFCR = DMA_CFCR_TCF;
        dma->CCR = 0;
        SAI1_Block_A->CR1 = 0;
        return;
    }
    
    _queue_if_possible(ac);

    if(ac.waiting_thread && _ptr_plus_one(ac.rd_ptr, ac) != ac.wr_ptr)
    {
        ac.waiting_thread->ss_p.ival1 = 0;
        ac.waiting_thread->ss.Signal();
        ac.waiting_thread = nullptr;

        //klog("sound: thread wake\n");
    }

    dma->CFCR = DMA_CFCR_TCF;
    __DMB();
}

#if USE_PCM_ZERO
extern "C" void EXTI0_IRQHandler()
{
    //CriticalGuard cg(sl_sound);
    auto v = PCM_ZERO.value();
    if(!v && volume_pct)
    {
        pcm_mute_set(false);
        SPKR_NSD.set();
        PCM_MUTE.clear();
    }
    else
    {
        pcm_mute_set(true);
        SPKR_NSD.clear();
        PCM_MUTE.set();
    }
    EXTI->PR1 = EXTI_PR1_PR0;
    __DMB();
}
#endif

int sound_set_volume(int new_vol_pct)
{
    if(new_vol_pct < 0 || new_vol_pct > 100)
        return -1;
    
    volume_pct = new_vol_pct + 1;   // set one more so we can detect 0 = invalid value

#if USE_PCM_ZERO
    bool pcmz = PCM_ZERO.value();
#else
    const bool pcmz = false;
#endif

    if(!pcmz && volume_pct)
    {
        pcm_mute_set(false);
        SPKR_NSD.set();
        PCM_MUTE.clear();
    }
    else
    {
        pcm_mute_set(true);
        SPKR_NSD.clear();
        PCM_MUTE.set();
    }

    // set volume on PCM, scaled from 128 (muted) to 255 (full)
    //  for now just linear in top part of range
    uint16_t val = new_vol_pct ? (155U + new_vol_pct) : 0U;
    uint16_t pcmregs[] = {
        (uint16_t)(0x1000U | val),
        (uint16_t)(0x1100U | val)
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

static bool cdce_init()
{
    static bool cdce_inited = false;
    if(cdce_inited)
        return true;
    
    uint8_t cdce_id = 0;
    i2c_register_read(cdce_address, (uint8_t)0x80, &cdce_id, 1);
    klog("sound: freq generator devid: %x\n", cdce_id);

    if((cdce_id & 0x87U) == 0x81U)
    {
        /* successfully found a CDCE913
            This is connected to an extenal 27 MHz 12 pF crystal

            Default has S2/S1 = 0 (used as SDA/SCL) and S0 pulled high on the board

            Thus, from table 9-2:
                Y1 output = Y1_1        = state 1   = 11b   = Y1 enabled
                Freq = FS1_1            = 0b        = fVCO1_0
                SSC = SSC1_1            = 000b      = off
                Y2/3 output = Y2Y3_1    = state 1   = 11b   = Y2/Y3 enabled

            Register IDs are OR'd with 0x80 to get single register read/write

        */

        // program for 12pF exernal crystal
        uint8_t xtal_load = 12U << 3;
        i2c_register_write(cdce_address, (uint8_t)0x85U, &xtal_load, 1);

        // disable Y2/Y3
        uint8_t y2y3_ostate = 0U;
        i2c_register_write(cdce_address, (uint8_t)0x95U, &y2y3_ostate, 1);
        
        cdce_inited = true;
    }

    return cdce_inited;
}

struct pll_divider
{
    unsigned int M, N, Pdiv;
};

static pll_divider calc_pll(double freq)
{
    /*
        fOUT = fVCO / Pdiv
        fVCO = fIN * N / M

        fVCO in [80-230] MHz
        M in [1-511]
        N in [1-4095]
        Pdiv in [1-127]

        Assume max freq = 96kHz*1024 = 98MHz-> Pdiv2

        For Pdiv2, we can go down to e.g. fVCO of 100 MHz, so 50MHz target
        Pdiv4 down to 25 MHz
        Pdiv8 down to 12.5 MHz
        Pdiv16 down to 6.25 MHz which is sufficient for audio frequencies of 6.1 kHZ
        */

    unsigned int P = 32;
    if(freq >= 50000000.0)
        P = 2;
    else if(freq >= 25000000.0)
        P = 4;
    else if(freq >= 12500000.0)
        P = 8;
    else if(freq >= 6250000.0)
        P = 16;
    
    double fvco = freq * (double)P;
    const double fin = 27000000.0;

    // try all values of M [1-511] that keep N < 4095 and produce the closest falue to fvco
    double min_error = fvco;
    unsigned int best_M = 0;
    unsigned int best_N = 0;

    for(unsigned int M = 1; M <= 511; M++)
    {
        // N = fVCO * M / fIN
        unsigned int N = (unsigned int)std::round(fvco * (double)M / fin);
        if(N < 1 || N > 4095)
            continue;

        double act_fvco = fin * (double)N / (double)M;
        auto cur_error = std::abs(act_fvco - fvco);

        if(cur_error < min_error)
        {
            min_error = cur_error;
            best_M = M;
            best_N = N;
        }
    }

    return pll_divider { .M = best_M, .N = best_N, .Pdiv = P };
}

int sound_set_extfreq(double freq)
{
    cdce_init();

    auto fvals = calc_pll(freq);
    klog("sound: M: %u, N: %u, Pdiv: %u\n", fvals.M, fvals.N, fvals.Pdiv);

    // disable PLL1 - don't wait for i2c completion in this function because we may
    //  be called from an uninterruptible syscall, therefore also use
    //  static vars so they don't get destroyed with stack change prior to use by i2c thread
    static uint8_t pllcfg = 0xed;
    i2c_register_write(cdce_address, (uint8_t)0x94U, &pllcfg, 1, false);

    // Program output divider first
    static uint8_t pdiv[2];
    pdiv[0] = ((fvals.Pdiv >> 8) & 0x3U) | 0xb4U;       // default value for upper bits
    pdiv[1] = fvals.Pdiv & 0xffU;

    i2c_register_write(cdce_address, (uint8_t)0x82U, &pdiv[0], 1, false);
    i2c_register_write(cdce_address, (uint8_t)0x83U, &pdiv[1], 1, false);

    // Need to calculate N, R, Q, P values
    auto P = 4 - (int)(log2((double)fvals.N / (double)fvals.M));
    if(P < 0) P = 0;
    auto Np = fvals.N * (1U << P);
    auto Q = (unsigned int)((double)Np / (double)fvals.M);
    auto R = Np - fvals.M * Q;

    klog("sound: P: %u, Q: %u, R: %u, N: %u\n", (unsigned)P, Q, R, fvals.N);

    auto fvco = 27000000.0 * (double)fvals.N / (double)fvals.M;
    unsigned int vco_range;
    if(fvco >= 175000000.0)
        vco_range = 3U;
    else if(fvco >= 150000000.0)
        vco_range = 2U;
    else if(fvco >= 125000000.0)
        vco_range = 1U;
    else
        vco_range = 0;
    
    static uint8_t pll[4];
    pll[0] = (fvals.N >> 4) & 0xffU;
    pll[1] = ((fvals.N & 0xfU) << 4) |
        ((R >> 5) & 0xfU);
    pll[2] = ((R & 0x1fU) << 3) |
        ((Q >> 3) & 0x7U);
    pll[3] = ((Q & 0x7U) << 5) |
        ((P & 0x7U) << 2) |
        vco_range;

    i2c_register_write(cdce_address, (uint8_t)0x98U, &pll[0], 1, false);
    i2c_register_write(cdce_address, (uint8_t)0x99U, &pll[1], 1, false);
    i2c_register_write(cdce_address, (uint8_t)0x9aU, &pll[2], 1, false);
    i2c_register_write(cdce_address, (uint8_t)0x9bU, &pll[3], 1, false);

    // enable PLL1
    static uint8_t pllcfg2 = 0x6d;
    i2c_register_write(cdce_address, (uint8_t)0x94U, &pllcfg2, 1, false);

    return 0;
}
