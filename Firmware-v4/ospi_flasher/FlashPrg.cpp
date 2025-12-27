/* -----------------------------------------------------------------------------
 * Copyright (c) 2014 - 2018 ARM Ltd.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software. Permission is granted to anyone to use this
 * software for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software in
 *    a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *
 * $Date:        11. December 2019
 * $Revision:    V1.2.1
 *
 * Project:      Flash Programming Functions for STMicroelectronics STM32F4xx Flash
 * --------------------------------------------------------------------------- */

/* History:
 *  Version 1.2.1
 *    Corrected STM32F42xxx_43xxx_OPT Algorithm
 *  Version 1.2.0
 *    Added STM32F4xx_1536 Algorithm
 *  Version 1.1.0
 *    Added STM32F4xx_1024dual Algorithm
 *  Version 1.0.2
 *    Added more Option Byte Algorithm
 *  Version 1.0.1
 *    Added STM32F411xx Option Byte Algorithm
 *  Version 1.0.0
 *    Initial release
 */

#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include "FlashOS.h"        // FlashOS Structures
#include "stm32mp2xx.h"
#include "pins.h"
#include "clocks.h"
#include "logger.h"
#include "w25.h"

static const constexpr pin USART6_TX { GPIOJ, 5, 6 };

// some standard ccr values
static const constexpr uint32_t ccr_spi_no_ab_no_addr =
    (1U << OCTOSPI_CCR_DMODE_Pos) |
    (1U << OCTOSPI_CCR_IMODE_Pos);
static const constexpr uint32_t ccr_spi_no_ab_no_data =
    (1U << OCTOSPI_CCR_ADMODE_Pos) |
    (2U << OCTOSPI_CCR_ADSIZE_Pos) |
    (1U << OCTOSPI_CCR_IMODE_Pos);
static const constexpr uint32_t ccr_spi_no_ab =
    (1U << OCTOSPI_CCR_DMODE_Pos) |
    (1U << OCTOSPI_CCR_ADMODE_Pos) |
    (2U << OCTOSPI_CCR_ADSIZE_Pos) |
    (1U << OCTOSPI_CCR_IMODE_Pos);
static const constexpr uint32_t ccr_spi_no_ab_no_addr_no_data =
    (1U << OCTOSPI_CCR_IMODE_Pos);

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

static int set_memmap()
{
    // set memory mapped mode
    OCTOSPI1->CCR = (1U << OCTOSPI_CCR_DMODE_Pos) |
        (0U << OCTOSPI_CCR_ABMODE_Pos) |
        (2U << OCTOSPI_CCR_ADSIZE_Pos) |
        (1U << OCTOSPI_CCR_ADMODE_Pos) |
        (1U << OCTOSPI_CCR_IMODE_Pos);
    OCTOSPI1->TCR = 0U;
    OCTOSPI1->IR = 0x03U;

    OCTOSPI1->CR = (3U << OCTOSPI_CR_FMODE_Pos) |
        OCTOSPI_CR_EN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    return 0;
}

extern "C"
int Init (uint32_t adr, uint32_t clk, uint32_t fnc)
{
    RCC->USART6CFGR |= RCC_USART6CFGR_USART6EN;
    (void)RCC->USART6CFGR;
    RCC->GPIOJCFGR |= RCC_GPIOJCFGR_GPIOxEN;
    (void)RCC->GPIOJCFGR;
    RCC->GPIODCFGR |= RCC_GPIODCFGR_GPIOxEN;
    (void)RCC->GPIODCFGR;
    RCC->OSPI1CFGR |= RCC_OSPI1CFGR_OSPI1RST;
    (void)RCC->OSPI1CFGR;
    RCC->OSPI1CFGR = (RCC->OSPI1CFGR & ~RCC_OSPI1CFGR_OSPI1RST) |
        RCC_OSPI1CFGR_OSPI1EN;
    (void)RCC->OSPI1CFGR;
    RCC->OSPIIOMCFGR |= RCC_OSPIIOMCFGR_OSPIIOMRST;
    (void)RCC->OSPIIOMCFGR;
    RCC->OSPIIOMCFGR = (RCC->OSPIIOMCFGR & ~RCC_OSPIIOMCFGR_OSPIIOMRST) |
        RCC_OSPIIOMCFGR_OSPIIOMEN;


    // Set up USART6 as TX only
    USART6_TX.set_as_af();

    // USART6 is clocked with HSI64 direct by default
    USART6->CR1 = 0;
    USART6->PRESC = 0;
    USART6->BRR = 64000000UL / 115200UL;
    USART6->CR2 = 0;
    USART6->CR3 = 0;
    USART6->CR1 = USART_CR1_FIFOEN | USART_CR1_TE | USART_CR1_UE;

    // say hi
    klog("FlashPrg: INIT\n");

    // Enable VDDIO3 power
    PWR->CR1 |= PWR_CR1_VDDIO3VMEN;
    while(!(PWR->CR1 & PWR_CR1_VDDIO3RDY));
    PWR->CR1 |= PWR_CR1_VDDIO3SV;
    klog("FlashPrg: VDDIO3 ready\n");

    // Enable OSPI1 pins
    const constexpr pin OSPI_PINS[] = {
        { GPIOD, 0, 10 },
        { GPIOD, 3, 10 },
        { GPIOD, 4, 10 },
        { GPIOD, 5, 10 }, 
        { GPIOD, 6, 10 },
        { GPIOD, 7, 10 },
    };
    for(const auto &p : OSPI_PINS)
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

    OCTOSPI1->CR |= OCTOSPI_CR_EN;

    // read jedec id
    uint8_t jedec_id[3];
    auto nb = ospi_ind_read(OCTOSPI1, 3, 0x9f, 0, 0, ccr_spi_no_ab_no_addr, 0, jedec_id);
    klog("FlashPrg: jedec_read: %u, [ %x, %x, %x ]\n",
        nb, jedec_id[0], jedec_id[1], jedec_id[2]);
    if(nb != 3 || jedec_id[0] != 0xef || (jedec_id[1] != 0x40 && jedec_id[1] != 0x70) || jedec_id[2] != 0x16)
        return -1;

    klog("FlashPrg: Init success\n");

    set_memmap();

    return 0;
}

extern "C"
int SEGGER_FL_Prepare(uint32_t, uint32_t, uint32_t)
{
    return Init(0, 0, 0);
}

extern "C"
int UnInit(uint32_t fnc)
{
    klog("FlashPrg: Uninit(%u)\n", fnc);
    set_memmap();
#if 0
    RCC->AHB5ENR &= ~RCC_AHB5ENR_XSPI2EN;
    (void)RCC->AHB5ENR;
    RCC->AHB5RSTR = RCC_AHB5RSTR_XSPI2RST;
    (void)RCC->AHB5RSTR;
    RCC->AHB5RSTR = 0;
    (void)RCC->AHB5RSTR;
#endif
    return 0;
}

extern "C"
int SEGGER_FL_Restore(uint32_t, uint32_t, uint32_t)
{
    klog("FlashPrg: SEGGER_FL_Restore\n");
    return UnInit(0);
}

#if 0
static uint32_t qspi_read_status()
{
    xspi_ind_write16(XSPI2, 0x555*2, 0x70);
    return xspi_ind_read16(XSPI2, 0);
}
#endif

static uint32_t ospi_read_status()
{
    uint8_t sr1;
    auto ret = ospi_ind_read(OCTOSPI1, 1, 0x05, 0, 0, ccr_spi_no_ab_no_addr, 0, &sr1);
    if(ret == 1)
        return (uint32_t)sr1;
    else
        return 0;
}

static int ospi_write_enable()
{
    return (ospi_ind_write<uint8_t>(OCTOSPI1, 0, 0x06, 0, 0, ccr_spi_no_ab_no_addr_no_data, 0, nullptr) == 0) ? 0 : -1;
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

int EraseSector(uint32_t addr)
{
//    klog("FlashPrg: EraseSector(%x)\n", addr);

    addr -= 0x60000000;

    auto weret = ospi_write_enable();
    if(weret != 0)
    {
        klog("FlashPrg: EraseSector: ospi_write_enable() failed: %d\n");
        set_memmap();
        return 1;
    }

    // wait ready
    auto sr = ospi_wait_not_busy();
    if(!(sr & 0x2U))
    {
        klog("FlashPrg: EraseSector: WEL not set: %x\n", sr);
        set_memmap();
        return 1;
    }

    ospi_ind_write<uint8_t>(OCTOSPI1, 0, 0x20, addr, 0, ccr_spi_no_ab_no_data, 0, nullptr);

    sr = ospi_wait_not_busy();
    //klog("FlashPrg: EraseSector: complete (%x)\n", sr);

    set_memmap();

    if(sr & 0x3)
        return 1;
    return 0;
}

int EraseBlock(uint32_t addr, uint32_t size)
{
//    klog("FlashPrg: EraseBlock(%x, %u)\n", addr, size);

    addr -= 0x60000000;

    auto weret = ospi_write_enable();
    if(weret != 0)
    {
        klog("FlashPrg: EraseBlock: ospi_write_enable() failed: %d\n");
        set_memmap();
        return 1;
    }

    // wait ready
    auto sr = ospi_wait_not_busy();
    if(!(sr & 0x2U))
    {
        klog("FlashPrg: EraseBlock: WEL not set: %x\n", sr);
        set_memmap();
        return 1;
    }

    uint8_t cmd;
    switch(size)
    {
        case 65536:
            cmd = 0xd8;
            break;
        case 32768:
            cmd = 0x52;
            break;
        default:
            klog("FlashPrg: EraseBlock: invalid block size: %u\n", size);
            return 1;
    }

    ospi_ind_write<uint8_t>(OCTOSPI1, 0, cmd, addr, 0, ccr_spi_no_ab_no_data, 0, nullptr);

    sr = ospi_wait_not_busy();
    //klog("FlashPrg: EraseBlock: complete (%x)\n", sr);

    set_memmap();

    if(sr & 0x3)
        return 1;
    return 0;
}

extern "C"
int SEGGER_FL_Erase(uint32_t sector_addr, uint32_t sector_idx, uint32_t nsects)
{
    klog("FlashPrg: SEGGER_FL_Erase(%x, %u, %u)\n", sector_addr, sector_idx, nsects);
    if(((sector_addr - OSPIADDR) / ERASE_SECTOR_SIZE) != sector_idx)
    {
        klog("FlashPrg: SEGGER_FL_Erase: addr/idx mismatch\n");
        return 1;
    }

    while(nsects)
    {
        // use 64 kiB block if possible
        if(((sector_addr % 65536) == 0) && (nsects >= (65536 / ERASE_SECTOR_SIZE)))
        {
            auto ret = EraseBlock(sector_addr, 65536);
            if(ret != 0)
                return ret;
            sector_addr += 65536;
            nsects -= (65536 / ERASE_SECTOR_SIZE);
        }
        else if(((sector_addr % 32768) == 0) && (nsects >= (32768 / ERASE_SECTOR_SIZE)))
        {
            auto ret = EraseBlock(sector_addr, 32768);
            if(ret != 0)
                return ret;
            sector_addr += 32768;
            nsects -= (32768 / ERASE_SECTOR_SIZE);
        }
        else
        {
            auto ret = EraseSector(sector_addr);
            if(ret != 0)
                return ret;
            sector_addr += ERASE_SECTOR_SIZE;
            nsects--;
        }
    }

    return 0;
}

static int pp_int(uint32_t devaddr, size_t n, const unsigned char *buf)
{
    auto weret = ospi_write_enable();
    if(weret != 0)
    {
        klog("FlashPrg: pp_int: ospi_write_enable() failed: %d\n");
        return -1;
    }

    // wait ready
    auto sr = ospi_wait_not_busy();
    if(!(sr & 0x2U))
    {
        klog("FlashPrg: pp_int: WEL not set: %x\n", sr);
        return -1;
    }

    n = (n < PROGRAM_PAGE_SIZE) ? n : PROGRAM_PAGE_SIZE;

    auto bw = ospi_ind_write(OCTOSPI1, n, 0x02, devaddr, 0, ccr_spi_no_ab, 0, buf);
    
    ospi_wait_not_busy();

    return (bw == (int)n) ? 0 : 1;
}

int ProgramPage (uint32_t addr, uint32_t sz, unsigned char *buf)
{
//    klog("FlashPrg: ProgramPage(%x, %u, %p)\n", addr, sz, buf);

    addr -= OSPIADDR;

    while(sz)
    {
        auto cur_sz = (sz < PROGRAM_PAGE_SIZE) ? sz : PROGRAM_PAGE_SIZE;
        auto ret = pp_int(addr, cur_sz, buf);
        if(ret < 0)
        {
            set_memmap();
            return 1;
        }

        sz -= cur_sz;
        addr += cur_sz;
        buf += cur_sz;
    }

    set_memmap();
    return 0;
}

extern "C"
int SEGGER_FL_Program(uint32_t daddr, uint32_t numbytes, uint8_t *src)
{
//    klog("FlashPrg: SEGGER_FL_Program(%x, %u, %p)\n", daddr, numbytes, src);

    while(numbytes)
    {
        auto ret = ProgramPage(daddr, PROGRAM_PAGE_SIZE, src);
        if(ret != 0)
            return ret;
        daddr += PROGRAM_PAGE_SIZE;
        src += PROGRAM_PAGE_SIZE;
        numbytes -= PROGRAM_PAGE_SIZE;
    }

    return 0;
}

uint32_t Verify(uint32_t addr, uint32_t size, unsigned char *buf)
{
//    klog("FlashPrg: Verify(%x, %u, %p)\n", addr, size, buf);

    set_memmap();

    uint32_t i = 0;
    for(; i < (size & ~0x3U); i += 4)
    {
        if(*(uint32_t *)(addr + i) != *(uint32_t *)(buf + i))
            return addr + i;
    }
    for(; i < (size & ~0x1U); i += 2)
    {
        if(*(uint16_t *)(addr + i) != *(uint16_t *)(buf + i))
            return addr + i;
    }
    for(; i < (size); i++)
    {
        if(*(uint8_t *)(addr + i) != *(uint8_t *)(buf + i))
            return addr + i;
    }
    return addr + size;
}

int EraseChip()
{
    klog("FlashPrg: EraseChip()\n");
    return 1;
#if 0
    xspi_ind_write16(XSPI2, 0x555*2, 0xaa);
    xspi_ind_write16(XSPI2, 0x2aa*2, 0x55);
    xspi_ind_write16(XSPI2, 0x555*2, 0x80);
    xspi_ind_write16(XSPI2, 0x555*2, 0xaa);
    xspi_ind_write16(XSPI2, 0x2aa*2, 0x55);
    xspi_ind_write16(XSPI2, 0x555*2, 0x10);

    // poll status until ready
    uint16_t sr = 0xffff;
    do
        IWDG->KR = 0xAAAA;                              // Reload IWDG
    while (((sr = qspi_read_status()) & (1U << 7)) == 0);

    // clear status register
    xspi_ind_write16(XSPI2, 0x555*2, 0x71);

    // Return to memory mapped mode
    //while(XSPI2->SR & XSPI_SR_BUSY);
    //XSPI2->CR = (XSPI2->CR & ~XSPI_CR_FMODE_Msk) |
    //   (3U << XSPI_CR_FMODE_Pos);

    // check for error
    if(sr == 0xffff)
        return 1;
    if(sr & (1UL << 5))
    {
        if(sr & (1UL << 1))
            return 1;
        return 1;
    }
#endif

    return 0;
}

timespec clock_cur()
{
    return timespec{};
}
