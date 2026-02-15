#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
#include "clocks.h"
#include "scheduler.h"
#include "thread.h"
#include "process.h"
#include "syscalls_int.h"
#include "errno.h"

#include "logger.h"

extern PProcess p_net;

#define CY_FAIL CY_RTOS_GENERAL_ERROR

cy_rslt_t cy_rtos_thread_create(cy_thread_t* thread, cy_thread_entry_fn_t entry_function,
                                const char* name, void* stack, uint32_t stack_size,
                                cy_thread_priority_t priority, cy_thread_arg_t arg)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    auto t = Thread::Create(name, (Thread::threadstart_t)entry_function,
        (void *)arg, true, GK_PRIORITY_NORMAL, p_net);
    sched.Schedule(t);
#pragma GCC diagnostic pop

    *thread = t->id;
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_thread_exit(void)
{
    klog("cy_rtos_thread_exit\n");
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_thread_terminate(cy_thread_t* thread)
{
    Thread::Kill(*thread, nullptr);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_thread_join(cy_thread_t* thread)
{
    syscall_pthread_join(*thread, nullptr, &errno);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_thread_is_running(cy_thread_t* thread, bool* running)
{
    klog("cy_rtos_thread_is_running\n");
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_thread_get_state(cy_thread_t* thread, cy_thread_state_t* state)
{
    klog("cy_rtos_thread_get_state\n");
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_thread_get_handle(cy_thread_t* thread)
{
    klog("cy_rtos_thread_get_handle\n");
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_thread_wait_notification(cy_time_t timeout_ms)
{
    klog("cy_rtos_thread_wait_notification\n");
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_thread_set_notification(cy_thread_t* thread)
{
    klog("cy_rtos_thread_set_notification\n");
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_thread_get_name(cy_thread_t* thread, const char** thread_name)
{
    auto t = ThreadList.Get(*thread).v;
    if(t)
    {
        *thread_name = t->name.c_str();
        return CY_RSLT_SUCCESS;
    }
    else
    {
        *thread_name = nullptr;
        klog("cy_rtos_thread_get_name fail\n");
        return CY_FAIL;
    }
}

cy_rslt_t cy_rtos_mutex_init(cy_mutex_t* mutex, bool recursive)
{
    auto m = MutexList.Create(recursive, false);
    if(!m)
    {
        return CY_FAIL;
    }
    
    auto p = GetCurrentProcessForCore();
    *mutex = p->owned_mutexes.add(m);
    
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_mutex_get(cy_mutex_t* mutex, cy_time_t timeout_ms)
{
    kernel_time until = kernel_time_invalid();
    int clock_id = 0;
    if(timeout_ms == CY_RTOS_NEVER_TIMEOUT)
    {
        clock_id = CLOCK_WAIT_FOREVER;
    }
    else if(timeout_ms == 0)
    {
        clock_id = CLOCK_TRY_ONCE;
    }
    else
    {
        clock_id = CLOCK_MONOTONIC_RAW;
        until = clock_cur() + kernel_time_from_ms(timeout_ms);
    }
    syscall_pthread_mutex_trylock(mutex, clock_id, &until, &errno);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_mutex_set(cy_mutex_t* mutex)
{
    syscall_pthread_mutex_unlock(mutex, &errno);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_mutex_deinit(cy_mutex_t* mutex)
{
    syscall_pthread_mutex_destroy(mutex, &errno);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_semaphore_init(cy_semaphore_t* semaphore, uint32_t maxcount, uint32_t initcount)
{
    if(maxcount == 0)
    {
        klog("cy_rtos_semaphore_init: maxcount = 0\n");
        return CY_FAIL;
    }
    if(initcount > maxcount)
    {
        klog("cy_rtos_semaphore_init: initcount > maxcount\n");
        initcount = maxcount;
    }

    *semaphore = new SimpleSignal(initcount, maxcount);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_semaphore_get(cy_semaphore_t* semaphore, cy_time_t timeout_ms)
{
    kernel_time until = (timeout_ms == CY_RTOS_NEVER_TIMEOUT) ?
        kernel_time_invalid() : (clock_cur() + kernel_time_from_ms(timeout_ms));

    auto sem = reinterpret_cast<SimpleSignal **>(semaphore);
    return ((*sem)->Wait(SimpleSignal::SignalOperation::Sub, 1U, until) == 0) ? CY_RTOS_TIMEOUT : CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_semaphore_set(cy_semaphore_t* semaphore)
{
    auto sem = reinterpret_cast<SimpleSignal **>(semaphore);
    (*sem)->Signal(SimpleSignal::SignalOperation::AddIfLessThanMax, 1U);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_semaphore_get_count(cy_semaphore_t* semaphore, size_t* count)
{
    auto sem = reinterpret_cast<SimpleSignal **>(semaphore);
    *count = (*sem)->Value();
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_semaphore_deinit(cy_semaphore_t* semaphore)
{
    auto sem = reinterpret_cast<SimpleSignal **>(semaphore);
    delete (*sem);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_delay_milliseconds(cy_time_t num_ms)
{
    Block(clock_cur() + kernel_time_from_ms(num_ms));
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_time_get(cy_time_t* tval)
{
    *tval = kernel_time_to_ms(clock_cur());
    return CY_RSLT_SUCCESS;
}
