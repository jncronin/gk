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
    void*    buf;
    char const* name;
    uint32_t q;
} osal_queue_def_t;

#define _OSAL_Q_NAME(_name) .name = #_name
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
    static _type _name##_##buf[_depth];\
    osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf, _OSAL_Q_NAME(_name) };


/* C interface to the C++ Spinlock class */
typedef void * osal_mutex_t;
typedef void * osal_mutex_def_t;

typedef void * osal_queue_t;

typedef void * osal_semaphore_t;
typedef void * osal_semaphore_def_t;

osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef);
bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr);
bool osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec);
void osal_semaphore_reset(osal_semaphore_t sem_hdl); // TODO removed

osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef);
bool osal_mutex_lock (osal_mutex_t sem_hdl, uint32_t msec);
bool osal_mutex_unlock(osal_mutex_t mutex_hdl);

osal_queue_t osal_queue_create(osal_queue_def_t* qdef);
bool osal_queue_receive(osal_queue_t qhdl, void* data, uint32_t msec);
bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr);
bool osal_queue_empty(osal_queue_t qhdl);

#ifdef __cplusplus
}
#endif

#endif
