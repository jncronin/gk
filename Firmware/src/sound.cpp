#include "stm32h7xx.h"
#include "sound.h"
#include "pins.h"
#include <math.h>

static constexpr const pin SAI1_SCK_A { GPIOE, 5, 6 };
static constexpr const pin SAI1_SD_A { GPIOC, 1, 6 };
static constexpr const pin SAI1_FS_A { GPIOE, 4, 6 };
static constexpr const pin SAI1_MCLK_A { GPIOE, 2, 6 };
static constexpr const pin PCM_FMT { GPIOI, 4 };
static constexpr const pin PCM_DEMP { GPIOI, 5 };
static constexpr const pin PCM_MUTE { GPIOI, 6 };
static constexpr const pin PCM_ZERO { GPIOB, 2 };   // input
static constexpr const pin SPKR_NSD { GPIOI, 8 };

// test buffer for sine wave

/* 48kHz, 440 Hz wave -> 109 samples/wave */
constexpr int fs = 48000;
constexpr int fwave = 440;
constexpr int nchan = 2;
constexpr int nsamps = fs / fwave;
int16_t sine_wave[nsamps * nchan];

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

    /* SAI1 receives 49.152 MHz clock
        Connected to PCM1754 

        This expects 32 bits/channel (we only use 16 of them) 
            thus 64 bits for an entire left/right sample

        To produce accurate MCLK, we need SAI clock of 1024 * Fs (where Fs = e.g. 48kHz)


        I2S frame is left then right, left has FS low
        First 16 bits is data then zeros, can do this with the 'slot' mechanism of SAI
        - Slot1 = left data, Slot 2 = off
        - Slot3 = right data, Slot 4 = off
    */
    
    RCC->APB2ENR |= RCC_APB2ENR_SAI1EN;
    (void)RCC->APB2ENR;

    SAI1_Block_A->CR1 =
        SAI_xCR1_MCKEN |
        SAI_xCR1_OSR |
        (2UL << SAI_xCR1_MCKDIV_Pos) |
        (4UL << SAI_xCR1_DS_Pos) |
        SAI_xCR1_DMAEN;
    SAI1_Block_A->CR2 =
        (2UL << SAI_xCR2_FTH_Pos);
    SAI1_Block_A->FRCR =
        SAI_xFRCR_FSOFF |
        SAI_xFRCR_FSDEF |
        (31UL << SAI_xFRCR_FSALL_Pos) |
        (63UL << SAI_xFRCR_FRL_Pos);

    /*
    SAI1_Block_A->SLOTR =
        (3UL << SAI_xSLOTR_NBSLOT_Pos) |        // 4x 16-bit slots
        (5UL << SAI_xSLOTR_SLOTEN_Pos);         // enable slots 1 and 3
    */
    SAI1_Block_A->SLOTR =
        (1UL << SAI_xSLOTR_NBSLOT_Pos) |        // 2x 32-bit slots
        (3UL << SAI_xSLOTR_SLOTEN_Pos) |
        (2UL << SAI_xSLOTR_SLOTSZ_Pos);
    
    PCM_MUTE.clear();

    // generate 440 hz sine wave
    for(int i = 0; i < nsamps; i++)
    {
        // i / nsamps = proportion of 2*pi
        float omega = (float)i / (float)nsamps * 2.0f * 3.14159f;
        float val = sinf(omega) * 20000.0f;
        auto ival = (int16_t)roundf(val);

        for(int j = 0; j < nchan; j++)
        {
            sine_wave[i * nchan + j] = ival;
        }
    }

    // DMA setup
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    (void)RCC->AHB1ENR;


    DMA1_Stream0->CR = (1UL << DMA_SxCR_MSIZE_Pos) |
        (1UL << DMA_SxCR_PSIZE_Pos) |
        DMA_SxCR_MINC |
        DMA_SxCR_CIRC | 
        (1UL << DMA_SxCR_DIR_Pos);
    DMA1_Stream0->NDTR = nsamps * nchan;
    DMA1_Stream0->M0AR = (uint32_t)(uintptr_t)sine_wave;
    DMA1_Stream0->PAR = (uint32_t)(uintptr_t)&SAI1_Block_A->DR;
    //DMA1_Stream0->FCR

    // DMAMUX1 request mux input 87
    DMAMUX1_Channel0->CCR = 87UL;

    // enable
    SAI1_Block_A->CR1 |= SAI_xCR1_SAIEN | SAI_xCR1_DMAEN;
    DMA1_Stream0->CR |= DMA_SxCR_EN;

/*
    while(true)
    {
        for(int i = 0; i < nsamps; i++)
        {
            // wait for fifo not 3/4 full
            while(((SAI1_Block_A->SR & SAI_xSR_FLVL_Msk) >> SAI_xSR_FLVL_Pos) >= 4);

            // left/right
            SAI1_Block_A->DR = -sine_wave[i];
            SAI1_Block_A->DR = -sine_wave[i];
        }
    } */
        
}