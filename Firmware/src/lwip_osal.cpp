#include "osmutex.h"
#include "osqueue.h"
#include "clocks.h"

#include <lwip/sys.h>
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/ethip6.h"
#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>

#include <tusb.h>

extern Process kernel_proc;
extern Scheduler s;

extern char _slwip_data, _elwip_data;

#define LWIP_DATA __attribute__((section(".lwip_data")))


void sys_mutex_lock(sys_mutex_t *mutex)
{
    reinterpret_cast<Mutex *>(mutex->mut)->lock();
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    reinterpret_cast<Mutex *>(mutex->mut)->unlock();
}

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    mutex->mut = new Mutex();
    return ERR_OK;
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    if(mutex->mut)
    {
        delete reinterpret_cast<Mutex *>(mutex->mut);
        mutex->mut = nullptr;
    }    
}

uint32_t sys_disable_interrupts()
{
    return DisableInterrupts();
}

void sys_restore_interrupts(uint32_t cpsr)
{
    RestoreInterrupts(cpsr);
}

void sys_sl_lock(uint32_t *sl)
{
    reinterpret_cast<Spinlock *>(sl)->lock();
}

void sys_sl_unlock(uint32_t *sl)
{
    reinterpret_cast<Spinlock *>(sl)->unlock();
}

uint32_t sys_now()
{
    return clock_cur_ms();
}

void sys_init()
{

}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
    uint32_t data_start = (uint32_t)&_slwip_data;
    uint32_t data_end = (uint32_t)&_elwip_data;
    auto t = Thread::Create(name, thread, arg, true, 5, kernel_proc, Either, InvalidMemregion(),
        MPUGenerate(data_start, data_end - data_start, 6, false, RW, NoAccess, WBWA_NS));
    s.Schedule(t);
    sys_thread_t tret;
    tret.thread_handle = t;
    return tret;
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    mbox->mbx = new Queue(size, sizeof(void *));
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    if(mbox->mbx)
    {
        auto q = reinterpret_cast<Queue *>(mbox->mbx);
        delete q;
        mbox->mbx = nullptr;
    }
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    auto q = reinterpret_cast<Queue *>(mbox->mbx);
    while(!q->Push(&msg))
    {
        Yield();
    }
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    auto q = reinterpret_cast<Queue *>(mbox->mbx);
    if(q->Push(&msg))
        return ERR_OK;
    else
        return ERR_MEM;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    return sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    auto q = reinterpret_cast<Queue *>(mbox->mbx);
    auto ret = q->TryPop(msg);
    if(ret)
        return 0;
    else
        return SYS_MBOX_EMPTY;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    auto now = clock_cur_ms();
    auto tout_time = now + (uint64_t)timeout;
    auto q = reinterpret_cast<Queue *>(mbox->mbx);
    do
    {
        auto ret = q->TryPop(msg);
        if(ret)
        {
            return clock_cur_ms() - now;
        }
        Yield();
    } while(clock_cur_ms() < tout_time);
    *msg = nullptr;
    return SYS_ARCH_TIMEOUT;
}

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    sem->sem = new SimpleSignal(count);
    if(sem->sem == nullptr)
    {
        return ERR_MEM;
    }
    return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem)
{
    auto ss = reinterpret_cast<SimpleSignal *>(sem->sem);
    ss->Signal();
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    auto ss = reinterpret_cast<SimpleSignal *>(sem->sem);
    if(timeout == 0)
    {
        while(!ss->Wait()) Yield();
        return 0;
    }
    else
    {
        auto now = clock_cur_ms();
        auto tout_time = now + (uint64_t)timeout;
        do
        {
            auto ret = ss->WaitOnce();
            if(ret)
            {
                return clock_cur_ms() - now;
            }
            Yield();
        } while(clock_cur_ms() < tout_time);
        return SYS_ARCH_TIMEOUT;
    }    
}

void sys_sem_free(sys_sem_t *sem)
{
    if(sem->sem)
    {
        auto ss = reinterpret_cast<SimpleSignal *>(sem->sem);
        delete ss;
        sem->sem = nullptr;
    }
}
