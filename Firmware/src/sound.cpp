#include "stm32h7xx.h"
#include "sound.h"
#include "pins.h"
#include <math.h>
#include "osmutex.h"
#include <cstring>
#include "scheduler.h"
#include "SEGGER_RTT.h"

static constexpr const pin SAI1_SCK_A { GPIOE, 5, 6 };
static constexpr const pin SAI1_SD_A { GPIOC, 1, 6 };
static constexpr const pin SAI1_FS_A { GPIOE, 4, 6 };
static constexpr const pin SAI1_MCLK_A { GPIOE, 2, 6 };
static constexpr const pin PCM_FMT { GPIOI, 4 };
static constexpr const pin PCM_DEMP { GPIOI, 5 };
static constexpr const pin PCM_MUTE { GPIOI, 6 };
static constexpr const pin PCM_ZERO { GPIOB, 2 };   // input
static constexpr const pin SPKR_NSD { GPIOI, 8 };

RTCREG_DATA int volume_pct;

// buffer
struct audio_conf
{
    unsigned int nbuffers;
    unsigned int buf_size_bytes;
    unsigned int buf_ndtr;
    unsigned int wr_ptr;
    unsigned int rd_ptr;
    unsigned int rd_ready_ptr;
    uint32_t *silence;
    Thread *waiting_thread;
    bool enabled;
};
SRAM4_DATA MemRegion mr_sound;
constexpr unsigned int max_buffer_size = 32*1024;
static SRAM4_DATA Spinlock sl_sound;
static SRAM4_DATA audio_conf ac;

extern Spinlock s_rtt;

static void _queue_if_possible();

static void _clear_buffers()
{
    ac.wr_ptr = 0;
    ac.rd_ptr = 0;
    ac.rd_ready_ptr = 0;
    ac.enabled = false;
}

static unsigned int _ptr_plus_one(unsigned int i)
{
    i++;
    if(i >= ac.nbuffers) i = 0;
    return i;
}

void init_sound()
{
    // pins
    SAI1_SCK_A.set_as_af();
    SAI1_SD_A.set_as_af();
    SAI1_FS_A.set_as_af();
    SAI1_MCLK_A.set_as_af();

    SPKR_NSD.clear();
    SPKR_NSD.set_as_output();

    PCM_FMT.clear();
    PCM_FMT.set_as_output();

    PCM_DEMP.clear();
    PCM_DEMP.set_as_output();

    PCM_MUTE.set();
    PCM_MUTE.set_as_output();

    PCM_ZERO.set_as_input();

    // Set PCM_ZERO IRQ, both edges
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;
    SYSCFG->EXTICR[0] &= SYSCFG_EXTICR1_EXTI2_Msk;
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI2_PB;

    EXTI->RTSR1 |= EXTI_RTSR1_TR2;
    EXTI->FTSR1 |= EXTI_FTSR1_TR2;
    EXTI->IMR1 |= EXTI_IMR1_IM2;

    NVIC_EnableIRQ(EXTI2_IRQn);

    mr_sound = memblk_allocate(max_buffer_size, MemRegionType::SRAM, "sound buffer");
    if(!mr_sound.valid)
        mr_sound = memblk_allocate(max_buffer_size, MemRegionType::AXISRAM, "sound buffer");
    if(!mr_sound.valid)
        mr_sound = memblk_allocate(max_buffer_size, MemRegionType::SDRAM, "sound buffer");

    RCC->APB2ENR |= RCC_APB2ENR_SAI1EN;
    (void)RCC->APB2ENR;

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    (void)RCC->AHB1ENR;
}

struct pll_setup
{
    unsigned int mult;
    unsigned int mult_frac;
    unsigned int div;
};

constexpr pll_setup pll_multiplier(int freq)
{
    // frac is out of 8192
    switch(freq)
    {
        case 48000:
            return { 43, 65, 7 };
        case 24000:
            return { 43, 65, 14 };
        case 12000:
            return { 43, 65, 28 };
        case 8000:
            return { 40, 7864, 40 };
        case 16000:
            return { 40, 7864, 20 };
        case 32000:
            return { 40, 7864, 10 };
        case 44100:
            return { 39, 4208, 7 };
        case 22050:
            return { 39, 4208, 14 };
        case 11025:
            return { 39, 4208, 28 };
    }

    return { 0, 0, 0 };
}

int syscall_audiosetmode(int nchan, int nbits, int freq, size_t buf_size_bytes, int *_errno)
{
    CriticalGuard cg(sl_sound);

    /* this should be the first call from any process - stop sound output then reconfigure */
    PCM_MUTE.set();
    SPKR_NSD.clear();
    SAI1_Block_A->CR1 = 0;
    DMA1_Stream0->CR = 0;

    /* Calculate PLL divisors */
    const auto mult = pll_multiplier(freq);
    if(mult.mult == 0)
    {
        if(_errno) *_errno = EINVAL;
        return -1;
    }

    RCC->CR &= ~RCC_CR_PLL2ON;
    __DMB();
    RCC->PLL2FRACR = mult.mult_frac;
    RCC->PLL2DIVR = (1UL << RCC_PLL2DIVR_R2_Pos) |
        (0UL << RCC_PLL2DIVR_Q2_Pos) |
        ((mult.div - 1) << RCC_PLL2DIVR_P2_Pos) |
        ((mult.mult - 1) << RCC_PLL2DIVR_N2_Pos);
    RCC->CR |= RCC_CR_PLL2ON;
    while(!(RCC->CR & RCC_CR_PLL2RDY));

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
    switch(nbits)
    {
        case 8:
            dsize = 2;
            break;
        case 16:
            dsize = 4;
            break;
        case 24:
            dsize = 6;
            break;
        case 32:
            dsize = 7;
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
        (2UL << SAI_xCR2_FTH_Pos);
    SAI1_Block_A->FRCR =
        SAI_xFRCR_FSOFF |
        SAI_xFRCR_FSDEF |
        (31UL << SAI_xFRCR_FSALL_Pos) |
        (63UL << SAI_xFRCR_FRL_Pos);

    SAI1_Block_A->SLOTR =
        (1UL << SAI_xSLOTR_NBSLOT_Pos) |        // 2x 32-bit slots
        (3UL << SAI_xSLOTR_SLOTEN_Pos) |
        (2UL << SAI_xSLOTR_SLOTSZ_Pos);

    /* Set up buffers */
    ac.nbuffers = max_buffer_size / buf_size_bytes - 1;
    ac.buf_size_bytes = buf_size_bytes;
    ac.buf_ndtr = buf_size_bytes * 8 / nbits;
    _clear_buffers();

    if(ac.nbuffers < 2)
    {
        *_errno = EINVAL;
        return -1;
    }

    /* Set up silence buffer - last one */
    ac.silence = (uint32_t *)(mr_sound.address + buf_size_bytes * ac.nbuffers);
    memset(ac.silence, 0, buf_size_bytes);

    /* Prepare DMA for running */
    DMA1_Stream0->CR = (1UL << DMA_SxCR_MSIZE_Pos) |
        (1UL << DMA_SxCR_PSIZE_Pos) |
        DMA_SxCR_MINC |
        DMA_SxCR_DBM | 
        (1UL << DMA_SxCR_DIR_Pos) |
        DMA_SxCR_TCIE;
    DMA1_Stream0->NDTR = ac.buf_ndtr;
    DMA1_Stream0->M0AR = (uint32_t)(uintptr_t)ac.silence;
    DMA1_Stream0->M1AR = (uint32_t)(uintptr_t)ac.silence;
    DMA1_Stream0->PAR = (uint32_t)(uintptr_t)&SAI1_Block_A->DR;
    //DMA1_Stream0->FCR

    // DMAMUX1 request mux input 87
    DMAMUX1_Channel0->CCR = 87UL;

    NVIC_EnableIRQ(DMA1_Stream0_IRQn);

    {
        CriticalGuard cg2(s_rtt);
        klog("audiosetmode: set %d hz, %d channels, %d bit\n",
            freq, nchan, nbits);
    }
    
    return 0;
}

int syscall_audioenable(int enable, int *_errno)
{
    CriticalGuard cg(sl_sound);
    if(enable && !ac.enabled)
    {
        //_queue_if_possible();
        DMA1_Stream0->CR |= DMA_SxCR_EN;
        SAI1_Block_A->CR1 |= SAI_xCR1_SAIEN;
        PCM_MUTE.clear();
        SPKR_NSD.set();
    }
    else if(ac.enabled)
    {
        SPKR_NSD.clear();
        PCM_MUTE.set();
        SAI1_Block_A->CR1 &= ~SAI_xCR1_SAIEN;
        DMA1_Stream0->CR &= ~DMA_SxCR_EN;
    }
    ac.enabled = enable;
    return 0;
}

void _queue_if_possible()
{
    uint32_t next_buffer;
    if(ac.rd_ready_ptr != ac.wr_ptr)
    {
        // we can queue the next buffer
        next_buffer = mr_sound.address + ac.wr_ptr * ac.buf_size_bytes;
        ac.wr_ptr = _ptr_plus_one(ac.wr_ptr);
    }
    else
    {
        // queue silence
        next_buffer = (uint32_t)ac.silence;
    }

    // handle the unlikely condition that CT changed whilst we were writing
    while(true)
    {
        auto target = (DMA1_Stream0->CR & DMA_SxCR_CT_Msk) ? &DMA1_Stream0->M0AR : &DMA1_Stream0->M1AR;
        *target = next_buffer;
        __DMB();
        target = (DMA1_Stream0->CR & DMA_SxCR_CT_Msk) ? &DMA1_Stream0->M0AR : &DMA1_Stream0->M1AR;
        if(*target == next_buffer) break;
    }
}

int syscall_audioqueuebuffer(const void *buffer, void **next_buffer, int *_errno)
{
    CriticalGuard cg(sl_sound);
    if(buffer)
    {
        if(!next_buffer)
        {
            // cannot allow rd_ptr ready to get ahead of rd_ready_ptr
            *_errno = EINVAL;
            return -1;
        }
        ac.rd_ready_ptr = _ptr_plus_one(ac.rd_ready_ptr);        
    }
    if(next_buffer)
    {
        if(_ptr_plus_one(ac.rd_ptr) == ac.wr_ptr)
        {
            // buffers full
            *_errno = EBUSY;
            return -3;
        }
        *next_buffer = (void *)(mr_sound.address + ac.rd_ptr * ac.buf_size_bytes);
        ac.rd_ptr = _ptr_plus_one(ac.rd_ptr);
    }
    return 0;
}

int syscall_audiowaitfree(int *_errno)
{
    CriticalGuard cg(sl_sound);
    if(_ptr_plus_one(ac.rd_ptr) == ac.wr_ptr)
    {
        ac.waiting_thread = GetCurrentThreadForCore();
        return -2;
    }
    else
    {
        ac.waiting_thread = nullptr;
        return 0;
    }
}

extern "C" void DMA1_Stream0_IRQHandler()
{
    CriticalGuard cg(sl_sound);
    _queue_if_possible();

    if(ac.waiting_thread && _ptr_plus_one(ac.rd_ptr) != ac.wr_ptr)
    {
        ac.waiting_thread->ss_p.ival1 = 0;
        ac.waiting_thread->ss.Signal();
        ac.waiting_thread = nullptr;
    }

    DMA1->LIFCR = DMA_LIFCR_CTCIF0;
    __DMB();
}

extern "C" void EXTI2_IRQHandler()
{
    CriticalGuard cg(sl_sound);
    auto v = PCM_ZERO.value();
    if(!v && volume_pct)
    {
        PCM_MUTE.clear();
        SPKR_NSD.set();
    }
    else
    {
        PCM_MUTE.set();
        SPKR_NSD.clear();
    }
    EXTI->PR1 = EXTI_PR1_PR2;
    __DMB();
}

int sound_set_volume(int new_vol_pct)
{
    if(new_vol_pct < 0 || new_vol_pct > 100)
        return -1;
    
    volume_pct = new_vol_pct;

    if(!PCM_ZERO.value() && volume_pct)
    {
        PCM_MUTE.clear();
        SPKR_NSD.set();
    }
    else
    {
        PCM_MUTE.set();
        SPKR_NSD.clear();
    }

    // TODO: set volume on PCM

    return 0;
}

int sound_get_volume()
{
    return volume_pct;
}