/***************************************************************************//**
* \file cy_lpa_compat.h
* \version 1.0
*
* \brief
* Compilation macros.
*
********************************************************************************
* \copyright
* Copyright 2019, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/


#ifndef OL_COMPAT_H__
#define OL_COMPAT_H__ (1)

#if defined(__IAR_SYSTEMS_ICC__)
#define CYPRESS_WEAK            __WEAK
#define CYPRESS_PACKED(struct)  __packed struct

#elif defined(__MBED__)
#include "mbed_toolchain.h"
#define CYPRESS_WEAK            MBED_WEAK
#define CYPRESS_PACKED          MBED_PACKED

#else
#define CYPRESS_WEAK           __attribute__((weak))
#define CYPRESS_PACKED(struct) struct __attribute__((packed))

#endif  /* defined(IAR) */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* !OL_COMPAT_H__ */

