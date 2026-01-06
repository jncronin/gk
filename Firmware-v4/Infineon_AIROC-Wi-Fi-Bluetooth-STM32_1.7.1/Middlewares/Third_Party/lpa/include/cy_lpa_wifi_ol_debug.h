/***************************************************************************//**
* \file cy_lpa_wifi_ol_debug.h
* \version 1.0
*
* \brief
* Debug helper.
*
********************************************************************************
* \copyright
* Copyright 2020, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/


#ifndef OL_DEBUG_H__
#define OL_DEBUG_H__   (1)

#if !defined(USE_TRACE)
#define USE_TRACE 1
#endif /* !defined(USE_TRACE) */

#if (USE_TRACE != 0)
#include <stdio.h>
#define TRACE(...) printf(__VA_ARGS__)
#else
#define TRACE(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* !OL_DEBUG_H__ */

