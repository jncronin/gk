#include <stm32h7rsxx.h>
#include "pins.h"
#include "clocks.h"
#include "SEGGER_RTT.h"

static const constexpr pin XSPI_PINS[] =
{
    { GPIOO, 0, 9 },    // CS#
    { GPION, 1, 9 },    // CS#
    { GPION, 0, 9 },
    { GPION, 2, 9 },
    { GPION, 3, 9 },
    { GPION, 4, 9 },
    { GPION, 5, 9 },
    { GPION, 6, 9 },
    { GPION, 7, 9 },
    { GPION, 8, 9 },
    { GPION, 9, 9 },
    { GPION, 10, 9 },
    { GPION, 11, 9 },
    { GPIOO, 2, 9 },
    { GPIOO, 3, 9 },
    { GPIOO, 4, 9 },
    { GPIOO, 5, 9 },
    { GPIOP, 0, 9 },
    { GPIOP, 1, 9 },
    { GPIOP, 2, 9 },
    { GPIOP, 3, 9 },
    { GPIOP, 4, 9 },
    { GPIOP, 5, 9 },
    { GPIOP, 6, 9 },
    { GPIOP, 7, 9 },
    { GPIOP, 8, 9 },
    { GPIOP, 9, 9 },
    { GPIOP, 10, 9 },
    { GPIOP, 11, 9 },
    { GPIOP, 12, 9 },
    { GPIOP, 13, 9 },
    { GPIOP, 14, 9 },
    { GPIOP, 15, 9 },
};

static const constexpr pin XSPI1_RESET { GPIOD, 1 };
static const constexpr pin XSPI2_RESET { GPIOD, 0 };

uint32_t id0 = 0;
uint32_t id1 = 0;
uint32_t cr0 = 0;
uint32_t cr1 = 0;

template <typename T> static int xspi_ind_write(XSPI_TypeDef *instance, size_t nbytes, size_t addr, const T *d)
{
    static_assert(sizeof(T) <= 4);

    if(!d)
        return -1;
    
    // set indirect write mode
    while(instance->SR & XSPI_SR_BUSY);
    instance->CR = (instance->CR & ~XSPI_CR_FMODE_Msk & ~XSPI_CR_FTHRES_Msk) |
        (0UL << XSPI_CR_FMODE_Pos) |
        ((sizeof(T) - 1) << XSPI_CR_FTHRES_Pos);

    // write data
    instance->DLR = nbytes-1;
    instance->AR = addr;

    size_t ret = 0;
    
    while(nbytes)
    {
        while(!(instance->SR & XSPI_SR_FTF));
        *(volatile T *)&instance->DR = *d++;
        ret += sizeof(T);

        if(nbytes < sizeof(T))
            nbytes = 0;
        else
            nbytes -= sizeof(T);
    }

    while(!(instance->SR & XSPI_SR_TCF));
    instance->FCR = XSPI_FCR_CTCF;

    return (int)ret;
}

template <typename T> static int xspi_ind_read(XSPI_TypeDef *instance, size_t nbytes, size_t addr, T *d)
{
    static_assert(sizeof(T) <= 4);

    if(!d)
        return -1;
    
    // set indirect read mode
    while(instance->SR & XSPI_SR_BUSY);
    instance->CR = (instance->CR & ~XSPI_CR_FMODE_Msk & ~XSPI_CR_FTHRES_Msk) |
        (1UL << XSPI_CR_FMODE_Pos) |
        ((sizeof(T) - 1) << XSPI_CR_FTHRES_Pos);

    // ADMODE needs to be != 0
    while(instance->SR & XSPI_SR_BUSY);
    instance->CCR = (instance->CCR & ~XSPI_CCR_ADMODE_Msk) |
        (4UL << XSPI_CCR_ADMODE_Pos);

    // read data
    while(instance->SR & XSPI_SR_BUSY);
    instance->DLR = nbytes-1;
    instance->AR = addr;

    size_t ret = 0;
    
    while(nbytes)
    {
        while(!(instance->SR & XSPI_SR_FTF));
        *d++ = *(volatile T *)&instance->DR;
        ret += sizeof(T);

        if(nbytes < sizeof(T))
            nbytes = 0;
        else
            nbytes -= sizeof(T);
    }

    while(!(instance->SR & XSPI_SR_TCF));
    instance->FCR = XSPI_FCR_CTCF;

    return (int)ret;
}

static uint16_t xspi_ind_read16(XSPI_TypeDef *instance, size_t addr)
{
    uint16_t ret;
    if(xspi_ind_read(instance, 2, addr, &ret) != 2)
        return 0xffff;
    else
        return ret;
}

static int xspi_ind_write16(XSPI_TypeDef *instance, size_t addr, uint16_t v)
{
    return xspi_ind_write(instance, 2, addr, &v);
}

[[maybe_unused]] static uint32_t xspi_read_status()
{
    xspi_ind_write16(XSPI2, 0x555*2, 0x70);
    return xspi_ind_read16(XSPI2, 0);
}

extern "C" int init_xspi()
{
    // enable CSI for compensation cell
    RCC->CR |= RCC_CR_CSION;
    while(!(RCC->CR & RCC_CR_CSIRDY));

    RCC->APB4ENR |= RCC_APB4ENR_SBSEN;
    (void)RCC->APB4ENR;

    SBS->CCCSR |= SBS_CCCSR_COMP_EN | SBS_CCCSR_XSPI1_COMP_EN |
        SBS_CCCSR_XSPI2_COMP_EN;
    
    // pin setup
    for(const auto &p : XSPI_PINS)
    {
        p.set_as_af();
    }
    // CS# needs pullups
    XSPI_PINS[0].set_as_af(pin::PushPull, pin::PullUp);
    XSPI_PINS[1].set_as_af(pin::PushPull, pin::PullUp);

    // hardware reset chips
    XSPI1_RESET.clear();
    XSPI2_RESET.clear();
    XSPI1_RESET.set_as_output();
    XSPI2_RESET.set_as_output();

    delay_ms(1);
    XSPI1_RESET.set();
    XSPI2_RESET.set();
    delay_ms(1);

    // Init XSPI controller
    RCC->AHB5ENR |= RCC_AHB5ENR_XSPI1EN | RCC_AHB5ENR_XSPI2EN | RCC_AHB5ENR_XSPIMEN;
    (void)RCC->AHB5ENR;
    RCC->AHB5RSTR = RCC_AHB5RSTR_XSPI1RST | RCC_AHB5RSTR_XSPI2RST | RCC_AHB5RSTR_XSPIMRST;
    (void)RCC->AHB5RSTR;
    RCC->AHB5RSTR = 0;
    (void)RCC->AHB5RSTR;

    // Power to XSPI pins
    PWR->CSR2 |= PWR_CSR2_EN_XSPIM1 | PWR_CSR2_EN_XSPIM2;

    XSPIM->CR = 0;  // direct mode

    /* XSPI1 - dual-octal HyperBus 2x 64MByte, 200 MHz
        We can only run at ~133 due to STM32 restrictions (dual octal without DQS)
         - this allows latency of 2x5 clk
        Default:
            - initial latency 7 clk
            - fixed latency - 2 times initial
            - legacy wrapped burst
            - burst length 32 bytes
     */
    XSPI1->CR = (1UL << XSPI_CR_FMODE_Pos) | XSPI_CR_EN | XSPI_CR_TCEN |
        XSPI_CR_DMM;
    XSPI1->LPTR = 0xfffffU; // max - still < 1ms @ 200 MHz
    XSPI1->DCR1 = (5UL << XSPI_DCR1_MTYP_Pos) |
        (0UL << XSPI_DCR1_CSHT_Pos) |
        (26UL << XSPI_DCR1_DEVSIZE_Pos);
    XSPI1->DCR3 = (25UL << XSPI_DCR3_CSBOUND_Pos);      // cannot wrap > 1/2 of each chip (2 dies per chip)
    XSPI1->DCR4 = 532 - 4 - 1;      // tCSM=4us/133 MHz
    XSPI1->DCR2 = (5UL << XSPI_DCR2_WRAPSIZE_Pos) |
        (1UL << XSPI_DCR2_PRESCALER_Pos); 
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->CCR = XSPI_CCR_DQSE |
        (4UL << XSPI_CCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_CCR_DMODE_Pos) |
        XSPI_CCR_DDTR | XSPI_CCR_ADDTR;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WCCR = XSPI_WCCR_DQSE |
        (4UL << XSPI_WCCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_WCCR_DMODE_Pos) |
        XSPI_WCCR_DDTR | XSPI_WCCR_ADDTR;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WPCCR = XSPI_WPCCR_DQSE |
        (4UL << XSPI_WPCCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_WPCCR_DMODE_Pos) |
        XSPI_WPCCR_DDTR | XSPI_WPCCR_ADDTR;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->HLCR = (5UL << XSPI_HLCR_TRWR_Pos) |
        (7UL << XSPI_HLCR_TACC_Pos) |
        XSPI_HLCR_LM;

    // Do some indirect register reads to prove we're connected
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->DLR = 3; // 2 bytes per register per chip

    xspi_ind_read(XSPI1, 4, 0, &id0);
    if(id0 != 0x0f860f86)
    {
        // try again
        xspi_ind_read(XSPI1, 4, 0, &id0);
        if(id0 != 0x0f860f86)
        {
            __asm__ volatile("bkpt \n" ::: "memory");
        }
    }

    xspi_ind_read(XSPI1, 4, 1*4, &id1);
    xspi_ind_read(XSPI1, 4, 0x800*4, &cr0);
    xspi_ind_read(XSPI1, 4, 0x801*4, &cr1);

    // Try and enable differential clock...
    SEGGER_RTT_printf(0, "xspi: enabling differential clk\n");

    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->CR &= ~XSPI_CR_FMODE;        // indirect write mode

    while(XSPI1->SR & XSPI_SR_BUSY);
    // don't use any latency in register write mode
    XSPI1->HLCR |= XSPI_HLCR_WZL;

    // don't use RWDS in register write mode
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WCCR &= ~XSPI_WCCR_DQSE;

    uint32_t new_cr1 = 0xff81ff81U;
    xspi_ind_write(XSPI1, 4, 0x801U*4, &new_cr1);

    uint32_t new_cr0 = 0x8f088f08U; // hybrid burst, 128 byte burst, 5 initial latency, fixed latency
    xspi_ind_write(XSPI1, 4, 0x800U*4, &new_cr0);

    // set new latency
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->HLCR = (5UL << XSPI_HLCR_TRWR_Pos) |
        (5UL << XSPI_HLCR_TACC_Pos) |
        XSPI_HLCR_LM;

    xspi_ind_read(XSPI1, 4, 0x800U*4, &cr0);
    xspi_ind_read(XSPI1, 4, 0x801U*4, &cr1);

    if(cr0 != new_cr0)
    {
        __asm__ volatile ("bkpt \n" ::: "memory");
    }
    if(cr1 != new_cr1)
    {
        __asm__ volatile ("bkpt \n" ::: "memory");
    }

    while(XSPI1->SR & XSPI_SR_BUSY);
    // reenable latency in write mode
    XSPI1->HLCR &= ~XSPI_HLCR_WZL;

    // reenable RWDS in write mode
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WCCR |= XSPI_WCCR_DQSE;

    // set XSPI1 to memory mapped mode
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->CR |= 3U << XSPI_CR_FMODE_Pos;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->DCR1 = (XSPI1->DCR1 & ~XSPI_DCR1_MTYP_Msk) | (4U << XSPI_DCR1_MTYP_Pos);
    while(XSPI1->SR & XSPI_SR_BUSY);

    /* XSPI2 - octal HyperBus 64 MByte, 166 MHz
        read latency (initial) 16 clk for 166 MHz, no additional
        no write latency
        256 kbyte sectors

        write buffer programming - 512 bytes at a time on 512 byte boundary
        see datasheet p31 for write flowchart
    */
    XSPI2->CR = (3UL << XSPI_CR_FMODE_Pos) |
        XSPI_CR_TCEN | XSPI_CR_EN;
    XSPI2->LPTR = 0xfffffU; // max - still < 1ms @ 200 MHz
    XSPI2->DCR1 = (4UL << XSPI_DCR1_MTYP_Pos) |
        (0UL << XSPI_DCR1_CSHT_Pos) |
        (25UL << XSPI_DCR1_DEVSIZE_Pos);
    XSPI2->DCR3 = 0;    // ?max burst length
    XSPI2->DCR4 = 0;    // no refresh needed
    XSPI2->DCR2 = (3UL << XSPI_DCR2_WRAPSIZE_Pos) |                          
        (0UL << XSPI_DCR2_PRESCALER_Pos); // use PLL2, 160MHz
    while(XSPI2->SR & XSPI_SR_BUSY);
    XSPI2->CCR = XSPI_CCR_DQSE |    // respect RWDS from device
        //(4UL << XSPI_CCR_ADMODE_Pos) | // 8 address lines
        //(4UL << XSPI_CCR_DMODE_Pos) |
        XSPI_CCR_DDTR | XSPI_CCR_ADDTR;
    XSPI2->WCCR = 0 |
        //(4UL << XSPI_CCR_ADMODE_Pos) | // 8 address lines
        //(4UL << XSPI_CCR_DMODE_Pos) |
        XSPI_CCR_DDTR | XSPI_CCR_ADDTR;
    XSPI2->WPCCR = 0 |  // respect RWDS from device
        //(4UL << XSPI_CCR_ADMODE_Pos) | // 8 address lines
        //(4UL << XSPI_CCR_DMODE_Pos) |
        XSPI_CCR_DDTR | XSPI_CCR_ADDTR;

    while(XSPI2->SR & XSPI_SR_BUSY);
    XSPI2->HLCR = (7UL << XSPI_HLCR_TRWR_Pos) |
        (16UL << XSPI_HLCR_TACC_Pos) |
        XSPI_HLCR_WZL;

    // Try and enter ID mode
    xspi_ind_write16(XSPI2, 0x555*2, 0xaa);
    xspi_ind_write16(XSPI2, 0x2aa*2, 0x55);
    xspi_ind_write16(XSPI2, 0x555*2, 0x90);

    SEGGER_RTT_printf(0, "xspi2: manufacturer id: %04x, dev id: %04x\n",
        xspi_ind_read16(XSPI2, 0),
        xspi_ind_read16(XSPI2, 2));

    // Exit ID mode
    xspi_ind_write16(XSPI2, 0, 0xff);    

    // Return to memory mapped mode
    while(XSPI2->SR & XSPI_SR_BUSY);
    XSPI2->CR = (XSPI2->CR & ~XSPI_CR_FMODE_Msk) |
        (3U << XSPI_CR_FMODE_Pos);

    return 0;
}

