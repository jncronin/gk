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

static const constexpr pin USART6_TX { GPIOJ, 5, 6 };

#if 0
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
#endif

extern "C"
int Init (uint32_t adr, uint32_t clk, uint32_t fnc)
{
    RCC->USART6CFGR |= RCC_USART6CFGR_USART6EN;
    (void)RCC->USART6CFGR;
    RCC->GPIOJCFGR |= RCC_GPIOJCFGR_GPIOxEN;
    (void)RCC->GPIOJCFGR;
    RCC->GPIODCFGR |= RCC_GPIODCFGR_GPIOxEN;
    (void)RCC->GPIODCFGR;

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


    return 0;
}

extern "C"
int UnInit(uint32_t fnc)
{
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

#if 0
static uint32_t qspi_read_status()
{
    xspi_ind_write16(XSPI2, 0x555*2, 0x70);
    return xspi_ind_read16(XSPI2, 0);
}
#endif

int EraseSector(uint32_t addr)
{
#if 0
    addr -= 0x70000000;
    if(addr > 64*1024*1024)
        return 1;

    IWDG->KR = 0xAAAA;                                  // Reload IWDG
  
    xspi_ind_write16(XSPI2, 0x555*2, 0xaa);
    xspi_ind_write16(XSPI2, 0x2aa*2, 0x55);
    xspi_ind_write16(XSPI2, 0x555*2, 0x80);
    xspi_ind_write16(XSPI2, 0x555*2, 0xaa);
    xspi_ind_write16(XSPI2, 0x2aa*2, 0x55);
    xspi_ind_write16(XSPI2, addr, 0x30);

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
    //    (3U << XSPI_CR_FMODE_Pos);

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

#if 0
static int pp_int(uint32_t devaddr, size_t n, const unsigned char *buf)
{
    // program a single 512 byte/256 word page
    IWDG->KR = 0xAAAA;                                  // Reload IWDG

    // Write to Buffer, sector address
    xspi_ind_write16(XSPI2, 0x555*2, 0xaa);
    xspi_ind_write16(XSPI2, 0x2aa*2, 0x55);
    xspi_ind_write16(XSPI2, /*sector address*/ devaddr, 0x25);

    // 16-bit Word count - 1, sector address
    xspi_ind_write16(XSPI2, /*sector address*/ devaddr, 255);

    // Address/Data pair, *256
    for(int idx = 0; idx < 256; idx++)
    {
        uint16_t data;
        if((idx * 2) <= (n-2))
            data = *(uint16_t *)(buf + idx*2);
        else if((idx * 2) <= (n - 1))
            data = 0xff00U | (uint16_t)*(uint8_t *)(buf + idx*2);
        else
            data = 0xffffU;
        xspi_ind_write16(XSPI2, /*data address*/ devaddr+idx*2, data);
    }

    // Program buffer to flash confirm, sector address
    xspi_ind_write16(XSPI2, /*sector address*/ devaddr, 0x29);

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
    //    (3U << XSPI_CR_FMODE_Pos);

    // check for error
    if(sr == 0xffff)
        return -1;
    if(sr & (1UL << 4))
    {
        if(sr & (1UL << 3))
            return -2;
        else if(sr & (1UL << 1))
            return -3;
        return -4;
    }

    return 0;
}
#endif

int ProgramPage (uint32_t addr, uint32_t sz, unsigned char *buf)
{
#if 0
    addr -= 0x70000000;
    if(addr > 64*1024*1024)
        return 1;

    IWDG->KR = 0xAAAA;                                  // Reload IWDG

    while(sz)
    {
        auto cur_sz = (sz < 512) ? sz : 512;
        auto ret = pp_int(addr, cur_sz, buf);
        if(ret < 0)
        {
            return 1;
        }

        sz -= cur_sz;
        addr += cur_sz;
        buf += cur_sz;
    }
#endif

    return 0;
}

uint32_t Verify(uint32_t addr, uint32_t size, unsigned char *buf)
{
#if 0
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
#endif
    return 0;
}

int EraseChip()
{
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
