#include "stm32mp2xx.h"
#include "clocks.h"
#include "sd.h"
#include <cstdio>
#include "pins.h"
#include "osqueue.h"
#include "osmutex.h"
#include "ostypes.h"
#include "scheduler.h"
//#include "osnet.h"
//#include "ext4_thread.h"
#include "gk_conf.h"
#include "vmem.h"
#include "pmic.h"
#include "gic.h"
#include "sdif.h"
#include <cassert>

#define SDMMC1_VMEM ((SDMMC_TypeDef *)PMEM_TO_VMEM(SDMMC1_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define GPIOE_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOE_BASE))
#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))
#define RIFSC_VMEM (PMEM_TO_VMEM(RIFSC_BASE))

#define DEBUG_SD    0
#define PROFILE_SDT 0

extern PProcess p_kernel;

#define SDCLK 200000000

#define SDCLK_IDENT     200000
#define SDCLK_DS        25000000
#define SDCLK_HS        50000000

int sd_perform_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read, int nretries = 10);

static constexpr pin sd_pins[] =
{
    { GPIOE_VMEM, 0, 10 },
    { GPIOE_VMEM, 1, 10 },
    { GPIOE_VMEM, 2, 10 },
    { GPIOE_VMEM, 3, 10 },
    { GPIOE_VMEM, 4, 10 },
    { GPIOE_VMEM, 5, 10 }
};

enum class resp_type { None, R1, R1b, R2, R3, R4, R4b, R5, R6, R7 };
enum class data_dir { None, ReadBlock, WriteBlock, ReadStream, WriteStream };

enum StatusFlags { CCRCFAIL = 1, DCRCFAIL = 2, CTIMEOUT = 4, DTIMEOUT = 8,
    TXUNDERR = 0x10, RXOVERR = 0x20, CMDREND = 0x40, CMDSENT = 0x80,
    DATAEND = 0x100, /* reserved */ DBCKEND = 0x400, DABORT = 0x800,
    DPSMACT = 0x1000, CPSMACT = 0x2000, TXFIFOHE = 0x4000, RXFIFOHF = 0x8000,
    TXFIFOF = 0x10000, RXFIFOF = 0x20000, TXFIFOE = 0x40000, RXFIFOE = 0x80000 };

static inline void delay_ms(unsigned int v)
{
    Block(clock_cur() + kernel_time_from_ms(v));
}

int sd_cache_init();
void init_sd()
{
    sdmmc[0] = SDIF();
    sdmmc[1] = SDIF();

    init_sdmmc1();

    if(sd_cache_init())
    {
        return;
    }
}

int sd_unmount()
{
    MutexGuard mg(sdmmc[0].m);
    sdmmc[0].sd_issue_command(0, SDIF::resp_type::None);
    delay_ms(1);
    SDMMC1_VMEM->POWER = 0;
    delay_ms(1);

    sdmmc[0].sd_ready = false;
    pmic_set_power(PMIC_Power_Target::SDCard, 0);
    pmic_set_power(PMIC_Power_Target::SDCard_IO, 0);
    PWR_VMEM->CR8 &= ~PWR_CR8_VDDIO1SV;

    return 0;
}

static int sd_perform_transfer_int(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read)
{
    if(!sdmmc[0].sd_ready)
    {
        if(sdmmc[0].reset() != 0)
        {
            return -1;
        }
    }

    bool sd_multi = false;
    uint32_t mem_len = block_count * 512U;
    uint32_t mem_start = (uint32_t)(uintptr_t)mem_address;

    SDMMC1_VMEM->DCTRL = 0;
    SDMMC1_VMEM->DLEN = mem_len;
    SDMMC1_VMEM->DCTRL = 
        (is_read ? SDMMC_DCTRL_DTDIR : 0UL) |
        (9UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);
    SDMMC1_VMEM->CMD = 0;
    SDMMC1_VMEM->CMD = SDMMC_CMD_CMDTRANS;
    SDMMC1_VMEM->IDMABASER = mem_start;
    SDMMC1_VMEM->IDMACTRL = SDMMC_IDMA_IDMAEN;

    int cmd_id = 0;
    if(block_count == 1)
    {
        sd_multi = false;
        if(is_read)
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
        if(is_read)
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
        
        klog("sd: pre-command: %d, STA: %x, DCTRL: %x, DCOUNT: %x, DLEN: %x, IDMACTRL: %x, IDMABASE: %x\n", cmd_id,
            SDMMC1_VMEM->STA, SDMMC1_VMEM->DCTRL, SDMMC1_VMEM->DCOUNT, SDMMC1_VMEM->DLEN, SDMMC1_VMEM->IDMACTRL, SDMMC1_VMEM->IDMABASER);
    }
#endif

    auto ret = sdmmc[0].sd_issue_command(cmd_id, SDIF::resp_type::R1, block_start * (sdmmc[0].is_hc ? 1U : 512U), nullptr, true);

    if(ret != 0)
    {
        sdmmc[0].sd_ready = false;
        return ret;
    }
    if(!sdmmc[0].tfer_complete.Wait(clock_cur() + kernel_time_from_ms(125)))
    {
        klog("sdmmc: TIMEOUT block %u, nblocks %u, is_read: %s, addr: %p\n",
            block_start, block_count, is_read ? "true" : "false", mem_address);
        sdmmc[0].sd_ready = false;
        return -1;
    }
    if(sdmmc[0].sd_status)
    {
#if DEBUG_SD
        {
            
            klog("sd: post-command: %d: FAIL: STA: %x (%x), DCTRL: %x, DCOUNT: %x, DLEN: %x, IDMACTRL: %x, IDMABASE: %x\n", cmd_id,
                SDMMC1_VMEM->STA, sdmmc[0].sd_status, SDMMC1_VMEM->DCTRL, SDMMC1_VMEM->DCOUNT, SDMMC1_VMEM->DLEN, SDMMC1_VMEM->IDMACTRL, SDMMC1_VMEM->IDMABASER);
        }
#endif
        sdmmc[0].sd_ready = false;
    }
#if DEBUG_SD
    else
    {
        
        klog("sd: post-command: %d: SUCCESS: STA: %x (%x), DCTRL: %x, DCOUNT: %x, DLEN: %x, IDMACTRL: %x, IDMABASE: %x\n", cmd_id,
            SDMMC1_VMEM->STA, sdmmc[0].sd_status, SDMMC1_VMEM->DCTRL, SDMMC1_VMEM->DCOUNT, SDMMC1_VMEM->DLEN, SDMMC1_VMEM->IDMACTRL, SDMMC1_VMEM->IDMABASER);
    }
#endif
    if(sd_multi)
    {
        // send stop command
        [[maybe_unused]] int stop_ret = sdmmc[0].sd_issue_command(12, SDIF::resp_type::R1b);
#if DEBUG_SD
        {
            
            klog("sd: post-stop command: ret: %x, STA: %x\n",
                stop_ret, SDMMC1_VMEM->STA);
        }
#endif

        if(!is_read)
        {
            // poll status register until write done
            while(true)
            {
                uint32_t resp;
                int sr_ret = sdmmc[0].sd_issue_command(13, SDIF::resp_type::R1, sdmmc[0].rca, &resp);
                if(sr_ret != 0)
                {
                    sdmmc[0].sd_ready = false;
                    break;
                }
                if(resp != 0x900)
                {
                    //klog("sd: not in tran state: %x\n", resp);
                }
                else
                {
                    break;
                }
            }
        }

        sd_multi = false;
    }

    return 0;
}

int sd_perform_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read, int nretries)
{
    assert(block_count == 128U);
    assert(mem_address);
    assert(((uintptr_t)mem_address & 0xffffU) == 0);
    
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
        klog("sd_perform_transfer %s of %d blocks at %x failed: %x\n",
            is_read ? "read" : "write", block_count, (uint32_t)(uintptr_t)mem_address, ret);
    }

    return ret;
}

void SDMMC1_IRQHandler()
{
    auto const errors = DCRCFAIL |
        CCRCFAIL |
        DABORT |
        DTIMEOUT |
        TXUNDERR | RXOVERR;
    auto sta = SDMMC1_VMEM->STA;
#if DEBUG_SD
    klog("sd: int: %lx\n", sta);
#endif
    if(sta & errors)
    {
        SDMMC1_VMEM->DCTRL |= SDMMC_DCTRL_FIFORST;
        
        sdmmc[0].sd_status = sta;
        sdmmc[0].sd_dcount = SDMMC1_VMEM->DCOUNT;
        sdmmc[0].sd_idmabase = SDMMC1_VMEM->IDMABASER;
        sdmmc[0].sd_idmactrl = SDMMC1_VMEM->IDMACTRL;
        sdmmc[0].sd_idmasize = SDMMC1_VMEM->IDMABSIZE;
        sdmmc[0].sd_dctrl = SDMMC1_VMEM->DCTRL;
        sdmmc[0].sd_ready = false;
        sdmmc[0].tfer_complete.Signal();
    }
    else if(sta & DATAEND)
    {
        sdmmc[0].sd_status = 0;
        sdmmc[0].tfer_complete.Signal();
    }
    else
    {
        klog("sdmmc: unexpected interrupt: %x\n", sta);
        sdmmc[0].sd_ready = false;
        sdmmc[0].sd_status = sta;
        sdmmc[0].tfer_complete.Signal();
    }

    SDMMC1_VMEM->ICR = sta & (errors | DATAEND) & 0x4005ff;
}

bool sd_get_ready()
{
    //printf("SD status %lx\n", sd_status);
    MutexGuard mg(sdmmc[0].m);
    return sdmmc[0].sd_ready;
}

uint64_t sd_get_size()
{
    MutexGuard mg(sdmmc[0].m);
    if(!sdmmc[0].sd_ready)
    {
        sdmmc[0].reset();
        if(!sdmmc[0].sd_ready)
            return 0;
    }
    return sdmmc[0].get_size();
}
