/*******************************************************************************
* \file cybt_platform_internal.h
*
* \brief
* Defines variables specific to PSoC6 BLESS platform.
*
********************************************************************************/

#ifndef CYBT_PLATFORM_INTERNAL_H
#define CYBT_PLATFORM_INTERNAL_H

#define PATCH_DOWNLOAD_FN          NULL
#define BLESS_CONTROLLER           WICED_TRUE

#if !defined (DISABLE_LPTIMER)
#define DISABLE_LPTIMER
#endif

#define LOCK                             (true)
#define UNLOCK                           (false)
#define CONTROLLER_SLEEP(enable)

/* Need to update VSC for bless if its needed */
#define ENABLE_SLEEP_MODE()       (1)

#endif //CYBT_PLATFORM_INTERNAL_H

