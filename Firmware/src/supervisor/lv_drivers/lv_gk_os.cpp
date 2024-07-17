#include "lvgl.h"
#include "thread.h"
#include "osmutex.h"
#include "scheduler.h"

/**
 * Create a new thread
 * @param thread        a variable in which the thread will be stored
 * @param prio          priority of the thread
 * @param callback      function of the thread
 * @param stack_size    stack size in bytes
 * @param user_data     arbitrary data, will be available in the callback
 * @return              LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_thread_init(lv_thread_t * thread, lv_thread_prio_t prio, void (*callback)(void *), size_t stack_size,
                           void * user_data)
{
    int gkprio = GK_PRIORITY_NORMAL;
    switch(prio)
    {
        case LV_THREAD_PRIO_LOW:
        case LV_THREAD_PRIO_LOWEST:
            gkprio = GK_PRIORITY_LOW;
            break;
        case LV_THREAD_PRIO_MID:
            gkprio = GK_PRIORITY_NORMAL;
            break;
        case LV_THREAD_PRIO_HIGH:
        case LV_THREAD_PRIO_HIGHEST:
            gkprio = GK_PRIORITY_HIGH;
            break;
    }

    auto t = Thread::Create("lv_thread", (Thread::threadstart_t)(void *)callback, user_data, true, gkprio,
        GetCurrentThreadForCore()->p, GetCurrentThreadForCore()->tss.affinity);
    if(t)
    {
        *thread = t;
        Schedule(t);
        return LV_RESULT_OK;
    }
    return LV_RESULT_INVALID;
}

/**
 * Delete a thread
 * @param thread        the thread to delete
 * @return              LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_thread_delete(lv_thread_t * thread)
{
    (*(Thread**)thread)->Cleanup(nullptr);
    return LV_RESULT_OK;
}

/**
 * Create a mutex
 * @param mutex         a variable in which the thread will be stored
 * @return              LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_mutex_init(lv_mutex_t * mutex)
{
    *mutex = new Mutex();
    if(!*mutex) return LV_RESULT_INVALID;
    return LV_RESULT_OK;
}


/**
 * Lock a mutex
 * @param mutex         the mutex to lock
 * @return              LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_mutex_lock(lv_mutex_t * mutex)
{
    (*((Mutex **)mutex))->lock();
    return LV_RESULT_OK;
}


/**
 * Lock a mutex from interrupt
 * @param mutex         the mutex to lock
 * @return              LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_mutex_lock_isr(lv_mutex_t * mutex)
{
    (*((Mutex **)mutex))->lock();
    return LV_RESULT_OK;
}


/**
 * Unlock a mutex
 * @param mutex         the mutex to unlock
 * @return              LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_mutex_unlock(lv_mutex_t * mutex)
{
    (*((Mutex **)mutex))->unlock();
    return LV_RESULT_OK;
}


/**
 * Delete a mutex
 * @param mutex         the mutex to delete
 * @return              LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_mutex_delete(lv_mutex_t * mutex)
{
    delete *((Mutex **)mutex);
    return LV_RESULT_OK;
}


/**
 * Create a thread synchronization object
 * @param sync          a variable in which the sync will be stored
 * @return              LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_thread_sync_init(lv_thread_sync_t * sync)
{
    *sync = new BinarySemaphore();
    if(!*sync) return LV_RESULT_INVALID;
    return LV_RESULT_OK;
}


/**
 * Wait for a "signal" on a sync object
 * @param sync      a sync object
 * @return          LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_thread_sync_wait(lv_thread_sync_t * sync)
{
    (*(BinarySemaphore**)sync)->Wait();
    return LV_RESULT_OK;
}


/**
 * Send a wake-up signal to a sync object
 * @param sync      a sync object
 * @return          LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_thread_sync_signal(lv_thread_sync_t * sync)
{
    (*(BinarySemaphore**)sync)->Signal();
    return LV_RESULT_OK;
}


/**
 * Delete a sync object
 * @param sync      a sync object to delete
 * @return          LV_RESULT_OK: success; LV_RESULT_INVALID: failure
 */
lv_result_t lv_thread_sync_delete(lv_thread_sync_t * sync)
{
    delete *((BinarySemaphore**)sync);
    return LV_RESULT_OK;
}

