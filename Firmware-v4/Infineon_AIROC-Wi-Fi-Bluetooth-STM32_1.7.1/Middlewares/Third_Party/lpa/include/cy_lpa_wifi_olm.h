/***************************************************************************//**
* \file cy_lpa_wifi_olm.h
* \version 1.0
*
* \brief
* Offload Manager API.
*
********************************************************************************
* \copyright
* Copyright 2020, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/


#ifndef OLM_H__
#define OLM_H__    (1)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "cy_lpa_wifi_ol_common.h" /* for ol_info_t */
#define OLM_LOG_ENABLED 1


/* Number of beacons it can miss 1 to 10 is the range */
#define LPA_BCNTRIM 9

/* Beacon wait period in milli-seconds */
#define LPA_BCN_WAIT_PERIOD 10

/* Time for beacon require in seconds */
#define LPA_BCN_REACQUIRE_START 3

/* Roam time threshold should be higher than LPA_BCN_REACQUIRE_START
 * value in seconds
 */
#define LPA_ROAM_TIME_THRESHOLD 4

/*
 * PM2 return to sleep time for Max Power Savings  ( in ms)
 * Min should be PM2_SLEEP_RET_TIME_MIN (10  WHD allowed)
 * Max should be PM2_SLEEP_RET_TIME_MAX (2000 WHD allowed)
 */
#define LPA_PM2_SLEEP_RET_TIME  10

/*******************************************************************************
* LPA Data Structures
*******************************************************************************/

/******************************************************************************/
/** \addtogroup group_lpa_structures *//** \{ */
/******************************************************************************/

/** Offload Manager API (integrated into the platform/system). */
struct ol_desc;

/** Offload Manager context.
 * Private structure; visible to allow for static definition. */
typedef struct olm
{
    const struct ol_desc *ol_list;      /**< Offload Assist list */
    ol_info_t ol_info;                  /**< Offload info */
} olm_t;

/** \} */


/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/******************************************************************************/
/** \addtogroup group_lpa_high_level *//** \{ */
/******************************************************************************/

/** Offload Manager initialization function.
 *
 * Initialize the Offload Manager with a NULL-terminated list of offload descriptors
 * (instance context, function pointers and instance configuration).
 *
 * @param olm        : The pointer to the olm structure @ref olm_t.
 * @param ol_list    : The pointer to the ol_list structure @ref ol_desc_t.
 *
 *******************************************************************************/
extern void cylpa_olm_init(olm_t *olm, const struct ol_desc *ol_list);

/** This function initializes the Offload Manager.
 *
 * @param olm        : The pointer to the olm structure @ref olm_t.
 * @param whd        : The pointer to whd
 * @param ip         : The pointer to ip
 *
 *******************************************************************************/
extern int cylpa_olm_init_ols(olm_t *olm, void *whd, void *ip);

/** Offload Manager de-initialization function.
 *
 * Call the de-initialization function for each offload.
 *
 * @param olm        : The pointer to the olm structure @ref olm_t.
 *
 *******************************************************************************/
extern void cylpa_olm_deinit_ols(olm_t *olm);

/** Offload Manager de-init and free resources
 *
 * @param olm        : The pointer to the olm structure @ref olm_t
 *
 * *****************************************************************************/
extern void cylpa_olm_deinit(olm_t *olm);

/** Configure wlan low power mode when going to sleep and
 *  restore it default when awake.
 *
 * @param whd        : The pointer to the whd interface
 * @param value      : The iovar value to be set into WLAN f/w
 * @param min_power  : boolean min_power set to true ( when LPA is configuring for low power)
 *                      else false ( i.e default value applied)
 *
 * *****************************************************************************/
extern void cylpa_olm_configure_wlan_pmode ( void *whd , uint32_t value, bool min_power );


/** Configure  wlan for low power using IOVAR(s)
 *  called once after cylpa_olm_init
 * @param whd        : The pointer to the whd interface
 *
 * *****************************************************************************/
extern void cylpa_olm_init_wlan_config ( void *whd );

/** \} */


/******************************************************************************/
/** \addtogroup group_loglevel_enum *//**  \{ */
/******************************************************************************/
/** Log offload assist enumerations
 */
typedef enum
{
    LOG_OLA_OLM = 0,   /**< LOG assist OLM */
    LOG_OLA_ARP,       /**< LOG assist ARP */
    LOG_OLA_PF,        /**< LOG assist PF */
    LOG_OLA_TKO,       /**< LOG assist TKO */
    LOG_OLA_MAX_INDEX  /**< LOG assist MAX */
} LOG_OFFLOAD_ASSIST_T;

/** Log Level enumerations
 */
typedef enum
{
    LOG_OLA_LVL_OFF = 0,  /**< LOG LEVEL OFF */
    LOG_OLA_LVL_ERR,      /**< LOG LEVEL ERROR */
    LOG_OLA_LVL_WARNING,  /**< LOG LEVEL WARNING */
    LOG_OLA_LVL_NOTICE,   /**< LOG LEVEL NOTICE */
    LOG_OLA_LVL_INFO,     /**< LOG LEVEL INFO */
    LOG_OLA_LVL_DEBUG,    /**< LOG LEVEL DEBUG */
    LOG_OLA_LVL_MAX_INDEX   /**< LOG LEVEL MAX INDEX NOT USED */

} LOG_OFFLOAD_ASSIST_LEVEL_T;

/** \} */
#if defined (OLM_LOG_ENABLED)


/******************************************************************************/
/** \addtogroup group_lpa_log_level *//** \{ */
/******************************************************************************/
/*
 * OL_LOG Debug verbosity settings
 */
/**
 * This function gets log level for a facility
 *
 * @param assist : Offload Assistant type @ref LOG_OFFLOAD_ASSIST_T
 *
 * @return       : currently set log level
 *               : -1 BAD ARGS
 *
 */
extern int ol_log_get_level(LOG_OFFLOAD_ASSIST_T assist);

/**
 * This function sets log level for a facility
 * @param assist : Offload Assistant type @ref LOG_OFFLOAD_ASSIST_T
 * @param level  : Offload assist logging level @ref LOG_OFFLOAD_ASSIST_LEVEL_T
 *
 * @return       : 0 = success
 *                 -1 = BAD ARGS
 *
 */
extern int ol_log_set_level(LOG_OFFLOAD_ASSIST_T assist, LOG_OFFLOAD_ASSIST_LEVEL_T level);

/** This function prints logging if levels are correct
 *
 * @param assist : Offload Assistant type @ref LOG_OFFLOAD_ASSIST_T
 * @param level  : Offload assist logging level @ref LOG_OFFLOAD_ASSIST_LEVEL_T
 * @param fmt    : Format string like printf()
 * @param ...    : variable argument list for printf
 *
 * @return       : # characters printed
 *
 */
extern int ol_logging(LOG_OFFLOAD_ASSIST_T assist, LOG_OFFLOAD_ASSIST_LEVEL_T level, const char *fmt, ...);

/** \} */

/**< Offload Logging Macros */
#define OL_LOG(...) ol_logging(__VA_ARGS__)                     /**< ex: OL_LOG(LOG_OLA_ARP, LOG_OLA_LVL_NOTICE, "ARP OL IP: %s\n", ip_string); */

#define OL_LOG_OLM(...) ol_logging(LOG_OLA_OLM, __VA_ARGS__)    /**< ex: OL_LOG_ARP(LOG_OLA_LVL_NOTICE, "ARP OL IP: %s\n", ip_string); */
#define OL_LOG_ARP(...) ol_logging(LOG_OLA_ARP, __VA_ARGS__)    /**< ex: OL_LOG_ARP(LOG_OLA_LVL_NOTICE, "ARP OL IP: %s\n", ip_string); */
#define OL_LOG_PF(...) ol_logging(LOG_OLA_PF, __VA_ARGS__)      /**< ex: OL_LOG_PF(LOG_OLA_LVL_ERR, "PF: Bad Args\n"); */
#define OL_LOG_TKO(...) ol_logging(LOG_OLA_TKO, __VA_ARGS__)    /**< ex: OL_LOG_TKO(LOG_OLA_LVL_ERR, "PF: Bad Args\n"); */

#else   /* next section is NOT defined(MBED_CONF_APP_OLM_TEST) */

#define ol_log_get_level(assist)            - 1
#define ol_log_set_level(assist, level)     - 1

/* No log printing when we are not building for TEST_CONSOLE */
#define OL_LOG(...)
#define OL_LOG_OLM(...)
#define OL_LOG_ARP(...)
#define OL_LOG_PF(...)
#define OL_LOG_TKO(...)

#endif  /* defined(OLM_LOG_ENABLED) */

#ifdef __cplusplus
}
#endif

#endif /* !OLM_H__ */

