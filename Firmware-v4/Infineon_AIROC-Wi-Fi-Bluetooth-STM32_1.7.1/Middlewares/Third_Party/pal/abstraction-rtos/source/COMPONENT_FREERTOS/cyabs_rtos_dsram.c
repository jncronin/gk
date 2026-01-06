/***********************************************************************************************//**
 * \file cyabs_freertos_dsram.c
 *
 * \brief
 * Provides implementations for functions required to enable deepsleep ram.
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
#include <cmsis_compiler.h> // For __WEAK
#include "FreeRTOS.h"
#include "cyabs_rtos.h"
#include "cyabs_rtos_dsram.h"

void vPortSetupTimerInterrupt(void);


//--------------------------------------------------------------------------------------------------
// vStoreDSRAMContextWithWFI
//--------------------------------------------------------------------------------------------------
__WEAK void vStoreDSRAMContextWithWFI(void)
{
}


//--------------------------------------------------------------------------------------------------
// vRestoreDSRAMContext
//--------------------------------------------------------------------------------------------------
__WEAK void vRestoreDSRAMContext(void)
{
}


//--------------------------------------------------------------------------------------------------
// cyabs_dsram_enter_dsram
//--------------------------------------------------------------------------------------------------
__WEAK void cyabs_rtos_enter_dsram(void)
{
    vStoreDSRAMContextWithWFI();
}


//--------------------------------------------------------------------------------------------------
// cyabs_dsram_exit_dsram
//--------------------------------------------------------------------------------------------------
__WEAK void cyabs_rtos_exit_dsram(void)
{
    vPortSetupTimerInterrupt();

    vRestoreDSRAMContext();
}
