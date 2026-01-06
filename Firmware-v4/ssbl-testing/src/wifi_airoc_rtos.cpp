#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
#include "clocks.h"

#include "logger.h"

#define CY_FAIL CY_RSLT_CREATE(CY_RSLT_TYPE_FATAL, CY_RSLT_MODULE_ABSTRACTION_HAL, 0)

cy_rslt_t cy_rtos_thread_create(cy_thread_t* thread, cy_thread_entry_fn_t entry_function,
                                const char* name, void* stack, uint32_t stack_size,
                                cy_thread_priority_t priority, cy_thread_arg_t arg)
{
    klog("cy_rtos_thread_create\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_thread_exit(void)
{
    klog("cy_rtos_thread_exit\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_thread_terminate(cy_thread_t* thread)
{
    klog("cy_rtos_thread_terminate\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_thread_join(cy_thread_t* thread)
{
    klog("cy_rtos_thread_join\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_thread_is_running(cy_thread_t* thread, bool* running)
{
    klog("cy_rtos_thread_is_running\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_thread_get_state(cy_thread_t* thread, cy_thread_state_t* state)
{
    klog("cy_rtos_thread_get_state\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_thread_get_handle(cy_thread_t* thread)
{
    klog("cy_rtos_thread_get_handle\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_thread_wait_notification(cy_time_t timeout_ms)
{
    klog("cy_rtos_thread_wait_notification\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_thread_set_notification(cy_thread_t* thread)
{
    klog("cy_rtos_thread_set_notification\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_thread_get_name(cy_thread_t* thread, const char** thread_name)
{
    klog("cy_rtos_thread_get_name\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_mutex_init(cy_mutex_t* mutex, bool recursive)
{
    klog("cy_rtos_mutex_init\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_mutex_get(cy_mutex_t* mutex, cy_time_t timeout_ms)
{
    klog("cy_rtos_mutex_get\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_mutex_set(cy_mutex_t* mutex)
{
    klog("cy_rtos_mutex_set\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_mutex_deinit(cy_mutex_t* mutex)
{
    klog("cy_rtos_mutex_deinit\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_semaphore_init(cy_semaphore_t* semaphore, uint32_t maxcount, uint32_t initcount)
{
    klog("cy_rtos_semaphore_init\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_semaphore_get(cy_semaphore_t* semaphore, cy_time_t timeout_ms)
{
    klog("cy_rtos_semaphore_get\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_semaphore_set(cy_semaphore_t* semaphore)
{
    klog("cy_rtos_semaphore_set\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_semaphore_get_count(cy_semaphore_t* semaphore, size_t* count)
{
    klog("cy_rtos_semaphore_get_count\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_semaphore_deinit(cy_semaphore_t* semaphore)
{
    klog("cy_rtos_semaphore_deinit\n");
    return CY_FAIL;
}

cy_rslt_t cy_rtos_delay_milliseconds(cy_time_t num_ms)
{
    udelay(num_ms * 1000);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_time_get(cy_time_t* tval)
{
    *tval = kernel_time_to_ms(clock_cur());
    return CY_RSLT_SUCCESS;
}
