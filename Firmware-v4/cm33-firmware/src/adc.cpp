#include <stm32mp2xx.h>
#include "adc.h"
#include "clocks.h"
#include "pins.h"

#define dma HPDMA2_Channel12

extern uint32_t adc_vals[4];

void init_adc()
{
    // Set up ADC1/HPDMA2 to sample the joystick axes continuously

    // ADC1 is clocked at 200 MHz.  We need 4 conversions every 5 ms or so.

    // Disable deep power down
    ADC1->CR = 0;
    (void)ADC1->CR;

    ADC12_COMMON->CCR = 
        //ADC_CCR_VBATEN |
        ADC_CCR_VREFEN |
        (8U << ADC_CCR_PRESC_Pos);         // /32 = 2 MHz
    ADC1->DIFSEL = 0;

    // Enable ADC
    ADC1->ISR = ADC_ISR_ADRDY;
    ADC1->CR |= ADC_CR_ADEN;
    (void)ADC1->CR;
    while(!(ADC1->ISR & ADC_ISR_ADRDY));

    // Run calibration
    ADC1->CR |= ADC_CR_ADCAL;
    
    // First, clear any calibration factors
    ADC1->CALFACT = 0;
    ADC1->CR &= ~ADC_CR_ADCALDIF;   // single ended calibration
    ADC1->CFGR1 &= ~ADC_CFGR1_RES_Msk;  // default 12 bit

    // average out several calibrations
    uint64_t calib_avg = 0;
    const unsigned int n_avgs = 10;
    for(unsigned int i = 0; i < n_avgs; i++)
    {
        ADC1->CR |= ADC_CR_ADSTART;
        __DSB();
        while(ADC1->CR & ADC_CR_ADSTART);
        auto cur_calib = ADC1->DR;
        calib_avg += (uint64_t)cur_calib;
    }
    calib_avg /= n_avgs;
    ADC1->CALFACT = (ADC1->CALFACT & ~ADC_CALFACT_CALFACT_S_Msk) |
        (calib_avg << ADC_CALFACT_CALFACT_S_Pos);

    ADC1->CR &= ~ADC_CR_ADCAL;

    ADC1->CFGR1 = 
        ADC_CFGR1_CONT |
        ADC_CFGR1_OVRMOD |
        (0x3U << ADC_CFGR1_DMNGT_Pos);

    // x4 oversampling
    ADC1->CFGR2 = (0U << ADC_CFGR2_OVSS_Pos) |
        (3U << ADC_CFGR2_OVSR_Pos) |
        ADC_CFGR2_ROVSE;
    ADC1->PCSEL = (1U << 0) | (1U << 1) | (1U << 8) | (1U << 4);
    ADC1->SQR1 = (3U << ADC_SQR1_L_Pos) |   // 4 conversions
        (0U << ADC_SQR1_SQ1_Pos) |          // JOY_A_X
        (1U << ADC_SQR1_SQ2_Pos) |          // JOY_A_Y
        (8U << ADC_SQR1_SQ3_Pos) |          // JOY_B_X
        (4U << ADC_SQR1_SQ4_Pos);           // JOY_B_Y
    ADC1->SQR2 = 0;
    ADC1->SQR3 = 0;
    ADC1->SQR4 = 0;

    // 246.5 clock cycles/conversion
    // for 2 MHz, 4 channels, x4 oversampling this gives 507 Hz update frequency
    const unsigned smpr = 6u;
    ADC1->SMPR1 = (smpr << ADC_SMPR1_SMP0_Pos) |
        (smpr << ADC_SMPR1_SMP1_Pos) |
        (smpr << ADC_SMPR1_SMP2_Pos) |
        (smpr << ADC_SMPR1_SMP3_Pos) |
        (smpr << ADC_SMPR1_SMP4_Pos) |
        (smpr << ADC_SMPR1_SMP5_Pos) |
        (smpr << ADC_SMPR1_SMP6_Pos) |
        (smpr << ADC_SMPR1_SMP7_Pos) |
        (smpr << ADC_SMPR1_SMP8_Pos) |
        (smpr << ADC_SMPR1_SMP9_Pos);
    ADC1->SMPR2 = (smpr << ADC_SMPR2_SMP10_Pos) |
        (smpr << ADC_SMPR2_SMP11_Pos) |
        (smpr << ADC_SMPR2_SMP12_Pos) |
        (smpr << ADC_SMPR2_SMP13_Pos) |
        (smpr << ADC_SMPR2_SMP14_Pos) |
        (smpr << ADC_SMPR2_SMP15_Pos) |
        (smpr << ADC_SMPR2_SMP16_Pos) |
        (smpr << ADC_SMPR2_SMP17_Pos) |
        (smpr << ADC_SMPR2_SMP18_Pos);


    // Set up DMA
    RCC->HPDMA2CFGR |= RCC_HPDMA2CFGR_HPDMA2EN;
    RCC->HPDMA2CFGR &= ~RCC_HPDMA2CFGR_HPDMA2RST;
    __asm__ volatile("dsb sy\n" ::: "memory");

    HPDMA2->SECCFGR = (1U << 12);
    HPDMA2->PRIVCFGR = (1U << 12);

    dma->CCR = 0;
    dma->CTR1 =
        (0U << DMA_CTR1_DBL_1_Pos) |
        DMA_CTR1_DINC |
        DMA_CTR1_DSEC |
        (2U << DMA_CTR1_DDW_LOG2_Pos) |
        //DMA_CTR1_SAP |
        DMA_CTR1_SSEC |
        (0U << DMA_CTR1_SBL_1_Pos) |
        (1U << DMA_CTR1_SDW_LOG2_Pos);
    dma->CTR2 = (81U << DMA_CTR2_REQSEL_Pos);
    dma->CTR3 = 0;
    dma->CBR1 = 8U |
        DMA_CBR1_BRDDEC |
        (0U << DMA_CBR1_BRC_Pos);
    dma->CSAR = (uint32_t)(uintptr_t)&ADC1->DR;
    dma->CDAR = (uint32_t)(uintptr_t)adc_vals;
    dma->CBR2 = (16U << DMA_CBR2_BRDAO_Pos);     // -16 every block
    dma->CLLR = 4U; // anything not zero
    dma->CCR |= DMA_CCR_EN;

    ADC1->CR |= ADC_CR_ADSTART;
}
