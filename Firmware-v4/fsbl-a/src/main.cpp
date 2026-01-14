#include <stm32mp2xx.h>
#include <cstddef>

#include "pins.h"
#include "logger.h"
#include "clocks.h"

static const constexpr pin EV_BLUE      { GPIOJ, 7 };
static const constexpr pin EV_RED       { GPIOH, 4 };
static const constexpr pin EV_GREEN     { GPIOD, 8 };
static const constexpr pin EV_ORANGE    { GPIOJ, 6 };

void init_clocks();

// This points to the boot_api_context_t structure which we do not use.
//  Pass it on to the ssbl-a
uint32_t _bootrom_val;

static const constexpr pin QSPI_PINS[]
{
    { GPIOD, 3, 10 },
    { GPIOD, 0, 10 },
    { GPIOD, 4, 10 },
    { GPIOD, 5, 10 },
    { GPIOD, 6, 10 },
    { GPIOD, 7, 10 }
};

static const constexpr pin USART6_TX { GPIOJ, 5, 6 };

void log(const char *s);
void log(char c);

static const constexpr uint32_t ccr_spi_no_ab_no_addr =
    (1U << OCTOSPI_CCR_DMODE_Pos) |
    (1U << OCTOSPI_CCR_IMODE_Pos);
static const constexpr uint32_t ccr_spi_no_ab_no_addr_no_data =
    (1U << OCTOSPI_CCR_IMODE_Pos);

template <typename T> static int ospi_ind_read(OCTOSPI_TypeDef *instance, size_t nbytes,
    uint32_t inst, uint32_t addr, uint32_t ab, uint32_t ccr, uint32_t ndummy,
    T* d)
{
    static_assert(sizeof(T) <= 4);

    if(nbytes && !d)
        return -1;

    __asm__ volatile("dsb sy\n" ::: "memory");
    instance->CR |= OCTOSPI_CR_ABORT;
    __asm__ volatile("dsb sy\n" ::: "memory");
    
    // set indirect read mode
    while(instance->SR & OCTOSPI_SR_BUSY);
    instance->CR = (instance->CR & ~OCTOSPI_CR_FMODE_Msk & ~OCTOSPI_CR_FTHRES_Msk) |
        (1UL << OCTOSPI_CR_FMODE_Pos) |
        ((sizeof(T) - 1) << OCTOSPI_CR_FTHRES_Pos);

    if(nbytes)
    {
        while(instance->SR & OCTOSPI_SR_BUSY);
        instance->DLR = nbytes - 1;
    }
    while(instance->SR & OCTOSPI_SR_BUSY);
    instance->TCR = (instance->TCR & ~OCTOSPI_TCR_DCYC_Msk) |
        (ndummy << OCTOSPI_TCR_DCYC_Pos);
    while(instance->SR & OCTOSPI_SR_BUSY);
    instance->CCR = ccr;
    while(instance->SR & OCTOSPI_SR_BUSY);
    instance->IR = inst;
    if(ccr & OCTOSPI_CCR_ABMODE_Msk)
    {
        while(instance->SR & OCTOSPI_SR_BUSY);
        instance->ABR = ab;
    }
    if(ccr & OCTOSPI_CCR_ADMODE_Msk)
    {
        while(instance->SR & OCTOSPI_SR_BUSY);
        instance->AR = addr;
    }

    size_t ret = 0;
    
    while(nbytes)
    {
        while(!(instance->SR & OCTOSPI_SR_FTF));
        *d++ = *(volatile T *)&instance->DR;
        ret += sizeof(T);

        if(nbytes < sizeof(T))
            nbytes = 0;
        else
            nbytes -= sizeof(T);
    }

    if(d)
    {
        while(!(instance->SR & OCTOSPI_SR_TCF));
        instance->FCR = OCTOSPI_FCR_CTCF;
    }

    return (int)ret;
}

template <typename T> static int ospi_ind_write(OCTOSPI_TypeDef *instance, size_t nbytes,
    uint32_t inst, uint32_t addr, uint32_t ab, uint32_t ccr, uint32_t ndummy,
    const T* d)
{
    static_assert(sizeof(T) <= 4);

    if(nbytes && !d)
        return -1;

    __asm__ volatile("dsb sy\n" ::: "memory");
    instance->CR |= OCTOSPI_CR_ABORT;
    __asm__ volatile("dsb sy\n" ::: "memory");

    // set indirect write mode
    while(instance->SR & OCTOSPI_SR_BUSY);
    instance->CR = (instance->CR & ~OCTOSPI_CR_FMODE_Msk & ~OCTOSPI_CR_FTHRES_Msk) |
        (0UL << OCTOSPI_CR_FMODE_Pos) |
        ((sizeof(T) - 1) << OCTOSPI_CR_FTHRES_Pos);

    if(nbytes)
    {
        while(instance->SR & OCTOSPI_SR_BUSY);
        instance->DLR = nbytes - 1;
    }
    while(instance->SR & OCTOSPI_SR_BUSY);
    instance->TCR = (instance->TCR & ~OCTOSPI_TCR_DCYC_Msk) |
        (ndummy << OCTOSPI_TCR_DCYC_Pos);
    while(instance->SR & OCTOSPI_SR_BUSY);
    instance->CCR = ccr;
    while(instance->SR & OCTOSPI_SR_BUSY);
    instance->IR = inst;
    if(ccr & OCTOSPI_CCR_ABMODE_Msk)
    {
        while(instance->SR & OCTOSPI_SR_BUSY);
        instance->ABR = ab;
    }
    if(ccr & OCTOSPI_CCR_ADMODE_Msk)
    {
        while(instance->SR & OCTOSPI_SR_BUSY);
        instance->AR = addr;
    }

    size_t ret = 0;
    
    while(nbytes)
    {
        while(!(instance->SR & OCTOSPI_SR_FTF));
        *(volatile T *)&instance->DR = *d++;
        ret += sizeof(T);

        if(nbytes < sizeof(T))
            nbytes = 0;
        else
            nbytes -= sizeof(T);
    }

    if(d)
    {
        while(!(instance->SR & OCTOSPI_SR_TCF));
        instance->FCR = OCTOSPI_FCR_CTCF;
    }

    return (int)ret;
}

static uint32_t ospi_read_status()
{
    uint8_t sr1;
    auto ret = ospi_ind_read(OCTOSPI1, 1, 0x05, 0, 0, ccr_spi_no_ab_no_addr, 0, &sr1);
    if(ret == 1)
        return (uint32_t)sr1;
    else
        return 0;
}

static uint32_t ospi_wait_not_busy()
{
    while(true)
    {
        auto sr = ospi_read_status();
        if(!(sr & 0x1))
            return sr;
    }
}

int main(uint32_t bootrom_val)
{
    _bootrom_val = bootrom_val;

    // Set up USART6 as TX only
    USART6_TX.set_as_af();
    RCC->USART6CFGR |= RCC_USART6CFGR_USART6EN;
    (void)RCC->USART6CFGR;

    // USART6 is clocked with HSI64 direct by default
    USART6->CR1 = 0;
    USART6->PRESC = 0;
    USART6->BRR = 64000000UL / 115200UL;
    USART6->CR2 = 0;
    USART6->CR3 = 0;
    USART6->CR1 = USART_CR1_FIFOEN | USART_CR1_TE | USART_CR1_UE;

    klog("FSBL: start\n");

    // Set up clocks so that we can get a nice fast clock for QSPI
    init_clocks();

    // Enable QSPI XIP
    for(const auto &p : QSPI_PINS)
    {
        p.set_as_af();
    }
    RCC->OSPIIOMCFGR |= RCC_OSPIIOMCFGR_OSPIIOMEN;
    RCC->OSPI1CFGR |= RCC_OSPI1CFGR_OSPI1EN;
    (void)RCC->OSPI1CFGR;

    OCTOSPIM->CR = 0;
    OCTOSPI1->CR = 0;
    OCTOSPI1->DCR1 = (2U << OCTOSPI_DCR1_MTYP_Pos) |
        (21U << OCTOSPI_DCR1_DEVSIZE_Pos) |     // 4 MBytes/32 Mb - we use W25Q32JV in production
        (0x3fU << OCTOSPI_DCR1_CSHT_Pos) |
        OCTOSPI_DCR1_DLYBYP;
    OCTOSPI1->DCR2 = (1U << OCTOSPI_DCR2_PRESCALER_Pos);        // 100 MHz/2 => 50 MHz
    OCTOSPI1->DCR3 = 0;
    OCTOSPI1->DCR4 = 0;
    OCTOSPI1->FCR = 0xdU;

    OCTOSPI1->CCR = (1U << OCTOSPI_CCR_DMODE_Pos) |
        (0U << OCTOSPI_CCR_ABMODE_Pos) |
        (2U << OCTOSPI_CCR_ADSIZE_Pos) |
        (1U << OCTOSPI_CCR_ADMODE_Pos) |
        (1U << OCTOSPI_CCR_IMODE_Pos);
    OCTOSPI1->TCR = 0U;
    OCTOSPI1->IR = 0x03U;

    OCTOSPI1->CR |= OCTOSPI_CR_EN;

    uint8_t jedec_id[3];
    auto nb = ospi_ind_read(OCTOSPI1, 3, 0x9f, 0, 0, ccr_spi_no_ab_no_addr, 0, jedec_id);
    if(nb == 3 && jedec_id[0] == 0xef && (jedec_id[1] == 0x40 || jedec_id[1] == 0x70) && jedec_id[2] == 0x16)
    {
        klog("FSBL: W25Q32JV found, enabling quad IO\n");

        SYSCFG->VDDIO3CCCR |= SYSCFG_VDDIO3CCCR_EN;

        // get current sr2
        uint8_t sr2 = 0;
        ospi_ind_read(OCTOSPI1, 1, 0x35, 0, 0, ccr_spi_no_ab_no_addr, 0, &sr2);
        if(!(sr2 & 0x2))
        {
            // write enable, followed by write new sr
            ospi_ind_write<uint8_t>(OCTOSPI1, 0, 0x06, 0, 0, ccr_spi_no_ab_no_addr_no_data, 0, nullptr);
            sr2 |= 0x2;
            ospi_ind_write<uint8_t>(OCTOSPI1, 1, 0x31, 0, 0, ccr_spi_no_ab_no_addr, 0, &sr2);

            // wait for busy clear
            ospi_wait_not_busy();

            // check the write succeeded
            ospi_ind_read(OCTOSPI1, 1, 0x35, 0, 0, ccr_spi_no_ab_no_addr, 0, &sr2);
        }

        OCTOSPI1->CCR = (1U << OCTOSPI_CCR_DMODE_Pos) |
            (0U << OCTOSPI_CCR_ABMODE_Pos) |
            (2U << OCTOSPI_CCR_ADSIZE_Pos) |
            (1U << OCTOSPI_CCR_ADMODE_Pos) |
            (1U << OCTOSPI_CCR_IMODE_Pos);
        OCTOSPI1->TCR = 0U;
        OCTOSPI1->IR = 0x03U;

        if(sr2 & 0x2)
        {
            klog("FSBL: QE set\n");

            while(OCTOSPI1->SR & OCTOSPI_SR_BUSY);
            OCTOSPI1->CR &= ~OCTOSPI_CR_EN;
            while(OCTOSPI1->SR & OCTOSPI_SR_BUSY);

            // set up quad IO read at 100 MHz, 1 AB 0xfX, 2x dummy bytes, instruction still on one line
            OCTOSPI1->DCR2 = (0U << OCTOSPI_DCR2_PRESCALER_Pos);        // 100 MHz
            OCTOSPI1->TCR |= OCTOSPI_TCR_SSHIFT;

            while(OCTOSPI1->SR & OCTOSPI_SR_BUSY);
            OCTOSPI1->CR |= OCTOSPI_CR_EN;

            // training
            /* For frequencies greater or equal to 50MHz, procedure is given below:
                1. When rst_n is low, DLYB lock FSM is put in Idle mode. Note that dll_start_lock must be
                set to ‘0’ during hard reset.
                2. Once the input signal dll_clk is stable (after a frequency switch for example) the lock
                sequence can be started by setting dll_start_lock to ‘1’ and maintain this value.
                3. During lock sequence DLYB can not be used until dll_locked flag is set to ‘1’.
                4. Once it is done, delay line for master and slaves are PVT compensated (tracking V and
                T evolution but not a frequency switch).
                5. Then the phase can be programmed using tx_ph_select[5:0] and rx_ph_select[5:0]
                input signals. The selected delays are applied to TX output clock and RX output clock
                once tx_ph_select_ack and rx_ph_select_ack are set to ‘1’ respectively.
                6. If another lock sequence is needed, for a frequency switch for example, dll_start_lock
                signal must go to ‘0’ and sequence can be restarted with step 2. */
            
            static uint8_t mfg_dev[32 * 256];
            OCTOSPI1->DCR1 &= ~OCTOSPI_DCR1_DLYBYP;
            RCC->OSPI1CFGR &= ~RCC_OSPI1CFGR_OSPI1DLLRST;

            unsigned int start_good = ~0U;
            unsigned int end_good = ~0U;

            for(unsigned int tap = 0; tap < 32; tap++)
            {
                SYSCFG->DLYBOS1CR = 0;
                __asm__ volatile("dsb sy\n" ::: "memory");
                SYSCFG->DLYBOS1CR = (tap << SYSCFG_DLYBOS1CR_RX_TAP_SEL_Pos) |
                    SYSCFG_DLYBOS1CR_EN;
                __asm__ volatile("dsb sy\n" ::: "memory");
                while(!(SYSCFG->DLYBOS1SR & SYSCFG_DLYBOS1SR_LOCK));

                ospi_ind_read(OCTOSPI1, 256, 0x94, 0, 0xf0f0f0f0,
                    (3U << OCTOSPI_CCR_DMODE_Pos) |
                    (0U << OCTOSPI_CCR_ABSIZE_Pos) |
                    (3U << OCTOSPI_CCR_ABMODE_Pos) |
                    (2U << OCTOSPI_CCR_ADSIZE_Pos) |
                    (3U << OCTOSPI_CCR_ADMODE_Pos) |
                    (1U << OCTOSPI_CCR_IMODE_Pos)
                    , 4, &mfg_dev[tap * 256]);

                bool is_good = true;
                for(unsigned int i = 0; i < 128; i++)
                {
                    if(mfg_dev[i * 2 + tap * 256] != 0xef || mfg_dev[i * 2 + 1 + tap * 256] != 0x15)
                    {
                        is_good = false;
                        break;
                    }
                }

                if(is_good)
                {
                    if(start_good == ~0U)
                        start_good = tap;
                    end_good = tap;
                }
            }

            if(start_good == ~0U)
            {
                klog("FSBL: QPI: fail tuning, try DPI\n");
                
                // set up dual IO read
                OCTOSPI1->DCR2 = (0U << OCTOSPI_DCR2_PRESCALER_Pos);        // 100 MHz
                OCTOSPI1->TCR |= OCTOSPI_TCR_SSHIFT;

                static uint8_t mfg_dev2[32 * 256];
                OCTOSPI1->DCR1 &= ~OCTOSPI_DCR1_DLYBYP;
                RCC->OSPI1CFGR &= ~RCC_OSPI1CFGR_OSPI1DLLRST;

                start_good = ~0U;
                end_good = ~0U;

                for(unsigned int tap = 0; tap < 32; tap++)
                {
                    SYSCFG->DLYBOS1CR = 0;
                    __asm__ volatile("dsb sy\n" ::: "memory");
                    SYSCFG->DLYBOS1CR = (tap << SYSCFG_DLYBOS1CR_RX_TAP_SEL_Pos) |
                        SYSCFG_DLYBOS1CR_EN;
                    __asm__ volatile("dsb sy\n" ::: "memory");
                    while(!(SYSCFG->DLYBOS1SR & SYSCFG_DLYBOS1SR_LOCK));

                    ospi_ind_read(OCTOSPI1, 256, 0x92, 0, 0xf0f0f0f0, 
                        (2U << OCTOSPI_CCR_DMODE_Pos) |
                        (0U << OCTOSPI_CCR_ABSIZE_Pos) |
                        (2U << OCTOSPI_CCR_ABMODE_Pos) |
                        (2U << OCTOSPI_CCR_ADSIZE_Pos) |
                        (2U << OCTOSPI_CCR_ADMODE_Pos) |
                        (1U << OCTOSPI_CCR_IMODE_Pos)
                        , 0, &mfg_dev2[tap * 256]);

                    bool is_good = true;
                    for(unsigned int i = 0; i < 128; i++)
                    {
                        if(mfg_dev2[i * 2 + tap * 256] != 0xef || mfg_dev2[i * 2 + 1 + tap * 256] != 0x15)
                        {
                            is_good = false;
                            //klog("DPI: fail tuning for %u at %u\n", tap, i);
                            break;
                        }
                    }

                    if(is_good)
                    {
                        if(start_good == ~0U)
                            start_good = tap;
                        end_good = tap;
                    }
                }

                if(start_good == ~0U)
                {
                    // fail tuning
                    klog("FSBL: DPI: fail tuning, revert to SPI\n");
                    OCTOSPI1->CCR = (1U << OCTOSPI_CCR_DMODE_Pos) |
                        (0U << OCTOSPI_CCR_ABMODE_Pos) |
                        (2U << OCTOSPI_CCR_ADSIZE_Pos) |
                        (1U << OCTOSPI_CCR_ADMODE_Pos) |
                        (1U << OCTOSPI_CCR_IMODE_Pos);
                    OCTOSPI1->TCR = 0U;
                    OCTOSPI1->IR = 0x03U;
                    OCTOSPI1->DCR2 = (1U << OCTOSPI_DCR2_PRESCALER_Pos);        // 100 MHz/2 => 50 MHz
                }
                else
                {
                    klog("FSBL: DPI: pass tuning for (%u, %u), set to %u\n", start_good, end_good,
                        (start_good + end_good) / 2);
                    SYSCFG->DLYBOS1CR = 0;
                    __asm__ volatile("dsb sy\n" ::: "memory");
                    SYSCFG->DLYBOS1CR = (((start_good + end_good) / 2) << SYSCFG_DLYBOS1CR_RX_TAP_SEL_Pos) |
                        SYSCFG_DLYBOS1CR_EN;
                    __asm__ volatile("dsb sy\n" ::: "memory");
                    while(!(SYSCFG->DLYBOS1SR & SYSCFG_DLYBOS1SR_LOCK));

                    OCTOSPI1->CCR = (2U << OCTOSPI_CCR_DMODE_Pos) |
                        (0U << OCTOSPI_CCR_ABSIZE_Pos) |
                        (2U << OCTOSPI_CCR_ABMODE_Pos) |
                        (2U << OCTOSPI_CCR_ADSIZE_Pos) |
                        (2U << OCTOSPI_CCR_ADMODE_Pos) |
                        (1U << OCTOSPI_CCR_IMODE_Pos);
                    OCTOSPI1->IR = 0xbb;
                    OCTOSPI1->ABR = 0xf0;
                    OCTOSPI1->TCR = (OCTOSPI1->TCR & ~OCTOSPI_TCR_DCYC_Msk) |
                        (0U << OCTOSPI_TCR_DCYC_Pos);   // 0 dummy bytes for dual IO
                }
            }
            else
            {
                klog("FSBL: QPI: pass tuning for (%u, %u), set to %u\n", start_good, end_good,
                        (start_good + end_good) / 2);
                SYSCFG->DLYBOS1CR = 0;
                __asm__ volatile("dsb sy\n" ::: "memory");
                SYSCFG->DLYBOS1CR = (((start_good + end_good) / 2) << SYSCFG_DLYBOS1CR_RX_TAP_SEL_Pos) |
                    SYSCFG_DLYBOS1CR_EN;
                __asm__ volatile("dsb sy\n" ::: "memory");
                while(!(SYSCFG->DLYBOS1SR & SYSCFG_DLYBOS1SR_LOCK));

                OCTOSPI1->CCR = (3U << OCTOSPI_CCR_DMODE_Pos) |
                    (0U << OCTOSPI_CCR_ABSIZE_Pos) |
                    (3U << OCTOSPI_CCR_ABMODE_Pos) |
                    (2U << OCTOSPI_CCR_ADSIZE_Pos) |
                    (3U << OCTOSPI_CCR_ADMODE_Pos) |
                    (1U << OCTOSPI_CCR_IMODE_Pos);
                OCTOSPI1->IR = 0xeb;
                OCTOSPI1->ABR = 0xf0f0f0;
                OCTOSPI1->TCR = (OCTOSPI1->TCR & ~OCTOSPI_TCR_DCYC_Msk) |
                    (4U << OCTOSPI_TCR_DCYC_Pos);   // 2 bytes = 4 clock cycles at QPI
            }

            while(OCTOSPI1->SR & OCTOSPI_SR_BUSY);
        }
    }


    OCTOSPI1->CR = (3U << OCTOSPI_CR_FMODE_Pos) |
        OCTOSPI_CR_EN;

    klog("FSBL: starting SSBL\n");

    // Set up VDERAM for access by SSBL-a
    RCC->VDERAMCFGR |= RCC_VDERAMCFGR_VDERAMEN;
    (void)RCC->VDERAMCFGR;
    SYSCFG->VDERAMCR |= SYSCFG_VDERAMCR_VDERAM_EN;  // allocate to system rather than VDEC
    (void)SYSCFG->VDERAMCR;
    /* allocate last block of last page for access from non-secure world (for _cur_ms and other things)
        this is the last 512 bytes @ 0x0e0bfe00
    */
    for(unsigned int i = 0; i < 31; i++)
    {
        RISAB6->PGSECCFGR[i] = 0xffU;       // secure access only - required to execute AP2 code from here
    }
    RISAB6->PGSECCFGR[31] = 0x7fU;
    RISAB6->CR |= RISAB_CR_SRWIAD;          // allow secure data access back to the last page
    
    EV_ORANGE.set_as_output();

    // say hi
    for(int n = 0; n < 10; n++)
    {
        EV_ORANGE.set();
        for(int i = 0; i < 2500000; i++);
        EV_ORANGE.clear();
        for(int i = 0; i < 2500000; i++);
    }

    void (*ssbl)(uint32_t bootrom_val) = (void (*)(uint32_t))0x60020000;
    extern uint64_t AP_Target;
    AP_Target = 0x60020000;
    __asm__ volatile("sev \n" ::: "memory");
    ssbl(bootrom_val);

    return 0;
}

timespec clock_cur()
{
    return { 0, 0 };
}
