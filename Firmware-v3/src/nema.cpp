#include <nema_core.h>
#include <stm32h7rsxx.h>
#include "logger.h"
#include "screen.h"
#include "memblk.h"
#include "osmutex.h"
#include "cache.h"
#include "gk_conf.h"
#include "scheduler.h"
#include "thread.h"
#include "process.h"
#include "syscalls_int.h"

#define GPU2D_ITCTRL                    (0x0F8U)   /*!< GPU2D Interrupt Control Register Offset            */
#define GPU2D_CLID                      (0x148U)   /*!< GPU2D Last Command List Identifier Register Offset */
#define GPU2D_BREAKPOINT                (0x080U)   /*!< GPU2D Breakpoint Register Offset                   */
#define GPU2D_SYS_INTERRUPT             (0xff8U)   /*!< GPU2D System Interrupt Register Offset             */

#define GPU2D_FLAG_CLC                   0x00000001U              /*!< Command List Complete Interrupt Flag  */

Mutex m_ehold;
static UserspaceSemaphore sem_nema_irq;

static nema_ringbuffer_t ring_buffer_str = {
    .bo = 
    {
        .size = 0x20000,
        .fd = 0,
        .base_virt = (void *)0x901d0000,
        .base_phys = 0x901d0000
    },
    .offset = 0,
    .last_submission_id = 0
};

static Mutex m_nema[MUTEX_MAX + 1] = { 0 };

void init_nema()
{
    RCC->AHB5ENR |= RCC_AHB5ENR_GPU2DEN;
    (void)RCC->AHB5ENR;

    RCC->AHB5RSTR = RCC_AHB5RSTR_GPU2DRST;
    (void)RCC->AHB5RSTR;

    RCC->AHB5RSTR = 0;
    (void)RCC->AHB5RSTR;

    NVIC_EnableIRQ(GPU2D_IRQn);
    NVIC_EnableIRQ(GPU2D_ER_IRQn);
}

int syscall_nemaenable(pthread_mutex_t *nema_mutexes, size_t nmutexes,
    void *nema_rb, sem_t *nema_irq_sem, pthread_mutex_t *eof_mutex, int *_errno)
{
    if(nmutexes != MUTEX_MAX + 1)
    {
        *_errno = EINVAL;
        return -1;
    }
    if(!nema_mutexes || !nema_rb || !nema_irq_sem || !eof_mutex)
    {
        *_errno = EINVAL;
        return -1;
    }

    /* First try and allocate MPU space to access the NEMA registers */
    auto t = GetCurrentThreadForCore();
    CriticalGuard cg(t->sl);

    auto &p = t->p;
    CriticalGuard cg2(p.sl);

    auto mpu_nema = MPUGenerate(GPU2D_BASE, 0x1000, 0, false, RW, RW, DEV_S);

    int reg_id = p.AddMPURegion(mpu_nema);
    if(reg_id == -1)
    {
        *_errno = ENOMEM;
        return -1;
    }

    p.UpdateMPURegionsForThreads();

    /* Return mutex/sem ids etc */
    for(unsigned int i = 0; i < MUTEX_MAX + 1; i++)
    {
        auto mutex = &nema_mutexes[i];
        *reinterpret_cast<Mutex **>(mutex) = &m_nema[i];
    }
    nema_irq_sem->s = &sem_nema_irq;
    memcpy(nema_rb, &ring_buffer_str, sizeof(nema_ringbuffer_t));

    return 0;
}

uint32_t nema_reg_read(uint32_t reg)
{
    auto ret = *(volatile uint32_t *)(GPU2D_BASE + reg);
    __asm__ volatile("dsb \n" ::: "memory");
    return ret;
}

void nema_reg_write(uint32_t reg, uint32_t value)
{
    __asm__ volatile("dmb \n" ::: "memory");
    *(volatile uint32_t *)(GPU2D_BASE + reg) = value;
}

extern "C" void GPU2D_IRQHandler()
{
    auto isr_flags = nema_reg_read(GPU2D_ITCTRL);

    if(isr_flags & GPU2D_FLAG_CLC)
    {
        /* Clear completion flag */
        nema_reg_write(GPU2D_ITCTRL, isr_flags & ~GPU2D_FLAG_CLC);

        sem_nema_irq.post();
    }
    else
    {
        BKPT();
    }
    __DMB();
}

extern "C" void GPU2D_ER_IRQHandler()
{
    auto isr = nema_reg_read(GPU2D_SYS_INTERRUPT);
    nema_ext_hold_deassert_imm(0);
    nema_reg_write(GPU2D_SYS_INTERRUPT, isr);
    m_ehold.unlock(nullptr, true);
    __DMB();
}
