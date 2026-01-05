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

#define GPIOC_VMEM (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE)
static const constexpr pin WIFI_REG_ON { GPIOC_VMEM, 7 };
static const constexpr pin BT_REG_ON { GPIOC_VMEM, 8 };

#define GPIOI_VMEM (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOI_BASE)
static const constexpr pin MCO1 { GPIOI_VMEM, 6, 1 };

enum StatusFlags { CCRCFAIL = 1, DCRCFAIL = 2, CTIMEOUT = 4, DTIMEOUT = 8,
    TXUNDERR = 0x10, RXOVERR = 0x20, CMDREND = 0x40, CMDSENT = 0x80,
    DATAEND = 0x100, /* reserved */ DBCKEND = 0x400, DABORT = 0x800,
    DPSMACT = 0x1000, CPSMACT = 0x2000, TXFIFOHE = 0x4000, RXFIFOHF = 0x8000,
    TXFIFOF = 0x10000, RXFIFOF = 0x20000, TXFIFOE = 0x40000, RXFIFOE = 0x80000 };

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
    RCC_VMEM->GPIOCCFGR |= RCC_GPIOCCFGR_GPIOxEN;
    RCC_VMEM->GPIOICFGR |= RCC_GPIOICFGR_GPIOxEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    // Enable 32kHz sleep clock
    RCC_VMEM->FCALCOBS0CFGR = RCC_FCALCOBS0CFGR_CKOBSEN |
        (0x86U << RCC_FCALCOBS0CFGR_CKINTSEL_Pos);      // LSE
    RCC_VMEM->MCO1CFGR = RCC_MCO1CFGR_MCO1ON | RCC_MCO1CFGR_MCO1SEL;
    __asm__ volatile("dsb sy\n" ::: "memory");
    MCO1.set_as_af();

    for(const auto &p : sdmmc2_pins)
        p.set_as_af();
    WIFI_REG_ON.set_as_output();
    WIFI_REG_ON.clear();
    BT_REG_ON.set_as_output();
    BT_REG_ON.set();

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
    // functions to set power
    sdmmc[1].supply_off = []() { WIFI_REG_ON.clear(); udelay(10000); return 0; };
    sdmmc[1].supply_on = []() { WIFI_REG_ON.set(); udelay(250000); return 0; };
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
                ret = sd_issue_command(5, resp_type::R4, 0xff8000U, resp);
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
                    cmd5_s18a = ((resp[0] >> 25) & 0x1) != 0;       // later OR'd with ACMD41 S18A, if relevant

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
    
    (void)is_v2;

    if(!is_sdio || is_mem)
    {
        // proceed to memory init ACMD41
        klog("sd%d: memory init\n", iface_id);
    }

    if(!is_sdio && !is_mem)
    {
        // card has no functions we support
        return -1;
    }

    // Check logical OR of ACMD41 S18A and CMD5 S18A


    return (is_mem || is_sdio) ? 0 : -1;
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
        klog("SD%d: clock set to %d (div = %d)\n", iface_id, clock_speed, div);
    }

    clock_period_ns = 1000000000 / clock_speed;

    return 0;
}

#if 0
void sd_reset()
{
    sd_ready = false;
    is_4bit = false;
    is_hc = false;
    tfer_inprogress = false;
    dma_ready = false;
    sd_multi = false;
    sd_status = 0;

    // Clocks to SDMMC1/2 (crossbar 51 + 52)
    // Use PLL4 = 1200 MHz / 6 -> 200 MHz SDCLK
    RCC_VMEM->PREDIVxCFGR[51] = 0;
    RCC_VMEM->FINDIVxCFGR[51] = 0x45;
    RCC_VMEM->XBARxCFGR[51] = 0x40;
    RCC_VMEM->PREDIVxCFGR[52] = 0;
    RCC_VMEM->FINDIVxCFGR[52] = 0x45;
    RCC_VMEM->XBARxCFGR[52] = 0x40;
  
    
    // Assume SD is already inserted, panic otherwise

    // This follows RM 58.6.7

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

    /* Pins */
    for(const auto &p : sd_pins)
    {
        p.set_as_af();
    }

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

    // Ensure clock is set-up for identification mode - other bits are zero after reset
    SDMMC_set_clock(SDCLK_IDENT);

    // Hardware flow control
    SDMMC1_VMEM->CLKCR |= SDMMC_CLKCR_HWFC_EN;

    // power-cycle card here
    auto mv = smc_set_power(SMC_Power_Target::SDCard, 0);
    if(mv != 0)
    {
        klog("SD: failed to switch off card %d\n", mv);
        return;
    }
    PWR_VMEM->CR8 &= ~PWR_CR8_VDDIO1VRSEL;
    delay_ms(10);
    mv = smc_set_power(SMC_Power_Target::SDCard_IO, 3300);
    if(mv != 3300)
    {
        klog("SD: failed to turn on SDMMC1 IO %d\n", mv);
        return;
    }
    PWR_VMEM->CR8 |= PWR_CR8_VDDIO1SV;

    // set SDMMC state to power cycle (no drive on any pins)
    SDMMC1_VMEM->POWER = 2UL << SDMMC_POWER_PWRCTRL_Pos;

    // wait 1 ms
    delay_ms(1);

    // enable card VCC and wait power ramp-up time
    mv = smc_set_power(SMC_Power_Target::SDCard, 3300);
    if(mv != 3300)
    {
        smc_set_power(SMC_Power_Target::SDCard_IO, 0);
        PWR_VMEM->CR8 &= ~PWR_CR8_VDDIO1SV;
        klog("SD: failed to turn on card %d\n", mv);
        return;
    }
    delay_ms(10);

    // disable SDMMC output
    SDMMC1_VMEM->POWER = 0UL;

    // wait 1 ms
    delay_ms(1);

    // Enable clock to card
    SDMMC1_VMEM->POWER = 3UL << SDMMC_POWER_PWRCTRL_Pos;

    // Wait 74 CK cycles = 0.37 ms at 200 kHz
    delay_ms(1);
    
    // Issue CMD0 (go idle state)
    auto ret = sd_issue_command(0, resp_type::None);
    if(ret != 0)
        return;
    
    // Issue CMD8 (bcr, argument = supply voltage, R7)
    uint32_t resp[4];
    ret = sd_issue_command(8, resp_type::R7, 0x1aa, resp);
    bool is_v2 = false;
    if(ret == CTIMEOUT)
    {
        is_v2 = false;
        klog("init_sd: CMD8 timed out - assuming <v2 card\n");
    }
    else if(ret == 0)
    {
        if((resp[0] & 0x1ff) == 0x1aa)
        {
            is_v2 = true;
        }
        else
        {
            klog("init_sd: CMD8 invalid response %lx\n", resp[0]);
            return;
        }
    }
    else
        return;
    
    (void)is_v2;

    // Inquiry ACMD41 to get voltage window
    ret = sd_issue_command(55, resp_type::R1, 0, resp);
    if(ret != 0)
        return;
    
    ret = sd_issue_command(41, resp_type::R3, 0, resp);
    if(ret != 0)
        return;

    // We can promise ~3.1-3.4 V
    if(!(resp[0] & 0x380000))
    {
        klog("init_sd: invalid voltage range\n");
        return;
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
            return;
        
        ret = sd_issue_command(41, resp_type::R3, ocr, resp);
        if(ret != 0)
            return;
        
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
                klog("init_sd: %s capacity card ready\n", is_hc ? "high" : "normal");
                if(is_1v8)
                {
                    klog("init_sd: 1.8v signalling support\n");
                }
            }
            is_ready = true;
        }

        // TODO: timeout
    }
    
    if(is_1v8)
    {
        // send CMD11 to begin voltage switch sequence

        // PLSS p.47 and RM p.2186
        SDMMC1_VMEM->POWER |= SDMMC_POWER_VSWITCHEN;
        
        ret = sd_issue_command(11, resp_type::R1, 0);
        if(ret != 0)
        {
            is_ready = false;
            vswitch_failed = true;
            return;
        }

        klog("init_sd: CMD11 returned successfully\n");

        // wait for CK_STOP
        while(!(SDMMC1_VMEM->STA & SDMMC_STA_CKSTOP));
        SDMMC1_VMEM->ICR = SDMMC_ICR_CKSTOPC;

        // sample BUSYD0
        auto busyd0 = SDMMC1_VMEM->STA & SDMMC_STA_BUSYD0;
        if(!busyd0)
        {
            klog("init_sd: D0 high, aborting voltage switch\n");
            is_ready = false;
            vswitch_failed = true;
            return;
        }

        klog("init_sd: D0 low - proceeding with vswitch\n");
        mv = smc_set_power(SMC_Power_Target::SDCard_IO, 1800);
        if(mv != 1800)
        {
            klog("init_sd: voltage regulator change failed: %u\n", mv);
            is_ready = false;
            vswitch_failed = true;
            return;
        }

        delay_ms(10);
        PWR_VMEM->CR8 |= ~PWR_CR8_VDDIO1VRSEL;

        // continue vswitch sequence
        SDMMC1_VMEM->POWER |= SDMMC_POWER_VSWITCH;

        // wait for finish
        while(!(SDMMC1_VMEM->STA & SDMMC_STA_VSWEND));
        SDMMC1_VMEM->ICR = SDMMC_ICR_VSWENDC | SDMMC_ICR_CKSTOPC;

        busyd0 = SDMMC1_VMEM->STA & SDMMC_STA_BUSYD0;

        if(busyd0)
        {
            klog("init_sd: D0 low after voltage switch - failing\n");
            is_ready = false;
            vswitch_failed = true;
            return;
        }

        klog("init_sd: voltage switch succeeded\n");

        SDMMC1_VMEM->POWER &= ~(SDMMC_POWER_VSWITCH | SDMMC_POWER_VSWITCHEN);
    }

    // CMD2  - ALL_SEND_CID
    ret = sd_issue_command(2, resp_type::R2, 0, cid);
    if(ret != 0)
    {
        klog("init_sd: CMD2 failed\n");
        return;
    }

    // CMD3 - SEND_RELATIVE_ADDR
    uint32_t cmd3_ret;
    ret = sd_issue_command(3, resp_type::R6, 0, &cmd3_ret);
    if(ret != 0)
        return;

    if(cmd3_ret & (7UL << 13))
    {
        klog("init_sd: card error %lx\n", cmd3_ret & 0xffff);
        return;
    }
    if(!(cmd3_ret & (1UL << 8)))
    {
        klog("init_sd: card not ready for data\n");
        return;
    }
    rca = cmd3_ret & 0xffff0000;

    // We know the card is an SD card so can cope with 25 MHz
    SDMMC_set_clock(SDCLK_DS);

    // Get card status - should be in data transfer mode, standby state
    ret = sd_issue_command(13, resp_type::R1, rca, resp);
    if(ret != 0)
        return;
    
    if(resp[0] != 0x700)
    {
        klog("init_sd: card not in standby state\n");
        return;
    }

    // Send CMD9 to get cards CSD (needed to calculate card size)
    ret = sd_issue_command(9, resp_type::R2, rca, csd);
    if(ret != 0)
        return;

    {
        klog("init_sd: CSD: %lx, %lx, %lx, %lx\n", csd[0], csd[1], csd[2], csd[3]);
        klog("init_sd: sd card size %llu kB\n", sd_get_size() / 1024ULL);
    }
    sd_size = sd_get_size();

    // Select card to put it in transfer state
    ret = sd_issue_command(7, resp_type::R1b, rca, resp);
    if(ret != 0)
        return;
    
    // Get card status again - should be in tranfer state
    ret = sd_issue_command(13, resp_type::R1, rca, resp);
    if(ret != 0)
        return;
    
    if(resp[0] != 0x900)
    {
        klog("init_sd: card not in transfer state\n");
        return;
    }

    // If not SDHC ensure blocklen is 512 bytes
    if(!is_hc)
    {
        ret = sd_issue_command(16, resp_type::R1, 512, resp);
        if(ret != 0)
            return;
    }

    // Empty FIFO
    //while(!(SDMMC1_VMEM->STA & RXFIFOE))
    //    (void)SDMMC1_VMEM->FIFO;

    // Read SCR register - 64 bits from card in transfer mode - ACMD51 with data
    SDMMC1_VMEM->DCTRL = 0;
    int timeout_ns = 200000000;
    SDMMC1_VMEM->DTIMER = timeout_ns / clock_period_ns;
    SDMMC1_VMEM->DLEN = 8;
    SDMMC1_VMEM->DCTRL = 
        SDMMC_DCTRL_DTDIR |
        (3UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);

    ret = sd_issue_command(55, resp_type::R1, rca);
    if(ret != 0)
        return;
    
    ret = sd_issue_command(51, resp_type::R1, 0, resp, true);
    if(ret != 0)
        return;

    int scr_idx = 0;
    while(!(SDMMC1_VMEM->STA & DBCKEND) && !(SDMMC1_VMEM->STA & DATAEND) && !(SDMMC1_VMEM->STA & DTIMEOUT))
    {
        if(SDMMC1_VMEM->STA & DCRCFAIL)
        {
            klog("init_sd: dcrc fail\n");
            return;
        }
        if(SDMMC1_VMEM->STA & DTIMEOUT)
        {
            klog("init_sd: dtimeout\n");
            return;
        }
        if(scr_idx < 2 && !(SDMMC1_VMEM->STA & SDMMC_STA_RXFIFOE))
        {
            scr[1 - scr_idx] = __builtin_bswap32(SDMMC1_VMEM->FIFO);
            scr_idx++;
        }
    }
    SDMMC1_VMEM->ICR = DBCKEND | DATAEND;
    
    {
        klog("init_sd: scr %lx %lx, dcount %lx, sta %lx\n", scr[0], scr[1], SDMMC1_VMEM->DCOUNT, SDMMC1_VMEM->STA);
    }

    // can we use 4-bit signalling?
    if((scr[1] & (0x5UL << (48-32))) == (0x5UL << (48-32)))
    {
        ret = sd_issue_command(55, resp_type::R1, rca);
        if(ret != 0)
            return;
        ret = sd_issue_command(6, resp_type::R1, 2);
        if(ret != 0)
            return;

        {
            
            klog("init_sd: set to 4-bit mode\n");
        }
        is_4bit = true;
        SDMMC1_VMEM->CLKCR |= SDMMC_CLKCR_WIDBUS_0;
    }

    // can we use 48 MHz interface?
    if(((scr[1] >> (56 - 32)) & 0xfU) >= 1)
    {
        // supports CMD6

        // send inquiry CMD6
        SDMMC1_VMEM->DLEN = 64;
        SDMMC1_VMEM->DCTRL = 
            SDMMC_DCTRL_DTDIR |
            (6UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);

        ret = sd_issue_command(6, resp_type::R1, 0, nullptr, true);
        if(ret != 0)
            return;
        
        // read response
        int cmd6_buf_idx = 0;
        while(!(SDMMC1_VMEM->STA & DBCKEND) && !(SDMMC1_VMEM->STA & DATAEND) && !(SDMMC1_VMEM->STA & DTIMEOUT))
        {
            if(SDMMC1_VMEM->STA & DCRCFAIL)
            {
                
                klog("dcrcfail\n");
                return;
            }
            if(SDMMC1_VMEM->STA & DTIMEOUT)
            {
                
                klog("dtimeout\n");
                return;
            }
            if(SDMMC1_VMEM->STA & SDMMC_STA_RXOVERR)
            {
                
                klog("rxoverr\n");
                return;
            }
            if(cmd6_buf_idx < 16 && !(SDMMC1_VMEM->STA & SDMMC_STA_RXFIFOE))
            {
                cmd6_buf[16 - 1 - cmd6_buf_idx] = __builtin_bswap32(SDMMC1_VMEM->FIFO);
                cmd6_buf_idx++;
            }
        }

        SDMMC1_VMEM->ICR = DBCKEND | DATAEND;

        // can we enable high speed mode? - check bits 415:400
        auto fg1_support = (cmd6_buf[12] >> 16) & 0xffffUL;
        {
            
            klog("init_sd: cmd6: fg1_support: %lx\n", fg1_support);
        }
        auto fg4_support = (cmd6_buf[14]) & 0xffffUL;
        klog("init_sd: cmd6: fg4_support: %lx\n", fg4_support);
        uint32_t mode_set = 0;
        if(is_1v8 && (fg1_support & 0x10) && (fg4_support & 0x2))
        {
            klog("init_sd: supports ddr50 mode at 1.44W\n");
            mode_set = 0x80ff1ff4;
        }
        else if(fg1_support & 0x2)
        {
            klog("init_sd: supports hs/sdr25 mode at 0.72W\n");
            mode_set = 0x80fffff1;
        }

        if(mode_set)
        {
            // try and switch to higher speed mode
            SDMMC1_VMEM->DCTRL = 0;
            SDMMC1_VMEM->DLEN = 64;
            SDMMC1_VMEM->DCTRL = 
                SDMMC_DCTRL_DTDIR |
                (6UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);
            
            ret = sd_issue_command(6, resp_type::R1, mode_set, nullptr, true);
            if(ret != 0)
                return;
            
            cmd6_buf_idx = 0;
            while(!(SDMMC1_VMEM->STA & DBCKEND) && !(SDMMC1_VMEM->STA & DATAEND) && !(SDMMC1_VMEM->STA & DTIMEOUT))
            {
                if(SDMMC1_VMEM->STA & DCRCFAIL)
                {
                    
                    klog("dcrcfail\n");
                    return;
                }
                if(SDMMC1_VMEM->STA & DTIMEOUT)
                {
                    
                    klog("dtimeout\n");
                    return;
                }
                if(SDMMC1_VMEM->STA & SDMMC_STA_RXOVERR)
                {
                    
                    klog("rxoverr\n");
                    return;
                }
                if(cmd6_buf_idx < 16 && !(SDMMC1_VMEM->STA & SDMMC_STA_RXFIFOE))
                {
                    cmd6_buf[16 - 1 - cmd6_buf_idx] = __builtin_bswap32(SDMMC1_VMEM->FIFO);
                    cmd6_buf_idx++;
                }
            }
            SDMMC1_VMEM->ICR = DBCKEND | DATAEND;

            auto fg1_setting = (cmd6_buf[11] >> 24) & 0xfUL;
            auto fg4_setting = (cmd6_buf[12] >> 4) & 0xfUL;
            {
                
                klog("init_sd: cmd6: fg1_setting: %lx, fg4_setting: %lx\n", fg1_setting,
                    fg4_setting);
            }

#if GK_SD_USE_HS_MODE
            if(fg1_setting == 1)
            {
                SDMMC_set_clock(SDCLK_HS);
                SDMMC1_VMEM->DTIMER = timeout_ns / clock_period_ns;
                {
                    klog("init_sd: set to high speed mode\n");
                }
            }
            else if(fg1_setting == 4)
            {
                SDMMC_set_clock(SDCLK_HS, true);
                SDMMC1_VMEM->DTIMER = timeout_ns / clock_period_ns;
                {
                    klog("init_sd: set to DDR50 mode\n");
                }
            }
#endif
        }
    }

    // Hardware flow control - prevents buffer under/overruns
    SDMMC1_VMEM->CLKCR |= SDMMC_CLKCR_HWFC_EN;

    // Enable interrupts
    SDMMC1_VMEM->MASK = DCRCFAIL | DTIMEOUT |
        TXUNDERR | RXOVERR | DATAEND;
    gic_set_target(155, GIC_ENABLED_CORES);
    gic_set_enable(155);
    
    {
        
        klog("init_sd: success\n");
    }
    tfer_inprogress = false;
    sd_ready = true;
}

#endif

#if 0
static uint32_t csd_extract(int startbit, int endbit)
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

uint64_t sd_get_size()
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
#endif