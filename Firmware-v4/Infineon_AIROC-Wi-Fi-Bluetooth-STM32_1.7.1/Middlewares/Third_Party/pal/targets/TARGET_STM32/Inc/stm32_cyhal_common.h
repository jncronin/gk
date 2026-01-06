/***************************************************************************************************
 * \file stim32_cyhal_common.h
 *
 * \brief
 * Common HAL header file for stm32 support
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2021 Cypress Semiconductor Corporation
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

// *SUSPEND-FORMATTING*
#if defined (STM32H723xx)  || \
    defined (STM32H725xx)  || \
    defined (STM32H730xx)  || \
    defined (STM32H730xxQ) || \
    defined (STM32H733xx)  || \
    defined (STM32H735xx)  || \
    defined (STM32H742xx)  || \
    defined (STM32H743xx)  || \
    defined (STM32H745xx)  || \
    defined (STM32H747xx)  || \
    defined (STM32H750xx)  || \
    defined (STM32H753xx)  || \
    defined (STM32H755xx)  || \
    defined (STM32H757xx)  || \
    defined (STM32H7A3xx)  || \
    defined (STM32H7A3xxQ) || \
    defined (STM32H7B0xx)  || \
    defined (STM32H7B0xxQ) || \
    defined (STM32H7B3xx)  || \
    defined (STM32H7B3xxQ)
    #define TARGET_STM32H7xx
#elif defined (STM32L552xx) || defined (STM32L562xx)
    #define TARGET_STM32L5xx
#elif defined (STM32U575xx) || \
    defined (STM32U585xx)   || \
    defined (STM32U595xx)   || \
    defined (STM32U599xx)   || \
    defined (STM32U5A5xx)   || \
    defined (STM32U5A9xx)
    #define TARGET_STM32U5xx
#elif defined (STM32H563xx)
    #define TARGET_STM32H5xx
#else
    #error "Selected STM32 device is not supported by this package."
#endif

#if defined (TARGET_STM32H7xx)
    #include "stm32h7xx.h"
    #include "stm32h7xx_hal.h"
#elif defined (TARGET_STM32L5xx)
    #include "stm32l5xx.h"
    #include "stm32l5xx_hal.h"
    #include "stm32l5xx_hal_def.h"
#elif defined (TARGET_STM32U5xx)
    #include "stm32u5xx.h"
    #include "stm32u5xx_hal.h"
    #include "stm32u5xx_hal_def.h"
#elif defined (TARGET_STM32H5xx)
    #include "stm32h5xx.h"
    #include "stm32h5xx_hal.h"
    #include "stm32h5xx_hal_def.h"
#else
    #error "Selected STM32 device is not supported by this package."
#endif

/* D-cache maintenance for DMA buffers */
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    #define _CYHAL_DCACHE_MAINTENANCE
    #define _CYHAL_DMA_BUFFER_ALIGN_BYTES      (32u)
#else
    #define _CYHAL_DMA_BUFFER_ALIGN_BYTES      (4u)
#endif /* defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U) */

/* Macro to ALIGN */
#if defined (__ARMCC_VERSION) /* ARM Compiler */
    #define ALIGN_HAL_COMMON(buf, x) __align(x) buf
#elif defined   (__GNUC__)    /* GNU Compiler */
    #define ALIGN_HAL_COMMON(buf, x)  buf __attribute__ ((aligned (x)))
#elif defined (__ICCARM__)    /* IAR Compiler */
    #define ALIGN_HAL_COMMON(buf, x) __ALIGNED(x) buf
#endif

/* Macro to get variable aligned for cache maintenance purpose */
#define CYHAL_ALIGN_DMA_BUFFER(arg) ALIGN_HAL_COMMON(arg, _CYHAL_DMA_BUFFER_ALIGN_BYTES)
// *RESUME-FORMATTING*
