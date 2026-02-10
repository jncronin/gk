#include "stm32mp2xx.h"
#include "clocks.h"
#include "sd.h"
#include <cstdio>
#include "pins.h"
#include "osqueue.h"
#include "osmutex.h"
#include "ostypes.h"
#include "scheduler.h"
#include "cache.h"
//#include "osnet.h"
//#include "ext4_thread.h"
#include "gk_conf.h"
#include "vmem.h"
#include "pmic.h"
#include "gic.h"
#include "sdif.h"

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

int sd_perform_transfer_async(const sd_request &req);
int sd_perform_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read, int nretries = 10);

static const constexpr unsigned int unaligned_buf_size = 512;
__attribute__((aligned(32))) char unaligned_buf[unaligned_buf_size];

static volatile bool tfer_inprogress = false;
static volatile bool dma_ready = false;
static volatile bool sd_multi = false;

extern char _ssdt_data, _esdt_data;

static constexpr pin sd_pins[] =
{
    { GPIOE_VMEM, 0, 10 },
    { GPIOE_VMEM, 1, 10 },
    { GPIOE_VMEM, 2, 10 },
    { GPIOE_VMEM, 3, 10 },
    { GPIOE_VMEM, 4, 10 },
    { GPIOE_VMEM, 5, 10 }
};

FixedQueue<sd_request, 32> sdt_queue;
SimpleSignal sdt_ready;

enum class resp_type { None, R1, R1b, R2, R3, R4, R4b, R5, R6, R7 };
enum class data_dir { None, ReadBlock, WriteBlock, ReadStream, WriteStream };

enum StatusFlags { CCRCFAIL = 1, DCRCFAIL = 2, CTIMEOUT = 4, DTIMEOUT = 8,
    TXUNDERR = 0x10, RXOVERR = 0x20, CMDREND = 0x40, CMDSENT = 0x80,
    DATAEND = 0x100, /* reserved */ DBCKEND = 0x400, DABORT = 0x800,
    DPSMACT = 0x1000, CPSMACT = 0x2000, TXFIFOHE = 0x4000, RXFIFOHF = 0x8000,
    TXFIFOF = 0x10000, RXFIFOF = 0x20000, TXFIFOE = 0x40000, RXFIFOE = 0x80000 };

static void *sd_thread(void *param);

static inline void delay_ms(unsigned int v)
{
    Block(clock_cur() + kernel_time_from_ms(v));
}

static inline bool is_valid_dma(void *mem_address, uint32_t block_count)
{
    /* SDMMC1_VMEM cannot access memory > 32-bits */
    uint64_t mem_start = (uint64_t)(uintptr_t)mem_address;
    if(mem_start >= 0x100000000ULL)
    {
        return false;
    }
    if(mem_start & 0x3U)
    {
        return false;
    }
    return true;
}

int sd_cache_init();
void init_sd()
{
    sdmmc[0] = SDIF();
    sdmmc[1] = SDIF();

    if(sd_cache_init())
    {
        return;
    }
    Schedule(Thread::Create("sd", sd_thread, nullptr, true, GK_PRIORITY_VHIGH, p_kernel));
}

static inline uint32_t do_mdma_transfer(uint32_t src, uint32_t dest)
{
    // TODO: use HPDMA here similar to MDMA in previous version
    memcpy((void *)dest, (const void *)src, 512);
    return 0;
}

#if 0
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
#endif

// sd thread function
void *sd_thread(void *param)
{
    (void)param;
    bool sd_shutdown = false;

    //RCC->AHB3ENR |= RCC_AHB3ENR_MDMAEN;
    //(void)RCC->AHB3ENR;

    init_sdmmc1();

    while(true)
    {
        if(!sdmmc[0].sd_ready && !sd_shutdown)
        {
            sdmmc[0].reset();
            if(!sdmmc[0].sd_ready)
                Block(clock_cur() + kernel_time_from_ms(1000));
            continue;
        }

        sd_request sdr;
        if(sdt_queue.Pop(&sdr))
        {
            if(sd_shutdown || sdr.block_count == 0 || !sdr.completion_event || !sdr.mem_address)
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

            if(sdr.block_count == 0xffffffff && sdr.block_start == 0xffffffff &&
                sdr.mem_address == (void *)0xffffffff)
            {
                // unmount
                sdmmc[0].sd_issue_command(0, SDIF::resp_type::None);
                delay_ms(1);
                SDMMC1_VMEM->POWER = 0;
                delay_ms(1);

                sd_shutdown = true;
                pmic_set_power(PMIC_Power_Target::SDCard, 0);
                pmic_set_power(PMIC_Power_Target::SDCard_IO, 0);
                PWR_VMEM->CR8 &= ~PWR_CR8_VDDIO1SV;

                if(sdr.res_out)
                {
                    *sdr.res_out = 0;
                }
                if(sdr.completion_event)
                {
                    sdr.completion_event->Signal();
                }

                continue;
            }

#if PROFILE_SDT
            {
                
                klog("sdt: %s block %d, block_len %d\n",
                    sdr.is_read ? "read " : "write", sdr.block_start, sdr.block_count);
            }
#endif

            bool is_valid = is_valid_dma(sdr.mem_address, sdr.block_count);
            uint32_t mem_len = sdr.block_count * 512U;
            uint32_t mem_start = is_valid ? (uint32_t)(uintptr_t)sdr.mem_address : (uint32_t)(uintptr_t)unaligned_buf;

            if(!is_valid && !sdr.is_read)
            {
                // need to copy memory to the sram buffer
                do_mdma_transfer((uint32_t)(uintptr_t)sdr.mem_address, (uint32_t)(uintptr_t)unaligned_buf);
                CleanA35Cache((uint32_t)(uintptr_t)unaligned_buf, unaligned_buf_size, CacheType_t::Data);
            }

            SDMMC1_VMEM->DCTRL = 0;
            SDMMC1_VMEM->DLEN = mem_len;
            SDMMC1_VMEM->DCTRL = 
                (sdr.is_read ? SDMMC_DCTRL_DTDIR : 0UL) |
                (9UL << SDMMC_DCTRL_DBLOCKSIZE_Pos);
            SDMMC1_VMEM->CMD = 0;
            SDMMC1_VMEM->CMD = SDMMC_CMD_CMDTRANS;
            SDMMC1_VMEM->IDMABASER = mem_start;
            SDMMC1_VMEM->IDMACTRL = SDMMC_IDMA_IDMAEN;

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
                
                klog("sd: pre-command: %d, STA: %x, DCTRL: %x, DCOUNT: %x, DLEN: %x, IDMACTRL: %x, IDMABASE: %x\n", cmd_id,
                    SDMMC1_VMEM->STA, SDMMC1_VMEM->DCTRL, SDMMC1_VMEM->DCOUNT, SDMMC1_VMEM->DLEN, SDMMC1_VMEM->IDMACTRL, SDMMC1_VMEM->IDMABASER);
            }
#endif

            auto ret = sdmmc[0].sd_issue_command(cmd_id, SDIF::resp_type::R1, sdr.block_start * (sdmmc[0].is_hc ? 1U : 512U), nullptr, true);

            if(ret != 0)
            {
                sdmmc[0].sd_ready = false;
                if(sdr.res_out)
                {
                    *sdr.res_out = ret;
                }
            }
            sdt_ready.Wait(SimpleSignal::Set, 0U);
            if(sdr.res_out)
            {
                *sdr.res_out = sdmmc[0].sd_status;
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
                if(!sdr.is_read)
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
                        if(resp != 0xd00)
                        {
                            
                            klog("sd: not in rcv state: %x\n", resp);
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                // send stop command
                [[maybe_unused]] int stop_ret = sdmmc[0].sd_issue_command(12, SDIF::resp_type::R1b);
#if DEBUG_SD
                {
                    
                    klog("sd: post-stop command: ret: %x, STA: %x\n",
                        stop_ret, SDMMC1_VMEM->STA);
                }
#endif
                sd_multi = false;
            }

            if(!is_valid && sdr.is_read)
            {
                // need to copy memory from the sram buffer
                InvalidateA35Cache((uint32_t)(uintptr_t)unaligned_buf, unaligned_buf_size,
                    CacheType_t::Data);
                do_mdma_transfer((uint32_t)(uintptr_t)unaligned_buf, (uint32_t)(uintptr_t)sdr.mem_address);

                // because in the calling function we invalidate sdr.mem_address, first clean to it here
                //  TODO: make this more efficient with only a single invalidate call i.e. that above
                //   or just use HPDMA in do_mdma_transfer
                mem_start = (uint32_t)(uintptr_t)sdr.mem_address;
                auto cache_start = mem_start & ~0x1fU;
                auto cache_end = (mem_start + unaligned_buf_size + 0x1fU) & ~0x1fU;

                CleanA35Cache(cache_start, cache_end - cache_start, CacheType_t::Data);
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
    auto mem_start = (uintptr_t)mem_address;
    auto mem_end = mem_start + block_count * 512U;
    auto cache_start = mem_start & ~0x1fU;
    auto cache_end = (mem_end + 0x1fU) & ~0x1fU;

    if(!is_read)
    {
        // writes need to commit all cache contents to ram first
        //CleanA35Cache(cache_start, cache_end - cache_start, CacheType_t::Data);
    }
    else
    {
        if(cache_start != mem_start)
        {
            // commit first cache line for unaligned reads
            CleanA35Cache(cache_start, 32, CacheType_t::Data);
        }
        if(cache_end != mem_end)
        {
            // commit last cache line for unaligned reads
            CleanA35Cache(cache_end - 32, 32, CacheType_t::Data);
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
        //InvalidateA35Cache(cache_start, cache_end - cache_start, CacheType_t::Data);
    }

    int cret = *ret;

    if(cret != 0)
    {
        klog("sd_perform_transfer %s of %d blocks at %x failed: %x, DCOUNT: %x, DCTRL: %x, IDMACTRL: %x, IDMABASE: %x, IDMASIZE: %x, retrying\n",
            is_read ? "read" : "write", block_count, (uint32_t)(uintptr_t)mem_address, cret,
            sdmmc[0].sd_dcount, sdmmc[0].sd_dctrl, sdmmc[0].sd_idmactrl, sdmmc[0].sd_idmabase,
            sdmmc[0].sd_idmasize);
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
            ret = sd_perform_transfer_int(block_start + nblocks_sent, 1,
                (void *)(((char *)mem_address) + (512U * nblocks_sent)), is_read);
            
            if(ret == 0)
            {
                failed = false;
                break;
            }
        }

        if(failed)
        {
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
        klog("sd_perform_transfer %s of %d blocks at %x failed: %x\n",
            is_read ? "read" : "write", block_count, (uint32_t)(uintptr_t)mem_address, ret);
    }

    return ret;
}

void SDMMC1_IRQHandler()
{
    auto const errors = DCRCFAIL |
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
        sdt_ready.Signal();
    }
    else if(sta & DATAEND)
    {
        sdmmc[0].sd_status = 0;
        sdt_ready.Signal();
    }
    else
    {
    }

    SDMMC1_VMEM->ICR = sta & (errors | DATAEND) & 0x4005ff;
}

bool sd_get_ready()
{
    //printf("SD status %lx\n", sd_status);
    return sdmmc[0].sd_ready;
}

uint64_t sd_get_size()
{
    return sdmmc[0].get_size();
}
