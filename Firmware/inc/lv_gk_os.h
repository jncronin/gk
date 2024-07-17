#ifndef LV_GK_OS_H
#define LV_GK_OS_H

/* LV is C therefore define as void * for C and actual GK types for GK */
#ifdef LV_GK_DRIVER
#include "thread.h"
#include "osmutex.h"
typedef Thread lv_thread_t;
typedef Mutex lv_mutex_t;
typedef Condition lv_thread_sync_t;
#else
typedef void *lv_thread_t;
typedef void *lv_mutex_t;
typedef void *lv_thread_sync_t;
#endif


#endif
