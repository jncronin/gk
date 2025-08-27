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

/* nema profiling:
    userspace glgears, vsync off:
        Mesa software 31 fps
*/

#define GPU2D_ITCTRL                    (0x0F8U)   /*!< GPU2D Interrupt Control Register Offset            */
#define GPU2D_CLID                      (0x148U)   /*!< GPU2D Last Command List Identifier Register Offset */
#define GPU2D_BREAKPOINT                (0x080U)   /*!< GPU2D Breakpoint Register Offset                   */
#define GPU2D_SYS_INTERRUPT             (0xff8U)   /*!< GPU2D System Interrupt Register Offset             */

#define GPU2D_FLAG_CLC                   0x00000001U              /*!< Command List Complete Interrupt Flag  */

Mutex m_ehold(false, true);
static UserspaceSemaphore sem_nema_irq;

const uint32_t nema_mr_size = 0x8000;
const uint32_t nema_mr_align = 1024;
static MemRegion mr_nema;

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

static nema_buffer_t cl_a, cl_b;

static Mutex m_nema[MUTEX_MAX + 1] = { 0 };

bool nema_is_mutex(const Mutex *m)
{
    for(int i = 0; i < MUTEX_MAX + 1; i++)
    {
        if(m == &m_nema[i])
            return true;
    }
    return false;
}

bool nema_is_sem(const UserspaceSemaphore *sem)
{
    if(sem == &sem_nema_irq)
        return true;
    return false;
}

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

    ICACHE->CR |= ICACHE_CR_EN;

    mr_nema = memblk_allocate(nema_mr_size, MemRegionType::AXISRAM, "nema buffers");
    if(!mr_nema.valid)
        mr_nema = memblk_allocate(nema_mr_size, MemRegionType::SRAM, "nema buffers");
    if(!mr_nema.valid)
        mr_nema = memblk_allocate(nema_mr_size, MemRegionType::SDRAM, "nema buffers");
    
    if(mr_nema.valid)
    {
        klog("nema: buffers 0x%x - 0x%x\n", mr_nema.address, mr_nema.address + mr_nema.length);

        ring_buffer_str.bo.base_phys = mr_nema.address;
        ring_buffer_str.bo.base_virt = (void*)(uintptr_t)mr_nema.address;
        ring_buffer_str.bo.size = nema_mr_align;
        ring_buffer_str.bo.fd = 2;      // don't deallocate

        cl_a.base_phys = ring_buffer_str.bo.base_phys + ring_buffer_str.bo.size;
        cl_a.base_phys = (cl_a.base_phys + nema_mr_align - 1) & ~(nema_mr_align - 1);
        cl_a.base_virt = (void *)cl_a.base_phys;
        cl_a.size = ((mr_nema.length - (cl_a.base_phys - mr_nema.address)) / 2) & ~(nema_mr_align - 1);
        cl_a.fd = 2;

        cl_b.base_phys = cl_a.base_phys + cl_a.size;
        cl_b.base_phys = (cl_b.base_phys + nema_mr_align - 1) & ~(nema_mr_align - 1);
        cl_b.base_virt = (void *)cl_b.base_phys;
        cl_b.size = (mr_nema.length - (cl_b.base_phys - mr_nema.address)) & ~(nema_mr_align - 1);
        cl_b.fd = 2;

        klog("nema: rb: %08x - %08x, cl_a: %08x - %08x, cl_b: %08x - %08x\n",
            ring_buffer_str.bo.base_phys, ring_buffer_str.bo.base_phys + ring_buffer_str.bo.size,
            cl_a.base_phys, cl_a.base_phys + cl_a.size,
            cl_b.base_phys, cl_b.base_phys + cl_b.size);
    }
}

int syscall_nemaenable(pthread_mutex_t *nema_mutexes, size_t nmutexes,
    void *nema_rb, sem_t *nema_irq_sem, pthread_mutex_t *eof_mutex, 
    void *_cl_a, void *_cl_b, int *_errno)
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

    // make ring/circ buffers WT
    auto mpu_nema_rb = MPUGenerate(mr_nema.address, mr_nema.length, 0, false,
        RW, RW, WT_NS);
    int rb_reg_id = p.AddMPURegion(mpu_nema_rb);
    if(rb_reg_id == -1)
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
    if(addr_is_valid(reinterpret_cast<nema_buffer_t *>(_cl_a), true))
        memcpy(_cl_a, &cl_a, sizeof(cl_a));
    if(addr_is_valid(reinterpret_cast<nema_buffer_t *>(_cl_b), true))
        memcpy(_cl_b, &cl_b, sizeof(cl_b));

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

int syscall_icacheinvalidate(int *_errno)
{
    ICACHE->CR |= ICACHE_CR_CACHEINV;
    return 0;
}

extern "C" void GPU2D_IRQHandler()
{
    auto isr_flags = nema_reg_read(GPU2D_ITCTRL);

    if(isr_flags & GPU2D_FLAG_CLC)
    {
        sem_nema_irq.post();
        /* Clear completion flag */
        nema_reg_write(GPU2D_ITCTRL, isr_flags & ~GPU2D_FLAG_CLC);

    }
    else
    {
        BKPT_IF_DEBUGGER();
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
