/***********************************************************************************************//**
 * \file cyabs_rtos_dsram.h
 *
 * \brief
 * Internal definitions for DSRAM implementation on Freertos
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2019-2021 Cypress Semiconductor Corporation (an Infineon company) or
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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


/******************************************************
*                 Function Declarations
******************************************************/
/** Enters DS-RAM Low Power Mode.
 *
 */
void cyabs_rtos_enter_dsram(void);

/** Exits DS-RAM Low Power Mode.
 *
 */
void cyabs_rtos_exit_dsram(void);

/** Stores DS-RAM Context.
 *
 */
void vStoreDSRAMContextWithWFI(void);

/** Restores DS-RAM Context.
 *
 */
void vRestoreDSRAMContext(void);

#ifdef __cplusplus
} // extern "C"
#endif
