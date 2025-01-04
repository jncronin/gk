#include <stm32h7rsxx.h>
#include "pins.h"
#include "clocks.h"
#include "SEGGER_RTT.h"
#include "gk_conf.h"

INTFLASH_RDATA static const constexpr pin XSPI_PINS[] =
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
#if GK_XSPI_DUAL_MEMORY
    { GPIOO, 3, 9 },
#endif
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
#if GK_XSPI_DUAL_MEMORY
    { GPIOP, 8, 9 },
    { GPIOP, 9, 9 },
    { GPIOP, 10, 9 },
    { GPIOP, 11, 9 },
    { GPIOP, 12, 9 },
    { GPIOP, 13, 9 },
    { GPIOP, 14, 9 },
    { GPIOP, 15, 9 },
#endif
};

INTFLASH_RDATA static const constexpr pin XSPI1_RESET { GPIOD, 1 };
INTFLASH_RDATA static const constexpr pin XSPI2_RESET { GPIOD, 0 };

#if GK_XSPI_DUAL_MEMORY
typedef uint32_t uint_xspi;
#else
typedef uint16_t uint_xspi;
#endif

const constexpr unsigned int xspi_mult = sizeof(uint_xspi);
uint_xspi id0 = 0;
uint_xspi id1 = 0;
uint_xspi cr0 = 0;
uint_xspi cr1 = 0;
uint_xspi id0_1 = 0;
uint_xspi id1_1 = 0;
uint_xspi cr0_1 = 0;
uint_xspi cr1_1 = 0;

template <typename T> INTFLASH_FUNCTION static int xspi_ind_write(XSPI_TypeDef *instance, size_t nbytes, size_t addr, const T *d)
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

template <typename T> INTFLASH_FUNCTION static int xspi_ind_read(XSPI_TypeDef *instance, size_t nbytes, size_t addr, T *d)
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

// Ensure these go in intflash
template INTFLASH_FUNCTION static int xspi_ind_write<uint16_t>(XSPI_TypeDef *instance, size_t nbytes, size_t addr, const uint16_t *d);
template INTFLASH_FUNCTION static int xspi_ind_write<uint32_t>(XSPI_TypeDef *instance, size_t nbytes, size_t addr, const uint32_t *d);
template INTFLASH_FUNCTION static int xspi_ind_read<uint16_t>(XSPI_TypeDef *instance, size_t nbytes, size_t addr, uint16_t *d);
template INTFLASH_FUNCTION static int xspi_ind_read<uint32_t>(XSPI_TypeDef *instance, size_t nbytes, size_t addr, uint32_t *d);

INTFLASH_FUNCTION static uint16_t xspi_ind_read16(XSPI_TypeDef *instance, size_t addr)
{
    uint16_t ret;
    if(xspi_ind_read(instance, 2, addr, &ret) != 2)
        return 0xffff;
    else
        return ret;
}

INTFLASH_FUNCTION static int xspi_ind_write16(XSPI_TypeDef *instance, size_t addr, uint16_t v)
{
    return xspi_ind_write(instance, 2, addr, &v);
}

[[maybe_unused]] INTFLASH_FUNCTION static uint32_t xspi_read_status()
{
    xspi_ind_write16(XSPI2, 0x555*2, 0x70);
    return xspi_ind_read16(XSPI2, 0);
}

extern "C" INTFLASH_FUNCTION int init_xspi()
{
    // enable CSI for compensation cell
    RCC->CR |= RCC_CR_CSION;
    while(!(RCC->CR & RCC_CR_CSIRDY));

    RCC->APB4ENR |= RCC_APB4ENR_SBSEN;
    (void)RCC->APB4ENR;

    SBS->CCCSR |= SBS_CCCSR_COMP_EN | SBS_CCCSR_XSPI1_COMP_EN |
        SBS_CCCSR_XSPI2_COMP_EN | SBS_CCCSR_XSPI1_IOHSLV |
        SBS_CCCSR_XSPI2_IOHSLV;
    
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

    delay_ms(5);
    XSPI1_RESET.set();
    XSPI2_RESET.set();
    delay_ms(5);

    // Pull-ups on NCS PN1/PO0 in standby mode
    PWR->PUCRN = PWR_PUCRN_PUN1;
    PWR->PUCRO = PWR_PUCRO_PUO0;
    PWR->APCR |= PWR_APCR_APC;


    // Init XSPI controller
    RCC->AHB5ENR |= RCC_AHB5ENR_XSPI1EN | RCC_AHB5ENR_XSPI2EN | RCC_AHB5ENR_XSPIMEN;
    (void)RCC->AHB5ENR;
    RCC->AHB5RSTR = RCC_AHB5RSTR_XSPI1RST | RCC_AHB5RSTR_XSPI2RST | RCC_AHB5RSTR_XSPIMRST;
    (void)RCC->AHB5RSTR;
    RCC->AHB5RSTR = 0;
    (void)RCC->AHB5RSTR;

    // Power to XSPI pins
    PWR->CSR2 |= PWR_CSR2_EN_XSPIM1 | PWR_CSR2_EN_XSPIM2 |
        (3U << PWR_CSR2_XSPICAP1_Pos);
        ;

    XSPIM->CR = 0;  // direct mode

    /* XSPI1 - dual-octal HyperBus 2x 64MByte, 200 MHz
        Run at 184 MHz -> 1 clk = 5.43 ns
        Default:
            - initial latency 7 clk
            - fixed latency - 2 times initial
            - legacy wrapped burst
            - burst length 32 bytes
     */
    XSPI1->CR = (1UL << XSPI_CR_FMODE_Pos) | XSPI_CR_EN | XSPI_CR_TCEN
#if GK_XSPI_DUAL_MEMORY
        | XSPI_CR_DMM
#endif
        ;
    XSPI1->LPTR = 0xffffU; // max - still < 1ms @ 200 MHz
    XSPI1->DCR1 = (5UL << XSPI_DCR1_MTYP_Pos) |
        (1UL << XSPI_DCR1_CSHT_Pos) |
#if GK_XSPI_DUAL_MEMORY
        (26UL << XSPI_DCR1_DEVSIZE_Pos)
#else
        (25UL << XSPI_DCR1_DEVSIZE_Pos)
#endif
        ;
    XSPI1->DCR3 = (25UL << XSPI_DCR3_CSBOUND_Pos);      // cannot wrap > 1/2 of each chip (2 dies per chip)
    XSPI1->DCR4 = 184 - 4 - 1;      // tCSM=1us/184 MHz
    XSPI1->DCR2 = (0UL << XSPI_DCR2_WRAPSIZE_Pos) |     // start without wrap
        (7UL << XSPI_DCR2_PRESCALER_Pos);               // start slow for id
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->CCR = XSPI_CCR_DQSE |
        (4UL << XSPI_CCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_CCR_DMODE_Pos) |
        XSPI_CCR_ADSIZE |
        XSPI_CCR_DDTR | XSPI_CCR_ADDTR;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WCCR = XSPI_WCCR_DQSE |
        (4UL << XSPI_WCCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_WCCR_DMODE_Pos) |
        XSPI_WCCR_ADSIZE |
        XSPI_WCCR_DDTR | XSPI_WCCR_ADDTR;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WPCCR = XSPI_CCR_DQSE |
        (4UL << XSPI_WPCCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_WPCCR_DMODE_Pos) |
        XSPI_WPCCR_ADSIZE |
        XSPI_WPCCR_DDTR | XSPI_WPCCR_ADDTR;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->HLCR = (7UL << XSPI_HLCR_TRWR_Pos) |
        (7UL << XSPI_HLCR_TACC_Pos) |
        XSPI_HLCR_LM;
    /*while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->TCR = XSPI_TCR_DHQC;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WPTCR = XSPI_WPTCR_DHQC;*/

    // Do some indirect register reads to prove we're connected
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->DLR = 3; // 2 bytes per register per chip

    xspi_ind_read(XSPI1, 4, 0, &id0);
    if(id0 != (uint_xspi)0x0f860f86U)
    {
        // try again
        xspi_ind_read(XSPI1, 4, 0, &id0);
        if(id0 != (uint_xspi)0x0f860f86U)
        {
            __asm__ volatile("bkpt \n" ::: "memory");
        }
    }

    xspi_ind_read(XSPI1, xspi_mult, 1*xspi_mult, &id1);
    xspi_ind_read(XSPI1, xspi_mult, 0x800*xspi_mult, &cr0);
    xspi_ind_read(XSPI1, xspi_mult, 0x801*xspi_mult, &cr1);

    xspi_ind_read(XSPI1, xspi_mult, 0x1000000*xspi_mult, &id0_1);
    xspi_ind_read(XSPI1, xspi_mult, 0x1000001*xspi_mult, &id1_1);
    xspi_ind_read(XSPI1, xspi_mult, 0x1000800*xspi_mult, &cr0_1);
    xspi_ind_read(XSPI1, xspi_mult, 0x1000801*xspi_mult, &cr1_1);

    // Try and enable differential clock...
    INTFLASH_STRING static char msg_xspi_enable_diffclk[] = "xspi: enabling differential clk\n";
    SEGGER_RTT_printf(0, msg_xspi_enable_diffclk);

    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->CR &= ~XSPI_CR_FMODE;        // indirect write mode

    while(XSPI1->SR & XSPI_SR_BUSY);
    // don't use any latency in register write mode
    XSPI1->HLCR |= XSPI_HLCR_WZL;

    // don't use RWDS in register write mode
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WCCR &= ~XSPI_WCCR_DQSE;

    
    uint_xspi new_cr1 = (uint_xspi)0xff81ff81U;
    xspi_ind_write(XSPI1, xspi_mult, 0x801U*xspi_mult, &new_cr1);

#if GK_XSPI_DUAL_MEMORY
    uint_xspi new_cr0 = 0x8f0a8f0aU; // hybrid burst, 16 byte burst, fixed latency
#else
    uint_xspi new_cr0 = (uint_xspi)0x8f0b8f0bU; // 32 byte burst in single memory mode
#endif
    uint_xspi drive_strength = 0U;       // default
    new_cr0 |= (drive_strength << 12) | (drive_strength << 28);
    uint_xspi latency = 2U;  // 7 clock @200 MHz max freq
    new_cr0 |= (latency << 4) | (latency << 20);
    xspi_ind_write(XSPI1, xspi_mult, 0x800U*xspi_mult, &new_cr0);

    // set new latency
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->HLCR = (7UL << XSPI_HLCR_TRWR_Pos) |
        (7UL << XSPI_HLCR_TACC_Pos) |
        XSPI_HLCR_LM;

    xspi_ind_read(XSPI1, xspi_mult, 0x800U*xspi_mult, &cr0);
    xspi_ind_read(XSPI1, xspi_mult, 0x801U*xspi_mult, &cr1);
    xspi_ind_read(XSPI1, xspi_mult, 0x1000800*xspi_mult, &cr0_1);
    xspi_ind_read(XSPI1, xspi_mult, 0x1000801*xspi_mult, &cr1_1);

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

    // Perform a indirect mode memory test
    /*while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->DCR1 = (XSPI1->DCR1 & ~XSPI_DCR1_MTYP_Msk) | (4U << XSPI_DCR1_MTYP_Pos);
    const uint32_t test_max = 0x10000U;
    static uint32_t i = 0;
    for(i = 0; i < test_max; i += 4)
    {
        uint_xspi val = i;
        xspi_ind_write(XSPI1, xspi_mult, i, &val);
    }*/
    /*for(i = 0; i < test_max; i += 4)
    {
        static uint32_t j = 0;
        xspi_ind_read(XSPI1, 4, i, &j);
        if(i != j) __BKPT();
    }*/

    // set higher interface speed with wrap enabled
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->DCR2 = (3UL << XSPI_DCR2_WRAPSIZE_Pos) |     // 16 byte hybrid read per chip = 32 bytes at XSPI interface
        (1UL << XSPI_DCR2_PRESCALER_Pos);               


    // XSPI timing calibration in memory mode
    /*while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->DCR1 = (XSPI1->DCR1 & ~XSPI_DCR1_MTYP_Msk) | (4U << XSPI_DCR1_MTYP_Pos);
    uint32_t tst_data[2];
    tst_data[0] = 0x11223344;
    tst_data[1] = 0x55667788;
    xspi_ind_write(XSPI1, sizeof(tst_data), 0, tst_data);
    static int errs[0x20] = { 0 };

    for(unsigned int coarse = 0; coarse < 0x1fU; coarse++)
    {
        while(XSPI1->SR & XSPI_SR_BUSY);
        XSPI1->CALSIR = (coarse << XSPI_CALSIR_COARSE_Pos);

        errs[coarse] = 0;
        for(int i = 0; i < 1024*32; i++)
        {
            uint32_t recv_data[sizeof(tst_data)/sizeof(uint32_t)];
            xspi_ind_read(XSPI1, sizeof(recv_data), 0, recv_data);
            for(unsigned int j = 0; j < sizeof(tst_data)/sizeof(uint32_t); j++)
            {
                if(recv_data[j] != tst_data[j])
                {
                    errs[coarse]++;
                    break;
                }
            }
        }
    }

    while(XSPI1->SR & XSPI_SR_BUSY);
    auto coarse = (XSPI1->CALFCR & XSPI_CALFCR_COARSE_Msk) >> XSPI_CALFCR_COARSE_Pos;
    auto fine = (XSPI1->CALFCR & XSPI_CALFCR_FINE_Msk) >> XSPI_CALFCR_FINE_Pos;
    XSPI1->CALMR = (coarse/4) << XSPI_CALMR_COARSE_Pos |
        (fine/4) << XSPI_CALMR_FINE_Pos;
    */

    // Fixed CALSOR - standard for 150 MHz clock seems to be 0x80003
    //  increase a bit to deal with second chip write errors
    //while(XSPI1->SR & XSPI_SR_BUSY);
    //XSPI1->CALSOR = 0x8000f;
    //while(XSPI1->SR & XSPI_SR_BUSY);
    //XSPI1->CALSIR = 0x8000f;

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
    XSPI2->CR = (1UL << XSPI_CR_FMODE_Pos) |
        XSPI_CR_TCEN | XSPI_CR_EN;
    XSPI2->LPTR = 0xfffffU; // max - still < 1ms @ 200 MHz
    XSPI2->DCR1 = (4UL << XSPI_DCR1_MTYP_Pos) |
        (0UL << XSPI_DCR1_CSHT_Pos) |
        (25UL << XSPI_DCR1_DEVSIZE_Pos);
    XSPI2->DCR3 = 0;    // ?max burst length
    XSPI2->DCR4 = 0;    // no refresh needed
    XSPI2->DCR2 = (0UL << XSPI_DCR2_WRAPSIZE_Pos) |           // TODO enable hybrid wrap on device               
        (1UL << XSPI_DCR2_PRESCALER_Pos); // use PLL2, 384MHz /3
    while(XSPI2->SR & XSPI_SR_BUSY);
    XSPI2->CCR = XSPI_CCR_DQSE |    // respect RWDS from device
        XSPI_CCR_DDTR | XSPI_CCR_ADDTR;
    XSPI2->WCCR = 0 |
        XSPI_CCR_DDTR | XSPI_CCR_ADDTR;
    XSPI2->WPCCR = 0 |
        XSPI_CCR_DDTR | XSPI_CCR_ADDTR;

    while(XSPI2->SR & XSPI_SR_BUSY);
    XSPI2->HLCR = (7UL << XSPI_HLCR_TRWR_Pos) |
        (16UL << XSPI_HLCR_TACC_Pos) |
        XSPI_HLCR_WZL;

    // Try and enter ID mode
    xspi_ind_write16(XSPI2, 0x555*2, 0xaa);
    xspi_ind_write16(XSPI2, 0x2aa*2, 0x55);
    xspi_ind_write16(XSPI2, 0x555*2, 0x90);

    INTFLASH_STRING static char msg_xspi_manfid[] = "xspi2: manufacturer id: %04x, dev id: %04x\n";
    SEGGER_RTT_printf(0, msg_xspi_manfid,
        xspi_ind_read16(XSPI2, 0),
        xspi_ind_read16(XSPI2, 2));

    // Exit ID mode
    xspi_ind_write16(XSPI2, 0, 0xf0);

    // Enter ASP mode
    xspi_ind_write16(XSPI2, 0x555*2, 0xaa);
    xspi_ind_write16(XSPI2, 0x2aa*2, 0x55);
    xspi_ind_write16(XSPI2, 0x555*2, 0x40);

    delay_ms(1);

    // Get ASP register
    auto aspr = xspi_ind_read16(XSPI2, 0);
    if(aspr & (1U << 11))
    {    
        if(aspr != 0xfeffU) __BKPT();

        // Enable hybrid burst
        xspi_ind_write16(XSPI2, 0, 0xa0);
        xspi_ind_write16(XSPI2, 0, aspr & 0xf7ffU);

        delay_ms(5);

        // Check
        if(xspi_ind_read16(XSPI2, 0) != (aspr & 0xf7ffU)) __BKPT();
    }

    // Exit
    xspi_ind_write16(XSPI2, 0, 0xf0);

    // Enable hybrid burst 32-byte wrap
    while(XSPI2->SR & XSPI_SR_BUSY);
    XSPI2->DCR2 = (3UL << XSPI_DCR2_WRAPSIZE_Pos) |
        (1UL << XSPI_DCR2_PRESCALER_Pos);               

    // Return to memory mapped mode
    while(XSPI2->SR & XSPI_SR_BUSY);
    XSPI2->CR = (XSPI2->CR & ~XSPI_CR_FMODE_Msk) |
        (3U << XSPI_CR_FMODE_Pos);

    // Now set IO compensation cell to use already calculated values
    // see: https://community.st.com/t5/stm32-mcus-products/stm32h7s7l8h6h-xspi-instability/td-p/749315
    SBS->CCSWVALR = SBS->CCVALR;
    (void)SBS->CCSWVALR;
    SBS->CCCSR |= SBS_CCCSR_XSPI1_COMP_CODESEL |
        SBS_CCCSR_XSPI2_COMP_CODESEL;

    return 0;
}

