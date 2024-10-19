#include <nema_core.h>
#include <stm32h7rsxx.h>
#include "logger.h"
#include "screen.h"
#include "memblk.h"
#include "osmutex.h"
#include "cache.h"
#include "gk_conf.h"

#define GPU2D_ITCTRL                    (0x0F8U)   /*!< GPU2D Interrupt Control Register Offset            */
#define GPU2D_CLID                      (0x148U)   /*!< GPU2D Last Command List Identifier Register Offset */
#define GPU2D_BREAKPOINT                (0x080U)   /*!< GPU2D Breakpoint Register Offset                   */
#define GPU2D_SYS_INTERRUPT             (0xff8U)   /*!< GPU2D System Interrupt Register Offset             */

#define GPU2D_FLAG_CLC                   0x00000001U              /*!< Command List Complete Interrupt Flag  */

static BinarySemaphore sem_ehold;

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

volatile static int last_cl_id = -1;

static Mutex m_nema[MUTEX_MAX + 1] = { 0 };

uint32_t nema_reg_read(uint32_t reg)
{
    auto ret = *(volatile uint32_t *)(GPU2D_BASE + reg);
    __DSB();
    return ret;
}

void nema_reg_write(uint32_t reg, uint32_t value)
{
    __DMB();
    *(volatile uint32_t *)(GPU2D_BASE + reg) = value;
}

int32_t nema_sys_init()
{
    RCC->AHB5ENR |= RCC_AHB5ENR_GPU2DEN;
    (void)RCC->AHB5ENR;

    int ret = nema_rb_init(&ring_buffer_str, 1);
    if(ret < 0) return ret;

    last_cl_id = 0;

    return 0;
}

nema_buffer_t nema_buffer_create_pool(int pool, int size)
{
    return nema_buffer_create(size);
}

nema_buffer_t nema_buffer_create(int size)
{
    auto mr = memblk_allocate(size, MemRegionType::AXISRAM, "nema buffer");
    if(!mr.valid)
        mr = memblk_allocate(size, MemRegionType::SRAM, "nema buffer");
    if(!mr.valid)
        mr = memblk_allocate(size, MemRegionType::SDRAM, "nema buffer");
    
    nema_buffer_t ret;
    if(mr.valid)
    {
        ret.size = mr.length;
        ret.base_phys = mr.address;
        ret.base_virt = (void *)mr.address;
    }
    else
    {
        ret.size = 0;
        ret.base_phys = 0;
        ret.base_virt = 0;
    }
    ret.fd = 0;

    return ret;
}

void nema_buffer_destroy(nema_buffer_t *bo)
{
    MemRegion mr;
    mr.address = bo->base_phys;
    mr.length = bo->size;

    if(mr.address < 0x30000000U)
        mr.rt = MemRegionType::AXISRAM;
    else if(mr.address < 0x90000000U)
        mr.rt = MemRegionType::SRAM;
    else
        mr.rt = MemRegionType::SDRAM;
    
    memblk_deallocate(mr);
}

void *nema_buffer_map(nema_buffer_t *bo)
{
    return bo->base_virt;
}

void nema_buffer_unmap(nema_buffer_t *bo)
{
}

void *nema_host_malloc(size_t size)
{
    return malloc(size);
}

void nema_host_free(void *ptr)
{
    free(ptr);
}

void nema_buffer_flush(nema_buffer_t *bo)
{
    if(bo->base_phys == ring_buffer_str.bo.base_phys)
        return;
    CleanM7Cache(bo->base_phys, bo->size, CacheType_t::Data);
}

int nema_mutex_lock(int mutex_id)
{
    if(mutex_id < 0 || mutex_id > MUTEX_MAX)
        return -1;
    m_nema[mutex_id].lock();
    return 0;
}

int nema_mutex_unlock(int mutex_id)
{
    if(mutex_id < 0 || mutex_id > MUTEX_MAX)
        return -1;
    m_nema[mutex_id].unlock();
    return 0;
}

int nema_wait_irq()
{
    // TODO: wait on semaphore
    return 0;
}

int nema_wait_irq_cl(int cl_id)
{
    while(last_cl_id < cl_id)
    {
        nema_wait_irq();
        last_cl_id = nema_reg_read(GPU2D_CLID);        
    }

    return 0;
}

INTFLASH_FUNCTION void nematest()
{
    RCC->AHB5ENR |= RCC_AHB5ENR_GPU2DEN;
    (void)RCC->AHB5ENR;
    NVIC_EnableIRQ(GPU2D_IRQn);
    NVIC_EnableIRQ(GPU2D_ER_IRQn);

    int gpupres = nema_checkGPUPresence();

    klog("nema: gpu presence %d\n", gpupres);

    int ret = nema_init();
    klog("nema: nema_init: %d\n", ret);

    nema_ext_hold_enable(0);
    nema_ext_hold_irq_enable(0);


    int x = 0;
    auto cl = nema_cl_create();
    while(true)
    {
        //klog("nema: start frame\n");
        //delay_ms(50);

        nema_cl_bind_circular(&cl); // circular bound lists can get implicitly submitted
        nema_cl_rewind(&cl);

        nema_set_clip(0, 0, 640, 480);

        nema_bind_dst_tex((uintptr_t)screen_get_frame_buffer(), 640, 480,
            NEMA_BGRA8888, 640*4);
        
        nema_clear(nema_rgba(0, 0, 255, 255));
        nema_fill_triangle((100+x)%640, 100, (200+x)%640, 200, (100+x)%640, 200, nema_rgba(255, 0, 0, 255));


        nema_ext_hold_assert(0, 1);

        nema_cl_unbind();
        nema_cl_submit(&cl);
        //nema_cl_wait(&cl);

        sem_ehold.Wait();
        extern Condition scr_vsync;
        scr_vsync.Wait();
        

        //nema_cl_destroy(&cl);

        //screen_flip();

        //delay_ms(100);

        x++;
    }
}

extern "C" void GPU2D_IRQHandler()
{
    auto isr_flags = nema_reg_read(GPU2D_ITCTRL);

    if(isr_flags & GPU2D_FLAG_CLC)
    {
        /* Clear completion flag */
        nema_reg_write(GPU2D_ITCTRL, isr_flags & ~GPU2D_FLAG_CLC);

        last_cl_id = nema_reg_read(GPU2D_CLID);        
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
    screen_flip();
    nema_ext_hold_deassert_imm(0);
    nema_reg_write(GPU2D_SYS_INTERRUPT, isr);
    sem_ehold.Signal();
    __DMB();
}
