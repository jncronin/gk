#ifndef TUSB_OS_CUSTOM_H
#define TUSB_OS_CUSTOM_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Define a queue with fixed data storage */
typedef struct
{
    uint16_t depth;
    uint16_t item_sz;
} osal_queue_def_t;

#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
    osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type) };


/* C interface to the C++ Spinlock class */
typedef void * osal_mutex_t;
typedef void * osal_mutex_def_t;

typedef void * osal_queue_t;

osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef);
bool osal_mutex_lock (osal_mutex_t sem_hdl, uint32_t msec);
bool osal_mutex_unlock(osal_mutex_t mutex_hdl);
bool osal_mutex_delete(osal_mutex_t mutex_hdl);

osal_queue_t osal_queue_create(osal_queue_def_t* qdef);
bool osal_queue_receive(osal_queue_t qhdl, void* data, uint32_t msec);
bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr);
bool osal_queue_empty(osal_queue_t qhdl);
bool osal_queue_delete(osal_queue_t qhdl);

#ifdef __cplusplus
}
#endif

#endif
