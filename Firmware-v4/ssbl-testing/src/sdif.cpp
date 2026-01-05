#include "stm32mp2xx.h"
#include "clocks.h"
#include "sdif.h"
#include <cstdio>
#include "pins.h"
#include "osmutex.h"
#include "vmem.h"
#include "gic.h"
#include "pmic.h"
#include "logger.h"
#include "smc.h"

#define SDMMC1_VMEM ((SDMMC_TypeDef *)PMEM_TO_VMEM(SDMMC1_BASE))
#define SDMMC2_VMEM ((SDMMC_TypeDef *)PMEM_TO_VMEM(SDMMC2_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define GPIOE_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOE_BASE))
#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))
#define RIFSC_VMEM (PMEM_TO_VMEM(RIFSC_BASE))

#define DEBUG_SD    0
#define PROFILE_SDT 0

extern char _ssdt_data, _esdt_data;

SDIF sdmmc[2] = { SDIF(), SDIF() };

static void delay_ms(unsigned int ms)
{
    udelay(ms * 1000);
}

static constexpr pin sdmmc1_pins[] =
{
    { GPIOE_VMEM, 0, 10 },
    { GPIOE_VMEM, 1, 10 },
    { GPIOE_VMEM, 2, 10 },
    { GPIOE_VMEM, 3, 10 },
    { GPIOE_VMEM, 4, 10 },
    { GPIOE_VMEM, 5, 10 }
};

static constexpr pin sdmmc2_pins[] =
{
    { GPIOE_VMEM, 8, 12 },
    { GPIOE_VMEM, 11, 12 },
    { GPIOE_VMEM, 12, 12 },
    { GPIOE_VMEM, 13, 12 },
    { GPIOE_VMEM, 14, 12 },
    { GPIOE_VMEM, 15, 12 }
};

enum StatusFlags { CCRCFAIL = 1, DCRCFAIL = 2, CTIMEOUT = 4, DTIMEOUT = 8,
    TXUNDERR = 0x10, RXOVERR = 0x20, CMDREND = 0x40, CMDSENT = 0x80,
    DATAEND = 0x100, /* reserved */ DBCKEND = 0x400, DABORT = 0x800,
    DPSMACT = 0x1000, CPSMACT = 0x2000, TXFIFOHE = 0x4000, RXFIFOHF = 0x8000,
    TXFIFOF = 0x10000, RXFIFOF = 0x20000, TXFIFOE = 0x40000, RXFIFOE = 0x80000,
    SDIOIT = 0x400000
};

void init_sdmmc1()
{
    RCC_VMEM->GPIOECFGR |= RCC_GPIOECFGR_GPIOxEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    for(const auto &p : sdmmc1_pins)
        p.set_as_af();

    // Clocks to SDMMC1/2 (crossbar 51 + 52)
    // Use PLL4 = 1200 MHz / 6 -> 200 MHz SDCLK
    RCC_VMEM->PREDIVxCFGR[51] = 0;
    RCC_VMEM->FINDIVxCFGR[51] = 0x45;
    RCC_VMEM->XBARxCFGR[51] = 0x40;

    // Reset SDMMC
    RCC_VMEM->SDMMC1CFGR |= RCC_SDMMC1CFGR_SDMMC1RST | RCC_SDMMC1CFGR_SDMMC1DLLRST;
    (void)RCC_VMEM->SDMMC1CFGR;
    RCC_VMEM->SDMMC1CFGR &= ~(RCC_SDMMC1CFGR_SDMMC1RST | RCC_SDMMC1CFGR_SDMMC1DLLRST);
    (void)RCC_VMEM->SDMMC1CFGR;

    // Clock SDMMC
    RCC_VMEM->SDMMC1CFGR |= RCC_SDMMC1CFGR_SDMMC1EN;
    (void)RCC_VMEM->SDMMC1CFGR;

    RCC_VMEM->GPIOECFGR |= RCC_GPIOECFGR_GPIOxEN;;
    (void)RCC_VMEM->GPIOFCFGR;

    /* Give SDMMC1 access to DDR via RIFSC.  Use same CID as CA35/secure/priv for the master interface ID 1 */
    *(volatile uint32_t *)(RIFSC_VMEM + 0xc10 + 1 * 0x4) =
        (1UL << 2) |                // use cid specified here
        (1UL << 4) |                // CID 1
        (1UL << 8) |                // secure
        (1UL << 9);                 // priv
    /* The IDMA is forced to non-secure if its RISUP (RISUP 76) is programmed as non-secure,
        therefore set as secure here */
    const uint32_t risup = 76;
    const uint32_t risup_word = risup / 32;
    const uint32_t risup_bit = risup % 32;
    auto risup_reg = (volatile uint32_t *)(RIFSC_VMEM + 0x10 + 0x4 * risup_word);
    auto old_val = *risup_reg;
    old_val |= 1U << risup_bit;
    *risup_reg = old_val;
    __asm__ volatile("dmb sy\n" ::: "memory");

    sdmmc[0].iface = SDMMC1_VMEM;
    sdmmc[0].iface_id = 1;
    // functions to set power
    sdmmc[0].supply_off = []() { return smc_set_power(SMC_Power_Target::SDCard, 0) == 0 ? 0 : -1; };
    sdmmc[0].supply_on = []() { return smc_set_power(SMC_Power_Target::SDCard, 3300) == 3300 ? 0 : -1; };
    sdmmc[0].io_1v8 = []() { return smc_set_power(SMC_Power_Target::SDCard_IO, 1800) == 1800 ? 0 : -1; };
    sdmmc[0].io_3v3 = []() { return smc_set_power(SMC_Power_Target::SDCard_IO, 3300) == 3300 ? 0 : -1; };
    sdmmc[0].pwr_valid_reg = &PWR_VMEM->CR8;
    sdmmc[0].rcc_reg = &RCC_VMEM->SDMMC1CFGR;
}

void init_sdmmc2()
{
    RCC_VMEM->GPIOECFGR |= RCC_GPIOECFGR_GPIOxEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    for(const auto &p : sdmmc2_pins)
        p.set_as_af();

    // Clocks to SDMMC1/2 (crossbar 51 + 52)
    // Use PLL4 = 1200 MHz / 6 -> 200 MHz SDCLK
    RCC_VMEM->PREDIVxCFGR[52] = 0;
    RCC_VMEM->FINDIVxCFGR[52] = 0x45;
    RCC_VMEM->XBARxCFGR[52] = 0x40;

    // Reset SDMMC
    RCC_VMEM->SDMMC2CFGR |= RCC_SDMMC2CFGR_SDMMC2RST | RCC_SDMMC2CFGR_SDMMC2DLLRST;
    (void)RCC_VMEM->SDMMC2CFGR;
    RCC_VMEM->SDMMC2CFGR &= ~(RCC_SDMMC2CFGR_SDMMC2RST | RCC_SDMMC2CFGR_SDMMC2DLLRST);
    (void)RCC_VMEM->SDMMC2CFGR;

    // Clock SDMMC
    RCC_VMEM->SDMMC2CFGR |= RCC_SDMMC2CFGR_SDMMC2EN;
    (void)RCC_VMEM->SDMMC2CFGR;

    /* Give SDMMC2 access to DDR via RIFSC.  Use same CID as CA35/secure/priv for the master interface ID 1 */
    *(volatile uint32_t *)(RIFSC_VMEM + 0xc10 + 2 * 0x4) =
        (1UL << 2) |                // use cid specified here
        (1UL << 4) |                // CID 1
        (1UL << 8) |                // secure
        (1UL << 9);                 // priv
    /* The IDMA is forced to non-secure if its RISUP (RISUP 77) is programmed as non-secure,
        therefore set as secure here */
    const uint32_t risup = 77;
    const uint32_t risup_word = risup / 32;
    const uint32_t risup_bit = risup % 32;
    auto risup_reg = (volatile uint32_t *)(RIFSC_VMEM + 0x10 + 0x4 * risup_word);
    auto old_val = *risup_reg;
    old_val |= 1U << risup_bit;
    *risup_reg = old_val;
    __asm__ volatile("dmb sy\n" ::: "memory");

    sdmmc[1].iface = SDMMC2_VMEM;
    sdmmc[1].iface_id = 2;
    sdmmc[1].default_io_voltage = 1800;
    sdmmc[1].default_supply_current = 2500;
    // functions to set power
    sdmmc[1].io_1v8 = []() { return smc_set_power(SMC_Power_Target::SDIO_IO, 1800) == 1800 ? 0 : -1; };
    sdmmc[1].io_3v3 = []() { return smc_set_power(SMC_Power_Target::SDIO_IO, 1800) == 1800 ? 0 : -1; };
    sdmmc[1].pwr_valid_reg = &PWR_VMEM->CR7;
    sdmmc[1].rcc_reg = &RCC_VMEM->SDMMC2CFGR;
}

int SDIF::reset()
{
    sd_ready = false;
    is_4bit = false;
    is_hc = false;
    tfer_inprogress = false;
    dma_ready = false;
    sd_multi = false;
    sd_status = 0;
    is_mem = false;
    is_sdio = false;
    sdio_n_extra_funcs = 0;
    cmd5_s18a = false;
    is_1v8 = false;

    *rcc_reg |= RCC_SDMMC1CFGR_SDMMC1RST | RCC_SDMMC1CFGR_SDMMC1DLLRST;
    (void)*rcc_reg;
    *rcc_reg &= ~(RCC_SDMMC1CFGR_SDMMC1RST | RCC_SDMMC1CFGR_SDMMC1DLLRST);
    (void)*rcc_reg;

    *rcc_reg |= RCC_SDMMC1CFGR_SDMMC1EN;
    (void)*rcc_reg;

    set_clock(SDCLK_IDENT);

    // power cycle card
    if(supply_off && supply_off())
    {
        klog("sd%d: failed to turn off card\n", iface_id);
        return -1;
    }
    *pwr_valid_reg &= ~PWR_CR8_VDDIO1VRSEL;
    __asm__ volatile("dsb sy\n" ::: "memory");
    udelay(10000);
    if(io_3v3 && io_3v3())
    {
        klog("sd%d: failed to turn on sd io 3.3V\n", iface_id);
        return -1;
    }
    *pwr_valid_reg |= PWR_CR8_VDDIO1SV;
    __asm__ volatile("dsb sy\n" ::: "memory");

    // set SDMMC state to power cycle (no drive on any pins)
    iface->POWER = 2UL << SDMMC_POWER_PWRCTRL_Pos;

    // wait 1 ms
    delay_ms(1);

    // enable card VCC and wait power ramp-up time
    if(supply_on && supply_on())
    {
        if(io_0) io_0();
        *pwr_valid_reg &= ~PWR_CR8_VDDIO1SV;
        klog("sd%d: failed to turn on card power\n", iface_id);
        return -1;
    }
    delay_ms(10);

    // disable SDMMC output
    iface->POWER = 0UL;

    // wait 1 ms
    delay_ms(1);

    // Enable clock to card
    iface->POWER = 3UL << SDMMC_POWER_PWRCTRL_Pos;

    // Wait 74 CK cycles = 0.37 ms at 200 kHz
    delay_ms(1);
    
    // Issue CMD0 (go idle state)
    auto ret = sd_issue_command(0, resp_type::None);
    if(ret != 0)
        return -1;

    // Issue CMD8 (bcr, argument = supply voltage, R7)
    uint32_t resp[4];
    ret = sd_issue_command(8, resp_type::R7, 0x1aa, resp);
    bool is_v2 = false;
    if(ret == CTIMEOUT)
    {
        is_v2 = false;
        klog("sd%d: CMD8 timed out - assuming <v2 card\n", iface_id);
    }
    else if(ret == 0)
    {
        if((resp[0] & 0x1ff) == 0x1aa)
        {
            is_v2 = true;
        }
        else
        {
            klog("sd%d: CMD8 invalid response %lx\n", resp[0], iface_id);
            return -1;
        }
    }
    else
        return -1;

    // Issue CMD5 to see if the card has SDIO
    ret = sd_issue_command(5, resp_type::R4, 0, resp);
    if(ret == 0)
    {
        // valid response - go through SDIO setup
        klog("sd%d: CMD5 response %8x\n",
            iface_id,
            resp[0]);

        
        // check OCR
        auto ocr = resp[0] & 0xffffffU;
        if((ocr & 0xff8000) != 0xff8000)
        {
            klog("sd%d: sdio: unsupported ocr: %x\n", iface_id, ocr);
        }
        else
        {
            // power up IO
            int nretries = 0;
            while(true)
            {
                auto arg = 0xff8000U;
                if(!vswitch_failed)
                    arg |= (1U << 24);
                ret = sd_issue_command(5, resp_type::R4, arg, resp);
                if(ret != 0)
                {
                    klog("sd%d: sdio: cmd5(%x) failed %d\n", iface_id, 0xff8000U, ret);
                    is_sdio = false;
                    break;
                }
                if(resp[0] & (1U << 31))
                {
                    // ready
                    sdio_n_extra_funcs = (resp[0] >> 28) & 0x7U;
                    is_mem = ((resp[0] >> 27) & 0x1) != 0;
                    is_sdio = true;
                    cmd5_s18a = ((resp[0] >> 24) & 0x1) != 0;       // later OR'd with ACMD41 S18A, if relevant

                    klog("sd%d: sdio with %u extra functions initialised\n", iface_id, sdio_n_extra_funcs);
                    break;
                }

                nretries++;
                if(nretries >= 10)
                {
                    is_sdio = false;
                    klog("sd%d: sdio: cmd5(%x) repeat count exceeded\n", iface_id, 0xff8000U);
                    break;
                }
            }
        }
    }
    else if(ret == CTIMEOUT)
    {
        klog("sd%d: CMD5 timeout - no IO function\n", iface_id);
        is_sdio = false;
    }
    else
    {
        klog("sd%d: CMD5 returned %x\n", iface_id, ret);
    }
    
    if(!is_sdio || is_mem)
    {
        // proceed to memory init ACMD41
        // Inquiry ACMD41 to get voltage window
        ret = sd_issue_command(55, resp_type::R1, 0, resp);
        if(ret != 0)
            return -1;
        
        ret = sd_issue_command(41, resp_type::R3, 0, resp);
        if(ret != 0)
            return -1;

        // We can promise ~3.1-3.4 V
        if(!(resp[0] & 0x380000))
        {
            klog("sd%d: invalid voltage range\n", iface_id);
            return -1;
        }

        // Send init ACMD41 repeatedly until ready
        bool is_ready = false;
        is_hc = false;

        if(vswitch_failed)
            is_1v8 = false;

        while(!is_ready)
        {
            uint32_t ocr = 0xff8000 | (is_v2 ? ((1UL << 30) | 
                (vswitch_failed ? 0UL : (1UL << 24))): 0);
            ret = sd_issue_command(55, resp_type::R1, 0, resp);
            if(ret != 0)
                return -1;
            
            ret = sd_issue_command(41, resp_type::R3, ocr, resp);
            if(ret != 0)
                return -1;
            
            if(resp[0] & (1UL << 31))
            {
                if(resp[0] & (1UL << 30))
                {
                    is_hc = true;
                }

                if(resp[0] & (1UL << 24))
                {
                    if(!vswitch_failed)
                        is_1v8 = true;
                }

                {
                    klog("sd%d: %s capacity card ready\n", iface_id,
                        is_hc ? "high" : "normal");
                    if(is_1v8)
                    {
                        klog("sd%d: 1.8v signalling support\n", iface_id);
                    }
                }
                is_ready = true;
                is_mem = true;
            }
        }
    }

    if(!is_sdio && !is_mem)
    {
        // card has no functions we support
        return -1;
    }

    // Check logical OR of ACMD41 S18A and CMD5 S18A
    if(!vswitch_failed &&
        (is_1v8 || cmd5_s18a) && io_1v8)
    {
        // proceed to voltage switch
        // send CMD11 to begin voltage switch sequence

        // PLSS p.47 and RM p.2186
        iface->POWER |= SDMMC_POWER_VSWITCHEN;
        
        ret = sd_issue_command(11, resp_type::R1, 0);
        if(ret != 0)
        {
            vswitch_failed = true;
            return -1;
        }

        klog("sd%d: CMD11 returned successfully\n", iface_id);

        // wait for CK_STOP
        while(!(iface->STA & SDMMC_STA_CKSTOP));
        iface->ICR = SDMMC_ICR_CKSTOPC;

        // sample BUSYD0
        auto busyd0 = iface->STA & SDMMC_STA_BUSYD0;
        if(!busyd0)
        {
            klog("sd%d: D0 high, aborting voltage switch\n", iface_id);
            vswitch_failed = true;
            return -1;
        }

        klog("sd%d: D0 low - proceeding with vswitch\n", iface_id);
        if(io_1v8() != 0)
        {
            klog("sd%d: voltage regulator change failed\n", iface_id);
            vswitch_failed = true;
            return -1;
        }

        delay_ms(10);
        *pwr_valid_reg |= PWR_CR8_VDDIO1VRSEL;

        // continue vswitch sequence
        iface->POWER |= SDMMC_POWER_VSWITCH;

        // wait for finish
        while(!(iface->STA & SDMMC_STA_VSWEND));
        iface->ICR = SDMMC_ICR_VSWENDC | SDMMC_ICR_CKSTOPC;

        busyd0 = iface->STA & SDMMC_STA_BUSYD0;

        if(busyd0)
        {
            klog("sd%d: D0 low after voltage switch - failing\n", iface_id);
            vswitch_failed = true;
            return -1;
        }

        klog("sd%d: voltage switch succeeded\n", iface_id);

        iface->POWER &= ~(SDMMC_POWER_VSWITCH | SDMMC_POWER_VSWITCHEN);
    }

    if(is_mem)
    {
        // CMD2  - ALL_SEND_CID
        ret = sd_issue_command(2, resp_type::R2, 0, cid);
        if(ret != 0)
        {
            klog("sd%d: CMD2 failed\n", iface_id);
            return -1;
        }
    }

    // CMD3 - SEND_RELATIVE_ADDR
    uint32_t cmd3_ret;
    ret = sd_issue_command(3, resp_type::R6, 0, &cmd3_ret);
    if(ret != 0)
        return -1;

    if(cmd3_ret & (7UL << 13))
    {
        klog("sd%d: card error %lx\n", iface_id, cmd3_ret & 0xffff);
        return -1;
    }
    if(is_mem && !(cmd3_ret & (1UL << 8)))
    {
        klog("sd%d: card not ready for data %x\n", iface_id, cmd3_ret);
        return -1;
    }
    rca = cmd3_ret & 0xffff0000;

    if(is_mem)
    {
        // memory cards definitely support 25 MHz
        set_clock(SDCLK_DS);

        // Get card status - should be in data transfer mode, standby state
        ret = sd_issue_command(13, resp_type::R1, rca, resp);
        if(ret != 0)
            return -1;
        
        if(resp[0] != 0x700)
        {
            klog("sd%d: card not in standby state\n", iface_id);
            return -1;
        }

        // Send CMD9 to get cards CSD (needed to calculate card size)
        ret = sd_issue_command(9, resp_type::R2, rca, csd);
        if(ret != 0)
            return -1;

        {
            klog("sd%d: CSD: %lx, %lx, %lx, %lx\n", iface_id, csd[0], csd[1], csd[2], csd[3]);
            klog("sd%d: sd card size %llu kB\n", iface_id, get_size() / 1024ULL);
        }
        sd_size = get_size();
    }

    // Select card to put it in transfer state
    ret = sd_issue_command(7, resp_type::R1b, rca, resp);
    if(ret != 0)
        return -1;

    if(is_sdio)
    {
        if(read_cccr() != 0)
        {
            klog("sd%d: read_cccr failed\n", iface_id);
            return -1;
        }
        if((cccr[8] & (1U << 6)) == 0)
        {
            // not low speed - can speed up interface
            set_clock(SDCLK_DS);
        }
    }

    if(is_mem)
    {
        // Get card status again - should be in tranfer state
        ret = sd_issue_command(13, resp_type::R1, rca, resp);
        if(ret != 0)
            return -1;
        
        if(resp[0] != 0x900)
        {
            klog("sd%d: card not in transfer state\n", iface_id);
            return -1;
        }

        // If not SDHC ensure blocklen is 512 bytes
        if(!is_hc)
        {
            ret = sd_issue_command(16, resp_type::R1, 512, resp);
            if(ret != 0)
                return -1;
        }

        // Read SCR register - 64 bits from card in transfer mode - ACMD51 with data
        iface->DCTRL = 0;
        int timeout_ns = 200000000;
        iface->DTIMER = timeout_ns / clock_period_ns;
        iface->DLEN = 8;
        iface->DCTRL = 
            SDMMC_DCTRL_DTDIR |
            (3UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);

        ret = sd_issue_command(55, resp_type::R1, rca);
        if(ret != 0)
            return -1;
        
        ret = sd_issue_command(51, resp_type::R1, 0, resp, true);
        if(ret != 0)
            return -1;

        int scr_idx = 0;
        while(!(iface->STA & DBCKEND) && !(iface->STA & DATAEND) && !(iface->STA & DTIMEOUT))
        {
            if(iface->STA & DCRCFAIL)
            {
                klog("sd%d: dcrc fail\n", iface_id);
                return -1;
            }
            if(iface->STA & DTIMEOUT)
            {
                klog("sd%d: dtimeout\n", iface_id);
                return -1;
            }
            if(scr_idx < 2 && !(iface->STA & SDMMC_STA_RXFIFOE))
            {
                scr[1 - scr_idx] = __builtin_bswap32(iface->FIFO);
                scr_idx++;
            }
        }
        iface->ICR = DBCKEND | DATAEND;
        
        {
            klog("sd%d: scr %lx %lx, dcount %lx, sta %lx\n", iface_id, scr[0], scr[1], iface->DCOUNT, iface->STA);
        }
    }

    // set 4 bit mode via one or both of memory and io interfaces
    bool bit4_set = false;
    if(is_mem)
    {
        // can we use 4-bit signalling?
        if((scr[1] & (0x5UL << (48-32))) == (0x5UL << (48-32)))
        {
            ret = sd_issue_command(55, resp_type::R1, rca);
            if(ret != 0)
                return -1;
            ret = sd_issue_command(6, resp_type::R1, 2);
            if(ret != 0)
                return -1;

            klog("sd%d: mem: card set to 4-bit mode\n", iface_id);
            bit4_set = true;
        }
    }
    if(is_sdio)
    {
        if((cccr[8] & 0x40U) == 0 || (cccr[8] & 0x80U))
        {
            if(write_cccr(7, (cccr[7] & ~0x3U) | 0x2U | 0x80U))
            {
                klog("sd%d: sdio: write_cccr failed\n", iface_id);
                return -1;
            }
        }

        if((cccr[7] & 0x3U) == 0x2U)
        {
            klog("sd%d: sdio: card set to 4-bit mode\n", iface_id);
            bit4_set = true;
        }
    }

    if(bit4_set)
    {
        iface->CLKCR |= SDMMC_CLKCR_WIDBUS_0;
    }

    if(is_mem)
    {
        // can we use 48 MHz interface?
        if(((scr[1] >> (56 - 32)) & 0xfU) >= 1)
        {
            // supports CMD6

            // send inquiry CMD6
            iface->DLEN = 64;
            iface->DCTRL = 
                SDMMC_DCTRL_DTDIR |
                (6UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);

            ret = sd_issue_command(6, resp_type::R1, 0, nullptr, true);
            if(ret != 0)
                return -1;
            
            // read response
            int cmd6_buf_idx = 0;
            while(!(iface->STA & DBCKEND) && !(iface->STA & DATAEND) && !(iface->STA & DTIMEOUT))
            {
                if(iface->STA & DCRCFAIL)
                {
                    
                    klog("sd%d: dcrcfail\n", iface_id);
                    return -1;
                }
                if(iface->STA & DTIMEOUT)
                {
                    
                    klog("sd%d: dtimeout\n", iface_id);
                    return -1;
                }
                if(iface->STA & SDMMC_STA_RXOVERR)
                {
                    
                    klog("sd%d: rxoverr\n", iface_id);
                    return -1;
                }
                if(cmd6_buf_idx < 16 && !(iface->STA & SDMMC_STA_RXFIFOE))
                {
                    cmd6_buf[16 - 1 - cmd6_buf_idx] = __builtin_bswap32(iface->FIFO);
                    cmd6_buf_idx++;
                }
            }

            iface->ICR = DBCKEND | DATAEND;

            // can we enable high speed mode? - check bits 415:400
            auto fg1_support = (cmd6_buf[12] >> 16) & 0xffffUL;
            {
                
                klog("sd%d: cmd6: fg1_support: %lx\n", iface_id, fg1_support);
            }
            auto fg4_support = (cmd6_buf[14]) & 0xffffUL;
            klog("sd%d: cmd6: fg4_support: %lx\n", iface_id, fg4_support);
            uint32_t mode_set = 0;
            if(is_1v8 && (fg1_support & 0x10) && (fg4_support & 0x2))
            {
                klog("sd%d: supports ddr50 mode at 1.44W\n", iface_id);
                mode_set = 0x80ff1ff4;
            }
            else if(fg1_support & 0x2)
            {
                klog("sd%d: supports hs/sdr25 mode at 0.72W\n", iface_id);
                mode_set = 0x80fffff1;
            }

            if(mode_set)
            {
                // try and switch to higher speed mode
                iface->DCTRL = 0;
                iface->DLEN = 64;
                iface->DCTRL = 
                    SDMMC_DCTRL_DTDIR |
                    (6UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);
                
                ret = sd_issue_command(6, resp_type::R1, mode_set, nullptr, true);
                if(ret != 0)
                    return -1;
                
                cmd6_buf_idx = 0;
                while(!(iface->STA & DBCKEND) && !(iface->STA & DATAEND) && !(iface->STA & DTIMEOUT))
                {
                    if(iface->STA & DCRCFAIL)
                    {
                        
                        klog("sd%d: dcrcfail\n", iface_id);
                        return -1;
                    }
                    if(iface->STA & DTIMEOUT)
                    {
                        
                        klog("sd%d: dtimeout\n", iface_id);
                        return -1;
                    }
                    if(iface->STA & SDMMC_STA_RXOVERR)
                    {
                        
                        klog("sd%d: rxoverr\n", iface_id);
                        return -1;
                    }
                    if(cmd6_buf_idx < 16 && !(iface->STA & SDMMC_STA_RXFIFOE))
                    {
                        cmd6_buf[16 - 1 - cmd6_buf_idx] = __builtin_bswap32(iface->FIFO);
                        cmd6_buf_idx++;
                    }
                }
                iface->ICR = DBCKEND | DATAEND;

                auto fg1_setting = (cmd6_buf[11] >> 24) & 0xfUL;
                auto fg4_setting = (cmd6_buf[12] >> 4) & 0xfUL;
                {
                    
                    klog("sd%d: cmd6: fg1_setting: %lx, fg4_setting: %lx\n", iface_id, fg1_setting,
                        fg4_setting);
                }

#if GK_SD_USE_HS_MODE
                if(fg1_setting == 1)
                {
                    set_clock(SDCLK_HS);
                    iface->DTIMER = timeout_ns / clock_period_ns;
                    {
                        klog("sd%d: set to high speed mode\n", iface_id);
                    }
                }
                else if(fg1_setting == 4)
                {
                    set_clock(SDCLK_HS, true);
                    iface->DTIMER = timeout_ns / clock_period_ns;
                    {
                        klog("sd%d: set to DDR50 mode\n", iface_id);
                    }
                }
#endif
            }
        }
    }

    // configure sdio parameters
    if(is_sdio)
    {        
        // enable higher power mode
        if((cccr[0x12] & 0x1) && default_supply_current > 200)
        {
            if(write_cccr(0x12, cccr[0x12] | 0x2) != 0)
                return -1;
        }

        // TODO: UHS support
        if(cccr[0x13] & 0x1)
        {
            // high speed support
            if(write_cccr(0x13, (cccr[0x13] & ~0xeU) | 0x2U) != 0)
                return -1;
            
            // wait 8 clock cycles
            auto delay = clock_period_ns * 8;
            udelay((delay + 999) / 1000);
            if(read_cccr(0x13) != 0)
                return -1;

            if((cccr[0x13] & 0xeU) == 0x2U)
                set_clock(SDCLK_HS);
        }

        read_cccr();
    }

    // Hardware flow control - prevents buffer under/overruns
    iface->CLKCR |= SDMMC_CLKCR_HWFC_EN;

    // Enable interrupts
    iface->MASK = DCRCFAIL | DTIMEOUT |
        TXUNDERR | RXOVERR | DATAEND;
    if(is_sdio)
        iface->MASK |= SDIOIT;

    // TODO
    //gic_set_target(155, GIC_ENABLED_CORES);
    //gic_set_enable(155);

    klog("sd%d: init success\n", iface_id);
    tfer_inprogress = false;
    sd_ready = true;

    return (is_mem || is_sdio) ? 0 : -1;
}

int SDIF::write_cccr(unsigned int reg, uint8_t v)
{
    if(!is_sdio)
        return -1;

    uint32_t resp;
    uint32_t arg = reg << 9;
    arg |= (uint32_t)v;
    arg |= 1U << 27;        // RAW
    arg |= 1U << 31;        // write

    auto ret = sd_issue_command(52, resp_type::R5,arg, &resp);
    if(ret != 0)
    {
        klog("sd%d: write_cccr ret %d\n", ret);
        return ret;
    }
    auto status = (resp >> 8) & 0xffU;
    if(status & 0xcbU)
    {
        klog("sd%d: write_cccr status error: %x\n", resp);
        return -1;
    }
    resp &= 0xffU;
    //klog("sd%d: CCCR[%2u] = %x\n", iface_id, reg, resp);

    if(reg < 0x16)
        cccr[reg] = (uint8_t)resp;
    return 0;
}

int SDIF::read_cccr(unsigned int reg, uint8_t *val_out)
{
    if(!is_sdio)
        return -1;

    uint32_t resp;
    auto ret = sd_issue_command(52, resp_type::R5, reg << 9, &resp);
    if(ret != 0)
    {
        klog("sd%d: read_cccr ret %d\n", ret);
        return ret;
    }
    auto status = (resp >> 8) & 0xffU;
    if(status & 0xcbU)
    {
        klog("sd%d: read_cccr status error: %x\n", resp);
        return -1;
    }
    resp &= 0xffU;
    //klog("sd%d: CCCR[%2u] = %x\n", iface_id, reg, resp);

    if(reg < 0x16)
        cccr[reg] = (uint8_t)resp;
    if(val_out)
        *val_out = (uint8_t)resp;
    
    return 0;
}

int SDIF::read_cccr()
{
    if(!is_sdio)
        return -1;

    for(unsigned int reg = 0; reg < 0x16; reg++)
    {
        auto ret = read_cccr(reg);
        if(ret != 0)
            return ret;
    }

    return 0;
}

uint32_t SDIF::csd_extract(int startbit, int endbit) const
{   
    uint32_t ret = 0;

    int cur_ret_bit = 0;
    int cur_byte = 0;
    while(startbit > 31)
    {
        startbit -= 32;
        endbit -= 32;
        cur_byte++;
    }

    while(endbit > 0)
    {
        auto cur_part = csd[cur_byte] >> startbit;

        if(endbit < 31)
        {
            // need to mask end bits
            uint32_t mask = ~(0xFFFFFFFFUL << (endbit - startbit + 1));
            cur_part &= mask;
        }

        ret += (cur_part << cur_ret_bit);

        auto act_endbit = endbit > 31 ? 31 : endbit;

        cur_ret_bit += (act_endbit - startbit + 1);
        startbit -= 32;
        if(startbit < 0)
            startbit = 0;
        endbit -= 32;
        cur_byte++;
    }

    return ret;
}

uint64_t SDIF::get_size() const
{
    switch(csd[3] >> 30)
    {
        case 0:
            // CSD v1.0
            {
                auto c_size = (uint64_t)csd_extract(62, 73);
                auto c_size_mult = (uint64_t)csd_extract(47, 49);
                auto read_bl_len = (uint64_t)csd_extract(80, 83);

                auto mult = 1ULL << (c_size_mult + 2);
                auto blocknr = (c_size + 1) * mult;
                auto block_len = 1ULL << read_bl_len;

                return blocknr * block_len;
            }

        case 1:
            // CSD v2.0
            {
                auto c_size = (uint64_t)csd_extract(48, 69);

                return (c_size + 1) * 512 * 1024;
            }

        case 2:
            // CSD v3.0
            {
                auto c_size = (uint64_t)csd_extract(48, 75);

                return (c_size + 1) * 512 * 1024;
            }
    }

    return 0;
}

int SDIF::sd_issue_command(uint32_t command, resp_type rt, uint32_t arg, uint32_t *resp, 
    bool with_data,
    bool ignore_crc,
    int timeout_retry)
{
    int tcnt = 0;
    for(; tcnt < timeout_retry; tcnt++)
    {
#if DEBUG_SD
        if(tcnt > 0)
        {
            klog("sd%d: issue_command: retry %d for command %lu, sta: %lx\n", iface_id,
                tcnt, command,
                iface->STA);
        }
#endif

        // For now, error here if there are unhandled CMD flags
        const auto cmd_flags = CCRCFAIL | CTIMEOUT | CMDREND | CMDSENT | TXUNDERR | RXOVERR | SDMMC_STA_BUSYD0END;

        if(iface->STA & cmd_flags)
        {
            klog("sd%d: issue_command: unhandled flags: %lx\n", iface_id, iface->STA);
            return -1;
        }

        uint32_t waitresp = 0;
        switch(rt)
        {
            case resp_type::None:
                waitresp = 0;
                break;
            case resp_type::R1:
            case resp_type::R1b:
            case resp_type::R4:
            case resp_type::R4b:
            case resp_type::R5:
            case resp_type::R6:
            case resp_type::R7:
                waitresp = 1;
                break;
            case resp_type::R3:
                waitresp = 2;
                break;
            case resp_type::R2:
                waitresp = 3;
                break;
        }

        switch(rt)
        {
            case resp_type::R2:
            case resp_type::R3:
            case resp_type::R4:
            case resp_type::R4b:
                ignore_crc = true;
                break;
            default:
                break;
        }

        bool with_busy = (rt == resp_type::R1b) || (rt == resp_type::R4b);

        iface->ARG = arg;

        uint32_t start_data = with_data ? SDMMC_CMD_CMDTRANS : 0UL;

        if(command == 12U)
            command |= SDMMC_CMD_CMDSTOP;

        iface->CMD = command | (waitresp << SDMMC_CMD_WAITRESP_Pos) | SDMMC_CMD_CPSMEN | start_data;

        if(rt == resp_type::None)
        {
            while(!(iface->STA & CMDSENT));      // TODO: WFI
#if DEBUG_SD
            klog("sd%d: issue_command: sent %lu no response expected\n", iface_id, command);
#endif
            iface->ICR = CMDSENT;
            return 0;
        }

        bool timeout = false;

        uint32_t sta_cmdrend = 0;
        while(!((sta_cmdrend = iface->STA) & CMDREND))
        {
            if(iface->STA & CCRCFAIL)
            {
                if(ignore_crc)
                {
                    iface->ICR = CCRCFAIL;
                    break;
                }

#if DEBUG_SD
                klog("sd%d: issue_command: sent %lu invalid crc response\n", iface_id, command);
#endif
                iface->ICR = CCRCFAIL;
                return CCRCFAIL;
            }

            if(iface->STA & CTIMEOUT)
            {
                iface->ICR = CTIMEOUT;
                timeout = true;
                break;
            }
        }

        if(timeout)
        {
#if DEBUG_SD
            {
                klog("sd%d: issue_command: timeout, sta: %lx, cmdr: %lx, dctrl: %lx\n",
                    iface_id,
                    iface->STA, iface->CMD, iface->DCTRL);
            }
#endif
            Block(clock_cur() + kernel_time_from_ms(tcnt * 5));
            //delay_ms(tcnt * 5);
            continue;
        }

        if(with_busy)
        {
            if(sta_cmdrend & SDMMC_STA_BUSYD0)
            {
                while(true)
                {
                    if(iface->STA & SDMMC_STA_BUSYD0END)
                    {
                        iface->ICR = SDMMC_ICR_BUSYD0ENDC;
                        break;
                    }
                    if(iface->STA & SDMMC_STA_DTIMEOUT)
                    {
                        return CTIMEOUT;
                    }
                }
            }
        }

#if DEBUG_SD
        char dbg_msg[128];
        sprintf(dbg_msg, "sd%d: issue_command: sent %u received reponse", iface_id, command);
#endif

        if(resp)
        {
            auto nresp = (rt == resp_type::R2 ? 4 : 1);
            for(int i = 0; i < nresp; i++)
            {
                resp[nresp - i - 1] = (&iface->RESP1)[i];
#if DEBUG_SD
                {
                    char msg2[32];
                    sprintf(msg2, " %x", resp[nresp - i - 1]);
                    strcat(dbg_msg, msg2);
                }
#endif
            }
        }

        
#if DEBUG_SD
        {
            klog("%s\n", dbg_msg);
        }
#endif

        iface->ICR = CMDREND;
        return 0;
    }

    // timeout
    {
        klog("sd%d: issue_command: sent %lu command timeout\n", iface_id, command);
    }
    return CTIMEOUT;
}

int SDIF::set_clock(int freq, bool ddr)
{
    auto div = SDCLK / freq;
    auto rem = SDCLK - (freq * div);
    if(rem)
        div++;

    if(div == 1)
    {
        div = 0;
    }
    else if(div & 0x1)
    {
        div = (div / 2) + 1;
    }
    else
    {
        div = div / 2;
    }
    
    if(div > 1023) div = 1023;
    if(div < 0) div = 0;

    auto clkcr = iface->CLKCR;
    clkcr &= ~SDMMC_CLKCR_CLKDIV_Msk;
    clkcr |= div;

    if(ddr)
        clkcr |= SDMMC_CLKCR_DDR;
    else
        clkcr &= ~SDMMC_CLKCR_DDR;

    if(freq >= 50000000)
        clkcr |= SDMMC_CLKCR_BUSSPEED;
    else
        clkcr &= ~SDMMC_CLKCR_BUSSPEED;
    
    iface->CLKCR = clkcr;

    clock_speed = (div == 0) ? SDCLK : (SDCLK / (div * 2));
    {
        klog("sd%d: clock set to %d (div = %d)\n", iface_id, clock_speed, div);
    }

    clock_period_ns = 1000000000 / clock_speed;

    return 0;
}

int SDIF::sdio_enable_function(unsigned int func, bool enable)
{
    if(!sd_ready)
        return -1;
    
    if(!is_sdio)
        return -1;

    if(func == 0)
        return 0;
    
    if(func > 7)
        return -1;

    auto wval = enable ? (cccr[0x2] | (1U << func)) :
        (cccr[0x2] & ~(1U << func));
    
    return write_cccr(0x2, wval);
}

int SDIF::sdio_enable_func_int(unsigned int func, bool enable)
{
    if(!sd_ready)
        return -1;
    
    if(!is_sdio)
        return -1;

    if(func > 7)
        return -1;

    auto wval = enable ? (cccr[0x4] | (1U << func) | 0x1U) :
        (cccr[0x4] & ~(1U << func));

    if(wval != 0)
        wval |= 0x1;        // re-enable master interrupts if other functions have IF
    
    return write_cccr(0x4, wval);
}

int SDIF::sdio_set_func_block_size(unsigned int func, size_t block_size)
{
    if(!is_sdio)
        return -1;
    if(!sd_ready)
        return -1;
    if(func > 7)
        return -1;
    if(block_size > 2048)
        return -1;
    if(block_size == 0)
        return -1;

    unsigned int base_addr = (func * 0x100) + 0x10;
    auto ret = write_cccr(base_addr, (uint8_t)(block_size & 0xffU));
    if(ret != 0)
        return ret;
    ret = write_cccr(base_addr + 1, (uint8_t)((block_size >> 8) & 0xffU));
    return ret;
}
