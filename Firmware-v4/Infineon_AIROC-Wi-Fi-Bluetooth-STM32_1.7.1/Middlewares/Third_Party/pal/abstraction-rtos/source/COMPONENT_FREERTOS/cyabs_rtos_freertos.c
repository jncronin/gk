/***********************************************************************************************//**
 * \file cyabs_rtos_freertos.c
 *
 * \brief
 * Implementation for FreeRTOS abstraction
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2018-2022 Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **************************************************************************************************/

#include <cy_utils.h>
#include <cy_result.h>
#include <cyabs_rtos.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include "cyabs_rtos_internal.h"

static const uint32_t  TASK_IDENT = 0xABCDEF01U;


typedef struct
{
    cy_timer_callback_t     cb;
    cy_timer_callback_arg_t arg;
} callback_data_t;


// Wrapper function to convert FreeRTOS callback signature to match expectation
// for our cyabs_rtos abstraction API.
static void timer_callback(TimerHandle_t arg)
{
    callback_data_t* cb_arg = (callback_data_t*)pvTimerGetTimerID(arg);
    if (NULL != cb_arg->cb)
    {
        cb_arg->cb(cb_arg->arg);
    }
}


//==================================================================================================
// Error Converter
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// cy_rtos_last_error
//--------------------------------------------------------------------------------------------------
cy_rtos_error_t cy_rtos_last_error(void)
{
    return pdFALSE;
}


//==================================================================================================
// Threads
//==================================================================================================

typedef struct
{
    StaticTask_t      task;
    SemaphoreHandle_t sema;
    uint32_t          magic;
    void*             memptr;
} cy_task_wrapper_t;


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_create
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_create(cy_thread_t* thread, cy_thread_entry_fn_t entry_function,
                                const char* name, void* stack, uint32_t stack_size,
                                cy_thread_priority_t priority, cy_thread_arg_t arg)
{
    cy_rslt_t status;
    if ((thread == NULL) || (stack_size < CY_RTOS_MIN_STACK_SIZE))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else if ((stack != NULL) && (0 != (((uint32_t)stack) & CY_RTOS_ALIGNMENT_MASK)))
    {
        status = CY_RTOS_ALIGNMENT_ERROR;
    }
    else
    {
        // If the user provides a stack, we need to allocate memory for the StaticTask_t. If we
        // allocate memory we also need to clean it up. This is true when the task exits itself or
        // when it is killed. In the case it is killed is fairly straight forward. In the case
        // where it exits, we can't clean up any allocated memory since we can't free it before
        // calling vTaskDelete() and vTaskDelete() never returns. Thus we need to do it in join.
        // However, if the task exited itself it has also released any memory it allocated. Thus
        // in order to be able to reliably free memory as part of join, we need to know that the
        // data we are accessing (the StaticTask_t) has not been freed. We therefore need to always
        // allocate that object ourselves. This means we also need to allocate the stack if the
        // user did not provide one.
        uint32_t offset = (stack == NULL)
            ? (stack_size & ~CY_RTOS_ALIGNMENT_MASK)
            : 0;
        uint32_t size  = offset + sizeof(cy_task_wrapper_t);
        uint8_t* ident = (uint8_t*)pvPortMalloc(size);

        if (ident == NULL)
        {
            status = CY_RTOS_NO_MEMORY;
        }
        else
        {
            StackType_t stack_size_rtos =
                ((stack_size & ~CY_RTOS_ALIGNMENT_MASK) / sizeof(StackType_t));
            StackType_t* stack_rtos = (stack == NULL)
                ? (StackType_t*)ident
                : (StackType_t*)stack;

            cy_task_wrapper_t* wrapper = (cy_task_wrapper_t*)(ident + offset);
            wrapper->sema = xSemaphoreCreateBinary();
            CY_ASSERT(wrapper->sema != NULL);
            wrapper->magic  = TASK_IDENT;
            wrapper->memptr = ident;
            CY_ASSERT(((uint32_t)wrapper & CY_RTOS_ALIGNMENT_MASK) == 0UL);
            *thread = xTaskCreateStatic((TaskFunction_t)entry_function, name, stack_size_rtos, arg,
                                        (UBaseType_t)priority, stack_rtos, &(wrapper->task));
            CY_ASSERT(((void*)*thread == (void*)&(wrapper->task)) || (*thread == NULL));
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_exit
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_exit(void)
{
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    // Ideally this would just call vTaskDelete(NULL); however FreeRTOS
    // does not provide any way to know when the task is actually cleaned
    // up. It will tell you that it has been deleted, but when delete is
    // called from the thread itself it doesn't actually get deleted at
    // that time. It just gets added to the list of items that will be
    // deleted when the idle task runs, but there is no way of knowing
    // that the idle task ran unless you add an application hook which is
    // not something that can be done here. This means that
    // cy_rtos_join_thread() has no way of knowing that it is actually
    // save to cleanup memory. So, instad of deleting here, we use a
    // semaphore to indicate that we can delete and then join waits on
    // the semaphore.

    // This cast is ok because the handle internally represents the TCB that we created in the
    // thread create function.
    cy_task_wrapper_t* wrapper = ((cy_task_wrapper_t*)handle);
    if (wrapper->magic == TASK_IDENT)
    {
        // This signals to the thread deleting the current thread that it it is safe to delete the
        // current thread.
        xSemaphoreGive(wrapper->sema);
    }
    else
    {
        CY_ASSERT(false);
    }

    // This function is not expected to return and calling cy_rtos_join_thread will call vTaskDelete
    // on this thread and clean up.
    while (1)
    {
        #if defined(INCLUDE_vTaskSuspend)
        vTaskSuspend(handle);
        #else
        vTaskDelay(10000);
        #endif
    }
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_terminate
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_terminate(cy_thread_t* thread)
{
    cy_rslt_t status;
    if (thread == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        vTaskDelete(*thread);
        // Check to see if we allocated the task and if so free it up.
        cy_task_wrapper_t* wrapper = ((cy_task_wrapper_t*)*thread);
        vTaskSuspendAll();
        if (wrapper->magic == TASK_IDENT)
        {
            wrapper->magic = 0;
            vSemaphoreDelete(wrapper->sema);
            vPortFree(wrapper->memptr);
        }
        xTaskResumeAll();
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_is_running
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_is_running(cy_thread_t* thread, bool* running)
{
    cy_rslt_t status;
    if ((thread == NULL) || (running == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        eTaskState st = eTaskGetState(*thread);
        *running = (st == eRunning);
        status   = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_get_state
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_get_state(cy_thread_t* thread, cy_thread_state_t* state)
{
    cy_rslt_t status;
    if ((thread == NULL) || (state == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        eTaskState st = eTaskGetState(*thread);
        switch (st)
        {
            case eSuspended:
                *state = CY_THREAD_STATE_INACTIVE;
                break;

            case eReady:
                *state = CY_THREAD_STATE_READY;
                break;

            case eRunning:
                *state = CY_THREAD_STATE_RUNNING;
                break;

            case eBlocked:
                *state = CY_THREAD_STATE_BLOCKED;
                break;

            case eDeleted:
                *state = CY_THREAD_STATE_TERMINATED;
                break;

            case eInvalid:
            default:
                *state = CY_THREAD_STATE_UNKNOWN;
                break;
        }

        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_join
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_join(cy_thread_t* thread)
{
    cy_rslt_t status = CY_RSLT_SUCCESS;
    if (thread == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        cy_task_wrapper_t* wrapper = ((cy_task_wrapper_t*)(*thread));
        // This makes sure that the thread to be deleted has completed.  See cy_rtos_exit_thread()
        // for description of why this is done.
        if (wrapper->magic == TASK_IDENT)
        {
            xSemaphoreTake(wrapper->sema, portMAX_DELAY);
            status = cy_rtos_terminate_thread(thread);
        }
        *thread = NULL;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_get_handle
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_get_handle(cy_thread_t* thread)
{
    cy_rslt_t status = CY_RSLT_SUCCESS;

    if (thread == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        *thread = xTaskGetCurrentTaskHandle();
    }

    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_wait_notification
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_wait_notification(cy_time_t timeout_ms)
{
    uint32_t ret;

    ret = ulTaskNotifyTake(pdTRUE, (timeout_ms == CY_RTOS_NEVER_TIMEOUT) ?
                           portMAX_DELAY : convert_ms_to_ticks(timeout_ms));
    if (0 != ret)
    {
        /* Received notify from another thread or ISR */
        return CY_RSLT_SUCCESS;
    }
    else
    {
        return CY_RTOS_TIMEOUT;
    }
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_set_notification
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_set_notification(cy_thread_t* thread)
{
    cy_rslt_t status;
    if (thread == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        if (is_in_isr())
        {
            BaseType_t taskWoken = pdFALSE;
            /* No error checking as this function always returns pdPASS. */
            vTaskNotifyGiveFromISR(*thread, &taskWoken);
            portEND_SWITCHING_ISR(taskWoken);
        }
        else
        {
            /* No error checking as this function always returns pdPASS. */
            xTaskNotifyGive(*thread);
        }
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_thread_get_name
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_thread_get_name(cy_thread_t* thread, const char** thread_name)
{
    *thread_name = pcTaskGetName(*thread);
    return CY_RSLT_SUCCESS;
}


//==================================================================================================
// Scheduler
//==================================================================================================
static uint16_t _cy_rtos_suspend_count = 0;
static uint16_t _cy_rtos_suspend_count_from_ISR = 0;
UBaseType_t uxSavedInterruptStatus[CY_RTOS_MAX_SUSPEND_NESTING];
//--------------------------------------------------------------------------------------------------
// cy_rtos_scheduler_suspend
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_scheduler_suspend(void)
{
    cy_rslt_t status = CY_RSLT_SUCCESS;
    if (is_in_isr())
    {
        if (_cy_rtos_suspend_count_from_ISR < (CY_RTOS_MAX_SUSPEND_NESTING -1))
        {
            uxSavedInterruptStatus[_cy_rtos_suspend_count_from_ISR] = taskENTER_CRITICAL_FROM_ISR();
            ++_cy_rtos_suspend_count_from_ISR;
        }
        else
        {
            status = CY_RTOS_BAD_PARAM;
        }
    }
    else
    {
        taskENTER_CRITICAL();
    }

    if (status == CY_RSLT_SUCCESS)
    {
        ++_cy_rtos_suspend_count;
    }

    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_scheduler_resume
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_scheduler_resume(void)
{
    cy_rslt_t status;
    if (_cy_rtos_suspend_count > 0)
    {
        if (is_in_isr())
        {
            if (_cy_rtos_suspend_count_from_ISR > 0) //We have at least one suspend from ISR call
            {
                taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus[_cy_rtos_suspend_count_from_ISR -
                                                                  1]);
                --_cy_rtos_suspend_count_from_ISR;
                status = CY_RSLT_SUCCESS;
            }
            else
            {
                status = CY_RTOS_BAD_PARAM;
            }
        }
        else
        {
            taskEXIT_CRITICAL();
            status = CY_RSLT_SUCCESS;
        }
        --_cy_rtos_suspend_count;
    }
    else
    {
        status = CY_RTOS_BAD_PARAM;
    }
    return status;
}


//==================================================================================================
// Mutexes
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// cy_rtos_mutex_init
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_mutex_init(cy_mutex_t* mutex, bool recursive)
{
    cy_rslt_t status;
    if (mutex == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        mutex->is_recursive = recursive;
        mutex->mutex_handle = recursive
            ? xSemaphoreCreateRecursiveMutex()
            : xSemaphoreCreateMutex();
        if (mutex->mutex_handle == NULL)
        {
            status = CY_RTOS_NO_MEMORY;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_mutex_get
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_mutex_get(cy_mutex_t* mutex, cy_time_t timeout_ms)
{
    cy_rslt_t status;
    if (mutex == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        TickType_t ticks  = convert_ms_to_ticks(timeout_ms);
        BaseType_t result = (mutex->is_recursive)
                    ? xSemaphoreTakeRecursive(mutex->mutex_handle, ticks)
                    : xSemaphoreTake(mutex->mutex_handle, ticks);

        status = (result == pdFALSE)
                ? CY_RTOS_TIMEOUT
                : CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_mutex_set
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_mutex_set(cy_mutex_t* mutex)
{
    cy_rslt_t status;
    if (mutex == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        BaseType_t result = (mutex->is_recursive)
                    ? xSemaphoreGiveRecursive(mutex->mutex_handle)
                    : xSemaphoreGive(mutex->mutex_handle);

        status = (result == pdFALSE)
                ? CY_RTOS_GENERAL_ERROR
                : CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_mutex_deinit
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_mutex_deinit(cy_mutex_t* mutex)
{
    cy_rslt_t status;
    if (mutex == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        vSemaphoreDelete(mutex->mutex_handle);
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//==================================================================================================
// Semaphores
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// cy_rtos_semaphore_init
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_semaphore_init(cy_semaphore_t* semaphore, uint32_t maxcount, uint32_t initcount)
{
    cy_rslt_t status;
    if (semaphore == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        *semaphore = xSemaphoreCreateCounting(maxcount, initcount);
        if (*semaphore == NULL)
        {
            status = CY_RTOS_NO_MEMORY;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_semaphore_get
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_semaphore_get(cy_semaphore_t* semaphore, cy_time_t timeout_ms)
{
    cy_rslt_t status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // xSemaphoreTakeFromISR does not take timeout as a parameter
    // since it cannot block. Hence we return an error if the user
    // tries to set a timeout.
    if ((semaphore == NULL) || (is_in_isr() && (timeout_ms != 0)))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        TickType_t ticks = convert_ms_to_ticks(timeout_ms);
        status = CY_RSLT_SUCCESS;

        if (is_in_isr())
        {
            if (pdFALSE == xSemaphoreTakeFromISR(*semaphore, &xHigherPriorityTaskWoken))
            {
                status = CY_RTOS_TIMEOUT;
            }
        }
        else
        {
            if (pdFALSE == xSemaphoreTake(*semaphore, ticks))
            {
                status = CY_RTOS_TIMEOUT;
            }
        }
    }

    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_semaphore_set
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_semaphore_set(cy_semaphore_t* semaphore)
{
    cy_rslt_t status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (semaphore == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        BaseType_t ret;

        if (is_in_isr())
        {
            ret = xSemaphoreGiveFromISR(*semaphore, &xHigherPriorityTaskWoken);
            #if configUSE_PREEMPTION == 1
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            #endif
        }
        else
        {
            ret = xSemaphoreGive(*semaphore);
        }

        if (ret == pdFALSE)
        {
            status = CY_RTOS_GENERAL_ERROR;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }

    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_semaphore_get_count
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_semaphore_get_count(cy_semaphore_t* semaphore, size_t* count)
{
    cy_rslt_t status;
    if (semaphore == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        *count = uxSemaphoreGetCount(*semaphore);
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_semaphore_deinit
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_semaphore_deinit(cy_semaphore_t* semaphore)
{
    cy_rslt_t status;
    if (semaphore == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        vSemaphoreDelete(*semaphore);
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//==================================================================================================
// Events
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// cy_rtos_init_event
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_event_init(cy_event_t* event)
{
    cy_rslt_t status;
    if (event == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        *event = xEventGroupCreate();
        if (*event == NULL)
        {
            status = CY_RTOS_NO_MEMORY;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_event_setbits
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_event_setbits(cy_event_t* event, uint32_t bits)
{
    cy_rslt_t status;
    if (event == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        BaseType_t ret;
        if (is_in_isr())
        {
            BaseType_t bt = pdFALSE;
            ret = xEventGroupSetBitsFromISR(*event, (EventBits_t)bits, &bt);
        }
        else
        {
            // xEventGroupSetBits does not return pass/fail, but instead returns the value of the
            // event bits at the time the function returns. There is potential to
            // return 0 (value equal to pdFALSE), so instead treat it as successful after the call
            // to xEventGroupSetBits to avoid false error.
            xEventGroupSetBits(*event, (EventBits_t)bits);
            ret = pdTRUE;
        }

        if (ret == pdFALSE)
        {
            status = CY_RTOS_GENERAL_ERROR;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_event_clearbits
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_event_clearbits(cy_event_t* event, uint32_t bits)
{
    cy_rslt_t status;
    if (event == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        BaseType_t ret;
        if (is_in_isr())
        {
            ret = xEventGroupClearBitsFromISR(*event, (EventBits_t)bits);
        }
        else
        {
            // xEventGroupClearBits does not return pass/fail, but instead returns the value of the
            // event bits before the requested bits were cleared. There is potential to
            // return 0 (value equal to pdFALSE), so instead treat it as successful after the call
            // to xEventGroupClearBits to avoid false error.
            xEventGroupClearBits(*event, (EventBits_t)bits);
            ret = pdTRUE;
        }

        if (ret == pdFALSE)
        {
            status = CY_RTOS_GENERAL_ERROR;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_event_getbits
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_event_getbits(cy_event_t* event, uint32_t* bits)
{
    cy_rslt_t status;
    if ((event == NULL) || (bits == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        *bits  = xEventGroupGetBits(*event);
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_event_waitbits
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_event_waitbits(cy_event_t* event, uint32_t* bits, bool clear, bool all,
                                 cy_time_t timeout_ms)
{
    cy_rslt_t status;
    if ((event == NULL) || (bits == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        TickType_t ticks = convert_ms_to_ticks(timeout_ms);
        uint32_t   bitsVal  = *bits;

        *bits = xEventGroupWaitBits(*event, (EventBits_t)bitsVal, (BaseType_t)clear,
                                    (BaseType_t)all, ticks);
        status   = (((bitsVal & *bits) == bitsVal) || (((bitsVal & *bits) > 0) & !all))
            ? CY_RSLT_SUCCESS
            : CY_RTOS_TIMEOUT;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_event_deinit
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_event_deinit(cy_event_t* event)
{
    cy_rslt_t status;
    if (event == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        vEventGroupDelete(*event);
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//==================================================================================================
// Queues
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// cy_rtos_queue_init
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_queue_init(cy_queue_t* queue, size_t length, size_t itemsize)
{
    cy_rslt_t status;
    if (queue == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        *queue = xQueueCreate(length, itemsize);
        if (*queue == NULL)
        {
            status = CY_RTOS_NO_MEMORY;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_queue_put
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_queue_put(cy_queue_t* queue, const void* item_ptr, cy_time_t timeout_ms)
{
    cy_rslt_t status;
    if ((queue == NULL) || (item_ptr == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        BaseType_t ret;
        if (is_in_isr())
        {
            ret = xQueueSendToBackFromISR(*queue, item_ptr, NULL);
        }
        else
        {
            TickType_t ticks = convert_ms_to_ticks(timeout_ms);
            ret = xQueueSendToBack(*queue, item_ptr, ticks);
        }

        if (ret == pdFALSE)
        {
            status = CY_RTOS_GENERAL_ERROR;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_queue_get
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_queue_get(cy_queue_t* queue, void* item_ptr, cy_time_t timeout_ms)
{
    cy_rslt_t status;
    if ((queue == NULL) || (item_ptr == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        BaseType_t ret;
        if (is_in_isr())
        {
            ret = xQueueReceiveFromISR(*queue, item_ptr, NULL);
        }
        else
        {
            TickType_t ticks = convert_ms_to_ticks(timeout_ms);
            ret = xQueueReceive(*queue, item_ptr, ticks);
        }

        if (ret == pdFALSE)
        {
            status = CY_RTOS_GENERAL_ERROR;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_queue_count
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_queue_count(cy_queue_t* queue, size_t* num_waiting)
{
    cy_rslt_t status;
    if ((queue == NULL) || (num_waiting == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        *num_waiting = is_in_isr()
            ? uxQueueMessagesWaitingFromISR(*queue)
            : uxQueueMessagesWaiting(*queue);
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_queue_space
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_queue_space(cy_queue_t* queue, size_t* num_spaces)
{
    cy_rslt_t status;
    if ((queue == NULL) || (num_spaces == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        *num_spaces = uxQueueSpacesAvailable(*queue);
        status      = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_queue_reset
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_queue_reset(cy_queue_t* queue)
{
    cy_rslt_t status;
    if (queue == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        BaseType_t ret = xQueueReset(*queue);

        if (ret == pdFALSE)
        {
            status = CY_RTOS_GENERAL_ERROR;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_queue_deinit
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_queue_deinit(cy_queue_t* queue)
{
    cy_rslt_t status;
    if (queue == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        vQueueDelete(*queue);
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//==================================================================================================
// Timers
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// cy_rtos_timer_init
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_timer_init(cy_timer_t* timer, cy_timer_trigger_type_t type,
                             cy_timer_callback_t fun, cy_timer_callback_arg_t arg)
{
    cy_rslt_t status;
    if ((timer == NULL) || (fun == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        // Create a wrapper callback to make sure to call fun() with arg as opposed
        // to providing the timer reference as FreeRTOS does by default.
        callback_data_t* cb_arg = (callback_data_t*)malloc(sizeof(callback_data_t));
        if (NULL == cb_arg)
        {
            status = CY_RTOS_NO_MEMORY;
        }
        else
        {
            cb_arg->cb  = fun;
            cb_arg->arg = arg;

            BaseType_t reload = (type == CY_TIMER_TYPE_PERIODIC) ? pdTRUE : pdFALSE;
            *timer = xTimerCreate("", 1, (UBaseType_t)reload, cb_arg, &timer_callback);

            if (*timer == NULL)
            {
                free(cb_arg);
                status = CY_RTOS_NO_MEMORY;
            }
            else
            {
                status = CY_RSLT_SUCCESS;
            }
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_timer_start
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_timer_start(cy_timer_t* timer, cy_time_t num_ms)
{
    cy_rslt_t status;
    if (timer == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        TickType_t ticks = convert_ms_to_ticks(num_ms);
        BaseType_t ret   = xTimerChangePeriod(*timer, ticks, 0);

        if (ret == pdPASS)
        {
            ret = xTimerStart(*timer, 0);
        }

        if (ret == pdFALSE)
        {
            status = CY_RTOS_GENERAL_ERROR;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_timer_stop
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_timer_stop(cy_timer_t* timer)
{
    cy_rslt_t status;
    if (timer == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        BaseType_t ret = xTimerStop(*timer, 0);

        if (ret == pdFALSE)
        {
            status = CY_RTOS_GENERAL_ERROR;
        }
        else
        {
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_timer_is_running
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_timer_is_running(cy_timer_t* timer, bool* state)
{
    cy_rslt_t status;
    if ((timer == NULL) || (state == NULL))
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        BaseType_t active = xTimerIsTimerActive(*timer);
        *state = (active != pdFALSE);

        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_timer_deinit
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_timer_deinit(cy_timer_t* timer)
{
    cy_rslt_t status;
    if (timer == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        void*      cb  = pvTimerGetTimerID(*timer);
        BaseType_t ret = xTimerDelete(*timer, 0);

        if (ret == pdFALSE)
        {
            status = CY_RTOS_GENERAL_ERROR;
        }
        else
        {
            free(cb);
            status = CY_RSLT_SUCCESS;
        }
    }
    return status;
}


//==================================================================================================
// Time
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// cy_rtos_time_get
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_time_get(cy_time_t* tval)
{
    cy_rslt_t status;
    if (tval == NULL)
    {
        status = CY_RTOS_BAD_PARAM;
    }
    else
    {
        *tval  = (cy_time_t)((xTaskGetTickCount() * 1000LL) / configTICK_RATE_HZ);
        status = CY_RSLT_SUCCESS;
    }
    return status;
}


//--------------------------------------------------------------------------------------------------
// cy_rtos_delay_milliseconds
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_rtos_delay_milliseconds(cy_time_t num_ms)
{
    vTaskDelay(convert_ms_to_ticks(num_ms));
    return CY_RSLT_SUCCESS;
}
