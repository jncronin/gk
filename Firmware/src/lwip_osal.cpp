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

__attribute__((section(".sram4"))) uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

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

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    auto tout_time = clock_cur_ms() + (uint64_t)timeout;
    auto q = reinterpret_cast<Queue *>(mbox->mbx);
    do
    {
        auto ret = q->Pop(msg);
        if(ret)
        {
            return ERR_OK;
        }
    } while(clock_cur_ms() < tout_time);
    return SYS_ARCH_TIMEOUT;
}

static struct pbuf *received_frame;
static struct netif netif_data;

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
  /* this shouldn't happen, but if we get another packet before
  parsing the previous, we must signal our inability to accept it */
  if (received_frame) return false;

  if (size)
  {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);

    if (p)
    {
      /* pbuf_alloc() has already initialized struct; all we need to do is copy the data */
      memcpy(p->payload, src, size);

      /* store away the pointer for service_traffic() to later handle */
      received_frame = p;
    }
  }

  return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
  struct pbuf *p = (struct pbuf *)ref;

  (void)arg; /* unused for this example */

  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

[[maybe_unused]] static void service_traffic(void)
{
  /* handle any packet received by tud_network_recv_cb() */
  if (received_frame)
  {
    tcpip_input(received_frame, &netif_data);
    pbuf_free(received_frame);
    received_frame = NULL;
    tud_network_recv_renew();
  }

  sys_check_timeouts();
}

void tud_network_init_cb(void)
{
  /* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
  if (received_frame)
  {
    pbuf_free(received_frame);
    received_frame = NULL;
  }
}
