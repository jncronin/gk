#include "stm32h7xx.h"
#include "clocks.h"
#include "sd.h"
#include <cstdio>
#include "pins.h"
#include "osqueue.h"
#include "osmutex.h"
#include "SEGGER_RTT.h"
#include "scheduler.h"

#define DEBUG_SD    1

extern Spinlock s_rtt;
extern Scheduler s;
extern Process kernel_proc;

#define SDCLK   192000000

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
SDT_DATA static volatile bool sd_multi = false;
__attribute__((section(".sram4"))) volatile uint64_t sd_size = 0;

SDT_DATA static uint32_t cmd6_buf[512/32];

extern char _ssdt_data, _esdt_data;

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

static void sd_thread(void *param);

static int sd_issue_command(uint32_t command, resp_type rt, uint32_t arg = 0, uint32_t *resp = nullptr, 
    bool with_data = false,
    bool ignore_crc = false,
    int timeout_retry = 10)
{
    int tcnt = 0;
    for(; tcnt < timeout_retry; tcnt++)
    {
#ifdef DEBUG_SD
        ITM_SendChar('m');
        if(tcnt > 0)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "sd_issue_command: retry %d for command %lu, sta: %lx\n", tcnt, command,
                SDMMC1->STA);
        }
#endif

        // For now, error here if there are unhandled CMD flags
        const auto cmd_flags = CCRCFAIL | CTIMEOUT | CMDREND | CMDSENT | TXUNDERR | RXOVERR;

        if(SDMMC1->STA & cmd_flags)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "sd_issue_command: unhandled flags: %lx\n", SDMMC1->STA);
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

        SDMMC1->ARG = arg;

        uint32_t start_data = with_data ? SDMMC_CMD_CMDTRANS : 0UL;

        SDMMC1->CMD = command | (waitresp << SDMMC_CMD_WAITRESP_Pos) | SDMMC_CMD_CPSMEN | start_data;

        if(rt == resp_type::None)
        {
            while(!(SDMMC1->STA & CMDSENT));      // TODO: WFI
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "sd_issue_command: sent %lu no response expected\n", command);
            SDMMC1->ICR = CMDSENT;
            return 0;
        }

        bool timeout = false;

        while(!(SDMMC1->STA & CMDREND))
        {
            if(SDMMC1->STA & CCRCFAIL)
            {
                if(ignore_crc)
                {
                    SDMMC1->ICR = CCRCFAIL;
                    break;
                }

                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "sd_issue_command: sent %lu invalid crc response\n", command);
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
#ifdef DEBUG_SD
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "sd_issue_command: timeout, sta: %lx, cmdr: %lx, dctrl: %lx\n",
                    SDMMC1->STA, SDMMC1->CMD, SDMMC1->DCTRL);
            }
#endif
            delay_ms(tcnt * 5);
            continue;
        }

    #ifdef DEBUG_SD
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "sd_issue_command: sent %lu received reponse", command);
        }
    #endif

        if(resp)
        {
            auto nresp = (rt == resp_type::R2 ? 4 : 1);
            for(int i = 0; i < nresp; i++)
            {
                resp[nresp - i - 1] = (&SDMMC1->RESP1)[i];
    #ifdef DEBUG_SD
                {
                    CriticalGuard cg(s_rtt);
                    SEGGER_RTT_printf(0, " %lx", resp[nresp - i - 1]);
                }
    #endif
            }
        }

        
    #ifdef DEBUG_SD
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "\n");
        }
    #endif

        SDMMC1->ICR = CMDREND;
        return 0;
    }

    // timeout
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "sd_issue_command: sent %lu command timeout\n", command);
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
        SEGGER_RTT_printf(0, "SDIO clock set to %d (div = %d)\n", clock_speed, div);
    }

    clock_period_ns = 1000000000 / clock_speed;
}


void init_sd()
{
    uint32_t data_start = (uint32_t)&_ssdt_data;
    uint32_t data_end = (uint32_t)&_esdt_data;

    s.Schedule(Thread::Create("sd", sd_thread, nullptr, true, 5, kernel_proc, 
        Either, InvalidMemregion(),
        MPUGenerate(data_start, data_end - data_start, 6, false, RW, NoAccess, WBWA_NS)));
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
        SEGGER_RTT_printf(0, "init_sd: CMD8 timed out - assuming <v2 card\n");
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
            SEGGER_RTT_printf(0, "init_sd: CMD8 invalid response %lx\n", resp[0]);
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
        SEGGER_RTT_printf(0, "init_sd: invalid voltage range\n");
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
                SEGGER_RTT_printf(0, "init_sd: %s capacity card ready\n", is_hc ? "high" : "normal");
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
        SEGGER_RTT_printf(0, "init_sd: card error %lx\n", cmd3_ret & 0xffff);
        return;
    }
    if(!(cmd3_ret & (1UL << 8)))
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "init_sd: card not ready for data\n");
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
        SEGGER_RTT_printf(0, "init_sd: card not in standby state\n");
        return;
    }

    // Send CMD9 to get cards CSD (needed to calculate card size)
    ret = sd_issue_command(9, resp_type::R2, rca, csd);
    if(ret != 0)
        return;

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "init_sd: CSD: %lx, %lx, %lx, %lx\n", csd[0], csd[1], csd[2], csd[3]);
        SEGGER_RTT_printf(0, "init_sd: sd card size %lu kB\n", sd_get_size() / 1024);
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
        SEGGER_RTT_printf(0, "init_sd: card not in transfer state\n");
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
            SEGGER_RTT_printf(0, "init_sd: dcrc fail\n");
            return;
        }
        if(SDMMC1->STA & DTIMEOUT)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "init_sd: dtimeout\n");
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
        SEGGER_RTT_printf(0, "init_sd: scr %lx %lx, dcount %lx, sta %lx\n", scr[0], scr[1], SDMMC1->DCOUNT, SDMMC1->STA);
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
            SEGGER_RTT_printf(0, "init_sd: set to 4-bit mode\n");
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
                SEGGER_RTT_printf(0, "dcrcfail\n");
                return;
            }
            if(SDMMC1->STA & DTIMEOUT)
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "dtimeout\n");
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
            SEGGER_RTT_printf(0, "init_sd: cmd6: fg1_support: %lx\n", fg1_support);
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
                    SEGGER_RTT_printf(0, "dcrcfail\n");
                    return;
                }
                if(SDMMC1->STA & DTIMEOUT)
                {
                    CriticalGuard cg(s_rtt);
                    SEGGER_RTT_printf(0, "dtimeout\n");
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
                SEGGER_RTT_printf(0, "init_sd: cmd6: fg1_setting: %lx\n", fg1_setting);
            }

            if(fg1_setting == 1)
            {
                SDMMC_set_clock(48000000);
                SDMMC1->DTIMER = timeout_ns / clock_period_ns;
                {
                    CriticalGuard cg(s_rtt);
                    SEGGER_RTT_printf(0, "init_sd: set to high speed mode\n");
                }
            }
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
        SEGGER_RTT_printf(0, "init_sd: success\n");
    }
    tfer_inprogress = false;
    sd_ready = true;
}

// sd thread function
void sd_thread(void *param)
{
    (void)param;

    while(true)
    {
        if(!sd_ready)
        {
            sd_reset();
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
                continue;
            }

            SDMMC1->DCTRL = 0;
            SDMMC1->DLEN = sdr.block_count * 512;
            SDMMC1->DCTRL = 
                (sdr.is_read ? SDMMC_DCTRL_DTDIR : 0UL) |
                (9UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);
            SDMMC1->CMD = 0;
            SDMMC1->CMD = SDMMC_CMD_CMDTRANS;
            SDMMC1->IDMABASE0 = (uint32_t)(uintptr_t)sdr.mem_address;
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

            sdt_ready.Reset();
            auto ret = sd_issue_command(cmd_id, resp_type::R1, sdr.block_start * (is_hc ? 1 : 512), nullptr, true);

            if(ret != 0)
            {
                sd_ready = false;
                if(sdr.res_out)
                {
                    *sdr.res_out = ret;
                }
            }

            sdt_ready.Wait();
            if(sdr.res_out)
            {
                *sdr.res_out = sd_status;
            }
            if(sd_status)
            {
                sd_ready = false;
            }
            else if(sd_multi)
            {
                // send stop command
                sd_issue_command(12, resp_type::R1b);
                sd_multi = false;
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
    SRAM4RegionAllocator<SimpleSignal> ralloc;
    auto cond = ralloc.allocate(1);
    if(!cond)
        return -3;
    new(cond) SimpleSignal();

    SRAM4RegionAllocator<int> ialloc;
    auto ret = ialloc.allocate(1);
    if(!ret)
        return -5;
    *ret = -4;

    sd_request req;
    req.block_start = block_start;
    req.block_count = block_count;
    req.mem_address = mem_address;
    req.is_read = is_read;
    req.completion_event = cond;
    req.res_out = ret;

    if(!is_read)
    {
        SCB_CleanDCache_by_Addr((uint32_t *)mem_address, block_count * 512);
    }

    auto send_ret = sd_perform_transfer_async(req);
    if(send_ret)
    {
        cond->~SimpleSignal();
        ralloc.deallocate(cond, 1);
        ialloc.deallocate(ret, 1);
        return send_ret;
    }
    
    cond->Wait();

    if(is_read)
    {
        SCB_InvalidateDCache_by_Addr((uint32_t *)mem_address, block_count * 512);
    }

    int cret = *ret;

    cond->~SimpleSignal();
    ralloc.deallocate(cond, 1);
    ialloc.deallocate(ret, 1);

    if(cret != 0)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "sd_perform_transfer %s of %d blocks at %x failed: %x\n",
            is_read ? "read" : "write", block_count, (uint32_t)(uintptr_t)mem_address, cret);
    }

    return cret;
}

int sd_perform_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read, int nretries)
{
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
        SEGGER_RTT_printf(0, "sd_perform_transfer %s of %d blocks at %x failed: %x\n",
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
        sd_status = SDMMC1->STA;
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