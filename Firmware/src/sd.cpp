#include "stm32h7xx.h"
#include "clocks.h"
#include "sd.h"
#include <cstdio>
#include "pins.h"

#define SDCLK   192000000

static constexpr pin sd_pins[] =
{
    { GPIOC, 8, 12 },
    { GPIOC, 9, 12 },
    { GPIOC, 10, 12 },
    { GPIOC, 11, 12 },
    { GPIOC, 12, 12 },
    { GPIOD, 2, 12 }
};

static int clock_speed = 0;
static int clock_period_ns = 0;

static uint32_t cid[4];
static uint32_t csd[4];
static uint32_t rca;
static uint32_t scr[2];
static bool is_4bit = false;
volatile bool sd_ready = false;
static bool is_hc = false;
static volatile bool tfer_inprogress = false;
static volatile bool dma_ready = false;
static volatile uint32_t sd_status = 0;
static volatile bool sd_multi = false;

static uint32_t cmd6_buf[512/32];

SemaphoreHandle_t sd_sem;
TaskHandle_t sd_task = nullptr;

enum class resp_type { None, R1, R1b, R2, R3, R4, R4b, R5, R6, R7 };
enum class data_dir { None, ReadBlock, WriteBlock, ReadStream, WriteStream };

enum StatusFlags { CCRCFAIL = 1, DCRCFAIL = 2, CTIMEOUT = 4, DTIMEOUT = 8,
    TXUNDERR = 0x10, RXOVERR = 0x20, CMDREND = 0x40, CMDSENT = 0x80,
    DATAEND = 0x100, /* reserved */ DBCKEND = 0x400, CMDACT = 0x800,
    TXACT = 0x1000, RXACT = 0x2000, TXFIFOHE = 0x4000, RXFIFOHF = 0x8000,
    TXFIFOF = 0x10000, RXFIFOF = 0x20000, TXFIFOE = 0x40000, RXFIFOE = 0x80000,
    TXDAVL = 0x100000, RXDAVL = 0x200000, SDIOIT = 0x400000 };

static int sd_issue_command(uint32_t command, resp_type rt, uint32_t arg = 0, uint32_t *resp = nullptr, bool ignore_crc = false,
    int timeout_retry = 10)
{
    int tcnt = 0;
    for(; tcnt < timeout_retry; tcnt++)
    {
#ifdef DEBUG_SD
        ITM_SendChar('m');
        if(tcnt > 0)
        {
            printf("sd_issue_command: retry %i for command %lu\n", tcnt, command);
        }
#endif

        // For now, error here if there are unhandled CMD flags
        const auto cmd_flags = CCRCFAIL | CTIMEOUT | CMDREND | CMDSENT;

        if(SDMMC1->STA & cmd_flags)
        {
            printf("sd_issue_command: unhandled flags: %lx\n", SDMMC1->STA);
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
            case resp_type::R3:
            case resp_type::R4:
            case resp_type::R4b:
            case resp_type::R5:
            case resp_type::R6:
            case resp_type::R7:
                waitresp = 1;
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

        SDMMC1->CMD = command | (waitresp << 6) | SDMMC_CMD_CPSMEN;

        if(rt == resp_type::None)
        {
            while(!(SDMMC1->STA & CMDSENT));      // TODO: WFI
            printf("sd_issue_command: sent %lu no response expected\n", command);
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

                printf("sd_issue_command: sent %lu invalid crc response\n", command);
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
            if(tcnt)
                vTaskDelay(pdMS_TO_TICKS(tcnt * 5));
            continue;
        }

    #ifdef DEBUG_SD
        printf("sd_issue_command: sent %lu received reponse", command);
    #endif

        if(resp)
        {
            auto nresp = (rt == resp_type::R2 ? 4 : 1);
            for(int i = 0; i < nresp; i++)
            {
                resp[nresp - i - 1] = (&SDMMC1->RESP1)[i];
    #ifdef DEBUG_SD
                printf(" %lx", resp[i]);
    #endif
            }
        }

        
    #ifdef DEBUG_SD
        printf("\n");
    #endif

        SDMMC1->ICR = CMDREND;
        return 0;
    }

    // timeout
    printf("sd_issue_command: sent %lu command timeout\n", command);
    return CTIMEOUT;
}

static void SDMMC_set_clock(int freq)
{
    // Anything less than 48 MHz needs a divider
    auto div = SDCLK / freq;
    auto rem = SDCLK - (freq * div);
    if(rem)
        div++;
    
    div -= 1;
    if(div > 1023) div = 1023;
    if(div < 0) div = 0;

    auto clkcr = SDMMC1->CLKCR;
    clkcr &= ~SDMMC_CLKCR_CLKDIV_Msk;
    clkcr |= div;
    SDMMC1->CLKCR;

    clock_speed = SDCLK / (div + 1);
    printf("SDIO clock set to %d (div = %d)\n", clock_speed, div);

    clock_period_ns = 1000000000 / clock_speed;
}


void init_sd()
{
    sd_sem = xSemaphoreCreateMutex();
    if(!sd_sem)
    {
        printf("Couldn't allocate sd mutex\n");
        while(true);
    }
}

void sd_reset()
{
    sd_ready = false;
    // Assume SD is already inserted, panic otherwise


    // Init DMA2 for SD reads (we use DMA1 for I2S writes, overkill but allows truly concurrent DMA)
    //  SDIO is mapped to Stream 3, Channel 4
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;

    // Disable DMA channel
    DMA2_Stream3->CR = 0;


    /* Pins:
        D0:     PC8
        D1:     PC9
        D2:     PC10
        D3:     PC11
        CK:     PC12
        CMD:    PD2

        all AF12, all push-pull
        */

    // Reset SDIO
    RCC->AHB3RSTR = RCC_AHB3RSTR_SDMMC1RST;
    (void)RCC->AHB3RSTR;
    RCC->AHB3RSTR = 0;
    (void)RCC->AHB3RSTR;

    
    // Clock SDIO
    RCC->AHB3ENR |= RCC_AHB3ENR_SDMMC1EN;
    (void)RCC->AHB3ENR;

    for(const auto &p : sd_pins)
    {
        p.set_as_af();
    }

    /* Input clock is 192 MHz
        We need to set to < 400 kHz for start-up
    */
    SDMMC1->CLKCR = 0;    // reset value - ensure 1 bit data bus
    SDMMC1->MASK = 0;     // no interrupts during init
    is_4bit = false;
    SDMMC_set_clock(350000);
    
    // Clock SDIO
    SDMMC1->POWER = SDMMC_POWER_PWRCTRL_Msk;
    (void)SDMMC1->POWER;
    while(SDMMC1->POWER != SDMMC_POWER_PWRCTRL_Msk);
    printf("SDIO clock started\n");

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
        printf("init_sd: CMD8 timed out - assuming <v2 card\n");
    }
    else if(ret == 0)
    {
        if((resp[0] & 0x1ff) == 0x1aa)
        {
            is_v2 = true;
        }
        else
        {
            printf("init_sd: CMD8 invalid response %lx\n", resp[0]);
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
        printf("init_sd: invalid voltage range\n");
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

            printf("init_sd: %s capacity card ready\n", is_hc ? "high" : "normal");
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
        printf("init_sd: card error %lx\n", cmd3_ret & 0xffff);
        return;
    }
    if(!(cmd3_ret & (1UL << 8)))
    {
        printf("init_sd: card not ready for data\n");
        return;
    }
    rca = cmd3_ret & 0xffff0000;

    // We know the card is an SD card so can cope with 25 MHz
    SDMMC_set_clock(25000000);

    // Get card status - should be in data transfer mode, standby state
    ret = sd_issue_command(13, resp_type::R1, rca, resp);
    if(ret != 0)
        return;
    
    if(resp[0] != 0x700)
    {
        printf("init_sd: card not in standby state\n");
        return;
    }

    // Send CMD9 to get cards CSD (needed to calculate card size)
    ret = sd_issue_command(9, resp_type::R2, rca, csd);
    if(ret != 0)
        return;

    printf("init_sd: CSD: %lx, %lx, %lx, %lx\n", csd[0], csd[1], csd[2], csd[3]);
    printf("init_sd: sd card size %llu bytes\n", sd_get_size());

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
        printf("init_sd: card not in transfer state\n");
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
    while(SDMMC1->STA & RXDAVL)
        (void)SDMMC1->FIFO;

    // Read SCR register - 64 bits from card in transfer mode - ACMD51 with data
    ret = sd_issue_command(55, resp_type::R1, rca);
    if(ret != 0)
        return;

    
    // read data
    //int timeout_ns = 20000000;   // 20ms
    int timeout_ns = 200000000;
    SDMMC1->DTIMER = timeout_ns / clock_period_ns;
    SDMMC1->DLEN = 8;
    SDMMC1->DCTRL = SDMMC_DCTRL_DTEN |
        SDMMC_DCTRL_DTDIR |
        (3UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);
    
    ret = sd_issue_command(51, resp_type::R1, 0, resp);
    if(ret != 0)
        return;

    while(!(SDMMC1->STA & DBCKEND) && !(SDMMC1->STA & DATAEND) && !(SDMMC1->STA & DTIMEOUT))
    {
        if(SDMMC1->STA & DCRCFAIL)
        {
            printf("init_sd: dcrc fail\n");
            return;
        }
        if(SDMMC1->STA & DTIMEOUT)
        {
            printf("init_sd: dtimeout\n");
        }
    }
    SDMMC1->ICR = DBCKEND | DATAEND;
    
    for(int i = 0; i < 2; i++)
    {
        scr[1 - i] = __builtin_bswap32(SDMMC1->FIFO);
    }

    printf("init_sd: scr %lx %lx, dcount %lx, sta %lx\n", scr[0], scr[1], SDMMC1->DCOUNT, SDMMC1->STA);

    // can we use 4-bit signalling?
    if((scr[1] & (0x5UL << (48-32))) == (0x5UL << (48-32)))
    {
        ret = sd_issue_command(55, resp_type::R1, rca);
        if(ret != 0)
            return;
        ret = sd_issue_command(6, resp_type::R1, 2);
        if(ret != 0)
            return;

        printf("init_sd: set to 4-bit mode\n");
        is_4bit = true;
        SDMMC1->CLKCR |= SDMMC_CLKCR_WIDBUS_0;
    }

    // can we use 48 MHz interface?
    if(((scr[1] >> (56 - 32)) & 0xfU) >= 1)
    {
        // supports CMD6

        // send inquiry CMD6
        SDMMC1->DLEN = 512/8;
        SDMMC1->DCTRL = SDMMC_DCTRL_DTEN |
            SDMMC_DCTRL_DTDIR |
            (6UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);

        ret = sd_issue_command(6, resp_type::R1, 0);
        if(ret != 0)
            return;
        
        // read response
        for(int i = 0; i < 512/32; i++)
        {
            while(!(SDMMC1->STA & RXDAVL) && !(SDMMC1->STA & DCRCFAIL) && !(SDMMC1->STA & DTIMEOUT));
            if(SDMMC1->STA & DCRCFAIL)
            {
                printf("dcrcfail\n");
                return;
            }
            if(SDMMC1->STA & DTIMEOUT)
            {
                printf("dtimeout\n");
                return;
            }
            cmd6_buf[512/32 - 1 - i] = __builtin_bswap32(SDMMC1->FIFO);
        }

        while(!(SDMMC1->STA & DBCKEND) && !(SDMMC1->STA & DATAEND) && !(SDMMC1->STA & DTIMEOUT))
        {
            if(SDMMC1->STA & DCRCFAIL)
            {
                printf("init_sd: dcrc fail\n");
                return;
            }
            if(SDMMC1->STA & DTIMEOUT)
            {
                printf("init_sd: dtimeout\n");
            }
        }
        SDMMC1->ICR = DBCKEND | DATAEND;

        // can we enable high speed mode? - check bits 415:400
        auto fg1_support = (cmd6_buf[12] >> 16) & 0xffffUL;
        printf("init_sd: cmd6: fg1_support: %lx\n", fg1_support);

        if(fg1_support & 0x2)
        {
            // try and switch to high speed mode
            SDMMC1->DLEN = 512/8;
            SDMMC1->DCTRL = SDMMC_DCTRL_DTEN |
                SDMMC_DCTRL_DTDIR |
                (6UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);
            
            ret = sd_issue_command(6, resp_type::R1, 0x80000001);
            if(ret != 0)
                return;
            
            // read response
            for(int i = 0; i < 512/32; i++)
            {
                while(!(SDMMC1->STA & RXDAVL) && !(SDMMC1->STA & DCRCFAIL) && !(SDMMC1->STA & DTIMEOUT));
                if(SDMMC1->STA & DCRCFAIL)
                {
                    printf("dcrcfail\n");
                    return;
                }
                if(SDMMC1->STA & DTIMEOUT)
                {
                    printf("dtimeout\n");
                    return;
                }
                cmd6_buf[512/32 - 1 - i] = __builtin_bswap32(SDMMC1->FIFO);
            }

            while(!(SDMMC1->STA & DBCKEND) && !(SDMMC1->STA & DATAEND) && !(SDMMC1->STA & DTIMEOUT))
            {
                if(SDMMC1->STA & DCRCFAIL)
                {
                    printf("init_sd: dcrc fail\n");
                    return;
                }
                if(SDMMC1->STA & DTIMEOUT)
                {
                    printf("init_sd: dtimeout\n");
                }
            }
            SDMMC1->ICR = DBCKEND | DATAEND;

            auto fg1_setting = (cmd6_buf[11] >> 24) & 0xfUL;
            printf("init_sd: cmd6: fg1_setting: %lx\n", fg1_setting);

            if(fg1_setting == 1)
            {
                SDMMC_set_clock(48000000);
                printf("init_sd: set to high speed mode\n");
            }

        }
    }

    // Hardware flow control - prevents buffer under/overruns
    SDMMC1->CLKCR |= SDMMC_CLKCR_HWFC_EN;

    // Init DMA2 for SD reads (we use DMA1 for I2S writes, overkill but allows truly concurrent DMA)
    //  SDIO is mapped to Stream 3, Channel 4
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;

    // Enable interrupts
    SDMMC1->MASK = DCRCFAIL | DTIMEOUT |
        TXUNDERR | RXOVERR | DATAEND;
    //NVIC_SetPriority(DMA2_Stream3_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    //NVIC_SetPriority(SDMMC1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    NVIC_EnableIRQ(SDMMC1_IRQn);
    
    printf("init_sd: success\n");
    tfer_inprogress = false;
    sd_ready = true;


}

int sd_read_block_async(uint32_t block_addr, void *ptr)
{
    // only one thread can use SD at a time
    while(!xSemaphoreTake(sd_sem, portMAX_DELAY));
    sd_task = xTaskGetCurrentTaskHandle();
    sd_status = 0;
    sd_multi = false;

    while(!sd_ready)
        sd_reset();

    SDMMC1->ICR = 0xffffffff;
    
    SDMMC1->DLEN = 512;

    //while(SDMMC1->STA & (SDMMC_STA_TXACT | SDMMC_STA_RXACT | SDMMC_STA_CMDACT));

    // set up DMA
    while(DMA2_Stream3->CR & DMA_SxCR_EN);

#ifdef DEBUG_SD
    ITM_SendChar('\n');
    ITM_SendChar('r');
#endif

    DMA2->LIFCR = DMA_LIFCR_CDMEIF3 |
        DMA_LIFCR_CTEIF3 |
        DMA_LIFCR_CHTIF3 |
        DMA_LIFCR_CTCIF3;
    DMA2_Stream3->CR = 0;
    DMA2_Stream3->PAR = (uint32_t)(uintptr_t)&SDMMC1->FIFO;
    DMA2_Stream3->M0AR = (uint32_t)(uintptr_t)ptr;
    //DMA2_Stream3->NDTR = 128;
    DMA2_Stream3->CR = DMA_SxCR_CHSEL_2 |
        DMA_SxCR_MSIZE_1 |
        DMA_SxCR_PSIZE_1 |
        DMA_SxCR_MINC |
        DMA_SxCR_MBURST_0 |
        DMA_SxCR_PBURST_0 |
        DMA_SxCR_PFCTRL |
        DMA_SxCR_TEIE /*|
        DMA_SxCR_TCIE*/;
    DMA2_Stream3->FCR = DMA_SxFCR_FTH_Msk |
        DMA_SxFCR_DMDIS /*|
        DMA_SxFCR_FEIE */;
    DMA2_Stream3->CR |= DMA_SxCR_EN;

    SDMMC1->DCTRL = SDMMC_DCTRL_DTEN |
        SDMMC_DCTRL_DTDIR |
        (9UL << SDMMC_DCTRL_DBLOCKSIZE_Pos) |
        SDMMC_DCTRL_DMAEN;

    auto ret = sd_issue_command(17, resp_type::R1, block_addr * (is_hc ? 1 : 512));
    if(ret != 0)
    {
        xSemaphoreGive(sd_sem);
        return ret;
    }    

    return 0;
}

int sd_read_blocks_async(uint32_t block_addr, uint32_t block_count, void *ptr)
{
    // only one thread can use SD at a time
    while(!xSemaphoreTake(sd_sem, portMAX_DELAY));
    sd_task = xTaskGetCurrentTaskHandle();
    sd_status = 0;
    sd_multi = true;

    while(!sd_ready)
        sd_reset();

    SDMMC1->ICR = 0xffffffff;
    
    SDMMC1->DLEN = 512 * block_count;

    while(SDMMC1->STA & (SDMMC_STA_TXACT | SDMMC_STA_RXACT | SDMMC_STA_CMDACT));

    // set up DMA
    while(DMA2_Stream3->CR & DMA_SxCR_EN);

#ifdef DEBUG_SD
    ITM_SendChar('\n');
    ITM_SendChar('R');
#endif

    DMA2->LIFCR = DMA_LIFCR_CDMEIF3 |
        DMA_LIFCR_CTEIF3 |
        DMA_LIFCR_CHTIF3 |
        DMA_LIFCR_CTCIF3;
    DMA2_Stream3->CR = 0;
    DMA2_Stream3->PAR = (uint32_t)(uintptr_t)&SDMMC1->FIFO;
    DMA2_Stream3->M0AR = (uint32_t)(uintptr_t)ptr;
    //DMA2_Stream3->NDTR = 128 * block_count;
    DMA2_Stream3->CR = DMA_SxCR_CHSEL_2 |
        DMA_SxCR_MSIZE_1 |
        DMA_SxCR_PSIZE_1 |
        DMA_SxCR_MINC |
        DMA_SxCR_MBURST_0 |
        DMA_SxCR_PBURST_0 |
        DMA_SxCR_PFCTRL |
        DMA_SxCR_TEIE /*|
        DMA_SxCR_TCIE*/;
    DMA2_Stream3->FCR = DMA_SxFCR_FTH_Msk |
        DMA_SxFCR_DMDIS /*|
        DMA_SxFCR_FEIE*/;
    DMA2_Stream3->CR |= DMA_SxCR_EN;

    SDMMC1->DCTRL = SDMMC_DCTRL_DTEN |
        SDMMC_DCTRL_DTDIR |
        (9UL << SDMMC_DCTRL_DBLOCKSIZE_Pos) |
        SDMMC_DCTRL_DMAEN;

    auto ret = sd_issue_command(18, resp_type::R1, block_addr * (is_hc ? 1 : 512));
    if(ret != 0)
    {
        xSemaphoreGive(sd_sem);
        return ret;
    }    

    return 0;
}

int sd_write_block_async(uint32_t block_addr, const void *ptr)
{
    // only one thread can use SD at a time
    while(!xSemaphoreTake(sd_sem, portMAX_DELAY));
    sd_task = xTaskGetCurrentTaskHandle();
    sd_status = 0;
    sd_multi = false;

    while(!sd_ready)
        sd_reset();

    SDMMC1->ICR = 0xffffffff;
    
    SDMMC1->DLEN = 512;

    while(SDMMC1->STA & (SDMMC_STA_TXACT | SDMMC_STA_RXACT | SDMMC_STA_CMDACT));

    // set up DMA
    while(DMA2_Stream3->CR & DMA_SxCR_EN);

#ifdef DEBUG_SD
    ITM_SendChar('\n');
    ITM_SendChar('w');
#endif

    DMA2->LIFCR = DMA_LIFCR_CDMEIF3 |
        DMA_LIFCR_CTEIF3 |
        DMA_LIFCR_CHTIF3 |
        DMA_LIFCR_CTCIF3;
    DMA2_Stream3->CR = 0;
    DMA2_Stream3->PAR = (uint32_t)(uintptr_t)&SDMMC1->FIFO;
    DMA2_Stream3->M0AR = (uint32_t)(uintptr_t)ptr;
    //DMA2_Stream3->NDTR = 128;
    DMA2_Stream3->CR = DMA_SxCR_CHSEL_2 |
        DMA_SxCR_MSIZE_1 |
        DMA_SxCR_PSIZE_1 |
        DMA_SxCR_MINC |
        DMA_SxCR_MBURST_0 |
        DMA_SxCR_PBURST_0 |
        DMA_SxCR_PFCTRL |
        DMA_SxCR_TEIE |
        DMA_SxCR_DIR_0 /*|
        DMA_SxCR_TCIE*/;
    DMA2_Stream3->FCR = DMA_SxFCR_FTH_Msk |
        DMA_SxFCR_DMDIS /*|
        DMA_SxFCR_FEIE */;
    SCB_CleanDCache_by_Addr((uint32_t *)ptr, 512);
    DMA2_Stream3->CR |= DMA_SxCR_EN;

    auto ret = sd_issue_command(24, resp_type::R1, block_addr * (is_hc ? 1 : 512));
    if(ret != 0)
    {
        xSemaphoreGive(sd_sem);
        return ret;
    }

    SDMMC1->DCTRL = SDMMC_DCTRL_DTEN |
        //SDMMC_DCTRL_DTDIR |
        (9UL << SDMMC_DCTRL_DBLOCKSIZE_Pos) |
        SDMMC_DCTRL_DMAEN;


    return 0;
}

int sd_write_blocks_async(uint32_t block_addr, uint32_t block_count, const void *ptr)
{
    // only one thread can use SD at a time
    while(!xSemaphoreTake(sd_sem, portMAX_DELAY));
    sd_task = xTaskGetCurrentTaskHandle();
    sd_status = 0;
    sd_multi = true;

    while(!sd_ready)
        sd_reset();

    SDMMC1->ICR = 0xffffffff;
    
    SDMMC1->DLEN = 512 * block_count;

    while(SDMMC1->STA & (SDMMC_STA_TXACT | SDMMC_STA_RXACT | SDMMC_STA_CMDACT));

    // set up DMA
    while(DMA2_Stream3->CR & DMA_SxCR_EN);

#ifdef DEBUG_SD
    ITM_SendChar('\n');
    ITM_SendChar('W');
#endif

    DMA2->LIFCR = DMA_LIFCR_CDMEIF3 |
        DMA_LIFCR_CTEIF3 |
        DMA_LIFCR_CHTIF3 |
        DMA_LIFCR_CTCIF3;
    DMA2_Stream3->CR = 0;
    DMA2_Stream3->PAR = (uint32_t)(uintptr_t)&SDMMC1->FIFO;
    DMA2_Stream3->M0AR = (uint32_t)(uintptr_t)ptr;
    //DMA2_Stream3->NDTR = 128 * block_count;
    DMA2_Stream3->CR = DMA_SxCR_CHSEL_2 |
        DMA_SxCR_MSIZE_1 |
        DMA_SxCR_PSIZE_1 |
        DMA_SxCR_MINC |
        DMA_SxCR_MBURST_0 |
        DMA_SxCR_PBURST_0 |
        DMA_SxCR_PFCTRL |
        DMA_SxCR_TEIE |
        DMA_SxCR_DIR_0 /*|
        DMA_SxCR_TCIE */;
    DMA2_Stream3->FCR = DMA_SxFCR_FTH_Msk |
        DMA_SxFCR_DMDIS /*|
        DMA_SxFCR_FEIE*/;
    
    SCB_CleanDCache_by_Addr((uint32_t *)ptr, 512 * block_count);
    DMA2_Stream3->CR |= DMA_SxCR_EN;

    auto ret = sd_issue_command(25, resp_type::R1, block_addr * (is_hc ? 1 : 512));
    if(ret != 0)
    {
        xSemaphoreGive(sd_sem);
        return ret;
    }    

    SDMMC1->DCTRL = SDMMC_DCTRL_DTEN |
        //SDMMC_DCTRL_DTDIR |
        (9UL << SDMMC_DCTRL_DBLOCKSIZE_Pos) |
        SDMMC_DCTRL_DMAEN;

    return 0;
}



int sd_read_block_poll(uint32_t block_addr, void *ptr)
{
    // only one thread can use SD at a time
    while(!xSemaphoreTake(sd_sem, portMAX_DELAY));
    sd_task = xTaskGetCurrentTaskHandle();
    sd_status = 0;
    sd_multi = false;

    SDMMC1->ICR = 0xffffffff;
    SDMMC1->DLEN = 512;
    SDMMC1->DCTRL = SDMMC_DCTRL_DTEN |
        SDMMC_DCTRL_DTDIR |
        (9UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);

    uint32_t ret = 0;
    
    sd_issue_command(17, resp_type::R1, block_addr * (is_hc ? 1 : 512));
    for(int i = 0; i < 512/4; i++)
    {
        while(!(SDMMC1->STA & SDMMC_STA_RXDAVL))
        {
            if(sd_status)
            {
                xSemaphoreGive(sd_sem);
                return sd_status;
            }
        }
        ((uint32_t *)ptr)[i] = SDMMC1->FIFO;
    }

    while(!xTaskNotifyWaitIndexed(2, 0, 0, &ret, portMAX_DELAY));
    SDMMC1->ICR = 0xffffffff;
    printf("sd_block_read %lx block_addr %lx ptr %lx status %lx\n", ret, block_addr,
        (uint32_t)(uintptr_t)ptr, SDMMC1->STA);
    xSemaphoreGive(sd_sem);

    return 0;
}

int sd_read_blocks(uint32_t block_addr, uint32_t block_count, void *ptr)
{
    if(((uint32_t)(uintptr_t)ptr) & 0x1f)
    {
        printf("sd unaligned buffer\n");
        while(true);
    }

    auto ret = sd_read_blocks_async(block_addr, block_count, ptr);
    if(ret != 0)
    {
        printf("sd_read_blocks_async failed %d\n", ret);
        return ret;
    }

    while(!xTaskNotifyWaitIndexed(2, 0, 0, (uint32_t *)&ret, portMAX_DELAY));
    SCB_InvalidateDCache_by_Addr((uint32_t *)ptr, 512 * block_count);
    SDMMC1->ICR = 0xffffffff;
    if(ret)
    {
        printf("sd_blocks_read %x block_addr %lx ptr %lx status %lx\n", ret, block_addr,
            (uint32_t)(uintptr_t)ptr, SDMMC1->STA);
    }
    else if(sd_ready)
        sd_issue_command(12, resp_type::R1b);   // stop
    xSemaphoreGive(sd_sem);

    return ret;
}

int sd_write_blocks(uint32_t block_addr, uint32_t block_count, const void *ptr)
{
    if(((uint32_t)(uintptr_t)ptr) & 0x1f)
    {
        printf("sd unaligned buffer\n");
        while(true);
    }

    auto ret = sd_write_blocks_async(block_addr, block_count, ptr);
    if(ret != 0)
    {
        printf("sd_write_blocks_async failed %d\n", ret);
        return ret;
    }

    while(!xTaskNotifyWaitIndexed(2, 0, 0, (uint32_t *)&ret, portMAX_DELAY));
    SDMMC1->ICR = 0xffffffff;
    if(ret)
    {
        printf("sd_blocks_write %x block_addr %lx ptr %lx status %lx\n", ret, block_addr,
            (uint32_t)(uintptr_t)ptr, SDMMC1->STA);
    }
    else if(sd_ready)
        sd_issue_command(12, resp_type::R1b);   // stop
    xSemaphoreGive(sd_sem);

    return ret;
}

int sd_read_block(uint32_t block_addr, void *ptr)
{
    //uint32_t unaligned_buf[128];
    auto obuf = ptr;

    //printf("sd_block_read start addr %lx status %lx\n", block_addr, SDMMC1->STA);

    if(((uint32_t)(uintptr_t)obuf) & 0x1f)
    {
        printf("sd unaligned buffer\n");
        while(true);
    }
        //ptr = unaligned_buf;

    auto ret = sd_read_block_async(block_addr, ptr);
    if(ret != 0)
    {
        printf("sd_read_block_async failed %d\n", ret);
        return ret;
    }

    while(!xTaskNotifyWaitIndexed(2, 0, 0, (uint32_t *)&ret, portMAX_DELAY));
    SCB_InvalidateDCache_by_Addr((uint32_t *)ptr, 512);
    SDMMC1->ICR = 0xffffffff;
    if(ret)
    {
        printf("sd_block_read %x block_addr %lx ptr %lx status %lx\n", ret, block_addr,
            (uint32_t)(uintptr_t)ptr, SDMMC1->STA);
    }
    xSemaphoreGive(sd_sem);

    /*if(ret == 0 && ((uint32_t)(uintptr_t)obuf) & 0x3)
    {
        uint8_t *d = (uint8_t *)obuf;
        const uint8_t *s = (const uint8_t *)unaligned_buf;
        for(int i = 0; i < 512; i++)
            d[i] = s[i];
    }*/
    return ret;
}

int sd_write_block(uint32_t block_addr, const void *ptr)
{
    if(((uint32_t)(uintptr_t)ptr) & 0x1f)
    {
        printf("sd unaligned buffer\n");
        while(true);
    }
    //uint32_t unaligned_buf[128];
    auto obuf = ptr;

    //printf("sd_block_read start addr %lx status %lx\n", block_addr, SDMMC1->STA);

    if(((uint32_t)(uintptr_t)obuf) & 0x3)
    {
        printf("sd unaligned buffer\n");
        while(true);
    }
        //ptr = unaligned_buf;

    auto ret = sd_write_block_async(block_addr, ptr);
    if(ret != 0)
    {
        printf("sd_write_block_async failed %d\n", ret);
        return ret;
    }

    while(!xTaskNotifyWaitIndexed(2, 0, 0, (uint32_t *)&ret, portMAX_DELAY));
    SDMMC1->ICR = 0xffffffff;
    if(ret)
    {
        printf("sd_block_write %x block_addr %lx ptr %lx status %lx\n", ret, block_addr,
            (uint32_t)(uintptr_t)ptr, SDMMC1->STA);
    }

    xSemaphoreGive(sd_sem);

    /*if(ret == 0 && ((uint32_t)(uintptr_t)obuf) & 0x3)
    {
        uint8_t *d = (uint8_t *)obuf;
        const uint8_t *s = (const uint8_t *)unaligned_buf;
        for(int i = 0; i < 512; i++)
            d[i] = s[i];
    }*/
    return ret;
}

extern "C" void DMA2_Stream3_IRQHandler() __attribute__((section(".itcm")));
extern "C" void DMA2_Stream3_IRQHandler()
{
#ifdef DEBUG_SD
    ITM_SendChar('D');
#endif
    //printf("DMA IRQ: %lx\n", DMA2->LISR);
    BaseType_t hpt1 = pdFALSE, hpt2 = pdFALSE;
    auto lisr = DMA2->LISR;
    if(lisr & DMA_LISR_TCIF3)
    {
#ifdef DEBUG_SD
        ITM_SendChar('C');
#endif
        // tfer complete
        sd_status = 0;
        xTaskNotifyIndexedFromISR(sd_task, 2, sd_status, eSetValueWithOverwrite, &hpt1);
    }
    else
    {
#ifdef DEBUG_SD
        ITM_SendChar('E');
#endif
        // error
        sd_status |= (lisr & 0x0f400000UL) | SDMMC1->STA |
            (((DMA2_Stream3->FCR >> 3) & 0x7) << 28);
        sd_ready = false;
        DMA2_Stream3->CR = 0;
        xTaskNotifyIndexedFromISR(sd_task, 2, sd_status, eSetValueWithOverwrite, &hpt2);
    }
    DMA2->LIFCR = DMA_LIFCR_CTCIF3 |
        DMA_LIFCR_CTEIF3 |
        DMA_LIFCR_CFEIF3;
    //SDMMC1->ICR = DATAEND | DBCKEND;

    if(hpt2) hpt1 = pdTRUE;
    portYIELD_FROM_ISR(hpt1);
}

extern "C" void SDMMC1_IRQHandler() __attribute__((section(".itcm")));
extern "C" void SDMMC1_IRQHandler()
{
#ifdef DEBUG_SD
    ITM_SendChar('S');
#endif
    //printf("SDIO IRQ: %lx\n", SDMMC1->STA);
    BaseType_t hpt1 = pdFALSE;
    auto const errors = DCRCFAIL |
        DTIMEOUT |
        TXUNDERR | RXOVERR;
    auto sta = SDMMC1->STA;
    if(sta & errors)
    {
#ifdef DEBUG_SD
        ITM_SendChar('e');
#endif
        sd_status = SDMMC1->STA;
        sd_ready = false;
        DMA2_Stream3->CR = 0;
        xTaskNotifyIndexedFromISR(sd_task, 2, sd_status, eSetValueWithOverwrite, &hpt1);
    }
    else if(sta & DATAEND)
    {
#ifdef DEBUG_SD
        ITM_SendChar('c');
#endif
        sd_status = 0;
        xTaskNotifyIndexedFromISR(sd_task, 2, sd_status, eSetValueWithOverwrite, &hpt1);
    }
    else
    {
#ifdef DEBUG_SD
        ITM_SendChar('u');
#endif
    }

    SDMMC1->ICR = sta & (errors | DATAEND) & 0x4005ff;

    portYIELD_FROM_ISR(hpt1);
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
