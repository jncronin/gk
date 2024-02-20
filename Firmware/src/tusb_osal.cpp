#include "tusb_os_custom.h"
#include "osqueue.h"

osal_queue_t osal_queue_create(osal_queue_def_t qd)
{
    auto ret = new Queue(qd.depth, qd.item_sz);
    return ret;
}

bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr)
{
    (void)in_isr;
    return reinterpret_cast<Queue *>(qhdl)->Push(data);
}
