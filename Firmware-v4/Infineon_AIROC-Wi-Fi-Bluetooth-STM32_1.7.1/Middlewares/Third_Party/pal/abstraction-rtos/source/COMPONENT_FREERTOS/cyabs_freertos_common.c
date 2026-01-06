/***********************************************************************************************//**
 * \file cyabs_freertos_common.c
 *
 * \brief
 * Provides implementations for common functions in FreeRTOS abstraction.
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2022 Cypress Semiconductor Corporation (an Infineon company) or
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
#include "cyabs_rtos.h"
#if defined(CY_USING_HAL)
#include "cyhal.h"
#endif


#if defined(CY_USING_HAL)
//--------------------------------------------------------------------------------------------------
// _cyhal_system_irq_clear_disabled_in_pending
//--------------------------------------------------------------------------------------------------
__WEAK void _cyhal_system_irq_clear_disabled_in_pending(void)
{
    /*
     * The weak implementation for function that
     * clears disabled interrupts in pending state
     */
}


#endif /* defined(CY_USING_HAL) */


//--------------------------------------------------------------------------------------------------
// convert_ms_to_ticks
//--------------------------------------------------------------------------------------------------
cy_time_t convert_ms_to_ticks(cy_time_t timeout_ms)
{
    // Don't convert away from CY_RTOS_NEVER_TIMEOUT due to it being max value
    // Also, skip conversion if timeout_ms is 0
    if (timeout_ms == CY_RTOS_NEVER_TIMEOUT)
    {
        return CY_RTOS_NEVER_TIMEOUT;
    }
    else if (timeout_ms == 0)
    {
        return 0;
    }
    else
    {
        // Convert using our conversion to avoid overflow
        uint64_t ticks =
            ((uint64_t)(((uint64_t)(timeout_ms) * (uint64_t)configTICK_RATE_HZ) /
                        (uint64_t)1000));

        if (ticks >= UINT32_MAX)
        {
            // if ticks if more than 32 bits, change ticks to max possible value
            // that isn't CY_RTOS_NEVER_TIMEOUT.
            ticks = UINT32_MAX - 1;
        }
        return (cy_time_t)ticks;
    }
}
