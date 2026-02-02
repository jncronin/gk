#include "klog_buffer.h"
#include "retram.h"
#include "vmem.h"
#include "osmutex.h"
#include "clocks.h"
#include "scheduler.h"
#include "thread.h"
#include "process.h"
#include <stm32mp2xx.h>

#define KLOGMAGIC 0x474f4c4b534f4b47

#define USART6_VMEM ((USART_TypeDef *)PMEM_TO_VMEM(USART6_BASE))

static uint8_t *klog_buf_uart = (uint8_t *)PMEM_TO_VMEM(0x20090000);
static uint8_t *klog_buf_file = (uint8_t *)PMEM_TO_VMEM(0x20098000);

static Condition klog_updated{};

void init_klogbuffer()
{
    if(retram->klog.magic == KLOGMAGIC)
    {
        // already setup.  dump anything which may be there
        klogbuffer_purge_uart();
    }
    else
    {
        retram->klog.b_file.init(klog_buf_file, 0x8000U);
        retram->klog.b_uart.init(klog_buf_uart, 0x8000U);
        retram->klog.magic = KLOGMAGIC;
    }
}

static void uart_log(char c)
{
    if(c == '\n')
    {
        while((USART6_VMEM->ISR & USART_ISR_TXFNF_Msk) == 0);
        USART6_VMEM->TDR = '\r';
        while((USART6_VMEM->ISR & USART_ISR_TXFNF_Msk) == 0);
        USART6_VMEM->TDR = '\n';
    }
    else
    {
        while((USART6_VMEM->ISR & USART_ISR_TXFNF_Msk) == 0);
        USART6_VMEM->TDR = c;
    }
}

static void uart_log(const char *buf, size_t count)
{
    while(count--)
    {
        uart_log(*buf++);
    }
}

int klogbuffer_purge_uart()
{
    while(true)
    {
        char rbuf[256];
        auto bret = retram->klog.b_uart.read(rbuf, sizeof(rbuf));
        if(bret > 0)
        {
            uart_log(rbuf, (size_t)bret);
        }
        if(bret < 0 || (size_t)bret != sizeof(rbuf))
        {
            return 0;
        }
    }
}

ssize_t log_fwrite(const void *buf, size_t count)
{
    auto fret = retram->klog.b_file.write(buf, count);
    auto uret = retram->klog.b_uart.write(buf, count);

    klog_updated.Signal();

    return std::max(fret, uret);
}

static void *klogbuffer_thread(void *)
{
    while(true)
    {
        klog_updated.Wait(clock_cur() + kernel_time_from_ms(500));
        klogbuffer_purge_uart();
    }
}

void init_klogbuffer_thread()
{
    sched.Schedule(Thread::Create("klogbuffer", klogbuffer_thread, nullptr, true,
        GK_PRIORITY_IDLE + 1, p_kernel));
}
