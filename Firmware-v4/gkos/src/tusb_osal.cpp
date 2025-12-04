#include "tusb_os_custom.h"
#include "osqueue.h"
#include "osmutex.h"

osal_queue_t osal_queue_create(osal_queue_def_t *qd)
{
    auto ret = new CStyleQueue(qd->depth, qd->item_sz);
    while(!ret)
    {
        ret = new CStyleQueue(qd->depth, qd->item_sz);
    }
    return ret;
}

bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr)
{
    (void)in_isr;
    return reinterpret_cast<CStyleQueue *>(qhdl)->Push(data);
}

bool osal_queue_receive(osal_queue_t qhdl, void* data, uint32_t msec)
{
    return reinterpret_cast<CStyleQueue *>(qhdl)->Pop(data);
}

bool osal_queue_empty(osal_queue_t qhdl)
{
    return reinterpret_cast<CStyleQueue *>(qhdl)->empty();
}

osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef)
{
    return new Mutex();
}

bool osal_mutex_delete(osal_mutex_t mutex_hdl)
{
    auto m = reinterpret_cast<Mutex *>(mutex_hdl);
    if(m)
    {
        delete m;
        return true;
    }
    return false;
}

bool osal_mutex_lock (osal_mutex_t sem_hdl, uint32_t msec)
{
    reinterpret_cast<Mutex *>(sem_hdl)->lock();
    return true;
}

bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
{
    return reinterpret_cast<Mutex *>(mutex_hdl)->unlock();
}
