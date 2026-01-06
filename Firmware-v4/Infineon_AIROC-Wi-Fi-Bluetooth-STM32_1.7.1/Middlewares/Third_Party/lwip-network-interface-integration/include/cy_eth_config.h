
#ifdef COMPONENT_CAT3
#ifndef ETH_LWIP_CONF_H
#define ETH_LWIP_CONF_H
/***********************************************************************************************************************
 * HEADER FILES
 **********************************************************************************************************************/
#include "xmc_eth_mac.h"

/*PHY identifiers*/
#define PHY_USER_DEFINED (0U)
#define PHY_KSZ8081RNB   (1U)
#define PHY_KSZ8031RNL   (2U)
#define PHY_DP83848C     (3U)

/**
 * @brief Enum to indicate runtime error.
 */
typedef enum ETH_LWIP_ERROR
{
    ETH_LWIP_ERROR_NONE,          /**< No error*/
    ETH_LWIP_ERROR_PHY_BUSY,      /**< PHY is busy*/
    ETH_LWIP_ERROR_PHY_ERROR,     /**< PHY status error*/
    ETH_LWIP_ERROR_PHY_DEVICE_ID, /**< Wrong PHY ID used*/
    ETH_LWIP_ERROR_PHY_TIMEOUT    /**< PHY read failed*/
}ETH_LWIP_ERROR_t;

typedef struct ethernet_lwip
{
    XMC_ETH_MAC_t *eth_mac;        /**< Pointer to the ETH peripheral driver structure */
    struct netif *xnetif;          /**< Pointer to lwIP network interface structure */
    bool initialized;              /**< Indicates if the APP instance is initialized */
} ETH_LWIP_t;

/**
 * @brief Error handler function.
 *
 * @param  error_code Code representing error \ref ETH_LWIP_ERROR_t .
 *
 * @return  None
 *
 * Description:<br>
 * The APP provides a weak definition for this function where it waits forever when there is an error at runtime.
 * This function can be re-implemented in the application to capture the error.
 */
void ETH_LWIP_Error (ETH_LWIP_ERROR_t error_code);

/**
 * @brief Polling function to be used when polling is enabled in the APP UI.
 *
 * @return  None
 *
 * Description:<br>
 * When polling is enabled in the Network Interface tab of the APP, the interrupt handling will be disabled.
 * To propagate the received data, this function should be executed as an idle task function.
 *
 * Example Usage:
 * @code
 * //Pre-condition: Enable polling in Network Interface tab.
 * //Example below, executes periodic lwIP function after initializing ETH_LWIP APP.
 * int main(void)
 * {
 *   ETH_LWIP_STATUS_t init_status;
 *
 *   init_status = ETH_LWIP_Init(&ETH_LWIP_0);
 *   if(init_status == ETH_LWIP_STATUS_SUCCESS)
 *   {
 *       //Periodic function
 *       while(1)
 *       {
 *         ETH_LWIP_Poll();
 *         sys_check_timeouts();
 *       }
 *   }
 *   else
 *   {
 *     XMC_DEBUG("main: Application initialization failed");
 *     while(1U)
 *     {
 *     }
 *   }
 *   return 1U;
 * }
 *
 *  @endcode
 */
void ETH_LWIP_Poll (void);

#endif
#endif
