#include "stm32h7xx.h"
#include "clocks.h"
#include "sd.h"
#include <cstdio>
#include "pins.h"
#include "osqueue.h"
#include "osmutex.h"
#include "SEGGER_RTT.h"
#include "scheduler.h"
#include "cache.h"
#include "osnet.h"
#include "ext4_thread.h"
#include "gk_conf.h"

#define DEBUG_SD    0
#define PROFILE_SDT 0

extern Spinlock s_rtt;
extern Process kernel_proc;

#if GK_OVERCLOCK
#define SDCLK   240000000
#else
#define SDCLK   192000000
#endif

#define SDCLK_IDENT     200000
#define SDCLK_DS        25000000
#define SDCLK_HS        50000000

#define SDT_DATA    __attribute__((section(".sdt_data")))

SDT_DATA static int clock_speed = 0;
SDT_DATA static int clock_period_ns = 0;

SDT_DATA static uint32_t cid[4] = { 0 };
SDT_DATA static uint32_t csd[4] = { 0 };
SDT_DATA static uint32_t rca;
SDT_DATA static uint32_t scr[2];
SDT_DATA static bool is_4bit = false;
__attribute__((section(".sram4"))) volatile bool sd_ready = false;
SDT_DATA static bool is_hc = false;
SDT_DATA static volatile bool tfer_inprogress = false;
SDT_DATA static volatile bool dma_ready = false;
__attribute__((section(".sram4"))) static volatile uint32_t sd_status = 0;
__attribute__((section(".sram4"))) static volatile uint32_t sd_dcount = 0;
__attribute__((section(".sram4"))) static volatile uint32_t sd_idmabase = 0;
__attribute__((section(".sram4"))) static volatile uint32_t sd_idmasize = 0;
__attribute__((section(".sram4"))) static volatile uint32_t sd_idmactrl = 0;
__attribute__((section(".sram4"))) static volatile uint32_t sd_dctrl = 0;


SDT_DATA static volatile bool sd_multi = false;
__attribute__((section(".sram4"))) volatile uint64_t sd_size = 0;

SDT_DATA static uint32_t cmd6_buf[512/32];

extern char _ssdt_data, _esdt_data;

SRAM4_DATA MemRegion mr_unaligned_buf;

static constexpr pin sd_pins[] =
{
    { GPIOC, 8, 12 },
    { GPIOC, 9, 12 },
    { GPIOC, 10, 12 },
    { GPIOC, 11, 12 },
    { GPIOC, 12, 12 },
    { GPIOD, 2, 12 }
};


__attribute__((section(".sram4"))) FixedQueue<sd_request, 32> sdt_queue;
__attribute__((section(".sram4"))) SimpleSignal sdt_ready;

enum class resp_type { None, R1, R1b, R2, R3, R4, R4b, R5, R6, R7 };
enum class data_dir { None, ReadBlock, WriteBlock, ReadStream, WriteStream };

enum StatusFlags { CCRCFAIL = 1, DCRCFAIL = 2, CTIMEOUT = 4, DTIMEOUT = 8,
    TXUNDERR = 0x10, RXOVERR = 0x20, CMDREND = 0x40, CMDSENT = 0x80,
    DATAEND = 0x100, /* reserved */ DBCKEND = 0x400, DABORT = 0x800,
    DPSMACT = 0x1000, CPSMACT = 0x2000, TXFIFOHE = 0x4000, RXFIFOHF = 0x8000,
    TXFIFOF = 0x10000, RXFIFOF = 0x20000, TXFIFOE = 0x40000, RXFIFOE = 0x80000 };

static void *sd_thread(void *param);

static inline bool is_valid_dma(void *mem_address, uint32_t block_count)
{
    /* SDMMC1 can only access FlashA, FlashB, AXISRAM, QUADSPI and FMC */
    uint32_t mem_start = (uint32_t)(uintptr_t)mem_address;
    uint32_t mem_len = block_count * 512U;
    uint32_t mem_end = mem_start + mem_len;
    bool is_valid = false;
    if((mem_start >= 0x08000000U) && (mem_end < 0x08200000U))
    {
        is_valid = true;
    }
    else if((mem_start >= 0x24000000U) && (mem_end < 0x24080000U))
    {
        is_valid = true;
    }
    else if((mem_start >= 0x60000000U) && (mem_end < 0xd4000000U))
    {
        is_valid = true;
    }
    if(mem_start & 0x3U)
    {
        is_valid = false;
    }
    return is_valid;
}

static int sd_issue_command(uint32_t command, resp_type rt, uint32_t arg = 0, uint32_t *resp = nullptr, 
    bool with_data = false,
    bool ignore_crc = false,
    int timeout_retry = 10)
{
    int tcnt = 0;
    for(; tcnt < timeout_retry; tcnt++)
    {
#if DEBUG_SD
        ITM_SendChar('m');
        if(tcnt > 0)
        {
            CriticalGuard cg(s_rtt);
            klog("sd_issue_command: retry %d for command %lu, sta: %lx\n", tcnt, command,
                SDMMC1->STA);
        }
#endif

        // For now, error here if there are unhandled CMD flags
        const auto cmd_flags = CCRCFAIL | CTIMEOUT | CMDREND | CMDSENT | TXUNDERR | RXOVERR | SDMMC_STA_BUSYD0END;

        if(SDMMC1->STA & cmd_flags)
        {
            CriticalGuard cg(s_rtt);
            klog("sd_issue_command: unhandled flags: %lx\n", SDMMC1->STA);
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
            case resp_type::R4b:
                ignore_crc = true;
                break;
            default:
                break;
        }

        bool with_busy = (rt == resp_type::R1b) || (rt == resp_type::R4b);

        SDMMC1->ARG = arg;

        uint32_t start_data = with_data ? SDMMC_CMD_CMDTRANS : 0UL;

        if(command == 12U)
            command |= SDMMC_CMD_CMDSTOP;

        SDMMC1->CMD = command | (waitresp << SDMMC_CMD_WAITRESP_Pos) | SDMMC_CMD_CPSMEN | start_data;

        if(rt == resp_type::None)
        {
            while(!(SDMMC1->STA & CMDSENT));      // TODO: WFI
#if DEBUG_SD
            CriticalGuard cg(s_rtt);
            klog("sd_issue_command: sent %lu no response expected\n", command);
#endif
            SDMMC1->ICR = CMDSENT;
            return 0;
        }

        bool timeout = false;

        uint32_t sta_cmdrend = 0;
        while(!((sta_cmdrend = SDMMC1->STA) & CMDREND))
        {
            if(SDMMC1->STA & CCRCFAIL)
            {
                if(ignore_crc)
                {
                    SDMMC1->ICR = CCRCFAIL;
                    break;
                }

#if DEBUG_SD
                CriticalGuard cg(s_rtt);
                klog("sd_issue_command: sent %lu invalid crc response\n", command);
#endif
                SDMMC1->ICR = CCRCFAIL;
                return CCRCFAIL;
            }

            if(SDMMC1->STA & CTIMEOUT)
            {
                SDMMC1->ICR = CTIMEOUT;
                timeout = true;
                break;
            }
        }

        if(timeout)
        {
#if DEBUG_SD
            {
                CriticalGuard cg(s_rtt);
                klog("sd_issue_command: timeout, sta: %lx, cmdr: %lx, dctrl: %lx\n",
                    SDMMC1->STA, SDMMC1->CMD, SDMMC1->DCTRL);
            }
#endif
            delay_ms(tcnt * 5);
            continue;
        }

        if(with_busy)
        {
            if(sta_cmdrend & SDMMC_STA_BUSYD0)
            {
                while(true)
                {
                    if(SDMMC1->STA & SDMMC_STA_BUSYD0END)
                    {
                        SDMMC1->ICR = SDMMC_ICR_BUSYD0ENDC;
                        break;
                    }
                    if(SDMMC1->STA & SDMMC_STA_DTIMEOUT)
                    {
                        return CTIMEOUT;
                    }
                }
            }
        }

#if DEBUG_SD
        {
            CriticalGuard cg(s_rtt);
            klog("sd_issue_command: sent %lu received reponse", command);
        }
#endif

        if(resp)
        {
            auto nresp = (rt == resp_type::R2 ? 4 : 1);
            for(int i = 0; i < nresp; i++)
            {
                resp[nresp - i - 1] = (&SDMMC1->RESP1)[i];
#if DEBUG_SD
                {
                    CriticalGuard cg(s_rtt);
                    klog(" %lx", resp[nresp - i - 1]);
                }
#endif
            }
        }

        
#if DEBUG_SD
        {
            CriticalGuard cg(s_rtt);
            klog("\n");
        }
#endif

        SDMMC1->ICR = CMDREND;
        return 0;
    }

    // timeout
    {
        CriticalGuard cg(s_rtt);
        klog("sd_issue_command: sent %lu command timeout\n", command);
    }
    return CTIMEOUT;
}

static void SDMMC_set_clock(int freq)
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

    auto clkcr = SDMMC1->CLKCR;
    clkcr &= ~SDMMC_CLKCR_CLKDIV_Msk;
    clkcr |= div;
    SDMMC1->CLKCR = clkcr;

    clock_speed = (div == 0) ? SDCLK : (SDCLK / (div * 2));
    {
        CriticalGuard cg(s_rtt);
        klog("SDIO clock set to %d (div = %d)\n", clock_speed, div);
    }

    clock_period_ns = 1000000000 / clock_speed;
}


void init_sd()
{
    Schedule(Thread::Create("sd", sd_thread, nullptr, true, GK_PRIORITY_VHIGH, kernel_proc, 
        PreferM4));
}

void sd_reset()
{
    sd_ready = false;
    is_4bit = false;
    is_hc = false;
    tfer_inprogress = false;
    dma_ready = false;
    sd_multi = false;
    sd_status = 0;
    
    // Assume SD is already inserted, panic otherwise

    // This follows RM 58.6.7

    // Reset SDMMC
    RCC->AHB3RSTR = RCC_AHB3RSTR_SDMMC1RST;
    (void)RCC->AHB3RSTR;
    RCC->AHB3RSTR = 0;
    (void)RCC->AHB3RSTR;

    // Clock SDMMC
    RCC->AHB3ENR |= RCC_AHB3ENR_SDMMC1EN;
    (void)RCC->AHB3ENR;

    /* Pins */
    for(const auto &p : sd_pins)
    {
        p.set_as_af();
    }

    // Ensure clock is set-up for identification mode - other bits are zero after reset
    SDMMC_set_clock(SDCLK_IDENT);

    // Hardware flow control
    SDMMC1->CLKCR |= SDMMC_CLKCR_HWFC_EN;

    // TODO: power-cycle card here (need external FET not currently on pcb)

    // set SDMMC state to power cycle (no drive on any pins)
    SDMMC1->POWER = 2UL << SDMMC_POWER_PWRCTRL_Pos;

    // wait 1 ms
    delay_ms(1);

    // TODO: enable card VCC and wait power ramp-up time

    // disable SDMMC output
    SDMMC1->POWER = 0UL;

    // wait 1 ms
    delay_ms(1);

    // Enable clock to card
    SDMMC1->POWER = 3UL << SDMMC_POWER_PWRCTRL_Pos;

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
        CriticalGuard cg(s_rtt);
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
            CriticalGuard cg(s_rtt);
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
        CriticalGuard cg(s_rtt);
        klog("init_sd: invalid voltage range\n");
        return;
    }

    // Send init ACMD41 repeatedly until ready
    bool is_ready = false;
    is_hc = false;

    while(!is_ready)
    {
        uint32_t ocr = 0xff8000 | (is_v2 ? (1UL << 30) : 0);
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

            {
                CriticalGuard cg(s_rtt);
                klog("init_sd: %s capacity card ready\n", is_hc ? "high" : "normal");
            }
            is_ready = true;
        }

        // TODO: timeout
    }

    // CMD2  - ALL_SEND_CID
    ret = sd_issue_command(2, resp_type::R2, 0, cid);
    if(ret != 0)
        return;

    // CMD3 - SEND_RELATIVE_ADDR
    uint32_t cmd3_ret;
    ret = sd_issue_command(3, resp_type::R6, 0, &cmd3_ret);
    if(ret != 0)
        return;

    if(cmd3_ret & (7UL << 13))
    {
        CriticalGuard cg(s_rtt);
        klog("init_sd: card error %lx\n", cmd3_ret & 0xffff);
        return;
    }
    if(!(cmd3_ret & (1UL << 8)))
    {
        CriticalGuard cg(s_rtt);
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
        CriticalGuard cg(s_rtt);
        klog("init_sd: card not in standby state\n");
        return;
    }

    // Send CMD9 to get cards CSD (needed to calculate card size)
    ret = sd_issue_command(9, resp_type::R2, rca, csd);
    if(ret != 0)
        return;

    {
        CriticalGuard cg(s_rtt);
        klog("init_sd: CSD: %lx, %lx, %lx, %lx\n", csd[0], csd[1], csd[2], csd[3]);
        klog("init_sd: sd card size %lu kB\n", sd_get_size() / 1024);
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
        CriticalGuard cg(s_rtt);
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
    //while(!(SDMMC1->STA & RXFIFOE))
    //    (void)SDMMC1->FIFO;

    // Read SCR register - 64 bits from card in transfer mode - ACMD51 with data
    SDMMC1->DCTRL = 0;
    int timeout_ns = 200000000;
    SDMMC1->DTIMER = timeout_ns / clock_period_ns;
    SDMMC1->DLEN = 8;
    SDMMC1->DCTRL = 
        SDMMC_DCTRL_DTDIR |
        (3UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);

    ret = sd_issue_command(55, resp_type::R1, rca);
    if(ret != 0)
        return;
    
    ret = sd_issue_command(51, resp_type::R1, 0, resp, true);
    if(ret != 0)
        return;

    int scr_idx = 0;
    while(!(SDMMC1->STA & DBCKEND) && !(SDMMC1->STA & DATAEND) && !(SDMMC1->STA & DTIMEOUT))
    {
        if(SDMMC1->STA & DCRCFAIL)
        {
            CriticalGuard cg(s_rtt);
            klog("init_sd: dcrc fail\n");
            return;
        }
        if(SDMMC1->STA & DTIMEOUT)
        {
            CriticalGuard cg(s_rtt);
            klog("init_sd: dtimeout\n");
            return;
        }
        if(scr_idx < 2 && !(SDMMC1->STA & SDMMC_STA_RXFIFOE))
        {
            scr[1 - scr_idx] = __builtin_bswap32(SDMMC1->FIFO);
            scr_idx++;
        }
    }
    SDMMC1->ICR = DBCKEND | DATAEND;
    
    {
        CriticalGuard cg(s_rtt);
        klog("init_sd: scr %lx %lx, dcount %lx, sta %lx\n", scr[0], scr[1], SDMMC1->DCOUNT, SDMMC1->STA);
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
            CriticalGuard cg(s_rtt);
            klog("init_sd: set to 4-bit mode\n");
        }
        is_4bit = true;
        SDMMC1->CLKCR |= SDMMC_CLKCR_WIDBUS_0;
    }

    // can we use 48 MHz interface?
    if(((scr[1] >> (56 - 32)) & 0xfU) >= 1)
    {
        // supports CMD6

        // send inquiry CMD6
        SDMMC1->DLEN = 64;
        SDMMC1->DCTRL = 
            SDMMC_DCTRL_DTDIR |
            (6UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);

        ret = sd_issue_command(6, resp_type::R1, 0, nullptr, true);
        if(ret != 0)
            return;
        
        // read response
        int cmd6_buf_idx = 0;
        while(!(SDMMC1->STA & DBCKEND) && !(SDMMC1->STA & DATAEND) && !(SDMMC1->STA & DTIMEOUT))
        {
            if(SDMMC1->STA & DCRCFAIL)
            {
                CriticalGuard cg(s_rtt);
                klog("dcrcfail\n");
                return;
            }
            if(SDMMC1->STA & DTIMEOUT)
            {
                CriticalGuard cg(s_rtt);
                klog("dtimeout\n");
                return;
            }
            if(SDMMC1->STA & SDMMC_STA_RXOVERR)
            {
                CriticalGuard cg(s_rtt);
                klog("rxoverr\n");
                return;
            }
            if(cmd6_buf_idx < 16 && !(SDMMC1->STA & SDMMC_STA_RXFIFOE))
            {
                cmd6_buf[16 - 1 - cmd6_buf_idx] = __builtin_bswap32(SDMMC1->FIFO);
                cmd6_buf_idx++;
            }
        }

        SDMMC1->ICR = DBCKEND | DATAEND;

        // can we enable high speed mode? - check bits 415:400
        auto fg1_support = (cmd6_buf[12] >> 16) & 0xffffUL;
        {
            CriticalGuard cg(s_rtt);
            klog("init_sd: cmd6: fg1_support: %lx\n", fg1_support);
        }

        if(fg1_support & 0x2)
        {
            // try and switch to high speed mode
            SDMMC1->DCTRL = 0;
            SDMMC1->DLEN = 64;
            SDMMC1->DCTRL = 
                SDMMC_DCTRL_DTDIR |
                (6UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);
            
            ret = sd_issue_command(6, resp_type::R1, 0x80000001, nullptr, true);
            if(ret != 0)
                return;
            
            cmd6_buf_idx = 0;
            while(!(SDMMC1->STA & DBCKEND) && !(SDMMC1->STA & DATAEND) && !(SDMMC1->STA & DTIMEOUT))
            {
                if(SDMMC1->STA & DCRCFAIL)
                {
                    CriticalGuard cg(s_rtt);
                    klog("dcrcfail\n");
                    return;
                }
                if(SDMMC1->STA & DTIMEOUT)
                {
                    CriticalGuard cg(s_rtt);
                    klog("dtimeout\n");
                    return;
                }
                if(SDMMC1->STA & SDMMC_STA_RXOVERR)
                {
                    CriticalGuard cg(s_rtt);
                    klog("rxoverr\n");
                    return;
                }
                if(cmd6_buf_idx < 16 && !(SDMMC1->STA & SDMMC_STA_RXFIFOE))
                {
                    cmd6_buf[16 - 1 - cmd6_buf_idx] = __builtin_bswap32(SDMMC1->FIFO);
                    cmd6_buf_idx++;
                }
            }
            SDMMC1->ICR = DBCKEND | DATAEND;

            auto fg1_setting = (cmd6_buf[11] >> 24) & 0xfUL;
            {
                CriticalGuard cg(s_rtt);
                klog("init_sd: cmd6: fg1_setting: %lx\n", fg1_setting);
            }

#if GK_SD_USE_HS_MODE
            if(fg1_setting == 1)
            {
                SDMMC_set_clock(SDCLK_HS);
                SDMMC1->DTIMER = timeout_ns / clock_period_ns;
                {
                    CriticalGuard cg(s_rtt);
                    klog("init_sd: set to high speed mode\n");
                }
            }
#endif
        }
    }

    // Hardware flow control - prevents buffer under/overruns
    SDMMC1->CLKCR |= SDMMC_CLKCR_HWFC_EN;

    // Enable interrupts
    SDMMC1->MASK = DCRCFAIL | DTIMEOUT |
        TXUNDERR | RXOVERR | DATAEND;
    NVIC_EnableIRQ(SDMMC1_IRQn);
    
    {
        CriticalGuard cg(s_rtt);
        klog("init_sd: success\n");
    }
    tfer_inprogress = false;
    sd_ready = true;
}

static inline uint32_t do_mdma_transfer(uint32_t src, uint32_t dest)
{
    // get least alignment of src/dest
    uint32_t align_val = (src & 0x7f) | (dest & 0x7f);
    uint32_t size_val = 3U;
    if(align_val & 0x1U)
    {
        // byte alignment
        size_val = 0U;
    }
    else if(align_val & 0x2U)
    {
        // hword alignement
        size_val = 1U;
    }
    else if(align_val & 0x4U)
    {
        // word alignemnt
        size_val = 2U;
    }
    
    uint32_t ctbr = 0UL;
    if(src >= 0x20000000 && src < 0x20020000)
    {
        ctbr |= MDMA_CTBR_SBUS;
    }
    if(dest >= 0x20000000 && dest < 0x20020000)
    {
        ctbr |= MDMA_CTBR_DBUS;
    }
    if(ctbr && size_val == 3U)
    {
        size_val = 2U;
    }

    MDMA_Channel0->CTCR = MDMA_CTCR_SWRM |  // software trigger
        (1UL << MDMA_CTCR_TRGM_Pos) |       // block transfers
        (127UL << MDMA_CTCR_TLEN_Pos) |     // 128 bytes/buffer (max)
        (size_val << MDMA_CTCR_DINCOS_Pos) |      // dest increment 4 bytes
        (size_val << MDMA_CTCR_SINCOS_Pos) |      // src increment 4 bytes
        (size_val << MDMA_CTCR_DSIZE_Pos) |       // dest 4 byte transfers
        (size_val << MDMA_CTCR_SSIZE_Pos) |       // src 4 byte transfers
        (2U << MDMA_CTCR_DINC_Pos) |        // dest increments
        (2U << MDMA_CTCR_SINC_Pos);         // src increments
    MDMA_Channel0->CBNDTR = 512UL;          // 512 byte blocks, no repeat
    MDMA_Channel0->CSAR = src;
    MDMA_Channel0->CDAR = dest;
    MDMA_Channel0->CMAR = 0;
    MDMA_Channel0->CTBR = ctbr;
    MDMA_Channel0->CIFCR = 0x1f; // clear all interrupts
    uint32_t cr_val = MDMA_CCR_SWRQ | (3UL << MDMA_CCR_PL_Pos);
    MDMA_Channel0->CCR = cr_val;
    MDMA_Channel0->CCR = cr_val | MDMA_CCR_EN;
    while(!(MDMA_Channel0->CISR & MDMA_CISR_CTCIF));
    return 0;
}

// sd thread function
void *sd_thread(void *param)
{
    (void)param;

    mr_unaligned_buf = memblk_allocate(512U, MemRegionType::AXISRAM);

    RCC->AHB3ENR |= RCC_AHB3ENR_MDMAEN;
    (void)RCC->AHB3ENR;

    while(true)
    {
        if(!sd_ready)
        {
            sd_reset();
            if(!sd_ready)
                Block(clock_cur() + kernel_time::from_ms(1000));
            continue;
        }

        sd_request sdr;
        if(sdt_queue.Pop(&sdr))
        {
            if(sdr.block_count == 0 || !sdr.completion_event || !sdr.mem_address)
            {
                if(sdr.res_out)
                {
                    *sdr.res_out = -2;
                }
                if(sdr.completion_event)
                {
                    sdr.completion_event->Signal();
                }
                continue;
            }

#if PROFILE_SDT
            {
                CriticalGuard cg(s_rtt);
                klog("sdt: %s block %d, block_len %d\n",
                    sdr.is_read ? "read " : "write", sdr.block_start, sdr.block_count);
            }
#endif

            bool is_valid = is_valid_dma(sdr.mem_address, sdr.block_count);
            uint32_t mem_len = sdr.block_count * 512U;
            uint32_t mem_start = is_valid ? (uint32_t)(uintptr_t)sdr.mem_address : mr_unaligned_buf.address;

            if(!is_valid && !sdr.is_read)
            {
                // need to copy memory to the sram buffer
                do_mdma_transfer((uint32_t)(uintptr_t)sdr.mem_address, mr_unaligned_buf.address);
            }

            SDMMC1->DCTRL = 0;
            SDMMC1->DLEN = mem_len;
            SDMMC1->DCTRL = 
                (sdr.is_read ? SDMMC_DCTRL_DTDIR : 0UL) |
                (9UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);
            SDMMC1->CMD = 0;
            SDMMC1->CMD = SDMMC_CMD_CMDTRANS;
            SDMMC1->IDMABASE0 = mem_start;
            SDMMC1->IDMACTRL = SDMMC_IDMA_IDMAEN;

            int cmd_id = 0;
            if(sdr.block_count == 1)
            {
                sd_multi = false;
                if(sdr.is_read)
                {
                    cmd_id = 17;
                }
                else
                {
                    cmd_id = 24;
                }
            }
            else
            {
                sd_multi = true;
                if(sdr.is_read)
                {
                    cmd_id = 18;
                }
                else
                {
                    cmd_id = 25;
                }
            }

#if DEBUG_SD
            {
                CriticalGuard cg(s_rtt);
                klog("sd: pre-command: STA: %x, DCTRL: %x, DCOUNT: %x, DLEN: %x, IDMACTRL: %x, IDMABASE: %x\n",
                    SDMMC1->STA, SDMMC1->DCTRL, SDMMC1->DCOUNT, SDMMC1->DLEN, SDMMC1->IDMACTRL, SDMMC1->IDMABASE0);
            }
#endif

            auto ret = sd_issue_command(cmd_id, resp_type::R1, sdr.block_start * (is_hc ? 1U : 512U), nullptr, true);

            if(ret != 0)
            {
                sd_ready = false;
                if(sdr.res_out)
                {
                    *sdr.res_out = ret;
                }
            }

            sdt_ready.Wait(SimpleSignal::Set, 0U);
            if(sdr.res_out)
            {
                *sdr.res_out = sd_status;
            }
            if(sd_status)
            {
#if DEBUG_SD
                {
                    CriticalGuard cg(s_rtt);
                    klog("sd: post-command FAIL: STA: %x (%x), DCTRL: %x, DCOUNT: %x, DLEN: %x, IDMACTRL: %x, IDMABASE: %x\n",
                        SDMMC1->STA, sd_status, SDMMC1->DCTRL, SDMMC1->DCOUNT, SDMMC1->DLEN, SDMMC1->IDMACTRL, SDMMC1->IDMABASE0);
                }
#endif
                sd_ready = false;
            }
#if DEBUG_SD
            else
            {
                CriticalGuard cg(s_rtt);
                klog("sd: post-command SUCCESS: STA: %x (%x), DCTRL: %x, DCOUNT: %x, DLEN: %x, IDMACTRL: %x, IDMABASE: %x\n",
                    SDMMC1->STA, sd_status, SDMMC1->DCTRL, SDMMC1->DCOUNT, SDMMC1->DLEN, SDMMC1->IDMACTRL, SDMMC1->IDMABASE0);
            }
#endif
            if(sd_multi)
            {
                if(!sdr.is_read)
                {
                    // poll status register until write done
                    while(true)
                    {
                        uint32_t resp;
                        int sr_ret = sd_issue_command(13, resp_type::R1, rca, &resp);
                        if(sr_ret != 0)
                        {
                            sd_ready = false;
                            break;
                        }
                        if(resp != 0xd00)
                        {
                            CriticalGuard cg(s_rtt);
                            klog("sd: not in rcv state: %x\n", resp);
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                // send stop command
                [[maybe_unused]] int stop_ret = sd_issue_command(12, resp_type::R1b);
#if DEBUG_SD
                {
                    CriticalGuard cg(s_rtt);
                    klog("sd: post-stop command: ret: %x, STA: %x\n",
                        stop_ret, SDMMC1->STA);
                }
#endif
                sd_multi = false;
            }

            if(!is_valid && sdr.is_read)
            {
                // need to copy memory from the sram buffer
                do_mdma_transfer(mr_unaligned_buf.address, (uint32_t)(uintptr_t)sdr.mem_address);
            }

            if(sdr.completion_event)
            {
                sdr.completion_event->Signal();
            }
        }
    }
}

int sd_perform_transfer_async(const sd_request &req)
{
    return sdt_queue.Push(req) ? 0 : -1;
}

static int sd_perform_transfer_int(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read)
{
    auto t = GetCurrentThreadForCore();
    auto cond = &t->ss;
    auto ret = (int *)&t->ss_p.ival1;
    *ret = -4;

    sd_request req;
    req.block_start = block_start;
    req.block_count = block_count;
    req.mem_address = mem_address;
    req.is_read = is_read;
    req.completion_event = cond;
    req.res_out = ret;

    /* M7 cache lines are 32 bytes, so on an unaligned read the subsequent invalidate will
        delete valid data in the cache - often this is part of the lwext4 structs, so
        is vaguely important.

        To avoid this, if we are unaligned then also clean the cache here so that the memory
        has the correct data once we invalidate it again later (after a read). */
    auto mem_start = (uint32_t)mem_address;
    auto mem_end = mem_start + block_count * 512U;
    auto cache_start = mem_start & ~0x1fU;
    auto cache_end = (mem_end + 0x1fU) & ~0x1fU;

    if(!is_read)
    {
        // writes need to commit all cache contents to ram first
        CleanM7Cache(cache_start, cache_end - cache_start, CacheType_t::Data);
    }
    else
    {
        if(cache_start != mem_start)
        {
            // commit first cache line for unaligned reads
            CleanM7Cache(cache_start, 32, CacheType_t::Data);
        }
        if(cache_end != mem_end)
        {
            // commit last cache line for unaligned reads
            CleanM7Cache(cache_end - 32, 32, CacheType_t::Data);
        }
    }

    auto send_ret = sd_perform_transfer_async(req);
    if(send_ret)
    {
        return send_ret;
    }
    
    cond->Wait(SimpleSignal::Set, 0UL);

    if(is_read)
    {
        // bring data back into cache if necessary
        InvalidateM7Cache(cache_start, cache_end - cache_start, CacheType_t::Data);
    }

    int cret = *ret;

    if(cret != 0)
    {
        CriticalGuard cg(s_rtt);
        klog("sd_perform_transfer %s of %d blocks at %x failed: %x, DCOUNT: %x, DCTRL: %x, IDMACTRL: %x, IDMABASE: %x, IDMASIZE: %x, retrying\n",
            is_read ? "read" : "write", block_count, (uint32_t)(uintptr_t)mem_address, cret,
            sd_dcount, sd_dctrl, sd_idmactrl, sd_idmabase, sd_idmasize);
    }

    return cret;
}

static int sd_perform_unaligned_transfer_int(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read)
{
    auto t = GetCurrentThreadForCore();
    auto cond = &t->ss;
    auto ret = (int *)&t->ss_p.ival1;
    *ret = -4;

    sd_request req;
    req.block_start = block_start;
    req.block_count = block_count;
    req.mem_address = mem_address;
    req.is_read = is_read;
    req.completion_event = cond;
    req.res_out = ret;

    /* M7 cache lines are 32 bytes, so on an unaligned read the subsequent invalidate will
        delete valid data in the cache - often this is part of the lwext4 structs, so
        is vaguely important.

        To avoid this, if we are unaligned then also clean the cache here so that the memory
        has the correct data once we invalidate it again later (after a read). */
    auto mem_start = (uint32_t)mem_address;
    auto mem_end = mem_start + block_count * 512U;
    auto cache_start = mem_start & ~0x1fU;
    auto cache_end = (mem_end + 0x1fU) & ~0x1fU;

    if(!is_read)
    {
        // writes need to commit all cache contents to ram first
        CleanM7Cache(cache_start, cache_end - cache_start, CacheType_t::Data);
    }
    else
    {
        if(cache_start != mem_start)
        {
            // commit first cache line for unaligned reads
            CleanM7Cache(cache_start, 32, CacheType_t::Data);
        }
        if(cache_end != mem_end)
        {
            // commit last cache line for unaligned reads
            CleanM7Cache(cache_end - 32, 32, CacheType_t::Data);
        }
    }

    auto send_ret = sd_perform_transfer_async(req);
    if(send_ret)
    {
        return send_ret;
    }
    
    cond->Wait(SimpleSignal::Set, 0UL);

    if(is_read)
    {
        InvalidateM7Cache(cache_start, cache_end - cache_start, CacheType_t::Data);
    }

    int cret = *ret;

    if(cret != 0)
    {
        CriticalGuard cg(s_rtt);
        klog("sd_perform_transfer %s of %d blocks at %x failed: %x, DCOUNT: %x, DCTRL: %x, IDMACTRL: %x, IDMABASE: %x, IDMASIZE: %x, retrying\n",
            is_read ? "read" : "write", block_count, (uint32_t)(uintptr_t)mem_address, cret,
            sd_dcount, sd_dctrl, sd_idmactrl, sd_idmabase, sd_idmasize);
    }

    return cret;
}


static int sd_perform_unaligned_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read, int nretries)
{
    int ret = 0;
    uint32_t nblocks_sent = 0;
    while(nblocks_sent < block_count)
    {
        bool failed = true;
        // only ever one block at a time for unaligned transfers
        for(int i = 0; i < nretries; i++)
        {
            ret = sd_perform_unaligned_transfer_int(block_start + nblocks_sent, 1,
                (void *)(((char *)mem_address) + (512U * nblocks_sent)), is_read);
            
            if(ret == 0)
            {
                failed = false;
                break;
            }
        }

        if(failed)
        {
            CriticalGuard cg(s_rtt);
            klog("sd_perform_unaligned_transfer %s of %d blocks at %x failed: %x\n",
                is_read ? "read" : "write", block_count, (uint32_t)(uintptr_t)mem_address, ret);
            return ret;
        }

        nblocks_sent++;
    }

    return ret;    
}

int sd_perform_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read, int nretries)
{
    /* Check mem_address is valid for direct SD DMA */
    if(!is_valid_dma(mem_address, block_count))
    {
        {
            CriticalGuard cg(s_rtt);
            klog("sd_perform_transfer: unaligned transfer\n");
        }
        return sd_perform_unaligned_transfer(block_start, block_count,
            mem_address, is_read, nretries);
    }

    int ret = 0;
    for(int i = 0; i < nretries; i++)
    {
        ret = sd_perform_transfer_int(block_start, block_count,
            mem_address, is_read);
        
        if(ret == 0)
        {
            return 0;
        }
    }

    {
        CriticalGuard cg(s_rtt);
        klog("sd_perform_transfer %s of %d blocks at %x failed: %x\n",
            is_read ? "read" : "write", block_count, (uint32_t)(uintptr_t)mem_address, ret);
    }

    return ret;
}

extern "C" void SDMMC1_IRQHandler()
{
    auto const errors = DCRCFAIL |
        DTIMEOUT |
        TXUNDERR | RXOVERR;
    auto sta = SDMMC1->STA;
    if(sta & errors)
    {
        SDMMC1->DCTRL |= SDMMC_DCTRL_FIFORST;
        
        sd_status = sta;
        sd_dcount = SDMMC1->DCOUNT;
        sd_idmabase = SDMMC1->IDMABASE0;
        sd_idmactrl = SDMMC1->IDMACTRL;
        sd_idmasize = SDMMC1->IDMABSIZE;
        sd_dctrl = SDMMC1->DCTRL;
        sd_ready = false;
        sdt_ready.Signal();
    }
    else if(sta & DATAEND)
    {
        sd_status = 0;
        sdt_ready.Signal();
    }
    else
    {
    }

    SDMMC1->ICR = sta & (errors | DATAEND) & 0x4005ff;
}

bool sd_get_ready()
{
    //printf("SD status %lx\n", sd_status);
    return sd_ready;
}

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
